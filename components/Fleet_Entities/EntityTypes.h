#pragma once
#ifndef ENTITY_TYPES_H
#define ENTITY_TYPES_H

#include <stdint.h>
#include <string.h>

// ---------------------------------------------------------------------------
// Fleet Entities - the vocabulary. ROADMAP section 4.1.
//
// ZERO DEPENDENCIES, deliberately. No Arduino.h, no LVGL, no ArduinoJson, no
// PubSubClient. Per ROADMAP Q9 this library must compile and unit-test on a
// PC, which is also what keeps the Q8 ESP-IDF escape hatch real. If something
// here ever needs Arduino, it belongs in a provider instead.
//
// That constraint is why this file uses fixed char arrays rather than String,
// and why nothing here knows how to serialise itself to JSON. Discovery
// payloads are built by walking the registry from OUTSIDE it.
// ---------------------------------------------------------------------------

// Deliberately NOT bare single-word ALL-CAPS names where they could collide
// with Arduino's global macro namespace - see CLAUDE.md. Entities are scoped
// enums, but scoping does not protect against the preprocessor.

enum class EntityKind : uint8_t {
    SENSOR = 0,     // read-only numeric or text reading
    BINARY_SENSOR,  // read-only on/off (motion, door, occupancy)
    SWITCH,         // writable on/off
    LIGHT,          // writable on/off plus optional brightness / colour
    BUTTON,         // stateless, momentary; press is the whole payload
    NUMBER,         // writable numeric within a range
    // A WRITABLE free-text entity. HA's `text` platform requires a
    // command_topic and silently rejects a config without one - the entity
    // just never appears. A read-only string is a SENSOR whose valueType is
    // TEXT_VAL, not this. Same applies to SWITCH, NUMBER, LIGHT and BUTTON:
    // all are command platforms.
    TEXT,
    CLIMATE,        // setpoint + mode + current reading
    WEATHER,        // condition + forecast
};

// Where a value comes from. A card NEVER branches on this - ROADMAP 4.1's
// whole point is that the same card renders a local I2C sensor and a remote HA
// entity identically. It exists so providers know what they own, and so
// discovery knows what to advertise.
enum class EntitySource : uint8_t {
    LOCAL = 0,  // a physical peripheral on this board (I2C sensor, etc.)
    SYSTEM,     // this board's own telemetry: rssi, ip, uptime, heap
    MQTT,       // a plain MQTT topic we subscribe to
    HA,         // a Home Assistant entity, reached over MQTT
    VIRTUAL,    // computed or derived; no external source at all
};

// The value itself. A tagged union rather than a variant so this stays
// trivially copyable and header-only on a PC compiler as well as xtensa.
enum class ValueType : uint8_t {
    NONE = 0,
    BOOL,
    INT,
    FLOAT,
    TEXT_VAL,   // not "STRING": avoids any chance of a macro collision, and
                // reads unambiguously next to EntityKind::TEXT
};

// Longest value we will hold inline. Text entities on a panel are labels and
// short status strings, not documents; anything longer is a design smell.
static constexpr uint8_t ENTITY_TEXT_MAX = 48;

struct EntityValue {
    ValueType type = ValueType::NONE;
    union {
        bool    b;
        int32_t i;
        float   f;
    };
    char text[ENTITY_TEXT_MAX] = {0};

    EntityValue() : type(ValueType::NONE), i(0) {}

    static EntityValue makeBool(bool v)    { EntityValue e; e.type = ValueType::BOOL;  e.b = v; return e; }
    static EntityValue makeInt(int32_t v)  { EntityValue e; e.type = ValueType::INT;   e.i = v; return e; }
    static EntityValue makeFloat(float v)  { EntityValue e; e.type = ValueType::FLOAT; e.f = v; return e; }
    static EntityValue makeText(const char *v) {
        EntityValue e;
        e.type = ValueType::TEXT_VAL;
        if (v) {
            // Manual copy rather than strncpy: guarantees termination and
            // avoids the truncation warning strncpy produces at -Wall.
            uint8_t n = 0;
            while (v[n] && n < ENTITY_TEXT_MAX - 1) { e.text[n] = v[n]; n++; }
            e.text[n] = '\0';
        }
        return e;
    }

    // Value equality, used to suppress no-op updates. A provider republishing
    // an unchanged reading should not dirty the entity and cause a redraw -
    // that is most of the rate-limiting ROADMAP 4.2 wants, for free.
    bool equals(const EntityValue &o) const {
        if (type != o.type) return false;
        switch (type) {
            case ValueType::NONE:     return true;
            case ValueType::BOOL:     return b == o.b;
            case ValueType::INT:      return i == o.i;
            // Exact comparison is intended. Sensor readings arrive already
            // quantised by their source, so "did this change" is a question
            // about the reported value, not about float precision.
            case ValueType::FLOAT:    return f == o.f;
            case ValueType::TEXT_VAL: return strcmp(text, o.text) == 0;
        }
        return false;
    }
};

const char *entityKindName(EntityKind k);
const char *entitySourceName(EntitySource s);

// HA platform string for the discovery payload's "p" key. Kept here beside the
// enum so a new kind cannot be added without deciding how it is advertised.
const char *entityKindHaPlatform(EntityKind k);

#endif // ENTITY_TYPES_H
