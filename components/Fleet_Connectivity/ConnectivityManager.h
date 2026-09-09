#pragma once
#ifndef CONNECTIVITY_MANAGER_H
#define CONNECTIVITY_MANAGER_H

#include <Arduino.h>
#include <IPAddress.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <atomic>
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
    SignalBand  getSignalBand() const { return signalBandFromRssi(getRssi()); }
    uint8_t     getLastDisconnectReason() const { return (uint8_t)_lastReason.load(); }

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
