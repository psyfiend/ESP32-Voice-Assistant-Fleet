#pragma once
#ifndef CONNECTIVITY_DEFAULTS_H
#define CONNECTIVITY_DEFAULTS_H

#include <Arduino.h>
#include "ConnectivityTypes.h"
#include "bsp_loader.h" // pulls in the active board's identity macro (WS_P4_7B, etc.)

// Optional local-only override for STA credentials during dev/testing, so a
// real home AP password never needs to be committed. Not required - if
// absent, STA_SSID/STA_PASSWORD below stay blank (NVS-stored credentials,
// once the GUI can set them, take priority over this either way). To use:
// create components/Fleet_Connectivity/ConnectivityLocalSecrets.h
// (gitignored) with:
//     #define LOCAL_STA_SSID     "your-ap-name"
//     #define LOCAL_STA_PASSWORD "your-ap-password"
#if __has_include("ConnectivityLocalSecrets.h")
    #include "ConnectivityLocalSecrets.h"
#endif
#ifndef LOCAL_STA_SSID
    #define LOCAL_STA_SSID ""
#endif
#ifndef LOCAL_STA_PASSWORD
    #define LOCAL_STA_PASSWORD ""
#endif

// -----------------------------------------------------------------------
// Fleet Connectivity - compile-time fallback values only, not the runtime
// source of truth. ConnectivityManager reads NVS first and falls back to
// CONNECTIVITY_DEFAULT_WIFI below only when NVS has never been configured
// (fresh flash, or after a factory reset). Deliberately NOT part of
// Fleet_BSP.h - these are deployment settings, not hardware-wiring facts.
// See docs/ROADMAP.md's Connectivity sections for the full design.
// -----------------------------------------------------------------------

struct WiFiDefaults {
    // --- Behaviour ---
    ConnMode    MODE;                   // see ConnectivityTypes.h (ROADMAP Q1)

    // --- Station mode ---
    const char *STA_SSID;
    const char *STA_PASSWORD;
    uint32_t    STA_CONNECT_TIMEOUT_MS; // per-attempt deadline
    uint8_t     STA_RETRY_COUNT;

    // --- Identity ---
    // Empty string means "derive from DeviceIdentity::hostname()", which is
    // what every board should normally do. Only set this to pin one board to a
    // fixed name. Applied BEFORE WiFi.begin() - see ConnectivityManager.
    const char *HOSTNAME;

    // Append the last 6 MAC hex digits to the hostname and AP SSID?
    //   true  -> "fleet-ws-p4-7b-98d510" / "Fleet-ws-p4-7b-98D510"
    //   false -> "fleet-ws-p4-7b"        / "Fleet-ws-p4-7b"
    // Also settable at runtime from the UI. Turning it OFF makes names
    // collide between two boards of the same model - fine when you own one of
    // each, a real nuisance with a pair. It never affects the MQTT/HA device
    // id, which always keeps the MAC (see DeviceIdentity.h for why).
    bool        APPEND_MAC_SUFFIX;

    // --- AP / captive portal fallback ---
    const char *AP_SSID;                // empty = DeviceIdentity::apSsid()
    const char *AP_PASSWORD;            // empty = DeviceIdentity::apPassword()
    const char *AP_IP;                  // e.g. "192.168.4.1"
    const char *AP_SUBNET;              // e.g. "255.255.255.0"
    uint16_t    AP_IDLE_TIMEOUT_MIN;    // ConnMode::STA_PLUS_AP only; 0 = never idle down

    // --- Radio ---
    // --- Link liveness (issue #49) -------------------------------------
    //
    // The fault these exist for: a board sits in STA_CONNECTED holding an IP,
    // WiFi.status() keeps saying WL_CONNECTED, WiFi.RSSI() keeps handing back
    // the value it read at association, and nothing on the LAN can reach the
    // board. Observed on WS_P4_5 (~3-4 h), WS_P4_4B (~5-6 h) and WS_P4_7B.
    // Every one of those boards ran its UI perfectly throughout, so nothing
    // the device asked ITSELF could have detected it.

    // How often to re-read RSSI while associated. Before this existed, RSSI
    // was captured once in the GOT_IP handler and never again - the header
    // glyph's "full bars for six hours" was not a stale cache, it was a value
    // nobody ever asked for a second time. Cheap: one call, no allocation.
    uint32_t    RSSI_POLL_MS;
    // Past this age a reading is not reported as signal strength at all. It
    // only goes stale if the poll itself stops succeeding, which is a signal
    // in its own right.
    uint32_t    RSSI_STALE_MS;

    // Gateway ICMP probe. Runs only while associated and only when nothing
    // else has produced evidence recently, so a healthy board with a working
    // broker almost never sends one. esp_ping is already in liblwip.a and on
    // the include path for every environment - verified, no new dependency.
    bool        PROBE_ENABLED;
    uint32_t    PROBE_INTERVAL_MS;      // gap between probes while idle
    uint32_t    PROBE_TIMEOUT_MS;       // per-probe deadline
    uint8_t     PROBE_FAILS_SUSPECT;    // consecutive failures -> LINK_SUSPECT
    uint8_t     PROBE_FAILS_DEAD;       // consecutive failures -> LINK_DEAD

    // How many consecutive "cannot reach the broker" reports from above count
    // as evidence about the LINK rather than about the broker. A broker that
    // is genuinely down must not trigger a re-association, so this is
    // deliberately higher than the probe's threshold and is only ever
    // corroborating evidence - the probe is what actually convicts.
    uint8_t     REMOTE_FAILS_SUSPECT;

    // Recovery ladder, once LINK_DEAD is reached. Each rung is tried once,
    // then the next. Gap between rungs so a router that is simply rebooting
    // gets a chance to come back on its own before we cycle the radio.
    uint32_t    RECOVERY_STEP_MS;

    // TX power cap in dBm; 0 leaves the chip default (maximum) in place.
    // Capping keeps the radio's current bursts from sagging the board rail,
    // which on these panels shows up as display glitches or a brownout reset
    // when the PA first keys up. CYD_S3_3248W535 has a documented history of
    // exactly that symptom - see docs/HARDWARE_STATUS.md. Technique borrowed
    // from the ESP32-P4-NINA-Display project's wifi_apply_tx_power().
    uint8_t     TX_POWER_DBM;
};

// Time is deployment config on the same NVS-over-default layering as WiFi, so
// it lives here rather than in its own component.
struct TimeDefaults {
    // POSIX TZ string - carries the DST rules, so switchovers are handled by
    // the C library rather than by us. US Pacific: PST is UTC-8, PDT starts on
    // the 2nd Sunday in March and ends on the 1st Sunday in November.
    const char *TZ;
    const char *NTP_PRIMARY;
    const char *NTP_SECONDARY;
    const char *NTP_TERTIARY;
    bool        CLOCK_24H;   // false = 12-hour with an AM/PM indicator
};

// One fleet-wide default - every device is expected to join the same home
// AP, so no per-device override is expected to be needed for most fields.
// Where one genuinely is, override that single field in-place with an
// #if/#else keyed on the board's own identity macro (already in scope via
// bsp_loader.h above), e.g.:
//
//     #if defined(WS_P4_7B)
//         .TX_POWER_DBM = 15,
//     #else
//         .TX_POWER_DBM = 0,
//     #endif
//
// NOTE: C++ designated initializers must follow declaration order (same rule
// as the BSP structs - see CLAUDE.md).
static const WiFiDefaults CONNECTIVITY_DEFAULT_WIFI = {
    // STA with AP fallback: join the network, and raise our own AP only when
    // that fails. This is the intended default and always was; it had been left
    // on STA_PLUS_AP after the APSTA feasibility spike (issue #5) and never set
    // back, which meant every board stood up an access point on every boot for
    // no reason.
    .MODE                   = ConnMode::STA_WITH_AP_FALLBACK,

    .STA_SSID               = LOCAL_STA_SSID,
    .STA_PASSWORD           = LOCAL_STA_PASSWORD,
    .STA_CONNECT_TIMEOUT_MS = 15000,
    .STA_RETRY_COUNT        = 3,

    .HOSTNAME               = "",   // derive per-device
    .APPEND_MAC_SUFFIX      = false, // unique names out of the box

    .AP_SSID                = "",   // derive per-device
    .AP_PASSWORD            = "",   // derive per-device
    .AP_IP                  = "192.168.4.1",
    .AP_SUBNET              = "255.255.255.0",
    .AP_IDLE_TIMEOUT_MIN    = 10,

    // --- Link liveness (issue #49) ---
    // Sized against the observed fault, not against a guess: the boards died
    // 3-6 hours in and stayed dead until rebooted, so detection measured in
    // minutes is ample and false positives are the thing worth avoiding.
    // While LINK_SUSPECT the probe interval is divided (see the manager), so
    // conviction takes ~90 s from the first missed probe rather than 4 min.
    .RSSI_POLL_MS           = 10000,
    .RSSI_STALE_MS          = 45000,

    .PROBE_ENABLED          = true,
    .PROBE_INTERVAL_MS      = 60000,
    .PROBE_TIMEOUT_MS       = 2000,
    .PROBE_FAILS_SUSPECT    = 2,
    .PROBE_FAILS_DEAD       = 4,

    // Higher than the probe's threshold on purpose. A broker that is genuinely
    // down would otherwise get the radio cycled underneath it, which fixes
    // nothing and drops a working link. This corroborates; the probe convicts.
    .REMOTE_FAILS_SUSPECT   = 3,

    .RECOVERY_STEP_MS       = 30000,

// CYD_S3_3248W535 has a documented history of resetting when the radio first
// transmits (see docs/HARDWARE_STATUS.md), and it reappeared during AP-fallback
// testing on 2026-09-04 - resets on the first STA attempt and when the AP comes
// up. Capping TX power reduces the current spike when the PA keys up, which is
// the mitigation the ESP32-P4-NINA-Display project applies for exactly this
// symptom ("keeps the radio's current bursts from sagging the board rail").
//
// 13 dBm is a starting point, not a measured optimum: still ample for a panel
// sitting in the same house as its AP, roughly a third of the current draw of
// full power. If resets persist, try 11; if they are gone and range is poor,
// try 15. Set back to 0 to confirm the cap is what is actually helping.
#if defined(CYD_S3_3248)
    .TX_POWER_DBM           = 13,
#else
    .TX_POWER_DBM           = 0,    // chip default until a board proves it needs a cap
#endif
};

static const TimeDefaults CONNECTIVITY_DEFAULT_TIME = {
    .TZ            = "PST8PDT,M3.2.0,M11.1.0",   // US Pacific, with DST rules
    .NTP_PRIMARY   = "pool.ntp.org",
    .NTP_SECONDARY = "time.cloudflare.com",
    .NTP_TERTIARY  = "time.google.com",
    .CLOCK_24H     = false,                       // 12-hour with AM/PM
};

#endif
