#include "Cards/CardPopup.h"
#include "Cards/Card.h"
#include "Cards/CardIcons.h"
#include "UI/UITokens.h"
#include "UI/UIToolkit.h"
#include "src/core/lv_obj_event_private.h"   // lv_hit_test_info_t: the tap catcher's hit test
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
// What the eye gets instead is cheap because it is small: the held card's
// border fades to the accent while the finger is down, the card leaps, and the
// switch's knob slides (see "The hold, and the leap").

// Per the repo's debug-flag convention (CLAUDE.md). What opening and closing
// cost on this side of the frame: building the window and tearing it down.
// The frames themselves are -D DEBUG_FRAMES (GUIManager.cpp). With this flag,
// a long press on the window's title also cycles the leap: 2 frames, 1, none.
#ifdef DEBUG_POPUP
    #define DBG_POPUP(...) Serial.printf("[Popup:debug] " __VA_ARGS__)
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

// The frame after a window appears or goes. -D DEBUG_FRAMES cannot see it: it
// only prints bursts of three frames or more, and closing is one.
void dbgRenderStart(lv_event_t *e) { (void)e; if (s_dbgWantFrame) s_dbgRenderT0 = millis(); }
void dbgRenderReady(lv_event_t *e) {
    (void)e;
    if (!s_dbgWantFrame || !s_dbgRenderT0) return;
    s_dbgFrameMs   = millis() - s_dbgRenderT0;
    s_dbgGlassMs   = s_dbgPressMs ? millis() - s_dbgPressMs : 0;
    s_dbgWantFrame = false;
    s_dbgRenderT0  = 0;
}

void dbgPrint(lv_timer_t *t) {
    (void)t;
    if (s_dbgGlassMs) DBG_POPUP("%s; its frame drew in %lu ms; long press to glass %lu ms\n", s_dbgLine,
                                (unsigned long)s_dbgFrameMs, (unsigned long)s_dbgGlassMs);
    else              DBG_POPUP("%s; its frame drew in %lu ms\n", s_dbgLine, (unsigned long)s_dbgFrameMs);
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
// LEAP: the long press has fired and the card is leaping; the window comes
// after LEAP_FRAMES frames. Modal from here, like OPEN.
enum class PopupPhase : uint8_t { PHASE_CLOSED, PHASE_LEAP, PHASE_OPEN };
// Where the held card is. OWNED: its window is open, and it keeps the accent
// border until the window closes.
enum class HoldPhase  : uint8_t { HOLD_NONE, HOLD_PRESSING, HOLD_LETTING_GO, HOLD_LEAPING, HOLD_OWNED };

// D6: auto-close after 60 s untouched. The owner chose all four close routes.
constexpr uint32_t POPUP_AUTOCLOSE_MS = 60000;
constexpr uint32_t POPUP_TICK_MS      = 250;

// The hold and the leap, in millimetres so they look the same on every board.
constexpr float    HOLD_SINK_MM    = 0.4f;   // how far the card sinks by the long press
constexpr float    LEAP_GROW_MM    = 1.0f;   // how much larger than normal it leaps
constexpr uint32_t HOLD_LETGO_MS   = 120;    // a tap lets go this fast, from full
constexpr uint32_t KNOB_SLIDE_MS   = 160;
// Frames of leap before the window. 2 = ~66 ms the window waits for it. With
// DEBUG_POPUP a long press on the window's title cycles 2 -> 1 -> 0.
uint8_t s_leapFrames = 2;

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

    lv_obj_t *win = nullptr;
    lv_obj_t *btnLeft = nullptr, *lblLeft = nullptr;
    lv_obj_t *btnHistory = nullptr, *btnMembers = nullptr;
    lv_obj_t *lblTitleArea = nullptr, *lblTitleName = nullptr;
    int32_t   titleW = 0;      // what the two title labels may use together
    lv_obj_t *stage = nullptr, *autoBar = nullptr;

    // The main view's widgets; null while another view is showing.
    lv_obj_t *hero = nullptr, *knob = nullptr, *heroIcon = nullptr;
    lv_obj_t *lblWhat = nullptr, *lblValue = nullptr, *lblUnit = nullptr, *lblAgo = nullptr;
    int32_t   heroW = 0, heroH = 0, knobH = 0, knobInset = 0;
    bool      toggleHero = false;
    bool      knobPlaced = false;   // the first placement jumps; every later one slides
    int32_t   knobTarget = 0;

    lv_timer_t *timer = nullptr;
    uint32_t    lastTouchMs = 0;
    uint32_t    lastSig = 0;
    uint32_t    lastAgeMs = 0;
    lv_point_t  pressStart = {0, 0};
};
Popup s;

// The card under the finger. `surface` is a card's own LVGL object, which a
// page rebuild can delete at any moment - so it is checked with
// lv_obj_is_valid() before it is touched outside an animation (an animation
// whose var is the surface is deleted with it, by LVGL).
struct Hold {
    HoldPhase phase = HoldPhase::HOLD_NONE;
    lv_obj_t *surface = nullptr;
    int32_t   v = 0;          // 0..255: how far toward the accent
    uint8_t   leapStep = 0;
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

Agg aggregate() {
    Agg a;
    for (uint8_t i = 0; i < s.nEnt; i++) {
        const Entity *e = s.ent[i];
        if (!e) continue;
        if (e->value.type == ValueType::BOOL) { a.anyBool = true; if (e->value.b) a.on = true; }
        if (!e->available) a.available = false;
        if (e->paused)     a.paused = true;
        if (e->everSet) {
            a.everSet = true;
            if (e->lastChangeMs > a.lastChangeMs) a.lastChangeMs = e->lastChangeMs;
        }
    }
    return a;
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
              (e->cmdFailed ? 8u : 0u) | (e->everSet ? 16u : 0u));
        if (e->value.type == ValueType::BOOL) mixIn(e->value.b ? 1u : 0u);
    }
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

void forgetWidgets() {
    s.btnLeft = s.lblLeft = s.btnHistory = s.btnMembers = nullptr;
    s.lblTitleArea = s.lblTitleName = nullptr;
    s.stage = s.autoBar = nullptr;
    s.hero = s.knob = s.heroIcon = nullptr;
    s.lblWhat = s.lblValue = s.lblUnit = s.lblAgo = nullptr;
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

// ---------------------------------------------------------------------------
// The main view: the hero beside what it shows (card-sheet 11.1)
// ---------------------------------------------------------------------------
void renderMain() {
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

void toggleCb(lv_event_t *ev) {
    (void)ev;
    if (!s.reg) return;
    // Any on -> all off, else all on: what a tap on the card itself does.
    const bool want = !aggregate().on;
    const uint32_t now = millis();
    for (uint8_t i = 0; i < s.nEnt; i++) {
        const Entity *e = s.ent[i];
        if (e && e->desc.writable) s.reg->commandValue(e->desc.id, EntityValue::makeBool(want), now);
    }
    renderMain();
}

void buildMain() {
    const UIPalette &p = UI::pal();
    const UIType    &t = UI::type();
    const Entity    &e = *s.ent[0];

    lv_obj_t *row = plain(s.stage);
    lv_obj_set_size     (row, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(row, mm(4), 0);

    // The stage is not laid out yet, so its height is worked out from the
    // window's: everything but the header row, the padding and a margin.
    const int32_t winH = lv_area_get_height(&s.winRect);
    s.heroH = winH - 2 * s.pad - UI::minTouch() - mm(1.6f) - mm(4);
    if (s.heroH < UI::minTouch()) s.heroH = UI::minTouch();

    // D3: a SWITCH opens on its control - the hero IS a big toggle. Lights get
    // their slider in 2.10b; until then they read like everything else.
    s.toggleHero = (e.desc.kind == EntityKind::SWITCH && e.desc.writable);
    if (s.toggleHero) {
        s.heroW     = s.heroH * 42 / 100;
        s.knobInset = s.heroW * 6 / 100;
        s.knobH     = s.heroH * 48 / 100;
        s.hero = plain(row);
        lv_obj_set_size(s.hero, s.heroW, s.heroH);
        lv_obj_set_style_radius(s.hero, s.heroW * 30 / 100, 0);
        lv_obj_set_style_bg_opa(s.hero, LV_OPA_COVER, 0);
        lv_obj_add_flag(s.hero, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(s.hero, toggleCb, LV_EVENT_CLICKED, nullptr);
        s.knob = plain(s.hero);
        lv_obj_set_size(s.knob, s.heroW - 2 * s.knobInset, s.knobH);
        lv_obj_set_x   (s.knob, s.knobInset);
        lv_obj_set_style_radius(s.knob, (s.heroW - 2 * s.knobInset) * 30 / 100, 0);
        lv_obj_set_style_bg_opa(s.knob, LV_OPA_COVER, 0);
        s.heroIcon = makeLabel(s.knob, t.ICON, p.TEXT);
        lv_obj_center(s.heroIcon);
    } else {
        int32_t d = s.heroH * 60 / 100;
        if (d > mm(24)) d = mm(24);
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
    const int32_t colMax = lv_area_get_width(&s.winRect) - 2 * s.pad - s.heroW - mm(4);
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
        if (!e->available)                       lv_label_set_text(v, "Unavailable");
        else if (e->value.type == ValueType::BOOL) lv_label_set_text(v, cardStateWord(e->desc, e->value.b));
        else { cardFormatValue(*e, buf, sizeof(buf), true, s.tempUnit); lv_label_set_text(v, buf); }
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
    s.hero = s.knob = s.heroIcon = nullptr;
    s.lblWhat = s.lblValue = s.lblUnit = s.lblAgo = nullptr;
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
// The leap trial: a long press on the title cycles how many frames the card
// leaps for before the window appears - 2, 1, none - from the next window.
// Debug builds only: the trial ends in a decision, not a setting.
void leapCycleCb(lv_event_t *ev) {
    (void)ev;
    s_leapFrames = (s_leapFrames == 0) ? 2 : (uint8_t)(s_leapFrames - 1);
    UIToolkit::show_toast(s_leapFrames == 2 ? "Leap: 2 frames - from the next window"
                        : s_leapFrames == 1 ? "Leap: 1 frame - from the next window"
                                            : "Leap: none - from the next window");
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
    lv_obj_add_event_cb(hdr, leapCycleCb, LV_EVENT_LONG_PRESSED, nullptr);
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
    // The mock's chart icon is not in the board's icon subset, and this laptop
    // cannot regenerate it (no Node.js). clock-outline stands in until it can.
    s.btnHistory = iconButton(right, MDI_CLOCK_OUTLINE, t.ICON_MD, historyCb);
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
// The hold, and the leap (owner, 2026-10-04)
//
// From touch-down the card's border fades toward the scheme's ACCENT over the
// long-press time, a little wider, while the card sinks slightly - so a tap
// shows the start of it, and a hold shows it arriving. At the long press the
// card LEAPS, larger than normal, for s_leapFrames frames, and then the window
// appears. The card keeps the accent border while its window is open, which
// says which card the window belongs to.
//
// ALL OF IT IS transform_width/height AND THE BORDER - NEVER A SCALE. A scale
// renders the card to an intermediate layer every frame (~175 KB for a P4_5
// card, more than LVGL's whole 128 KB pool - LESSONS, "LVGL allocates a
// LAYER"). transform_width/height only draws the card's own background,
// border and shadow larger or smaller (lv_obj.c, LV_EVENT_DRAW_MAIN), with no
// layer; its contents stay where they are, which nobody sees in a frame or
// two. One card redraw per frame: 5-8 ms on WS_P4_5 in Midnight.
// ---------------------------------------------------------------------------

// The look at `v` (0..255 toward the accent), with the card `grow` px larger
// on every side (negative: sinking).
void holdLook(lv_obj_t *sf, int32_t v, int32_t grow) {
    const int32_t base = UI::met().BORDER_W;
    const int32_t wide = LV_MAX(base, UI::sc(2));
    lv_obj_set_style_border_color(sf, lv_color_mix(UI::c(UI::pal().ACCENT), UI::border(), (uint8_t)v), 0);
    lv_obj_set_style_border_width(sf, base + (wide - base) * v / 255, 0);
    lv_obj_set_style_transform_width (sf, grow, 0);
    lv_obj_set_style_transform_height(sf, grow, 0);
}

// Back to exactly what Card::restyle() gives every card: the border token at
// the scheme's width, no transform.
void holdRestore(lv_obj_t *sf) {
    lv_obj_set_style_border_color(sf, UI::border(), 0);
    lv_obj_set_style_border_width(sf, UI::met().BORDER_W, 0);
    lv_obj_set_style_transform_width (sf, 0, 0);
    lv_obj_set_style_transform_height(sf, 0, 0);
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
    holdLook((lv_obj_t *)var, v, -(mm(HOLD_SINK_MM) * v) / 255);
    if (h.phase == HoldPhase::HOLD_PRESSING && movedTooFar()) letGo();
}

void letGoExec(void *var, int32_t v) {
    h.v = v;
    holdLook((lv_obj_t *)var, v, -(mm(HOLD_SINK_MM) * v) / 255);
}

void letGoDone(lv_anim_t *a) {
    lv_obj_t *sf = (lv_obj_t *)a->var;
    holdRestore(sf);
    if (h.surface == sf && h.phase == HoldPhase::HOLD_LETTING_GO) h = Hold();
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

// Drop whatever card was held, at once - a new press, or the window closing.
void holdDrop() {
    if (h.surface && lv_obj_is_valid(h.surface)) {
        lv_anim_delete(h.surface, holdExec);
        lv_anim_delete(h.surface, letGoExec);
        holdRestore(h.surface);
    }
    h = Hold();
}

void holdStart(lv_obj_t *sf) {
    holdDrop();
    h.surface = sf;
    h.phase   = HoldPhase::HOLD_PRESSING;
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var     (&a, sf);
    lv_anim_set_values  (&a, 0, 255);
    lv_anim_set_duration(&a, CardPopup::LONG_PRESS_MS);
    lv_anim_set_path_cb (&a, lv_anim_path_linear);   // it is a progress bar
    lv_anim_set_exec_cb (&a, holdExec);
    lv_anim_start(&a);
}

void showWindow();

// After every refresh: steps the leap, one look per frame actually drawn, then
// asks for the window. The first refresh after the long press is the frame
// that drew the first leap step.
void displayRefrReadyCb(lv_event_t *e) {
    (void)e;
    if (s.phase != PopupPhase::PHASE_LEAP || h.phase != HoldPhase::HOLD_LEAPING) return;
    if (!lv_obj_is_valid(h.surface)) { h = Hold(); showWindow(); return; }
    h.leapStep++;
    if (h.leapStep < s_leapFrames) {
        holdLook(h.surface, 255, mm(LEAP_GROW_MM) * (h.leapStep + 1) / s_leapFrames);
        return;
    }
    // Seen: back to its own size, the accent kept, and the window.
    holdLook(h.surface, 255, 0);
    h.phase = HoldPhase::HOLD_OWNED;
    showWindow();
}

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
    if (s.win)   { lv_obj_delete(s.win);     s.win = nullptr; }
    forgetWidgets();
    holdDrop();
    if (s_catcher) lv_obj_clear_flag(s_catcher, LV_OBJ_FLAG_CLICKABLE);
    s.phase = PopupPhase::PHASE_CLOSED;
    s.nEnt = 0;
#ifdef DEBUG_POPUP
    dbgLater("close: torn down in %lu us", (unsigned long)(micros() - t0));
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

// The catcher's two jobs: let presses on the window through, and close on a
// tap anywhere else.
void catcherCb(lv_event_t *ev) {
    const lv_event_code_t code = lv_event_get_code(ev);
    if (code == LV_EVENT_HIT_TEST) {
        lv_hit_test_info_t *info = lv_event_get_hit_test_info(ev);
        if (!info || !s.win) return;
        const lv_point_t *pt = info->point;
        const lv_area_t  &W  = s.winRect;
        if (pt->x >= W.x1 && pt->x <= W.x2 && pt->y >= W.y1 && pt->y <= W.y2) info->res = false;
    } else if (code == LV_EVENT_CLICKED) {
        CardPopup::close();
    }
}

// The window itself: built, made modal, and the clock started. Straight from
// open() with no leap, or from displayRefrReadyCb() once the leap was seen.
void showWindow() {
#ifdef DEBUG_POPUP
    const uint32_t t0 = micros();
#endif
    s.phase = PopupPhase::PHASE_OPEN;
    makeWindow();
    buildContents();
    if (s_catcher) lv_obj_add_flag(s_catcher, LV_OBJ_FLAG_CLICKABLE);
    s.lastTouchMs = millis();
    s.lastAgeMs   = s.lastTouchMs;
    s.timer = lv_timer_create(tickCb, POPUP_TICK_MS, nullptr);
#ifdef DEBUG_POPUP
    dbgLater("open: window built in %lu us after a %u-frame leap",
             (unsigned long)(micros() - t0), (unsigned)s_leapFrames);
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
    // After each frame is drawn: what steps the leap.
    if (lv_display_t *d = lv_display_get_default()) {
        lv_display_add_event_cb(d, displayRefrReadyCb, LV_EVENT_REFR_READY, nullptr);
#ifdef DEBUG_POPUP
        lv_display_add_event_cb(d, dbgRenderStart, LV_EVENT_RENDER_START, nullptr);
        lv_display_add_event_cb(d, dbgRenderReady, LV_EVENT_RENDER_READY, nullptr);
#endif
    }
    // The tap catcher, once, for the life of the device. See s_catcher.
    s_catcher = plain(lv_layer_top());
    lv_obj_set_size(s_catcher, lv_pct(100), lv_pct(100));
    lv_obj_add_flag(s_catcher, LV_OBJ_FLAG_ADV_HITTEST);
    lv_obj_add_event_cb(s_catcher, catcherCb, LV_EVENT_HIT_TEST, nullptr);
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
    snprintf(s.area, sizeof(s.area), "%s", card._area);
    snprintf(s.name, sizeof(s.name), "%s", card.label());

    // --- Where the window goes ---------------------------------------------
    // The mock: ~68 mm wide (62% of the P4_5, nearly all of the 4B), from just
    // under the system header to just above the deck's peeking header.
    lv_obj_t *scr = lv_screen_active();
    const int32_t sw = lv_obj_get_width(scr), sh = lv_obj_get_height(scr);
    int32_t w = mm(68);
    if (w > sw - 2 * mm(3)) w = sw - 2 * mm(3);
    const int32_t top    = UIToolkit::systemHeaderPx() + mm(2);
    const int32_t bottom = sh - mm(6) - mm(2);   // mm(6): the deck's header, step 2
    s.winRect.x1 = (sw - w) / 2;
    s.winRect.x2 = s.winRect.x1 + w - 1;
    s.winRect.y1 = top;
    s.winRect.y2 = bottom - 1;
    s.winRadius  = mm(2.4f);
    // 1.2 mm, down from the mock's 2.2: the owner wanted the X "closer to the
    // corner" (H1). Still clear of the 2.4 mm corner radius.
    s.pad        = mm(1.2f);

    s.view        = PopupView::VIEW_MAIN;
    s.closeQueued = false;
    s.phase       = PopupPhase::PHASE_LEAP;   // modal from here

    // --- The leap -----------------------------------------------------------
    // The held card stops where its hold got to and takes the full accent; a
    // card that somehow was not the held one gets the accent all the same.
    if (h.surface == card._surface && lv_obj_is_valid(h.surface)) {
        lv_anim_delete(h.surface, holdExec);
        lv_anim_delete(h.surface, letGoExec);
    } else {
        holdDrop();
        h.surface = card._surface;
    }
    if (s_leapFrames) {
        h.phase    = HoldPhase::HOLD_LEAPING;
        h.leapStep = 0;
        holdLook(h.surface, 255, mm(LEAP_GROW_MM) / s_leapFrames);
    } else {
        holdLook(h.surface, 255, 0);
        h.phase = HoldPhase::HOLD_OWNED;
        showWindow();
    }
}
