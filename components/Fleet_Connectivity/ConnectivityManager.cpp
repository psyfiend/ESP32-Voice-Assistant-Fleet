#include "ConnectivityManager.h"
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

// How often to re-attempt STA while parked in AP fallback. Without this a
// device whose router rebooted would sit in AP mode until someone power-cycled
// it - the failure mode that makes AP fallback feel broken rather than helpful.
static const uint32_t RETRY_WHILE_AP_MS = 60000;
static const uint32_t JOIN_TIMEOUT_MS   = 20000;
static const uint32_t BOOT_HOLD_MS      = 1500;

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
    Serial.printf("[Conn] TX power capped to %u dBm (%s)\n",
                  _defaults.TX_POWER_DBM, err == ESP_OK ? "ok" : esp_err_to_name(err));
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

    // Hostname MUST be applied before WiFi.begin(). Setting it afterwards
    // silently has no effect until the next reconnect - a classic time-waster.
    WiFi.setHostname(host);

    _gotIp.store(0);
    _reissueCount.store(0);
    _reissuePending.store(0);
    _attemptStartedMs = millis();
    _attempt++;
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

    WiFi.mode((_mode.load() == (int32_t)ConnMode::STA_PLUS_AP ||
               (ConnState)_state.load() == ConnState::STA_CONNECTED)
              ? WIFI_AP_STA : WIFI_AP_STA);
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

    // --- re-issue a connect after a transient disconnect ----------------
    // Done here rather than in the event handler so WiFi.begin() is never
    // called re-entrantly from the event task.
    if (_reissuePending.exchange(0)) {
        char ssid[33], pass[65];
        LOCK();
        strlcpy(ssid, _ssid, sizeof(ssid));
        strlcpy(pass, _password, sizeof(pass));
        UNLOCK();
        WiFi.begin(ssid, pass);
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
    if (st == ConnState::STA_CONNECTING &&
        now - _attemptStartedMs > _defaults.STA_CONNECT_TIMEOUT_MS) {

        DBG_WIFI("attempt %u timed out after %lums (status=%d)\n",
                 _attempt, (unsigned long)(now - _attemptStartedMs), (int)WiFi.status());
        WiFi.disconnect(false);
        if (_attempt < _defaults.STA_RETRY_COUNT) {
            startStaAttempt();
        } else {
            ConnMode mode = (ConnMode)_mode.load();
            bool wantAp = (mode == ConnMode::STA_WITH_AP_FALLBACK) ||
                          (mode == ConnMode::STA_PLUS_AP) ||
                          _apFallbackForced;
            if (wantAp && raiseAp("STA retries exhausted")) {
                setState(ConnState::AP_ACTIVE, LinkType::AP);
                _attemptStartedMs = now;      // reuse as the AP-retry clock
            } else {
                enterDegraded(mode == ConnMode::STA_ONLY
                              ? "STA_ONLY: retries exhausted, no AP by policy"
                              : "retries exhausted and AP unavailable");
                _attemptStartedMs = now;
            }
        }
    }

    // --- keep trying STA while parked in AP / DEGRADED -------------------
    if ((st == ConnState::AP_ACTIVE || st == ConnState::DEGRADED) &&
        _configured && !_joinActive.load() &&
        now - _attemptStartedMs > RETRY_WHILE_AP_MS) {
        _attempt = 0;
        startStaAttempt();
    }

    // --- AP idle shutdown (mode STA_PLUS_AP only) ------------------------
    if (isApActive() && (ConnMode)_mode.load() == ConnMode::STA_PLUS_AP &&
        _defaults.AP_IDLE_TIMEOUT_MIN > 0 && _apClients.load() == 0) {
        uint32_t idleMs = (uint32_t)_defaults.AP_IDLE_TIMEOUT_MIN * 60000UL;
        if (now - _apLastClientMs > idleMs) {
            stopAp("idle timeout, no clients");
            if ((ConnState)_state.load() == ConnState::APSTA) {
                setState(ConnState::STA_CONNECTED, LinkType::STA);
            }
        }
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
        captureLinkInfo();
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
            // Transient reason mid-attempt. Routers commonly answer the first
            // association with AUTH_EXPIRE (reason 2) and accept the immediate
            // retry - observed on real hardware, where waiting out the 15 s
            // deadline instead of re-issuing cost ~13 s of avoidable boot time.
            // Bounded so a flapping AP cannot spin here.
            if (_reissueCount.load() < 6) {
                _reissueCount.fetch_add(1);
                _reissuePending.store(1);
                DBG_WIFI("transient disconnect mid-attempt - re-issuing connect (%d/6)\n",
                         (int)_reissueCount.load());
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
        Serial.printf("[Conn] AP client connected (%d total).\n", (int)_apClients.load());
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
        out.printf("  RSSI: %d dBm\n", (int)getRssi());
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
