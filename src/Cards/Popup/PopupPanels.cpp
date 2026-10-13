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

// The panels and their rows are the kit's (Kit::folder(), Kit::rows() - 2.12 P3):
// built from the row specs, the panel as wide as its longest row.
int32_t deckNeedW() {
    DeckRowSpec r[DECK_ROWS_MAX];
    return Kit::rowsNeedW(r, deckSpecs(r));
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
    // SETTINGS IN THE RIGHT HALF, ALWAYS (owner, round 9). Its track's taps
    // are panelTrackCb()'s, which a null user value tells is SETTINGS'.
    Kit::Folder F;
    Kit::folder(F, { &s.winRect, false, "SETTINGS", deckNeedW(), rows, w / 2, deckTabCb, panelTrackCb, nullptr });
    s.deck = F.root; s.deckTab = F.tab; s.deckTabLbl = F.tabLbl; s.deckPane = F.pane;
    s.deckBody = F.body; s.deckTrack = F.track; s.deckUp = F.up; s.deckDown = F.down;
    s.deckH = F.h; s.deckHead = F.head; s.deckHide = F.hide;
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
    Kit::rows(s.deckBody, r, deckSpecs(r));
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
    // The left half; its tab stops a millimetre short of SETTINGS' (owner,
    // round 7: two tabs side by side read as two). A user value of 1 tells
    // panelTrackCb() it is this panel's track.
    Kit::Folder F;
    Kit::folder(F, { &s.winRect, true, scenes ? "SCENES" : activity ? "ACTIVITY" : "CHART", Kit::rowsNeedW(r, n), n,
                     w - w / 2 - mm(UI::shape().S3), chartTabCb, panelTrackCb, (void *)1 });
    s_chart.root = F.root; s_chart.tab = F.tab; s_chart.pane = F.pane; s_chart.body = F.body; s_chart.h = F.h;
    s_chart.track = F.track; s_chart.up = F.up; s_chart.down = F.down;
    s_chart.kind = s_sideWant;
    s.deckHead = F.head; s.deckHide = F.hide;
    Kit::rows(F.body, r, n);
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
