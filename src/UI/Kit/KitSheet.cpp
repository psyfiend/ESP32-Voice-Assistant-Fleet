// The sheet's frame (2.12 P4, moved from the card window - CardPopup.cpp, 2.10a-2.10e): where it
// goes, the window, its header row and stage, the auto-close, the quiet build, the tap catcher,
// and closing. The panels are KitSheetPanels.cpp, the sparks KitSparks.cpp.
//
// USABLE FIRST (owner, 2026-10-04 evening). The window used to grow out of the card and land with
// the dim; every version of that was measured, and the motion was never the slow part - the frame
// it landed in was (a full-screen redraw for the dim, ~120 ms on Midnight, ~200 ms on Linen, after
// ~240 ms of growing). A sheet appears complete in the first frame and closes in one. What was
// learned is in docs/LESSONS.md, "Effects that cover the screen". Nothing is drawn behind it.
#include "KitSheetInternal.h"
#include "UI/UIToolkit.h"
#include "Settings.h"
#include "src/core/lv_obj_event_private.h"   // lv_hit_test_info_t: the tap catcher's hit test
#include "src/core/lv_obj_draw_private.h"    // lv_obj_get_ext_draw_size: the window's shadow
#include <Arduino.h>
#include <math.h>
#include <string.h>

namespace Kit {

Sheet *s_open = nullptr;

// THE TAP CATCHER: "tap outside" and the modal rule, with nothing drawn. A
// transparent full-screen object on lv_layer_top(), made once at sheetBegin()
// and never deleted, hidden or moved - each of those would invalidate the whole
// screen (LESSONS, "Never toggle HIDDEN on a screen-sized object"). It is
// switched by CLICKABLE alone, which costs no redraw. Its hit test says "not
// here" inside the window, so LVGL goes on to search the screen, where the
// window is (pointer_search_obj() in lv_indev.c: system layer, top layer, then
// the screen).
static lv_obj_t *s_catcher = nullptr;
static bool      s_catcherFolded = false;   // this touch, on the catcher, folded a panel: its click is spent

// THE P4_5 IS THE REFERENCE (owner, 2026-10-05: its toggle "is the perfect
// size"). Its window is 787 x 545 px with the system header showing; what is
// inside a sheet is sized as the same share of the biggest window of THIS shape
// that fits in it - see propH.
constexpr float SHEET_REF_ASPECT = 787.0f / 545.0f;

static bool inWindow(const Sheet &S, const lv_point_t &pt) {
    const lv_area_t &W = S.winRect;
    return pt.x >= W.x1 && pt.x <= W.x2 && pt.y >= W.y1 && pt.y <= W.y2;
}

// What the open sheet lets presses through to beside it (sheetBeside()).
static const SheetBeside *s_beside = nullptr;

void sheetBeside(const SheetBeside *b) { s_beside = b; }

static bool onBeside(const lv_point_t &pt) {
    if (!s_beside || !s_beside->holder || lv_obj_has_flag(s_beside->holder, LV_OBJ_FLAG_HIDDEN)) return false;
    const uint32_t n = lv_obj_get_child_count(s_beside->holder);
    for (uint32_t i = 0; i < n; i++) {
        lv_obj_t *k = lv_obj_get_child(s_beside->holder, i);
        if (lv_obj_has_flag(k, LV_OBJ_FLAG_HIDDEN)) continue;
        lv_area_t a;
        lv_obj_get_coords(k, &a);
        if (pt.x >= a.x1 && pt.x <= a.x2 && pt.y >= a.y1 && pt.y <= a.y2) return true;
    }
    return false;
}

static bool besideOpen() { return s_beside && s_beside->isOpen && s_beside->isOpen(); }

// The catcher's jobs: let presses on the window and its panels through, and
// close on a tap anywhere else. AND WHILE A PANEL IS OPEN, THE WINDOW IS OUT OF
// REACH (owner, round 10): the topmost thing on the screen is the panel, so a
// press anywhere in the window only folds it - it used to fold the panel and
// also press whatever lay under the finger, the X closing the window before
// the panel had finished folding, the back arrow leaving the view.
static void catcherCb(lv_event_t *ev) {
    Sheet *S = s_open;
    const lv_event_code_t code = lv_event_get_code(ev);
    if (code == LV_EVENT_HIT_TEST) {
        lv_hit_test_info_t *info = lv_event_get_hit_test_info(ev);
        if (!info || !S || !S->win) return;
        const lv_point_t &pt = *info->point;
        const bool panelOpen = sheetPanelOpen(*S) || besideOpen();
        if (sheetInPanel(*S, pt) || onBeside(pt) || (!panelOpen && inWindow(*S, pt))) info->res = false;
    } else if (code == LV_EVENT_PRESSED) {
        s_catcherFolded = false;
        if (S && (sheetPanelOpen(*S) || besideOpen()) && inWindow(*S, S->pressStart)) {
            s_catcherFolded = true;
            if (S->right.state == PANEL_OPEN) sheetRightSet(*S, PANEL_PEEK);
            sheetSideFold(*S);
            if (besideOpen() && s_beside->fold) s_beside->fold();
        }
    } else if (code == LV_EVENT_CLICKED) {
        if (s_catcherFolded) { s_catcherFolded = false; return; }
        if (S) sheetClose(*S);
    }
}

// Every press, wherever it lands: when the open sheet was last touched, and
// where, for its auto-close and its catcher. The input device hears PRESSED
// before the object does (send_event() in lv_indev.c).
static void touchCb(lv_event_t *ev) {
    (void)ev;
    if (!s_open) return;
    if (lv_indev_t *in = lv_indev_active()) lv_indev_get_point(in, &s_open->pressStart);
    s_open->lastTouchMs = millis();
}

void sheetBegin() {
    if (s_catcher) return;
    if (lv_indev_t *in = lv_indev_get_next(nullptr)) lv_indev_add_event_cb(in, touchCb, LV_EVENT_PRESSED, nullptr);
    s_catcher = plain(lv_layer_top());
    lv_obj_set_size(s_catcher, lv_pct(100), lv_pct(100));
    lv_obj_add_flag(s_catcher, LV_OBJ_FLAG_ADV_HITTEST);
    lv_obj_add_event_cb(s_catcher, catcherCb, LV_EVENT_HIT_TEST, nullptr);
    lv_obj_add_event_cb(s_catcher, catcherCb, LV_EVENT_PRESSED,  nullptr);
    lv_obj_add_event_cb(s_catcher, catcherCb, LV_EVENT_CLICKED,  nullptr);
}

Sheet *sheetShowing() { return s_open; }

// --- Where it goes ------------------------------------------------------------
// Tall: from just under the system header to just above the panels' tabs.
//
// WIDE: UP TO TWICE AS WIDE AS TALL, AND NEVER NEAR THE SCREEN'S SIDES (owner,
// 2026-10-06). Round 1 of 2.10b gave every board the P4_5 window's 787:545
// shape - itself inherited from the mock, which was drawn for the 4B - and that
// held the P4_5 to 68 mm on a 110 mm screen, its title ellipsised, with room to
// spare. Now 2:1, capped so a gap of at least W_SIDE_MIN or 8% of the screen's
// width stays on each side: P4_5 ~93 mm, the 7" panels ~129 mm, the 4B ~60 mm.
// The height used is the one WITH the system header showing, so hiding the
// header makes the window taller but never wider. The contents stay tied to
// the height (propH, below).
void sheetPlace(Sheet &S) {
    lv_obj_t *scr = lv_screen_active();
    const int32_t sw = lv_obj_get_width(scr), sh = lv_obj_get_height(scr);
    const UIShape &k = UI::shape();
    const int32_t top    = UIToolkit::systemHeaderPx() + UI::mm(k.S6);
    const int32_t bottom = sh - UI::mm(k.H_TABS) - UI::mm(k.S6);
    const int32_t refH   = bottom - (UIToolkit::systemHeaderFullPx() + UI::mm(k.S6));
    // AND NEVER WIDER THAN W_WINDOW_MAX, 100 mm (owner, round 5): on a 7" panel
    // the 2:1 window was ~129 mm, and every reach to the X or across the window
    // got longer while the space to tap outside it shrank. 100 mm is the P4_5's
    // window and a little - a hand's span.
    const int32_t side = LV_MAX(UI::mm(k.W_SIDE_MIN), sw * 8 / 100);
    int32_t w = LV_MIN(2 * refH, UI::mm(k.W_WINDOW_MAX));
    if (w > sw - 2 * side) w = sw - 2 * side;
    S.winRect.x1 = (sw - w) / 2;
    S.winRect.x2 = S.winRect.x1 + w - 1;
    S.winRect.y1 = top;
    S.winRect.y2 = bottom - 1;
    // What the contents are sized from: the height of the biggest window of
    // the P4_5's shape that fits inside this one. The P4_5 itself gives its own
    // height (with the header shown); the 7" panels theirs; the 4B, whose width
    // is capped, less than its height. So the contents are the same share of
    // the window everywhere (owner), and the 4B's shrink.
    S.propH     = LV_MIN(refH, (int32_t)lroundf((float)w / SHEET_REF_ASPECT));
    S.winRadius = UI::mm(k.R_WINDOW);
    // 1.2 mm, down from the mock's 2.2: the owner wanted the X "closer to the
    // corner" (H1). Still clear of the 2.4 mm corner radius.
    S.pad       = UI::mm(k.S4);
}

// HOW LONG A WINDOW WAITS, AND WHETHER IT SAYS SO (owner, T6, 2026-10-14):
// system settings, read as each window opens - "close" in seconds or "off",
// "bar" "off" to hide the countdown. Set from a PC until the system settings
// page (Settings.h).
static uint32_t closeSetting() {
    char v[8];
    if (!Settings::system("window", "close", v, sizeof(v))) return SHEET_AUTOCLOSE_MS;
    if (!strcmp(v, "off")) return 0;
    const int sec = atoi(v);
    return sec > 0 ? (uint32_t)sec * 1000 : SHEET_AUTOCLOSE_MS;
}

static bool barSetting() {
    char v[8];
    return !Settings::system("window", "bar", v, sizeof(v)) || strcmp(v, "off") != 0;
}

uint32_t windowCloseMs()  { return closeSetting(); }
bool     windowBarShown() { return barSetting(); }

// --- The window ---------------------------------------------------------------
// ON THE SCREEN AS ITS TOPMOST OBJECT - not lv_layer_top(). LVGL draws the top
// layer OVER the page, never instead of it (lv_refr.c), so with the window up
// there every redraw inside it - opening, a toggle, the countdown bar - drew the
// cards beneath it first. On the screen, an opaque window is what
// lv_refr_get_top_obj() finds covering those areas, and LVGL draws only the
// window. Created last, so it is above the header, the deck and the drawer; a
// page rebuild moves only the cards to the back.
//
// The scheme's raised surface and border (KS_SHEET_FACE), and THE HALO (D-35,
// 2.11): every window floats over the page on a black rim fading to clear, in
// place of the scheme's lift (Linen's shadow). No dim (its cost: a full-screen
// redraw to open and one to close, 2.10a). No clip_corner: it would cost a
// window-sized layer (card-sheet 8). Clickable, so a tap inside never reaches
// the catcher.
static void makeWindow(Sheet &S) {
    S.win = plain(lv_screen_active());
    lv_obj_add_flag(S.win, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_pos (S.win, S.winRect.x1, S.winRect.y1);
    lv_obj_set_size(S.win, lv_area_get_width(&S.winRect), lv_area_get_height(&S.winRect));
    add(S.win, KitStyle::KS_SHEET_FACE);
    haloAttach(S.win);
}

// D6's drag down, on the HEADER ROW only (card-sheet 13): the light's tall
// slider would fight a drag that could start anywhere. How the gesture gets
// here is in buildFrame() - it is not the obvious way.
static void headerGestureCb(lv_event_t *ev) {
    (void)ev;
    lv_indev_t *in = lv_indev_active();
    if (!in || lv_indev_get_gesture_dir(in) != LV_DIR_BOTTOM) return;
    lv_indev_wait_release(in);   // before the action - LESSONS
    if (s_open) sheetClose(*s_open);
}

static void buildFrame(Sheet &S) {
    const SheetSpec &spec = *S.spec;
    const UIType  &t = UI::type();
    const UIShape &k = UI::shape();
    const int32_t btn = UI::minTouch();
    const int32_t gap = UI::mm(k.S5);

    lv_obj_set_flex_flow(S.win, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(S.win, S.pad, 0);
    lv_obj_set_style_pad_row(S.win, UI::mm(k.S5), 0);

    // --- Header row --------------------------------------------------------
    lv_obj_t *hdr = plain(S.win);
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
    if (spec.headerLongCb) lv_obj_add_event_cb(hdr, spec.headerLongCb, LV_EVENT_LONG_PRESSED, nullptr);

    // Both ends the same width, so the title sits in the true middle.
    const uint8_t nRight = spec.nTabs ? spec.nTabs : 1;
    const int32_t slotW  = btn * nRight + gap * (nRight - 1);

    lv_obj_t *left = plain(hdr);
    lv_obj_set_size(left, slotW, btn);
    S.titleLeft  = left;
    S.titleSlotW = slotW;
    S.titleSlack = slotW - btn;
    // The X and the back arrow are LVGL's own symbols, which every built-in
    // Montserrat carries; the hero face is the largest one already linked.
    S.btnLeft = iconButton(left, LV_SYMBOL_CLOSE, UIToolkit::Font_Hero, spec.leftCb, &S.lblLeft);

    lv_obj_t *title = plain(hdr);
    lv_obj_set_height   (title, btn);
    lv_obj_set_flex_grow(title, 1);
    lv_obj_set_flex_flow(title, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(title, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    S.lblTitleArea = label(title, KitStyle::KS_LABEL_NAME_DIM);
    S.lblTitleName = label(title, KitStyle::KS_LABEL_NAME);
    // ONE LINE EACH, ellipsised, at widths sheetLayoutTitle() works out per
    // view. A DOT label with a content height wraps instead of ellipsising,
    // which is how "Members" ended up on two lines and "All Lamps >" overflowed
    // the centred row and lost its left half (owner's V3).
    const int32_t lh = lv_font_get_line_height(t.NAME);
    lv_label_set_long_mode(S.lblTitleArea, LV_LABEL_LONG_DOT);
    lv_label_set_long_mode(S.lblTitleName, LV_LABEL_LONG_DOT);
    lv_obj_set_height(S.lblTitleArea, lh);
    lv_obj_set_height(S.lblTitleName, lh);
    S.titleW = lv_area_get_width(&S.winRect) - 2 * S.pad - 2 * slotW - 2 * gap;
    // The first part is a link: the whole header row's height takes the tap,
    // and it brightens while pressed. Gestures still bubble to the row, so a
    // drag down that starts on it still closes the window.
    if (spec.crumbCb) {
        lv_obj_add_flag(S.lblTitleArea, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_set_ext_click_area(S.lblTitleArea, (btn - lh) / 2);
        lv_obj_set_style_text_color(S.lblTitleArea, UI::c(UI::pal().TEXT), UI::part(LV_PART_MAIN, LV_STATE_PRESSED));
        lv_obj_add_event_cb(S.lblTitleArea, spec.crumbCb, LV_EVENT_CLICKED, nullptr);
    }

    lv_obj_t *right = plain(hdr);
    lv_obj_set_size     (right, slotW, btn);
    lv_obj_set_flex_flow(right, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(right, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(right, gap, 0);
    // The row is end-aligned, so the last made is the one in the corner.
    for (uint8_t i = 0; i < spec.nTabs && i < SHEET_TABS_MAX; i++)
        S.tab[i] = iconButton(right, spec.tabGlyph[i], t.ICON_MD, spec.tabCb[i]);

    // --- The stage: whichever view is showing -------------------------------
    S.stage = plain(S.win);
    lv_obj_set_width    (S.stage, lv_pct(100));
    lv_obj_set_flex_grow(S.stage, 1);
    lv_obj_set_flex_flow(S.stage, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(S.stage, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    // --- A floating tab under the right slot (the card window's Scenes) ------
    // Out of the flow, in the corner of the window's contents, a header row
    // and its gap down - so it never pushes the stage. Hidden until the client
    // shows it.
    if (spec.floatGlyph) {
        S.floatTab = iconButton(S.win, spec.floatGlyph, t.ICON_MD, spec.floatCb);
        lv_obj_add_flag(S.floatTab, LV_OBJ_FLAG_FLOATING);
        lv_obj_align(S.floatTab, LV_ALIGN_TOP_RIGHT, 0, btn + UI::mm(k.S5));
        lv_obj_add_flag(S.floatTab, LV_OBJ_FLAG_HIDDEN);
    }

    // --- The auto-close bar: how long until the window gives up -------------
    const bool bar = spec.bar ? spec.bar == 1 : barSetting();
    S.autoBar = (S.closeMs && bar) ? countdownBar(S.win, lv_area_get_width(&S.winRect)) : nullptr;

    if (spec.buildFirst) spec.buildFirst();
}

// THE COUNTDOWN BAR ON THE BOTTOM EDGE (owner, TEST_2.12; #97): H_BAR thin - 2 px
// at 294 and 255 PPI, 1 px at 170, where it was 6 and 3 - and down on the box's
// edge inside its border, so the gap to what the box holds is its whole bottom
// padding. Between the rounded corners, so it never crosses their curve. Its
// full width is kept in its user data.
lv_obj_t *countdownBar(lv_obj_t *box, int32_t boxW) {
    const int32_t full = LV_MAX(1, boxW - 2 * lv_obj_get_style_radius(box, LV_PART_MAIN));
    lv_obj_t *bar = plain(box);
    lv_obj_add_flag(bar, LV_OBJ_FLAG_FLOATING);
    lv_obj_set_user_data(bar, (void *)(intptr_t)full);
    lv_obj_set_size(bar, full, LV_MAX(1, UI::mm(UI::shape().H_BAR)));
    add(bar, KitStyle::KS_SHEET_BAR);
    lv_obj_align(bar, LV_ALIGN_BOTTOM_MID, 0, lv_obj_get_style_pad_bottom(box, LV_PART_MAIN));
    return bar;
}

// A NEW TIMEOUT OR BAR, AT ONCE (owner, round 4): the wait starts again from
// now, and the bar comes or goes with it.
void sheetRetime(Sheet &S, uint32_t closeMs, bool bar) {
    if (!S.win) return;
    S.closeMs     = closeMs;
    S.lastTouchMs = millis();
    const bool want = closeMs && bar;
    if (want && !S.autoBar) S.autoBar = countdownBar(S.win, lv_area_get_width(&S.winRect));
    else if (!want && S.autoBar) { lv_obj_delete(S.autoBar); S.autoBar = nullptr; }
    countdownSet(S.autoBar, S);
}

void countdownSet(lv_obj_t *bar, const Sheet &S) {
    if (!bar || !S.closeMs) return;
    const int32_t full = (int32_t)(intptr_t)lv_obj_get_user_data(bar);
    const int32_t w = LV_MAX(1, (int32_t)((int64_t)full * LV_MIN(sheetLeftMs(S), S.closeMs) / S.closeMs));
    if (w != lv_obj_get_width(bar)) lv_obj_set_width(bar, w);   // its align keeps it in the middle
}

uint32_t sheetLeftMs(const Sheet &S) {
    if (!S.closeMs) return UINT32_MAX;
    const uint32_t idle = millis() - S.lastTouchMs;
    return idle >= S.closeMs ? 0 : S.closeMs - idle;
}

// Share the title row between its first part ("Area > ") and the name.
//  - Both fit in the middle, between ends of equal width: centred, as always.
//  - They fit if the title takes the room beside the left button (a window
//    with two tabs on the right has a tab's width spare on the left): the
//    left slot shrinks to its button, and the title sits left of centre
//    (owner, round 7).
//  - Neither: THE NAME ALONE (owner, round 7, the 4B: "Offi...Desk" was hard
//    to read). The link back goes with it; the back arrow still does that job.
void sheetLayoutTitle(Sheet &S) {
    if (!S.lblTitleArea || !S.lblTitleName) return;
    const lv_font_t *f = UI::type().NAME;
    const char *area = lv_label_get_text(S.lblTitleArea);
    const int32_t aw = (area && area[0]) ? textW(area, f) + 1 : 0;
    int32_t nw = textW(lv_label_get_text(S.lblTitleName), f) + 1;
    bool crumb = true, wide = false;
    if (aw + nw > S.titleW) {
        if (aw + nw <= S.titleW + S.titleSlack) wide = true;
        else { crumb = false; wide = nw > S.titleW; }
    }
    const int32_t room = S.titleW + (wide ? S.titleSlack : 0);
    if (S.titleLeft) lv_obj_set_width(S.titleLeft, S.titleSlotW - (wide ? S.titleSlack : 0));
    if (nw > room) nw = room;
    if (crumb && aw) {
        lv_obj_clear_flag(S.lblTitleArea, LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_width(S.lblTitleArea, aw);
    } else {
        lv_obj_add_flag(S.lblTitleArea, LV_OBJ_FLAG_HIDDEN);
    }
    lv_obj_set_width(S.lblTitleName, nw);
}

// THE TABS WORK AS TABS (owner, 2026-10-06): shown on every view they belong
// to, the one showing lit (as a chosen selector button is).
void sheetTabLook(lv_obj_t *tab, bool shown, bool lit) {
    if (!tab) return;
    if (!shown) { lv_obj_add_flag(tab, LV_OBJ_FLAG_HIDDEN); return; }
    lv_obj_clear_flag(tab, LV_OBJ_FLAG_HIDDEN);
    iconButtonLit(tab, lit);
}

// --- Its clock: the auto-close and the bar, then the client's -----------------
static void tickCb(lv_timer_t *t) {
    Sheet *S = (Sheet *)lv_timer_get_user_data(t);
    if (!S || !S->win) return;
    const uint32_t now  = millis();
    if (!sheetLeftMs(*S)) { sheetClose(*S); return; }

    countdownSet(S->autoBar, *S);
    if (S->spec && S->spec->tick) S->spec->tick(now);
}

// What the first frame must draw: the window's own rectangle, and - only where
// the scheme gives it a shadow - four thin bands around it. NOT one area of the
// window plus its shadow: LVGL draws an area in strips as wide as the area,
// and a strip that sticks out past the window is not covered by it, so the
// cards under the whole window would be drawn first (LESSONS, "Effects that
// cover the screen").
static void invalidateWindow(Sheet &S) {
    lv_obj_t *scr = lv_screen_active();
    const lv_area_t &W = S.winRect;
    lv_obj_invalidate_area(scr, &W);
    const int32_t e = lv_obj_get_ext_draw_size(S.win);
    if (e <= 0) return;
    const lv_area_t bands[4] = {
        { W.x1 - e, W.y1 - e, W.x2 + e, W.y1 - 1 },   // above
        { W.x1 - e, W.y2 + 1, W.x2 + e, W.y2 + e },   // below
        { W.x1 - e, W.y1,     W.x1 - 1, W.y2     },   // left
        { W.x2 + 1, W.y1,     W.x2 + e, W.y2     },   // right
    };
    for (const lv_area_t &b : bands) lv_obj_invalidate_area(scr, &b);
}

// Built, made modal, its clock started, SETTINGS' tab sliding up and the
// sparks waiting their turn.
//
// BUILT QUIET (2026-10-05). The window's frame took ~77 ms where ~25 was
// expected, and DEBUG_POPUP showed why: it redrew 0,0..1032,627, not the
// window. LVGL lays out a new object's SIZE before its POSITION
// (lv_obj_refr_size, then lv_obj_refr_pos), so for one step the full-size
// window sat at (0,0) and that area was invalidated too; joined with the real
// one, it covered the cards under the window and the header, and every strip
// of it was wider than the window. So: everything is made and laid out with
// invalidation off, and then exactly what must be drawn is asked for.
void sheetOpen(Sheet &S, const SheetSpec &spec) {
    S.spec = &spec;
    S.closeQueued = false;
    S.closeMs = spec.closeMs == SHEET_NEVER ? 0 : spec.closeMs ? spec.closeMs : closeSetting();
    s_open = &S;
    facesNextWindow();                                // faces the last sheet used may be taken back
    lv_obj_t *scr = lv_screen_active();
    lv_obj_update_layout(scr);                        // settle anything pending, aloud
    lv_display_enable_invalidation(nullptr, false);
    makeWindow(S);
    buildFrame(S);
    if (spec.sparks) sparksCreate();
    if (spec.panelRows) rightCreate(S);
    lv_obj_update_layout(scr);
    lv_display_enable_invalidation(nullptr, true);
    invalidateWindow(S);

    if (s_catcher) lv_obj_add_flag(s_catcher, LV_OBJ_FLAG_CLICKABLE);
    S.lastTouchMs = millis();
    S.timer = lv_timer_create(tickCb, SHEET_TICK_MS, &S);
    if (spec.sparks) sparksStart();
    if (spec.panelRows) sheetRightSet(S, PANEL_PEEK);
}

// The deferred half of sheetClose(). Runs from LVGL's own timer handler,
// outside any event callback, so deleting the window's children here is safe.
// Everything goes in one frame.
static void closeAsync(void *p) {
    Sheet *S = (Sheet *)p;
    S->closeQueued = false;
    if (!S->win) return;
    const SheetSpec *spec = S->spec;
    if (spec && spec->closing) spec->closing();
    keyboardClose();   // words being typed for the window go with it, unsaved
    if (S->timer) { lv_timer_delete(S->timer); S->timer = nullptr; }
    sparksEnd();
    sheetSideEnd(*S);
    rightEnd(*S);
    lv_obj_delete(S->win);
    S->win = nullptr;
    S->btnLeft = S->lblLeft = S->floatTab = nullptr;
    for (lv_obj_t *&b : S->tab) b = nullptr;
    S->lblTitleArea = S->lblTitleName = S->titleLeft = nullptr;
    S->stage = S->autoBar = nullptr;
    if (s_catcher) lv_obj_clear_flag(s_catcher, LV_OBJ_FLAG_CLICKABLE);
    s_beside = nullptr;
    if (s_open == S) s_open = nullptr;
    if (spec && spec->closed) spec->closed();
}

void sheetClose(Sheet &S) {
    if (S.closeQueued || !S.win) return;
    S.closeQueued = true;
    lv_async_call(closeAsync, &S);
}

} // namespace Kit
