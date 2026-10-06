#include "VirtualProvider.h"
#include "VirtualEntities.h"
#include "LightColor.h"

// Every entity here that is expected to ANSWER. test_stuck is deliberately
// absent: its command is left to expire, which is the failure the whole
// harness exists to show.
static const char *const ECHOERS[] = {
    VIRT_ENT_SWITCH, VIRT_ENT_L1, VIRT_ENT_L2, VIRT_ENT_L3, VIRT_ENT_L4,
};
static const uint8_t ECHOER_COUNT = sizeof(ECHOERS) / sizeof(ECHOERS[0]);

// ---------------------------------------------------------------------------
// THE LAMPS ARE FOUR DIFFERENT KINDS OF LIGHT (2.10b), so a window can be seen
// hiding what a light cannot do, and "All Lamps" - a group of all four - can
// be seen doing what an HA light group does with members that differ:
//
//   Lamp 1  dimmable                  Power | Brightness
//   Lamp 2  dimmable, white range     Power | Brightness, Temperature
//   Lamp 3  dimmable, white, colour   Power | Brightness, Temperature, Colour
//   Lamp 4  on/off only               Power
//
// Each plays a bulb: it REMEMBERS its brightness and colour while off, and
// comes back on at them, which is what the owner saw HA do with a group
// (2026-10-05) - and, like HA, reports no levels at all while it is off.
// ---------------------------------------------------------------------------
namespace {
struct LampModel {
    const char *id;
    uint8_t     caps;
    uint16_t    minK, maxK;
};
const LampModel LAMPS[] = {
    { VIRT_ENT_L1, LIGHT_CAN_ONOFF | LIGHT_CAN_DIM,                                       0,    0 },
    { VIRT_ENT_L2, LIGHT_CAN_ONOFF | LIGHT_CAN_DIM | LIGHT_CAN_TEMP,                   2200, 6500 },
    { VIRT_ENT_L3, LIGHT_CAN_ONOFF | LIGHT_CAN_DIM | LIGHT_CAN_TEMP | LIGHT_CAN_COLOUR, 2000, 6500 },
    { VIRT_ENT_L4, LIGHT_CAN_ONOFF,                                                       0,    0 },
};
constexpr uint8_t LAMP_COUNT = sizeof(LAMPS) / sizeof(LAMPS[0]);

// What each bulb remembers.
struct Bulb {
    int16_t   bri = 180;
    int16_t   kelvin = 2700;
    int16_t   hue = 30;
    int8_t    sat = 80;
    LightMode mode = LightMode::LMODE_TEMP;
};
Bulb s_bulb[LAMP_COUNT];

int lampIndex(const char *id) {
    for (uint8_t i = 0; i < LAMP_COUNT; i++) if (strcmp(LAMPS[i].id, id) == 0) return i;
    return -1;
}

// What the lamp reports, as HA would put it: what it can do always; its
// levels only while on.
EntityAttrs report(uint8_t i, bool on) {
    const LampModel &m = LAMPS[i];
    const Bulb &b = s_bulb[i];
    EntityAttrs a;
    a.lightCaps = m.caps;
    a.minTempK  = m.minK;
    a.maxTempK  = m.maxK;
    if (!on) return a;
    if (!(m.caps & LIGHT_CAN_DIM)) { a.lightMode = LightMode::LMODE_ONOFF; return a; }
    a.brightness = b.bri;
    const bool colour = (m.caps & LIGHT_CAN_COLOUR) && b.mode == LightMode::LMODE_COLOUR;
    const bool temp   = (m.caps & LIGHT_CAN_TEMP) && !colour;
    if (colour) {
        a.lightMode = LightMode::LMODE_COLOUR;
        a.hue = b.hue; a.sat = b.sat;
    } else if (temp) {
        a.lightMode  = LightMode::LMODE_TEMP;
        a.colorTempK = b.kelvin;
        lightKelvinToHs(b.kelvin, a.hue, a.sat);
    } else {
        a.lightMode = LightMode::LMODE_DIM;
    }
    a.rgb    = lightShownRgb(a);
    a.hasRgb = (a.rgb != 0);
    return a;
}

// The bulb takes what it was asked for - clamped to what it can do, as a real
// one would be (a temperature outside its range lands at the nearest end).
void absorb(uint8_t i, const EntityAttrs &asked) {
    const LampModel &m = LAMPS[i];
    Bulb &b = s_bulb[i];
    if (asked.brightness > 0) b.bri = asked.brightness;
    if (asked.lightMode == LightMode::LMODE_COLOUR && (m.caps & LIGHT_CAN_COLOUR) && asked.hue >= 0) {
        b.mode = LightMode::LMODE_COLOUR;
        b.hue  = asked.hue;
        b.sat  = asked.sat >= 0 ? asked.sat : 100;
    } else if (asked.lightMode == LightMode::LMODE_TEMP && (m.caps & LIGHT_CAN_TEMP) && asked.colorTempK > 0) {
        b.mode   = LightMode::LMODE_TEMP;
        b.kelvin = asked.colorTempK;
        if (m.minK && b.kelvin < (int16_t)m.minK) b.kelvin = (int16_t)m.minK;
        if (m.maxK && b.kelvin > (int16_t)m.maxK) b.kelvin = (int16_t)m.maxK;
    }
}
} // namespace

void VirtualProvider::begin(EntityRegistry *reg) {
    _reg = reg;
    if (!_reg) return;

    for (uint8_t i = 0; i < VIRTUAL_ENTITY_COUNT; i++) {
        _reg->add(VIRTUAL_ENTITIES[i]);
    }

    // All start off, and start SET rather than never-set. A switch that has
    // never had a value is a different thing from a switch that is off, and
    // only one of those is true here - we know perfectly well what state these
    // are in, because we are the device.
    for (uint8_t i = 0; i < VIRTUAL_ENTITY_COUNT; i++) {
        _reg->setValue(VIRTUAL_ENTITIES[i].id, EntityValue::makeBool(false), 0);
    }
    for (uint8_t i = 0; i < LAMP_COUNT; i++) _reg->setAttrs(LAMPS[i].id, report(i, false));
}

void VirtualProvider::loop(uint32_t nowMs) {
    if (!_reg) return;

    for (uint8_t i = 0; i < ECHOER_COUNT; i++) {
        Entity *e = _reg->find(ECHOERS[i]);
        if (!e) continue;
        const int lamp = lampIndex(ECHOERS[i]);

        // A LIGHT'S LEVELS FIRST (2.10b): the bulb takes what was asked once
        // the echo delay has passed since the LAST command - so a slider
        // dragged for two seconds is answered once, after it stops, as a
        // slow bulb would be. The report then matches the latest command and
        // ends the registry's wait.
        bool levelsDue = false;
        if (lamp >= 0 && e->attrPending && nowMs - e->attrPendingSinceMs >= VIRT_ECHO_DELAY_MS) {
            absorb((uint8_t)lamp, e->attrs);
            levelsDue = true;
        }

        const bool valueDue = e->pending && nowMs - e->pendingSinceMs >= VIRT_ECHO_DELAY_MS;
        if (valueDue) {
            // Echo back the value that was optimistically applied - which is
            // what a cooperative device does, and why the echo carries no new
            // information. setValue() finds nothing changed and declines to
            // dirty the entity; the one thing that matters is that it clears
            // `pending`, and that flag is what the card is watching. See Card.h.
            //
            // Copied rather than passed by reference: setValue() takes the lock
            // and then assigns into the very field the reference points at.
            const EntityValue echoed = e->value;
            _reg->setValue(ECHOERS[i], echoed, nowMs);
        }

        // A lamp that has just answered says what it is now showing: its
        // remembered levels when on, none when off.
        if (lamp >= 0 && (levelsDue || valueDue)) {
            const bool on = e->value.type == ValueType::BOOL && e->value.b;
            _reg->setAttrs(ECHOERS[i], report((uint8_t)lamp, on));
        }
    }
}
