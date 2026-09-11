#include "Cards/CardIcons.h"
#include "UI/UITokens.h"
#include <lvgl.h>
#include <stdio.h>
#include <string.h>

// device_class is the routing key throughout this file rather than EntityKind,
// and that is the right choice for the same reason Home Assistant made it:
// kind says what you can DO with an entity, device_class says what it IS. A
// temperature and an illuminance are both EntityKind::SENSOR and must not look
// alike; a switch and a light are different kinds and largely should.
static bool dc(const EntityDescriptor &d, const char *what) {
    return strcmp(d.deviceClass, what) == 0;
}

const char *cardIconFor(const EntityDescriptor &d) {
    // Ordered most specific first: battery and signal_strength are device
    // classes that appear on entities of several kinds, so they have to be
    // tested before anything falls through to a kind-based default.
    if (dc(d, "battery"))         return LV_SYMBOL_BATTERY_FULL;
    if (dc(d, "signal_strength")) return LV_SYMBOL_WIFI;
    if (dc(d, "temperature"))     return LV_SYMBOL_TINT;
    if (dc(d, "humidity"))        return LV_SYMBOL_TINT;
    if (dc(d, "illuminance"))     return LV_SYMBOL_EYE_OPEN;
    if (dc(d, "power") ||
        dc(d, "energy"))          return LV_SYMBOL_CHARGE;
    if (dc(d, "occupancy") ||
        dc(d, "motion"))          return LV_SYMBOL_EYE_OPEN;
    if (dc(d, "door") ||
        dc(d, "opening"))         return LV_SYMBOL_DRIVE;

    switch (d.kind) {
        case EntityKind::LIGHT:
        case EntityKind::SWITCH:        return LV_SYMBOL_POWER;
        case EntityKind::BUTTON:        return LV_SYMBOL_PLAY;
        case EntityKind::BINARY_SENSOR: return LV_SYMBOL_EYE_OPEN;
        case EntityKind::NUMBER:        return LV_SYMBOL_SETTINGS;
        case EntityKind::TEXT:          return LV_SYMBOL_FILE;
        case EntityKind::CLIMATE:       return LV_SYMBOL_HOME;
        case EntityKind::WEATHER:       return LV_SYMBOL_IMAGE;
        case EntityKind::SENSOR:        break;
    }
    return LV_SYMBOL_LIST;
}

uint32_t cardTintFor(const EntityDescriptor &d) {
    const UIPalette &p = UI::pal();

    if (dc(d, "temperature"))     return p.TINT_TEMP;
    if (dc(d, "humidity"))        return p.TINT_HUMID;
    if (dc(d, "illuminance"))     return p.TINT_LIGHT;
    if (dc(d, "power") ||
        dc(d, "energy"))          return p.TINT_POWER;
    if (dc(d, "pm25") ||
        dc(d, "carbon_dioxide") ||
        dc(d, "volatile_organic_compounds")) return p.TINT_AIR;

    // Everything without a tint of its own falls back to the accent rather
    // than to a grey. A sensor wall is meant to be scannable; an untinted card
    // should read as "no category", not as "broken".
    return p.ACCENT;
}

void cardFormatAge(uint32_t ageMs, char *out, size_t cap) {
    const uint32_t s = ageMs / 1000;
    if      (s < 10)     snprintf(out, cap, "now");
    else if (s < 60)     snprintf(out, cap, "%us", (unsigned)s);
    else if (s < 3600)   snprintf(out, cap, "%um", (unsigned)(s / 60));
    else if (s < 86400)  snprintf(out, cap, "%uh", (unsigned)(s / 3600));
    else                 snprintf(out, cap, "%ud", (unsigned)(s / 86400));
}

void cardFormatValue(const Entity &e, char *out, size_t cap, bool withUnit) {
    out[0] = '\0';

    // Never set, and not a lie about it. An em dash would be the typographic
    // answer and renders as a tofu box in stock Montserrat, so this is two
    // ASCII hyphens on purpose - see CardIcons.h.
    if (!e.everSet) { snprintf(out, cap, "--"); return; }

    const char *unit = (withUnit && e.desc.unit[0]) ? e.desc.unit : "";

    switch (e.value.type) {
        case ValueType::FLOAT:
            // One decimal. A panel read from across a room gains nothing from
            // the second, and a Zigbee sensor's own quantisation means the
            // extra digit is frequently noise rather than precision.
            snprintf(out, cap, "%.1f%s", (double)e.value.f, unit);
            break;
        case ValueType::INT:
            snprintf(out, cap, "%ld%s", (long)e.value.i, unit);
            break;
        case ValueType::TEXT_VAL:
            snprintf(out, cap, "%s", e.value.text);
            break;
        case ValueType::BOOL:
            // Deliberately empty. cards.md section 4 forbids state words; the
            // icon and its colour carry it.
            break;
        case ValueType::NONE:
            snprintf(out, cap, "--");
            break;
    }
}
