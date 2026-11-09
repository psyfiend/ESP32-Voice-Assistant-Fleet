#pragma once
#ifndef EXTERNAL_ENTITIES_HA_H
#define EXTERNAL_ENTITIES_HA_H

#include "Entity.h"

// ---------------------------------------------------------------------------
// The owner's compiled Home Assistant entities, read over the websocket (#43).
//
// A compiled stand-in for per-device entity selection (#20), like
// ExternalEntities.h beside it. The dashboard's cards come from the settings
// file and from HA (DECISIONS K171); this table declares the entities they
// bind to.
//
// The list itself is docs/design/dashboard-target-7b.md. THAT FILE IS THE
// SPEC AND THIS ONE IS THE TRANSCRIPTION - if they disagree, the doc wins and
// this file is wrong.
//
// Every entry is source = HA, advertise = false. Someone else owns these;
// announcing them to Home Assistant would duplicate entities HA already has.
//
// NO `icon` FIELDS, deliberately: a descriptor icon OUTRANKS the live one, so
// it would freeze whatever HA said on the day it was copied. Empty means
// "follow HA"; set one only to override. cards.md section 13.
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
//    Availability comes from the source saying so (#56), not from value age.
//    #57 is the same problem from the other side: the card wants "when did
//    this last CHANGE", which is a different question from "when did we last
//    hear anything".
//
// 2. kind IS WHAT THE THING IS TO THE USER; THE SERVICE DOMAIN COMES FROM THE
//    ENTITY ID.
//
//    A relay such as `switch.tv_room_switch_1` can be a light in the room. It
//    is declared LIGHT because that is what decides the card, but commands
//    must call `switch.turn_on`, not `light.turn_on`, and the only authority
//    on that is the domain prefix of the entity id (CommandRouter.h).
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
//    Left in deliberately as a side-by-side comparison of the two transports -
//    same sensor, same instant, two paths. Drop one when the websocket is
//    trusted; do NOT leave both on the finished dashboard.
// ---------------------------------------------------------------------------

// UTF-8 degree sign written as explicit bytes so the file's own encoding can
// never silently mangle it - same reason as ExternalEntities.h.
//
// FAHRENHEIT: all seven temperature entities report unit_of_measurement "F"
// on the owner's instance. HA serves values already converted to the user's
// configured display unit.
//
// THE SAME SENSOR THEREFORE ARRIVES IN TWO DIFFERENT UNITS DEPENDING ON THE
// TRANSPORT. The deck probe reads 53.276 F here and ~11.8 C through
// Zigbee2MQTT's raw topic in ExternalEntities.h, because the broker carries
// what the device emitted and HA carries what the user asked to see. Both are
// correct and they are not comparable without converting. The fleet cannot
// assume a unit from a device_class - the unit has to come from the source,
// per entity.
#define HA_DEG_F "\xC2\xB0" "F"

inline const EntityDescriptor HA_ENTITIES[] = {
    // --- Living -----------------------------------------------------------
    {
        .id          = "living_overhead",
        .name        = "Living Overhead",
        .kind        = EntityKind::LIGHT,
        .source      = EntitySource::HA,
        .valueType   = ValueType::BOOL,
        .writable    = true,
        .advertise   = false,
        .staleAfterMs = 0,
        // A relay (switch.tv_room_switch_1, note 2) that HA wraps as a light
        // (Switch-as-X), the switch itself hidden.
        .externalRef = "light.living_room_light_laundry",
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
        .externalRef = "light.kitchen_light_sink",
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
        // Dimmable - supported_color_modes is ['brightness']. What a light can
        // do is read from HA with every report, never declared here.
        .externalRef = "light.kitchen_light_table",
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
        .externalRef = "sensor.kitchen_temperature",
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
        // The thermostat's room is the Hallway (HA's logical sensor).
        .externalRef = "sensor.hallway_temperature",
    },

    // --- Office -----------------------------------------------------------
    {
        .id          = "office_desk",
        .name        = "Office Desk",
        .kind        = EntityKind::LIGHT,
        .source      = EntitySource::HA,
        .valueType   = ValueType::BOOL,
        // A group acts like any other light card, and what makes it different
        // "should be prominent, the first and most easy thing to see": the GROUP
        // bulb is the hero, as a custom on/off icon pair.
        .iconOn      = "mdi:lightbulb-group",
        .iconOff     = "mdi:lightbulb-group-outline",
        .writable    = true,
        .advertise   = false,
        .staleAfterMs = 0,
        // A GROUP entity (a Hue room), ['color_temp','xy']. HA and the Hue
        // bridge send each command on to its three bulbs. cards.md section 4,
        // ha-websocket.md section 9.
        .externalRef = "light.office_lights_hue",
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
        .externalRef = "light.office_light_overhead",
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
        .externalRef = "sensor.office_temperature",
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
        .externalRef = "sensor.eric_bedroom_temperature",
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
        // The porch's own sensor (physical): this card is that one reading;
        // the house-wide outdoor reading is outdoor_temp below.
        .externalRef = "sensor.porch_climate_temperature",
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
        .externalRef = "sensor.deck_motion_temperature",
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
        .externalRef = "light.porch_light",
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
        .externalRef = "sensor.deck_motion_illuminance",
    },
    {
        // THE HOUSE-WIDE OUTDOOR READING (HA's logical sensor: the porch, else
        // the deck, else the weather service). On no card: the page header's
        // sensor slot reads it (HeaderSlots.cpp).
        .id          = "outdoor_temp",
        .name        = "Outdoor Temperature",
        .kind        = EntityKind::SENSOR,
        .source      = EntitySource::HA,
        .valueType   = ValueType::FLOAT,
        .unit        = HA_DEG_F,
        .deviceClass = "temperature",
        .stateClass  = "measurement",
        .writable    = false,
        .advertise   = false,
        .staleAfterMs = 0,
        .externalRef = "sensor.outdoor_temperature",
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
        .externalRef = "binary_sensor.garage_north_door",
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
        .externalRef = "binary_sensor.garage_south_door",
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
        .externalRef = "sensor.garage_temperature",
    },

    // --- Test fixtures --------------------------------------------------------
    //
    // Three throwaway HA entities (helpers in HA) so two card states can be
    // tested on demand rather than waited for. Their cards are on the FLEET
    // page, not on House.
    //
    //   fleet_test_available  input_boolean. Tapping its card turns the sensor
    //                         below available / unavailable (ST_UNAVAILABLE)
    //   fleet_test_reading    a template sensor whose availability follows the
    //                         toggle above
    //   fleet_test_refuse     a template switch that accepts taps and never
    //                         changes state - the command-refused path
    {
        .id          = "test_available",
        .name        = "Test Available",
        .kind        = EntityKind::SWITCH,
        .source      = EntitySource::HA,
        .valueType   = ValueType::BOOL,
        .writable    = true,
        .advertise   = false,
        .staleAfterMs = 0,
        // input_boolean.* - the domain comes from the prefix, so the command
        // router calls input_boolean.turn_on/off. See note 2 above.
        .externalRef = "input_boolean.fleet_test_available",
    },
    {
        .id          = "test_reading",
        .name        = "Test Reading",
        .kind        = EntityKind::SENSOR,
        .source      = EntitySource::HA,
        .valueType   = ValueType::FLOAT,
        .writable    = false,
        .advertise   = false,
        .staleAfterMs = 0,
        .externalRef = "sensor.fleet_test_reading",
    },
    {
        .id          = "test_refuse",
        .name        = "Test Refuse",
        .kind        = EntityKind::SWITCH,
        .source      = EntitySource::HA,
        .valueType   = ValueType::BOOL,
        .writable    = true,
        .advertise   = false,
        .staleAfterMs = 0,
        .externalRef = "switch.fleet_test_refuse",
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
// ONE TRAP. The obvious-looking slug is the wrong room:
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
