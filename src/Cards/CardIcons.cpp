#include "Cards/CardIcons.h"
#include "UI/UITokens.h"
#include "UI/UIIcons.h"
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
    // 1. THE ENTITY'S OWN ICON WINS.
    //
    // cards.md section 5: "Prefer provider-supplied. We already publish an
    // `icon` in our own MQTT discovery payloads, so the inbound direction
    // should be symmetrical: if HA or an MQTT discovery message names an icon,
    // use it." Every descriptor in the tree already carries one and until the
    // MDI subset existed nothing could draw it, so all of them were ignored.
    //
    // A name the subset does not contain returns null rather than something
    // approximate, so an unknown icon falls through to the rules below instead
    // of confidently drawing the wrong thing.
    if (const char *g = mdiGlyph(d.icon)) return g;

    // 2. device_class. Ordered most specific first: battery and
    //    signal_strength appear on entities of several kinds, so they have to
    //    be tested before anything falls through to a kind-based default.
    if (dc(d, "battery"))         return MDI_BATTERY;
    if (dc(d, "signal_strength")) return MDI_WIFI;
    if (dc(d, "temperature"))     return MDI_THERMOMETER;
    if (dc(d, "humidity"))        return MDI_WATER_PERCENT;
    if (dc(d, "illuminance"))     return MDI_BRIGHTNESS_5;
    if (dc(d, "power") ||
        dc(d, "energy"))          return MDI_LIGHTNING_BOLT;
    if (dc(d, "pressure"))        return MDI_GAUGE;
    if (dc(d, "occupancy") ||
        dc(d, "motion"))          return MDI_MOTION_SENSOR;
    if (dc(d, "door"))            return MDI_DOOR_OPEN;
    if (dc(d, "window") ||
        dc(d, "opening"))         return MDI_WINDOW_OPEN;
    if (dc(d, "lock"))            return MDI_LOCK;
    if (dc(d, "moisture"))        return MDI_WATER_ALERT;
    if (dc(d, "smoke"))           return MDI_SMOKE_DETECTOR;
    if (dc(d, "carbon_dioxide"))  return MDI_MOLECULE_CO2;
    if (dc(d, "pm25") ||
        dc(d, "volatile_organic_compounds")) return MDI_AIR_FILTER;

    // 3. The domain, as a last resort.
    switch (d.kind) {
        case EntityKind::LIGHT:         return MDI_LIGHTBULB;
        case EntityKind::SWITCH:        return MDI_TOGGLE_SWITCH;
        case EntityKind::BUTTON:        return MDI_GESTURE_TAP_BUTTON;
        case EntityKind::BINARY_SENSOR: return MDI_CHECK_CIRCLE;
        case EntityKind::NUMBER:        return MDI_SPEEDOMETER;
        case EntityKind::TEXT:          return MDI_SCRIPT_TEXT;
        case EntityKind::CLIMATE:       return MDI_THERMOSTAT;
        case EntityKind::WEATHER:       return MDI_WEATHER_PARTLY_CLOUDY;
        case EntityKind::SENSOR:        break;
    }
    return MDI_GAUGE;
}

const char *cardIconForState(const EntityDescriptor &d, bool on) {
    // A glyph that changes with the reading, which cards.md section 5 asks for
    // as the fallback behaviour: "a local set whose glyph varies with the
    // reading - bulb off / half / on, thermometer by band, occupancy present /
    // absent". Only the pairs that genuinely read differently are listed; a
    // switch toggling between two near-identical glyphs is noise, and the card
    // already carries state in its colour.
    if (dc(d, "occupancy") || dc(d, "motion"))
        return on ? MDI_MOTION_SENSOR : MDI_MOTION_SENSOR_OFF;
    if (dc(d, "door"))   return on ? MDI_DOOR_OPEN   : MDI_DOOR_CLOSED;
    if (dc(d, "window") || dc(d, "opening"))
        return on ? MDI_WINDOW_OPEN : MDI_WINDOW_CLOSED;
    if (dc(d, "lock"))   return on ? MDI_LOCK_OPEN_VARIANT : MDI_LOCK;

    if (!d.icon[0]) {
        switch (d.kind) {
            case EntityKind::LIGHT:  return on ? MDI_LIGHTBULB : MDI_LIGHTBULB_OUTLINE;
            case EntityKind::SWITCH: return on ? MDI_TOGGLE_SWITCH : MDI_TOGGLE_SWITCH_OFF;
            default: break;
        }
    }
    return cardIconFor(d);
}

const char *cardBatteryGlyph(int pct) {
    if (pct >= 90) return MDI_BATTERY;
    if (pct >= 70) return MDI_BATTERY_90;
    if (pct >= 50) return MDI_BATTERY_70;
    if (pct >= 30) return MDI_BATTERY_50;
    if (pct >= 10) return MDI_BATTERY_30;
    return MDI_BATTERY_ALERT;
}

uint32_t cardAreaColor(const char *area) {
    if (!area || !area[0]) return 0;

    // Hues chosen to stay distinguishable from each other AND from the
    // semantic state palette - an area must never be mistaken for a warning.
    // Nothing here is amber or red for that reason.
    static const uint32_t AREA_HUES[] = {
        0x4A9EDA,   // blue
        0x53B88A,   // green
        0x9B7BD4,   // violet
        0x2FA8A8,   // teal
        0xC06FA8,   // magenta
        0x6E86D6,   // indigo
        0x7FA644,   // olive
        0xCF7A5B,   // clay
    };
    static const uint32_t N = sizeof(AREA_HUES) / sizeof(AREA_HUES[0]);

    // FNV-1a. Small, well-spread, and deterministic - which is the only
    // property that actually matters here.
    uint32_t h = 2166136261u;
    for (const char *c = area; *c; c++) {
        h ^= (uint8_t)*c;
        h *= 16777619u;
    }
    return AREA_HUES[h % N];
}

uint32_t cardTintFor(const EntityDescriptor &d) {
    const UIPalette &p = UI::pal();

    if (dc(d, "temperature"))     return p.TINT_TEMP;
    if (dc(d, "humidity") ||
        dc(d, "moisture"))        return p.TINT_HUMID;
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
