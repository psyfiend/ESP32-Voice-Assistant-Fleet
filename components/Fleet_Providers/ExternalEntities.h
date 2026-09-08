#pragma once
#ifndef EXTERNAL_ENTITIES_H
#define EXTERNAL_ENTITIES_H

#include "Entity.h"

// ---------------------------------------------------------------------------
// Entities owned by OTHER devices, which this panel reads and displays.
//
// TEMPORARY. This table is a stand-in for the build sheet (issue #20), which
// is where per-device entity selection belongs. It exists now so the inbound
// half of the pipeline can be tested against a real sensor rather than a mock.
// When the build sheet lands, this file goes away.
//
// Every entry here is:
//   source    = MQTT   - it arrives over the broker
//   advertise = false  - SOMEONE ELSE OWNS IT. Announcing it to Home Assistant
//                        would create a duplicate of an entity HA already has.
//                        This is the single flag separating "ours" from
//                        "theirs"; there is no other code path.
//
// All four below read from ONE topic. A Zigbee2MQTT device publishes its whole
// state as a single JSON message, so each entity names the key it wants via
// valueKey. Zigbee2MQTT's own Home Assistant discovery config does exactly
// this - it sets state_topic to the parent and picks fields out with
// value_template "{{ value_json['temperature'] }}". Our valueKey is the same
// idea without needing a template engine on the device.
// ---------------------------------------------------------------------------

// Philips Hue outdoor motion sensor, via Zigbee2MQTT.
// Payload on this topic looks like:
//   {"battery":100,"illuminance":1,"linkquality":108,"occupancy":false,
//    "temperature":14.65,"last_seen":"..."}
#define Z2M_DECK_MOTION "zigbee2mqtt/outdoor/deck/motion"

// Zigbee sensors report on their own schedule and a motion sensor can be quiet
// for a long time, so the stale window is generous. Too short and a working
// sensor greys out overnight; too long and a dead one looks alive. 30 minutes
// is a compromise to revisit once we see how often this device actually talks.
#define Z2M_STALE_MS 1800000

inline const EntityDescriptor EXTERNAL_ENTITIES[] = {
    {
        .id          = "deck_temp",
        .name        = "Deck Temperature",
        .kind        = EntityKind::SENSOR,
        .source      = EntitySource::MQTT,
        .valueType   = ValueType::FLOAT,
        .unit        = "\xC2\xB0" "C",   // UTF-8 degree sign, written as bytes
                                         // so the file's own encoding cannot
                                         // silently mangle it
        .deviceClass = "temperature",
        .stateClass  = "measurement",
        .icon        = "mdi:thermometer",
        .writable    = false,
        .advertise   = false,
        .staleAfterMs = Z2M_STALE_MS,
        .externalRef = Z2M_DECK_MOTION,
        .valueKey    = "temperature",
    },
    {
        .id          = "deck_motion",
        .name        = "Deck Motion",
        .kind        = EntityKind::BINARY_SENSOR,
        .source      = EntitySource::MQTT,
        .valueType   = ValueType::BOOL,
        .deviceClass = "occupancy",
        .icon        = "mdi:motion-sensor",
        .writable    = false,
        .advertise   = false,
        .staleAfterMs = Z2M_STALE_MS,
        .externalRef = Z2M_DECK_MOTION,
        .valueKey    = "occupancy",
    },
    {
        .id          = "deck_lux",
        .name        = "Deck Illuminance",
        .kind        = EntityKind::SENSOR,
        .source      = EntitySource::MQTT,
        .valueType   = ValueType::INT,
        .unit        = "lx",
        .deviceClass = "illuminance",
        .stateClass  = "measurement",
        .icon        = "mdi:brightness-5",
        .writable    = false,
        .advertise   = false,
        .staleAfterMs = Z2M_STALE_MS,
        .externalRef = Z2M_DECK_MOTION,
        .valueKey    = "illuminance",
    },
    {
        .id          = "deck_battery",
        .name        = "Deck Sensor Battery",
        .kind        = EntityKind::SENSOR,
        .source      = EntitySource::MQTT,
        .valueType   = ValueType::INT,
        .unit        = "%",
        .deviceClass = "battery",
        .stateClass  = "measurement",
        .icon        = "mdi:battery",
        // NOTE the order: diagnostic is declared BEFORE advertise in
        // EntityDescriptor, and C++ designated initialisers must follow
        // declaration order. Writing advertise first fails to compile with
        // "designator order for field 'diagnostic' does not match declaration
        // order" - the same rule CLAUDE.md records for BSP headers.
        .writable    = false,
        .diagnostic  = true,
        .advertise   = false,
        .staleAfterMs = Z2M_STALE_MS,
        .externalRef = Z2M_DECK_MOTION,
        .valueKey    = "battery",
    },
};

inline constexpr uint8_t EXTERNAL_ENTITY_COUNT =
    sizeof(EXTERNAL_ENTITIES) / sizeof(EXTERNAL_ENTITIES[0]);

#endif // EXTERNAL_ENTITIES_H
