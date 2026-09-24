#pragma once
#ifndef EXTERNAL_ENTITIES_HA_H
#define EXTERNAL_ENTITIES_HA_H

#include "Entity.h"

// ---------------------------------------------------------------------------
// The owner's 18 Home Assistant entities, read over the websocket (#43).
//
// TEMPORARY, exactly like ExternalEntities.h beside it: this is a stand-in for
// the build sheet (#20). It exists so #43 and 2.7 can be judged against the
// real dashboard rather than against virtual switches. When the build sheet
// lands, this file goes away.
//
// The list itself is docs/design/dashboard-target-7b.md. THAT FILE IS THE
// SPEC AND THIS ONE IS THE TRANSCRIPTION - if they disagree, the doc wins and
// this file is wrong.
//
// Every entry is source = HA, advertise = false. Someone else owns these;
// announcing them to Home Assistant would duplicate entities HA already has.
//
// NO `icon` FIELDS, deliberately, since 2.7. They used to transcribe HA's
// icons, and a descriptor icon OUTRANKS the live one - so they froze whatever
// HA said on the day they were copied. Empty means "follow HA"; set one only
// to override. cards.md section 13.
//
//
// THREE DECISIONS ARE ENCODED HERE THAT ARE NOT OBVIOUS FROM THE TABLE.
//
// 1. staleAfterMs IS ZERO ON EVERY ENTITY, AND THAT IS NOT LAZINESS.
//
//    subscribe_trigger fires on CHANGE. A kitchen that nobody walks through
//    and a thermostat holding 21.0 send nothing for hours, and both are
//    perfectly healthy. Under MQTT, age was weak evidence of death because a
//    Zigbee device republishes periodically; under a change-driven push feed
//    age is evidence of NOTHING. A stale window here would grey out every
//    quiet sensor in the house overnight and call it a fault.
//
//    Availability has to come from the source saying so - #56 - not from
//    value age. #57 is the same problem from the other side: the card wants
//    "when did this last CHANGE", which is a different question from "when did
//    we last hear anything". Until those land, zero is the honest value.
//
// 2. kind IS WHAT THE THING IS TO THE USER; THE SERVICE DOMAIN COMES FROM THE
//    ENTITY ID.
//
//    Row 1 is `switch.tv_room_switch_1` and the owner listed it as a light. It
//    is declared LIGHT here because that is what it is in the room and what
//    decides the card. But #44 must call `switch.turn_on`, not
//    `light.turn_on`, and the only authority on that is the domain prefix of
//    the entity id.
//
//    That prefix is safe to read. dashboard-target-7b.md warns against parsing
//    meaning out of an entity id, and it is right - but the warning is about
//    the part AFTER the dot, where `switch.office_plug_3d_printer` is actually
//    named "Living Room Plug". The part BEFORE the dot is structural: Home
//    Assistant guarantees it names the service domain. Reading that is not
//    inference, it is the documented format.
//
// 3. TWO OF THESE ARE THE SAME PHYSICAL SENSOR AS ExternalEntities.h.
//
//    `outdoor_deck_*` here and `deck_*` there are one Philips Hue outdoor
//    motion sensor, reached two ways: HA's websocket and Zigbee2MQTT's broker
//    topic. Registering both means the same reading appears twice under two
//    ids.
//
//    Left in deliberately, because it is the only side-by-side comparison of
//    the two transports we will ever get for free - same sensor, same instant,
//    two paths. If the numbers or the timing disagree, that is worth knowing
//    before the fleet leans on the websocket. Drop one once #43 is trusted;
//    do NOT quietly leave both on the finished dashboard.
// ---------------------------------------------------------------------------

// UTF-8 degree sign written as explicit bytes so the file's own encoding can
// never silently mangle it - same reason as ExternalEntities.h.
//
// FAHRENHEIT, AND THAT IS NOT A GUESS. Measured against the live instance on
// 2026-09-20: all seven temperature entities report unit_of_measurement "F".
// HA serves values already converted to the user's configured display unit.
//
// THE SAME SENSOR THEREFORE ARRIVES IN TWO DIFFERENT UNITS DEPENDING ON THE
// TRANSPORT. The deck probe reads 53.276 F here and ~11.8 C through
// Zigbee2MQTT's raw topic in ExternalEntities.h, because the broker carries
// what the device emitted and HA carries what the user asked to see. Both are
// correct and they are not comparable without converting.
//
// This is exactly the hazard note 3 predicted, arriving on the first look. It
// also means the fleet cannot assume a unit from a device_class - the unit has
// to come from the source, per entity.
#define HA_DEG_F "\xC2\xB0" "F"

inline const EntityDescriptor HA_ENTITIES[] = {
    // --- Living -----------------------------------------------------------
    {
        .id          = "living_overhead",
        .name        = "Living Overhead",
        .kind        = EntityKind::LIGHT,     // a switch.* entity; see note 2
        .source      = EntitySource::HA,
        .valueType   = ValueType::BOOL,
        .writable    = true,
        .advertise   = false,
        .staleAfterMs = 0,
        .externalRef = "switch.tv_room_switch_1",
    },

    // --- Kitchen ----------------------------------------------------------
    {
        .id          = "kitchen_sink",
        .name        = "Kitchen Sink",
        .kind        = EntityKind::LIGHT,
        .source      = EntitySource::HA,
        .valueType   = ValueType::BOOL,
        .writable    = true,
        .advertise   = false,
        .staleAfterMs = 0,
        .externalRef = "light.kitchen_switch_1",
    },
    {
        .id          = "kitchen_table",
        .name        = "Kitchen Table",
        .kind        = EntityKind::LIGHT,
        .source      = EntitySource::HA,
        .valueType   = ValueType::BOOL,
        .writable    = true,
        .advertise   = false,
        .staleAfterMs = 0,
        // Dimmable - supported_color_modes is ['brightness']. LightCard is
        // still identical to SwitchCard, so brightness is invisible for now.
        .externalRef = "light.dining_room_light",
    },
    {
        .id          = "kitchen_temp",
        .name        = "Kitchen Temperature",
        .kind        = EntityKind::SENSOR,
        .source      = EntitySource::HA,
        .valueType   = ValueType::FLOAT,
        .unit        = HA_DEG_F,
        .deviceClass = "temperature",
        .stateClass  = "measurement",
        .writable    = false,
        .advertise   = false,
        .staleAfterMs = 0,
        .externalRef = "sensor.kitchen_mmwave_temperature",
    },
    {
        .id          = "kitchen_occupancy",
        .name        = "Kitchen Occupancy",
        .kind        = EntityKind::BINARY_SENSOR,
        .source      = EntitySource::HA,
        .valueType   = ValueType::BOOL,
        .deviceClass = "occupancy",
        .writable    = false,
        .advertise   = false,
        .staleAfterMs = 0,
        .externalRef = "binary_sensor.kitchen_occupancy",
    },

    // --- Front ------------------------------------------------------------
    {
        .id          = "front_thermo",
        .name        = "Front Thermostat",
        .kind        = EntityKind::SENSOR,
        .source      = EntitySource::HA,
        .valueType   = ValueType::FLOAT,
        .unit        = HA_DEG_F,
        .deviceClass = "temperature",
        .stateClass  = "measurement",
        .writable    = false,
        .advertise   = false,
        .staleAfterMs = 0,
        .externalRef = "sensor.thermostat_temperature",
    },

    // --- Office -----------------------------------------------------------
    {
        .id          = "office_desk",
        .name        = "Office Desk",
        .kind        = EntityKind::LIGHT,
        .source      = EntitySource::HA,
        .valueType   = ValueType::BOOL,
        // The GROUP bulb is the HERO, not the corner - the owner's call on glass,
        // 2026-09-22: "it (mostly) acts and functions just like any other light
        // card", so the corner is the ordinary bulb every light wears, and what
        // makes it different "should be prominent, the first and most easy thing
        // to see". A custom on/off pair - the first real use of one.
        .iconOn      = "mdi:lightbulb-group",
        .iconOff     = "mdi:lightbulb-group-outline",
        .writable    = true,
        .advertise   = false,
        .staleAfterMs = 0,
        // A GROUP entity, ['color_temp','xy']. The most demanding card on the
        // dashboard and the one that will force LightCard to stop being a
        // SwitchCard. cards.md section 4.
        .externalRef = "light.office",
    },
    {
        .id          = "office_overhead",
        .name        = "Office Overhead",
        .kind        = EntityKind::LIGHT,
        .source      = EntitySource::HA,
        .valueType   = ValueType::BOOL,
        .writable    = true,
        .advertise   = false,
        .staleAfterMs = 0,
        .externalRef = "light.office_overhead",
    },
    {
        .id          = "office_occupancy",
        .name        = "Office Occupancy",
        .kind        = EntityKind::BINARY_SENSOR,
        .source      = EntitySource::HA,
        .valueType   = ValueType::BOOL,
        .deviceClass = "occupancy",
        .writable    = false,
        .advertise   = false,
        .staleAfterMs = 0,
        .externalRef = "binary_sensor.office_occupancy",
    },
    {
        .id          = "office_temp",
        .name        = "Office Temperature",
        .kind        = EntityKind::SENSOR,
        .source      = EntitySource::HA,
        .valueType   = ValueType::FLOAT,
        .unit        = HA_DEG_F,
        .deviceClass = "temperature",
        .stateClass  = "measurement",
        .writable    = false,
        .advertise   = false,
        .staleAfterMs = 0,
        .externalRef = "sensor.mmwave_temperature",
    },

    // --- Bedroom ----------------------------------------------------------
    {
        .id          = "bedroom_temp",
        .name        = "Eric Bedroom Temperature",
        .kind        = EntityKind::SENSOR,
        .source      = EntitySource::HA,
        .valueType   = ValueType::FLOAT,
        .unit        = HA_DEG_F,
        .deviceClass = "temperature",
        .stateClass  = "measurement",
        .writable    = false,
        .advertise   = false,
        .staleAfterMs = 0,
        .externalRef = "sensor.temp_3_eric_bedroom_temperature",
    },

    // --- Outside ----------------------------------------------------------
    {
        .id          = "outside_front_temp",
        .name        = "Front Door Temperature",
        .kind        = EntityKind::SENSOR,
        .source      = EntitySource::HA,
        .valueType   = ValueType::FLOAT,
        .unit        = HA_DEG_F,
        .deviceClass = "temperature",
        .stateClass  = "measurement",
        .writable    = false,
        .advertise   = false,
        .staleAfterMs = 0,
        .externalRef = "sensor.temp_2_front_door_temperature",
    },
    {
        .id          = "outside_deck_temp",
        .name        = "Deck Temperature (HA)",
        .kind        = EntityKind::SENSOR,
        .source      = EntitySource::HA,
        .valueType   = ValueType::FLOAT,
        .unit        = HA_DEG_F,
        .deviceClass = "temperature",
        .stateClass  = "measurement",
        .writable    = false,
        .advertise   = false,
        .staleAfterMs = 0,
        // Same physical sensor as deck_temp in ExternalEntities.h. See note 3.
        .externalRef = "sensor.outdoor_deck_motion_temperature",
    },
    {
        .id          = "outside_front_light",
        .name        = "Porch Light",
        .kind        = EntityKind::LIGHT,
        .source      = EntitySource::HA,
        .valueType   = ValueType::BOOL,
        .writable    = true,
        .advertise   = false,
        .staleAfterMs = 0,
        .externalRef = "light.porch_switch_1",
    },
    {
        .id          = "outside_deck_lux",
        .name        = "Deck Illuminance (HA)",
        .kind        = EntityKind::SENSOR,
        .source      = EntitySource::HA,
        .valueType   = ValueType::INT,
        .unit        = "lx",
        .deviceClass = "illuminance",
        .stateClass  = "measurement",
        .writable    = false,
        .advertise   = false,
        .staleAfterMs = 0,
        // Same physical sensor as deck_lux in ExternalEntities.h. See note 3.
        .externalRef = "sensor.outdoor_deck_motion_illuminance",
    },

    // --- Garage -----------------------------------------------------------
    {
        .id          = "garage_door_north",
        .name        = "Garage North",
        .kind        = EntityKind::BINARY_SENSOR,
        .source      = EntitySource::HA,
        .valueType   = ValueType::BOOL,
        // device_class garage_door picks the row in CardIcons.cpp's binary
        // sensor table: garage corner, garage / garage-open hero, "Open" /
        // "Closed" words. In HA this is `opening` shown as garage door.
        .deviceClass = "garage_door",
        .writable    = false,
        .advertise   = false,
        .staleAfterMs = 0,
        .externalRef = "binary_sensor.door_sensor_2_garage_north",
    },
    {
        .id          = "garage_door_south",
        .name        = "Garage South",
        .kind        = EntityKind::BINARY_SENSOR,
        .source      = EntitySource::HA,
        .valueType   = ValueType::BOOL,
        .deviceClass = "garage_door",
        .writable    = false,
        .advertise   = false,
        .staleAfterMs = 0,
        .externalRef = "binary_sensor.door_sensor_3_south_garage_opening_2",
    },
    {
        .id          = "garage_temp",
        .name        = "Garage Temperature",
        .kind        = EntityKind::SENSOR,
        .source      = EntitySource::HA,
        .valueType   = ValueType::FLOAT,
        .unit        = HA_DEG_F,
        .deviceClass = "temperature",
        .stateClass  = "measurement",
        .writable    = false,
        .advertise   = false,
        .staleAfterMs = 0,
        .externalRef = "sensor.temp_1_garage_temperature",
    },
};

inline constexpr uint8_t HA_ENTITY_COUNT =
    sizeof(HA_ENTITIES) / sizeof(HA_ENTITIES[0]);

// ---------------------------------------------------------------------------
// Area display names.
//
// HA's area_id is the STABLE KEY and HA's area name is not what the owner
// wants on screen - he shortened them deliberately ("Living Room" -> "Living")
// because a card header is narrow. So: group on the id, render from this
// table, and fall back to HA's own name when an id is not listed.
//
// ONE TRAP, MEASURED 2026-09-20. The obvious-looking slug is the wrong room:
//
//     bedroom    -> "Eric Bedroom"
//     bedroom_2  -> "Bedroom"
//
// Anything that guesses an area_id from a room name gets this backwards.
// ---------------------------------------------------------------------------
struct HaAreaName {
    const char *areaId;
    const char *display;
};

inline const HaAreaName HA_AREA_NAMES[] = {
    { "living_room", "Living"  },
    { "kitchen",     "Kitchen" },
    { "front_room",  "Front"   },
    { "office",      "Office"  },
    { "bedroom",     "Bedroom" },   // HA calls this one "Eric Bedroom"
    { "outside",     "Outside" },
    { "garage",      "Garage"  },
};

inline constexpr uint8_t HA_AREA_NAME_COUNT =
    sizeof(HA_AREA_NAMES) / sizeof(HA_AREA_NAMES[0]);

#endif // EXTERNAL_ENTITIES_HA_H
