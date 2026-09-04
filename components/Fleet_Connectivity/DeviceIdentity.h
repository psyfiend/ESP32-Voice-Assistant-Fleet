#pragma once
#ifndef DEVICE_IDENTITY_H
#define DEVICE_IDENTITY_H

#include <Arduino.h>

// ---------------------------------------------------------------------------
// One device identity, rendered two ways.
//
// A hostname and an MQTT/HA object id have incompatible character rules:
// hostnames are DNS labels (lowercase, alphanumerics and hyphens, no
// underscores, <=63 chars), while HA conventions use underscores. Computing
// them independently in two places is how they end up disagreeing after
// someone renames a board - so both derive from ONE identity string here.
//
//   identity  = <board macro, lowercased> + "-" + <last 6 hex of MAC>
//   hostname  = fleet-ws-p4-7b-98d510      (hyphens - DNS safe)
//   deviceId  = fleet_ws_p4_7b_98d510      (underscores - MQTT / HA)
//
// See GitHub issue #38 and docs/ROADMAP.md Q5.
// ---------------------------------------------------------------------------

namespace DeviceIdentity {

// Append the MAC suffix to the USER-FACING names (hostname, AP SSID)?
// Default true. Call before first use - ConnectivityManager applies the stored
// setting during begin().
//
// IMPORTANT ASYMMETRY: this deliberately does NOT affect deviceId(). Hostname
// and AP SSID are cosmetic - a collision there is a nuisance you notice
// immediately in the DHCP table. deviceId() is the MQTT/HA unique_id root, and
// two boards of the same model sharing one would silently merge their entities
// in Home Assistant, which is both much worse and much harder to spot. So the
// toggle buys you a tidy "fleet-ws-p4-7b" hostname without ever risking that.
void configure(bool appendMacSuffix);
bool appendMacSuffix();

// Board slug from the BSP's device-identity macro, e.g. "ws-p4-7b".
// Resolved at compile time from whichever BSP_<NAME>.h is active.
const char *boardSlug();

// Last 6 hex digits of the base MAC, lowercase, e.g. "98d510".
// Stable across reboots and unique per physical unit, which is what makes two
// boards with identical hardware distinguishable in HA.
const char *macSuffix();

// "fleet-ws-p4-7b-98d510" - DNS-safe. Must be applied BEFORE WiFi.begin();
// setting it afterwards silently has no effect until the next reconnect.
const char *hostname();

// "fleet_ws_p4_7b_98d510" - MQTT topic and HA unique_id component.
const char *deviceId();

// Default AP SSID for provisioning, e.g. "FleetSetup-98D510". Uppercase suffix
// so it is readable on a phone's network list.
const char *apSsid();

// Per-device AP password derived from the MAC. Better than a shared secret
// baked into the firmware, and shown on-screen in Settings so it is
// discoverable without being published. Always >= 8 chars (WPA2 minimum).
const char *apPassword();

// Validate a user-supplied hostname against DNS label rules. Returns true and
// writes a sanitized copy, or false when nothing usable remains. Rejecting
// early beats letting a hostname with a space fail in confusing,
// DHCP-server-specific ways later.
bool sanitizeHostname(const char *in, char *out, size_t outLen);

} // namespace DeviceIdentity

#endif // DEVICE_IDENTITY_H
