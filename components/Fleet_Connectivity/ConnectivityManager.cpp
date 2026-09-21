#include "ConnectivityManager.h"
#include "esp_heap_caps.h"   // pre-AP internal-heap probe, see raiseAp()
#include <WiFi.h>
#include <Preferences.h>
#include <esp_wifi.h>
#include "Fleet_BSP.h"

// NVS layout. Namespace renamed from the Phase-1 prototype's "wifi" so the
// stored shape can grow past a bare SSID/password pair; loadConfigFromNvs()
// migrates the old namespace on first boot rather than silently losing
// credentials someone already provisioned.
static const char *NVS_NS          = "fleetconn";
static const char *NVS_LEGACY_NS   = "wifi";
static const char *K_CONFIGURED    = "configured";
static const char *K_SSID          = "ssid";
static const char *K_PASS          = "password";
static const char *K_MODE          = "mode";
static const char *K_HOST          = "host";
static const char *K_MACSFX        = "macsfx";
static const char *K_MIGRATED      = "migrated";
static const char *K_PROVEN        = "proven";

// How often to re-attempt STA while parked in AP fallback. Without this a
// device whose router rebooted would sit in AP mode until someone power-cycled
// it - the failure mode that makes AP fallback feel broken rather than helpful.
// AP-mode STA retry backoff. A device sitting in AP mode should still notice
// when the real network comes back - but it must not retry forever on a fixed
// one-minute tick, which is what an earlier version did: with wrong
// credentials it re-attempted indefinitely, and on a marginal supply every
// radio burst is another brownout opportunity. Doubles from base to cap;
// resets on a successful connect or a credential change.
static const uint32_t AP_RETRY_BASE_MS  = 120000;    // 2 min
static const uint32_t AP_RETRY_MAX_MS   = 1800000;   // 30 min
static const uint32_t JOIN_TIMEOUT_MS   = 20000;
static const uint32_t BOOT_HOLD_MS      = 1500;

// --- Auth-failure policy (docs: "Connectivity behavior spec", signed off) ---
// A wrong password cannot fix itself, so it is never retried on a timer. But a
// router in a momentarily odd state looks identical to a wrong password from
// here, so the credentials get exactly one second chance before being written
// off. Two failures, a pause, two more, then stop.
static const int32_t  AUTH_FAILS_PER_ROUND = 2;
static const int32_t  AUTH_ROUNDS          = 2;
static const uint32_t SECOND_CHANCE_MS     = 45000;      // 45 s between rounds

// Credentials that have connected before and are NOW rejected almost certainly
// mean the AP's password changed. Still terminal, but worth a periodic probe -
// passwords sometimes change back, and one association attempt costs nothing.
// Unproven credentials get no probe at all: they are a typo.
static const uint32_t PROVEN_RECHECK_MS    = 2700000;    // 45 min

// While a client is attached to our AP, push the STA retry out rather than
// cancelling it. Capped, so a phone that silently auto-rejoins a saved network
// cannot pin the device in AP mode indefinitely.
static const uint32_t CLIENT_DEFER_STEP_MS = 300000;     // +5 min per evaluation
static const uint32_t CLIENT_DEFER_MAX_MS  = 1800000;    // never defer past 30 min total

// Troubleshooting-only detail. Baseline lines (state transitions, connect
// success/failure, AP up/down) always print - they are what you need from a
// board on a wall. Raw reason codes, per-entry scan dumps and join internals
// are noise until something is actually wrong, so they are gated. Enable with
// -D DEBUG_WIFI in one environment's build_flags; see CLAUDE.md's Debug flag
// convention.
#ifdef DEBUG_WIFI
    #define DBG_WIFI(...) Serial.printf("[Conn:debug] " __VA_ARGS__)
#else
    #define DBG_WIFI(...) do {} while (0)
#endif

// ---------------------------------------------------------------------------
// Name tables (declared in ConnectivityTypes.h)
// ---------------------------------------------------------------------------
const char *connModeName(ConnMode m) {
    switch (m) {
        case ConnMode::OFF:                  return "OFF";
        case ConnMode::STA_WITH_AP_FALLBACK: return "STA_WITH_AP_FALLBACK";
        case ConnMode::STA_PLUS_AP:          return "STA_PLUS_AP";
        case ConnMode::STA_ONLY:             return "STA_ONLY";
    }
    return "?";
}
const char *connStateName(ConnState s) {
    switch (s) {
        case ConnState::RADIO_OFF:       return "DISABLED";
        case ConnState::BOOT:           return "BOOT";
        case ConnState::STA_CONNECTING: return "STA_CONNECTING";
        case ConnState::STA_CONNECTED:  return "STA_CONNECTED";
        case ConnState::AP_ACTIVE:      return "AP_ACTIVE";
        case ConnState::APSTA:          return "APSTA";
        case ConnState::DEGRADED:       return "DEGRADED";
    }
    return "?";
}
const char *linkTypeName(LinkType l) {
    switch (l) {
        case LinkType::NONE:       return "none";
        case LinkType::STA:   return "wifi-sta";
        case LinkType::AP:    return "wifi-ap";
        case LinkType::APSTA: return "wifi-apsta";
        case LinkType::ETHERNET:   return "ethernet";
    }
    return "?";
}
const char *joinResultName(JoinResult r) {
    switch (r) {
        case JoinResult::NONE:           return "none";
        case JoinResult::SUCCESS:        return "success";
        case JoinResult::FAIL_AUTH:      return "wrong password";
        case JoinResult::FAIL_NO_AP:     return "network not found";
        case JoinResult::FAIL_TIMEOUT:   return "timed out";
        case JoinResult::FAIL_CANCELLED: return "cancelled";
    }
    return "?";
}
const char *linkHealthName(LinkHealth h) {
    switch (h) {
        case LinkHealth::LINK_UNKNOWN: return "unknown";
        case LinkHealth::LINK_HEALTHY: return "healthy";
        case LinkHealth::LINK_SUSPECT: return "suspect";
        case LinkHealth::LINK_DEAD:    return "DEAD";
    }
    return "?";
}

// ---------------------------------------------------------------------------
// Event plumbing. One instance, one handler - the handler is the single
// authority for state transitions (pattern borrowed from the NINA project,
// where a second handler registered elsewhere was explicitly avoided).
// ---------------------------------------------------------------------------
static ConnectivityManager *s_instance = nullptr;

static void wifiEventTrampoline(arduino_event_id_t event, arduino_event_info_t info) {
    if (s_instance) s_instance->handleEvent((int32_t)event, &info);
}

ConnectivityManager::ConnectivityManager()
    : _defaults(CONNECTIVITY_DEFAULT_WIFI) {
    _mutex = xSemaphoreCreateMutex();
}

// ---------------------------------------------------------------------------
// Small helpers
// ---------------------------------------------------------------------------
#define LOCK()   if (_mutex) xSemaphoreTake(_mutex, portMAX_DELAY)
#define UNLOCK() if (_mutex) xSemaphoreGive(_mutex)

void ConnectivityManager::setState(ConnState s, LinkType l) {
    ConnState prev = (ConnState)_state.load();
    _state.store((int32_t)s);
    _link.store((int32_t)l);
    if (prev != s) {
        Serial.printf("[Conn] %s -> %s (%s)\n", connStateName(prev), connStateName(s), linkTypeName(l));
    }
}

void ConnectivityManager::getSsid(char *out, size_t len) const {
    if (!out || !len) return;
    if (_mutex) xSemaphoreTake(_mutex, portMAX_DELAY);
    strlcpy(out, _ssid, len);
    if (_mutex) xSemaphoreGive(_mutex);
}

void ConnectivityManager::getHostname(char *out, size_t len) const {
    if (!out || !len) return;
    if (_mutex) xSemaphoreTake(_mutex, portMAX_DELAY);
    strlcpy(out, _hostname[0] ? _hostname : DeviceIdentity::hostname(), len);
    if (_mutex) xSemaphoreGive(_mutex);
}

const char *ConnectivityManager::getApSsid() const {
    return _defaults.AP_SSID[0] ? _defaults.AP_SSID : DeviceIdentity::apSsid();
}
const char *ConnectivityManager::getApPassword() const {
    return _defaults.AP_PASSWORD[0] ? _defaults.AP_PASSWORD : DeviceIdentity::apPassword();
}
IPAddress ConnectivityManager::getApIP() const { return WiFi.softAPIP(); }

bool ConnectivityManager::isOnline() const {
    // A CONFIRMED-DEAD LINK IS NOT ONLINE, whatever the driver says.
    //
    // This one line is what makes the #49 verdict mean something to the rest
    // of the system rather than only to the header glyph. Everything above
    // asks isOnline() and nothing above asks getLinkHealth(), so a board that
    // had convicted its own link still told MqttManager it was fine - and
    // MqttManager then spent up to SOCKET_TIMEOUT_S blocking the loop task on
    // a TCP connect that could not succeed.
    //
    // That is the owner's "the device frequently locks up where screen taps do
    // nothing". LVGL runs on that same task. The UI was not slow; it was not
    // running.
    if ((LinkHealth)_health.load() == LinkHealth::LINK_DEAD) return false;

    ConnState s = (ConnState)_state.load();
    return (s == ConnState::STA_CONNECTED || s == ConnState::APSTA) && _gotIp.load() != 0;
}
IPAddress ConnectivityManager::getIP() const { return WiFi.localIP(); }

// Reason-code classification. The exact set treated as "auth" is taken from
// the NINA project's wifi_join.c, which was tuned against real hardware - a
// broader set (e.g. folding in AUTH_EXPIRE) misreports ordinary roaming
// disassociations as a wrong password.
JoinResult ConnectivityManager::classifyDisconnect(uint8_t reason) {
    switch (reason) {
        case WIFI_REASON_AUTH_FAIL:
        case WIFI_REASON_4WAY_HANDSHAKE_TIMEOUT:
        case WIFI_REASON_MIC_FAILURE:
        case WIFI_REASON_HANDSHAKE_TIMEOUT:
            return JoinResult::FAIL_AUTH;
        case WIFI_REASON_NO_AP_FOUND:
            return JoinResult::FAIL_NO_AP;
        default:
            return JoinResult::NONE;   // transient - keep trying
    }
}

// ---------------------------------------------------------------------------
// Configuration
// ---------------------------------------------------------------------------
void ConnectivityManager::loadConfigFromNvs() {
    Preferences p;
    bool haveNew = false;

    // Opening a namespace read-only before it exists makes the Preferences
    // library log `nvs_open failed: NOT_FOUND` at error level on every boot -
    // alarming-looking noise for an entirely normal fresh flash. One
    // read-write open creates the namespace so later reads are quiet.
    { Preferences seed; if (seed.begin(NVS_NS, /*readOnly=*/false)) seed.end(); }

    if (p.begin(NVS_NS, /*readOnly=*/true)) {
        haveNew = p.getBool(K_CONFIGURED, false);
        if (haveNew) {
            LOCK();
            p.getString(K_SSID, _ssid, sizeof(_ssid));
            p.getString(K_PASS, _password, sizeof(_password));
            p.getString(K_HOST, _hostname, sizeof(_hostname));
            UNLOCK();
        }
        _mode.store((int32_t)p.getUChar(K_MODE, (uint8_t)_defaults.MODE));
        _proven = p.getBool(K_PROVEN, false);
        p.end();
    }

    // Probe the legacy namespace at most once ever. Without the guard, a board
    // that legitimately has no stored credentials logs a NOT_FOUND every boot
    // forever; with it, at worst one line on the very first boot.
    bool migrated = false;
    if (p.begin(NVS_NS, /*readOnly=*/true)) {
        migrated = p.getBool(K_MIGRATED, false);
        p.end();
    }

    if (!haveNew && !migrated) {
        // Migrate the prototype's namespace so credentials provisioned before
        // this rewrite are not silently lost on update.
        Preferences old;
        if (old.begin(NVS_LEGACY_NS, /*readOnly=*/true)) {
            if (old.getBool(K_CONFIGURED, false)) {
                LOCK();
                old.getString(K_SSID, _ssid, sizeof(_ssid));
                old.getString(K_PASS, _password, sizeof(_password));
                UNLOCK();
                haveNew = true;
                Serial.println("[Conn] Migrated credentials from legacy NVS namespace.");
            }
            old.end();
            if (haveNew) setStationCredentials(_ssid, _password);
        }
        // Stamp the flag whatever the outcome, so the legacy namespace is
        // probed at most once in this device's life.
        { Preferences q; if (q.begin(NVS_NS, false)) { q.putBool(K_MIGRATED, true); q.end(); } }
    }

    if (!haveNew) {
        LOCK();
        strlcpy(_ssid,     _defaults.STA_SSID,     sizeof(_ssid));
        strlcpy(_password, _defaults.STA_PASSWORD, sizeof(_password));
        strlcpy(_hostname, _defaults.HOSTNAME,     sizeof(_hostname));
        UNLOCK();
        _mode.store((int32_t)_defaults.MODE);
    }
    // Apply the stored MAC-suffix policy BEFORE anything reads a derived name.
    // DeviceIdentity caches its strings on first use, so a late call here would
    // leave hostname/AP SSID built against the wrong policy - which is exactly
    // what happened when this call went missing: APPEND_MAC_SUFFIX = false had
    // no effect at all, because nothing ever told DeviceIdentity about it.
    {
        Preferences q;
        bool appendMac = _defaults.APPEND_MAC_SUFFIX;
        if (q.begin(NVS_NS, /*readOnly=*/true)) {
            appendMac = q.getBool(K_MACSFX, _defaults.APPEND_MAC_SUFFIX);
            q.end();
        }
        DeviceIdentity::configure(appendMac);
    }

    _configured = (_ssid[0] != '\0');
}

bool ConnectivityManager::setMode(ConnMode m) {
    Preferences p;
    if (!p.begin(NVS_NS, false)) return false;
    p.putUChar(K_MODE, (uint8_t)m);
    p.putBool(K_CONFIGURED, true);
    p.end();
    _mode.store((int32_t)m);
    Serial.printf("[Conn] Mode set to %s (applies on next begin()).\n", connModeName(m));
    return true;
}

bool ConnectivityManager::setStationCredentials(const char *ssid, const char *password) {
    if (!ssid || !ssid[0]) return false;
    Preferences p;
    if (!p.begin(NVS_NS, false)) return false;
    p.putBool(K_CONFIGURED, true);
    p.putString(K_SSID, ssid);
    p.putString(K_PASS, password ? password : "");
    p.end();

    LOCK();
    strlcpy(_ssid, ssid, sizeof(_ssid));
    strlcpy(_password, password ? password : "", sizeof(_password));
    UNLOCK();
    _configured = true;

    // New credentials are unproven by definition, and they deserve a clean
    // slate: no inherited backoff, no inherited terminal stop. Skipped while an
    // interactive join is in flight, since that path calls this on success and
    // would otherwise immediately unprove what it just proved.
    if (!_joinActive.load()) {
        if (_proven) markProven(false);
        _authFailCount.store(0);
        _authRound.store(0);
        _authStopped.store(0);
        _apRetryDelayMs = 0;
        _clientDeferMs  = 0;
    }
    return true;
}

void ConnectivityManager::clearStationCredentials() {
    Preferences p;
    if (p.begin(NVS_NS, false)) { p.clear(); p.end(); }
    LOCK();
    _ssid[0] = _password[0] = '\0';
    UNLOCK();
    _configured = false;
}

// Written once, on the first successful connect, and cleared when credentials
// change - not per attempt, so NVS wear is a non-issue.
void ConnectivityManager::markProven(bool proven) {
    _proven = proven;
    Preferences p;
    if (p.begin(NVS_NS, false)) { p.putBool(K_PROVEN, proven); p.end(); }
    if (proven) Serial.println("[Conn] Credentials confirmed working.");
}

// The escape hatch from every terminal state. Without this, wrong-and-unproven
// credentials would need a reboot or a reflash to get out of - which is exactly
// the trap AP fallback exists to avoid.
void ConnectivityManager::retryNow() {
    if ((ConnState)_state.load() == ConnState::RADIO_OFF) return;
    Serial.println("[Conn] Manual retry requested.");
    _attempt = 0;
    _authFailCount.store(0);
    _authRound.store(0);
    _authStopped.store(0);
    _reissueCount.store(0);
    _reissuePending.store(0);
    _giveUpNow.store(0);
    _lastFailure.store((int32_t)JoinResult::NONE);
    _apRetryDelayMs = 0;
    _clientDeferMs  = 0;
    startStaAttempt();
}

bool ConnectivityManager::setAppendMacSuffix(bool on) {
    Preferences p;
    if (!p.begin(NVS_NS, false)) return false;
    p.putBool(K_MACSFX, on);
    p.putBool(K_CONFIGURED, true);
    p.end();
    DeviceIdentity::configure(on);
    Serial.printf("[Conn] MAC suffix %s - hostname now \"%s\", AP SSID \"%s\" (applies on next connect).\n",
                  on ? "enabled" : "disabled",
                  DeviceIdentity::hostname(), DeviceIdentity::apSsid());
    return true;
}

bool ConnectivityManager::setHostname(const char *hostname) {
    char clean[64];
    if (!DeviceIdentity::sanitizeHostname(hostname, clean, sizeof(clean))) {
        Serial.printf("[Conn] Rejected hostname \"%s\": no valid DNS label remains.\n",
                      hostname ? hostname : "(null)");
        return false;
    }
    Preferences p;
    if (!p.begin(NVS_NS, false)) return false;
    p.putString(K_HOST, clean);
    p.putBool(K_CONFIGURED, true);
    p.end();
    LOCK();
    strlcpy(_hostname, clean, sizeof(_hostname));
    UNLOCK();
    Serial.printf("[Conn] Hostname set to \"%s\" (applies on next connect).\n", clean);
    return true;
}

// ---------------------------------------------------------------------------
// Radio
// ---------------------------------------------------------------------------
void ConnectivityManager::applyRadioTuning() {
    if (_defaults.TX_POWER_DBM == 0) return;   // leave the chip default alone
    // esp_wifi_set_max_tx_power takes quarter-dBm units.
    esp_err_t err = esp_wifi_set_max_tx_power((int8_t)(_defaults.TX_POWER_DBM * 4));
    // Announced once. It is re-applied on every link-up and every AP raise
    // (both are points where the PA keys up hard), but repeating the line
    // mid-sequence reads like something is being reconfigured when nothing is.
    static bool announced = false;
    if (!announced && err == ESP_OK) {
        announced = true;
        Serial.printf("[Conn] TX power capped to %u dBm.\n", _defaults.TX_POWER_DBM);
    } else if (err != ESP_OK) {
        DBG_WIFI("TX power cap failed: %s\n", esp_err_to_name(err));
    }
}

bool ConnectivityManager::bootButtonHeld() const {
    if (bsp_hw.BOOT_BUTTON_PIN < 0) return false;
    pinMode(bsp_hw.BOOT_BUTTON_PIN, INPUT_PULLUP);
    uint32_t start = millis();
    while (millis() - start < BOOT_HOLD_MS) {
        if (digitalRead(bsp_hw.BOOT_BUTTON_PIN) != LOW) return false;
        delay(20);
    }
    return true;
}

void ConnectivityManager::startStaAttempt() {
    char ssid[33], pass[65], host[64];
    LOCK();
    strlcpy(ssid, _ssid, sizeof(ssid));
    strlcpy(pass, _password, sizeof(pass));
    strlcpy(host, _hostname[0] ? _hostname : DeviceIdentity::hostname(), sizeof(host));
    UNLOCK();

    // Two calls, deliberately. The first keeps NetworkManager's global default
    // in sync (it is what any freshly-created netif inherits); the second sets
    // the hostname on the STA interface itself via esp_netif_set_hostname,
    // which is the value actually sent in DHCP option 12. Only the second one
    // affects what your router displays - see the long note in begin().
    WiFi.setHostname(host);
    WiFi.STA.setHostname(host);

    _gotIp.store(0);
    _reissueCount.store(0);
    _reissuePending.store(0);
    _giveUpNow.store(0);
    _attemptStartedMs = millis();
    _attempt++;
    _attemptPub.store(_attempt);   // the UI reads this; _attempt is loop()-only
    setState(ConnState::STA_CONNECTING,
             isApActive() ? LinkType::APSTA : LinkType::STA);
    Serial.printf("[Conn] STA attempt %u/%u -> \"%s\" as \"%s\"\n",
                  _attempt, _defaults.STA_RETRY_COUNT, ssid, host);
    DBG_WIFI("  mode=%s timeout=%lums txcap=%udBm macSuffix=%s deviceId=%s\n",
             connModeName((ConnMode)_mode.load()),
             (unsigned long)_defaults.STA_CONNECT_TIMEOUT_MS,
             _defaults.TX_POWER_DBM,
             DeviceIdentity::appendMacSuffix() ? "on" : "off",
             DeviceIdentity::deviceId());
    WiFi.begin(ssid, pass);
}

bool ConnectivityManager::raiseAp(const char *why) {
    if (isApActive()) return true;
    IPAddress ip, sub;
    ip.fromString(_defaults.AP_IP);
    sub.fromString(_defaults.AP_SUBNET);

    // Always AP+STA, never AP-only. escalateAfterFailure() keeps a STA retry
    // ladder running the entire time the AP is up (AP_RETRY_BASE_MS doubling to
    // AP_RETRY_MAX_MS) and the retry re-issues WiFi.begin() without touching the
    // mode - so dropping the STA interface here would strand a board that raised
    // its AP over a router reboot in AP mode until someone power-cycled it,
    // which is the exact failure AP_RETRY_BASE_MS exists to prevent.
    //
    // This was previously a ternary whose two branches were both WIFI_AP_STA -
    // a leftover from an earlier design in which the fallback AP was AP-only.
    WiFi.mode(WIFI_AP_STA);
    // Internal RAM at the moment of truth. softAP() allocates from INTERNAL
    // DRAM, and when that allocation fails the WiFi driver does not report it -
    // it dereferences the null and panics inside ieee80211_hostap_attach. That
    // is a crash with no error message, so the number is worth having.
    //
    // CYD_S3_3248 is the board to watch: it is the fleet's only QSPI panel, so
    // it is the only one whose LVGL buffers must live in internal SRAM rather
    // than PSRAM, and it has the least headroom by a wide margin.
    DBG_WIFI("pre-AP internal heap: %u free, largest block %u\n",
             (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL),
             (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL));

    WiFi.softAPConfig(ip, ip, sub);
    bool ok = WiFi.softAP(getApSsid(), getApPassword());
    if (!ok) {
        // Notably possible on the P4 boards, where WiFi runs over an ESP32-C6
        // co-processor via esp_hosted - softAP support there is exactly what
        // GitHub issue #5 exists to establish.
        Serial.printf("[Conn] softAP() FAILED (%s). AP unavailable on this board.\n", why);
        return false;
    }
    _apActive.store(1);
    _apLastClientMs = millis();
    // Bringing the AP up is a second point where the PA keys up hard, so
    // re-apply the cap here as well as on link-up.
    applyRadioTuning();
    Serial.printf("[Conn] AP up: SSID \"%s\"  pass \"%s\"  IP %s  (%s)\n",
                  getApSsid(), getApPassword(), WiFi.softAPIP().toString().c_str(), why);
    return true;
}

void ConnectivityManager::stopAp(const char *why) {
    if (!isApActive()) return;
    // Genuinely stop rather than hide. The NINA/ha-dashboard trick of setting
    // ssid_hidden + max_connection=0 avoids a radio restart, but the AP keeps
    // running and keeps drawing power - which defeats the point of idling down.
    WiFi.softAPdisconnect(true);
    WiFi.mode(WIFI_STA);
    _apActive.store(0);
    _apClients.store(0);
    Serial.printf("[Conn] AP stopped (%s).\n", why);
}

// Decide where an exhausted attempt cycle lands, and when (or whether) to try
// again. The three outcomes come straight from the signed-off behavior spec.
void ConnectivityManager::escalateAfterFailure(uint32_t now) {
    JoinResult why  = (JoinResult)_lastFailure.load();
    ConnMode   mode = (ConnMode)_mode.load();
    bool wantAp = (mode == ConnMode::STA_WITH_AP_FALLBACK) ||
                  (mode == ConnMode::STA_PLUS_AP) ||
                  _apFallbackForced;

    bool authStop  = _authStopped.load() != 0;
    bool midRounds = (why == JoinResult::FAIL_AUTH) && !authStop;

    if (midRounds) {
        // Between the two auth rounds. Short, fixed pause - not the
        // environmental backoff, which would be far too long for a second look.
        _apRetryDelayMs = SECOND_CHANCE_MS;
    } else if (authStop) {
        // Terminal. Proven credentials get a periodic probe in case the AP's
        // password changed back; unproven ones get nothing at all, and the
        // retry block refuses to run for them.
        _apRetryDelayMs = _proven ? PROVEN_RECHECK_MS : 0;
    } else {
        // Environmental: the network was absent, which routinely resolves
        // itself. Retry indefinitely, backing off so a long outage does not
        // mean thrashing the radio - and the power rail - every two minutes.
        if (_apRetryDelayMs == 0 || _apRetryDelayMs == SECOND_CHANCE_MS) {
            _apRetryDelayMs = AP_RETRY_BASE_MS;
        } else {
            _apRetryDelayMs = (_apRetryDelayMs * 2 > AP_RETRY_MAX_MS)
                              ? AP_RETRY_MAX_MS : _apRetryDelayMs * 2;
        }
    }
    _clientDeferMs = 0;

    const char *reason = midRounds ? "auth rejected - one more round queued"
                       : authStop  ? "wrong credentials - waiting for setup"
                                   : "network unreachable";

    if (wantAp && raiseAp(reason)) {
        setState(ConnState::AP_ACTIVE, LinkType::AP);
    } else {
        enterDegraded(mode == ConnMode::STA_ONLY
                      ? "STA_ONLY: no AP by policy"
                      : "AP unavailable");
    }

    if (authStop && !_proven) {
        Serial.println("[Conn] These credentials have never worked. Not retrying "
                       "automatically - reboot, change them, or use Retry now.");
    } else {
        Serial.printf("[Conn] Next STA retry in %lu s (%s)\n",
                      (unsigned long)(_apRetryDelayMs / 1000), joinResultName(why));
    }
    _attemptStartedMs = now;      // doubles as the AP-retry clock
}

void ConnectivityManager::enterDegraded(const char *why) {
    setState(ConnState::DEGRADED, LinkType::NONE);
    Serial.printf("[Conn] DEGRADED: %s\n", why);
}

// ---------------------------------------------------------------------------
// Lifecycle
// ---------------------------------------------------------------------------
bool ConnectivityManager::begin() {
    s_instance = this;
    loadConfigFromNvs();

    ConnMode mode = (ConnMode)_mode.load();

    if (mode == ConnMode::OFF) {
        setState(ConnState::RADIO_OFF, LinkType::NONE);
        WiFi.mode(WIFI_OFF);
        Serial.println("[Conn] Connectivity disabled (mode OFF). Local-only operation.");
        return false;
    }

    // Recovery escape hatch for STA_ONLY. Without it, a user who selects
    // STA_ONLY and then changes their AP password has bricked the panel's
    // network config with no way back in. Holding BOOT during startup forces
    // AP fallback for this boot only; nothing is written to NVS.
    if (mode == ConnMode::STA_ONLY && bootButtonHeld()) {
        _apFallbackForced = true;
        Serial.println("[Conn] BOOT held - forcing AP fallback for this boot (STA_ONLY overridden).");
    }

    WiFi.onEvent(wifiEventTrampoline);
    WiFi.persistent(false);           // NVS is ours, not the WiFi driver's
    WiFi.setAutoReconnect(false);     // this class owns retry policy

    // The hostname MUST be set before WiFi.mode(), and this is subtle enough
    // to be worth spelling out.
    //
    // WiFi.setHostname() does NOT set the interface's hostname. It writes a
    // global `default_hostname[32]` in NetworkManager and returns true
    // unconditionally. That default is copied onto the STA netif only when the
    // netif is CREATED - which is what WiFi.mode() does. Call it afterwards and
    // the string changes but the interface never does, so DHCP keeps
    // advertising the stock esp32s3-XXXXXX name.
    //
    // Worse, WiFi.getHostname() reads back that same global buffer, so it
    // cheerfully reports the name you set while the router shows the old one -
    // which is exactly how this hid until someone checked a DHCP lease table.
    //
    // Note the 32-byte ceiling on that buffer: a long board slug plus a MAC
    // suffix could silently truncate.
    {
        char host[64];
        getHostname(host, sizeof(host));
        WiFi.setHostname(host);
    }

    WiFi.mode(mode == ConnMode::STA_PLUS_AP ? WIFI_AP_STA : WIFI_STA);
    applyRadioTuning();

    if (mode == ConnMode::STA_PLUS_AP) {
        raiseAp("mode STA_PLUS_AP");
    }

    if (!_configured) {
        Serial.println("[Conn] No SSID configured.");
        if (mode == ConnMode::STA_ONLY && !_apFallbackForced) {
            enterDegraded("STA_ONLY with no credentials - configure on-device");
        } else if (raiseAp("no credentials - provisioning")) {
            setState(ConnState::AP_ACTIVE, LinkType::AP);
        } else {
            enterDegraded("no credentials and AP unavailable");
        }
        return false;
    }

    _attempt = 0;
    startStaAttempt();
    return true;
}

void ConnectivityManager::loop() {
    ConnState st = (ConnState)_state.load();
    if (st == ConnState::RADIO_OFF || st == ConnState::BOOT) return;

    uint32_t now = millis();

    // Issue #49. Both are no-ops unless associated, and both run BEFORE the
    // retry machinery below so a link that has just been convicted is acted on
    // in the same pass rather than a loop later.
    pollRssi(now);
    liveness(now);

    // --- re-issue a connect after a transient disconnect ----------------
    // Done here rather than in the event handler so WiFi.begin() is never
    // called re-entrantly from the event task.
    if (_reissuePending.exchange(0)) {
        // Tear the in-flight association down before starting another.
        //
        // WiFi.begin() here failed with ESP_ERR_WIFI_STATE ("cannot set
        // config"); switching to esp_wifi_connect() then failed with
        // ESP_ERR_WIFI_CONN ("sta is connecting, return error") - a cleaner
        // error for the same underlying problem, and still a no-op, so the
        // log claimed a retry that never happened. Both observed on hardware.
        //
        // The resulting ASSOC_LEAVE is our own disconnect and is ignored by
        // the event handler, so this cannot feed itself.
        esp_wifi_disconnect();
        esp_err_t err = esp_wifi_connect();
        if (err != ESP_OK) DBG_WIFI("re-issue failed: %s\n", esp_err_to_name(err));
        else               DBG_WIFI("re-issued association\n");
        return;
    }

    // --- interactive join deadline -------------------------------------
    if (_joinActive.load() && (JoinPhase)_joinPhase.load() == JoinPhase::CONNECTING) {
        if (now - _joinStartedMs > JOIN_TIMEOUT_MS) {
            _joinResult.store((int32_t)JoinResult::FAIL_TIMEOUT);
            _joinPhase.store((int32_t)JoinPhase::REJOINING);
            Serial.println("[Conn] Interactive join timed out - restoring previous network.");
            WiFi.disconnect(false);
            _joinActive.store(0);
            _joinPhase.store((int32_t)JoinPhase::FINISHED);
            _attempt = 0;
            startStaAttempt();          // rejoin the rollback target
        }
        return;                          // a user join owns the radio
    }

    // --- async scan collection ------------------------------------------
    if ((JoinPhase)_joinPhase.load() == JoinPhase::SCANNING) {
        int n = WiFi.scanComplete();
        if (n == WIFI_SCAN_FAILED) {
            _joinPhase.store((int32_t)JoinPhase::SCAN_FAILED);
        } else if (n >= 0) {
            LOCK();
            _scanCount = 0;
            for (int i = 0; i < n && _scanCount < SCAN_MAX_RESULTS; i++) {
                String s = WiFi.SSID(i);
                if (s.length() == 0) continue;              // hidden
                bool dup = false;
                for (int j = 0; j < _scanCount; j++) {
                    if (s.equals(_scan[j].ssid)) { dup = true; break; }
                }
                if (dup) continue;                          // keep strongest only
                strlcpy(_scan[_scanCount].ssid, s.c_str(), sizeof(_scan[_scanCount].ssid));
                _scan[_scanCount].rssi    = (int8_t)WiFi.RSSI(i);
                _scan[_scanCount].secured = (WiFi.encryptionType(i) != WIFI_AUTH_OPEN);
                _scanCount++;
            }
            UNLOCK();
            WiFi.scanDelete();
            _joinPhase.store((int32_t)JoinPhase::SCAN_DONE);
            Serial.printf("[Conn] Scan done: %d unique network(s).\n", _scanCount);
            #ifdef DEBUG_WIFI
            DBG_WIFI("  %d raw hits collapsed to %d unique\n", n, _scanCount);
            for (int i = 0; i < _scanCount; i++) {
                DBG_WIFI("  scan[%d] %-32s %4d dBm  %s\n", i, _scan[i].ssid,
                         (int)_scan[i].rssi, _scan[i].secured ? "secured" : "open");
            }
            #endif
        }
    }

    // --- STA connect deadline / retry escalation ------------------------
    bool giveUp = _giveUpNow.exchange(0) != 0;
    if (st == ConnState::STA_CONNECTING &&
        (giveUp || now - _attemptStartedMs > _defaults.STA_CONNECT_TIMEOUT_MS)) {

        DBG_WIFI("attempt %u ended after %lums (status=%d)%s\n",
                 _attempt, (unsigned long)(now - _attemptStartedMs), (int)WiFi.status(),
                 giveUp ? " - terminal failure, not waiting out the deadline" : " - timed out");

        _reissuePending.store(0);   // do not let a queued re-issue resurrect this attempt
        WiFi.disconnect(false);

        // A terminal failure (wrong password, or an SSID that is simply not
        // there) skips the remaining attempts. Repeating them cannot change
        // the answer, and on a marginal power rail every extra radio burst is
        // another chance to brown out.
        bool exhausted = giveUp || (_attempt >= _defaults.STA_RETRY_COUNT);

        if (!exhausted) {
            startStaAttempt();
        } else {
            escalateAfterFailure(now);
        }
    }

    // --- keep trying STA while parked in AP / DEGRADED -------------------
    if ((st == ConnState::AP_ACTIVE || st == ConnState::DEGRADED) &&
        _configured && !_joinActive.load()) {

        // A terminal auth stop on credentials that have NEVER worked means
        // exactly one thing: they are wrong. No timer will change that, so
        // there is no timer. Only a reboot, a credentials change or the
        // "Retry now" control gets out of here.
        bool blocked = _authStopped.load() && !_proven;

        // Someone attached to our AP is probably mid-configuration; dropping
        // the radio to go chase the home network would be hostile. Defer -
        // but cap it, or a phone silently rejoining a saved network pins us
        // in AP mode forever.
        if (!blocked && _apClients.load() > 0 && _clientDeferMs < CLIENT_DEFER_MAX_MS) {
            uint32_t add = CLIENT_DEFER_MAX_MS - _clientDeferMs;
            if (add > CLIENT_DEFER_STEP_MS) add = CLIENT_DEFER_STEP_MS;
            if (now - _attemptStartedMs > _apRetryDelayMs + _clientDeferMs) {
                _clientDeferMs += add;
                DBG_WIFI("AP client attached - deferring STA retry by %lus (total %lus)\n",
                         (unsigned long)(add / 1000), (unsigned long)(_clientDeferMs / 1000));
            }
        }

        if (!blocked && now - _attemptStartedMs > _apRetryDelayMs + _clientDeferMs) {
            _attempt = 0;
            _authFailCount.store(0);
            _authRound.store(0);
            _authStopped.store(0);     // a scheduled recheck is a genuine fresh look
            _clientDeferMs = 0;
            startStaAttempt();
        }
    }

    // --- AP idle shutdown (mode STA_PLUS_AP only) ------------------------
    if (isApActive() && (ConnMode)_mode.load() == ConnMode::STA_PLUS_AP &&
        _defaults.AP_IDLE_TIMEOUT_MIN > 0 && _apClients.load() == 0) {
        uint32_t idleMs = (uint32_t)_defaults.AP_IDLE_TIMEOUT_MIN * 60000UL;
        if (now - _apLastClientMs > idleMs) {
            stopAp("idle timeout, no clients");
            // The state must stop claiming an AP the instant one stops
            // broadcasting. The header glyph renders AP_ACTIVE/APSTA as amber
            // arcs plus an "AP" badge, which tells the user to go join the
            // setup network - and pointing someone at a network that is not
            // there is worse than showing them the fault.
            ConnState st = (ConnState)_state.load();
            if (st == ConnState::APSTA) {
                // STA is up; the AP was the redundant half. Nothing is wrong.
                setState(ConnState::STA_CONNECTED, LinkType::STA);
            } else if (st == ConnState::AP_ACTIVE) {
                // STA is down and now there is no AP either. Retries continue:
                // DEGRADED is handled alongside AP_ACTIVE by the retry block
                // above, and the next failure re-raises the AP.
                enterDegraded("AP idled down, STA still retrying");
            }
        }
    }
}

// ---------------------------------------------------------------------------
// Link liveness - issue #49.
//
// THE FAULT THIS EXISTS FOR, because it dictates every choice below. Boards
// sat in STA_CONNECTED holding an IP, with WiFi.status() returning
// WL_CONNECTED and WiFi.RSSI() returning the value it read at association,
// while nothing on the LAN could reach them. WS_P4_5 at ~3-4 h, WS_P4_4B at
// ~5-6 h, WS_P4_7B intermittently. The UI ran perfectly throughout.
//
// So: NOTHING HERE MAY ASK THE DRIVER HOW IT IS DOING. Every signal in this
// section is either a round trip that left the board, or a report from a
// layer above about something it failed to reach. WiFi.status() appears
// exactly nowhere in it, and that is the point rather than an oversight.
// ---------------------------------------------------------------------------

uint32_t ConnectivityManager::rssiAgeMs() const {
    uint32_t at = (uint32_t)_rssiAtMs.load();
    if (at == 0) return UINT32_MAX;          // never measured
    return millis() - at;
}

bool ConnectivityManager::isRssiFresh() const {
    return rssiAgeMs() <= _defaults.RSSI_STALE_MS;
}

void ConnectivityManager::pollRssi(uint32_t now) {
    // THIS CALL CAN BLOCK FOR TEN SECONDS, AND ON A SICK P4 IT ALWAYS DOES.
    //
    // MEASURED on WS_P4_4B, 2026-09-20, after 30 hours with a dead C6 link:
    //
    //   E rpc_core: Response not received for [0x126](Req_WifiStaGetApInfo)
    //   [Conn:debug] RSSI read returned 0 - not storing
    //
    // ...every 10 s, forever. WiFi.RSSI() on a P4 is an RPC to the ESP32-C6
    // over SDIO. When that link is dead the request is never answered and the
    // call sits on its timeout - on the LOOP TASK, where LVGL lives. The owner
    // reported the panel "freezing every few seconds, for a few seconds". That
    // was this, and it was introduced by #49's own detection code, on exactly
    // the boards #49 exists for.
    //
    // The comment that used to sit further down said whether a failing read
    // fires during the fault was UNMEASURED. It fires. It is also ruinously
    // expensive, which that note did not anticipate.
    //
    // BACKOFF RATHER THAN REMOVAL, because the failing read is still the
    // cheapest evidence we have that the hosted transport is gone - deleting
    // it would throw away a real signal to fix a cost problem. Each consecutive
    // failure doubles the interval to a five-minute ceiling, so a dead board
    // pays the stall about once every five minutes instead of six times a
    // minute, and a healthy board is completely unaffected.
    uint32_t interval = _defaults.RSSI_POLL_MS;
    for (uint8_t i = 0; i < _rssiFails && interval < RSSI_POLL_MAX_MS; i++) {
        interval *= 2;
    }
    if (interval > RSSI_POLL_MAX_MS) interval = RSSI_POLL_MAX_MS;

    if (_lastRssiPollMs != 0 && (now - _lastRssiPollMs) < interval) return;
    _lastRssiPollMs = now;

    int32_t r = (int32_t)WiFi.RSSI();

    // 0 is what the driver hands back when it has nothing, and it is also a
    // legal-looking RSSI, so it must not be stored as a measurement. Leaving
    // the timestamp alone lets the value age out into SignalBand::NONE, which
    // is the honest answer: we asked and did not get one.
    //
    // Worth watching on the P4 boards specifically. RSSI there is an RPC to
    // the ESP32-C6 over SDIO, so a failing read is itself evidence about the
    // hosted transport - the cheapest liveness signal available, if it fires.
    // Whether it actually does during the fault is UNMEASURED; this logs it
    // so the next soak answers the question instead of us guessing again.
    if (r == 0) {
        // Cap the streak so the shift above cannot run away, and so the
        // interval is a predictable five minutes rather than an accident of
        // how long the board has been broken.
        if (_rssiFails < 16) _rssiFails++;
        DBG_WIFI("RSSI read returned 0 - not storing (age now %lums, "
                 "next poll in %lums)\n",
                 (unsigned long)rssiAgeMs(), (unsigned long)interval);
        return;
    }

    // A real reading means the RPC answered, so the transport is alive and the
    // backoff has nothing left to protect against.
    _rssiFails = 0;

    _rssi.store(r);
    _rssiAtMs.store((int32_t)now);
}

void ConnectivityManager::setHealth(LinkHealth h, const char *why) {
    LinkHealth prev = (LinkHealth)_health.load();
    if (prev == h) return;
    _health.store((int32_t)h);
    // Printed unconditionally, not behind DEBUG_WIFI. The single loudest fact
    // about the original fault was that the log contained no [Conn] lines at
    // all for hours - the connectivity layer never noticed anything, so there
    // was nothing to read. A health transition is exactly the line whose
    // absence was the evidence.
    Serial.printf("[Conn] Link health %s -> %s (%s)\n",
                  linkHealthName(prev), linkHealthName(h), why ? why : "");
}

// Called from the GOT_IP handler and nowhere else. A genuinely new
// association is the ONLY thing entitled to clear the ladder counter - see the
// note in runRecovery()'s last rung for why the failure path must not.
void ConnectivityManager::resetLiveness() {
    _probeFails      = 0;
    _remoteFails.store(0);
    _recoveryRung    = 0;
    _laddersRun      = 0;
    _lastRecoveryMs  = 0;
    _lastProbeMs     = 0;
    _lastEvidenceMs  = millis();
    _probeResult.store(0);
    setHealth(LinkHealth::LINK_UNKNOWN, "fresh association");
}

// --- the probe ------------------------------------------------------------
//
// esp_ping runs its own task and answers through callbacks, so this is split
// across two loop() passes rather than written as the bool-returning call it
// would like to be. ConnectivityManager promises never to block and the UI
// shares its task, so an answer that takes 2 s to arrive has to be collected,
// not waited for.

void ConnectivityManager::probeOnSuccess(esp_ping_handle_t, void *args) {
    static_cast<ConnectivityManager *>(args)->_probeResult.store(1);
}
void ConnectivityManager::probeOnTimeout(esp_ping_handle_t, void *args) {
    static_cast<ConnectivityManager *>(args)->_probeResult.store(2);
}
void ConnectivityManager::probeOnEnd(esp_ping_handle_t, void *args) {
    // A session that produced neither callback still has to resolve, or the
    // collector would wait on it forever and the probe would silently stop
    // being a check at all.
    ConnectivityManager *self = static_cast<ConnectivityManager *>(args);
    int32_t expected = 0;
    self->_probeResult.compare_exchange_strong(expected, 2);
}

// Which host this probe should aim at.
//
// A LADDER, not a single target, and the owner's reasoning for it is exactly
// right: "The router/gateway will always respond to a ping so long as the
// router is turned on. I can't say the same if I were to boot the devices up
// on a different network though, and not all routers will respond to a ping."
//
// There is a second reason he did not raise and it may matter more: **routers
// rate-limit ICMP**. A gateway that normally answers can drop a burst of them
// and look dead for exactly as long as it takes us to convict the link and
// cycle a working radio. Rotating the target means the failures that convict
// must come from three different hosts, which no rate-limiter produces.
//
// The rung is chosen by the consecutive-failure count, so a healthy board
// always asks the gateway (cheapest, most local, nothing leaves the LAN) and
// only a board that is already failing reaches further out. Conviction takes
// PROBE_FAILS_DEAD failures, which is enough to have tried every rung.
//
// Returns false when this rung has no usable address, which is normal - a
// network with no DNS server configured simply skips that rung.
bool ConnectivityManager::probeTarget(uint8_t rung, IPAddress &out) const {
    switch (rung % 3) {
    case 0:
        out = WiFi.gatewayIP();
        break;
    case 1:
        // The DNS server. Usually the gateway again on a home LAN, in which
        // case this rung is a free retry rather than a new host - still useful
        // against rate-limiting, and genuinely a different host wherever DNS
        // is a separate box (as it is here: gw .1, dns .60).
        out = WiFi.dnsIP();
        break;
    default:
        // Off-LAN, and the only rung that leaves the building.
        //
        // Reached only after the two local rungs have both failed, so a
        // standalone or air-gapped fleet never sends it: those boards fail all
        // three, _probeEverWorked stays false, and the never-answered guard
        // suppresses every verdict. That is the correct outcome for a board
        // with genuinely nothing to talk to.
        out = IPAddress(PROBE_FALLBACK_A, PROBE_FALLBACK_B,
                        PROBE_FALLBACK_C, PROBE_FALLBACK_D);
        break;
    }
    return out != IPAddress((uint32_t)0);
}

void ConnectivityManager::probeStart() {
    if (_ping) return;                        // one in flight is enough

    IPAddress tgt;
    if (!probeTarget(_probeFails, tgt)) {
        DBG_WIFI("probe rung %u has no address - skipping\n", (unsigned)(_probeFails % 3));
        // Count it, so a rung that never has an address cannot stall the
        // ladder on itself forever.
        if (_probeFails < 255) _probeFails++;
        return;
    }

    ip_addr_t target;
    // IPv4 only. The fleet has never been on a v6 network and a v6 literal
    // here would silently probe nothing.
    IP_ADDR4(&target, tgt[0], tgt[1], tgt[2], tgt[3]);

    esp_ping_config_t cfg = ESP_PING_DEFAULT_CONFIG();
    cfg.target_addr  = target;
    cfg.count        = 1;                     // one round trip is the question
    cfg.timeout_ms   = _defaults.PROBE_TIMEOUT_MS;
    cfg.task_stack_size = 2560;               // default is 2048; margin for the callbacks
    cfg.task_prio    = 2;                     // below the WiFi task, above idle

    esp_ping_callbacks_t cbs = {
        .cb_args        = this,
        .on_ping_success = probeOnSuccess,
        .on_ping_timeout = probeOnTimeout,
        .on_ping_end     = probeOnEnd,
    };

    _probeResult.store(0);
    if (esp_ping_new_session(&cfg, &cbs, &_ping) != ESP_OK) {
        _ping = nullptr;
        DBG_WIFI("probe session could not be created\n");
        return;
    }
    if (esp_ping_start(_ping) != ESP_OK) {
        esp_ping_delete_session(_ping);
        _ping = nullptr;
        DBG_WIFI("probe could not be started\n");
        return;
    }
    _probeLast = tgt;
    DBG_WIFI("probe rung %u -> %s\n", (unsigned)(_probeFails % 3), tgt.toString().c_str());
}

bool ConnectivityManager::probeCollect(bool &okOut) {
    if (!_ping) return false;
    int32_t r = _probeResult.load();
    if (r == 0) return false;                 // still in flight

    okOut = (r == 1);
    esp_ping_stop(_ping);
    esp_ping_delete_session(_ping);           // frees the task; not optional
    _ping = nullptr;
    _probeResult.store(0);
    return true;
}

void ConnectivityManager::noteRemoteUnreachable() {
    // Only meaningful while we believe we are online. Off-link this says
    // nothing we do not already know.
    if (!isOnline()) return;
    _remoteFails.fetch_add(1);
}

void ConnectivityManager::noteRemoteReachable() {
    // The strongest evidence available: a packet came back from off-device.
    // It outranks a failed probe, so it clears the counters outright.
    _remoteFails.store(0);
    _probeFails = 0;
    _lastEvidenceMs = millis();
    if ((LinkHealth)_health.load() != LinkHealth::LINK_HEALTHY) {
        setHealth(LinkHealth::LINK_HEALTHY, "traffic from off-device");
    }
}

void ConnectivityManager::liveness(uint32_t now) {
    if (!isOnline()) return;
    if (!_defaults.PROBE_ENABLED) return;

    // 1. Collect an outstanding probe before scheduling another.
    bool ok = false;
    if (probeCollect(ok)) {
        if (ok) {
            // Latched for the life of the boot, never cleared by resetLiveness():
            // "does ICMP work on this network" is a property of the network, and
            // one answered reply proves it for good. Clearing it on every
            // re-association would re-arm the never-worked guard exactly when
            // the probe is most needed.
            // Logged once, because "is the liveness probe usable on this
            // network" is the single fact that decides whether any of this
            // can work, and it should not have to be inferred from silence.
            if (!_probeEverWorked) {
                Serial.printf("[Conn] %s answers ICMP - liveness probe armed.\n",
                              _probeLast.toString().c_str());
            }
            _probeEverWorked = true;
            _probeFails      = 0;
            _lastEvidenceMs  = now;
            if ((LinkHealth)_health.load() != LinkHealth::LINK_HEALTHY) {
                setHealth(LinkHealth::LINK_HEALTHY, "probe answered");
            }
            _remoteFails.store(0);
        } else {
            if (_probeFails < 255) _probeFails++;

            // NEVER CONVICT ON AN INSTRUMENT THAT HAS NEVER WORKED.
            //
            // Plenty of routers and APs drop ICMP by policy. On such a network
            // every probe fails from the first second of the first boot, and
            // without this guard the fix for #49 would be far worse than the
            // fault: every board would declare its own healthy link dead
            // within minutes and start cycling its radio, forever.
            //
            // So a probe only becomes evidence once it has been seen to
            // succeed at least once on this network. Until then a failure
            // means "this test does not work here", not "the link is down" -
            // which is the same discipline as this project's oldest rule,
            // pointed at our own instrument instead of at the device.
            if (!_probeEverWorked) {
                if (_probeFails == _defaults.PROBE_FAILS_SUSPECT) {
                    Serial.println("[Conn] Gateway does not answer ICMP - liveness probing "
                                   "disabled. Link health will rely on traffic alone.");
                }
                // Keep counting (the number is in the dump and tells the story)
                // but draw no conclusion from it.
                return;
            }

            Serial.printf("[Conn] Gateway probe failed (%u consecutive, %lus since last reply)\n",
                          (unsigned)_probeFails,
                          (unsigned long)((now - _lastEvidenceMs) / 1000));

            if (_probeFails >= _defaults.PROBE_FAILS_DEAD) {
                setHealth(LinkHealth::LINK_DEAD, "no probe target answers");
            } else if (_probeFails >= _defaults.PROBE_FAILS_SUSPECT) {
                setHealth(LinkHealth::LINK_SUSPECT, "probe not answering");
            }
        }
    }

    // 2. Corroborating evidence from above. This can raise suspicion and bring
    //    the next probe forward, but it never convicts on its own - a broker
    //    that is switched off is not a broken network, and cycling the radio
    //    under it would drop a perfectly good link.
    if ((int32_t)_remoteFails.load() >= (int32_t)_defaults.REMOTE_FAILS_SUSPECT) {
        if ((LinkHealth)_health.load() == LinkHealth::LINK_HEALTHY ||
            (LinkHealth)_health.load() == LinkHealth::LINK_UNKNOWN) {
            setHealth(LinkHealth::LINK_SUSPECT, "a remote host stopped answering");
        }
    }

    // 3. Schedule - and the important word is DEMAND. A probe is only sent
    //    when nothing else has produced evidence recently.
    //
    //    This matters more than it looks. esp_ping spawns a task, and a task
    //    stack comes out of INTERNAL RAM - the scarcest thing on this fleet
    //    (docs/LESSONS.md: "Internal heap is the scarcest thing on this fleet,
    //    and nothing announces it"). On CYD_S3_3248, the only QSPI board and
    //    so the only one whose LVGL buffers are also internal, spending 2.5 KB
    //    of it every sixty seconds forever would be a real cost paid by the
    //    one board that has never exhibited the fault.
    //
    //    A board with a healthy broker session refreshes its evidence from
    //    ordinary MQTT keepalive traffic and therefore sends NO probes at all.
    //    The probe exists for the cases that traffic cannot cover: MQTT off,
    //    MQTT down, or a link that just went quiet.
    uint32_t gap = _defaults.PROBE_INTERVAL_MS;
    LinkHealth h = (LinkHealth)_health.load();
    // Once suspicious, probe four times as often: conviction takes ~90 s from
    // the first missed probe rather than four minutes.
    if (h == LinkHealth::LINK_SUSPECT || h == LinkHealth::LINK_DEAD) gap /= 4;

    // A network that has never answered a probe gets checked rarely rather
    // than every minute forever. Without this the "probing disabled" line
    // logged above would be false - we would keep spending a task stack on an
    // instrument we had already decided not to believe, which is both a waste
    // on the board that can least afford it and a message that does not match
    // the behaviour. It stays on a slow retry rather than stopping outright
    // because the board may be moved to a different network without rebooting.
    if (!_probeEverWorked && _probeFails >= _defaults.PROBE_FAILS_DEAD) {
        gap = 30UL * 60UL * 1000UL;
    }

    // CALIBRATION, and it is not optional - without it the guard above becomes
    // a permanent gag on exactly the boards that need the probe most.
    //
    // The demand-driven rule says "do not probe while other evidence is
    // arriving", and a board with a healthy broker produces that evidence
    // continuously. So it would never probe, _probeEverWorked would never
    // latch, and the first probe of its life would be the one fired the moment
    // MQTT went down - which the never-worked guard would then correctly
    // refuse to believe. The link would be dead, the probe would say so, and
    // nothing would act.
    //
    // So until the instrument has proved itself once, probe on the ordinary
    // interval regardless of other evidence. That is at most PROBE_FAILS_DEAD
    // probes on a network that drops ICMP, one probe on a network that does
    // not, and then demand-driven behaviour for the rest of the boot.
    const bool calibrating   = !_probeEverWorked && _probeFails < _defaults.PROBE_FAILS_DEAD;
    const bool evidenceStale = (now - _lastEvidenceMs) >= gap;
    const bool dueAnyway     = (_lastProbeMs == 0) || ((now - _lastProbeMs) >= gap);

    if (!_ping && dueAnyway && (evidenceStale || calibrating)) {
        _lastProbeMs = now;
        probeStart();
    }

    // 4. Act.
    if (h == LinkHealth::LINK_DEAD) runRecovery(now);
}

// --- recovery -------------------------------------------------------------
//
// Escalating, one rung per RECOVERY_STEP_MS, because the cheapest action that
// works should be the one that runs. A router rebooting fixes itself; there is
// no reason to cycle the radio at it.
void ConnectivityManager::runRecovery(uint32_t now) {
    // Cool-off. Each completed ladder widens the gap before the next one, so a
    // board whose problem is upstream - the router is off, the uplink is down,
    // the AP has gone - stops cycling its radio every few minutes and settles
    // into checking occasionally.
    //
    // It converges on "keep the UI alive, keep looking, stop thrashing", which
    // is the right behaviour for a wall panel: the fault is not ours to fix
    // and the owner will notice the house's network before they notice us.
    uint32_t step = _defaults.RECOVERY_STEP_MS;
    for (uint8_t i = 0; i < _laddersRun && step < 15UL * 60UL * 1000UL; i++) step *= 2;

    if (_lastRecoveryMs != 0 && (now - _lastRecoveryMs) < step) return;
    _lastRecoveryMs = now;

    switch (_recoveryRung) {
    case 0:
        // Re-associate. Cheapest thing that could possibly work, and on the
        // evidence the most likely: the association is what died.
        Serial.println("[Conn] RECOVERY 1/3: re-associating (link confirmed dead).");
        esp_wifi_disconnect();
        esp_wifi_connect();
        _recoveryRung = 1;
        break;

    case 1:
        // Full radio cycle. On the P4 boards this also re-runs the esp_hosted
        // bring-up to the ESP32-C6, which is the layer most suspected of being
        // what actually fails - see issue #49's boot-time RPC timeout.
        Serial.println("[Conn] RECOVERY 2/3: cycling the radio.");
        WiFi.disconnect(true);
        WiFi.mode(WIFI_OFF);
        WiFi.mode(WIFI_STA);
        applyRadioTuning();
        _gotIp.store(0);
        _attempt = 0;
        _attemptPub.store(0);
        startStaAttempt();
        _recoveryRung = 2;
        break;

    default:
        // Give the state machine the fault properly, so AP fallback and the
        // normal retry ladder take over. From here the board behaves exactly
        // as it would after any other failed association - which is the
        // behaviour that was missing entirely, because nothing ever told it
        // the link had gone.
        if (_laddersRun < 255) _laddersRun++;
        Serial.printf("[Conn] RECOVERY 3/3: handing back to the state machine "
                      "(ladder %u since the last good link).\n", (unsigned)_laddersRun);
        _gotIp.store(0);
        enterDegraded("link dead and recovery exhausted");
        _recoveryRung = 0;
        // Deliberately NOT resetLiveness() - that would zero _laddersRun and
        // throw away the cool-off this branch just earned. Only a genuinely
        // restored link (GOT_IP) is allowed to declare the slate clean.
        _probeFails   = 0;
        _remoteFails.store(0);
        _probeResult.store(0);
        setHealth(LinkHealth::LINK_UNKNOWN, "recovery exhausted, retrying from scratch");
        escalateAfterFailure(now);
        break;
    }
}

// ---------------------------------------------------------------------------
// Events - runs on the Arduino event task. Never touches LVGL.
// ---------------------------------------------------------------------------
void ConnectivityManager::captureLinkInfo() {
    LOCK();
    strlcpy(_ssid, WiFi.SSID().c_str(), sizeof(_ssid));
    UNLOCK();
    _rssi.store((int32_t)WiFi.RSSI());
    _rssiAtMs.store((int32_t)millis());
}

void ConnectivityManager::handleEvent(int32_t eventId, void *infoPtr) {
    arduino_event_info_t *info = (arduino_event_info_t *)infoPtr;

    switch ((arduino_event_id_t)eventId) {

    case ARDUINO_EVENT_WIFI_STA_GOT_IP: {
        // GOT_IP alone is not proof that OUR join succeeded: a stale reconnect
        // to the previous network can land mid-join. Only accept it when the
        // live association matches what was actually requested. (This exact
        // trap is documented in the NINA project's wifi_join.c.)
        if (_joinActive.load()) {
            char want[33];
            LOCK(); strlcpy(want, _joinSsid, sizeof(want)); UNLOCK();
            if (!WiFi.SSID().equals(want)) {
                Serial.printf("[Conn] Ignoring GOT_IP from \"%s\" - join targeted \"%s\".\n",
                              WiFi.SSID().c_str(), want);
                break;
            }
            setStationCredentials(want, _joinPass);   // persist only on real success
            _joinResult.store((int32_t)JoinResult::SUCCESS);
            _joinPhase.store((int32_t)JoinPhase::FINISHED);
            _joinActive.store(0);
        }
        _gotIp.store(1);
        _attempt = 0;
        // A working link clears the failure history, so a later outage starts
        // from the short backoff again rather than inheriting a 30-minute wait.
        _apRetryDelayMs = 0;
        _clientDeferMs  = 0;
        _authFailCount.store(0);
        _authRound.store(0);
        _authStopped.store(0);
        _lastFailure.store((int32_t)JoinResult::NONE);
        // These credentials demonstrably work. From here on an auth rejection
        // means the AP's password changed rather than a typo, which earns a
        // periodic recheck instead of a permanent stop.
        if (!_proven) markProven(true);
        captureLinkInfo();
        // A new association is a clean slate for liveness: the probe counters,
        // the recovery rung and any lingering LINK_DEAD verdict all belong to
        // the link that just went away, not to this one. Without this a board
        // that recovered would carry its own conviction into the new session
        // and immediately start the ladder again.
        resetLiveness();
        _attemptPub.store(0);
        applyRadioTuning();                            // re-apply on every link-up
        setState(isApActive() ? ConnState::APSTA : ConnState::STA_CONNECTED,
                 isApActive() ? LinkType::APSTA : LinkType::STA);
        Serial.printf("[Conn] Online: %s  IP %s  RSSI %d dBm\n",
                      WiFi.SSID().c_str(), WiFi.localIP().toString().c_str(), (int)WiFi.RSSI());
        DBG_WIFI("  gw=%s mask=%s dns=%s ch=%d hostname=%s\n",
                 WiFi.gatewayIP().toString().c_str(), WiFi.subnetMask().toString().c_str(),
                 WiFi.dnsIP().toString().c_str(), (int)WiFi.channel(), WiFi.getHostname());

        // Mode 1 raised the AP only as a fallback; the link is back, so drop it.
        if (isApActive() && (ConnMode)_mode.load() == ConnMode::STA_WITH_AP_FALLBACK) {
            stopAp("STA restored");
            setState(ConnState::STA_CONNECTED, LinkType::STA);
        }
        break;
    }

    case ARDUINO_EVENT_WIFI_STA_DISCONNECTED: {
        uint8_t reason = info ? info->wifi_sta_disconnected.reason : 0;
        _lastReason.store((int32_t)reason);
        _gotIp.store(0);
        DBG_WIFI("STA_DISCONNECTED raw reason=%u -> %s (joinActive=%d)\n",
                 reason, joinResultName(classifyDisconnect(reason)), (int)_joinActive.load());

        if (_joinActive.load()) {
            JoinResult r = classifyDisconnect(reason);
            if (r != JoinResult::NONE) {
                Serial.printf("[Conn] Join failed: %s (reason %u)\n", joinResultName(r), reason);
                _joinResult.store((int32_t)r);
                _joinPhase.store((int32_t)JoinPhase::REJOINING);
                _joinActive.store(0);
                _joinPhase.store((int32_t)JoinPhase::FINISHED);
                _attempt = 0;
                startStaAttempt();      // roll back to the previous network
            }
            // Transient reason: let the join deadline in loop() decide.
            break;
        }

        ConnState cur = (ConnState)_state.load();

        if (cur == ConnState::STA_CONNECTING) {
            // ASSOC_LEAVE is OUR OWN disconnect - the give-up path calls
            // WiFi.disconnect() and the resulting event used to set the
            // re-issue flag, immediately restarting a connect we had just
            // abandoned. Never treat a self-inflicted disconnect as a reason
            // to retry.
            if (reason == WIFI_REASON_ASSOC_LEAVE) break;

            JoinResult r = classifyDisconnect(reason);

            if (r == JoinResult::FAIL_AUTH) {
                _authFailCount.fetch_add(1);
                _lastFailure.store((int32_t)JoinResult::FAIL_AUTH);

                if (_authFailCount.load() < AUTH_FAILS_PER_ROUND) {
                    _reissuePending.store(1);
                    DBG_WIFI("auth failure %d/%d this round\n",
                             (int)_authFailCount.load(), (int)AUTH_FAILS_PER_ROUND);
                    break;
                }

                // Round over. A second round gets a pause first, because a
                // router that is briefly unhappy presents exactly like a wrong
                // password and deserves one more look before being written off.
                _reissuePending.store(0);
                if (_authRound.load() + 1 < AUTH_ROUNDS) {
                    _authRound.fetch_add(1);
                    Serial.printf("[Conn] Auth rejected (reason %u) - retrying once "
                                  "in %lu s before giving up.\n",
                                  reason, (unsigned long)(SECOND_CHANCE_MS / 1000));
                } else {
                    Serial.printf("[Conn] Authentication rejected (reason %u) - "
                                  "credentials are wrong.\n", reason);
                    _authStopped.store(1);
                }
                _giveUpNow.store(1);        // loop() escalates immediately
                break;
            }

            if (r == JoinResult::FAIL_NO_AP) {
                Serial.printf("[Conn] Network \"not found\" (reason %u).\n", reason);
                _lastFailure.store((int32_t)JoinResult::FAIL_NO_AP);
                // Worth a couple of retries: a busy channel can mask a beacon.
                if (_reissueCount.load() < 2) {
                    _reissueCount.fetch_add(1);
                    _reissuePending.store(1);
                } else {
                    _giveUpNow.store(1);
                }
                break;
            }

            // Genuinely transient. Routers commonly answer a first association
            // with AUTH_EXPIRE (reason 2) and accept the immediate retry -
            // observed on real hardware, where waiting out the 15 s deadline
            // instead cost ~13 s of avoidable boot time.
            if (_reissueCount.load() < 6) {
                _reissueCount.fetch_add(1);
                _reissuePending.store(1);
                DBG_WIFI("transient disconnect (reason %u) - re-issuing (%d/6)\n",
                         reason, (int)_reissueCount.load());
            }
            break;
        }

        if (cur == ConnState::STA_CONNECTED || cur == ConnState::APSTA) {
            Serial.printf("[Conn] Link lost (reason %u) - reconnecting.\n", reason);
            _attempt = 0;
            startStaAttempt();
        }
        break;
    }

    case ARDUINO_EVENT_WIFI_AP_START:
        _apActive.store(1);
        break;

    case ARDUINO_EVENT_WIFI_AP_STOP:
        _apActive.store(0);
        _apClients.store(0);
        break;

    case ARDUINO_EVENT_WIFI_AP_STACONNECTED:
        _apClients.fetch_add(1);
        _apLastClientMs = millis();
        Serial.printf("[Conn] AP client connected (%d total)\n", (int)_apClients.load());
        break;

    case ARDUINO_EVENT_WIFI_AP_STADISCONNECTED:
        if (_apClients.load() > 0) _apClients.fetch_sub(1);
        _apLastClientMs = millis();
        break;

    default:
        break;
    }
}

// ---------------------------------------------------------------------------
// Interactive scan / join
// ---------------------------------------------------------------------------
bool ConnectivityManager::startScan() {
    JoinPhase ph = (JoinPhase)_joinPhase.load();
    if (ph == JoinPhase::SCANNING || ph == JoinPhase::CONNECTING) return false;
    if ((ConnState)_state.load() == ConnState::RADIO_OFF) return false;

    WiFi.scanDelete();
    _joinPhase.store((int32_t)JoinPhase::SCANNING);
    // Async: the LVGL task must never block on a scan (2-3 s of dead UI).
    WiFi.scanNetworks(/*async=*/true, /*show_hidden=*/false);
    return true;
}

int ConnectivityManager::getScanResults(WiFiScanEntry *out, int max) const {
    if (!out || max <= 0) return 0;
    if (_mutex) xSemaphoreTake(_mutex, portMAX_DELAY);
    int n = (_scanCount < max) ? _scanCount : max;
    memcpy(out, _scan, (size_t)n * sizeof(WiFiScanEntry));
    if (_mutex) xSemaphoreGive(_mutex);
    return n;
}

bool ConnectivityManager::startConnect(const char *ssid, const char *password) {
    if (!ssid || !ssid[0]) return false;
    JoinPhase ph = (JoinPhase)_joinPhase.load();
    if (ph == JoinPhase::SCANNING || ph == JoinPhase::CONNECTING) return false;

    // Rollback context, captured before anything is torn down.
    LOCK();
    strlcpy(_prevSsid, _ssid, sizeof(_prevSsid));
    strlcpy(_prevPass, _password, sizeof(_prevPass));
    strlcpy(_joinSsid, ssid, sizeof(_joinSsid));
    strlcpy(_joinPass, password ? password : "", sizeof(_joinPass));
    UNLOCK();

    _joinResult.store((int32_t)JoinResult::NONE);
    _joinPhase.store((int32_t)JoinPhase::CONNECTING);
    _joinActive.store(1);
    _joinStartedMs = millis();
    _gotIp.store(0);

    char host[64];
    getHostname(host, sizeof(host));
    WiFi.setHostname(host);
    WiFi.disconnect(false);
    setState(ConnState::STA_CONNECTING, isApActive() ? LinkType::APSTA : LinkType::STA);
    Serial.printf("[Conn] Interactive join -> \"%s\"\n", ssid);
    WiFi.begin(ssid, password ? password : "");
    return true;
}

void ConnectivityManager::cancelJoin() {
    if (!_joinActive.load()) return;
    _joinResult.store((int32_t)JoinResult::FAIL_CANCELLED);
    _joinActive.store(0);
    _joinPhase.store((int32_t)JoinPhase::FINISHED);
    WiFi.disconnect(false);
    LOCK();
    strlcpy(_ssid, _prevSsid, sizeof(_ssid));
    strlcpy(_password, _prevPass, sizeof(_password));
    UNLOCK();
    _attempt = 0;
    startStaAttempt();
    Serial.println("[Conn] Join cancelled - restoring previous network.");
}

void ConnectivityManager::ackJoinResult() {
    if ((JoinPhase)_joinPhase.load() == JoinPhase::FINISHED) {
        _joinPhase.store((int32_t)JoinPhase::IDLE);
        _joinResult.store((int32_t)JoinResult::NONE);
    }
}

// ---------------------------------------------------------------------------
// Diagnostics
// ---------------------------------------------------------------------------
void ConnectivityManager::dumpStatus(Print &out) const {
    char ssid[33], host[64];
    getSsid(ssid, sizeof(ssid));
    getHostname(host, sizeof(host));

    out.println("[CONNECTIVITY]");
    out.printf("  Mode: %s\n", connModeName(getMode()));
    out.printf("  State: %s (%s)\n", connStateName(getState()), linkTypeName(getLinkType()));
    out.printf("  Hostname: %s\n", host);
    out.printf("  Device ID: %s\n", DeviceIdentity::deviceId());
    if (isOnline()) {
        out.printf("  SSID: %s\n", ssid);
        out.printf("  IP: %s\n", WiFi.localIP().toString().c_str());
        out.printf("  Gateway: %s\n", WiFi.gatewayIP().toString().c_str());
        // The age is printed beside the value on purpose. "RSSI: -42 dBm" was
        // a true statement for six hours while the board was off the network
        // (issue #49); "-42 dBm (measured 21341 s ago)" could never have been
        // read that way by anyone.
        uint32_t age = rssiAgeMs();
        if (age == UINT32_MAX) {
            out.println("  RSSI: never measured");
        } else {
            out.printf("  RSSI: %d dBm (measured %lus ago%s)\n", (int)getRssi(),
                       (unsigned long)(age / 1000), isRssiFresh() ? "" : ", STALE");
        }
        out.printf("  Link health: %s\n", linkHealthName(getLinkHealth()));
        out.printf("  Gateway probe: %s, %u consecutive failure(s), evidence %lus old\n",
                   _probeEverWorked ? "working" : "NEVER ANSWERED (verdicts suppressed)",
                   (unsigned)_probeFails,
                   (unsigned long)((millis() - _lastEvidenceMs) / 1000));
        if (_laddersRun) out.printf("  Recovery ladders run: %u\n", (unsigned)_laddersRun);
        out.printf("  Remote-unreachable reports: %ld\n", (long)_remoteFails.load());
    } else {
        out.printf("  Last disconnect reason: %u\n", getLastDisconnectReason());
    }
    if (isApActive()) {
        out.printf("  AP: %s (pass %s) at %s, %u client(s)\n",
                   getApSsid(), getApPassword(),
                   WiFi.softAPIP().toString().c_str(), getApClientCount());
    } else {
        out.println("  AP: down");
    }
}
