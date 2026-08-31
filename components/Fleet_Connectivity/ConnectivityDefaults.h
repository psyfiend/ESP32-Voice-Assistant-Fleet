#pragma once
#ifndef CONNECTIVITY_DEFAULTS_H
#define CONNECTIVITY_DEFAULTS_H

#include <Arduino.h>
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
// See docs/FUTURE_IMPROVEMENTS.md's Connectivity section for the full
// design (per-device override mechanism, NVS layering, MQTT plans).
// -----------------------------------------------------------------------

struct WiFiDefaults {
    // --- Station mode ---
    const char *STA_SSID;
    const char *STA_PASSWORD;
    uint32_t    STA_CONNECT_TIMEOUT_MS; // how long to wait before falling back to the portal
    uint8_t     STA_RETRY_COUNT;

    // --- AP / captive portal fallback ---
    bool        PORTAL_ENABLED;  // if false, a failed STA connect does NOT fall back to a portal
    const char *PORTAL_SSID;
    const char *PORTAL_PASSWORD; // empty string = open AP
    const char *PORTAL_IP;       // e.g. "192.168.4.1"
    const char *PORTAL_SUBNET;   // e.g. "255.255.255.0"
};

// One fleet-wide default - every device is expected to join the same home
// AP, so no per-device override is expected to be needed for most fields.
// Where one genuinely is, override that single field in-place with an
// #if/#else keyed on the board's own identity macro (already in scope via
// bsp_loader.h above), e.g.:
//
//     #if defined(WS_P4_7B)
//         .PORTAL_SSID = "FleetSetup-7B",
//     #else
//         .PORTAL_SSID = "FleetSetup",
//     #endif
//
// No board needs an override yet, so none are applied below.
static const WiFiDefaults CONNECTIVITY_DEFAULT_WIFI = {
    .STA_SSID               = LOCAL_STA_SSID,
    .STA_PASSWORD           = LOCAL_STA_PASSWORD,
    .STA_CONNECT_TIMEOUT_MS = 15000,
    .STA_RETRY_COUNT        = 3,

    .PORTAL_ENABLED  = true,
    .PORTAL_SSID     = "FleetSetup",
    .PORTAL_PASSWORD = "",
    .PORTAL_IP       = "192.168.4.1",
    .PORTAL_SUBNET   = "255.255.255.0",
};

#endif
