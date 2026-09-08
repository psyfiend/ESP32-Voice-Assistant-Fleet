#pragma once
#ifndef SYSTEM_ENTITIES_H
#define SYSTEM_ENTITIES_H

#include "Entity.h"

// ---------------------------------------------------------------------------
// What the System provider offers, declared as data.
//
// Deliberately shaped like a BSP header: a table of `const` struct instances
// you can read top to bottom and diff against another board. Same rule applies
// as in Fleet_BSP.h - C++ designated initialisers must be listed in DECLARATION
// order (see EntityDescriptor in Entity.h). Skipping fields is fine; reordering
// the ones you do set is not, and fails to compile with
// "designator order for field 'X' does not match declaration order".
//
// The split this file exists to make obvious:
//
//   THIS FILE      - what the entities ARE. Static, declarative, no logic.
//   SystemProvider - where the values COME FROM. Polls the hardware.
//
// A future TemperatureProvider would have its own equivalent of this table
// beside it. Nothing central needs editing to add one.
// ---------------------------------------------------------------------------

// Ids. Referenced by cards and build sheets, so they are frozen the moment a
// dashboard uses one - renaming an id silently re-points a saved dashboard.
#define SYS_ENT_RSSI   "sys_rssi"
#define SYS_ENT_IP     "sys_ip"
#define SYS_ENT_UPTIME "sys_uptime"
#define SYS_ENT_HEAP   "sys_heap"

// Stale window. These are polled locally every 5 s; 15 s means one missed poll
// is not an alarm, but a wedged provider greys the cards out rather than
// leaving a confident number on screen forever.
#define SYS_ENT_STALE_MS 15000

// All four are:
//   source     = SYSTEM     - this board's own telemetry
//   writable   = false      - telemetry; nothing here is commandable
//   advertise  = true       - WE own these, so we publish them to HA
//   diagnostic = true       - panel health, not room state, so HA files them
//                             under diagnostics instead of the main controls
inline const EntityDescriptor SYSTEM_ENTITIES[] = {
    {
        .id          = SYS_ENT_RSSI,
        .name        = "WiFi Signal",
        .kind        = EntityKind::SENSOR,
        .source      = EntitySource::SYSTEM,
        .valueType   = ValueType::INT,
        .unit        = "dBm",
        .deviceClass = "signal_strength",
        // state_class measurement is what makes HA graph this and keep
        // long-term statistics. Without it, it is a number HA forgets.
        .stateClass  = "measurement",
        .icon        = "mdi:wifi",
        .writable    = false,
        .diagnostic  = true,
        .advertise   = true,
        .staleAfterMs = SYS_ENT_STALE_MS,
    },
    {
        .id          = SYS_ENT_IP,
        .name        = "IP Address",
        .kind        = EntityKind::TEXT,
        .source      = EntitySource::SYSTEM,
        .valueType   = ValueType::TEXT_VAL,
        // No device_class deliberately: HA has none that fits an IP address,
        // and inventing one makes the entity behave oddly in the UI.
        .icon        = "mdi:ip-network",
        .writable    = false,
        .diagnostic  = true,
        .advertise   = true,
        .staleAfterMs = SYS_ENT_STALE_MS,
    },
    {
        .id          = SYS_ENT_UPTIME,
        .name        = "Uptime",
        .kind        = EntityKind::SENSOR,
        .source      = EntitySource::SYSTEM,
        .valueType   = ValueType::INT,
        .unit        = "s",
        .deviceClass = "duration",
        // NO state_class, deliberately. "total_increasing" sounds right and is
        // wrong: a reboot resets uptime to zero, which HA reads as a counter
        // rollover and logs as a spurious jump in its statistics.
        .icon        = "mdi:timer-outline",
        .writable    = false,
        .diagnostic  = true,
        .advertise   = true,
        .staleAfterMs = SYS_ENT_STALE_MS,
    },
    {
        .id          = SYS_ENT_HEAP,
        .name        = "Free Heap",
        .kind        = EntityKind::SENSOR,
        .source      = EntitySource::SYSTEM,
        .valueType   = ValueType::INT,
        // Raw bytes: HA's data_size class formats into kB/MB itself, so the
        // byte count is the honest thing to publish.
        .unit        = "B",
        .deviceClass = "data_size",
        .stateClass  = "measurement",
        .icon        = "mdi:memory",
        .writable    = false,
        .diagnostic  = true,
        .advertise   = true,
        .staleAfterMs = SYS_ENT_STALE_MS,
    },
};

inline constexpr uint8_t SYSTEM_ENTITY_COUNT =
    sizeof(SYSTEM_ENTITIES) / sizeof(SYSTEM_ENTITIES[0]);

#endif // SYSTEM_ENTITIES_H
