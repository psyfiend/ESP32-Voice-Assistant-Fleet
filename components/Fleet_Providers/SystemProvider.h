#pragma once
#ifndef SYSTEM_PROVIDER_H
#define SYSTEM_PROVIDER_H

#include <Arduino.h>
#include "EntityRegistry.h"
#include "ConnectivityManager.h"

// ---------------------------------------------------------------------------
// SystemProvider - this board's own telemetry, as entities.
// ROADMAP milestone 1.7: "SystemProvider populates rssi/ip/uptime/heap".
//
// A provider is the ONLY kind of code that bridges a source to the registry
// (ROADMAP 4.1). The boundary it respects:
//
//   - It writes values INTO the registry. It never renders anything, never
//     touches LVGL, and never publishes to MQTT. Discovery and publishing are
//     a separate concern that walks the registry from outside.
//   - Fleet_Entities stays dependency-free; THIS is where Arduino and
//     ESP-specific calls are allowed to live. That is the whole reason
//     providers exist as a category rather than being folded into either side.
//
// All four entities are `advertise = true` (we own them) and `diagnostic =
// true` (they describe the panel's health, not the room), so in Home Assistant
// they land in the device's diagnostics section rather than cluttering its
// primary controls.
// ---------------------------------------------------------------------------

class SystemProvider {
public:
    // reg and link must outlive this object. Registers the entities; safe to
    // call once, at startup, before any other task is running.
    void begin(EntityRegistry *reg, ConnectivityManager *link);

    // Polls and writes changed values. Cheap: unchanged readings do not dirty
    // anything, so a stable RSSI produces no redraws at all.
    void loop(uint32_t nowMs);

    void setIntervalMs(uint32_t ms) { _intervalMs = ms; }

    // Entity ids live in SystemEntities.h as SYS_ENT_* - see that file for the
    // full declarative table. Not duplicated here: two lists of the same ids
    // is exactly how one gets renamed and the other does not.

private:
    EntityRegistry      *_reg  = nullptr;
    ConnectivityManager *_link = nullptr;

    uint32_t _intervalMs  = 5000;
    uint32_t _lastPollMs  = 0;
    bool     _begun       = false;
};

#endif // SYSTEM_PROVIDER_H
