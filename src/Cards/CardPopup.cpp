#include "Cards/CardPopup.h"
#include "Cards/Card.h"
#include "Cards/CardIcons.h"
#include "UI/UITokens.h"
#include "UI/UIToolkit.h"
#include <Arduino.h>
#include <math.h>
#include <string.h>

// See CardPopup.h for what this is and docs/design/card-sheet.md sections 11-13
// for why it looks the way it does. Step 1 of 2.10a: the dim, the grow, the
// window, its header, the four ways to close, the body and the inner views.
// The settings deck comes next.

// Per the repo's debug-flag convention (CLAUDE.md). How many frames the grow
// and shrink really get, and what the dim frame costs - the owner saw "only a
// few frames" (O2, 2026-10-03), and the next step is decided by these numbers.
#ifdef DEBUG_POPUP
    #define DBG_POPUP(...) Serial.printf("[Popup:debug] " __VA_ARGS__)
#else
    #define DBG_POPUP(...) do {} while (0)
#endif

namespace {

#ifdef DEBUG_POPUP
constexpr uint8_t DBG_MARKS = 48;
uint32_t s_dbgMark[DBG_MARKS];
uint8_t  s_dbgN = 0;

// One timestamp per animation step. An exec call happens once per LVGL timer
// pass, so the gaps between them ARE the frame times the eye sees.
void dbgMark() { if (s_dbgN < DBG_MARKS) s_dbgMark[s_dbgN++] = millis(); }

void dbgReport(const char *what) {
    if (s_dbgN < 2) { DBG_POPUP("%s: %u step(s)\n", what, (unsigned)s_dbgN); s_dbgN = 0; return; }
    char line[200];
    int n = 0;
    for (uint8_t i = 1; i < s_dbgN && n < (int)sizeof(line) - 8; i++) {
        n += snprintf(line + n, sizeof(line) - n, " %lu",
                      (unsigned long)(s_dbgMark[i] - s_dbgMark[i - 1]));
    }
    DBG_POPUP("%s: %u frames in %lu ms; gaps ms:%s\n", what, (unsigned)s_dbgN,
              (unsigned long)(s_dbgMark[s_dbgN - 1] - s_dbgMark[0]), line);
    s_dbgN = 0;
}
#else
inline void dbgMark() {}
inline void dbgReport(const char *) {}
#endif

// Compound names, never a bare ALL-CAPS word - Arduino's pin-mode macros eat
// those (CLAUDE.md, "Arduino's global macro namespace will eat your enum").
enum class PopupView  : uint8_t { VIEW_MAIN, VIEW_HISTORY, VIEW_MEMBERS };
enum class PopupPhase : uint8_t { PHASE_CLOSED, PHASE_GROWING, PHASE_OPEN, PHASE_SHRINKING };

// D6: auto-close after 60 s untouched. The owner chose all four close routes.
constexpr uint32_t POPUP_AUTOCLOSE_MS = 60000;
constexpr uint32_t POPUP_TICK_MS      = 250;
// The mock's timings: the frame grows in 0.2 s and shrinks back in 0.18 s.
constexpr uint32_t POPUP_GROW_MS      = 200;
constexpr uint32_t POPUP_SHRINK_MS    = 180;
// ~62%, the mock's dim. A BACKGROUND opacity on the scrim, which is ordinary
// blending - not an object opa, which would composite a screen-sized layer.
constexpr lv_opa_t POPUP_SCRIM_OPA    = 158;

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

    lv_area_t cardRect = {};   // where the card's surface sat, screen coords
    int32_t   cardRadius = 0;
    lv_area_t winRect = {};
    int32_t   winRadius = 0;
    int32_t   pad = 0;

    // The two ends of whichever animation is running.
    lv_area_t animA = {}, animB = {};
    int32_t   radA = 0, radB = 0;

    lv_obj_t *scrim = nullptr, *win = nullptr;
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

    lv_timer_t *timer = nullptr;
    uint32_t    lastTouchMs = 0;
    uint32_t    lastSig = 0;
    uint32_t    lastAgeMs = 0;
    lv_point_t  pressStart = {0, 0};
};
Popup s;

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

// ---------------------------------------------------------------------------
// The grow and the shrink: one exec callback, interpolating a rectangle.
// ---------------------------------------------------------------------------
void animExec(void *var, int32_t v) {
    lv_obj_t *o = (lv_obj_t *)var;
    auto lerp = [v](int32_t a, int32_t b) { return a + (int32_t)(((int64_t)(b - a) * v) >> 10); };
    const int32_t x1 = lerp(s.animA.x1, s.animB.x1), y1 = lerp(s.animA.y1, s.animB.y1);
    const int32_t x2 = lerp(s.animA.x2, s.animB.x2), y2 = lerp(s.animA.y2, s.animB.y2);
    lv_obj_set_pos (o, x1, y1);
    lv_obj_set_size(o, x2 - x1 + 1, y2 - y1 + 1);
    lv_obj_set_style_radius(o, lerp(s.radA, s.radB), 0);
    dbgMark();
}

void startAnim(uint32_t ms, lv_anim_path_cb_t path, lv_anim_completed_cb_t done) {
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var        (&a, s.win);
    lv_anim_set_values     (&a, 0, 1024);
    lv_anim_set_duration   (&a, ms);
    lv_anim_set_path_cb    (&a, path);
    lv_anim_set_exec_cb    (&a, animExec);
    lv_anim_set_completed_cb(&a, done);
    lv_anim_start(&a);
}

void forgetWidgets() {
    s.btnLeft = s.lblLeft = s.btnHistory = s.btnMembers = nullptr;
    s.lblTitleArea = s.lblTitleName = nullptr;
    s.stage = s.autoBar = nullptr;
    s.hero = s.knob = s.heroIcon = nullptr;
    s.lblWhat = s.lblValue = s.lblUnit = s.lblAgo = nullptr;
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
        lv_obj_set_y(s.knob, lit ? s.knobInset : s.heroH - s.knobH - s.knobInset);
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

// ---------------------------------------------------------------------------
// The window's contents, built once the frame has finished growing
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

void growDone(lv_anim_t *a) {
    (void)a;
    if (s.phase != PopupPhase::PHASE_GROWING) return;
    dbgReport("grow");
    s.phase = PopupPhase::PHASE_OPEN;
    // The scheme's lift - a real shadow on Linen, nothing on the dark schemes
    // (owner, L2). Only now, never during the grow: a window-sized shadow
    // redrawn every frame is the most expensive thing the animation could
    // carry (card-sheet 8).
    lv_obj_add_style(s.win, UI::paint(UIPaint::PAINT_LIFT), 0);
    buildContents();
    s.lastTouchMs = millis();
    s.lastAgeMs   = s.lastTouchMs;
    s.timer = lv_timer_create(tickCb, POPUP_TICK_MS, nullptr);
}

void shrinkDone(lv_anim_t *a) {
    (void)a;
    dbgReport("shrink");
    if (s.win)   { lv_obj_delete(s.win);   s.win = nullptr; }
    // The dim goes last, in one redraw, so the page never shows undimmed
    // around a window that is still on its way back.
    if (s.scrim) { lv_obj_delete(s.scrim); s.scrim = nullptr; }
    s.phase = PopupPhase::PHASE_CLOSED;
    s.nEnt = 0;
}

// The deferred half of close(). Runs from LVGL's own timer handler, outside any
// event callback, so deleting the window's children here is safe.
void closeNow(void *unused) {
    (void)unused;
    s.closeQueued = false;
    if (s.phase != PopupPhase::PHASE_GROWING && s.phase != PopupPhase::PHASE_OPEN) return;
    if (!s.win) { shrinkDone(nullptr); return; }

    lv_anim_delete(s.win, animExec);    // a close during the grow reverses it
    if (s.timer) { lv_timer_delete(s.timer); s.timer = nullptr; }

    // The contents go first: the frame shrinks back empty, as it grew. And so
    // does the shadow, for the same reason it waited until the grow was done.
    lv_obj_clean(s.win);
    forgetWidgets();
    lv_obj_set_style_pad_all(s.win, 0, 0);
    lv_obj_remove_style(s.win, UI::paint(UIPaint::PAINT_LIFT), 0);

    lv_obj_get_coords(s.win, &s.animA);
    s.animB = s.cardRect;
    s.radA  = lv_obj_get_style_radius(s.win, LV_PART_MAIN);
    s.radB  = s.cardRadius;
    s.phase = PopupPhase::PHASE_SHRINKING;
    startAnim(POPUP_SHRINK_MS, lv_anim_path_ease_in, shrinkDone);
}

void pressCb(lv_event_t *ev) {
    (void)ev;
    lv_indev_t *in = lv_indev_active();
    if (in) lv_indev_get_point(in, &s.pressStart);
    s.lastTouchMs = millis();
}

void scrimClickCb(lv_event_t *ev) { (void)ev; CardPopup::close(); }

} // namespace

// ---------------------------------------------------------------------------
// Public
// ---------------------------------------------------------------------------

void CardPopup::begin() {
    // Every press, whatever it lands on - the same reason GUIManager hooks the
    // input device rather than the screen (screenPressCb()).
    if (lv_indev_t *in = lv_indev_get_next(nullptr)) {
        lv_indev_add_event_cb(in, pressCb, LV_EVENT_PRESSED, nullptr);
    }
}

bool CardPopup::isOpen() { return s.phase != PopupPhase::PHASE_CLOSED; }

void CardPopup::close() {
    if (s.closeQueued) return;
    if (s.phase != PopupPhase::PHASE_GROWING && s.phase != PopupPhase::PHASE_OPEN) return;
    s.closeQueued = true;
    lv_async_call(closeNow, nullptr);
}

void CardPopup::open(Card &card) {
    if (s.phase != PopupPhase::PHASE_CLOSED) return;
    if (!card._surface || !card._nPrimary) return;

    // A FINGER THAT MOVED IS A DRAG, NOT A LONG PRESS (card-sheet 7). LVGL
    // fires LONG_PRESSED for a slow drag that never crossed the gesture
    // distance; it must not open a window.
    lv_indev_t *in = lv_indev_active();
    if (in) {
        lv_point_t now;
        lv_indev_get_point(in, &now);
        const int32_t dx = now.x - s.pressStart.x, dy = now.y - s.pressStart.y;
        const int32_t lim = mm(4);
        if (dx * dx + dy * dy > lim * lim) return;
        // Release the touch BEFORE building anything (LESSONS): the finger is
        // still down, and its release must not land on the new scrim.
        lv_indev_wait_release(in);
    }

    // --- Copy what the window needs; never keep the card -------------------
    s.nEnt = 0;
    for (uint8_t i = 0; i < card._nPrimary && i < CARD_PRIMARY_MAX; i++) {
        if (card._primary[i]) s.ent[s.nEnt++] = card._primary[i];
    }
    if (!s.nEnt) return;
    s.reg      = Card::s_reg;
    s.tempUnit = card.tempUnit();
    snprintf(s.area, sizeof(s.area), "%s", card._area);
    snprintf(s.name, sizeof(s.name), "%s", card.label());
    lv_obj_get_coords(card._surface, &s.cardRect);
    s.cardRadius = UI::sc(UI::met().RADIUS);

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

    const UIPalette &p = UI::pal();

    // --- The dim: at once, not faded (card-sheet 13) ------------------------
    // One full-screen redraw instead of one per frame of a fade (~85 ms each on
    // P4_5). Tapping it is "tap outside", the second close route.
    s.scrim = plain(lv_layer_top());
    lv_obj_set_size  (s.scrim, sw, sh);
    lv_obj_set_pos   (s.scrim, 0, 0);
    lv_obj_set_style_bg_color(s.scrim, UI::c(p.SCRIM), 0);
    lv_obj_set_style_bg_opa  (s.scrim, POPUP_SCRIM_OPA, 0);
    lv_obj_add_flag  (s.scrim, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(s.scrim, scrimClickCb, LV_EVENT_CLICKED, nullptr);

    // --- The frame, starting as the card -----------------------------------
    // The scheme's raised surface (the mock's window is Midnight's SURFACE_ALT
    // exactly), a hairline from the border token, and NO shadow and no
    // clip_corner: both would cost a window-sized layer (card-sheet 8).
    // Clickable, so a tap inside never falls through to the scrim and closes.
    s.win = plain(lv_layer_top());
    lv_obj_add_flag(s.win, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_bg_color    (s.win, UI::c(p.SURFACE_ALT), 0);
    lv_obj_set_style_bg_opa      (s.win, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(s.win, UI::border(), 0);
    lv_obj_set_style_border_width(s.win, UI::met().BORDER_W ? UI::met().BORDER_W : 1, 0);

    s.animA = s.cardRect;  s.radA = s.cardRadius;
    s.animB = s.winRect;   s.radB = s.winRadius;
    animExec(s.win, 0);

    // DRAW THE DIM NOW, BEFORE THE CLOCK STARTS. The dim is a full-screen
    // redraw (~90 ms on P4_5); left to the next refresh it lands in the
    // animation's first frame and eats half of a 200 ms grow - the owner saw
    // "only a few frames" (O2). Paying it here, synchronously, means the grow
    // starts from a screen that is already dim.
    const uint32_t tDim = millis();
    lv_refr_now(nullptr);
    DBG_POPUP("dim frame: %lu ms\n", (unsigned long)(millis() - tDim));
    (void)tDim;
#ifdef DEBUG_POPUP
    s_dbgN = 0;
#endif

    s.view        = PopupView::VIEW_MAIN;
    s.closeQueued = false;
    s.phase       = PopupPhase::PHASE_GROWING;
    startAnim(POPUP_GROW_MS, lv_anim_path_ease_out, growDone);
}
