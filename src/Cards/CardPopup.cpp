#include "Cards/CardPopup.h"
#include "Cards/Card.h"
#include "Cards/CardIcons.h"
#include "LightColor.h"
#include "UI/UITokens.h"
#include "UI/UIToolkit.h"
#include "src/core/lv_obj_event_private.h"   // lv_hit_test_info_t: the tap catcher's hit test
#include "src/core/lv_obj_draw_private.h"    // lv_obj_get_ext_draw_size: the window's shadow
#include <Arduino.h>
#include <math.h>
#include <stdarg.h>
#include <string.h>

// See CardPopup.h for what this is and docs/design/card-sheet.md sections 11-13
// for why it looks the way it does. Step 1 of 2.10a: the window, its header,
// the four ways to close, the body and the inner views. The settings deck
// comes next.
//
// USABLE FIRST (owner, 2026-10-04 evening). The window used to grow out of the
// card and land with the dim; every version of that was measured, and the
// motion was never the slow part - the frame it landed in was (a full-screen
// redraw for the dim, ~120 ms on Midnight, ~200 ms on Linen, after ~240 ms of
// growing). The window now appears complete in the first frame after the long
// press, and closes in one. What was learned is in docs/LESSONS.md, "Effects
// that cover the screen". Nothing is drawn behind it: the dim was tried that
// evening, drawn around the window a frame after it, and the owner chose
// speed - "the delay seems like an eternity and adds very little".
//
// What the eye gets instead is cheap because it is small: the held card is
// pressed in while its border fades to the accent (see "The hold"), now and
// then a spark runs along the window's edge (see "Interference"), and the
// switch's knob slides - and can be dragged.

// Per the repo's debug-flag convention (CLAUDE.md). What opening and closing
// cost on this side of the frame: building the window and tearing it down,
// and what the window's frame redrew. The frames themselves are
// -D DEBUG_FRAMES (GUIManager.cpp). With this flag, a long press on the
// window's title also cycles the interference: open + idle, open only, off.
#ifdef DEBUG_POPUP
    #define DBG_POPUP(...) Serial.printf("[Popup:debug] " __VA_ARGS__)
    #include "src/display/lv_display_private.h"   // inv_areas: what a frame redraws
    #include "src/core/lv_refr_private.h"         // lv_refr_get_top_obj()
    #include "HttpServer.h"                       // GET /popup
    #include <atomic>
    #include "freertos/FreeRTOS.h"
    #include "freertos/task.h"
#else
    #define DBG_POPUP(...) do {} while (0)
#endif

namespace {

#ifdef DEBUG_POPUP
char     s_dbgLine[120];
uint32_t s_dbgPressMs   = 0;   // when the long press fired
uint32_t s_dbgRenderT0  = 0;
uint32_t s_dbgFrameMs   = 0;   // how long the next frame took to draw
uint32_t s_dbgGlassMs   = 0;   // long press to that frame drawn
bool     s_dbgWantFrame = false;

// What that frame redraws: if the window's own frame costs more than the
// window, these say why (round 6: ~77 ms on Midnight where ~25 was expected).
constexpr uint8_t DBG_AREAS = 4;
lv_area_t s_dbgArea[DBG_AREAS];
uint8_t   s_dbgNArea = 0, s_dbgAllAreas = 0;

// The frame after a window appears or goes. -D DEBUG_FRAMES cannot see it: it
// only prints bursts of three frames or more, and closing is one.
void dbgRenderStart(lv_event_t *e) {
    if (!s_dbgWantFrame) return;
    s_dbgRenderT0 = millis();
    lv_display_t *d = (lv_display_t *)lv_event_get_current_target(e);
    s_dbgNArea = s_dbgAllAreas = 0;
    for (uint32_t i = 0; d && i < d->inv_p; i++) {
        if (d->inv_area_joined[i]) continue;
        s_dbgAllAreas++;
        if (s_dbgNArea < DBG_AREAS) s_dbgArea[s_dbgNArea++] = d->inv_areas[i];
    }
}
void dbgRenderReady(lv_event_t *e) {
    (void)e;
    if (!s_dbgWantFrame || !s_dbgRenderT0) return;
    s_dbgFrameMs   = millis() - s_dbgRenderT0;
    s_dbgGlassMs   = s_dbgPressMs ? millis() - s_dbgPressMs : 0;
    s_dbgWantFrame = false;
    s_dbgRenderT0  = 0;
}

void dbgProbe();   // below, where the window is known

void dbgPrint(lv_timer_t *t) {
    (void)t;
    if (s_dbgGlassMs) DBG_POPUP("%s; its frame drew in %lu ms; long press to glass %lu ms\n", s_dbgLine,
                                (unsigned long)s_dbgFrameMs, (unsigned long)s_dbgGlassMs);
    else              DBG_POPUP("%s; its frame drew in %lu ms\n", s_dbgLine, (unsigned long)s_dbgFrameMs);
    for (uint8_t i = 0; i < s_dbgNArea; i++) {
        const lv_area_t &a = s_dbgArea[i];
        DBG_POPUP("  redrew %ld,%ld..%ld,%ld (%ldx%ld)\n", (long)a.x1, (long)a.y1, (long)a.x2, (long)a.y2,
                  (long)lv_area_get_width(&a), (long)lv_area_get_height(&a));
    }
    if (s_dbgAllAreas > s_dbgNArea) DBG_POPUP("  ...%u areas in all\n", (unsigned)s_dbgAllAreas);
    dbgProbe();
    s_dbgPressMs = s_dbgGlassMs = 0;
}

// Printed 300 ms later, not now: a line written in the middle of the frames
// it describes blocks on the UART and becomes one of them (LESSONS, "A debug
// print is not free").
void dbgLater(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(s_dbgLine, sizeof(s_dbgLine), fmt, ap);
    va_end(ap);
    s_dbgWantFrame = true;
    s_dbgFrameMs   = 0;
    lv_timer_t *t = lv_timer_create(dbgPrint, 300, nullptr);
    lv_timer_set_repeat_count(t, 1);
}
#endif

// Compound names, never a bare ALL-CAPS word - Arduino's pin-mode macros eat
// those (CLAUDE.md, "Arduino's global macro namespace will eat your enum").
enum class PopupView  : uint8_t { VIEW_MAIN, VIEW_HISTORY, VIEW_MEMBERS };
enum class PopupPhase : uint8_t { PHASE_CLOSED, PHASE_OPEN };
// Where the held card is. OWNED: its window is open, and it stays pressed in,
// with the accent border, until the window closes.
enum class HoldPhase  : uint8_t { HOLD_NONE, HOLD_PRESSING, HOLD_LETTING_GO, HOLD_OWNED };
// How often a spark runs along the window's edge (see "Interference"): now
// and then while it is open; once per window; never.
enum class IntfMode   : uint8_t { INTF_NOW_AND_THEN, INTF_ONCE, INTF_OFF };

// D6: auto-close after 60 s untouched. The owner chose all four close routes.
constexpr uint32_t POPUP_AUTOCLOSE_MS = 60000;
constexpr uint32_t POPUP_TICK_MS      = 250;

// THE P4_5 IS THE REFERENCE (owner, 2026-10-05: its toggle "is the perfect
// size"). Its window is 787 x 545 px (68.1 x 47.1 mm) with the system header
// showing, and its hero 349 px of that height (30.2 mm, measured with calipers
// at 30). Every board's window takes this shape where the screen allows, and
// what is inside it the same share of it - see open(), propH and pm().
constexpr float POPUP_ASPECT  = 787.0f / 545.0f;
constexpr float REF_WIN_H_MM  = 47.1f;   // the P4_5 window's height, header showing
constexpr float HERO_H_MM     = 30.2f;   // its hero's

// The hold, in millimetres so it looks the same on every board. THE LEAP IS
// GONE (owner, round 6): it was clipped by the card's wrapper, and the window
// waiting two frames for it made the popup feel slower than it was - "the
// simple pressing down further visual effect of a long-press is by itself a
// really catchy visual".
constexpr float    HOLD_SINK_MM    = 0.4f;   // how far the card is pressed in at the long press
constexpr uint32_t HOLD_LETGO_MS   = 120;    // a tap lets go this fast, from full
constexpr uint32_t KNOB_SLIDE_MS   = 160;

IntfMode s_intfMode = IntfMode::INTF_NOW_AND_THEN;

struct Popup {
    PopupPhase phase = PopupPhase::PHASE_CLOSED;
    PopupView  view  = PopupView::VIEW_MAIN;
    bool       closeQueued = false;

    // Copied from the card at open - see "the window holds the entities".
    const Entity   *ent[CARD_PRIMARY_MAX] = {nullptr};
    uint8_t         nEnt = 0;
    EntityRegistry *reg  = nullptr;
    TempUnit        tempUnit = TempUnit::TEMP_INHERIT;
    char area[ENTITY_SHORT_MAX] = {0};
    char name[ENTITY_NAME_MAX]  = {0};

    lv_area_t winRect = {};
    int32_t   winRadius = 0;
    int32_t   pad = 0;
    int32_t   propH = 0;   // the height the contents are sized from - see open()

    lv_obj_t *win = nullptr;
    lv_obj_t *btnLeft = nullptr, *lblLeft = nullptr;
    lv_obj_t *btnHistory = nullptr, *btnMembers = nullptr;
    lv_obj_t *lblTitleArea = nullptr, *lblTitleName = nullptr;
    int32_t   titleW = 0;      // what the two title labels may use together
    lv_obj_t *stage = nullptr, *autoBar = nullptr;

    // The main view's widgets; null while another view is showing.
    lv_obj_t *hero = nullptr, *knob = nullptr, *heroIcon = nullptr;
    lv_obj_t *lblWhat = nullptr, *lblValue = nullptr, *lblUnit = nullptr, *lblAgo = nullptr;
    int32_t   heroW = 0, heroH = 0, heroR = 0, knobH = 0, knobInset = 0;
    bool      toggleHero = false;
    bool      knobPlaced = false;   // the first placement jumps; every later one slides
    int32_t   knobTarget = 0;
    // The knob under a finger (knobDragCb): where the drag began, where the
    // knob was then, and whether the CLICKED that ends a drag must be ignored.
    bool      knobDragging = false, swallowClick = false;
    int32_t   dragStartY = 0, dragKnobY0 = 0;

    // A light's controls (2.10b): see "The light's controls".
    GroupOn   groupOn = GroupOn::GROUP_ON_ANY;   // copied from the card at open
    bool      lightHero = false;
    uint8_t   lightCtl = 0;                      // LightCtl: which control the hero is
    lv_obj_t *fill = nullptr, *grip = nullptr, *mark = nullptr;
    lv_obj_t *btnCtl[4] = {nullptr, nullptr, nullptr, nullptr};   // power, dim, temp, colour
    lv_obj_t *swatch[8] = {nullptr};
    bool      sliding = false;                   // a finger is on the slider
    int32_t   slideVal = 0, sentVal = -1;        // what it shows; what was last sent
    uint32_t  sentMs = 0;

    // The settings deck (pathway 1, card-sheet 11.1): see "The settings deck".
    lv_obj_t *deck = nullptr, *deckTab = nullptr, *deckTabLbl = nullptr, *deckPane = nullptr;
    lv_obj_t *chipPause[2] = {nullptr, nullptr};   // Off, On
    lv_obj_t *chipGroup[2] = {nullptr, nullptr};   // Any, All
    int32_t   deckH = 0, deckHead = 0;
    int32_t   deckHide = 0;   // how much of the pane stays below the screen when open
    uint8_t   deckState = 0;                       // DeckState
    bool      deckFilled = false;                  // its rows exist (built at first open)

    lv_timer_t *timer = nullptr;
    uint32_t    lastTouchMs = 0;
    uint32_t    lastSig = 0;
    uint32_t    lastAgeMs = 0;
    lv_point_t  pressStart = {0, 0};
};
Popup s;
enum DeckState : uint8_t { DECK_HIDDEN, DECK_PEEK, DECK_OPEN };

// The card under the finger. `surface` is a card's own LVGL object, which a
// page rebuild can delete at any moment: a delete hook clears it the moment
// that happens (surfaceDeletedCb), and an animation whose var is the surface
// is deleted with it, by LVGL.
struct Hold {
    HoldPhase phase = HoldPhase::HOLD_NONE;
    lv_obj_t *surface = nullptr;
    int32_t   v = 0;          // 0..255: how far toward the accent
};
Hold h;
lv_indev_t *s_indev = nullptr;

// THE TAP CATCHER: "tap outside" and the modal rule, with nothing drawn. A
// transparent full-screen object on lv_layer_top(), made once at begin() and
// never deleted, hidden or moved - each of those would invalidate the whole
// screen (LESSONS, "Never toggle HIDDEN on a screen-sized object"). It is
// switched by CLICKABLE alone, which costs no redraw. Its hit test says "not
// here" inside the window, so LVGL goes on to search the screen, where the
// window is (pointer_search_obj() in lv_indev.c: system layer, top layer,
// then the screen).
lv_obj_t *s_catcher = nullptr;

// Millimetres on glass to this panel's pixels - the same derivation as
// UI::minTouch(), so the window is the same physical size on every board.
int32_t mm(float v) {
    return (int32_t)lroundf(v * (float)UIToolkit::ppi() / 25.4f);
}

// "P4_5 millimetres": a length that is `v` mm in the P4_5's window, the same
// SHARE of this one (owner, 2026-10-05: the hero the same proportion of the
// window on every board). For the hero and what is laid out around it; touch
// targets and text stay real millimetres, so they are the same size under a
// finger everywhere.
int32_t pm(float v) {
    return (int32_t)lroundf(v * (float)s.propH / REF_WIN_H_MM);
}

// A bare object: no theme styles, no scrolling, not clickable until asked.
lv_obj_t *plain(lv_obj_t *parent) {
    lv_obj_t *o = lv_obj_create(parent);
    lv_obj_remove_style_all(o);
    lv_obj_clear_flag(o, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(o, LV_OBJ_FLAG_CLICKABLE);
    return o;
}

lv_obj_t *makeLabel(lv_obj_t *parent, const lv_font_t *f, uint32_t hex) {
    lv_obj_t *l = lv_label_create(parent);
    lv_obj_set_style_text_font (l, f, 0);
    lv_obj_set_style_text_color(l, UI::c(hex), 0);
    lv_label_set_text(l, "");
    return l;
}

// Only touch LVGL when the words actually change: a re-render runs every
// second for the "N ago" line, and setting identical text still invalidates.
void setText(lv_obj_t *l, const char *t) {
    if (!l) return;
    const char *cur = lv_label_get_text(l);
    if (cur && strcmp(cur, t) == 0) return;
    lv_label_set_text(l, t);
}

// How wide a line of text draws, in pixels.
int32_t textW(const char *txt, const lv_font_t *f) {
    lv_point_t sz = {0, 0};
    lv_text_get_size(&sz, txt, f, 0, 0, LV_COORD_MAX, LV_TEXT_FLAG_NONE);
    return sz.x;
}

// A round icon button at the minimum touch size: a visible disc in the card
// surface colour, so it reads as a button and not a stray glyph (owner, H1),
// a shade lighter while pressed.
lv_obj_t *iconButton(lv_obj_t *parent, const char *glyph, const lv_font_t *f,
                     lv_event_cb_t cb, lv_obj_t **outLabel = nullptr) {
    const UIPalette &p = UI::pal();
    const int32_t sz = UI::minTouch();
    lv_obj_t *b = plain(parent);
    lv_obj_set_size  (b, sz, sz);
    lv_obj_add_flag  (b, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_radius(b, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(b, UI::c(p.SURFACE), 0);
    lv_obj_set_style_bg_opa  (b, LV_OPA_COVER,     0);
    lv_obj_set_style_bg_color(b, UI::c(UI::mix(p.SURFACE, p.TEXT, 20)),
                              UI::part(LV_PART_MAIN, LV_STATE_PRESSED));
    lv_obj_t *l = makeLabel(b, f, p.TEXT);
    lv_label_set_text(l, glyph);
    lv_obj_center(l);
    lv_obj_add_event_cb(b, cb, LV_EVENT_CLICKED, nullptr);
    if (outLabel) *outLabel = l;
    return b;
}

// ---------------------------------------------------------------------------
// What the entities say, aggregated the way StateCard aggregates them: a card
// with several primaries is "on" when any of them is.
// ---------------------------------------------------------------------------
struct Agg {
    bool on = false, anyBool = false, available = true, paused = false, everSet = false;
    uint32_t lastChangeMs = 0;
};

// ON BY THE CARD'S GROUP RULE (2.10b): any member, or all of them - GroupOn,
// what an HA group helper offers. One entity is the same rule with one member.
Agg aggregate() {
    Agg a;
    uint8_t nBool = 0, nOn = 0;
    for (uint8_t i = 0; i < s.nEnt; i++) {
        const Entity *e = s.ent[i];
        if (!e) continue;
        if (e->value.type == ValueType::BOOL) { a.anyBool = true; nBool++; if (e->value.b) nOn++; }
        if (!e->available) a.available = false;
        if (e->paused)     a.paused = true;
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
struct LightAgg {
    uint8_t   caps = 0;                       // LightCapBits, the union
    int32_t   bri = -1, kelvin = -1, hue = -1, sat = -1;
    LightMode mode = LightMode::LMODE_UNKNOWN;
    int32_t   minK = 0, maxK = 0;
};

LightAgg lightAggregate() {
    LightAgg L;
    int32_t briSum = 0, briN = 0, kSum = 0, kN = 0, satSum = 0, hN = 0;
    float hx = 0.f, hy = 0.f;
    for (uint8_t i = 0; i < s.nEnt; i++) {
        const Entity *e = s.ent[i];
        if (!e) continue;
        const EntityAttrs &at = e->attrs;
        L.caps |= at.lightCaps;
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

// Changes whenever anything the window shows could have changed. An FNV-1a
// over the fields, so a re-render costs nothing when nothing moved.
uint32_t signature() {
    uint32_t h = 2166136261u;
    auto mixIn = [&h](uint32_t v) { h = (h ^ v) * 16777619u; };
    for (uint8_t i = 0; i < s.nEnt; i++) {
        const Entity *e = s.ent[i];
        if (!e) continue;
        mixIn(e->lastChangeMs);
        mixIn(e->lastUpdateMs);
        mixIn((e->available ? 1u : 0u) | (e->paused ? 2u : 0u) | (e->pending ? 4u : 0u) |
              (e->cmdFailed ? 8u : 0u) | (e->everSet ? 16u : 0u) | (e->attrPending ? 32u : 0u));
        if (e->value.type == ValueType::BOOL) mixIn(e->value.b ? 1u : 0u);
        // A light's levels move without its value changing (2.10b).
        const EntityAttrs &at = e->attrs;
        mixIn((uint32_t)(uint16_t)at.brightness | ((uint32_t)at.lightCaps << 16) | ((uint32_t)at.lightMode << 24));
        mixIn((uint32_t)(uint16_t)at.colorTempK | ((uint32_t)(uint16_t)at.hue << 16));
        mixIn((uint32_t)(uint8_t)at.sat);
    }
    mixIn((uint32_t)s.groupOn);
    return h;
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

// The main view's widgets, before its stage is cleaned or the window goes.
void forgetMainWidgets() {
    s.hero = s.knob = s.heroIcon = nullptr;
    s.lblWhat = s.lblValue = s.lblUnit = s.lblAgo = nullptr;
    s.fill = s.grip = s.mark = nullptr;
    for (lv_obj_t *&b : s.btnCtl) b = nullptr;
    for (lv_obj_t *&w : s.swatch) w = nullptr;
    s.sliding = false;
}

void forgetWidgets() {
    s.btnLeft = s.lblLeft = s.btnHistory = s.btnMembers = nullptr;
    s.lblTitleArea = s.lblTitleName = nullptr;
    s.stage = s.autoBar = nullptr;
    forgetMainWidgets();
}

void knobExec(void *var, int32_t v) { lv_obj_set_y((lv_obj_t *)var, v); }

// The switch's knob SLIDES to its new end (owner, 2026-10-04: "absolutely" on
// the control giving feedback), except when the view is first built, where it
// is simply placed. The knob is inside the window, which covers the page, so
// each frame redraws only the toggle. The animation's var is the knob, so
// LVGL deletes it with the window.
void placeKnob(int32_t y) {
    if (!s.knob) return;
    if (!s.knobPlaced) {
        lv_obj_set_y(s.knob, y);
        s.knobPlaced = true;
        s.knobTarget = y;
        return;
    }
    if (y == s.knobTarget) return;
    s.knobTarget = y;
    lv_anim_delete(s.knob, knobExec);
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var     (&a, s.knob);
    lv_anim_set_values  (&a, lv_obj_get_y(s.knob), y);
    lv_anim_set_duration(&a, KNOB_SLIDE_MS);
    lv_anim_set_path_cb (&a, lv_anim_path_ease_in_out);
    lv_anim_set_exec_cb (&a, knobExec);
    lv_anim_start(&a);
}

// "Changed 3m ago", "Paused", "No reading yet" - under the value, every view.
void renderAgo(const Agg &a) {
    if (!s.lblAgo) return;
    char buf[48];
    if (a.paused) {
        setText(s.lblAgo, "Paused");
    } else if (!a.everSet) {
        setText(s.lblAgo, "No reading yet");
    } else {
        char age[16];
        cardFormatAge(millis() - a.lastChangeMs, age, sizeof(age));
        if (strcmp(age, "now") == 0) snprintf(buf, sizeof(buf), "Changed just now");
        else                         snprintf(buf, sizeof(buf), "Changed %s ago", age);
        setText(s.lblAgo, buf);
    }
}

void showView(PopupView v);   // below

// ---------------------------------------------------------------------------
// The light's controls (2.10b; card-sheet 11.1, Card Popup Mock v3)
//
// The hero is a tall slider for whichever control is chosen below it:
// brightness (filled from the bottom, in the light's own colour), colour
// temperature (warm at the bottom, cool at the top) or colour (a hue strip).
// Beside it: what it controls, the value, when it changed, and the selector -
// Power | Brightness, Temperature, Colour - showing only what the light (or,
// for a group, ANY member) can do. Colour shows eight swatches in place of the
// value line: a hue in degrees says nothing a swatch does not.
//
// A TAP JUMPS THERE, A DRAG FOLLOWS THE FINGER (owner, 2026-10-05). While a
// finger is on it, the slider and the value follow the finger every frame,
// and a command goes out at most every LIGHT_SEND_MS - "enough so that a user
// could adjust brightness and see the level change" - plus once on release,
// with where it ended. Power is the selector's first button.
//
// A GROUP ACTS AS HA'S LIGHT GROUP (owner, 2026-10-05): each member is sent
// what it can take. A brightness to a member that only switches turns it on;
// a temperature to a member with colour but no temperature becomes the
// nearest hue (HA's conversion); a colour to a member without one turns it
// on. Levels shown are the mean over the members that are on.
//
// ONLY WHAT IS SHOWING IS BUILT: choosing another control rebuilds the view,
// so the swatches exist only in Colour (owner: memory first).
//
// Cheap to move because it is small: a drag redraws the slider and the value
// - tens of thousands of pixels - never the window.
// ---------------------------------------------------------------------------
enum LightCtl : uint8_t { LCTL_DIM, LCTL_TEMP, LCTL_COLOUR };

constexpr uint32_t LIGHT_SEND_MS = 300;   // owner: "300ms sounds about right"

// Eight defaults, like HA's (card-sheet 11.2). Saving the current colour into
// one with a long press needs the settings store (2.10d).
struct Swatch { int16_t hue; int8_t sat; };
constexpr Swatch SWATCHES[8] = {
    {   0, 100 }, {  30, 100 }, {  55, 100 }, { 120,  90 },
    { 180,  90 }, { 225, 100 }, { 275,  90 }, { 320,  85 },
};

bool canSlide(uint8_t caps) { return caps & (LIGHT_CAN_DIM | LIGHT_CAN_TEMP | LIGHT_CAN_COLOUR); }

// D3: open on the control - brightness first, as HA's dialog does.
uint8_t firstCtl(uint8_t caps) {
    if (caps & LIGHT_CAN_DIM)  return LCTL_DIM;
    if (caps & LIGHT_CAN_TEMP) return LCTL_TEMP;
    return LCTL_COLOUR;
}

// The slider's range for the control showing.
void ctlRange(const LightAgg &L, int32_t &lo, int32_t &hi) {
    switch (s.lightCtl) {
        case LCTL_DIM:  lo = 1;      hi = 100;    break;
        case LCTL_TEMP: lo = L.minK; hi = L.maxK; break;
        default:        lo = 0;      hi = 359;    break;
    }
}

// Where `v` sits on the slider: 0 at the bottom, 1000 at the top.
int32_t permilleOf(const LightAgg &L, int32_t v) {
    int32_t lo, hi;
    ctlRange(L, lo, hi);
    return LV_CLAMP(0, (v - lo) * 1000 / LV_MAX(1, hi - lo), 1000);
}

int32_t stripInset();   // below, with the strip

// The value under the finger.
int32_t valueAtFinger(const LightAgg &L) {
    lv_indev_t *indev = lv_indev_active();
    lv_point_t pt = {0, 0};
    if (indev) lv_indev_get_point(indev, &pt);
    lv_area_t a;
    lv_obj_get_coords(s.hero, &a);
    const int32_t in = stripInset();
    const int32_t h  = lv_area_get_height(&a) - 2 * in;
    const int32_t permille = LV_CLAMP(0, 1000 - (pt.y - a.y1 - in) * 1000 / LV_MAX(1, h - 1), 1000);
    int32_t lo, hi;
    ctlRange(L, lo, hi);
    int32_t v = lo + (int32_t)((int64_t)(hi - lo) * permille / 1000);
    if (s.lightCtl == LCTL_TEMP) v = (v + 5) / 10 * 10;   // 10 K: finer than any eye
    return v;
}

// Send `v` for the control showing to every member, each what it can take.
// `sat` for a swatch; the strip itself is drawn - and sent - at full colour.
void lightSend(int32_t v, int8_t sat = -1) {
    if (!s.reg) return;
    const uint32_t now = millis();
    for (uint8_t i = 0; i < s.nEnt; i++) {
        const Entity *e = s.ent[i];
        if (!e || !e->desc.writable) continue;
        const EntityAttrs &at = e->attrs;
        LightCommand c;
        if (s.lightCtl == LCTL_DIM) {
            if (at.lightCaps & LIGHT_CAN_DIM) c.brightness = (int16_t)LV_MAX(1, (v * 255 + 50) / 100);
        } else if (s.lightCtl == LCTL_TEMP) {
            if (at.lightCaps & LIGHT_CAN_TEMP) {
                int32_t k = v;
                if (at.minTempK) k = LV_MAX(k, (int32_t)at.minTempK);
                if (at.maxTempK) k = LV_MIN(k, (int32_t)at.maxTempK);
                c.colorTempK = (int16_t)k;
            } else if (at.lightCaps & LIGHT_CAN_COLOUR) {
                lightKelvinToHs(v, c.hue, c.sat);
            }
        } else if (at.lightCaps & LIGHT_CAN_COLOUR) {
            c.hue = (int16_t)v;
            c.sat = sat >= 0 ? sat : 100;
        }
        if (c.brightness > 0 || c.colorTempK > 0 || c.hue >= 0) {
            s.reg->commandLight(e->desc.id, c, now);
        } else if (!(e->value.type == ValueType::BOOL && e->value.b)) {
            // What it cannot take, it ignores - and is turned on (HA).
            s.reg->commandValue(e->desc.id, EntityValue::makeBool(true), now);
        }
    }
    s.sentVal = v;
    s.sentMs  = now;
}

// Rough luminance, to keep the fill visible against its track.
int32_t lumOf(uint32_t hex) {
    return (int32_t)(((hex >> 16) & 0xFF) * 299 + ((hex >> 8) & 0xFF) * 587 + (hex & 0xFF) * 114) / 1000;
}

void renderLight() {
    if (!s.hero) return;
    const UIPalette &p = UI::pal();
    const Agg      a = aggregate();
    const LightAgg L = lightAggregate();
    const bool on = a.available && a.on;
    char buf[32];

    setText(s.lblWhat, s.lightCtl == LCTL_DIM ? "Brightness" : s.lightCtl == LCTL_TEMP ? "Temperature" : "Colour");

    // What the slider shows: the finger while it is down, otherwise the
    // light. -1: nothing to mark (off, or the light has not said).
    int32_t v;
    if (s.sliding)                    v = s.slideVal;
    else if (!on)                     v = -1;
    else if (s.lightCtl == LCTL_DIM)  v = L.bri >= 0 ? LV_MAX(1, (L.bri * 100 + 127) / 255) : 100;
    else if (s.lightCtl == LCTL_TEMP) v = L.kelvin;
    else                              v = L.hue;

    if (s.lblValue) {
        if (!a.available) {
            setText(s.lblValue, "Unavailable");
            lv_obj_set_style_text_color(s.lblValue, UI::c(p.ST_BAD), 0);
        } else {
            // On but no temperature: it is showing a colour, and a light in a
            // colour reports no kelvin (HA the same) - so no ring, and the
            // words say why (owner, round 1, L7).
            if (v < 0)                       setText(s.lblValue, !on ? "Off"
                                                     : L.mode == LightMode::LMODE_COLOUR ? "A colour" : "--");
            else if (s.lightCtl == LCTL_DIM) { snprintf(buf, sizeof(buf), "%ld%%", (long)v); setText(s.lblValue, buf); }
            else                             { snprintf(buf, sizeof(buf), "%ld K", (long)v); setText(s.lblValue, buf); }
            lv_obj_set_style_text_color(s.lblValue, UI::c(p.TEXT), 0);
        }
    }
    renderAgo(a);

    // --- The slider --------------------------------------------------------
    if (s.lightCtl == LCTL_DIM && s.fill) {
        const uint32_t track = UI::mix(p.SURFACE_ALT, p.TEXT, 10);
        lv_obj_set_style_bg_color(s.hero, UI::c(track), 0);
        // In the light's own colour when it has one that shows against the
        // track; the scheme's "on" colour otherwise.
        EntityAttrs shown;
        shown.lightMode = L.mode; shown.colorTempK = (int16_t)L.kelvin;
        shown.hue = (int16_t)L.hue; shown.sat = (int8_t)L.sat;
        uint32_t fillHex = lightShownRgb(shown);
        if (!fillHex || LV_ABS(lumOf(fillHex) - lumOf(track)) < 60) fillHex = p.ST_ACTIVE;
        // Never shorter than its own rounded end: at 1% a 3 px fill was a flat
        // line wider than the slider's rounded bottom. The value says 1%.
        const int32_t fh = v > 0 ? LV_MAX(s.heroH * v / 100, 2 * s.heroR) : 0;
        lv_obj_set_style_bg_color(s.fill, UI::c(fillHex), 0);
        lv_obj_set_y     (s.fill, s.heroH - fh);
        lv_obj_set_height(s.fill, fh);
        // The grip: a short bar near the top of the fill, where a finger takes it.
        // Small, so HIDDEN costs only its own few pixels (the HIDDEN lesson is
        // about screen-sized objects). Its x from the width it is GIVEN: read
        // back with lv_obj_get_width() before LVGL had laid it out, it was 0
        // on the first draw, and the grip sat half a slider to the right until
        // the next redraw a second later (owner, round 1).
        if (s.grip) {
            const int32_t gw = s.heroW * 34 / 100;
            lv_obj_set_style_bg_color(s.grip, UI::c(UI::contrastOf(fillHex, p.GROUND, p.TEXT)), 0);
            lv_obj_set_pos  (s.grip, (s.heroW - gw) / 2, s.heroH - fh + pm(1.4f));
            lv_obj_set_width(s.grip, gw);
            if (fh >= pm(5)) lv_obj_clear_flag(s.grip, LV_OBJ_FLAG_HIDDEN);
            else             lv_obj_add_flag  (s.grip, LV_OBJ_FLAG_HIDDEN);
        }
    } else if (s.mark) {
        // HIDDEN, not 0 wide: a 0-wide ring still drew its outline, the
        // stray vertical line beside the strip (owner, round 1, L7/L9).
        const int32_t mh = lv_obj_get_height(s.mark);
        if (v < 0) {
            lv_obj_add_flag(s.mark, LV_OBJ_FLAG_HIDDEN);
        } else {
            const int32_t in = stripInset();
            const int32_t y  = in + (1000 - permilleOf(L, v)) * (s.heroH - 2 * in) / 1000 - mh / 2;
            lv_obj_set_pos(s.mark, s.heroW * 10 / 100, LV_CLAMP(0, y, s.heroH - mh));
            lv_obj_clear_flag(s.mark, LV_OBJ_FLAG_HIDDEN);
        }
    }

    // --- The selector ------------------------------------------------------
    for (uint8_t k = 0; k < 4; k++) {
        lv_obj_t *b = s.btnCtl[k];
        if (!b) continue;
        const bool chosen = (k > 0 && k - 1 == s.lightCtl);
        lv_obj_set_style_bg_opa  (b, chosen ? LV_OPA_COVER : LV_OPA_TRANSP, 0);
        lv_obj_set_style_bg_color(b, UI::c(p.TEXT), 0);
        if (lv_obj_t *l = lv_obj_get_child(b, 0))
            lv_obj_set_style_text_color(l, UI::c(chosen ? p.SURFACE_ALT : (k == 0 && on) ? p.ST_ACTIVE : p.TEXT), 0);
    }

    // --- The swatches: a ring round the one the light is showing -----------
    for (uint8_t i = 0; i < 8; i++) {
        if (!s.swatch[i]) continue;
        int32_t dh = LV_ABS((int32_t)SWATCHES[i].hue - L.hue) % 360;
        if (dh > 180) dh = 360 - dh;
        const bool here = on && L.mode == LightMode::LMODE_COLOUR && L.hue >= 0 && dh <= 3 &&
                          LV_ABS((int32_t)SWATCHES[i].sat - L.sat) <= 3;
        lv_obj_set_style_outline_width(s.swatch[i], here ? LV_MAX(2, mm(0.4f)) : 0, 0);
        lv_obj_set_style_outline_color(s.swatch[i], UI::c(p.TEXT), 0);
    }
}

// The finger on the slider. See the section comment for the rules.
void lightSlideCb(lv_event_t *ev) {
    if (!s.hero) return;
    const lv_event_code_t code = lv_event_get_code(ev);
    const LightAgg L = lightAggregate();
    const uint32_t now = millis();
    if (code == LV_EVENT_PRESSED) {
        s.sliding  = true;
        s.slideVal = valueAtFinger(L);
        lightSend(s.slideVal);          // a tap jumps there
        renderLight();
    } else if (code == LV_EVENT_PRESSING) {
        const int32_t v = valueAtFinger(L);
        if (v == s.slideVal) return;
        s.slideVal = v;
        if (v != s.sentVal && now - s.sentMs >= LIGHT_SEND_MS) lightSend(v);
        renderLight();
    } else if (code == LV_EVENT_RELEASED || code == LV_EVENT_PRESS_LOST) {
        if (!s.sliding) return;
        if (s.slideVal != s.sentVal) lightSend(s.slideVal);   // where it ended
        s.sliding = false;
        renderLight();
    }
}

void commandAll(bool on);   // below: what Power does

void rebuildMainAsync(void *unused) {
    (void)unused;
    if (s.phase == PopupPhase::PHASE_OPEN && s.stage && s.view == PopupView::VIEW_MAIN)
        showView(PopupView::VIEW_MAIN);
}

void ctlCb(lv_event_t *ev) {
    const uint8_t k = (uint8_t)(uintptr_t)lv_event_get_user_data(ev);
    if (k == 0) { commandAll(!aggregate().on); return; }
    if (k - 1 == s.lightCtl) return;
    s.lightCtl = k - 1;
    // Only what is showing is built, so the view is rebuilt - deferred: this
    // button is in the stage being cleaned, and an event callback must not
    // delete its own object.
    lv_async_call(rebuildMainAsync, nullptr);
}

void swatchCb(lv_event_t *ev) {
    const uint8_t i = (uint8_t)(uintptr_t)lv_event_get_user_data(ev);
    if (i >= 8) return;
    lightSend(SWATCHES[i].hue, SWATCHES[i].sat);
    renderLight();
}

// The strip's colours run between the rounded ends: a radius in from each.
// The brightness fill uses the whole height.
int32_t stripInset() { return s.lightCtl == LCTL_DIM ? 0 : s.heroR; }

// A strip of `n` colours, bottom to top. LVGL is built with two gradient
// stops (LV_GRADIENT_MAX_STOPS), so it is n-1 square two-stop pieces over the
// straight middle of the slider; the slider itself draws the rounded ends, in
// the end colours, flat. No clip_corner, which would cost a layer (LESSONS).
// (Round 1 of this had the end pieces carry the rounding and reach into their
// neighbours; at a radius 3/4 of a piece tall that hid most of the next hue.)
void stripPieces(const uint32_t *cols, int n) {
    const int32_t in = stripInset();
    const int32_t span = s.heroH - 2 * in;
    lv_obj_set_style_bg_opa       (s.hero, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color     (s.hero, UI::c(cols[n - 1]), 0);   // top
    lv_obj_set_style_bg_grad_color(s.hero, UI::c(cols[0]), 0);       // bottom
    lv_obj_set_style_bg_grad_dir  (s.hero, LV_GRAD_DIR_VER, 0);
    lv_obj_set_style_bg_main_stop (s.hero, (uint8_t)(255 * in / s.heroH), 0);
    lv_obj_set_style_bg_grad_stop (s.hero, (uint8_t)(255 * (s.heroH - in) / s.heroH), 0);
    const int p = n - 1;
    for (int i = 0; i < p; i++) {
        const int32_t yTop = in + span - (int32_t)((int64_t)span * (i + 1) / p);
        const int32_t yBot = in + span - (int32_t)((int64_t)span * i / p);
        lv_obj_t *o = plain(s.hero);
        lv_obj_set_pos (o, 0, yTop);
        lv_obj_set_size(o, s.heroW, yBot - yTop);
        lv_obj_set_style_bg_opa       (o, LV_OPA_COVER, 0);
        lv_obj_set_style_bg_color     (o, UI::c(cols[i + 1]), 0);   // top
        lv_obj_set_style_bg_grad_color(o, UI::c(cols[i]), 0);       // bottom
        lv_obj_set_style_bg_grad_dir  (o, LV_GRAD_DIR_VER, 0);
    }
}

void buildLightHero(lv_obj_t *row) {
    const LightAgg L = lightAggregate();
    s.heroW = s.heroH * 42 / 100;   // the toggle's proportions
    const int32_t r = s.heroW * 30 / 100;
    s.heroR = r;
    s.hero = plain(row);
    lv_obj_set_size(s.hero, s.heroW, s.heroH);
    lv_obj_set_style_radius(s.hero, r, 0);
    lv_obj_add_flag(s.hero, LV_OBJ_FLAG_CLICKABLE);
    // A drag on the slider is the slider's: no gesture walks up from it to the
    // window or the screen, and no scroll is handed to a parent (LESSONS,
    // "Events do not reach the screen", and #68's scrolling layer).
    lv_obj_clear_flag(s.hero, LV_OBJ_FLAG_GESTURE_BUBBLE);
    lv_obj_clear_flag(s.hero, LV_OBJ_FLAG_SCROLL_CHAIN);
    lv_obj_add_event_cb(s.hero, lightSlideCb, LV_EVENT_PRESSED,    nullptr);
    lv_obj_add_event_cb(s.hero, lightSlideCb, LV_EVENT_PRESSING,   nullptr);
    lv_obj_add_event_cb(s.hero, lightSlideCb, LV_EVENT_RELEASED,   nullptr);
    lv_obj_add_event_cb(s.hero, lightSlideCb, LV_EVENT_PRESS_LOST, nullptr);

    if (s.lightCtl == LCTL_DIM) {
        lv_obj_set_style_bg_opa(s.hero, LV_OPA_COVER, 0);
        s.fill = plain(s.hero);
        lv_obj_set_size(s.fill, s.heroW, 0);
        lv_obj_set_style_radius(s.fill, r, 0);
        lv_obj_set_style_bg_opa(s.fill, LV_OPA_COVER, 0);
        s.grip = plain(s.hero);
        lv_obj_set_size(s.grip, s.heroW * 34 / 100, LV_MAX(3, pm(0.5f)));
        lv_obj_add_flag(s.grip, LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_style_radius(s.grip, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_bg_opa(s.grip, LV_OPA_COVER, 0);
        return;
    }
    if (s.lightCtl == LCTL_TEMP) {
        // The light's own range, warm at the bottom (HA's dialog the same way).
        const uint32_t cols[3] = { lightKelvinToRgb(L.minK), lightKelvinToRgb((L.minK + L.maxK) / 2),
                                   lightKelvinToRgb(L.maxK) };
        stripPieces(cols, 3);
    } else {
        uint32_t cols[7];
        for (int i = 0; i < 7; i++) cols[i] = lightHsToRgb(i * 60, 100);
        stripPieces(cols, 7);
    }
    // The marker: a white ring with a dark edge, which reads on every colour.
    s.mark = plain(s.hero);
    lv_obj_set_size(s.mark, s.heroW * 80 / 100, LV_MAX(8, pm(2.6f)));
    lv_obj_add_flag(s.mark, LV_OBJ_FLAG_HIDDEN);   // until renderLight() places it
    lv_obj_set_style_radius      (s.mark, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_border_width(s.mark, LV_MAX(2, pm(0.35f)), 0);
    lv_obj_set_style_border_color(s.mark, UI::c(0xFFFFFF), 0);
    lv_obj_set_style_outline_width(s.mark, 1, 0);
    lv_obj_set_style_outline_color(s.mark, UI::c(0x000000), 0);
    lv_obj_set_style_outline_opa (s.mark, LV_OPA_50, 0);
}

lv_obj_t *ctlButton(lv_obj_t *parent, const char *glyph, uint8_t k) {
    const int32_t sz = UI::minTouch();
    lv_obj_t *b = plain(parent);
    lv_obj_set_size(b, sz, sz);
    lv_obj_add_flag(b, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_radius(b, LV_RADIUS_CIRCLE, 0);
    lv_obj_t *l = makeLabel(b, UI::type().ICON_MD, UI::pal().TEXT);
    lv_label_set_text(l, glyph);
    lv_obj_center(l);
    lv_obj_add_event_cb(b, ctlCb, LV_EVENT_CLICKED, (void *)(uintptr_t)k);
    s.btnCtl[k] = b;
    return b;
}

void buildLightColumn(lv_obj_t *col) {
    const UIPalette &p = UI::pal();
    const UIType    &t = UI::type();
    const LightAgg   L = lightAggregate();
    s.lblWhat = makeLabel(col, t.TAG, p.TEXT_DIM);
    if (s.lightCtl != LCTL_COLOUR) {
        s.lblValue = makeLabel(col, UIToolkit::Font_Hero, p.TEXT);
        s.lblAgo   = makeLabel(col, t.TAG, p.TEXT_DIM);
    } else {
        const int32_t sz = mm(7.0f), gap = mm(1.6f);
        lv_obj_t *grid = plain(col);
        lv_obj_set_size     (grid, 4 * sz + 3 * gap + 2 * mm(0.6f), LV_SIZE_CONTENT);
        lv_obj_set_flex_flow(grid, LV_FLEX_FLOW_ROW_WRAP);
        lv_obj_set_style_pad_all   (grid, mm(0.6f), 0);   // room for the ring
        lv_obj_set_style_pad_column(grid, gap, 0);
        lv_obj_set_style_pad_row   (grid, gap, 0);
        for (uint8_t i = 0; i < 8; i++) {
            lv_obj_t *w = plain(grid);
            lv_obj_set_size(w, sz, sz);
            lv_obj_set_style_radius      (w, LV_RADIUS_CIRCLE, 0);
            lv_obj_set_style_bg_opa      (w, LV_OPA_COVER, 0);
            lv_obj_set_style_bg_color    (w, UI::c(lightHsToRgb(SWATCHES[i].hue, SWATCHES[i].sat)), 0);
            lv_obj_set_style_border_width(w, LV_MAX(1, mm(0.2f)), 0);
            lv_obj_set_style_border_color(w, UI::border(), 0);
            lv_obj_set_style_outline_pad (w, LV_MAX(2, mm(0.3f)), 0);
            lv_obj_add_flag(w, LV_OBJ_FLAG_CLICKABLE);
            lv_obj_add_event_cb(w, swatchCb, LV_EVENT_CLICKED, (void *)(uintptr_t)i);
            s.swatch[i] = w;
        }
    }

    // Power | the controls this light (any member) has, with a divider after
    // Power as in HA. A light with only one of them still shows it, so the
    // selector always says what the slider is.
    lv_obj_t *sel = plain(col);
    lv_obj_set_size     (sel, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(sel, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(sel, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_radius    (sel, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color  (sel, UI::c(p.SURFACE), 0);
    lv_obj_set_style_bg_opa    (sel, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_all   (sel, mm(0.6f), 0);
    lv_obj_set_style_pad_column(sel, mm(0.6f), 0);
    ctlButton(sel, MDI_POWER, 0);
    lv_obj_t *div = plain(sel);
    lv_obj_set_size(div, LV_MAX(2, mm(0.25f)), UI::minTouch() * 60 / 100);
    lv_obj_set_style_bg_color(div, UI::border(), 0);
    lv_obj_set_style_bg_opa  (div, LV_OPA_COVER, 0);
    if (L.caps & LIGHT_CAN_DIM)    ctlButton(sel, MDI_BRIGHTNESS_5, 1 + LCTL_DIM);
    if (L.caps & LIGHT_CAN_TEMP)   ctlButton(sel, MDI_SUN_THERMOMETER, 1 + LCTL_TEMP);
    if (L.caps & LIGHT_CAN_COLOUR) ctlButton(sel, MDI_PALETTE, 1 + LCTL_COLOUR);
}

// ---------------------------------------------------------------------------
// The main view: the hero beside what it shows (card-sheet 11.1)
// ---------------------------------------------------------------------------
void renderMain() {
    if (s.lightHero) { renderLight(); return; }
    if (!s.lblValue || !s.nEnt || !s.ent[0]) return;
    const UIPalette &p = UI::pal();
    const UIType    &t = UI::type();
    const Entity    &e = *s.ent[0];
    const Agg a = aggregate();
    char buf[48], wbuf[32];

    // --- What it is --------------------------------------------------------
    setText(s.lblWhat, s.toggleHero ? "Power" : (a.anyBool ? "State" : whatWord(e, wbuf, sizeof(wbuf))));

    // --- The value ---------------------------------------------------------
    // Words in the hero face (a built-in Montserrat the toolkit already links);
    // numbers in VALUE, the card's own digits face, with the unit beside it.
    const char *unit = "";
    if (!a.available) {
        setText(s.lblValue, "Unavailable");
        lv_obj_set_style_text_font (s.lblValue, UIToolkit::Font_Hero, 0);
        lv_obj_set_style_text_color(s.lblValue, UI::c(p.ST_BAD), 0);
    } else if (a.anyBool) {
        setText(s.lblValue, cardStateWord(e.desc, a.on));
        lv_obj_set_style_text_font (s.lblValue, UIToolkit::Font_Hero, 0);
        lv_obj_set_style_text_color(s.lblValue, UI::c(p.TEXT), 0);
    } else if (!e.everSet) {
        setText(s.lblValue, "--");
        lv_obj_set_style_text_font (s.lblValue, UIToolkit::Font_Hero, 0);
        lv_obj_set_style_text_color(s.lblValue, UI::c(p.TEXT_DIM), 0);
    } else {
        cardFormatValue(e, buf, sizeof(buf), false, s.tempUnit);
        setText(s.lblValue, buf);
        const bool numeric = (e.value.type == ValueType::INT || e.value.type == ValueType::FLOAT);
        lv_obj_set_style_text_font (s.lblValue, numeric ? t.VALUE : UIToolkit::Font_Hero, 0);
        lv_obj_set_style_text_color(s.lblValue, UI::c(p.TEXT), 0);
        if (numeric) unit = cardDisplayUnit(e, s.tempUnit);
    }
    setText(s.lblUnit, unit);
    if (unit[0]) lv_obj_clear_flag(s.lblUnit, LV_OBJ_FLAG_HIDDEN);
    else         lv_obj_add_flag  (s.lblUnit, LV_OBJ_FLAG_HIDDEN);

    // --- When --------------------------------------------------------------
    renderAgo(a);

    // --- The hero ----------------------------------------------------------
    const bool lit = a.available && a.anyBool && a.on;
    setText(s.heroIcon, cardHeroIcon(e, a.on));
    if (s.toggleHero) {
        // HA's switch dialog, stood upright: a tall track, the knob at the top
        // when on and the bottom when off. Never colour alone (the owner is a
        // little colour-blind): the knob's position says it too.
        lv_obj_set_style_bg_color(s.hero, UI::c(lit ? UI::mix(p.ST_ACTIVE, p.SURFACE_ALT, 70)
                                                    : UI::mix(p.SURFACE_ALT, p.TEXT, 10)), 0);
        lv_obj_set_style_bg_color(s.knob, UI::c(lit ? p.ST_ACTIVE
                                                    : UI::mix(p.SURFACE_ALT, p.TEXT, 25)), 0);
        placeKnob(lit ? s.knobInset : s.heroH - s.knobH - s.knobInset);
        lv_obj_set_style_text_color(s.heroIcon,
            UI::c(lit ? UI::contrastOf(p.ST_ACTIVE, p.GROUND, p.TEXT) : p.TEXT), 0);
    } else {
        lv_obj_set_style_bg_color(s.hero, UI::c(lit ? p.ST_ACTIVE : p.SURFACE), 0);
        lv_obj_set_style_text_color(s.heroIcon,
            UI::c(lit ? UI::contrastOf(p.ST_ACTIVE, p.GROUND, p.TEXT) : cardTintFor(e.desc)), 0);
    }
}

// Switch every primary to `on` - what a tap on the card itself does with any
// on -> all off, else all on.
void commandAll(bool on) {
    if (!s.reg) return;
    const uint32_t now = millis();
    for (uint8_t i = 0; i < s.nEnt; i++) {
        const Entity *e = s.ent[i];
        if (e && e->desc.writable) s.reg->commandValue(e->desc.id, EntityValue::makeBool(on), now);
    }
    renderMain();
}

void toggleCb(lv_event_t *ev) {
    (void)ev;
    // A drag has already decided (knobDragCb); this is the click that ends it.
    if (s.swallowClick) { s.swallowClick = false; return; }
    commandAll(!aggregate().on);
}

// THE KNOB CAN BE DRAGGED (owner, round 7): a knob that slides on a tap but
// cannot be moved by a finger is "jarring". It follows the finger up and down
// the track; let go, and it goes to whichever half it is in - the rest of the
// way by the same slide a tap uses - and the switch follows it. Dragged all the
// way, or most of the way, the switch changes; less than half, the knob slides
// back. A press that does not move is a tap, and toggleCb() has it.
void knobDragCb(lv_event_t *ev) {
    if (!s.knob || !s.hero) return;
    const lv_event_code_t code = lv_event_get_code(ev);
    lv_indev_t *in = lv_indev_active();
    lv_point_t p = {0, 0};
    if (in) lv_indev_get_point(in, &p);
    const int32_t topY    = s.knobInset;
    const int32_t bottomY = s.heroH - s.knobH - s.knobInset;

    if (code == LV_EVENT_PRESSED) {
        lv_anim_delete(s.knob, knobExec);
        s.dragStartY   = p.y;
        s.dragKnobY0   = lv_obj_get_y(s.knob);
        s.knobDragging = false;
        s.swallowClick = false;
    } else if (code == LV_EVENT_PRESSING) {
        const int32_t dy = p.y - s.dragStartY;
        if (!s.knobDragging && LV_ABS(dy) > mm(1.2f)) s.knobDragging = true;
        if (!s.knobDragging) return;
        const int32_t y = LV_CLAMP(topY, s.dragKnobY0 + dy, bottomY);
        lv_obj_set_y(s.knob, y);
        s.knobTarget = y;   // so the next placeKnob() slides from here
    } else if (code == LV_EVENT_RELEASED || code == LV_EVENT_PRESS_LOST) {
        if (!s.knobDragging) return;
        s.knobDragging = false;
        s.swallowClick = (code == LV_EVENT_RELEASED);
        const bool wantOn = lv_obj_get_y(s.knob) + s.knobH / 2 < s.heroH / 2;   // top half = on
        if (wantOn != aggregate().on) commandAll(wantOn);
        else                          renderMain();   // slides back where it was
    }
}

void buildMain() {
    const UIPalette &p = UI::pal();
    const UIType    &t = UI::type();
    const Entity    &e = *s.ent[0];

    lv_obj_t *row = plain(s.stage);
    lv_obj_set_size     (row, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(row, pm(4), 0);

    // THE SAME SHARE OF THE WINDOW ON EVERY BOARD (owner, 2026-10-05). It used
    // to be the window's height less the header row and margins, which on a
    // 7" panel made a toggle twice the P4_5's (60 mm on calipers) and on the 4B
    // nearly 40 mm.
    s.heroH = pm(HERO_H_MM);
    if (s.heroH < UI::minTouch()) s.heroH = UI::minTouch();

    // D3: every writable thing opens on its control. A light that can dim, or
    // has a white range or a colour, gets its slider (2.10b - "The light's
    // controls"); a switch, an on/off light, and a light whose source has not
    // said what it can do (an HA light until 2.10c) get the big toggle, which
    // is HA's own dialog for those.
    const bool writableLight = (e.desc.kind == EntityKind::LIGHT && e.desc.writable);
    s.lightHero  = writableLight && canSlide(lightAggregate().caps);
    s.toggleHero = !s.lightHero && e.desc.writable &&
                   (e.desc.kind == EntityKind::SWITCH || writableLight);
    if (s.lightHero) {
        buildLightHero(row);
        lv_obj_t *col = plain(row);
        lv_obj_set_size     (col, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
        lv_obj_set_flex_flow(col, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_flex_align(col, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
        lv_obj_set_style_pad_row(col, mm(1.0f), 0);
        buildLightColumn(col);
        renderLight();
        return;
    }
    if (s.toggleHero) {
        s.heroW     = s.heroH * 42 / 100;
        s.knobInset = s.heroW * 6 / 100;
        s.knobH     = s.heroH * 48 / 100;
        s.hero = plain(row);
        lv_obj_set_size(s.hero, s.heroW, s.heroH);
        lv_obj_set_style_radius(s.hero, s.heroW * 30 / 100, 0);
        lv_obj_set_style_bg_opa(s.hero, LV_OPA_COVER, 0);
        lv_obj_add_flag(s.hero, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(s.hero, toggleCb,   LV_EVENT_CLICKED,    nullptr);
        lv_obj_add_event_cb(s.hero, knobDragCb, LV_EVENT_PRESSED,    nullptr);
        lv_obj_add_event_cb(s.hero, knobDragCb, LV_EVENT_PRESSING,   nullptr);
        lv_obj_add_event_cb(s.hero, knobDragCb, LV_EVENT_RELEASED,   nullptr);
        lv_obj_add_event_cb(s.hero, knobDragCb, LV_EVENT_PRESS_LOST, nullptr);
        s.knob = plain(s.hero);
        lv_obj_set_size(s.knob, s.heroW - 2 * s.knobInset, s.knobH);
        lv_obj_set_x   (s.knob, s.knobInset);
        lv_obj_set_style_radius(s.knob, (s.heroW - 2 * s.knobInset) * 30 / 100, 0);
        lv_obj_set_style_bg_opa(s.knob, LV_OPA_COVER, 0);
        s.heroIcon = makeLabel(s.knob, t.ICON, p.TEXT);
        lv_obj_center(s.heroIcon);
    } else {
        // A share of the hero's height, like the toggle. The icon inside is a
        // fixed face, so on a 7" panel the disc grows around the same glyph.
        const int32_t d = s.heroH * 60 / 100;
        s.heroW = s.heroH = d;
        s.hero = plain(row);
        lv_obj_set_size(s.hero, d, d);
        lv_obj_set_style_radius(s.hero, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_bg_opa(s.hero, LV_OPA_COVER, 0);
        s.heroIcon = makeLabel(s.hero, t.ICON, p.TEXT);
        lv_obj_center(s.heroIcon);
    }

    lv_obj_t *col = plain(row);
    lv_obj_set_size     (col, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(col, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(col, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_row(col, mm(0.8f), 0);
    // Never wider than what is left beside the hero, so a long text value
    // ellipsises instead of running out of the window.
    const int32_t colMax = lv_area_get_width(&s.winRect) - 2 * s.pad - s.heroW - pm(4);
    lv_obj_set_style_max_width(col, colMax, 0);
    // AND NEVER NARROWER THAN ITS LONGEST ORDINARY WORDS. The row is centred,
    // so a column that changed width moved the whole group - the owner's B1:
    // the first tap turned "Changed 12m ago" into "Changed just now" and the
    // hero jumped left. Sized for the longest lines it will normally show.
    int32_t colMin = textW("Changed just now", t.TAG);
    if (textW("No reading yet", t.TAG) > colMin) colMin = textW("No reading yet", t.TAG);
    if (textW("Unavailable", UIToolkit::Font_Hero) > colMin) colMin = textW("Unavailable", UIToolkit::Font_Hero);
    lv_obj_set_style_min_width(col, colMin < colMax ? colMin : colMax, 0);

    s.lblWhat = makeLabel(col, t.TAG, p.TEXT_DIM);

    lv_obj_t *vrow = plain(col);
    lv_obj_set_size     (vrow, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(vrow, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(vrow, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_END);
    lv_obj_set_style_pad_column(vrow, mm(0.8f), 0);
    lv_obj_set_style_max_width(vrow, colMax, 0);
    s.lblValue = makeLabel(vrow, UIToolkit::Font_Hero, p.TEXT);
    lv_label_set_long_mode(s.lblValue, LV_LABEL_LONG_DOT);
    lv_obj_set_style_max_width(s.lblValue, colMax, 0);
    s.lblUnit  = makeLabel(vrow, t.UNIT, p.TEXT_DIM);

    s.lblAgo = makeLabel(col, t.TAG, p.TEXT_DIM);

    s.knobPlaced = false;
    renderMain();
}

// ---------------------------------------------------------------------------
// The inner views. 2.10a builds the navigation; 2.10e and the group work fill
// them (card-sheet section 13).
// ---------------------------------------------------------------------------
void buildHistory() {
    const UIPalette &p = UI::pal();
    lv_obj_t *l = makeLabel(s.stage, UI::type().NAME, p.TEXT_DIM);
    lv_label_set_text(l, "History arrives with 2.10e");
}

void buildMembers() {
    const UIPalette &p = UI::pal();
    const UIType    &t = UI::type();
    lv_obj_t *list = plain(s.stage);
    lv_obj_set_size     (list, lv_pct(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(list, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(list, mm(1.4f), 0);
    for (uint8_t i = 0; i < s.nEnt; i++) {
        const Entity *e = s.ent[i];
        if (!e) continue;
        lv_obj_t *r = plain(list);
        lv_obj_set_size     (r, lv_pct(100), LV_SIZE_CONTENT);
        lv_obj_set_flex_flow(r, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(r, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_set_style_pad_all(r, mm(1.4f), 0);
        lv_obj_set_style_radius (r, UI::sc(UI::met().RADIUS), 0);
        lv_obj_set_style_bg_color(r, UI::c(p.SURFACE), 0);
        lv_obj_set_style_bg_opa (r, LV_OPA_COVER, 0);
        lv_obj_t *n = makeLabel(r, t.NAME, p.TEXT);
        lv_label_set_text(n, e->desc.name);
        lv_obj_t *v = makeLabel(r, t.TAG, p.TEXT_DIM);
        char buf[48];
        const EntityAttrs &at = e->attrs;
        if (!e->available) {
            lv_label_set_text(v, "Unavailable");
        } else if (e->desc.kind == EntityKind::LIGHT && e->value.type == ValueType::BOOL && e->value.b &&
                   at.brightness >= 0) {
            // A light's levels, so a group's commands can be checked member by
            // member (2.10b): "On, 40%, 2700 K", "On, 40%, colour 30".
            const int pct = LV_MAX(1, (at.brightness * 100 + 127) / 255);
            if (at.lightMode == LightMode::LMODE_TEMP && at.colorTempK > 0)
                snprintf(buf, sizeof(buf), "On, %d%%, %d K", pct, at.colorTempK);
            else if (at.lightMode == LightMode::LMODE_COLOUR && at.hue >= 0)
                snprintf(buf, sizeof(buf), "On, %d%%, colour %d", pct, at.hue);
            else
                snprintf(buf, sizeof(buf), "On, %d%%", pct);
            lv_label_set_text(v, buf);
        } else if (e->value.type == ValueType::BOOL) {
            lv_label_set_text(v, cardStateWord(e->desc, e->value.b));
        } else {
            cardFormatValue(*e, buf, sizeof(buf), true, s.tempUnit);
            lv_label_set_text(v, buf);
        }
    }
    lv_obj_t *note = makeLabel(list, t.TAG, p.TEXT_DIM);
    lv_label_set_text(note, "Member controls arrive with the group work");
}

// Share the title row between "Area > " and the name. Both fit: each gets its
// own width. Otherwise the NAME keeps at least 60% - it is what the window is
// about - and the area gives way first, ellipsised.
void layoutTitle() {
    if (!s.lblTitleArea || !s.lblTitleName) return;
    const lv_font_t *f = UI::type().NAME;
    int32_t aw = textW(lv_label_get_text(s.lblTitleArea), f) + 1;
    int32_t nw = textW(lv_label_get_text(s.lblTitleName), f) + 1;
    if (aw + nw > s.titleW) {
        const int32_t nameFloor = s.titleW * 60 / 100;
        if (nw > s.titleW - aw) nw = (s.titleW - aw > nameFloor) ? s.titleW - aw : nameFloor;
        if (nw > s.titleW) nw = s.titleW;
        aw = s.titleW - nw;
    }
    lv_obj_set_width(s.lblTitleArea, aw > 0 ? aw : 0);
    lv_obj_set_width(s.lblTitleName, nw);
}

void showView(PopupView v) {
    s.view = v;
    forgetMainWidgets();
    lv_obj_clean(s.stage);

    // X on the first view, a back arrow on any view reached from it; the
    // navigation icons only on the first (card-sheet 11.1, mock v3).
    const bool inner = (v != PopupView::VIEW_MAIN);
    setText(s.lblLeft, inner ? LV_SYMBOL_LEFT : LV_SYMBOL_CLOSE);
    if (s.btnHistory) { if (inner) lv_obj_add_flag(s.btnHistory, LV_OBJ_FLAG_HIDDEN);
                        else       lv_obj_clear_flag(s.btnHistory, LV_OBJ_FLAG_HIDDEN); }
    if (s.btnMembers) { if (inner) lv_obj_add_flag(s.btnMembers, LV_OBJ_FLAG_HIDDEN);
                        else       lv_obj_clear_flag(s.btnMembers, LV_OBJ_FLAG_HIDDEN); }

    char title[ENTITY_NAME_MAX + 16];
    if (v == PopupView::VIEW_MAIN) {
        if (s.area[0]) { snprintf(title, sizeof(title), "%s > ", s.area); setText(s.lblTitleArea, title); }
        else           setText(s.lblTitleArea, "");
        setText(s.lblTitleName, s.name);
        buildMain();
    } else {
        snprintf(title, sizeof(title), "%s > ", s.name);
        setText(s.lblTitleArea, title);
        setText(s.lblTitleName, v == PopupView::VIEW_HISTORY ? "History" : "Members");
        if (v == PopupView::VIEW_HISTORY) buildHistory();
        else                              buildMembers();
    }
    layoutTitle();
    s.lastSig = signature();
}

void leftCb(lv_event_t *ev) {
    (void)ev;
    if (s.view == PopupView::VIEW_MAIN) CardPopup::close();
    else                                showView(PopupView::VIEW_MAIN);
}
void historyCb(lv_event_t *ev) { (void)ev; showView(PopupView::VIEW_HISTORY); }
void membersCb(lv_event_t *ev) { (void)ev; showView(PopupView::VIEW_MEMBERS); }

// D6's drag down, on the HEADER ROW only (card-sheet 13): the light's tall
// slider in 2.10b would fight a drag that could start anywhere. How the
// gesture gets here is in buildContents() - it is not the obvious way.
void headerGestureCb(lv_event_t *ev) {
    (void)ev;
    lv_indev_t *in = lv_indev_active();
    if (!in || lv_indev_get_gesture_dir(in) != LV_DIR_BOTTOM) return;
    lv_indev_wait_release(in);   // before the action - LESSONS
    CardPopup::close();
}

#ifdef DEBUG_POPUP
// The interference trial: a long press on the title cycles how often a spark
// runs along the edge - now and then, once per window, never - from the next
// window. Debug builds only, until the owner chooses (a setting, later).
void intfCycleCb(lv_event_t *ev) {
    (void)ev;
    s_intfMode = (s_intfMode == IntfMode::INTF_NOW_AND_THEN) ? IntfMode::INTF_ONCE
               : (s_intfMode == IntfMode::INTF_ONCE)         ? IntfMode::INTF_OFF
                                                             : IntfMode::INTF_NOW_AND_THEN;
    UIToolkit::show_toast(s_intfMode == IntfMode::INTF_NOW_AND_THEN ? "Sparks: now and then"
                        : s_intfMode == IntfMode::INTF_ONCE         ? "Sparks: once per window"
                                                                    : "Sparks: off");
}
#endif

// ---------------------------------------------------------------------------
// The window's contents
// ---------------------------------------------------------------------------
void buildContents() {
    const UIPalette &p = UI::pal();
    const UIType    &t = UI::type();
    const int32_t btn  = UI::minTouch();
    const int32_t gap  = mm(1.5f);

    lv_obj_set_flex_flow(s.win, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(s.win, s.pad, 0);
    lv_obj_set_style_pad_row(s.win, mm(1.6f), 0);

    // --- Header row --------------------------------------------------------
    lv_obj_t *hdr = plain(s.win);
    lv_obj_set_size     (hdr, lv_pct(100), btn);
    lv_obj_set_flex_flow(hdr, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(hdr, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(hdr, gap, 0);
    // Clickable so a press on the title lands HERE and can become a drag.
    lv_obj_add_flag(hdr, LV_OBJ_FLAG_CLICKABLE);
    // AND IT MUST NOT BUBBLE GESTURES. LVGL does not deliver a gesture to the
    // pressed object and bubble it up; it walks UP from the pressed object
    // PAST every one that has GESTURE_BUBBLE - on by default - and sends it to
    // the first that does not (lv_indev.c, indev_gesture()). With the flag
    // left on, the drag went past this row, the window and the top layer, and
    // reached nobody. The owner's C3, 2026-10-03. The buttons in the row keep
    // the flag, so a drag that starts on the X still arrives here.
    lv_obj_clear_flag(hdr, LV_OBJ_FLAG_GESTURE_BUBBLE);
    lv_obj_add_event_cb(hdr, headerGestureCb, LV_EVENT_GESTURE, nullptr);
#ifdef DEBUG_POPUP
    lv_obj_add_event_cb(hdr, intfCycleCb, LV_EVENT_LONG_PRESSED, nullptr);
#endif

    // Both ends the same width, so the title sits in the true middle.
    const uint8_t nRight = (s.nEnt > 1) ? 2 : 1;
    const int32_t slotW  = btn * nRight + gap * (nRight - 1);

    lv_obj_t *left = plain(hdr);
    lv_obj_set_size(left, slotW, btn);
    // The X and the back arrow are LVGL's own symbols, which every built-in
    // Montserrat carries; the hero face is the largest one already linked.
    s.btnLeft = iconButton(left, LV_SYMBOL_CLOSE, UIToolkit::Font_Hero, leftCb, &s.lblLeft);

    lv_obj_t *title = plain(hdr);
    lv_obj_set_height   (title, btn);
    lv_obj_set_flex_grow(title, 1);
    lv_obj_set_flex_flow(title, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(title, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    s.lblTitleArea = makeLabel(title, t.NAME, p.TEXT_DIM);
    s.lblTitleName = makeLabel(title, t.NAME, p.TEXT);
    // ONE LINE EACH, ellipsised, at widths layoutTitle() works out per view.
    // A DOT label with a content height wraps instead of ellipsising, which is
    // how "Members" ended up on two lines and "All Lamps >" overflowed the
    // centred row and lost its left half (owner's V3).
    const int32_t lh = lv_font_get_line_height(t.NAME);
    lv_label_set_long_mode(s.lblTitleArea, LV_LABEL_LONG_DOT);
    lv_label_set_long_mode(s.lblTitleName, LV_LABEL_LONG_DOT);
    lv_obj_set_height(s.lblTitleArea, lh);
    lv_obj_set_height(s.lblTitleName, lh);
    s.titleW = lv_area_get_width(&s.winRect) - 2 * s.pad - 2 * slotW - 2 * gap;

    lv_obj_t *right = plain(hdr);
    lv_obj_set_size     (right, slotW, btn);
    lv_obj_set_flex_flow(right, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(right, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(right, gap, 0);
    // The mock's chart icon - axes and bars (mdi:chart-bar, generated
    // 2026-10-05; clock-outline stood in until then).
    s.btnHistory = iconButton(right, MDI_CHART_BAR, t.ICON_MD, historyCb);
    // D4: a card standing for several things gets the members icon.
    if (s.nEnt > 1) s.btnMembers = iconButton(right, MDI_LIGHTBULB_GROUP, t.ICON_MD, membersCb);

    // --- The stage: whichever view is showing -------------------------------
    s.stage = plain(s.win);
    lv_obj_set_width    (s.stage, lv_pct(100));
    lv_obj_set_flex_grow(s.stage, 1);
    lv_obj_set_flex_flow(s.stage, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(s.stage, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    // --- The auto-close bar: how long until the window gives up -------------
    s.autoBar = plain(s.win);
    lv_obj_add_flag(s.autoBar, LV_OBJ_FLAG_FLOATING);
    lv_obj_set_size(s.autoBar, lv_area_get_width(&s.winRect) - 2 * s.pad, mm(0.5f) < 3 ? 3 : mm(0.5f));
    lv_obj_set_style_radius(s.autoBar, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(s.autoBar, UI::c(p.ACCENT), 0);
    lv_obj_set_style_bg_opa  (s.autoBar, LV_OPA_COVER, 0);
    lv_obj_align(s.autoBar, LV_ALIGN_BOTTOM_MID, 0, s.pad / 2);

    showView(PopupView::VIEW_MAIN);
}

// ---------------------------------------------------------------------------
// The clock: auto-close, the bar, and following the entity
// ---------------------------------------------------------------------------
void tickCb(lv_timer_t *t) {
    (void)t;
    if (s.phase != PopupPhase::PHASE_OPEN) return;
    const uint32_t now  = millis();
    const uint32_t idle = now - s.lastTouchMs;
    if (idle >= POPUP_AUTOCLOSE_MS) { CardPopup::close(); return; }

    if (s.autoBar) {
        const int32_t full = lv_area_get_width(&s.winRect) - 2 * s.pad;
        const int32_t w = (int32_t)((int64_t)full * (POPUP_AUTOCLOSE_MS - idle) / POPUP_AUTOCLOSE_MS);
        if (w != lv_obj_get_width(s.autoBar)) {
            lv_obj_set_width(s.autoBar, w < 1 ? 1 : w);
            lv_obj_align(s.autoBar, LV_ALIGN_BOTTOM_MID, 0, s.pad / 2);
        }
    }

    // The window is live (card-sheet 7): it follows the entity while open.
    const uint32_t sig = signature();
    if (sig != s.lastSig) {
        s.lastSig = sig;
        if (s.view == PopupView::VIEW_MEMBERS) showView(PopupView::VIEW_MEMBERS);
        else                                    renderMain();
        s.lastAgeMs = now;
    } else if (now - s.lastAgeMs >= 1000) {
        s.lastAgeMs = now;
        renderMain();   // the "N ago" line; no-op off the main view
    }
}

// The window, ON THE SCREEN AS ITS TOPMOST OBJECT - not lv_layer_top(). LVGL
// draws the top layer OVER the page, never instead of it (lv_refr.c:1049/1081),
// so with the window up there every redraw inside it - opening, a toggle, the
// countdown bar - drew the cards beneath it first. On the screen, an opaque
// window is what lv_refr_get_top_obj() finds covering those areas, and LVGL
// draws only the window. Created last, so it is above the header, the deck and
// the drawer; a page rebuild moves only the cards to the back.
//
// The scheme's raised surface (the mock's window is Midnight's SURFACE_ALT
// exactly) and the border token. No clip_corner: it would cost a window-sized
// layer (card-sheet 8). Clickable, so a tap inside never reaches the catcher.
void makeWindow() {
    const UIPalette &p = UI::pal();
    s.win = plain(lv_screen_active());
    lv_obj_add_flag(s.win, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_pos (s.win, s.winRect.x1, s.winRect.y1);
    lv_obj_set_size(s.win, lv_area_get_width(&s.winRect), lv_area_get_height(&s.winRect));
    lv_obj_set_style_radius      (s.win, s.winRadius, 0);
    lv_obj_set_style_bg_color    (s.win, UI::c(p.SURFACE_ALT), 0);
    lv_obj_set_style_bg_opa      (s.win, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(s.win, UI::border(), 0);
    lv_obj_set_style_border_width(s.win, UI::met().BORDER_W ? UI::met().BORDER_W : 1, 0);
    // The scheme's lift - a real shadow on Linen, nothing on the dark schemes
    // (owner, L2).
    lv_obj_add_style(s.win, UI::paint(UIPaint::PAINT_LIFT), 0);
}

// ---------------------------------------------------------------------------
// The hold (owner, 2026-10-04)
//
// From touch-down the card is pressed in while its border fades toward the
// scheme's ACCENT over the long-press time, a little wider - so a tap shows the
// start of it, and a hold shows it arriving. The card STAYS pressed in, with
// the accent, while its window is open: that says which card the window
// belongs to, and it does not snap back the moment the window appears (the
// owner found the snap-back "funky" with the leap).
//
// The look itself is the card's: Card::pressLook(), which also moves what is
// attached to the card's edges. Here is only the timing. One card redraw per
// frame: 5-8 ms on WS_P4_5 in Midnight.
// ---------------------------------------------------------------------------

// The card behind a surface. Card::build() puts it in the user data.
Card *cardOf(lv_obj_t *sf) { return sf ? (Card *)lv_obj_get_user_data(sf) : nullptr; }

// The look at `v` (0..255 of the way into the hold).
void holdLook(lv_obj_t *sf, int32_t v) {
    if (Card *c = cardOf(sf)) c->pressLook((uint8_t)v, mm(HOLD_SINK_MM) * v / 255);
}

void holdRestore(lv_obj_t *sf) {
    if (Card *c = cardOf(sf)) c->pressClear();
}

bool movedTooFar() {
    if (!s_indev) return false;
    lv_point_t now;
    lv_indev_get_point(s_indev, &now);
    const int32_t dx = now.x - s.pressStart.x, dy = now.y - s.pressStart.y;
    const int32_t lim = mm(4);
    return dx * dx + dy * dy > lim * lim;
}

void letGo();

// One step of the hold, from the animation. A finger that has moved is a drag
// or a swipe, not a hold: the card lets go at once.
void holdExec(void *var, int32_t v) {
    h.v = v;
    holdLook((lv_obj_t *)var, v);
    if (h.phase == HoldPhase::HOLD_PRESSING && movedTooFar()) letGo();
}

void letGoExec(void *var, int32_t v) {
    h.v = v;
    holdLook((lv_obj_t *)var, v);
}

// THE HELD CARD CAN BE DELETED UNDER THE FINGER - a swipe that started on it
// rebuilds the page. Twice on 2026-10-04 that left h.surface pointing at freed
// memory, and the finger's release then styled it: a load access fault inside
// lv_realloc (lv_obj_set_local_style_prop). Checking lv_obj_is_valid() is not
// enough either: the rebuild allocates new cards from the same pool, so the old
// address is soon a live, different object. So the card says when it goes.
void surfaceDeletedCb(lv_event_t *e) {
    if ((lv_obj_t *)lv_event_get_target(e) == h.surface) h = Hold();
}

// Make `sf` the held card, with its delete hook.
void holdAttach(lv_obj_t *sf) {
    h.surface = sf;
    lv_obj_add_event_cb(sf, surfaceDeletedCb, LV_EVENT_DELETE, nullptr);
}

// Let the held card go: its look back to normal, its hook off. The card is
// alive here - surfaceDeletedCb() clears h.surface the moment it is not.
void holdDetach() {
    if (h.surface) {
        lv_anim_delete(h.surface, holdExec);    // a hold or let-go still running
        lv_anim_delete(h.surface, letGoExec);
        holdRestore(h.surface);
        lv_obj_remove_event_cb(h.surface, surfaceDeletedCb);
    }
    h = Hold();
}

void letGoDone(lv_anim_t *a) {
    if (h.surface == (lv_obj_t *)a->var && h.phase == HoldPhase::HOLD_LETTING_GO) holdDetach();
}

// The finger came up (a tap) or moved off: fade back from wherever the hold
// had got to, at the same rate a full hold would take HOLD_LETGO_MS to undo.
void letGo() {
    if (!h.surface || h.phase != HoldPhase::HOLD_PRESSING) return;
    lv_obj_t *sf = h.surface;
    lv_anim_delete(sf, holdExec);
    h.phase = HoldPhase::HOLD_LETTING_GO;
    const uint32_t ms = HOLD_LETGO_MS * (uint32_t)h.v / 255;
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var         (&a, sf);
    lv_anim_set_values      (&a, h.v, 0);
    lv_anim_set_duration    (&a, ms ? ms : 1);
    lv_anim_set_path_cb     (&a, lv_anim_path_ease_out);
    lv_anim_set_exec_cb     (&a, letGoExec);
    lv_anim_set_completed_cb(&a, letGoDone);
    lv_anim_start(&a);
}

void holdStart(lv_obj_t *sf) {
    holdDetach();
    holdAttach(sf);
    h.phase = HoldPhase::HOLD_PRESSING;
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var     (&a, sf);
    lv_anim_set_values  (&a, 0, 255);
    lv_anim_set_duration(&a, CardPopup::LONG_PRESS_MS);
    lv_anim_set_path_cb (&a, lv_anim_path_linear);   // it is a progress bar
    lv_anim_set_exec_cb (&a, holdExec);
    lv_anim_start(&a);
}

// ---------------------------------------------------------------------------
// Interference: a spark that travels the window's edge (owner, 2026-10-04)
//
// The window is a projection, and now and then a spark of it runs along a
// short stretch of its border: a bright core in a wider glow, a fading tail
// behind, the odd fleck thrown off. It travels a section of the edge, then
// often runs again a little further on, overlapping the last run - so it reads
// as something CRAWLING along the border. Round 3 (owner, 2026-10-05): faster
// and more frenetic, more runs, each with its own dice - see intfTick().
//
// Round 1 was ten scattered pieces living one to three frames, all over the
// edge, all at once. The owner: it looked like "small porch timing issues or
// the wrong pclk frequency" - a real fault - not like electricity, and on a
// light scheme it could not be seen at all. So now: ONE spark at a time, slow
// enough to follow (a run is 250-600 ms), in one place (a quarter of a side
// or less), in electric colours - whites and pale blues on a dark scheme,
// deep blues and violet on a light one. Every roll of the dice is new: which
// edge, where on it, how long the section, the spark's length, its width, its
// colours, its speed, how many runs.
//
// When: never as the window appears - the owner read an immediate one as a
// render problem - but INTF_FIRST_MIN_MS..MAX after, then every
// INTF_GAP_MIN_MS..MAX, and never within INTF_QUIET_MS of a touch. A setting,
// later; the trial switch (title long press) has "now and then", "once", off.
//
// CHEAP BECAUSE IT IS SMALL (LESSONS, "Effects that cover the screen"): five
// plain objects on the screen above the window, a few thousand pixels each,
// moved once a frame. Off is a size of 0x0, never HIDDEN.
// ---------------------------------------------------------------------------
constexpr uint32_t INTF_FIRST_MIN_MS = 1500;
constexpr uint32_t INTF_FIRST_MAX_MS = 3500;
constexpr uint32_t INTF_GAP_MIN_MS   = 5000;
constexpr uint32_t INTF_GAP_MAX_MS   = 12000;
constexpr uint32_t INTF_QUIET_MS     = 2000;

enum IntfPiece : uint8_t { PIECE_GLOW, PIECE_TAIL1, PIECE_TAIL2, PIECE_CORE, PIECE_FLECK, PIECE_COUNT };

struct Interference {
    lv_obj_t   *o[PIECE_COUNT] = {nullptr};
    lv_timer_t *timer   = nullptr;
    uint32_t    nextAt  = 0;        // when the next spark starts
    bool        running = false;
    uint8_t     sparks  = 0;        // sparks so far in this window

    // The spark being drawn. `along` runs the length of one edge.
    uint8_t  edge = 0;              // 0 top, 1 bottom, 2 left, 3 right
    int32_t  runFrom = 0, runTo = 0;   // this run's section, in px along the edge
    int32_t  limit = 0;             // where the straight part of the edge ends
    int32_t  secLen = 0;
    uint8_t  runsLeft = 0;
    uint32_t runStart = 0, runMs = 0;
    bool     reverse = false;       // this run travels back along the edge
    int32_t  len = 0, thick = 0;    // the spark's head: length along, width across
    uint32_t core = 0, glow = 0, tail = 0;   // its colours
};
Interference g;

// The dice for a spark's size, in millimetres so it looks alike on every board.
int32_t rollLen()   { return mm(1.2f) + (int32_t)lv_rand(0, (uint32_t)LV_MAX(1, mm(3.3f))); }
int32_t rollThick() { return LV_MAX(3, mm(0.3f) + (int32_t)lv_rand(0, (uint32_t)LV_MAX(1, mm(0.55f)))); }

// A dark window wants light sparks; a light one, deep ones.
bool lightWindow() {
    const uint32_t w = UI::pal().SURFACE_ALT;
    return ((w >> 16) & 0xFF) + ((w >> 8) & 0xFF) + (w & 0xFF) > 3 * 128;
}

// The palette: [core, glow] pairs. Fixed colours, not tokens - these are the
// colours of electricity, the same on every scheme of a kind. Mixed into the
// window's colour for the tail, so the tail always fades into the window.
void pickColours() {
    static const uint32_t DARK[][2] = {
        {0xFFFFFF, 0x6CC8FF}, {0xF2FBFF, 0x00A8FF}, {0xE6F4FF, 0x5B8CFF},
        {0xFFFFFF, 0x9FE6FF}, {0xEAF0FF, 0x8C7BFF},
    };
    static const uint32_t LIGHT[][2] = {
        {0x1030D0, 0x3D7BFF}, {0x2A1690, 0x6A4DFF}, {0x003C9E, 0x0090D8},
        {0x10107A, 0x4A6CFF},
    };
    const bool light = lightWindow();
    const uint32_t n = light ? sizeof(LIGHT) / sizeof(LIGHT[0]) : sizeof(DARK) / sizeof(DARK[0]);
    const uint32_t k = lv_rand(0, n - 1);
    g.core = light ? LIGHT[k][0] : DARK[k][0];
    g.glow = light ? LIGHT[k][1] : DARK[k][1];
    g.tail = UI::mix(g.glow, UI::pal().SURFACE_ALT, 45);
}

// Put piece `i` `at` px along the edge, `l` long and `t` across, centred on
// the border line. l or t of 0 turns it off.
void intfPut(uint8_t i, int32_t at, int32_t l, int32_t t, uint32_t hex) {
    lv_obj_t *o = g.o[i];
    if (!o) return;
    if (l <= 0 || t <= 0) { lv_obj_set_size(o, 0, 0); return; }
    const lv_area_t &W = s.winRect;
    const int32_t bw = UI::met().BORDER_W ? UI::met().BORDER_W : 1;
    lv_obj_set_style_bg_color(o, UI::c(hex), 0);
    switch (g.edge) {
        case 0:  lv_obj_set_pos(o, W.x1 + at, W.y1 + bw / 2 - t / 2); lv_obj_set_size(o, l, t); break;
        case 1:  lv_obj_set_pos(o, W.x1 + at, W.y2 - bw / 2 - t / 2); lv_obj_set_size(o, l, t); break;
        case 2:  lv_obj_set_pos(o, W.x1 + bw / 2 - t / 2, W.y1 + at); lv_obj_set_size(o, t, l); break;
        default: lv_obj_set_pos(o, W.x2 - bw / 2 - t / 2, W.y1 + at); lv_obj_set_size(o, t, l); break;
    }
}

void intfAllOff() {
    for (uint8_t i = 0; i < PIECE_COUNT; i++) if (g.o[i]) lv_obj_set_size(g.o[i], 0, 0);
}

// Fast and frenetic (owner, round 8): a run is 120-300 ms.
void intfNextRun() {
    g.runMs    = lv_rand(120, 300);
    g.runStart = millis();
}

// Roll the dice for a new spark.
void intfStartSpark() {
    const lv_area_t &W = s.winRect;
    const int32_t r = s.winRadius;
    g.edge = (uint8_t)lv_rand(0, 3);
    const bool horiz = g.edge < 2;
    const int32_t side = horiz ? (W.x2 - W.x1 + 1) : (W.y2 - W.y1 + 1);
    g.limit = side - r;                                  // stay off the rounded corners
    // The section: an eighth to a quarter of the side, never under 8 mm.
    g.secLen = LV_MAX(mm(8.0f), side / 8 + (int32_t)lv_rand(0, (uint32_t)LV_MAX(1, side / 8)));
    if (g.secLen > g.limit - r) g.secLen = g.limit - r;
    if (g.secLen < mm(3.0f)) return;
    g.runFrom = r + (int32_t)lv_rand(0, (uint32_t)LV_MAX(1, g.limit - r - g.secLen));
    g.runTo   = g.runFrom + g.secLen;
    g.runsLeft = (uint8_t)lv_rand(2, 5);
    // ONE DIRECTION PER SPARK (owner, round 9): clockwise or anticlockwise
    // round the window, every run the same way - never back and forth over
    // itself. Along the top and right edges clockwise is the way `along`
    // counts; along the bottom and left it is the other way.
    const bool cw = lv_rand(0, 1);
    g.reverse = (g.edge == 0 || g.edge == 3) ? !cw : cw;
    g.len   = rollLen();
    g.thick = rollThick();
    pickColours();
    intfNextRun();
    g.running = true;
}

// Every frame while the window is open.
void intfTick(lv_timer_t *t) {
    (void)t;
    if (s.phase != PopupPhase::PHASE_OPEN) return;
    const uint32_t now = millis();

    if (!g.running) {
        if ((int32_t)(now - g.nextAt) < 0) return;
        if (s_intfMode == IntfMode::INTF_ONCE && g.sparks) return;
        if (now - s.lastTouchMs < INTF_QUIET_MS) { g.nextAt = now + 1000; return; }
        intfStartSpark();
        if (!g.running) { g.nextAt = now + 1000; return; }
        g.sparks++;
    }

    const uint32_t el = now - g.runStart;
    if (el >= g.runMs) {
        // This run is over. EACH RUN ROLLS ITS OWN DICE (owner, round 8: the
        // same spark repeated three times read as a loop): a new speed, and
        // EITHER a new length OR a new width - never both at once, so the runs
        // still look like one spark. It keeps its direction (round 9) and
        // edges on along it: a step of 0-45% of the section, so each run
        // mostly overlaps the last.
        if (--g.runsLeft) {
            int32_t step = g.secLen * (int32_t)lv_rand(0, 45) / 100;
            if (g.reverse) step = -step;
            g.runFrom = LV_CLAMP(s.winRadius, g.runFrom + step, g.limit - g.secLen);
            g.runTo   = g.runFrom + g.secLen;
            if (lv_rand(0, 1)) g.len = rollLen(); else g.thick = rollThick();
            intfNextRun();
            intfAllOff();
            return;
        }
        intfAllOff();
        g.running = false;
        g.nextAt  = now + lv_rand(INTF_GAP_MIN_MS, INTF_GAP_MAX_MS);
        return;
    }

    // Where the head is: eased along the section, either way, with jitter.
    const int32_t q    = (int32_t)(el * 1024 / g.runMs);
    const int32_t ease = q < 512 ? 2 * q * q / 1024 : 1024 - 2 * (1024 - q) * (1024 - q) / 1024;
    const int32_t travel = (g.runTo - g.runFrom - g.len) * ease / 1024;
    int32_t head = g.reverse ? g.runTo - g.len - travel : g.runFrom + travel;
    const int32_t jit = LV_MAX(1, mm(0.3f));
    head += (int32_t)lv_rand(0, (uint32_t)(2 * jit)) - jit;

    // Crackle: the odd frame the core drops out, the glow flares or the
    // fleck jumps off the line. The tail trails behind, whichever way it runs.
    const bool coreOn  = lv_rand(0, 99) < 85;
    const int32_t flare = lv_rand(0, 99) < 20 ? g.thick / 2 : 0;
    const int32_t gap   = LV_MAX(2, g.len / 3);
    const int32_t l1 = g.len * 3 / 4, l2 = g.len / 2;
    const int32_t t1 = g.reverse ? head + g.len + gap          : head - gap - l1;
    const int32_t t2 = g.reverse ? head + g.len + 2 * gap + l1 : head - 2 * gap - l1 - l2;
    intfPut(PIECE_GLOW,  head, g.len, g.thick + flare, g.glow);
    intfPut(PIECE_TAIL1, t1, l1, LV_MAX(2, g.thick * 2 / 3), g.tail);
    intfPut(PIECE_TAIL2, t2, l2, LV_MAX(1, g.thick / 3), g.tail);
    intfPut(PIECE_CORE,  head + g.len / 6, coreOn ? g.len * 2 / 3 : 0, LV_MAX(1, g.thick / 3), g.core);
    if (lv_rand(0, 99) < 12) {
        const int32_t sz = LV_MAX(2, g.thick / 2);
        const int32_t off = (int32_t)lv_rand(0, (uint32_t)LV_MAX(1, mm(1.8f)));
        const int32_t sign = (g.edge == 0 || g.edge == 2) ? -1 : 1;
        // Thrown off the line: the same edge, shifted outward by `off`.
        intfPut(PIECE_FLECK, head + g.len / 2, sz, sz, g.core);
        lv_obj_t *f = g.o[PIECE_FLECK];
        if (g.edge < 2) lv_obj_set_y(f, lv_obj_get_y(f) + sign * off);
        else            lv_obj_set_x(f, lv_obj_get_x(f) + sign * off);
    } else {
        intfPut(PIECE_FLECK, 0, 0, 0, 0);
    }
}

// The pieces, made with the window, above it, at 0x0. Called inside
// showWindow()'s quiet build (invalidation off), so making them costs nothing.
void intfCreate() {
    for (uint8_t i = 0; i < PIECE_COUNT; i++) g.o[i] = nullptr;
    g.running = false;
    g.sparks  = 0;
    if (s_intfMode == IntfMode::INTF_OFF) return;
    lv_obj_t *scr = lv_screen_active();
    for (uint8_t i = 0; i < PIECE_COUNT; i++) {
        g.o[i] = plain(scr);
        lv_obj_set_size(g.o[i], 0, 0);
        lv_obj_set_style_bg_opa(g.o[i], LV_OPA_COVER, 0);
        lv_obj_set_style_radius(g.o[i], LV_RADIUS_CIRCLE, 0);
    }
}

void intfStart() {
    if (s_intfMode == IntfMode::INTF_OFF) return;
    g.nextAt = millis() + lv_rand(INTF_FIRST_MIN_MS, INTF_FIRST_MAX_MS);
    g.timer  = lv_timer_create(intfTick, LV_DEF_REFR_PERIOD, nullptr);
}

void intfEnd() {
    if (g.timer) { lv_timer_delete(g.timer); g.timer = nullptr; }
    for (uint8_t i = 0; i < PIECE_COUNT; i++) {
        if (g.o[i]) lv_obj_delete(g.o[i]);
        g.o[i] = nullptr;
    }
    g.running = false;
}

// ---------------------------------------------------------------------------
// The settings deck (card-sheet 11.1 pathway 1, section 13; Card Popup Mock v3)
//
// A SETTINGS tab the width of the window peeks up from the bottom of the
// screen, under the window, as the window appears. Tap it and the deck opens
// up over the window's lower part; tap it again, or anywhere in the window,
// and it folds back to the tab; a tap outside closes the deck and the window
// together. It goes with the window, in the same frame.
//
// In 2.10a (section 13): Paused works - the flag is the entity's, kept on the
// device through #60's pause store, which is why long press could give it up.
// The label choice, the custom name and "on the dashboard" are shown but not
// yet live: their answers need a card id that survives a rebuild to be kept
// anywhere (2.10d), and a hidden card needs a way back (the arranger, #78).
//
// Opaque and on the screen, like the window, so moving it redraws only it.
// ---------------------------------------------------------------------------
constexpr uint32_t DECK_PEEK_MS = 200;   // the tab peeking up: a small touch the owner liked
constexpr uint32_t DECK_OPEN_MS = 220;
// 7.2 mm, from 8: the folder tab grew by twice its curve (round 11), and the
// rows paid for it, so an open deck still reaches the same height on the window.
constexpr float    DECK_ROW_MM  = 7.2f;

void deckExec(void *var, int32_t v) { lv_obj_set_y((lv_obj_t *)var, v); }

int32_t deckY(uint8_t state) {
    const int32_t sh = lv_obj_get_height(lv_screen_active());
    return state == DECK_OPEN ? sh - (s.deckH - s.deckHide) : state == DECK_PEEK ? sh - s.deckHead : sh;
}

void deckSet(uint8_t state) {
    if (!s.deck) return;
    const uint8_t was = s.deckState;
    s.deckState = state;
    lv_anim_delete(s.deck, deckExec);
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var     (&a, s.deck);
    lv_anim_set_values  (&a, lv_obj_get_y(s.deck), deckY(state));
    lv_anim_set_duration(&a, (was == DECK_HIDDEN) ? DECK_PEEK_MS : DECK_OPEN_MS);
    lv_anim_set_path_cb (&a, lv_anim_path_ease_out);
    lv_anim_set_exec_cb (&a, deckExec);
    lv_anim_start(&a);
}

// On the deck's tab or its pane, as they stand right now? The deck's own
// object is transparent and wider than the tab: a press beside the tab is a
// press on the page, which the catcher must take.
bool inDeck(const lv_point_t &pt) {
    if (!s.deck || s.deckState == DECK_HIDDEN) return false;
    lv_obj_t *parts[2] = { s.deckTab, s.deckPane };
    for (lv_obj_t *o : parts) {
        if (!o) continue;
        lv_area_t a;
        lv_obj_get_coords(o, &a);
        if (pt.x >= a.x1 && pt.x <= a.x2 && pt.y >= a.y1 && pt.y <= a.y2) return true;
    }
    return false;
}

void deckFill();
void deckOpen();

void deckTabCb(lv_event_t *ev) {
    (void)ev;
    if (s.deckState == DECK_OPEN) deckSet(DECK_PEEK);
    else                          deckOpen();
}

// One choice in a row. `live` false: drawn quieter, and taps do nothing.
//
// FILLED, like the window's X and chart buttons (owner, round 10): a chip in
// the pane's own colour did not read as a button. Unchosen ones take the card
// surface, as those discs do; a chosen live one, the accent.
lv_obj_t *deckChip(lv_obj_t *row, const char *text, bool live, bool selected,
                   lv_event_cb_t cb, void *user) {
    const UIPalette &p = UI::pal();
    lv_obj_t *c = plain(row);
    lv_obj_set_size(c, LV_SIZE_CONTENT, mm(6.0f));
    lv_obj_set_style_pad_hor(c, mm(2.0f), 0);
    lv_obj_set_style_radius(c, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_border_width(c, LV_MAX(1, mm(0.2f)), 0);
    lv_obj_set_style_bg_opa(c, LV_OPA_COVER, 0);
    lv_obj_t *l = makeLabel(c, UI::type().TAG, p.TEXT);
    lv_label_set_text(l, text);
    lv_obj_center(l);
    if (live) {
        lv_obj_set_style_border_color(c, UI::c(p.ACCENT), 0);
        lv_obj_set_style_bg_color(c, UI::c(selected ? p.ACCENT : p.SURFACE), 0);
        lv_obj_set_style_text_color(l, UI::c(selected ? UI::contrastOf(p.ACCENT, p.SURFACE_ALT, p.TEXT)
                                                      : p.ACCENT), 0);
        if (cb) {
            lv_obj_add_flag(c, LV_OBJ_FLAG_CLICKABLE);
            lv_obj_add_event_cb(c, cb, LV_EVENT_CLICKED, user);
        }
    } else {
        // Quieter: a soft edge, dim words, and the chosen one only a shade
        // darker than the rest.
        lv_obj_set_style_border_color(c, UI::c(UI::mix(p.SURFACE_ALT, p.TEXT, 25)), 0);
        lv_obj_set_style_bg_color(c, UI::c(selected ? UI::mix(p.SURFACE, p.TEXT_DIM, 35) : p.SURFACE), 0);
        lv_obj_set_style_text_color(l, UI::c(p.TEXT_DIM), 0);
    }
    return c;
}

// Paint a live chip as chosen or not.
void deckChipSelect(lv_obj_t *c, bool selected) {
    if (!c) return;
    const UIPalette &p = UI::pal();
    lv_obj_set_style_bg_color(c, UI::c(selected ? p.ACCENT : p.SURFACE), 0);
    lv_obj_t *l = lv_obj_get_child(c, 0);
    if (l) lv_obj_set_style_text_color(l, UI::c(selected ? UI::contrastOf(p.ACCENT, p.SURFACE_ALT, p.TEXT) : p.ACCENT), 0);
}

void deckRender() {
    const bool paused = aggregate().paused;
    deckChipSelect(s.chipPause[0], !paused);
    deckChipSelect(s.chipPause[1],  paused);
    const bool all = (s.groupOn == GroupOn::GROUP_ON_ALL);
    deckChipSelect(s.chipGroup[0], !all);
    deckChipSelect(s.chipGroup[1],  all);
}

// On when: any member / all members. On the held card too, so it repaints at
// once and its tap follows the same rule.
void groupChipCb(lv_event_t *ev) {
    s.groupOn = lv_event_get_user_data(ev) ? GroupOn::GROUP_ON_ALL : GroupOn::GROUP_ON_ANY;
    if (Card *c = cardOf(h.surface)) c->setGroupOn(s.groupOn);
    deckRender();
    renderMain();
}

// Paused: Off / On. Through the card when it is the held one (it repaints at
// once, and every card on the same entities follows through the registry).
void pauseChipCb(lv_event_t *ev) {
    const bool on = lv_event_get_user_data(ev) != nullptr;
    if (Card *c = cardOf(h.surface)) {
        c->setPaused(on);
    } else if (s.reg) {
        const uint32_t now = millis();
        for (uint8_t i = 0; i < s.nEnt; i++) if (s.ent[i]) s.reg->setPaused(s.ent[i]->desc.id, on, now);
    }
    deckRender();
    renderMain();
}

// A row: what it is on the left, its choices on the right.
lv_obj_t *deckRow(lv_obj_t *pane, const char *what) {
    lv_obj_t *r = plain(pane);
    lv_obj_set_size     (r, lv_pct(100), mm(DECK_ROW_MM));
    lv_obj_set_flex_flow(r, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(r, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_t *l = makeLabel(r, UI::type().NAME, UI::pal().TEXT);
    lv_label_set_text(l, what);
    lv_obj_t *chips = plain(r);
    lv_obj_set_size     (chips, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(chips, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_column(chips, mm(1.2f), 0);
    return chips;
}

// Built with the window, below the bottom of the screen, inside showWindow()'s
// quiet build (invalidation off); showWindow() then slides it up to its tab.
void deckCreate() {
    const UIPalette &p = UI::pal();
    const UIType    &t = UI::type();
    const lv_area_t &W = s.winRect;
    const int32_t w  = lv_area_get_width(&W);
    const int32_t r  = mm(1.6f);
    const int32_t bw = UI::met().BORDER_W ? UI::met().BORDER_W : 1;
    // The page deck's own header height (owner, round 9: its panels "are
    // already the perfect height to sit neatly under the bottom border of the
    // popup").
    s.deckHead = UIToolkit::sc(UIToolkit::PANEL_HEADER_H);
    // A FOLDER TAB (owner, round 10). The pane has a top edge of its own, and
    // the tab rises from it in the right half, the join curving in like a
    // file folder's - so an open deck is closed off from the window's body.
    //
    // FOLDED, ONLY THE TAB SHOWS (owner, round 11): the strip above the screen's
    // edge is the page deck's header height, and the pane's edge and the curve
    // sit just below it. So the tab reaches a curve's height further down than
    // what shows. OPEN, the pane's own bottom edge stays below the screen too:
    // the deck is deckHide taller than what it uncovers.
    const int32_t rf       = r;                        // the inner curve
    const int32_t tabAbove = s.deckHead + rf;          // the tab's part above the pane's edge
    s.deckHide = r + 2 * bw;
    // A group card has one more row: when it counts as on (2.10b).
    const int32_t rows = (s.nEnt > 1) ? 5 : 4;
    s.deckH    = tabAbove + (rf + mm(1.6f)) + rows * mm(DECK_ROW_MM) + mm(4.0f) + mm(1.6f) + s.deckHide;

    s.deck = plain(lv_screen_active());
    lv_obj_set_pos (s.deck, W.x1, deckY(DECK_HIDDEN));
    lv_obj_set_size(s.deck, w, s.deckH);

    // Built back to front: the pane, the tab over it, a block hiding the
    // tab's bottom where it overlaps the pane, and the inner curve. One colour
    // throughout - the tab used to be the card colour folded and the window's
    // open (owner, round 10). The scheme's lift on the pane and the tab: a
    // shadow on Linen, like the page deck's panels; nothing on the dark ones.
    const int32_t tabW = w / 2;                        // THE RIGHT HALF - see below
    const int32_t tabX = w - tabW;

    lv_obj_t *pane = plain(s.deck);
    s.deckPane = pane;
    lv_obj_set_pos (pane, 0, tabAbove);
    lv_obj_set_size(pane, w, s.deckH - tabAbove);
    lv_obj_set_style_radius      (pane, r, 0);
    lv_obj_set_style_bg_color    (pane, UI::c(p.SURFACE_ALT), 0);
    lv_obj_set_style_bg_opa      (pane, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(pane, UI::border(), 0);
    lv_obj_set_style_border_width(pane, bw, 0);
    lv_obj_add_style             (pane, UI::paint(UIPaint::PAINT_LIFT), 0);
    // The pane's padding keeps its rows below the tab's join.
    lv_obj_set_style_pad_hor(pane, mm(2.4f), 0);
    lv_obj_set_style_pad_top(pane, rf + mm(1.6f), 0);
    lv_obj_set_style_pad_bottom(pane, mm(1.6f) + s.deckHide, 0);   // nothing in the part that stays hidden
    lv_obj_set_flex_flow(pane, LV_FLEX_FLOW_COLUMN);
    lv_obj_add_flag(pane, LV_OBJ_FLAG_CLICKABLE);   // a tap on the pane stays on the pane

    // The tab: THE RIGHT HALF, ALWAYS (owner, round 9). Some cards will get a
    // second panel (a sensor's CHART, card-sheet 11.1); it takes the left half,
    // so SETTINGS is always in the same place. Each panel is the deck's full
    // width when open. It reaches a radius past the pane's edge, its lower
    // corners hidden by the block below.
    s.deckTab = plain(s.deck);
    lv_obj_set_pos (s.deckTab, tabX, 0);
    lv_obj_set_size(s.deckTab, tabW, tabAbove + r + bw);
    lv_obj_set_style_radius      (s.deckTab, r, 0);
    lv_obj_set_style_bg_color    (s.deckTab, UI::c(p.SURFACE_ALT), 0);
    lv_obj_set_style_bg_opa      (s.deckTab, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(s.deckTab, UI::border(), 0);
    lv_obj_set_style_border_width(s.deckTab, bw, 0);
    lv_obj_add_style             (s.deckTab, UI::paint(UIPaint::PAINT_LIFT), 0);
    lv_obj_add_flag(s.deckTab, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(s.deckTab, deckTabCb, LV_EVENT_CLICKED, nullptr);
    s.deckTabLbl = makeLabel(s.deckTab, t.TAG, p.ACCENT);
    lv_label_set_text(s.deckTabLbl, "SETTINGS");
    lv_obj_set_style_text_letter_space(s.deckTabLbl, mm(0.4f), 0);
    // Where round 11 had it, which the owner called right.
    lv_obj_align(s.deckTabLbl, LV_ALIGN_TOP_MID, 0, (s.deckHead - rf - lv_font_get_line_height(t.TAG)) / 2);

    // The block: the tab's lower corners, bottom border and (on Linen) the
    // shadow it casts downward, all of which lie inside the pane - painted out
    // in the pane's colour. Kept one border-width clear of the pane's right
    // edge, which carries on down past the tab.
    const UIMetrics &m = UI::met();
    const int32_t reach = m.SHADOW ? UI::sc(m.SHADOW) + UI::sc(m.SHADOW_Y) + 2 : 0;
    lv_obj_t *block = plain(s.deck);
    lv_obj_set_pos (block, tabX, tabAbove + bw);
    lv_obj_set_size(block, tabW - bw, r + bw + reach);
    lv_obj_set_style_bg_color(block, UI::c(p.SURFACE_ALT), 0);
    lv_obj_set_style_bg_opa  (block, LV_OPA_COVER, 0);

    // The inner curve where the tab meets the pane's edge: a concave corner,
    // which LVGL has no shape for. Two quarter arcs centred a radius out from
    // the join: a thick one in the pane's colour fills the corner outside the
    // curve, and a border-width one draws the edge along it. Their spill onto
    // the tab and the pane is the same colour, so it does not show.
    const lv_point_t ctr = { tabX - rf, tabAbove - rf };
    const int32_t ro = (rf * 1415 + 999) / 1000 + 1;   // reaches the join, rf * sqrt(2)
    auto quarter = [&](int32_t outer, int32_t width, lv_color_t col) {
        lv_obj_t *a = lv_arc_create(s.deck);
        lv_obj_remove_style_all(a);
        lv_obj_clear_flag(a, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_set_pos (a, ctr.x - outer, ctr.y - outer);
        lv_obj_set_size(a, 2 * outer, 2 * outer);
        lv_arc_set_bg_angles(a, 0, 90);                // from +x round to +y: the join's side
        lv_obj_set_style_arc_width  (a, width, LV_PART_MAIN);
        lv_obj_set_style_arc_color  (a, col, LV_PART_MAIN);
        lv_obj_set_style_arc_opa    (a, LV_OPA_COVER, LV_PART_MAIN);
        lv_obj_set_style_arc_rounded(a, false, LV_PART_MAIN);
        lv_obj_set_style_arc_opa    (a, LV_OPA_TRANSP, LV_PART_INDICATOR);
    };
    quarter(ro, ro - rf, UI::c(p.SURFACE_ALT));
    quarter(rf + bw, bw, UI::border());

    s.deckState  = DECK_HIDDEN;
    s.deckFilled = false;
}

// The pane's rows, BUILT WHEN THE DECK IS FIRST OPENED (2.10b, owner: try
// every reasonable saving). Folded, only the tab shows, and the rows were
// ~5 KB of LVGL's pool that most windows never needed.
void deckFill() {
    if (!s.deckPane || s.deckFilled) return;
    s.deckFilled = true;
    const UIType &t = UI::type();
    const UIPalette &p = UI::pal();
    lv_obj_t *pane = s.deckPane;

    // What the label row shows: the card's own choice, or what it inherits.
    CardLabel lbl = CardLabel::LBL_NAME;
    if (Card *c = cardOf(h.surface)) lbl = (c->labelMode() != CardLabel::LBL_INHERIT) ? c->labelMode() : cardLabelMode();
    const bool paused = aggregate().paused;

    // Paused first: the one that works, and the one a person comes for.
    lv_obj_t *row = deckRow(pane, "Paused");
    s.chipPause[0] = deckChip(row, "Off", true, !paused, pauseChipCb, nullptr);
    s.chipPause[1] = deckChip(row, "On",  true,  paused, pauseChipCb, (void *)1);

    // A card for several things: HA's group helper option (owner, 2026-10-05).
    // Live, and kept in RAM until saving arrives (2.10d).
    if (s.nEnt > 1) {
        const bool all = (s.groupOn == GroupOn::GROUP_ON_ALL);
        row = deckRow(pane, "On when");
        s.chipGroup[0] = deckChip(row, "Any is on",  true, !all, groupChipCb, nullptr);
        s.chipGroup[1] = deckChip(row, "All are on", true,  all, groupChipCb, (void *)1);
    }

    row = deckRow(pane, "Label");
    deckChip(row, "HA name", false, lbl == CardLabel::LBL_NAME,  nullptr, nullptr);
    deckChip(row, "Custom",  false, false,                        nullptr, nullptr);
    deckChip(row, "State",   false, lbl == CardLabel::LBL_STATE, nullptr, nullptr);
    deckChip(row, "None",    false, lbl == CardLabel::LBL_NONE,  nullptr, nullptr);

    row = deckRow(pane, "Custom name");
    deckChip(row, "Edit with keyboard", false, false, nullptr, nullptr);

    row = deckRow(pane, "On the dashboard");
    deckChip(row, "Shown",  false, true,  nullptr, nullptr);
    deckChip(row, "Hidden", false, false, nullptr, nullptr);

    lv_obj_t *note = makeLabel(pane, t.TAG, p.TEXT_DIM);
    lv_label_set_text(note, "Paused is kept on the device. The rest arrives with saving (2.10d).");
}

// Open the deck, building its rows the first time.
void deckOpen() {
    deckFill();
    deckSet(DECK_OPEN);
}

void deckGoneCb(lv_anim_t *a) { lv_obj_delete_async((lv_obj_t *)a->var); }

// The deck goes with the window - but HOW depends on how it was showing
// (owner, round 9): open, it vanishes with the window in the same frame;
// showing only its tab, the tab slides back down as fast as it came up, on
// its own, after the window has gone. It is no longer the popup's then: the
// pointers are cleared at once, its tab stops taking taps, and it deletes
// itself when it is out of sight. A window opened meanwhile builds its own.
void deckEnd() {
    if (s.deck) {
        lv_anim_delete(s.deck, deckExec);
        if (s.deckState == DECK_PEEK) {
            if (s.deckTab) lv_obj_clear_flag(s.deckTab, LV_OBJ_FLAG_CLICKABLE);
            lv_anim_t a;
            lv_anim_init(&a);
            lv_anim_set_var         (&a, s.deck);
            lv_anim_set_values      (&a, lv_obj_get_y(s.deck), deckY(DECK_HIDDEN));
            lv_anim_set_duration    (&a, DECK_PEEK_MS);
            lv_anim_set_path_cb     (&a, lv_anim_path_ease_in);
            lv_anim_set_exec_cb     (&a, deckExec);
            lv_anim_set_completed_cb(&a, deckGoneCb);
            lv_anim_start(&a);
        } else {
            lv_obj_delete(s.deck);
        }
    }
    s.deck = s.deckTab = s.deckTabLbl = s.deckPane = nullptr;
    s.chipPause[0] = s.chipPause[1] = nullptr;
    s.chipGroup[0] = s.chipGroup[1] = nullptr;
    s.deckState  = DECK_HIDDEN;
    s.deckFilled = false;
}

#ifdef DEBUG_POPUP
// What LVGL's own pool holds right now (interview G1: a window, open).
void dbgMem(char *buf, size_t cap) {
    lv_mem_monitor_t m;
    lv_mem_monitor(&m);
    snprintf(buf, cap, "lv_mem %lu KB used of %lu, biggest free %lu KB, frag %u%%",
             (unsigned long)((m.total_size - m.free_size) / 1024), (unsigned long)(m.total_size / 1024),
             (unsigned long)(m.free_biggest_size / 1024), (unsigned)m.frag_pct);
}

// Is the window what LVGL finds covering a strip across its middle? If not,
// every redraw inside it draws the page underneath first.
void dbgProbe() {
    if (!s.win) return;
    const lv_area_t &W = s.winRect;
    const int32_t mid = (W.y1 + W.y2) / 2;
    const lv_area_t strip = { W.x1, mid, W.x2, mid + 40 };
    lv_obj_t *top = lv_refr_get_top_obj(&strip, lv_screen_active());
    DBG_POPUP("  a strip across the window is covered by %s; window is child %ld of %lu\n",
              top == s.win ? "the window" : top ? "something else" : "nothing",
              (long)lv_obj_get_index(s.win), (unsigned long)lv_obj_get_child_count(lv_screen_active()));
}
#endif

// The deferred half of close(). Runs from LVGL's own timer handler, outside any
// event callback, so deleting the window's children here is safe. Everything
// goes in one frame, the card's border with it.
void closeNow(void *unused) {
    (void)unused;
    s.closeQueued = false;
    if (s.phase != PopupPhase::PHASE_OPEN) return;
#ifdef DEBUG_POPUP
    const uint32_t t0 = micros();
#endif
    if (s.timer) { lv_timer_delete(s.timer); s.timer = nullptr; }
    intfEnd();
    deckEnd();
    if (s.win)   { lv_obj_delete(s.win);     s.win = nullptr; }
    forgetWidgets();
    holdDetach();
    if (s_catcher) lv_obj_clear_flag(s_catcher, LV_OBJ_FLAG_CLICKABLE);
    s.phase = PopupPhase::PHASE_CLOSED;
    s.nEnt = 0;
#ifdef DEBUG_POPUP
    char mem[96];
    dbgMem(mem, sizeof(mem));
    dbgLater("close: torn down in %lu us; %s", (unsigned long)(micros() - t0), mem);
#endif
}

// Every press, whatever it lands on: where it began, when anything was last
// touched, and - on a card, with no window open - the start of a hold. The
// input device hears PRESSED and RELEASED before the object does, with the
// pressed object as the parameter (send_event() in lv_indev.c).
void pressCb(lv_event_t *ev) {
    lv_indev_t *in = lv_indev_active();
    if (in) lv_indev_get_point(in, &s.pressStart);
    s.lastTouchMs = millis();
    lv_obj_t *obj = (lv_obj_t *)lv_event_get_param(ev);
    if (s.phase == PopupPhase::PHASE_CLOSED && obj &&
        lv_obj_has_flag(obj, CardPopup::CARD_SURFACE_FLAG)) {
        holdStart(obj);
    }
}

// The finger came up. A swipe that LVGL was told to wait out ends with no
// RELEASED here (lv_indev.c, indev_proc_release: PRESS_LOST to the object
// only) - but a swipe has moved, and holdExec() has already let go.
void releaseCb(lv_event_t *ev) {
    (void)ev;
    if (h.phase == HoldPhase::HOLD_PRESSING) letGo();
}

bool inWindow(const lv_point_t &pt) {
    const lv_area_t &W = s.winRect;
    return pt.x >= W.x1 && pt.x <= W.x2 && pt.y >= W.y1 && pt.y <= W.y2;
}

// Did this touch, on the catcher, fold the deck? Then its click is spent.
bool s_catcherFolded = false;

// The catcher's jobs: let presses on the window and its deck through, and
// close on a tap anywhere else. AND WHILE THE DECK IS OPEN, THE WINDOW IS OUT
// OF REACH (owner, round 10): the topmost thing on the screen is the deck, so
// a press anywhere in the window only folds it - it used to fold the deck and
// also press whatever lay under the finger, the X closing the window before
// the deck had finished folding, the back arrow leaving the view.
void catcherCb(lv_event_t *ev) {
    const lv_event_code_t code = lv_event_get_code(ev);
    if (code == LV_EVENT_HIT_TEST) {
        lv_hit_test_info_t *info = lv_event_get_hit_test_info(ev);
        if (!info || !s.win) return;
        const lv_point_t &pt = *info->point;
        if (inDeck(pt) || (s.deckState != DECK_OPEN && inWindow(pt))) info->res = false;
    } else if (code == LV_EVENT_PRESSED) {
        s_catcherFolded = false;
        if (s.deckState == DECK_OPEN && inWindow(s.pressStart)) {
            s_catcherFolded = true;
            deckSet(DECK_PEEK);
        }
    } else if (code == LV_EVENT_CLICKED) {
        if (s_catcherFolded) { s_catcherFolded = false; return; }
        CardPopup::close();
    }
}

// What the window's first frame must draw: the window's own rectangle, and -
// only where the scheme gives it a shadow - four thin bands around it. NOT one
// area of the window plus its shadow: LVGL draws an area in strips as wide as
// the area, and a strip that sticks out past the window is not covered by it,
// so the cards under the whole window would be drawn first (LESSONS, "Effects
// that cover the screen").
void invalidateWindow() {
    lv_obj_t *scr = lv_screen_active();
    const lv_area_t &W = s.winRect;
    lv_obj_invalidate_area(scr, &W);
    const int32_t e = lv_obj_get_ext_draw_size(s.win);
    if (e <= 0) return;
    const lv_area_t bands[4] = {
        { W.x1 - e, W.y1 - e, W.x2 + e, W.y1 - 1 },   // above
        { W.x1 - e, W.y2 + 1, W.x2 + e, W.y2 + e },   // below
        { W.x1 - e, W.y1,     W.x1 - 1, W.y2     },   // left
        { W.x2 + 1, W.y1,     W.x2 + e, W.y2     },   // right
    };
    for (const lv_area_t &b : bands) lv_obj_invalidate_area(scr, &b);
}

// The window itself: built, made modal, the clock started, the deck sliding
// up and the sparks waiting their turn.
//
// BUILT QUIET (2026-10-05). The window's frame took ~77 ms where ~25 was
// expected, and DEBUG_POPUP showed why: it redrew 0,0..1032,627, not the
// window. LVGL lays out a new object's SIZE before its POSITION
// (lv_obj_refr_size, then lv_obj_refr_pos), so for one step the full-size
// window sat at (0,0) and that area was invalidated too; joined with the real
// one, it covered the cards under the window and the header, and every strip
// of it was wider than the window. So: everything is made and laid out with
// invalidation off, and then exactly what must be drawn is asked for.
void showWindow() {
#ifdef DEBUG_POPUP
    const uint32_t t0 = micros();
#endif
    s.phase = PopupPhase::PHASE_OPEN;
    lv_obj_t *scr = lv_screen_active();
    lv_obj_update_layout(scr);                        // settle anything pending, aloud
    lv_display_enable_invalidation(nullptr, false);
    makeWindow();
    buildContents();
    intfCreate();
    deckCreate();
    lv_obj_update_layout(scr);
    lv_display_enable_invalidation(nullptr, true);
    invalidateWindow();

    if (s_catcher) lv_obj_add_flag(s_catcher, LV_OBJ_FLAG_CLICKABLE);
    s.lastTouchMs = millis();
    s.lastAgeMs   = s.lastTouchMs;
    s.timer = lv_timer_create(tickCb, POPUP_TICK_MS, nullptr);
    intfStart();
    deckSet(DECK_PEEK);
#ifdef DEBUG_POPUP
    char mem[96];
    dbgMem(mem, sizeof(mem));
    dbgLater("open: window and deck built in %lu us; %s", (unsigned long)(micros() - t0), mem);
#endif
}

} // namespace

// ---------------------------------------------------------------------------
// Public
// ---------------------------------------------------------------------------

void CardPopup::begin() {
    // Every press and release, whatever it lands on - the same reason
    // GUIManager hooks the input device rather than the screen
    // (screenPressCb()). It also spares every card four event registrations.
    if (lv_indev_t *in = lv_indev_get_next(nullptr)) {
        s_indev = in;
        lv_indev_add_event_cb(in, pressCb,   LV_EVENT_PRESSED,  nullptr);
        lv_indev_add_event_cb(in, releaseCb, LV_EVENT_RELEASED, nullptr);
    }
#ifdef DEBUG_POPUP
    // What the window's own frames cost, and what they redrew.
    if (lv_display_t *d = lv_display_get_default()) {
        lv_display_add_event_cb(d, dbgRenderStart, LV_EVENT_RENDER_START, nullptr);
        lv_display_add_event_cb(d, dbgRenderReady, LV_EVENT_RENDER_READY, nullptr);
    }
#endif
    // The tap catcher, once, for the life of the device. See s_catcher.
    s_catcher = plain(lv_layer_top());
    lv_obj_set_size(s_catcher, lv_pct(100), lv_pct(100));
    lv_obj_add_flag(s_catcher, LV_OBJ_FLAG_ADV_HITTEST);
    lv_obj_add_event_cb(s_catcher, catcherCb, LV_EVENT_HIT_TEST, nullptr);
    lv_obj_add_event_cb(s_catcher, catcherCb, LV_EVENT_PRESSED,  nullptr);
    lv_obj_add_event_cb(s_catcher, catcherCb, LV_EVENT_CLICKED,  nullptr);
}

bool CardPopup::isOpen() { return s.phase != PopupPhase::PHASE_CLOSED; }

void CardPopup::close() {
    if (s.closeQueued) return;
    if (s.phase != PopupPhase::PHASE_OPEN) return;
    s.closeQueued = true;
    lv_async_call(closeNow, nullptr);
}

void CardPopup::open(Card &card) {
    if (s.phase != PopupPhase::PHASE_CLOSED) return;
    if (!card._surface || !card._nPrimary) return;

    // A FINGER THAT MOVED IS A DRAG, NOT A LONG PRESS (card-sheet 7). LVGL
    // fires LONG_PRESSED for a slow drag that never crossed the gesture
    // distance; it must not open a window.
    if (movedTooFar()) { letGo(); return; }
#ifdef DEBUG_POPUP
    s_dbgPressMs = millis();
#endif

    // --- Copy what the window needs; never keep the card -------------------
    s.nEnt = 0;
    for (uint8_t i = 0; i < card._nPrimary && i < CARD_PRIMARY_MAX; i++) {
        if (card._primary[i]) s.ent[s.nEnt++] = card._primary[i];
    }
    if (!s.nEnt) { letGo(); return; }
    // Release the touch BEFORE building anything (LESSONS): the finger is still
    // down, and its release must not land on the window. (It also means LVGL
    // sends no RELEASED for this touch - PRESS_LOST to the card instead.)
    if (lv_indev_t *in = lv_indev_active()) lv_indev_wait_release(in);
    s.reg      = Card::s_reg;
    s.tempUnit = card.tempUnit();
    s.groupOn  = card.groupOn();
    s.lightCtl = firstCtl(lightAggregate().caps);   // D3: open on the control
    snprintf(s.area, sizeof(s.area), "%s", card._area);
    snprintf(s.name, sizeof(s.name), "%s", card.label());

    // --- Where the window goes ---------------------------------------------
    // Tall: from just under the system header to just above the deck's tab.
    //
    // WIDE: THE P4_5'S PROPORTIONS, WHEREVER THE SCREEN ALLOWS (owner,
    // 2026-10-05). It used to be 68 mm on every board, which on the 7" panels
    // made a window taller than wide, its toggle the biggest share of it. Now
    // the width is the height times the P4_5 window's aspect (POPUP_ASPECT),
    // capped by the screen - which on the 4B it is. The height used is the one
    // WITH the system header showing, so hiding the header makes the window
    // taller but never wider.
    lv_obj_t *scr = lv_screen_active();
    const int32_t sw = lv_obj_get_width(scr), sh = lv_obj_get_height(scr);
    const int32_t top    = UIToolkit::systemHeaderPx() + mm(2);
    const int32_t bottom = sh - mm(6) - mm(2);   // mm(6): the deck's tab
    const int32_t refH   = bottom - (UIToolkit::systemHeaderFullPx() + mm(2));
    int32_t w = (int32_t)lroundf((float)refH * POPUP_ASPECT);
    if (w > sw - 2 * mm(3)) w = sw - 2 * mm(3);
    s.winRect.x1 = (sw - w) / 2;
    s.winRect.x2 = s.winRect.x1 + w - 1;
    s.winRect.y1 = top;
    s.winRect.y2 = bottom - 1;
    // What the contents are sized from: the height of the biggest window of
    // the P4_5's shape that fits inside this one. The P4_5 itself gives its own
    // height (with the header shown); the 7" panels theirs; the 4B, whose width
    // is capped, less than its height. So the hero is the same share of the
    // window everywhere (owner), and the 4B's shrinks.
    s.propH = LV_MIN(refH, (int32_t)lroundf((float)w / POPUP_ASPECT));
    s.winRadius  = mm(2.4f);
    // 1.2 mm, down from the mock's 2.2: the owner wanted the X "closer to the
    // corner" (H1). Still clear of the 2.4 mm corner radius.
    s.pad        = mm(1.2f);

    s.view        = PopupView::VIEW_MAIN;
    s.closeQueued = false;

    // --- The card stays pressed in, with the accent, while its window is open.
    // The held card stops where its hold got to and finishes it; a card that
    // somehow was not the held one gets the same look.
    if (h.surface == card._surface) {
        lv_anim_delete(h.surface, holdExec);
        lv_anim_delete(h.surface, letGoExec);
    } else {
        holdDetach();
        holdAttach(card._surface);
    }
    holdLook(h.surface, 255);
    h.phase = HoldPhase::HOLD_OWNED;
    showWindow();
}

#ifdef DEBUG_POPUP
// ---------------------------------------------------------------------------
// GET /popup (CardPopup.h): open, drive and measure a window from a PC. The
// handler runs on the HTTP server's task and never touches LVGL: it posts the
// request and waits; debugService(), an lv_timer on the LVGL thread, does the
// work and writes the reply.
// ---------------------------------------------------------------------------
namespace {
enum DbgReqState : int { DREQ_IDLE, DREQ_CLAIMED, DREQ_PENDING, DREQ_CLOSING, DREQ_DONE };
std::atomic<int> s_dreq{DREQ_IDLE};
struct DbgReq { int card = -1; int deck = -1; int ctl = -1; int set = -1; int view = -1; bool close = false, power = false; };
DbgReq s_dreqArgs;
char   s_dreqOut[2048];
size_t s_dreqLen = 0;

void dbgOut(const char *fmt, ...) {
    if (s_dreqLen >= sizeof(s_dreqOut) - 1) return;
    va_list ap;
    va_start(ap, fmt);
    const int n = vsnprintf(s_dreqOut + s_dreqLen, sizeof(s_dreqOut) - s_dreqLen, fmt, ap);
    va_end(ap);
    if (n > 0) s_dreqLen = LV_MIN(sizeof(s_dreqOut) - 1, s_dreqLen + (size_t)n);
}

// The cards on the screen, in tree order - the surfaces CARD_SURFACE_FLAG marks.
int dbgCards(lv_obj_t *o, lv_obj_t **out, int n, int cap) {
    const uint32_t cnt = lv_obj_get_child_count(o);
    for (uint32_t i = 0; i < cnt && n < cap; i++) {
        lv_obj_t *c = lv_obj_get_child(o, (int32_t)i);
        if (lv_obj_has_flag(c, CardPopup::CARD_SURFACE_FLAG)) out[n++] = c;
        else n = dbgCards(c, out, n, cap);
    }
    return n;
}

esp_err_t handlePopup(httpd_req_t *req) {
    DbgReq a;
    char q[64], v[8];
    if (httpd_req_get_url_query_str(req, q, sizeof(q)) == ESP_OK) {
        if (httpd_query_key_value(q, "card",  v, sizeof(v)) == ESP_OK) a.card  = atoi(v);
        if (httpd_query_key_value(q, "deck",  v, sizeof(v)) == ESP_OK) a.deck  = atoi(v);
        if (httpd_query_key_value(q, "ctl",   v, sizeof(v)) == ESP_OK) a.ctl   = atoi(v);
        if (httpd_query_key_value(q, "set",   v, sizeof(v)) == ESP_OK) a.set   = atoi(v);
        if (httpd_query_key_value(q, "view",  v, sizeof(v)) == ESP_OK) a.view  = atoi(v);
        if (httpd_query_key_value(q, "power", v, sizeof(v)) == ESP_OK) a.power = true;
        if (httpd_query_key_value(q, "close", v, sizeof(v)) == ESP_OK) a.close = atoi(v) != 0;
    }
    int expected = DREQ_IDLE;
    if (!s_dreq.compare_exchange_strong(expected, DREQ_CLAIMED)) {
        httpd_resp_set_status(req, "503 Service Unavailable");
        return httpd_resp_sendstr(req, "Busy; try again.\n");
    }
    s_dreqArgs = a;                 // written before the LVGL thread can see it
    s_dreq.store(DREQ_PENDING);
    for (int i = 0; i < 300 && s_dreq.load() != DREQ_DONE; i++) vTaskDelay(pdMS_TO_TICKS(10));
    expected = DREQ_PENDING;
    if (s_dreq.compare_exchange_strong(expected, DREQ_IDLE)) {
        httpd_resp_set_status(req, "503 Service Unavailable");
        return httpd_resp_sendstr(req, "The UI thread did not pick it up within 3 s.\n");
    }
    while (s_dreq.load() != DREQ_DONE) vTaskDelay(pdMS_TO_TICKS(10));   // a close always ends
    httpd_resp_set_type(req, "text/plain");
    const esp_err_t r = httpd_resp_send(req, s_dreqOut, (ssize_t)s_dreqLen);
    s_dreq.store(DREQ_IDLE);
    return r;
}
} // namespace

void CardPopup::debugService(lv_timer_t *t) {
    (void)t;
    char mem[96];
    const int st = s_dreq.load();
    if (st == DREQ_CLOSING) {
        if (isOpen()) return;   // closeNow() runs from lv_async_call
        dbgMem(mem, sizeof(mem));
        dbgOut("closed; %s\n", mem);
        s_dreq.store(DREQ_DONE);
        return;
    }
    if (st != DREQ_PENDING) return;
    const DbgReq a = s_dreqArgs;
    s_dreqLen = 0;
    s_dreqOut[0] = '\0';

    if (a.close) {
        if (!isOpen()) { dbgOut("no window open\n"); s_dreq.store(DREQ_DONE); return; }
        close();
        s_dreq.store(DREQ_CLOSING);
        return;
    }
    if (a.power) {
        // As a tap on Power (or on a switch's toggle).
        if (!isOpen()) dbgOut("no window open\n");
        else { const bool on = !aggregate().on; commandAll(on); dbgOut("power %s\n", on ? "on" : "off"); }
        s_dreq.store(DREQ_DONE);
        return;
    }
    if (a.view >= 0) {
        if (!isOpen()) dbgOut("no window open\n");
        else {
            showView(a.view == 1 ? PopupView::VIEW_HISTORY : a.view == 2 ? PopupView::VIEW_MEMBERS
                                                                          : PopupView::VIEW_MAIN);
            dbgMem(mem, sizeof(mem));
            dbgOut("view %d; %s\n", a.view, mem);
            for (uint8_t i = 0; i < s.nEnt; i++) {
                const Entity *e = s.ent[i];
                if (!e) continue;
                const EntityAttrs &at = e->attrs;
                dbgOut("  %-12s %s caps %x mode %d bri %d K %d hue %d sat %d%s%s\n", e->desc.id,
                       e->value.type == ValueType::BOOL && e->value.b ? "on " : "off", at.lightCaps,
                       (int)at.lightMode, at.brightness, at.colorTempK, at.hue, at.sat,
                       e->pending ? " pending" : "", e->attrPending ? " levels-pending" : "");
            }
        }
        s_dreq.store(DREQ_DONE);
        return;
    }
    if (a.set >= 0) {
        // As if a finger tapped the slider at this value (percent, kelvin, hue).
        if (!isOpen() || !s.lightHero) dbgOut("no light window open\n");
        else { lightSend(a.set); renderLight(); dbgOut("sent %d to control %d\n", a.set, (int)s.lightCtl); }
        s_dreq.store(DREQ_DONE);
        return;
    }
    if (a.ctl >= 0) {
        if (!isOpen() || !s.lightHero) dbgOut("no light window open\n");
        else {
            s.lightCtl = (uint8_t)LV_MIN(a.ctl, (int)LCTL_COLOUR);
            const uint32_t t0 = micros();
            showView(PopupView::VIEW_MAIN);
            const uint32_t us = micros() - t0;
            dbgMem(mem, sizeof(mem));
            dbgOut("control %d built in %lu us; %s\n", (int)s.lightCtl, (unsigned long)us, mem);
        }
        s_dreq.store(DREQ_DONE);
        return;
    }
    if (a.deck >= 0) {
        if (!isOpen()) dbgOut("no window open\n");
        else {
            if (a.deck) deckOpen(); else deckSet(DECK_PEEK);
            dbgMem(mem, sizeof(mem));
            dbgOut("deck %s; %s\n", a.deck ? "open" : "folded", mem);
        }
        s_dreq.store(DREQ_DONE);
        return;
    }

    lv_obj_t *cards[48];
    const int n = dbgCards(lv_screen_active(), cards, 0, 48);
    if (a.card < 0) {
        dbgMem(mem, sizeof(mem));
        dbgOut("%s; window %s\n", mem, isOpen() ? "open" : "closed");
        for (int i = 0; i < n; i++) {
            Card *c = cardOf(cards[i]);
            if (c) dbgOut("%2d  %-16s %-12s %u entit%s\n", i, c->label(), c->_area,
                          (unsigned)c->_nPrimary, c->_nPrimary == 1 ? "y" : "ies");
        }
        s_dreq.store(DREQ_DONE);
        return;
    }
    if (a.card >= n || !cardOf(cards[a.card])) dbgOut("no card %d (%d on this page)\n", a.card, n);
    else if (isOpen())                          dbgOut("a window is already open; /popup?close=1 first\n");
    else {
        dbgMem(mem, sizeof(mem));
        dbgOut("before: %s\n", mem);
        // A finger that moved is a drag (open() checks): there is no finger,
        // so where it "began" is wherever the input device last was.
        if (s_indev) lv_indev_get_point(s_indev, &s.pressStart);
        const uint32_t t0 = micros();
        open(*cardOf(cards[a.card]));
        const uint32_t us = micros() - t0;
        dbgMem(mem, sizeof(mem));
        dbgOut("open card %d: built in %lu us; window %ldx%ld px, propH %ld, hero %ldx%ld; %s\n",
               a.card, (unsigned long)us, (long)lv_area_get_width(&s.winRect),
               (long)lv_area_get_height(&s.winRect), (long)s.propH, (long)s.heroW, (long)s.heroH, mem);
    }
    s_dreq.store(DREQ_DONE);
}

void CardPopup::beginDebug(HttpServer &http) {
    http.addRoute("/popup", HTTP_GET, handlePopup);
    lv_timer_create(debugService, 20, nullptr);
}
#endif
