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

    // --- AP / captive portal fallback ---
    const char *AP_SSID;                // empty = DeviceIdentity::apSsid()
    const char *AP_PASSWORD;            // empty = DeviceIdentity::apPassword()
    const char *AP_IP;                  // e.g. "192.168.4.1"
    const char *AP_SUBNET;              // e.g. "255.255.255.0"
    uint16_t    AP_IDLE_TIMEOUT_MIN;    // ConnMode::STA_PLUS_AP only; 0 = never idle down

    // --- Radio ---
    // TX power cap in dBm; 0 leaves the chip default (maximum) in place.
    // Capping keeps the radio's current bursts from sagging the board rail,
    // which on these panels shows up as display glitches or a brownout reset
    // when the PA first keys up. CYD_S3_3248W535 has a documented history of
    // exactly that symptom - see docs/PROJECT_STATUS.md. Technique borrowed
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
    .MODE                   = ConnMode::STA_WITH_AP_FALLBACK,

    .STA_SSID               = LOCAL_STA_SSID,
    .STA_PASSWORD           = LOCAL_STA_PASSWORD,
    .STA_CONNECT_TIMEOUT_MS = 15000,
    .STA_RETRY_COUNT        = 3,

    .HOSTNAME               = "",   // derive per-device

    .AP_SSID                = "",   // derive per-device
    .AP_PASSWORD            = "",   // derive per-device
    .AP_IP                  = "192.168.4.1",
    .AP_SUBNET              = "255.255.255.0",
    .AP_IDLE_TIMEOUT_MIN    = 10,

    .TX_POWER_DBM           = 0,    // chip default until a board proves it needs a cap
};

static const TimeDefaults CONNECTIVITY_DEFAULT_TIME = {
    .TZ            = "PST8PDT,M3.2.0,M11.1.0",   // US Pacific, with DST rules
    .NTP_PRIMARY   = "pool.ntp.org",
    .NTP_SECONDARY = "time.cloudflare.com",
    .NTP_TERTIARY  = "time.google.com",
    .CLOCK_24H     = false,                       // 12-hour with AM/PM
};

#endif
