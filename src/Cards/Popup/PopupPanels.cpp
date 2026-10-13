// The folder panels (2.12 P1, split from CardPopup.cpp): SETTINGS on the right and the left panel
// (CHART, ACTIVITY or SCENES) - built, slid up and down, dragged, and gone with the window - and
// the controls in their rows. What the rows are is PopupSettingsRows.cpp and PopupSideRows.cpp.
#include "PopupState.h"

namespace popup {

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
// 260 ms, from 220 (owner, round 7: choppier now that the panel opens further;
// the page deck's panels take 300). Try 300 if it still stutters.
constexpr uint32_t DECK_OPEN_MS = 260;
// A row is UI::shape().H_ROW, 7.2 mm - from 8: the folder tab grew by twice its
// curve (round 11), and the rows paid for it, so an open deck still reaches the
// same height on the window.

void deckExec(void *var, int32_t v) { lv_obj_set_y((lv_obj_t *)var, v); }

// ---------------------------------------------------------------------------
// DRAGGING A PANEL (owner, 2.10d P1b round 2). A panel is a sheet that slides
// up to show itself; rows sliding about inside it broke that. So an open panel
// taller than the room above the screen's bottom is moved WHOLE by the finger:
// up, to bring its hidden rows on screen (it stays where it is let go, at most
// until its last row is in sight), and down, to fold it - let go more than a
// third of the way to its tab and it folds, less and it goes back up, like the
// light switches' handle. A caret at the screen's bottom says there are rows
// below; one at the top, that the panel has gone off the top. A folded tab is
// still opened by a tap only.
//
// HOW: the panel's holder sits in a TRACK - transparent, not clickable, the
// panel's width and the screen's height - and it is the track that scrolls,
// so LVGL's own scrolling does the dragging: a drag that starts on a dropdown
// or checkbox moves the panel and does not open or tick it. The slides still
// move the holder itself; the track scrolls only while the panel rests open
// ("armed"). Armed, the holder is put lower by the distance from open to
// folded, and the track scrolled by the same, so nothing moves on the glass
// and there is room to drag it down. No momentum and no elastic: it stops dead.
// ---------------------------------------------------------------------------

// Where an open panel of height h rests: just tall enough to show its last
// row, or - taller than the room - its tab's top on the window's top edge
// (owner, 2026-10-07: never above the window when it opens).
int32_t panelRestY(int32_t h) {
    const int32_t sh = lv_obj_get_height(lv_screen_active());
    return LV_MAX(s.winRect.y1, sh - (h - s.deckHide));
}

int32_t panelPeekY() { return lv_obj_get_height(lv_screen_active()) - s.deckHead; }

bool panelArmed(lv_obj_t *track) { return track && lv_obj_has_flag(track, LV_OBJ_FLAG_SCROLLABLE); }

// The carets, from where the panel stands: more rows below the screen; the
// pane gone off the top.
void panelCarets(lv_obj_t *track, lv_obj_t *root, lv_obj_t *pane, lv_obj_t *up, lv_obj_t *down) {
    if (!track || !root || !pane || !up || !down) return;
    const bool armed = panelArmed(track);
    const bool below = armed && lv_obj_get_scroll_bottom(track) > 0;
    const bool above = armed && lv_obj_get_y(root) - lv_obj_get_scroll_y(track) + lv_obj_get_y(pane) < 0;
    if (lv_obj_has_flag(down, LV_OBJ_FLAG_HIDDEN) == below) {
        if (below) lv_obj_clear_flag(down, LV_OBJ_FLAG_HIDDEN); else lv_obj_add_flag(down, LV_OBJ_FLAG_HIDDEN);
    }
    if (lv_obj_has_flag(up, LV_OBJ_FLAG_HIDDEN) == above) {
        if (above) lv_obj_clear_flag(up, LV_OBJ_FLAG_HIDDEN); else lv_obj_add_flag(up, LV_OBJ_FLAG_HIDDEN);
    }
}

// Resting open: the track takes over (see above).
void panelArm(lv_obj_t *track, lv_obj_t *root, int32_t h) {
    if (!track || panelArmed(track)) return;
    const int32_t rest = panelRestY(h), room = panelPeekY() - rest;
    lv_obj_set_y(root, rest + room);
    lv_obj_add_flag(track, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_scroll_to_y(track, room, LV_ANIM_OFF);   // brings the layout up to date first
}

// Before a slide: the holder back where it shows, the track at rest. Returns
// the holder's y to slide from.
int32_t panelDisarm(lv_obj_t *track, lv_obj_t *root) {
    if (!panelArmed(track)) return lv_obj_get_y(root);
    lv_anim_delete(track, nullptr);   // a slide back up, under way
    lv_obj_update_layout(track);
    const int32_t y = lv_obj_get_y(root) - lv_obj_get_scroll_y(track);
    lv_obj_scroll_to_y(track, 0, LV_ANIM_OFF);
    lv_obj_clear_flag(track, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_y(root, y);
    return y;
}

void panelTrackCb(lv_event_t *ev);   // with CHART, below: it serves both panels

int32_t deckY(uint8_t state) {
    return state == DECK_OPEN ? panelRestY(s.deckH) : state == DECK_PEEK ? panelPeekY()
                              : lv_obj_get_height(lv_screen_active());
}

void deckArmCb(lv_anim_t *a) {
    (void)a;
    if (s.deckState != DECK_OPEN) return;
    panelArm(s.deckTrack, s.deck, s.deckH);
    panelCarets(s.deckTrack, s.deck, s.deckPane, s.deckUp, s.deckDown);
}

void deckSet(uint8_t state) {
    if (!s.deck) return;
    const uint8_t was = s.deckState;
    s.deckState = state;
    // FOLDING, BACK TO THE TOP (owner, 2.10d): disarming puts a panel dragged
    // up back in one piece, so nothing of its rows shows in the tab's strip.
    const int32_t from = panelDisarm(s.deckTrack, s.deck);
    panelCarets(s.deckTrack, s.deck, s.deckPane, s.deckUp, s.deckDown);
    // Open, on top of the other panel, which folds to its tab.
    if (state == DECK_OPEN) { lv_obj_move_foreground(s.deckTrack); chartFold(); }
    lv_anim_delete(s.deck, deckExec);
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var     (&a, s.deck);
    lv_anim_set_values  (&a, from, deckY(state));
    lv_anim_set_duration(&a, (was == DECK_HIDDEN) ? DECK_PEEK_MS : DECK_OPEN_MS);
    lv_anim_set_path_cb (&a, lv_anim_path_ease_out);
    lv_anim_set_exec_cb (&a, deckExec);
    if (state == DECK_OPEN) lv_anim_set_completed_cb(&a, deckArmCb);
    lv_anim_start(&a);
}

// ---------------------------------------------------------------------------
// THE SECOND PANEL: CHART (owner, round 7 - a demo). The left half, where
// round 9 kept room for it: up from the bottom as SETTINGS is, but only while
// the chart (History) is showing, and back down when the window goes to
// another view. It is made the first time the chart shows and lives with the
// window, and goes with it as SETTINGS does (chartEnd()).
//
// SETTINGS' mirror image, built by the same buildFolder() (round 7, after the
// owner's N9: "I want to see how the tab effect works"), with demo rows so its
// pane is wider and taller than its tab. Its tab stops a millimetre short of
// SETTINGS' (owner, round 7): two tabs side by side read as two.
// ---------------------------------------------------------------------------
// THE LEFT PANEL IS ALSO SCENES (2.10d, DECISIONS K43). History and Scenes are
// never showing at once, so one panel in the left half serves both: CHART's
// rows on History, SCENES' on Scenes. A different view's panel is ended and
// made afresh (showView()).
ChartPanel s_chart;
uint8_t    s_sideWant = SIDE_CHART;   // what chartCreate() makes next

int32_t chartY(uint8_t state) {
    return state == DECK_OPEN ? panelRestY(s_chart.h) : state == DECK_PEEK ? panelPeekY()
                              : lv_obj_get_height(lv_screen_active());
}

void chartExec(void *var, int32_t v) { lv_obj_set_y((lv_obj_t *)var, v); }

// Either panel open: the window is out of reach until it folds.
bool panelOpen() { return s.deckState == DECK_OPEN || s_chart.state == DECK_OPEN; }

// On a panel's tab or its pane, as they stand right now? A panel's own object
// is transparent and as wide as the window: a press beside its tab is a press
// on the page, which the catcher must take.
bool inDeck(const lv_point_t &pt) {
    auto in = [&](lv_obj_t *o) {
        if (!o) return false;
        lv_area_t a;
        lv_obj_get_coords(o, &a);
        return pt.x >= a.x1 && pt.x <= a.x2 && pt.y >= a.y1 && pt.y <= a.y2;
    };
    if (s_chart.root && s_chart.state != DECK_HIDDEN && (in(s_chart.tab) || in(s_chart.pane))) return true;
    if (!s.deck || s.deckState == DECK_HIDDEN) return false;
    return in(s.deckTab) || in(s.deckPane);
}

void deckFill();
void deckOpen();

void deckTabCb(lv_event_t *ev) {
    (void)ev;
    if (s.deckState == DECK_OPEN) deckSet(DECK_PEEK);
    else                          deckOpen();
}

int32_t deckCtlH() { return mm(UI::shape().H_CONTROL); }

// The widest line of a dropdown's choices.
int32_t widestLine(const char *opts, const lv_font_t *f) {
    char line[48];
    int32_t w = 0;
    while (opts && *opts) {
        const char *e = strchr(opts, '\n');
        const size_t len = e ? (size_t)(e - opts) : strlen(opts);
        const size_t k = len < sizeof(line) - 1 ? len : sizeof(line) - 1;
        memcpy(line, opts, k);
        line[k] = '\0';
        w = LV_MAX(w, textW(line, f));
        opts = e ? e + 1 : nullptr;
    }
    return w;
}

int32_t deckCtlW(const DeckRowSpec &r) {
    const UIShape &k = UI::shape();
    if (r.kind == DeckRowKind::ROW_CHECK) return deckCtlH() + mm(k.S3);
    const lv_font_t *f = UI::type().TAG;
    if (r.kind == DeckRowKind::ROW_BUTTON) return textW(r.opts, f) + 2 * mm(k.S7);
    return widestLine(r.opts, f) + 2 * mm(k.S5) + textW(LV_SYMBOL_DOWN, f) + mm(k.S5);
}

// The pane's sides are UI::shape().S7 (2.4 mm).

// AS WIDE AS ITS LONGEST ROW (owner): each row's words, a gap, its control.
int32_t rowsNeedW(const DeckRowSpec *r, uint8_t n) {
    int32_t w = 0;
    for (uint8_t i = 0; i < n; i++)
        w = LV_MAX(w, textW(r[i].label, UI::type().NAME) + mm(UI::shape().S9) + deckCtlW(r[i]));
    return w + 2 * mm(UI::shape().S7);
}

int32_t deckNeedW() {
    DeckRowSpec r[DECK_ROWS_MAX];
    return rowsNeedW(r, deckSpecs(r));
}

lv_obj_t *deckDropdown(lv_obj_t *parent, const DeckRowSpec &r) {
    const UIPalette &p = UI::pal();
    const UIType    &t = UI::type();
    lv_obj_t *dd = lv_dropdown_create(parent);
    lv_dropdown_set_options(dd, r.opts);
    lv_dropdown_set_selected(dd, r.sel);
    lv_obj_set_size(dd, deckCtlW(r), deckCtlH());
    const int32_t lh = lv_font_get_line_height(t.TAG);
    lv_obj_set_style_text_font   (dd, t.TAG, 0);
    lv_obj_set_style_text_font   (dd, t.TAG, LV_PART_INDICATOR);
    lv_obj_set_style_text_color  (dd, UI::c(r.live ? p.TEXT : p.TEXT_DIM), 0);
    lv_obj_set_style_bg_color    (dd, UI::c(p.SURFACE), 0);
    lv_obj_set_style_bg_opa      (dd, LV_OPA_COVER, 0);
    lv_obj_set_style_radius      (dd, mm(UI::shape().R_CONTROL), 0);
    lv_obj_set_style_border_width(dd, LV_MAX(1, mm(UI::shape().LINE_HAIR)), 0);
    lv_obj_set_style_border_color(dd, UI::c(r.live ? p.ACCENT : UI::mix(p.SURFACE_ALT, p.TEXT, 25)), 0);
    lv_obj_set_style_pad_hor     (dd, mm(UI::shape().S5), 0);
    lv_obj_set_style_pad_ver     (dd, LV_MAX(0, (deckCtlH() - lh) / 2 - 1), 0);
    lv_obj_set_style_shadow_width(dd, 0, 0);
    if (lv_obj_t *list = lv_dropdown_get_list(dd)) {
        lv_obj_set_style_text_font   (list, t.TAG, 0);
        lv_obj_set_style_text_color  (list, UI::c(p.TEXT), 0);
        lv_obj_set_style_bg_color    (list, UI::c(p.SURFACE), 0);
        lv_obj_set_style_border_color(list, UI::c(p.ACCENT), 0);
        lv_obj_set_style_border_width(list, LV_MAX(1, mm(UI::shape().LINE_HAIR)), 0);
        lv_obj_set_style_radius      (list, mm(UI::shape().R_CONTROL), 0);
        lv_obj_set_style_shadow_width(list, 0, 0);
        // EVERY CHOICE IN SIGHT (owner, 2.10d): the default theme caps a list
        // at 260 px, which on the P4_5 and the 4B is five rows - Tap action's
        // Toggle sat above the top, scrolled out of view. Only the screen
        // limits it now (lv_dropdown_open() fits it above or below), and what
        // still has to scroll does not spring back.
        lv_obj_set_style_max_height  (list, lv_display_get_vertical_resolution(NULL), 0);
        UI::tameScroll(list);
        lv_obj_set_style_bg_color    (list, UI::c(p.ACCENT), UI::part(LV_PART_SELECTED, LV_STATE_CHECKED));
        lv_obj_set_style_text_color  (list, UI::c(UI::contrastOf(p.ACCENT, p.SURFACE_ALT, p.TEXT)),
                                      UI::part(LV_PART_SELECTED, LV_STATE_CHECKED));
        lv_obj_set_style_bg_color    (list, UI::c(UI::mix(p.SURFACE, p.TEXT, 20)),
                                      UI::part(LV_PART_SELECTED, LV_STATE_PRESSED));
    }
    if (!r.live) lv_obj_add_state(dd, LV_STATE_DISABLED);
    // A greyed row may come alive in the same window (rowLive(): the Tap scene
    // row), so its callback is attached anyway; a disabled dropdown sends none.
    if (r.cb) lv_obj_add_event_cb(dd, r.cb, LV_EVENT_VALUE_CHANGED, nullptr);
    return dd;
}

lv_obj_t *deckCheckbox(lv_obj_t *parent, const DeckRowSpec &r) {
    const UIPalette &p = UI::pal();
    lv_obj_t *cb = lv_checkbox_create(parent);
    lv_checkbox_set_text(cb, "");
    lv_obj_set_style_text_font   (cb, UI::type().NAME, 0);
    lv_obj_set_style_pad_column  (cb, 0, 0);
    // As tall as a dropdown (owner, S2: larger). The box is the font's line
    // height plus its own padding.
    const int32_t grow = LV_MAX(0, (deckCtlH() - lv_font_get_line_height(UI::type().NAME)) / 2);
    lv_obj_set_style_pad_all     (cb, grow, LV_PART_INDICATOR);
    lv_obj_set_style_radius      (cb, mm(UI::shape().R_CONTROL), LV_PART_INDICATOR);
    lv_obj_set_style_border_width(cb, LV_MAX(2, mm(UI::shape().LINE_MID)), LV_PART_INDICATOR);
    lv_obj_set_style_border_color(cb, UI::c(p.ACCENT), LV_PART_INDICATOR);
    lv_obj_set_style_bg_color    (cb, UI::c(p.SURFACE), LV_PART_INDICATOR);
    lv_obj_set_style_bg_opa      (cb, LV_OPA_COVER, LV_PART_INDICATOR);
    lv_obj_set_style_bg_color    (cb, UI::c(p.ACCENT), UI::part(LV_PART_INDICATOR, LV_STATE_CHECKED));
    lv_obj_set_style_text_color  (cb, UI::c(UI::contrastOf(p.ACCENT, p.SURFACE_ALT, p.TEXT)),
                                  UI::part(LV_PART_INDICATOR, LV_STATE_CHECKED));
    if (r.sel) lv_obj_add_state(cb, LV_STATE_CHECKED);
    if (r.cb) lv_obj_add_event_cb(cb, r.cb, LV_EVENT_VALUE_CHANGED, nullptr);
    return cb;
}

// A button in a row (2.10e: Entity properties), the dropdown's size and frame.
lv_obj_t *deckButton(lv_obj_t *parent, const DeckRowSpec &r) {
    const UIPalette &p = UI::pal();
    lv_obj_t *b = plain(parent);
    lv_obj_add_flag(b, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_size(b, deckCtlW(r), deckCtlH());
    lv_obj_set_style_radius      (b, mm(UI::shape().R_CONTROL), 0);
    lv_obj_set_style_bg_opa      (b, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color    (b, UI::c(p.SURFACE), 0);
    lv_obj_set_style_bg_color    (b, UI::c(UI::mix(p.SURFACE, p.TEXT, 15)), UI::part(LV_PART_MAIN, LV_STATE_PRESSED));
    lv_obj_set_style_border_width(b, LV_MAX(1, mm(UI::shape().LINE_HAIR)), 0);
    // Greyed (K17's "Added"): no accent, dim words, nothing to tap.
    lv_obj_set_style_border_color(b, UI::c(r.live ? p.ACCENT : UI::mix(p.SURFACE, p.TEXT, 25)), 0);
    lv_obj_t *l = makeLabel(b, UI::type().TAG, r.live ? p.TEXT : p.TEXT_DIM);
    lv_label_set_text(l, r.opts);
    lv_obj_center(l);
    if (!r.live) lv_obj_clear_flag(b, LV_OBJ_FLAG_CLICKABLE);
    else if (r.cb) lv_obj_add_event_cb(b, r.cb, LV_EVENT_CLICKED, nullptr);
    return b;
}

lv_obj_t *deckControl(lv_obj_t *row, const DeckRowSpec &r) {
    if (r.kind == DeckRowKind::ROW_CHECK)  return deckCheckbox(row, r);
    if (r.kind == DeckRowKind::ROW_BUTTON) return deckButton(row, r);
    return deckDropdown(row, r);
}

// A row: what it is on the left, its control on the right.
lv_obj_t *deckRow(lv_obj_t *pane, const char *what, bool live) {
    lv_obj_t *r = plain(pane);
    lv_obj_set_size     (r, lv_pct(100), mm(UI::shape().H_ROW));
    lv_obj_set_flex_flow(r, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(r, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_t *l = makeLabel(r, UI::type().NAME, live ? UI::pal().TEXT : UI::pal().TEXT_DIM);
    lv_label_set_text(l, what);
    return r;
}

// A FOLDER PANEL: SETTINGS in the right half, and its mirror image CHART in
// the left (round 7 - "I want to see how the tab effect works"). One builder,
// so the two are the same shape by construction.
struct Folder { lv_obj_t *root = nullptr, *tab = nullptr, *tabLbl = nullptr, *pane = nullptr, *body = nullptr;
                lv_obj_t *track = nullptr, *up = nullptr, *down = nullptr;
                int32_t h = 0; };

// Built below the bottom of the screen. `left`: the tab at the window's left
// edge and the pane growing rightwards from it; otherwise both on the right.
// `needW`: the widest row; `rows`: how many.
void buildFolder(Folder &F, bool left, const char *title, int32_t needW, int32_t rows,
                 int32_t tabW, lv_event_cb_t tabCb) {
    const UIPalette &p = UI::pal();
    const UIType    &t = UI::type();
    const UIShape   &k = UI::shape();
    const lv_area_t &W = s.winRect;
    const int32_t w  = lv_area_get_width(&W);
    const int32_t r  = mm(k.R_PANEL);
    const int32_t bw = UI::met().BORDER_W ? UI::met().BORDER_W : 1;
    // The page deck's own header height (owner, round 9: its panels "are
    // already the perfect height to sit neatly under the bottom border of the
    // popup").
    s.deckHead = UIToolkit::sc(UIToolkit::PANEL_HEADER_H);
    // A FOLDER TAB (owner, round 10). The pane has a top edge of its own, and
    // the tab rises from it, the join curving in like a file folder's - so an
    // open panel is closed off from the window's body.
    //
    // FOLDED, ONLY THE TAB SHOWS (owner, round 11): the strip above the screen's
    // edge is the page deck's header height, and the pane's edge and the curve
    // sit just below it. So the tab reaches a curve's height further down than
    // what shows. OPEN, the pane's own bottom edge stays below the screen too:
    // the panel is deckHide taller than what it uncovers.
    const int32_t rf       = r;                        // the inner curve
    const int32_t tabAbove = s.deckHead + rf;          // the tab's part above the pane's edge
    s.deckHide = r + 2 * bw;
    int32_t gapTop = mm(k.S5), gapBot = mm(k.S5);       // above the first row, below the last
    F.h = tabAbove + (rf + gapTop) + rows * mm(k.H_ROW) + gapBot + s.deckHide;
    // OPENS NO TALLER THAN THE WINDOW (owner, 2026-10-07): open, the tab's top
    // rests at or below the window's top edge. More rows than that: the panel
    // is dragged (see "Dragging a panel").
    const int32_t sh   = lv_obj_get_height(lv_screen_active());
    const int32_t maxH = sh - s.winRect.y1 + s.deckHide;
    // ONLY JUST TOO TALL: THE GAPS GIVE WAY, NOT A DRAG (owner, 2.10d P1b).
    // SETTINGS' six rows on the P4_5 were ~16 px over - every row in sight, a
    // drag only for the gap under the last one. Up to one gap's worth is
    // taken from the two gaps, half each.
    const int32_t over = F.h - maxH;
    if (over > 0 && over <= gapBot) {
        gapTop -= over / 2;
        gapBot -= over - over / 2;
        F.h = maxH;
    }

    // Built back to front: the pane, the tab over it, a block hiding the
    // tab's bottom where it overlaps the pane, and the inner curve. One colour
    // throughout - the tab used to be the card colour folded and the window's
    // open (owner, round 10). The scheme's lift on the pane and the tab: a
    // shadow on Linen, like the page deck's panels; nothing on the dark ones.
    const int32_t tabXw = left ? 0 : w - tabW;          // in the window's width

    // AS WIDE AS ITS LONGEST ROW (owner, 2026-10-07), its outer edge on the
    // window's, never narrower than its own tab. Within a curve's width of the
    // tab it is made the tab's width: the two then share one straight inner
    // edge, and there is no inner curve to draw.
    int32_t paneW = LV_CLAMP(tabW, needW, w);
    const bool flush = paneW < tabW + 2 * r;
    if (flush) paneW = tabW;
    const int32_t paneXw = left ? 0 : w - paneW;

    // THE HOLDER IS ONLY AS WIDE AS THE TAB AND PANE (round 8, measured on the
    // P4_5: 35-105 ms a frame while sliding). Moving an object redraws its
    // whole area, and a holder as wide as the window, transparent, made LVGL
    // draw the cards, the window and the panel in every strip of every frame.
    // Sized to the panel, most strips lie inside the opaque pane, and LVGL
    // starts drawing there. Widened by the shadow's reach on Linen, so the
    // holder does not clip it.
    const UIMetrics &m = UI::met();
    const int32_t reach = m.SHADOW ? UI::sc(m.SHADOW) + UI::sc(m.SHADOW_Y) + 2 : 0;
    const int32_t x0 = LV_MAX(0, LV_MIN(tabXw, paneXw) - reach);
    const int32_t x1 = LV_MIN(w, LV_MAX(tabXw + tabW, paneXw + paneW) + reach);
    const int32_t tabX  = tabXw - x0;                   // from here on, in the holder
    const int32_t paneX = paneXw - x0;

    // THE TRACK the holder is dragged in (see "Dragging a panel"): as wide as
    // the holder, from the screen's top to the hidden strip under its bottom.
    // Not clickable, so a press beside the panel goes through it. Scrolling is
    // switched on only while the panel rests open (panelArm()).
    F.track = plain(lv_screen_active());
    lv_obj_set_pos (F.track, W.x1 + x0, 0);
    lv_obj_set_size(F.track, x1 - x0, sh + s.deckHide);
    lv_obj_set_scroll_dir(F.track, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(F.track, LV_SCROLLBAR_MODE_OFF);
    lv_obj_clear_flag(F.track, LV_OBJ_FLAG_SCROLL_ELASTIC);
    lv_obj_clear_flag(F.track, LV_OBJ_FLAG_SCROLL_MOMENTUM);
    lv_obj_clear_flag(F.track, LV_OBJ_FLAG_SCROLL_CHAIN);   // nothing passed on to what is behind
    // The left panel is CHART / SCENES, the right SETTINGS: that says which.
    void *which = left ? (void *)1 : nullptr;
    lv_obj_add_event_cb(F.track, panelTrackCb, LV_EVENT_SCROLL, which);
    lv_obj_add_event_cb(F.track, panelTrackCb, LV_EVENT_SCROLL_END, which);

    F.root = plain(F.track);
    lv_obj_set_pos (F.root, 0, sh);   // out of sight
    lv_obj_set_size(F.root, x1 - x0, F.h);

    // THE CARETS (owner): a chevron at the screen's bottom while rows are below
    // it, one at the top while the pane is off the top (`down` and `up` name
    // where they sit, not where they point). Floating, so they stay put while
    // the track scrolls; drawn over the pane, in its middle.
    {
        static lv_point_precise_t ptsDown[3], ptsUp[3];
        const int32_t cw = mm(k.CARET_W), ch = mm(k.CARET_H), lw = LV_MAX(2, mm(k.LINE_PLOT)), o = lw / 2;
        ptsDown[0] = { o, o };      ptsDown[1] = { o + cw / 2, o + ch }; ptsDown[2] = { o + cw, o };
        ptsUp[0]   = { o, o + ch }; ptsUp[1]   = { o + cw / 2, o };      ptsUp[2]   = { o + cw, o + ch };
        auto caret = [&](const lv_point_precise_t *pts, int32_t y) {
            lv_obj_t *l = lv_line_create(F.track);
            lv_obj_remove_style_all(l);
            lv_line_set_points(l, pts, 3);
            lv_obj_set_size(l, cw + lw, ch + lw);
            lv_obj_set_pos (l, paneX + paneW / 2 - (cw + lw) / 2, y);
            lv_obj_set_style_line_width  (l, lw, 0);
            lv_obj_set_style_line_rounded(l, true, 0);
            lv_obj_set_style_line_color  (l, UI::c(UI::mix(p.SURFACE_ALT, p.TEXT, 60)), 0);
            lv_obj_add_flag  (l, LV_OBJ_FLAG_FLOATING);
            lv_obj_add_flag  (l, LV_OBJ_FLAG_HIDDEN);
            lv_obj_clear_flag(l, LV_OBJ_FLAG_CLICKABLE);
            return l;
        };
        // POINTING THE WAY TO DRAG (owner, P1b round 2): rows below the screen
        // show a caret pointing UP - "drag me up" - and a panel gone off the
        // top one pointing down.
        F.down = caret(ptsUp, sh - (ch + lw) - mm(k.S4));
        F.up   = caret(ptsDown, mm(k.S4));
    }

    // FLUSH: ONE PIECE (owner, 2026-10-07: a break in the left edge under the
    // title on the P4_5 and the 1060, a line under it on Linen). A tab exactly
    // as wide as its pane met it corner to rounded corner. So the pane starts
    // at the top and is the tab's shape itself; the tab below is only its
    // label and its touch area.
    const int32_t paneY = flush ? 0 : tabAbove;
    lv_obj_t *pane = plain(F.root);
    F.pane = pane;
    lv_obj_set_pos (pane, paneX, paneY);
    lv_obj_set_size(pane, paneW, F.h - paneY);
    lv_obj_set_style_radius      (pane, r, 0);
    lv_obj_set_style_bg_color    (pane, UI::c(p.SURFACE_ALT), 0);
    lv_obj_set_style_bg_opa      (pane, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(pane, UI::border(), 0);
    lv_obj_set_style_border_width(pane, bw, 0);
    lv_obj_add_style             (pane, UI::paint(UIPaint::PAINT_LIFT), 0);
    // The pane's padding keeps its rows below the tab's join. Its right margin
    // is the body's own (below).
    lv_obj_set_style_pad_left(pane, mm(k.S7), 0);
    lv_obj_set_style_pad_top(pane, (tabAbove - paneY) + rf + gapTop, 0);
    lv_obj_set_style_pad_bottom(pane, gapBot + s.deckHide, 0);   // nothing in the part that stays hidden
    lv_obj_add_flag(pane, LV_OBJ_FLAG_CLICKABLE);   // a tap on the pane stays on the pane

    // The rows sit in a body of their own, which never scrolls: the whole
    // panel is dragged instead (P1b round 2). A press on a row or a gap lands
    // on the body; a drag from there is passed up to the track.
    lv_obj_t *body = plain(pane);
    F.body = body;
    lv_obj_set_size(body, lv_pct(100), lv_pct(100));
    lv_obj_set_style_pad_right(body, mm(k.S7), 0);
    lv_obj_set_flex_flow(body, LV_FLEX_FLOW_COLUMN);
    lv_obj_add_flag(body, LV_OBJ_FLAG_CLICKABLE);

    // The tab: SETTINGS IN THE RIGHT HALF, ALWAYS (owner, round 9); a second
    // panel (CHART) takes the left half, so SETTINGS is always in the same
    // place. It reaches a radius past the pane's edge, its lower corners
    // hidden by the block below.
    F.tab = plain(F.root);
    lv_obj_set_pos (F.tab, tabX, 0);
    lv_obj_set_size(F.tab, tabW, tabAbove + r + bw);
    lv_obj_set_style_radius      (F.tab, r, 0);
    lv_obj_set_style_bg_color    (F.tab, UI::c(p.SURFACE_ALT), 0);
    lv_obj_set_style_bg_opa      (F.tab, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(F.tab, UI::border(), 0);
    lv_obj_set_style_border_width(F.tab, bw, 0);
    lv_obj_add_style             (F.tab, UI::paint(UIPaint::PAINT_LIFT), 0);
    lv_obj_add_flag(F.tab, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(F.tab, tabCb, LV_EVENT_CLICKED, nullptr);
    F.tabLbl = makeLabel(F.tab, t.TAG, p.ACCENT);
    lv_label_set_text(F.tabLbl, title);
    lv_obj_set_style_text_letter_space(F.tabLbl, mm(k.TRACK_TAB), 0);
    // Where round 11 had it, which the owner called right.
    lv_obj_align(F.tabLbl, LV_ALIGN_TOP_MID, 0, (s.deckHead - rf - lv_font_get_line_height(t.TAG)) / 2);
    if (flush) {
        // The pane is the shape; the tab draws nothing of its own.
        lv_obj_remove_style(F.tab, UI::paint(UIPaint::PAINT_LIFT), 0);
        lv_obj_set_style_bg_opa      (F.tab, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(F.tab, 0, 0);
        return;
    }

    // The block: the tab's lower corners, bottom border and (on Linen) the
    // shadow it casts downward, all of which lie inside the pane - painted out
    // in the pane's colour. Kept one border-width clear of the pane's outer
    // edge, which carries on down past the tab.
    // AND THE SHADOW IT CASTS INWARDS: a shadow spreads sideways too, and
    // round the tab's inner lower corner it showed below the pane's edge as a
    // small grey crescent at the foot of the inner curve (owner, 2026-10-06 -
    // "thought it was crud on the glass"). The block reaches in by the
    // shadow's width; the curve's arcs are drawn over it.
    lv_obj_t *block = plain(F.root);
    lv_obj_set_pos (block, left ? tabX + bw : tabX - reach, tabAbove + bw);
    lv_obj_set_size(block, tabW - bw + reach, r + bw + reach);
    lv_obj_set_style_bg_color(block, UI::c(p.SURFACE_ALT), 0);
    lv_obj_set_style_bg_opa  (block, LV_OPA_COVER, 0);

    // THE SEAM: the tip of the tab's inner border, on the pane's edge line at
    // the foot of the curve. The curve's fill arc covers it only up to its own
    // anti-aliased rim, so a 3-pixel speck was left (owner, round 7: "now that
    // I know about it I can't ignore it"). Painted out here; the arcs below are
    // drawn after, so nothing of the curve is lost.
    lv_obj_t *seam = plain(F.root);
    lv_obj_set_pos (seam, left ? tabX + tabW - 3 * bw - 2 : tabX - 2, tabAbove - bw - 1);
    lv_obj_set_size(seam, 3 * bw + 4, 2 * bw + 2);
    lv_obj_set_style_bg_color(seam, UI::c(p.SURFACE_ALT), 0);
    lv_obj_set_style_bg_opa  (seam, LV_OPA_COVER, 0);

    // The inner curve where the tab meets the pane's edge: a concave corner,
    // which LVGL has no shape for. Two quarter arcs centred a radius out from
    // the join: a thick one in the pane's colour fills the corner outside the
    // curve, and a border-width one draws the edge along it. Their spill onto
    // the tab and the pane is the same colour, so it does not show. Mirrored
    // for a tab on the left: the centre a radius to the tab's right, the
    // quarter from +y round to -x.
    const lv_point_t ctr = { left ? tabX + tabW + rf : tabX - rf, tabAbove - rf };
    const int32_t ro = (rf * 1415 + 999) / 1000 + 1;   // reaches the join, rf * sqrt(2)
    auto quarter = [&](int32_t outer, int32_t width, lv_color_t col) {
        lv_obj_t *a = lv_arc_create(F.root);
        lv_obj_remove_style_all(a);
        lv_obj_clear_flag(a, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_set_pos (a, ctr.x - outer, ctr.y - outer);
        lv_obj_set_size(a, 2 * outer, 2 * outer);
        if (left) lv_arc_set_bg_angles(a, 90, 180);   // from +y round to -x
        else      lv_arc_set_bg_angles(a, 0, 90);     // from +x round to +y: the join's side
        lv_obj_set_style_arc_width  (a, width, LV_PART_MAIN);
        lv_obj_set_style_arc_color  (a, col, LV_PART_MAIN);
        lv_obj_set_style_arc_opa    (a, LV_OPA_COVER, LV_PART_MAIN);
        lv_obj_set_style_arc_rounded(a, false, LV_PART_MAIN);
        lv_obj_set_style_arc_opa    (a, LV_OPA_TRANSP, LV_PART_INDICATOR);
    };
    quarter(ro, ro - rf, UI::c(p.SURFACE_ALT));
    quarter(rf + bw, bw, UI::border());
}

// SETTINGS: built with the window, below the bottom of the screen, inside
// showWindow()'s quiet build (invalidation off); showWindow() then slides it
// up to its tab. Its rows come from one list - deckSpecs() - read here for the
// size and again by deckFill() for the contents.
// What the rows are, as a number: their words, choices and whether each is live.
uint32_t s_deckSig = 0;   // the rows SETTINGS was built for
uint32_t deckSig(const DeckRowSpec *r, uint8_t n) {
    uint32_t h = 2166136261u;   // FNV-1a
    auto mix = [&](const char *p) { for (; p && *p; p++) { h ^= (uint8_t)*p; h *= 16777619u; } h ^= 0xFF; h *= 16777619u; };
    for (uint8_t i = 0; i < n; i++) { mix(r[i].label); mix(r[i].opts); h ^= r[i].live ? 1u : 2u; h *= 16777619u; }
    return h;
}

void deckCreate() {
    DeckRowSpec specs[DECK_ROWS_MAX];
    const int32_t rows = deckSpecs(specs);
    s_deckSig = deckSig(specs, (uint8_t)rows);
    const int32_t w = lv_area_get_width(&s.winRect);
    Folder F;
    buildFolder(F, false, "SETTINGS", deckNeedW(), rows, w / 2, deckTabCb);
    s.deck = F.root; s.deckTab = F.tab; s.deckTabLbl = F.tabLbl; s.deckPane = F.pane;
    s.deckBody = F.body; s.deckTrack = F.track; s.deckUp = F.up; s.deckDown = F.down;
    s.deckH = F.h;
    s.deckState  = DECK_HIDDEN;
    s.deckFilled = false;
}

// The pane's rows, BUILT WHEN THE DECK IS FIRST OPENED (2.10b, owner: try
// every reasonable saving). Folded, only the tab shows, and the rows were
// ~5 KB of LVGL's pool that most windows never needed.
void deckFill() {
    if (!s.deckBody || s.deckFilled) return;
    s.deckFilled = true;
    // The rows, from the one list (deckSpecs()): Paused first - the one that
    // works, and the one a person comes for - then the card's settings.
    DeckRowSpec r[DECK_ROWS_MAX];
    const uint8_t n = deckSpecs(r);
    for (uint8_t i = 0; i < n; i++) {
        lv_obj_t *row = deckRow(s.deckBody, r[i].label, r[i].live);
        lv_obj_t *ctl = deckControl(row, r[i]);
        if (r[i].out) *r[i].out = ctl;
    }
}

// SETTINGS FOLLOWS ITS ROWS (K17 testing, owner: "Add as a card" came and went).
// The panel is sized for its rows when it is made and filled when first opened,
// so rows that change later - another view (a member's own adds "Add as a
// card"), "Added", "Sure?" - were missing or cut off. When the rows are not
// the ones it was made for, it is made again in place: same state, no slide.
void deckSync() {
    if (!s.deck) return;
    DeckRowSpec r[DECK_ROWS_MAX];
    if (deckSig(r, deckSpecs(r)) == s_deckSig) return;
    const uint8_t st = s.deckState;
    lv_anim_delete(s.deck, deckExec);
    lv_anim_delete(s.deckTrack, nullptr);
    lv_obj_remove_event_cb(s.deckTrack, panelTrackCb);
    lv_obj_delete(s.deckTrack);   // the holder and its rows with it
    s.deck = s.deckTab = s.deckTabLbl = s.deckPane = s.deckBody = nullptr;
    s.deckTrack = s.deckUp = s.deckDown = nullptr;
    s.swPause = nullptr;
    s.ddGroup = nullptr;
    deckCreate();
    if (st == DECK_HIDDEN) return;
    s.deckState = st;
    if (st == DECK_OPEN) { deckFill(); lv_obj_move_foreground(s.deckTrack); }
    lv_obj_set_y(s.deck, deckY(st));
    if (st == DECK_OPEN) {
        panelArm(s.deckTrack, s.deck, s.deckH);
        panelCarets(s.deckTrack, s.deck, s.deckPane, s.deckUp, s.deckDown);
    }
}

// Open the deck, building its rows the first time.
void deckOpen() {
    deckFill();
    deckSet(DECK_OPEN);
}

// The holder is out of sight: its track goes, and the holder with it.
void deckGoneCb(lv_anim_t *a) { lv_obj_delete_async(lv_obj_get_parent((lv_obj_t *)a->var)); }

// The deck goes with the window - but HOW depends on how it was showing
// (owner, round 9): open, it vanishes with the window in the same frame;
// showing only its tab, the tab slides back down as fast as it came up, on
// its own, after the window has gone. It is no longer the popup's then: the
// pointers are cleared at once, its tab stops taking taps, and it deletes
// itself when it is out of sight. A window opened meanwhile builds its own.
void deckEnd() {
    if (s.deck) {
        lv_anim_delete(s.deck, deckExec);
        // No longer the popup's: its track must not fold or snap it from here.
        lv_obj_remove_event_cb(s.deckTrack, panelTrackCb);
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
            lv_obj_delete(s.deckTrack);
        }
    }
    s.deck = s.deckTab = s.deckTabLbl = s.deckPane = s.deckBody = nullptr;
    s.deckTrack = s.deckUp = s.deckDown = nullptr;
    s.swPause = nullptr;
    s.ddGroup = nullptr;
    s.deckState  = DECK_HIDDEN;
    s.deckFilled = false;
}

void chartTabCb(lv_event_t *ev) {
    (void)ev;
    chartSet(s_chart.state == DECK_OPEN ? DECK_PEEK : DECK_OPEN);
}

// Made the first time its view shows, rows and all: CHART's (a demo) or
// SCENES' (s_sideWant). SETTINGS builds its rows on first open.
void chartCreate() {
    DeckRowSpec r[DECK_ROWS_MAX];
    const bool scenes   = (s_sideWant == SIDE_SCENES);
    const bool activity = (s_sideWant == SIDE_ACTIVITY);
    const uint8_t n = scenes ? scenesSpecs(r) : activity ? activitySpecs(r) : chartSpecs(r);
    const int32_t w = lv_area_get_width(&s.winRect);
    Folder F;
    buildFolder(F, true, scenes ? "SCENES" : activity ? "ACTIVITY" : "CHART", rowsNeedW(r, n), n,
                w - w / 2 - mm(UI::shape().S3), chartTabCb);
    s_chart.root = F.root; s_chart.tab = F.tab; s_chart.pane = F.pane; s_chart.body = F.body; s_chart.h = F.h;
    s_chart.track = F.track; s_chart.up = F.up; s_chart.down = F.down;
    s_chart.kind = s_sideWant;
    for (uint8_t i = 0; i < n; i++) {
        lv_obj_t *row = deckRow(F.body, r[i].label, r[i].live);
        lv_obj_t *ctl = deckControl(row, r[i]);
        if (r[i].out) *r[i].out = ctl;
    }
}

void chartArmCb(lv_anim_t *a) {
    (void)a;
    if (s_chart.state != DECK_OPEN) return;
    panelArm(s_chart.track, s_chart.root, s_chart.h);
    panelCarets(s_chart.track, s_chart.root, s_chart.pane, s_chart.up, s_chart.down);
}

void chartSet(uint8_t state) {
    if (state == DECK_HIDDEN && !s_chart.root) return;
    // A panel just made has not been laid out: lv_obj_get_y() would say 0, and
    // the first slide came down from the top of the screen (owner, N9).
    const bool made = !s_chart.root;
    if (made) chartCreate();
    if (state == s_chart.state) return;
    const uint8_t was = s_chart.state;
    s_chart.state = state;
    const int32_t from = made ? chartY(DECK_HIDDEN) : panelDisarm(s_chart.track, s_chart.root);   // as SETTINGS
    panelCarets(s_chart.track, s_chart.root, s_chart.pane, s_chart.up, s_chart.down);
    if (state == DECK_OPEN) { lv_obj_move_foreground(s_chart.track); if (s.deckState == DECK_OPEN) deckSet(DECK_PEEK); }
    lv_anim_delete(s_chart.root, chartExec);
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var     (&a, s_chart.root);
    lv_anim_set_values  (&a, from, chartY(state));
    lv_anim_set_duration(&a, (was == DECK_HIDDEN || state == DECK_HIDDEN) ? DECK_PEEK_MS : DECK_OPEN_MS);
    lv_anim_set_path_cb (&a, state == DECK_HIDDEN ? lv_anim_path_ease_in : lv_anim_path_ease_out);
    lv_anim_set_exec_cb (&a, chartExec);
    if (state == DECK_OPEN) lv_anim_set_completed_cb(&a, chartArmCb);
    lv_anim_start(&a);
}

void chartFold() { if (s_chart.state == DECK_OPEN) chartSet(DECK_PEEK); }

// Both panels' tracks (see "Dragging a panel"): the carets follow every step;
// let go below the resting place, it folds past a third of the way to its tab,
// else goes back up. Above it, it stays where it was let go.
void panelTrackCb(lv_event_t *ev) {
    const bool chart = lv_event_get_user_data(ev) != nullptr;
    lv_obj_t *track = chart ? s_chart.track : s.deckTrack;
    if (!track || lv_event_get_target(ev) != track) return;
    // THE CEILING (owner, P1b round 2: D3, D8). LVGL's non-elastic clamp
    // (lv_indev_scroll.c elastic_diff()) holds only one way: for a drag up,
    // `scroll_end - diff < 0` with diff negative is never true, so one step of
    // the finger went past the end - the panel's bottom edge and a gap showed,
    // and it slid back when let go. Held here, before anything is drawn.
    if (panelArmed(track)) {
        const int32_t past = lv_obj_get_scroll_bottom(track);
        if (past < 0) lv_obj_scroll_to_y(track, lv_obj_get_scroll_y(track) + past, LV_ANIM_OFF);
    }
    if (chart) panelCarets(track, s_chart.root, s_chart.pane, s_chart.up, s_chart.down);
    else       panelCarets(track, s.deck, s.deckPane, s.deckUp, s.deckDown);
    if (lv_event_get_code(ev) != LV_EVENT_SCROLL_END || !panelArmed(track)) return;
    if ((chart ? s_chart.state : s.deckState) != DECK_OPEN) return;
    const int32_t room  = panelPeekY() - panelRestY(chart ? s_chart.h : s.deckH);
    const int32_t below = room - lv_obj_get_scroll_y(track);   // how far under its resting place
    if (below <= 0) return;
    if (below > room / 3) { if (chart) chartSet(DECK_PEEK); else deckSet(DECK_PEEK); }
    else lv_obj_scroll_to_y(track, room, LV_ANIM_ON);
}

// With the window, as SETTINGS goes (owner, N11): open, it vanishes in the
// same frame; showing its tab (or on its way down), it slides out of sight on
// its own and deletes itself there.
void chartEnd() {
    if (lv_obj_t *o = s_chart.root) {
        lv_anim_delete(o, chartExec);
        lv_obj_remove_event_cb(s_chart.track, panelTrackCb);   // as deckEnd()
        if (s_chart.state == DECK_OPEN) {
            lv_obj_delete(s_chart.track);
        } else {
            if (s_chart.tab) lv_obj_clear_flag(s_chart.tab, LV_OBJ_FLAG_CLICKABLE);
            lv_anim_t a;
            lv_anim_init(&a);
            lv_anim_set_var         (&a, o);
            lv_anim_set_values      (&a, lv_obj_get_y(o), chartY(DECK_HIDDEN));
            lv_anim_set_duration    (&a, DECK_PEEK_MS);
            lv_anim_set_path_cb     (&a, lv_anim_path_ease_in);
            lv_anim_set_exec_cb     (&a, chartExec);
            lv_anim_set_completed_cb(&a, deckGoneCb);
            lv_anim_start(&a);
        }
    }
    s_chart = ChartPanel();
    s.ddTapScene = nullptr;   // SCENES' rows went with it
    s_cbHidden   = nullptr;
    s_ddSpan     = nullptr;   // and CHART's
}

// The left panel belongs to its view (K43): CHART to History, SCENES to
// Scenes - up with it, gone when it goes. Another view's panel is ended and the
// right one made; the same view rebuilt (a scene list that changed) keeps its
// panel, open or not.
void sideFor(PopupView v) {
    // History's panel follows its subject (2.10e): CHART for a number, ACTIVITY
    // for the rest, none where there is no HA history. Another subject (a
    // member's history after the group's) gets its panel made afresh.
    uint8_t side = SIDE_NONE;
    if (v == PopupView::VIEW_HISTORY) {
        if (histSync() && s_chart.root) chartEnd();
        if (histFromHa(s_hv.e)) side = s_hv.chart ? SIDE_CHART : SIDE_ACTIVITY;
    } else if (v == PopupView::VIEW_SCENES) {
        side = SIDE_SCENES;
    }
    if (s_chart.root && s_chart.kind != side) chartEnd();
    if (side != SIDE_NONE && !s_chart.root) { s_sideWant = side; chartSet(DECK_PEEK); }
}

} // namespace popup
