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

// ---------------------------------------------------------------------------
// THE BINARY SENSOR TABLE. Milestone 2.7, cards.md section 13.
//
// One row per device_class, and one card type for all of them. A row carries
// everything that differs between a garage door and a motion sensor: the
// corner glyph, the hero's on/off pair, and the words a user may ask for in
// place of the name. Adding a class is adding a row.
//
// HA's own "Show as" setting is a device_class override, so it lands here with
// no extra work - the owner's garage doors are `opening` underneath and
// `garage_door` as shown.
//
// Every glyph here must be in the generated subset (scripts/gen_icon_font.py);
// the words are HA's own English wording for each class, so the panel and HA's
// frontend say the same thing about the same door. ASCII only - CLAUDE.md.
// ---------------------------------------------------------------------------
struct BinaryClass {
    const char *deviceClass;
    const char *corner;
    const char *on;
    const char *off;
    const char *wordOn;
    const char *wordOff;
};

static const BinaryClass BINARY_CLASSES[] = {
    // garage_door is its OWN class and does not match "door" - every lookup
    // is an exact compare - so it needs its own row.
    { "garage_door",  MDI_GARAGE,         MDI_GARAGE_OPEN,       MDI_GARAGE,        "Open",      "Closed"   },
    { "door",         MDI_DOOR_CLOSED,    MDI_DOOR_OPEN,         MDI_DOOR_CLOSED,   "Open",      "Closed"   },
    { "window",       MDI_WINDOW_CLOSED,  MDI_WINDOW_OPEN,       MDI_WINDOW_CLOSED, "Open",      "Closed"   },
    { "opening",      MDI_WINDOW_CLOSED,  MDI_WINDOW_OPEN,       MDI_WINDOW_CLOSED, "Open",      "Closed"   },
    { "occupancy",    MDI_MOTION_SENSOR,  MDI_MOTION_SENSOR,     MDI_MOTION_SENSOR_OFF, "Detected", "Clear" },
    { "motion",       MDI_MOTION_SENSOR,  MDI_MOTION_SENSOR,     MDI_MOTION_SENSOR_OFF, "Detected", "Clear" },
    { "presence",     MDI_ACCOUNT,        MDI_ACCOUNT,           MDI_ACCOUNT_OFF,   "Home",      "Away"     },
    // HA's lock binary_sensor is inverted from what the word suggests: ON
    // means UNLOCKED. Worded to match.
    { "lock",         MDI_LOCK,           MDI_LOCK_OPEN_VARIANT, MDI_LOCK,          "Unlocked",  "Locked"   },
    { "moisture",     MDI_WATER_ALERT,    MDI_WATER_ALERT,       MDI_WATER_PERCENT, "Wet",       "Dry"      },
    { "smoke",        MDI_SMOKE_DETECTOR, MDI_SMOKE_DETECTOR,    MDI_SMOKE_DETECTOR,"Detected",  "Clear"    },
    { "safety",       MDI_SHIELD_CHECK,   MDI_SHIELD_ALERT,      MDI_SHIELD_CHECK,  "Unsafe",    "Safe"     },
    { "problem",      MDI_ALERT_CIRCLE,   MDI_ALERT_CIRCLE,      MDI_CHECK_CIRCLE,  "Problem",   "OK"       },
    { "battery",      MDI_BATTERY,        MDI_BATTERY_ALERT,     MDI_BATTERY,       "Low",       "Normal"   },
    { "connectivity", MDI_WIFI,           MDI_WIFI,              MDI_WIFI_OFF,      "Connected", "Disconnected" },
    { "plug",         MDI_POWER_PLUG,     MDI_POWER_PLUG,        MDI_POWER_PLUG_OFF,"Plugged in","Unplugged" },
    { "power",        MDI_POWER,          MDI_POWER,             MDI_POWER,         "On",        "Off"      },
};

static const BinaryClass *binaryClass(const EntityDescriptor &d) {
    if (!d.deviceClass[0]) return nullptr;
    for (const BinaryClass &c : BINARY_CLASSES)
        if (strcmp(d.deviceClass, c.deviceClass) == 0) return &c;
    return nullptr;
}

// The icon a device_class or a domain implies, with no entity-specific
// override considered. Shared by the corner and by the hero's last resort.
static const char *classIcon(const EntityDescriptor &d) {
    if (d.kind == EntityKind::BINARY_SENSOR) {
        if (const BinaryClass *c = binaryClass(d)) return c->corner;
    }

    // device_class. Ordered most specific first: battery and
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
    // The binary classes - doors, occupancy, locks - are the table's job, above.
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

const char *cardCornerIcon(const Entity &e, bool cardHasHero) {
    const EntityDescriptor &d = e.desc;

    // 1. The user's override.
    if (const char *g = mdiGlyph(d.cornerIcon)) return g;

    // 2. OUR TABLE - device_class, then domain - which is what makes a light
    //    card wear a bulb in its corner wherever it appears. HA has no
    //    per-entity "type" icon to follow, so outside an override this is
    //    always ours (cards.md section 13).
    //
    //    EXCEPT when the corner is the card's ONLY icon. A value card has no
    //    hero glyph, so for a sensor our table has nothing specific to say
    //    about (no device_class) the entity's own icon is the better answer
    //    than a generic gauge - which is how the panel's own RSSI, heap and IP
    //    cards have always drawn wifi, memory and network glyphs.
    if (!cardHasHero && !d.deviceClass[0]) {
        if (const char *g = mdiGlyph(d.icon))       return g;
        if (const char *g = mdiGlyph(e.attrs.icon)) return g;
    }
    return classIcon(d);
}

const char *cardHeroIcon(const Entity &e, bool on) {
    const EntityDescriptor &d = e.desc;

    // Resolution order is cards.md section 13, and each step's reason is
    // there. A name the generated subset does not carry returns null from
    // mdiGlyph() and falls through, so an unknown icon draws the next-best
    // thing rather than tofu.

    // 1. The user's own state pair, both halves or neither.
    if (d.iconOn[0] && d.iconOff[0]) {
        if (const char *g = mdiGlyph(on ? d.iconOn : d.iconOff)) return g;
    }

    // 2. The user's single override.
    if (const char *g = mdiGlyph(d.icon)) return g;

    // 3. What the source says RIGHT NOW. Only present when a user or an
    //    integration set one - measured, see cards.md section 13 - but when it
    //    is, it may already be state-dependent (the owner's template occupancy
    //    sensors flip it) and it is his choice made in HA.
    if (const char *g = mdiGlyph(e.attrs.icon)) return g;

    // 4. Our own state pair, by device_class.
    if (d.kind == EntityKind::BINARY_SENSOR) {
        if (const BinaryClass *c = binaryClass(d)) return on ? c->on : c->off;
    }

    // 5. The domain's pair. Only where the two halves genuinely read
    //    differently; the card already carries state in its colour.
    switch (d.kind) {
        case EntityKind::LIGHT:  return on ? MDI_LIGHTBULB : MDI_LIGHTBULB_OUTLINE;
        case EntityKind::SWITCH: return on ? MDI_TOGGLE_SWITCH : MDI_TOGGLE_SWITCH_OFF;
        default: break;
    }

    // 6. Whatever the corner would say.
    return classIcon(d);
}

const char *cardStateWord(const EntityDescriptor &d, bool on) {
    if (d.kind == EntityKind::BINARY_SENSOR) {
        if (const BinaryClass *c = binaryClass(d)) return on ? c->wordOn : c->wordOff;
    }
    return on ? "On" : "Off";
}

static CardLabel s_labelMode = CardLabel::LBL_NAME;
void      cardSetLabelMode(CardLabel m) { s_labelMode = m; }
CardLabel cardLabelMode()               { return s_labelMode; }

CardLabel cardResolveLabel(CardLabel want) {
    if (want == CardLabel::LBL_INHERIT) want = s_labelMode;
    if (want == CardLabel::LBL_INHERIT) want = CardLabel::LBL_NAME;
    return want;
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

    // By kind of THING, for the corner icons of state cards - 2.7 round two.
    if (dc(d, "garage_door") || dc(d, "door") || dc(d, "window") ||
        dc(d, "opening") || dc(d, "lock"))             return p.TINT_OPENING;
    if (dc(d, "occupancy") || dc(d, "motion") ||
        dc(d, "presence"))                              return p.TINT_PRESENCE;
    if (d.kind == EntityKind::LIGHT)                    return p.TINT_LIGHTING;

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
