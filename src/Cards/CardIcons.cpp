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
    // garage_door is its OWN class and does not match "door" - dc() is an
    // exact compare. Without this the owner's two garage sensors fall all the
    // way through to the BINARY_SENSOR default and draw a check circle. They
    // are only correct today because HA sends an explicit mdi:garage, and the
    // whole point of dashboard-target-7b.md is that he should be able to
    // REMOVE that override to get the open/closed pair back.
    if (dc(d, "garage_door"))     return MDI_GARAGE;
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
    // The pair the owner actually asked for. It only engages once the static
    // mdi:garage override is removed in HA - while that override stands, HA
    // sends the same glyph for both states and step 1 of cardIconFor() honours
    // it, which is the behaviour he is currently seeing and disliking.
    if (dc(d, "garage_door")) return on ? MDI_GARAGE_OPEN : MDI_GARAGE;
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
    // Eight hues spread right around the wheel, deliberately far apart rather
    // than merely different - two areas next to each other on a page have to
    // be told apart at a glance, not on inspection. None of them is amber or
    // red: an area must never be mistaken for a warning.
    static const uint32_t AREA_HUES[] = {
        0x3E8FD9,   // blue
        0x38B24A,   // green
        0xA05FD6,   // violet
        0x11A8A8,   // teal
        0xD45A9E,   // magenta
        0x8FBF3F,   // lime
        0x5C6BC0,   // indigo
        0xE07A3F,   // ochre
    };
    static const uint32_t N = sizeof(AREA_HUES) / sizeof(AREA_HUES[0]);

    // FNV-1a. Small, well-spread, and deterministic - which is the only
    // property that actually matters here.
    uint32_t h = 2166136261u;
    for (const char *c = area; *c; c++) {
        h ^= (uint8_t)*c;
        h *= 16777619u;
    }

    // Fold the whole hash down rather than taking its low bits. A modulo of
    // the raw value leans on the last byte mixed in, which is why "Kitchen"
    // and "Lounge" landed on the same hue - they are the same length and end
    // similarly. Mixing the high half back in first spreads short names.
    // Finalised with a 15-bit fold rather than 16. Short names of similar
    // length - "Office", "Lounge", "Panel" - collided under the 16-bit fold and
    // landed on the same hue. Checked against the areas actually in use: all
    // five take a different colour, and seven of eight longer names do.
    //
    // A hash cannot GUARANTEE distinctness, and this does not pretend to. What
    // it guarantees is stability: the same name is the same colour on every
    // board and across reboots, which handing colours out in arrival order
    // would not be.
    h ^= h >> 15;
    h *= 2246822519u;
    h ^= h >> 13;
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

namespace {

// The fleet default. Fahrenheit because that is the owner's standing
// preference; a build sheet overrides it per page or per card, and TEMP_SOURCE
// turns conversion off entirely.
TempUnit s_tempUnit = TempUnit::TEMP_F;

DurationFormat s_durFormat = DurationFormat::DUR_AUTO;

bool isTemperature(const EntityDescriptor &d) {
    return strcmp(d.deviceClass, "temperature") == 0;
}

// Keyed on device_class, exactly like isTemperature() above, and NOT on the
// unit string. "s" is a perfectly good unit for something that is not a
// duration, and device_class is the field whose whole job is to say what a
// number means. sys_uptime already declares it.
bool isDuration(const EntityDescriptor &d) {
    return strcmp(d.deviceClass, "duration") == 0;
}

DurationFormat resolveDur(DurationFormat want) {
    if (want == DurationFormat::DUR_INHERIT) want = s_durFormat;
    if (want == DurationFormat::DUR_INHERIT) want = DurationFormat::DUR_AUTO;
    return want;
}

// Seconds -> the chosen text. Returns false when the caller should fall
// through to ordinary number formatting.
//
// Takes SECONDS, not milliseconds. The issue's title said milliseconds and the
// entity has always been seconds - SystemProvider writes nowMs / 1000 and a
// System Doctor dump reads "sys_uptime = 598 s". Dividing again here would
// show an uptime a thousand times short, which looks plausible and is the
// reason this comment exists.
bool formatDuration(int32_t secs, DurationFormat want, char *out, size_t cap) {
    if (secs < 0) return false;                      // not a duration we can draw

    const uint32_t s = (uint32_t)secs;
    const uint32_t days = s / 86400u;
    const uint32_t hrs  = (s % 86400u) / 3600u;
    const uint32_t mins = (s % 3600u) / 60u;
    const uint32_t secr = s % 60u;

    switch (resolveDur(want)) {
        case DurationFormat::DUR_SECONDS:
            return false;                            // caller prints the raw number

        case DurationFormat::DUR_CLOCK:
            // Hours accumulate rather than wrapping at 24 - a panel up for
            // three weeks says 504:00:00, which is wide and true.
            snprintf(out, cap, "%lu:%02lu:%02lu",
                     (unsigned long)(days * 24u + hrs),
                     (unsigned long)mins, (unsigned long)secr);
            return true;

        case DurationFormat::DUR_AUTO:
        default:
            // Seconds are dropped past the first day on purpose. Nobody reads
            // the seconds digit of a three-week uptime, and dropping it is
            // what keeps the widest case from being the one that overflows the
            // card it has to sit in.
            if (days)      snprintf(out, cap, "%lud %02lu:%02lu",
                                    (unsigned long)days, (unsigned long)hrs,
                                    (unsigned long)mins);
            else if (hrs)  snprintf(out, cap, "%lu:%02lu:%02lu",
                                    (unsigned long)hrs, (unsigned long)mins,
                                    (unsigned long)secr);
            else           snprintf(out, cap, "%02lu:%02lu",
                                    (unsigned long)mins, (unsigned long)secr);
            return true;
    }
}

// Which unit an entity's own string means. The degree sign is two UTF-8 bytes,
// so this reads the LAST character rather than trying to match the whole
// string - "C", "degC" and the degree-prefixed form all answer the same.
char sourceTempLetter(const EntityDescriptor &d) {
    const size_t n = strlen(d.unit);
    if (!n) return 0;
    const char last = d.unit[n - 1];
    return (last == 'C' || last == 'F') ? last : 0;
}

TempUnit resolveWant(TempUnit want) {
    if (want == TempUnit::TEMP_INHERIT) want = s_tempUnit;
    if (want == TempUnit::TEMP_INHERIT) want = TempUnit::TEMP_SOURCE;
    return want;
}

// UTF-8 for the degree sign, written as bytes so this file's own encoding
// cannot silently mangle it - the same guard ExternalEntities.h uses. The
// degree sign is inside stock Montserrat; an em dash and a middle dot are not.
const char *DEG_C = "\xC2\xB0" "C";
const char *DEG_F = "\xC2\xB0" "F";

} // namespace

// Decode one UTF-8 sequence. Hand-rolled rather than borrowed from LVGL's
// internals: the MDI glyphs live in the private use area above U+F0000, which
// is a FOUR byte sequence, and this needs to be right for exactly that case.
static uint32_t utf8First(const char *s) {
    const unsigned char *p = (const unsigned char *)s;
    if (!p || !p[0]) return 0;
    if (p[0] < 0x80) return p[0];
    if ((p[0] & 0xE0) == 0xC0 && p[1])
        return (uint32_t)(p[0] & 0x1F) << 6 | (p[1] & 0x3F);
    if ((p[0] & 0xF0) == 0xE0 && p[1] && p[2])
        return (uint32_t)(p[0] & 0x0F) << 12 | (uint32_t)(p[1] & 0x3F) << 6 | (p[2] & 0x3F);
    if ((p[0] & 0xF8) == 0xF0 && p[1] && p[2] && p[3])
        return (uint32_t)(p[0] & 0x07) << 18 | (uint32_t)(p[1] & 0x3F) << 12
             | (uint32_t)(p[2] & 0x3F) << 6  | (p[3] & 0x3F);
    return 0;
}

int32_t cardGlyphTopBearing(const lv_font_t *font, const char *utf8) {
    if (!font || !utf8) return 0;
    const uint32_t cp = utf8First(utf8);
    if (!cp) return 0;

    lv_font_glyph_dsc_t g;
    if (!lv_font_get_glyph_dsc(font, &g, cp, 0)) return 0;

    const int32_t baseline = (int32_t)font->line_height - (int32_t)font->base_line;
    const int32_t inkTop   = baseline - ((int32_t)g.ofs_y + (int32_t)g.box_h);
    return inkTop > 0 ? inkTop : 0;
}

void     cardSetTempUnit(TempUnit u) { s_tempUnit = u; }
TempUnit cardTempUnit()              { return s_tempUnit; }

void           cardSetDurationFormat(DurationFormat f) { s_durFormat = f; }
DurationFormat cardDurationFormat()                    { return s_durFormat; }

const char *cardDisplayUnit(const Entity &e, TempUnit want) {
    if (!e.desc.unit[0]) return "";

    // A formatted duration carries its own units inside the text - "4:15:33"
    // followed by "s" would be a caption that lies, which is the same rule
    // that sends temperature through this function rather than desc.unit.
    if (isDuration(e.desc) && resolveDur(DurationFormat::DUR_INHERIT)
                              != DurationFormat::DUR_SECONDS) return "";

    if (!isTemperature(e.desc)) return e.desc.unit;

    const char src = sourceTempLetter(e.desc);
    if (!src) return e.desc.unit;          // a temperature in neither C nor F

    switch (resolveWant(want)) {
        case TempUnit::TEMP_C: return DEG_C;
        case TempUnit::TEMP_F: return DEG_F;
        default:               return e.desc.unit;
    }
}

// The value as a number in the wanted unit, or the value unchanged when there
// is nothing to convert.
static float displayValue(const Entity &e, TempUnit want, bool &converted) {
    converted = false;
    float v = (e.value.type == ValueType::FLOAT) ? e.value.f
            : (e.value.type == ValueType::INT)   ? (float)e.value.i : 0.0f;
    if (!isTemperature(e.desc)) return v;

    const char src = sourceTempLetter(e.desc);
    if (!src) return v;

    const TempUnit w = resolveWant(want);
    if (w == TempUnit::TEMP_F && src == 'C') { converted = true; return v * 9.0f / 5.0f + 32.0f; }
    if (w == TempUnit::TEMP_C && src == 'F') { converted = true; return (v - 32.0f) * 5.0f / 9.0f; }
    return v;
}

void cardFormatValue(const Entity &e, char *out, size_t cap, bool withUnit, TempUnit want) {
    out[0] = '\0';

    // Never set, and not a lie about it. An em dash would be the typographic
    // answer and renders as a tofu box in stock Montserrat, so this is two
    // ASCII hyphens on purpose - see CardIcons.h.
    if (!e.everSet) { snprintf(out, cap, "--"); return; }

    // A duration is text, not a number with a unit, so it resolves before the
    // numeric paths below and returns straight out. DUR_SECONDS declines,
    // which falls through and prints the raw count exactly as before.
    if (isDuration(e.desc) && e.value.type == ValueType::INT &&
        formatDuration(e.value.i, DurationFormat::DUR_INHERIT, out, cap)) {
        return;
    }

    const char *unit = withUnit ? cardDisplayUnit(e, want) : "";

    bool converted = false;
    const float dv = displayValue(e, want, converted);

    switch (e.value.type) {
        case ValueType::FLOAT:
            // One decimal. A panel read from across a room gains nothing from
            // the second, and a Zigbee sensor's own quantisation means the
            // extra digit is frequently noise rather than precision.
            snprintf(out, cap, "%.1f%s", (double)dv, unit);
            break;
        case ValueType::INT:
            // A converted integer stops being one. 22 C is 71.6 F, and
            // printing 71 would be a rounding the source never made - so a
            // conversion promotes the reading to one decimal and an
            // unconverted integer stays exactly what arrived.
            if (converted) snprintf(out, cap, "%.1f%s", (double)dv, unit);
            else           snprintf(out, cap, "%ld%s", (long)e.value.i, unit);
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
