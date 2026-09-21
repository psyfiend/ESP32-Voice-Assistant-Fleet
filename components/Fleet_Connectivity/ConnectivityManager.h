#pragma once
#ifndef CONNECTIVITY_MANAGER_H
#define CONNECTIVITY_MANAGER_H

#include <Arduino.h>
#include <IPAddress.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <atomic>
// ICMP gateway probe. Already in liblwip.a and on the include path for every
// environment (verified against esp32p4_es and esp32s3 - the symbol
// esp_ping_new_session is in the archive and lwip is in flags/ld_libs), so
// this costs no new dependency.
#include <ping/ping_sock.h>
#include "ConnectivityTypes.h"
#include "ConnectivityDefaults.h"
#include "DeviceIdentity.h"

// ---------------------------------------------------------------------------
// ConnectivityManager - the fleet's link layer.
//
// Owns WiFi station mode, the fallback/provisioning AP, the four connectivity
// modes from docs/ROADMAP.md Q1, and the interactive scan/join flow the
// settings UI drives. Ethernet is represented in the type vocabulary
// (LinkType) but not implemented yet - callers ask isOnline()/getLinkType()
// rather than "is wifi up", so adding it later does not churn call sites.
//
// THREADING - read this before calling anything from UI code.
//
//   * Every method here is non-blocking. begin() kicks the state machine off
//     and returns immediately; loop() advances deadlines, retries and the AP
//     idle timer. Nothing spins on WiFi.status().
//   * WiFi events arrive on the Arduino event task, NOT the LVGL task. The
//     event handler is the single authority for state transitions; it never
//     touches LVGL and never calls back into UI code.
//   * The UI therefore POLLS the getters (cheap, all atomic or mutex-guarded)
//     rather than being called back. This is deliberate: a callback into the
//     UI layer from the event task would need LVGL locking at every call site
//     and would invert the dependency. Same conclusion both reference projects
//     reached independently - see docs/ROADMAP.md section 4.2.
//
// A NOTE ON ATOMIC WIDTH: scalars shared across tasks are int32_t-backed, not
// uint8_t. Sub-word atomics on RISC-V (i.e. the P4 boards) can read-modify-write
// neighbouring bytes in the same word. Borrowed from the ESP32-P4-NINA-Display
// project, which documents the same hazard.
// ---------------------------------------------------------------------------

class ConnectivityManager {
public:
    ConnectivityManager();

    // Applies hostname + TX power, raises the AP if the mode calls for it, and
    // starts the first join attempt. Returns false only if connectivity is
    // disabled or unconfigured - not merely because the link is not up yet.
    bool begin();

    // Drive from loop(). Handles connect deadlines, retry escalation, AP
    // fallback, the AP idle timer, and scan-result collection.
    void loop();

    // --- Link state -------------------------------------------------------
    ConnState   getState() const      { return (ConnState)_state.load(); }
    LinkType    getLinkType() const   { return (LinkType)_link.load(); }
    ConnMode    getMode() const       { return (ConnMode)_mode.load(); }
    bool        isOnline() const;
    IPAddress   getIP() const;
    int8_t      getRssi() const       { return (int8_t)_rssi.load(); }
    uint8_t     getLastDisconnectReason() const { return (uint8_t)_lastReason.load(); }

    // How long ago the RSSI value was actually measured. Before issue #49 this
    // question could not be asked: RSSI was read once in the GOT_IP handler
    // and never again, so getRssi() returned a number from the moment of
    // association for as long as the board stayed up. UINT32_MAX = never read.
    uint32_t    rssiAgeMs() const;
    bool        isRssiFresh() const;

    // NONE when the sample is too old to stand behind, so the header glyph
    // says "I do not know" instead of repeating a six-hour-old reading. That
    // substitution is the whole point - a confident wrong indicator is worse
    // than an honest blank one, which is this project's oldest lesson.
    SignalBand  getSignalBand() const {
        return isRssiFresh() ? signalBandFromRssi(getRssi()) : SignalBand::NONE;
    }

    // --- Link liveness (issue #49) ----------------------------------------

    LinkHealth getLinkHealth() const { return (LinkHealth)_health.load(); }

    // Which attempt of the current cycle is in flight, counting from 1 the way
    // the "[Conn] STA attempt 2/3" log line does - deliberately the same
    // numbering, so the glyph and the serial cannot disagree about which try
    // this is. 0 only before the first attempt.
    //
    // It exists so the UI can tell a first join apart from a board that has
    // been failing to get back on for ten minutes. Both are STA_CONNECTING;
    // they should not look the same to somebody walking past the panel.
    uint8_t    getAttempt() const { return (uint8_t)_attemptPub.load(); }
    bool       isRetrying() const {
        return (ConnState)_state.load() == ConnState::STA_CONNECTING && getAttempt() > 1;
    }

    // Evidence from a layer ABOVE that something beyond this device could not
    // be reached. Called by SystemCore, never by Fleet_MQTT itself - MQTT
    // deliberately knows nothing about the link it rides on (ROADMAP Q9), so
    // the owner of both objects is what joins them up.
    //
    // This is corroborating evidence only. It can raise LINK_SUSPECT and it
    // can trigger an immediate probe, but it never on its own declares the
    // link dead: a broker that is switched off is not a broken network.
    void noteRemoteUnreachable();
    // Anything at all arrived from off-device. The strongest evidence there
    // is, and it clears everything.
    void noteRemoteReachable();

    // Copies into caller storage - the underlying buffers are written from the
    // event task, so returning a pointer would hand out a data race.
    void getSsid(char *out, size_t len) const;
    void getHostname(char *out, size_t len) const;

    // --- Access point -----------------------------------------------------
    bool        isApActive() const { return _apActive.load() != 0; }
    const char *getApSsid() const;
    const char *getApPassword() const;   // shown in Settings; MAC-derived
    IPAddress   getApIP() const;
    uint8_t     getApClientCount() const { return (uint8_t)_apClients.load(); }

    // --- Configuration (NVS-backed, survives reflash) ---------------------
    bool setMode(ConnMode m);
    bool setStationCredentials(const char *ssid, const char *password);
    void clearStationCredentials();
    // Rejects anything that cannot be made into a valid DNS label rather than
    // letting it fail confusingly at the DHCP server. Takes effect next connect.
    bool setHostname(const char *hostname);
    // The escape hatch from every terminal state - "Retry now" in the settings
    // UI, and the only way back from wrong-and-unproven credentials short of a
    // reboot. Resets every counter, backoff and stop flag.
    void retryNow();
    // Have the stored credentials ever successfully connected? Decides what
    // happens AFTER a failed boot attempt, never whether that attempt runs.
    bool areCredentialsProven() const { return _proven; }
    // Append the MAC suffix to hostname and AP SSID. Never touches the MQTT/HA
    // device id - see DeviceIdentity.h.
    bool setAppendMacSuffix(bool on);
    bool getAppendMacSuffix() const { return DeviceIdentity::appendMacSuffix(); }

    // --- Interactive scan / join (settings UI) ----------------------------
    bool       startScan();
    JoinPhase  getJoinPhase() const  { return (JoinPhase)_joinPhase.load(); }
    int        getScanResults(WiFiScanEntry *out, int max) const;
    bool       startConnect(const char *ssid, const char *password);
    void       cancelJoin();
    JoinResult getJoinResult() const { return (JoinResult)_joinResult.load(); }
    // Terminal join states persist until the UI says it has shown them, so a
    // fast failure cannot flash past before the user sees why.
    void       ackJoinResult();

    // --- Diagnostics ------------------------------------------------------
    void dumpStatus(Print &out) const;

    // Internal: the Arduino WiFi event hook. Public only so the free-function
    // trampoline can reach it; do not call from application code.
    void handleEvent(int32_t eventId, void *info);

private:
    static const int SCAN_MAX_RESULTS = 20;

    void  applyRadioTuning();
    void  startStaAttempt();
    bool  raiseAp(const char *why);
    void  stopAp(const char *why);
    void  enterDegraded(const char *why);
    void  loadConfigFromNvs();
    void  setState(ConnState s, LinkType l);
    void  captureLinkInfo();
    static JoinResult classifyDisconnect(uint8_t reason);
    bool  bootButtonHeld() const;
    void  markProven(bool proven);
    void  escalateAfterFailure(uint32_t now);   // decide AP / DEGRADED and the next retry gap

    // --- Link liveness (issue #49) ---
    void  pollRssi(uint32_t now);         // re-read RSSI on a timer, with an age stamp
    void  liveness(uint32_t now);         // probe scheduling + verdict
    void  runRecovery(uint32_t now);      // the ladder, once LINK_DEAD
    void  setHealth(LinkHealth h, const char *why);
    void  resetLiveness();                // called on every fresh association

    // esp_ping is ASYNCHRONOUS - it runs its own task and answers through
    // callbacks - so the probe cannot be a bool-returning function call
    // however much one would read better here. probeStart() kicks one off and
    // returns immediately; probeCollect() picks up the answer on a later
    // loop() pass. Nothing in ConnectivityManager may block: the whole class
    // is built on that promise and the UI runs on the same task.
    // Which host to aim rung N at. See the long note at the definition: the
    // ladder exists because not every router answers ICMP, and because the
    // ones that do will rate-limit it.
    bool  probeTarget(uint8_t rung, IPAddress &out) const;
    void  probeStart();
    bool  probeCollect(bool &okOut);      // true when a verdict is ready
    static void probeOnSuccess(esp_ping_handle_t h, void *args);
    static void probeOnTimeout(esp_ping_handle_t h, void *args);
    static void probeOnEnd(esp_ping_handle_t h, void *args);

    const WiFiDefaults &_defaults;

    // Cross-task scalars. int32_t-backed - see the class comment.
    std::atomic<int32_t> _state{(int32_t)ConnState::BOOT};
    std::atomic<int32_t> _link{(int32_t)LinkType::NONE};
    std::atomic<int32_t> _mode{(int32_t)ConnMode::STA_WITH_AP_FALLBACK};
    std::atomic<int32_t> _rssi{0};
    std::atomic<int32_t> _lastReason{0};
    std::atomic<int32_t> _apActive{0};
    std::atomic<int32_t> _apClients{0};
    std::atomic<int32_t> _joinPhase{(int32_t)JoinPhase::IDLE};
    std::atomic<int32_t> _joinResult{(int32_t)JoinResult::NONE};
    std::atomic<int32_t> _joinActive{0};   // gates auto-retry while a user join runs
    std::atomic<int32_t> _gotIp{0};
    // Set by the event handler on a transient disconnect, consumed by loop().
    // The re-issue itself must not happen inside the event callback - calling
    // into the WiFi stack re-entrantly from the event task is asking for trouble.
    std::atomic<int32_t> _reissuePending{0};
    std::atomic<int32_t> _reissueCount{0};
    // How the current attempt cycle is failing. FAIL_AUTH is terminal: the same
    // credentials will not start working on the next try, so retrying is just
    // noise. Drives both the give-up decision and the AP re-try backoff.
    std::atomic<int32_t> _lastFailure{(int32_t)JoinResult::NONE};
    std::atomic<int32_t> _authFailCount{0};
    // Which of the two boot-time auth rounds we are in. A router in a
    // momentarily odd state can look exactly like a wrong password, so the
    // credentials get a second chance ~45 s later before being written off.
    std::atomic<int32_t> _authRound{0};
    // Set once both rounds have failed on auth. What happens next depends on
    // _proven: an unproven device stops entirely, a proven one rechecks
    // periodically in case the AP's password changed back.
    std::atomic<int32_t> _authStopped{0};
    // Set by the event handler when a failure is terminal, so loop() stops
    // waiting out the remaining 15 s deadline for an answer it already has.
    std::atomic<int32_t> _giveUpNow{0};

    // --- Link liveness (issue #49). Atomic because the UI polls them. ---
    std::atomic<int32_t> _health{(int32_t)LinkHealth::LINK_UNKNOWN};
    // millis() at which _rssi was last actually measured. 0 = never. Paired
    // with _rssi rather than folded into it because "how old" and "how strong"
    // are different questions and the fault was caused by only being able to
    // ask the second one.
    std::atomic<int32_t> _rssiAtMs{0};
    // A publishable copy of _attempt, which is otherwise a plain uint8_t only
    // touched from loop(). The UI reads this one.
    std::atomic<int32_t> _attemptPub{0};
    // Set by noteRemoteUnreachable() from SystemCore's task; consumed by
    // liveness() on the main loop, so the counter itself never races.
    std::atomic<int32_t> _remoteFails{0};

    // Probe result, written from the esp_ping task and read from loop().
    // 0 = no verdict yet, 1 = reply received, 2 = timed out.
    std::atomic<int32_t> _probeResult{0};

    uint32_t _lastRssiPollMs   = 0;

    // Consecutive RSSI reads that returned nothing. Backs the poll interval
    // off, because on a P4 whose C6 link is dead WiFi.RSSI() is an SDIO RPC
    // that blocks the LOOP TASK on a ~10 s timeout every single time - the
    // "freezing every few seconds" the owner saw on WS_P4_4B. See pollRssi().
    uint8_t  _rssiFails        = 0;
    static constexpr uint32_t RSSI_POLL_MAX_MS = 300000;   // 5 minutes
    uint32_t _lastProbeMs      = 0;
    uint32_t _lastEvidenceMs   = 0;
    uint8_t  _probeFails       = 0;
    uint8_t  _recoveryRung     = 0;
    uint32_t _lastRecoveryMs   = 0;
    // Has a gateway probe EVER been answered on this network? Until it has,
    // probe failures prove nothing - many routers drop ICMP by policy, and on
    // one of those a probe-driven verdict would convict every healthy board on
    // the fleet. Latched for the boot; see the long note at the failure site.
    bool     _probeEverWorked  = false;
    // The host the last probe was aimed at, so the "armed" line can name which
    // rung of the ladder actually answered rather than assuming the gateway.
    IPAddress _probeLast;

    // Rung 3 of the probe ladder - the only target that leaves the LAN, and
    // only reached after the gateway and the DNS server have both failed.
    // Split into octets rather than written as a literal so it is greppable
    // and so changing it needs no include here. 8.8.8.8.
    static constexpr uint8_t PROBE_FALLBACK_A = 8, PROBE_FALLBACK_B = 8,
                             PROBE_FALLBACK_C = 8, PROBE_FALLBACK_D = 8;
    // Completed recovery ladders since the last confirmed-good link. Caps the
    // thrash when recovery cannot fix the problem, which is the usual case
    // when the fault is upstream of this device entirely.
    uint8_t  _laddersRun       = 0;
    esp_ping_handle_t _ping    = nullptr;   // non-null only while one is in flight

    // Guarded by _mutex.
    SemaphoreHandle_t _mutex = nullptr;
    char          _ssid[33]     = {0};
    char          _password[65] = {0};
    char          _hostname[64] = {0};
    char          _joinSsid[33] = {0};
    char          _joinPass[65] = {0};
    char          _prevSsid[33] = {0};   // rollback target if a user join fails
    char          _prevPass[65] = {0};
    WiFiScanEntry _scan[SCAN_MAX_RESULTS];
    int           _scanCount = 0;

    uint32_t _attemptStartedMs = 0;
    uint32_t _joinStartedMs    = 0;
    uint32_t _apLastClientMs   = 0;
    // Grows each time a full attempt cycle fails while parked in AP, so a
    // device with genuinely wrong credentials stops thrashing the radio (and
    // the power rail) every minute forever. Reset on success.
    uint32_t _apRetryDelayMs   = 0;
    // Accumulated deferral while a client is attached to our AP. Someone is
    // probably configuring us and yanking the radio mid-form is hostile - but
    // this is capped, because a phone that auto-rejoins a saved network would
    // otherwise pin the device in AP mode forever.
    uint32_t _clientDeferMs    = 0;
    bool     _proven           = false;   // NVS-backed; see areCredentialsProven()
    uint8_t  _attempt          = 0;
    bool     _configured       = false;
    bool     _apFallbackForced = false;   // BOOT-button recovery override
};

#endif // CONNECTIVITY_MANAGER_H
