#pragma once
#ifndef CONNECTIVITY_TYPES_H
#define CONNECTIVITY_TYPES_H

#include <Arduino.h>

// ---------------------------------------------------------------------------
// Fleet connectivity vocabulary.
//
// Split out from ConnectivityManager.h so the GUI, MQTT and (eventually) the
// Entity Registry can speak about link state without pulling in the manager's
// own dependencies. See docs/ROADMAP.md sections 4.x and Q1/Q9.
//
// NAMING HAZARD - read before adding an enumerator here.
// `enum class` scoping does NOT protect against the preprocessor. Arduino's
// headers define a lot of bare uppercase macros, and any collision rewrites the
// enumerator into a numeric constant, which breaks the *whole* enum - every
// member then reports as "not a member of X", pointing nowhere near the real
// cause. Two were hit while writing this file:
//     DISABLED  -> esp32-hal-gpio.h  (interrupt mode 0x00)   => now RADIO_OFF
//     WIFI_STA / WIFI_AP -> WiFiType.h (wifi_mode_t aliases) => now STA / AP
// Before adding a name, grep the framework for `#define <NAME>`.
// ---------------------------------------------------------------------------

// User-selectable connectivity behaviour (ROADMAP Q1, decided 2026-09-03).
// Stored in NVS; the compile-time default is only the fresh-flash fallback.
enum class ConnMode : uint8_t {
    OFF                  = 0, // Radio never started. A board with no network is
                              // a fully supported configuration, not a failure -
                              // local-only cards must still work.
    STA_WITH_AP_FALLBACK = 1, // DEFAULT. STA only; raise the AP after STA fails.
    STA_PLUS_AP          = 2, // APSTA from boot; AP idles down after
                              // AP_IDLE_TIMEOUT_MIN with no client (0 = never).
    STA_ONLY             = 3, // Never raise an AP. Needs the physical recovery
                              // escape hatch - see ConnectivityManager::begin().
};

// Which physical link currently carries traffic. Ethernet is declared now and
// implemented later (ROADMAP Q9): CYD_P4_1060 has a port today, and some
// WS_P4_4B variants ship with one. Callers ask getLinkType(), never "is wifi up".
enum class LinkType : uint8_t {
    NONE = 0,
    STA,
    AP,
    APSTA,
    ETHERNET,
};

// Connection state machine. Deliberately richer than a bool: the header status
// widget maps these 1:1 onto glyphs, and DEGRADED vs AP_ACTIVE vs DISABLED are
// three very different things a user needs to tell apart at a glance.
enum class ConnState : uint8_t {
    RADIO_OFF = 0,  // ConnMode::OFF - radio never started
    BOOT,           // begin() not yet called
    STA_CONNECTING, // join in flight (initial or retry)
    STA_CONNECTED,  // associated and holding an IP
    AP_ACTIVE,      // AP up, no STA link (fallback, or provisioning)
    APSTA,          // AP up AND STA connected
    DEGRADED,       // retries exhausted and no AP available (mode 3, or AP failed)
};

// Why an interactive join ended. Distinguishing these is the difference between
// a settings screen that tells the user what to fix and one that says "failed".
// Classified from the raw 802.11 reason code - see classifyDisconnect().
enum class JoinResult : uint8_t {
    NONE = 0,      // no interactive join has completed since the last ack
    SUCCESS,
    FAIL_AUTH,     // wrong password / handshake rejected
    FAIL_NO_AP,    // SSID not found on any channel
    FAIL_TIMEOUT,  // deadline elapsed with no terminal answer
    FAIL_CANCELLED,// user backed out
};

// Progress of the scan/join worker driven from the settings UI. Separate from
// ConnState because a device stays STA_CONNECTED while the user scans for a
// different network.
enum class JoinPhase : uint8_t {
    IDLE = 0,
    SCANNING,
    SCAN_DONE,
    SCAN_FAILED,
    CONNECTING,
    REJOINING,     // join failed; restoring the previous network
    FINISHED,      // terminal - read getJoinResult(), then ackJoinResult()
};

// One scan hit, already de-duplicated and sorted strongest-first.
struct WiFiScanEntry {
    char    ssid[33];
    int8_t  rssi;
    bool    secured;
};

// Signal buckets shared by the UI so firmware and glyphs never disagree about
// what "weak" means. Thresholds per docs/ROADMAP.md; recalibrate against real
// boards in their real mounting positions before treating them as settled.
enum class SignalBand : uint8_t { NONE = 0, WEAK, FAIR, GOOD, EXCELLENT };

inline SignalBand signalBandFromRssi(int8_t rssi) {
    if (rssi == 0)    return SignalBand::NONE;
    if (rssi >= -55)  return SignalBand::EXCELLENT;
    if (rssi >= -70)  return SignalBand::GOOD;
    if (rssi >= -82)  return SignalBand::FAIR;
    return SignalBand::WEAK;
}

// Human-readable names, for logs, the diagnostics dump and the settings UI.
const char *connModeName(ConnMode m);
const char *connStateName(ConnState s);
const char *linkTypeName(LinkType l);
const char *joinResultName(JoinResult r);

#endif // CONNECTIVITY_TYPES_H
