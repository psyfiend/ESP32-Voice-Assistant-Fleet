// The window's shared helpers (2.12 P1, split from CardPopup.cpp): its sizes, bare objects and
// labels, and what the entities it shows say - members, on or off, a light's levels.
#include "PopupState.h"

namespace popup {

// Millimetres on glass to this panel's pixels - UI::mm(), the same derivation
// as UI::minTouch(), so the window is the same physical size on every board.
int32_t mm(float v) { return UI::mm(v); }

// "P4_5 millimetres": a length that is `v` mm in the P4_5's window, the same
// SHARE of this one (owner, 2026-10-05: the hero the same proportion of the
// window on every board). For the hero and what is laid out around it; touch
// targets and text stay real millimetres, so they are the same size under a
// finger everywhere.
int32_t pm(float v) {
    return (int32_t)lroundf(v * (float)s.propH / REF_WIN_H_MM);
}

// A bare object, and a label in a font and colour: the kit's (2.12 P3).
lv_obj_t *plain(lv_obj_t *parent) { return Kit::plain(parent); }

lv_obj_t *makeLabel(lv_obj_t *parent, const lv_font_t *f, uint32_t hex) { return Kit::label(parent, f, hex); }

// Only touch LVGL when the words actually change: a re-render runs every
// second for the "N ago" line, and setting identical text still invalidates.
void setText(lv_obj_t *l, const char *t) {
    if (!l) return;
    const char *cur = lv_label_get_text(l);
    if (cur && strcmp(cur, t) == 0) return;
    lv_label_set_text(l, t);
}

// How wide a line of text draws, in pixels.
int32_t textW(const char *txt, const lv_font_t *f) { return Kit::textW(txt, f); }

// A round icon button at the minimum touch size: a soft dent in the card
// surface (owner, round 6), so it reads as a button and not a stray glyph
// (owner, H1), tinted while pressed - the kit's (Kit::iconButton()).
lv_obj_t *iconButton(lv_obj_t *parent, const char *glyph, const lv_font_t *f,
                     lv_event_cb_t cb, lv_obj_t **outLabel) {
    return Kit::iconButton(parent, glyph, f, cb, outLabel);
}

// ---------------------------------------------------------------------------
// The members of what the window stands for (2.10c): a group defined here -
// the card's entities - or at the source - the members the registry learnt.
// The Members view lists these, and a tap on one opens it.
// ---------------------------------------------------------------------------
uint8_t memberCount() { return s.native ? s.native->nMembers : s.nEnt; }

const Entity *memberAt(uint8_t i) {
    if (s.native) return s.reg ? s.reg->memberOf(*s.native, i) : nullptr;
    return (i < s.nEnt) ? s.ent[i] : nullptr;
}

bool hasMembers() { return s.native ? s.native->nMembers > 0 : s.nEnt > 1; }

// Should ent[] hold the members? While any is paused - and the group itself is
// not, which is a group paused as a whole (Card::setPaused pauses both).
bool nativeWantsMembers() {
    if (!s.native || s.native->paused) return false;
    for (uint8_t i = 0; i < s.native->nMembers; i++) {
        const Entity *m = memberAt(i);
        if (m && m->paused) return true;
    }
    return false;
}

void applyNative() {
    if (!s.native) return;
    s.expanded = nativeWantsMembers();
    for (const Entity *&e : s.ent) e = nullptr;
    s.nEnt = 0;
    if (!s.expanded) { s.ent[s.nEnt++] = s.native; return; }
    for (uint8_t i = 0; i < s.native->nMembers && s.nEnt < POPUP_ENT_MAX; i++)
        if (const Entity *m = memberAt(i)) s.ent[s.nEnt++] = m;
}

// ---------------------------------------------------------------------------
// What the entities say, aggregated the way StateCard aggregates them: a card
// with several primaries is "on" when any of them is.
// ---------------------------------------------------------------------------

// Every member paused? Then the window is paused, and shows them as frozen.
bool allPaused() {
    uint8_t n = 0;
    for (uint8_t i = 0; i < s.nEnt; i++) if (s.ent[i] && s.ent[i]->paused) n++;
    return s.nEnt && n == s.nEnt;
}

// A PAUSED MEMBER IS OUT OF THE GROUP (owner, 2.10b round 1) - not counted,
// not offered, not commanded - unless every member is; Card::counts() is the
// same rule for the card.
bool counts(const Entity *e) {
    return e && (!e->paused || allPaused());
}

// ON BY THE CARD'S GROUP RULE (2.10b): any member, or all of them - GroupOn,
// what an HA group helper offers. One entity is the same rule with one member.
Agg aggregate() {
    Agg a;
    uint8_t nBool = 0, nOn = 0;
    a.paused = allPaused();
    for (uint8_t i = 0; i < s.nEnt; i++) {
        const Entity *e = s.ent[i];
        if (!e) continue;
        if (!counts(e)) { a.nPaused++; continue; }
        if (e->value.type == ValueType::BOOL) { a.anyBool = true; nBool++; if (e->value.b) nOn++; }
        if (!e->available) a.available = false;
        if (e->everSet) {
            a.everSet = true;
            if (e->lastChangeMs > a.lastChangeMs) a.lastChangeMs = e->lastChangeMs;
        }
    }
    a.on = (s.groupOn == GroupOn::GROUP_ON_ALL) ? (nBool && nOn == nBool) : nOn > 0;
    return a;
}

// ---------------------------------------------------------------------------
// A light's levels, aggregated the way HA's light group does it: what the
// members can do is the UNION (any mode any member supports is offered); the
// levels are the mean over the members that are on and report one - the hue
// a circular mean, so red and magenta do not average to green. A member that
// is off reports no levels, as in HA.
// ---------------------------------------------------------------------------

LightAgg lightAggregate() {
    LightAgg L;
    int32_t briSum = 0, briN = 0, kSum = 0, kN = 0, satSum = 0, hN = 0;
    float hx = 0.f, hy = 0.f;
    const bool group = s.nEnt > 1;
    for (uint8_t i = 0; i < s.nEnt; i++) {
        const Entity *e = s.ent[i];
        if (!e) continue;
        const EntityAttrs &at = e->attrs;
        L.allCaps |= at.lightCaps;
        // IN A GROUP A PAUSED MEMBER NEVER OFFERS A MODE - not even when every
        // member is paused (owner, round 3 P4: pausing the last one brought
        // Lamp 3's Colour back). One paused light keeps its own, greyed.
        if (!group || !e->paused) L.caps |= at.lightCaps;
        if (!counts(e)) continue;   // and its levels are left out too
        if (at.minTempK && (!L.minK || at.minTempK < L.minK)) L.minK = at.minTempK;
        if (at.maxTempK > L.maxK) L.maxK = at.maxTempK;
        if (!(e->value.type == ValueType::BOOL && e->value.b)) continue;
        if (at.brightness >= 0) { briSum += at.brightness; briN++; }
        if (at.lightMode == LightMode::LMODE_TEMP && at.colorTempK > 0) { kSum += at.colorTempK; kN++; }
        if (at.lightMode == LightMode::LMODE_COLOUR && at.hue >= 0) {
            const float r = (float)at.hue * 0.0174533f;
            hx += cosf(r); hy += sinf(r);
            satSum += at.sat >= 0 ? at.sat : 100;
            hN++;
        }
    }
    if (briN) L.bri    = (briSum + briN / 2) / briN;
    if (kN)   L.kelvin = (kSum + kN / 2) / kN;
    if (hN) {
        int32_t h = (int32_t)lroundf(atan2f(hy, hx) * 57.2958f);
        L.hue = (h + 360) % 360;
        L.sat = (satSum + hN / 2) / hN;
    }
    L.mode = (hN > kN) ? LightMode::LMODE_COLOUR : kN ? LightMode::LMODE_TEMP
           : briN      ? LightMode::LMODE_DIM    : LightMode::LMODE_UNKNOWN;
    if (!L.minK || L.maxK <= L.minK) { L.minK = 2000; L.maxK = 6500; }   // HA's own defaults
    return L;
}

// "Temperature" from "temperature", "Carbon dioxide" from "carbon_dioxide".
// HA's device_class is the honest answer to "what is this a reading of".
const char *whatWord(const Entity &e, char *buf, size_t cap) {
    const char *dc = e.desc.deviceClass;
    if (!dc[0]) return e.value.type == ValueType::TEXT_VAL ? "Status" : "Value";
    size_t n = 0;
    for (; dc[n] && n < cap - 1; n++) buf[n] = (dc[n] == '_') ? ' ' : dc[n];
    buf[n] = '\0';
    if (buf[0] >= 'a' && buf[0] <= 'z') buf[0] = (char)(buf[0] - 'a' + 'A');
    return buf;
}

// --- A PAUSED WINDOW (owner, 2.10b round 1) ---------------------------------
// Its controls are greyed - every colour mixed toward the window - and a
// PAUSED pill, the card's own badge, sits at the top of the column. Built
// in at build time (the strips' colours are baked into their pieces), so a
// change of pause rebuilds the view.
uint32_t quiet(uint32_t hex) {
    return s.builtPaused ? UI::mix(hex, UI::pal().SURFACE_ALT, 65) : hex;
}

// Inside one member's own views (its controls, its history): the group's
// entities are set aside in groupEnt while ent[] holds the member.
bool inMember() { return s.groupN > 0; }

// Rough luminance, to keep the fill visible against its track.
int32_t lumOf(uint32_t hex) {
    return (int32_t)(((hex >> 16) & 0xFF) * 299 + ((hex >> 8) & 0xFF) * 587 + (hex & 0xFF) * 114) / 1000;
}
} // namespace popup
