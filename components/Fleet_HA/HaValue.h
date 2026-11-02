#pragma once
#ifndef HA_VALUE_H
#define HA_VALUE_H

#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include "Entity.h"
#include "EntityRegistry.h"
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
    // A light's, 2.10c (#65). ha-websocket.md section 9 has what the owner's
    // lights send; xy_color is left out - hs_color says the same in our terms.
    attrs["supported_color_modes"] = true;
    attrs["color_mode"]            = true;
    attrs["color_temp_kelvin"]     = true;
    attrs["min_color_temp_kelvin"] = true;
    attrs["max_color_temp_kelvin"] = true;
    attrs["hs_color"]              = true;
    // A thermostat's, 2.13 (K144).
    attrs["current_temperature"]   = true;
    attrs["temperature"]           = true;
    attrs["current_humidity"]      = true;
    attrs["hvac_action"]           = true;
    attrs["fan_mode"]              = true;
}

inline int haClamp(int v, int lo, int hi) { return v < lo ? lo : (v > hi ? hi : v); }
inline float haClampF(float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }

// A temperature attribute in tenths, or ATTR_NONE when absent or null.
inline int16_t haTenths(JsonVariantConst v) {
    if (!(v.is<float>() || v.is<int>())) return EntityAttrs::ATTR_NONE;
    const float f = v.as<float>() * 10.0f;
    return (int16_t)haClampF(f + (f < 0 ? -0.5f : 0.5f), -3000.0f, 3000.0f);   // rounded
}

// One of HA's colour modes, as what it lets a control do. onoff is the only
// mode that cannot be dimmed; hs, xy and the rgb family all mean "has a hue".
inline uint8_t haModeCaps(const char *m) {
    if (!m || !m[0])                   return 0;
    if (strcmp(m, "onoff") == 0)       return LIGHT_CAN_ONOFF;
    if (strcmp(m, "brightness") == 0 ||
        strcmp(m, "white") == 0)       return LIGHT_CAN_ONOFF | LIGHT_CAN_DIM;
    if (strcmp(m, "color_temp") == 0)  return LIGHT_CAN_ONOFF | LIGHT_CAN_DIM | LIGHT_CAN_TEMP;
    if (strcmp(m, "hs") == 0 || strcmp(m, "xy") == 0 || strncmp(m, "rgb", 3) == 0)
                                       return LIGHT_CAN_ONOFF | LIGHT_CAN_DIM | LIGHT_CAN_COLOUR;
    return 0;   // "unknown", or a mode HA adds later: offer nothing we cannot back
}

inline LightMode haLightMode(const char *m) {
    const uint8_t caps = haModeCaps(m);
    if (caps & LIGHT_CAN_COLOUR) return LightMode::LMODE_COLOUR;
    if (caps & LIGHT_CAN_TEMP)   return LightMode::LMODE_TEMP;
    if (caps & LIGHT_CAN_DIM)    return LightMode::LMODE_DIM;
    if (caps)                    return LightMode::LMODE_ONOFF;
    return LightMode::LMODE_UNKNOWN;
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

    // WHAT A LIGHT CAN DO, 2.10c: the union of its supported modes. Sent with
    // every state, off included, so it is never remembered between reports.
    // Absent (a switch, a sensor) leaves 0: "not reported", the big toggle.
    JsonArrayConst modes = attrs["supported_color_modes"];
    if (!modes.isNull()) {
        for (JsonVariantConst m : modes) out.lightCaps |= haModeCaps(m | "");
        if (!out.lightCaps) out.lightCaps = LIGHT_CAN_ONOFF;
    }

    // Null while off, like the levels.
    out.lightMode = haLightMode(attrs["color_mode"] | "");

    JsonVariantConst k = attrs["color_temp_kelvin"];
    if (k.is<int>()) out.colorTempK = (int16_t)haClamp(k.as<int>(), 1, 32767);
    JsonVariantConst kMin = attrs["min_color_temp_kelvin"];
    JsonVariantConst kMax = attrs["max_color_temp_kelvin"];
    if (kMin.is<int>()) out.minTempK = (uint16_t)haClamp(kMin.as<int>(), 0, 65535);
    if (kMax.is<int>()) out.maxTempK = (uint16_t)haClamp(kMax.as<int>(), 0, 65535);

    // [hue 0-360, saturation 0-100] as floats; HA sends it in a temperature
    // mode too (the white's nearest hue), which is what the virtual lamps copy.
    JsonArrayConst hs = attrs["hs_color"];
    if (!hs.isNull() && hs.size() == 2) {
        const float h = hs[0] | -1.0f, sat = hs[1] | -1.0f;
        if (h >= 0.0f && sat >= 0.0f) {
            out.hue = (int16_t)(((int)(h + 0.5f)) % 360);
            out.sat = (int8_t)haClamp((int)(sat + 0.5f), 0, 100);
        }
    }

    // A thermostat's (2.13, K144). Every climate entity sends these; nothing
    // else on the owner's HA sends a bare "temperature" attribute.
    out.curTemp10 = haTenths(attrs["current_temperature"]);
    out.target10  = haTenths(attrs["temperature"]);
    JsonVariantConst hum = attrs["current_humidity"];
    if (hum.is<float>() || hum.is<int>()) out.curHumidity = (int8_t)haClamp((int)(hum.as<float>() + 0.5f), 0, 100);
    snprintf(out.action, sizeof(out.action), "%s", attrs["hvac_action"] | "");
    snprintf(out.fanMode, sizeof(out.fanMode), "%s", attrs["fan_mode"] | "");
}

// ---------------------------------------------------------------------------
// AN ENTITY THE BOARD WAS NEVER TOLD ABOUT, 2.13 (design/ha-cards.md 2.3): a
// card added from HA's catalogue. Its kind from HA's domain, the way the
// compiled table declares each by hand; device class and unit as HA reported
// them when it was added. False for a domain no card can show yet.
// ---------------------------------------------------------------------------
inline bool haDescriptor(const char *ref, const char *deviceClass, const char *unit,
                         EntityDescriptor &d) {
    if (!ref || !ref[0] || !strchr(ref, '.')) return false;
    d = EntityDescriptor{};
    auto is = [&](const char *dom) {
        const size_t n = strlen(dom);
        return strncmp(ref, dom, n) == 0 && ref[n] == '.';
    };
    if (is("light")) {
        d.kind = EntityKind::LIGHT;         d.valueType = ValueType::BOOL; d.writable = true;
    } else if (is("switch") || is("input_boolean") || is("fan")) {
        d.kind = EntityKind::SWITCH;        d.valueType = ValueType::BOOL; d.writable = true;
    } else if (is("binary_sensor")) {
        d.kind = EntityKind::BINARY_SENSOR; d.valueType = ValueType::BOOL;
    } else if (is("sensor")) {
        // A unit, or a measuring device class, is a number; anything else (a
        // phase, a name) is words.
        const bool num = (unit && unit[0]) || (deviceClass && deviceClass[0] &&
                         strcmp(deviceClass, "enum") != 0 && strcmp(deviceClass, "timestamp") != 0 &&
                         strcmp(deviceClass, "date") != 0);
        d.kind = EntityKind::SENSOR;        d.valueType = num ? ValueType::FLOAT : ValueType::TEXT_VAL;
        if (num) snprintf(d.stateClass, sizeof(d.stateClass), "measurement");
    } else if (is("climate")) {
        d.kind = EntityKind::CLIMATE;       d.valueType = ValueType::TEXT_VAL;   // the mode; readings are attributes
    } else if (is("scene") || is("script") || is("button")) {
        d.kind = EntityKind::BUTTON;        d.valueType = ValueType::TEXT_VAL; d.writable = true;
    } else {
        return false;
    }
    d.source    = EntitySource::HA;
    d.advertise = false;   // someone else's
    snprintf(d.externalRef, sizeof(d.externalRef), "%s", ref);
    snprintf(d.deviceClass, sizeof(d.deviceClass), "%s", deviceClass ? deviceClass : "");
    snprintf(d.unit, sizeof(d.unit), "%s", unit ? unit : "");
    return true;
}

// ---------------------------------------------------------------------------
// What a state says about the ENTITY rather than its value - 2.10c (#65).
// Read by both paths for the same reason as the two above.
//
//   friendly_name  a learnt member's name (a declared entity keeps its own);
//                  for every entity, Entity::sourceName (Label "From HA")
//   entity_id      a light group's members, as HA or Hue define them. On the
//                  owner's light.office: three bulbs. Its `lights` list holds
//                  their names in a DIFFERENT order, so it is never read:
//                  each member's name comes from its own state
//                  (ha-websocket.md section 9).
// ---------------------------------------------------------------------------
inline void haEntityFilter(JsonObject attrs) {
    attrs["friendly_name"] = true;
    attrs["entity_id"]     = true;
}

inline void haLearn(EntityRegistry &reg, const Entity &e, JsonVariantConst attrs) {
    if (attrs.isNull()) return;
    if (e.learnt) reg.adoptName(e.desc.id, attrs["friendly_name"] | "");
    reg.setSourceName(e.desc.id, attrs["friendly_name"] | "");   // Label "From HA" (2.10d)

    // Lights only: a group of lights is commanded as one light, which is what
    // the window does with it. Other domains' groups wait for their cards.
    JsonArrayConst list = attrs["entity_id"];
    if (list.isNull() || e.desc.kind != EntityKind::LIGHT ||
        strncmp(e.desc.externalRef, "light.", 6) != 0) return;
    const char *refs[ENTITY_MEMBERS_MAX];
    uint8_t n = 0;
    for (JsonVariantConst v : list) {
        const char *r = v | "";
        if (n < ENTITY_MEMBERS_MAX && strncmp(r, "light.", 6) == 0) refs[n++] = r;
    }
    reg.learnMembers(e.desc.id, refs, n);
}

#endif // HA_VALUE_H
