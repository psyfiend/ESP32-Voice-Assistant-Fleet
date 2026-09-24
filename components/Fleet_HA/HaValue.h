#pragma once
#ifndef HA_VALUE_H
#define HA_VALUE_H

#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include "Entity.h"
#include <ArduinoJson.h>

// ---------------------------------------------------------------------------
// Turn Home Assistant's string state into an EntityValue of the declared type.
//
// ONE COPY, DELIBERATELY. Both paths that receive a value need this: HaRest
// on the initial fetch and HaProvider on every live trigger event. The obvious
// thing is a private method on each, and this session has already been bitten
// TWICE by exactly that - a rule implemented in one code path and a second
// path that did the same job slightly differently (the swipe press origin, the
// drawer top offset). Both times the fix was to delete the second copy rather
// than to synchronise it.
//
// A divergence here would be worse than either of those, because it would be
// invisible: the boot value and the live value for the same entity would
// disagree about what "closed" or "12.5" means, and only after something
// changed.
// ---------------------------------------------------------------------------

// Returns false when `state` is not a value at all. The caller must NOT write
// in that case: "unavailable" is not a reading of zero, and writing one makes
// a dead thermostat confidently display 0 degrees.
inline bool haCoerceState(const EntityDescriptor &d, const char *state,
                          EntityValue &out) {
    if (!state || !state[0])                 return false;
    if (strcmp(state, "unavailable") == 0)   return false;
    if (strcmp(state, "unknown")     == 0)   return false;

    switch (d.valueType) {
        case ValueType::BOOL: {
            // HA reports on/off for switches, lights AND binary sensors. A
            // binary_sensor with device_class garage_door still says on/off -
            // the device class only changes how HA's own frontend words it,
            // which was confirmed against the live instance rather than
            // assumed. open/closed/true/false are accepted anyway so a future
            // entity that does report them is not silently dropped.
            const bool on  = (strcmp(state, "on")     == 0) ||
                             (strcmp(state, "open")   == 0) ||
                             (strcmp(state, "true")   == 0);
            const bool off = (strcmp(state, "off")    == 0) ||
                             (strcmp(state, "closed") == 0) ||
                             (strcmp(state, "false")  == 0);
            if (!on && !off) return false;
            out.type = ValueType::BOOL;
            out.b    = on;
            return true;
        }
        case ValueType::FLOAT: {
            char *end = nullptr;
            float f = strtof(state, &end);
            if (end == state) return false;
            out.type = ValueType::FLOAT;
            out.f    = f;
            return true;
        }
        case ValueType::INT: {
            char *end = nullptr;
            long l = strtol(state, &end, 10);
            if (end == state) return false;
            out.type = ValueType::INT;
            out.i    = (int32_t)l;
            return true;
        }
        case ValueType::TEXT_VAL: {
            out.type = ValueType::TEXT_VAL;
            snprintf(out.text, sizeof(out.text), "%s", state);
            return true;
        }
        default:
            return false;
    }
}

// ---------------------------------------------------------------------------
// The attributes a card draws, and ONE copy of how they are read. Milestone 2.7.
//
// Same reasoning as haCoerceState() above: HaRest reads them on the initial
// fetch and HaProvider on every live event, and two readers that disagree
// would show a light one colour at boot and another after its first change.
//
// haAttrFilter() marks the fields in an ArduinoJson filter; haReadAttrs()
// reads them back. Kept side by side so a field cannot be added to one and
// forgotten in the other - which is how the icon ended up filtered IN on the
// websocket for weeks and never read by anything.
// ---------------------------------------------------------------------------
inline void haAttrFilter(JsonObject attrs) {
    attrs["icon"]       = true;
    attrs["brightness"] = true;
    attrs["rgb_color"]  = true;
}

inline void haReadAttrs(JsonVariantConst attrs, EntityAttrs &out) {
    out = EntityAttrs{};
    if (attrs.isNull()) return;

    const char *icon = attrs["icon"] | "";
    snprintf(out.icon, sizeof(out.icon), "%s", icon);

    // A light that is OFF reports brightness as null, not 0. is<int>() is
    // false for null, which leaves -1: "not reported". HA's scale is 0-255.
    JsonVariantConst b = attrs["brightness"];
    if (b.is<int>()) {
        int v = b.as<int>();
        out.brightness = (int16_t)(v < 0 ? 0 : (v > 255 ? 255 : v));
    }

    // Present for every colour mode, colour temperature included - measured
    // on light.office, which reads [255,167,88] at 2710 K. Null when off.
    JsonArrayConst rgb = attrs["rgb_color"];
    if (!rgb.isNull() && rgb.size() == 3) {
        const uint32_t r = rgb[0] | 0, g = rgb[1] | 0, bl = rgb[2] | 0;
        out.rgb    = ((r & 0xFF) << 16) | ((g & 0xFF) << 8) | (bl & 0xFF);
        out.hasRgb = true;
    }
}

#endif // HA_VALUE_H
