// The sheet's two folder panels (2.12 P4, moved from the card window's PopupPanels.cpp): SETTINGS
// on the right and the view's own panel on the left - built (Kit::folder()), slid up and down,
// dragged, and gone with the sheet. What their rows are is the client's.
//
// SETTINGS (card-sheet 11.1 pathway 1, section 13; Card Popup Mock v3): a tab the width of half
// the window peeks up from the bottom of the screen, under the window, as the window appears. Tap
// it and the panel opens up over the window's lower part; tap it again, or anywhere in the window,
// and it folds back to the tab; a tap outside closes the panel and the window together. It goes
// with the window, in the same frame. Opaque and on the screen, like the window, so moving it
// redraws only it.
//
// THE LEFT PANEL (owner, round 7, "I want to see how the tab effect works"): SETTINGS' mirror
// image in the left half, made by the same Kit::folder(), its tab a millimetre short of SETTINGS'
// (two tabs side by side read as two). It belongs to a view (K43) - the card window's CHART or
// ACTIVITY on History, SCENES on Scenes - up with it, gone when it goes.
#include "KitSheetInternal.h"

namespace Kit {

constexpr uint32_t PEEK_MS = 200;   // the tab peeking up: a small touch the owner liked
// 260 ms, from 220 (owner, round 7: choppier now that the panel opens further;
// the page deck's panels take 300). Try 300 if it still stutters.
constexpr uint32_t OPEN_MS = 260;

static void slideExec(void *var, int32_t v) { lv_obj_set_y((lv_obj_t *)var, v); }

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
static int32_t restY(const Sheet &S, int32_t h) {
    const int32_t sh = lv_obj_get_height(lv_screen_active());
    return LV_MAX(S.winRect.y1, sh - (h - S.panelHide));
}

static int32_t peekY(const Sheet &S) { return lv_obj_get_height(lv_screen_active()) - S.panelHead; }

static int32_t panelY(const Sheet &S, const SheetPanel &P, uint8_t state) {
    return state == PANEL_OPEN ? restY(S, P.h) : state == PANEL_PEEK ? peekY(S)
                               : lv_obj_get_height(lv_screen_active());
}

static bool armed(lv_obj_t *track) { return track && lv_obj_has_flag(track, LV_OBJ_FLAG_SCROLLABLE); }

// The carets, from where the panel stands: more rows below the screen; the
// pane gone off the top.
static void carets(const SheetPanel &P) {
    if (!P.track || !P.root || !P.pane || !P.up || !P.down) return;
    const bool on    = armed(P.track);
    const bool below = on && lv_obj_get_scroll_bottom(P.track) > 0;
    const bool above = on && lv_obj_get_y(P.root) - lv_obj_get_scroll_y(P.track) + lv_obj_get_y(P.pane) < 0;
    if (lv_obj_has_flag(P.down, LV_OBJ_FLAG_HIDDEN) == below) {
        if (below) lv_obj_clear_flag(P.down, LV_OBJ_FLAG_HIDDEN); else lv_obj_add_flag(P.down, LV_OBJ_FLAG_HIDDEN);
    }
    if (lv_obj_has_flag(P.up, LV_OBJ_FLAG_HIDDEN) == above) {
        if (above) lv_obj_clear_flag(P.up, LV_OBJ_FLAG_HIDDEN); else lv_obj_add_flag(P.up, LV_OBJ_FLAG_HIDDEN);
    }
}

// Resting open: the track takes over (see above).
static void arm(const Sheet &S, const SheetPanel &P) {
    if (!P.track || armed(P.track)) return;
    const int32_t rest = restY(S, P.h), room = peekY(S) - rest;
    lv_obj_set_y(P.root, rest + room);
    lv_obj_add_flag(P.track, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_scroll_to_y(P.track, room, LV_ANIM_OFF);   // brings the layout up to date first
}

// Before a slide: the holder back where it shows, the track at rest. Returns
// the holder's y to slide from.
static int32_t disarm(const SheetPanel &P) {
    if (!armed(P.track)) return lv_obj_get_y(P.root);
    lv_anim_delete(P.track, nullptr);   // a slide back up, under way
    lv_obj_update_layout(P.track);
    const int32_t y = lv_obj_get_y(P.root) - lv_obj_get_scroll_y(P.track);
    lv_obj_scroll_to_y(P.track, 0, LV_ANIM_OFF);
    lv_obj_clear_flag(P.track, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_y(P.root, y);
    return y;
}

bool sheetPanelOpen(const Sheet &S) { return S.right.state == PANEL_OPEN || S.left.state == PANEL_OPEN; }

// On a panel's tab or its pane, as they stand right now? A panel's own object
// is transparent and as wide as its holder: a press beside its tab is a press
// on the page, which the catcher must take.
bool sheetInPanel(const Sheet &S, const lv_point_t &pt) {
    auto in = [&](lv_obj_t *o) {
        if (!o) return false;
        lv_area_t a;
        lv_obj_get_coords(o, &a);
        return pt.x >= a.x1 && pt.x <= a.x2 && pt.y >= a.y1 && pt.y <= a.y2;
    };
    if (S.left.root && S.left.state != PANEL_HIDDEN && (in(S.left.tab) || in(S.left.pane))) return true;
    if (!S.right.root || S.right.state == PANEL_HIDDEN) return false;
    return in(S.right.tab) || in(S.right.pane);
}

// Both panels' tracks: the carets follow every step; let go below the resting
// place, it folds past a third of the way to its tab, else goes back up. Above
// it, it stays where it was let go. The user value says which: null SETTINGS,
// 1 the left panel.
static void trackCb(lv_event_t *ev) {
    Sheet *S = s_open;
    if (!S) return;
    const bool left = lv_event_get_user_data(ev) != nullptr;
    SheetPanel &P = left ? S->left : S->right;
    if (!P.track || lv_event_get_target(ev) != P.track) return;
    // THE CEILING (owner, P1b round 2: D3, D8). LVGL's non-elastic clamp
    // (lv_indev_scroll.c elastic_diff()) holds only one way: for a drag up,
    // `scroll_end - diff < 0` with diff negative is never true, so one step of
    // the finger went past the end - the panel's bottom edge and a gap showed,
    // and it slid back when let go. Held here, before anything is drawn.
    if (armed(P.track)) {
        const int32_t past = lv_obj_get_scroll_bottom(P.track);
        if (past < 0) lv_obj_scroll_to_y(P.track, lv_obj_get_scroll_y(P.track) + past, LV_ANIM_OFF);
    }
    carets(P);
    if (lv_event_get_code(ev) != LV_EVENT_SCROLL_END || !armed(P.track)) return;
    if (P.state != PANEL_OPEN) return;
    const int32_t room  = peekY(*S) - restY(*S, P.h);
    const int32_t below = room - lv_obj_get_scroll_y(P.track);   // how far under its resting place
    if (below <= 0) return;
    if (below > room / 3) { if (left) sheetSideSet(*S, PANEL_PEEK); else sheetRightSet(*S, PANEL_PEEK); }
    else lv_obj_scroll_to_y(P.track, room, LV_ANIM_ON);
}

// The holder is out of sight: its track goes, and the holder with it.
static void goneCb(lv_anim_t *a) { lv_obj_delete_async(lv_obj_get_parent((lv_obj_t *)a->var)); }

// A panel going with its sheet - HOW depends on how it was showing (owner,
// round 9): open, it vanishes with the window in the same frame; showing only
// its tab, the tab slides back down as fast as it came up, on its own, after
// the window has gone. It is no longer the sheet's then: its track stops
// answering, its tab stops taking taps, and it deletes itself out of sight. A
// sheet opened meanwhile builds its own. `slide`: which of the two.
static void letGo(SheetPanel &P, bool slide) {
    if (!P.root) return;
    lv_anim_delete(P.root, slideExec);
    lv_obj_remove_event_cb(P.track, trackCb);
    if (!slide) {
        lv_obj_delete(P.track);
        return;
    }
    if (P.tab) lv_obj_clear_flag(P.tab, LV_OBJ_FLAG_CLICKABLE);
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var         (&a, P.root);
    lv_anim_set_values      (&a, lv_obj_get_y(P.root), lv_obj_get_height(lv_screen_active()));
    lv_anim_set_duration    (&a, PEEK_MS);
    lv_anim_set_path_cb     (&a, lv_anim_path_ease_in);
    lv_anim_set_exec_cb     (&a, slideExec);
    lv_anim_set_completed_cb(&a, goneCb);
    lv_anim_start(&a);
}

static void take(SheetPanel &P, const Folder &F) {
    P.root = F.root; P.tab = F.tab; P.tabLbl = F.tabLbl; P.pane = F.pane; P.body = F.body;
    P.track = F.track; P.up = F.up; P.down = F.down; P.h = F.h;
}

static void forget(SheetPanel &P) {
    P.root = P.tab = P.tabLbl = P.pane = P.body = nullptr;
    P.track = P.up = P.down = nullptr;
    P.state  = PANEL_HIDDEN;
    P.filled = false;
}

// ---------------------------------------------------------------------------
// SETTINGS
// ---------------------------------------------------------------------------
static uint8_t rightRows(const Sheet &S, RowSpec *r) {
    return (S.spec && S.spec->panelRows) ? S.spec->panelRows(r) : 0;
}

// What the rows are, as a number: their words, choices and whether each is live.
static uint32_t rowsSig(const RowSpec *r, uint8_t n) {
    uint32_t h = 2166136261u;   // FNV-1a
    auto mix = [&](const char *p) { for (; p && *p; p++) { h ^= (uint8_t)*p; h *= 16777619u; } h ^= 0xFF; h *= 16777619u; };
    for (uint8_t i = 0; i < n; i++) { mix(r[i].label); mix(r[i].opts); h ^= r[i].live ? 1u : 2u; h *= 16777619u; }
    return h;
}

static void rightTabCb(lv_event_t *ev) {
    (void)ev;
    Sheet *S = s_open;
    if (!S) return;
    if (S->right.state == PANEL_OPEN) sheetRightSet(*S, PANEL_PEEK);
    else                              sheetRightOpen(*S);
}

static void rightArmCb(lv_anim_t *a) {
    (void)a;
    Sheet *S = s_open;
    if (!S || S->right.state != PANEL_OPEN) return;
    arm(*S, S->right);
    carets(S->right);
}

// Built with the sheet, below the bottom of the screen, inside its quiet build
// (invalidation off); sheetOpen() then slides it up to its tab. Its rows come
// from one list - the spec's panelRows - read here for the size and again when
// it first opens for the contents.
void rightCreate(Sheet &S) {
    RowSpec r[ROWS_MAX];
    const uint8_t n = rightRows(S, r);
    S.right.sig = rowsSig(r, n);
    const int32_t w = lv_area_get_width(&S.winRect);
    // SETTINGS IN THE RIGHT HALF, ALWAYS (owner, round 9).
    Folder F;
    folder(F, { &S.winRect, false, S.spec->panelTitle, rowsNeedW(r, n), n, w / 2, rightTabCb, trackCb, nullptr });
    take(S.right, F);
    S.panelHead = F.head;
    S.panelHide = F.hide;
    S.right.state  = PANEL_HIDDEN;
    S.right.filled = false;
}

// The pane's rows, BUILT WHEN THE PANEL IS FIRST OPENED (2.10b, owner: try
// every reasonable saving). Folded, only the tab shows, and the rows were
// ~5 KB of LVGL's pool that most windows never needed.
static void rightFill(Sheet &S) {
    if (!S.right.body || S.right.filled) return;
    S.right.filled = true;
    RowSpec r[ROWS_MAX];
    rows(S.right.body, r, rightRows(S, r));
}

void sheetRightSet(Sheet &S, uint8_t state) {
    SheetPanel &P = S.right;
    if (!P.root) return;
    const uint8_t was = P.state;
    P.state = state;
    // FOLDING, BACK TO THE TOP (owner, 2.10d): disarming puts a panel dragged
    // up back in one piece, so nothing of its rows shows in the tab's strip.
    const int32_t from = disarm(P);
    carets(P);
    // Open, on top of the other panel, which folds to its tab.
    if (state == PANEL_OPEN) { lv_obj_move_foreground(P.track); sheetSideFold(S); }
    lv_anim_delete(P.root, slideExec);
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var     (&a, P.root);
    lv_anim_set_values  (&a, from, panelY(S, P, state));
    lv_anim_set_duration(&a, (was == PANEL_HIDDEN) ? PEEK_MS : OPEN_MS);
    lv_anim_set_path_cb (&a, lv_anim_path_ease_out);
    lv_anim_set_exec_cb (&a, slideExec);
    if (state == PANEL_OPEN) lv_anim_set_completed_cb(&a, rightArmCb);
    lv_anim_start(&a);
}

void sheetRightOpen(Sheet &S) {
    rightFill(S);
    sheetRightSet(S, PANEL_OPEN);
}

// SETTINGS FOLLOWS ITS ROWS (K17 testing, owner: "Add as a card" came and
// went). The panel is sized for its rows when it is made and filled when first
// opened, so rows that change later - another view, "Added" - were missing or
// cut off. When the rows are not the ones it was made for, it is made again in
// place: same state, no slide.
void sheetRightSync(Sheet &S) {
    SheetPanel &P = S.right;
    if (!P.root) return;
    RowSpec r[ROWS_MAX];
    if (rowsSig(r, rightRows(S, r)) == P.sig) return;
    const uint8_t st = P.state;
    lv_anim_delete(P.root, slideExec);
    lv_anim_delete(P.track, nullptr);
    lv_obj_remove_event_cb(P.track, trackCb);
    lv_obj_delete(P.track);   // the holder and its rows with it
    forget(P);
    if (S.spec && S.spec->panelGone) S.spec->panelGone();
    rightCreate(S);
    if (st == PANEL_HIDDEN) return;
    P.state = st;
    if (st == PANEL_OPEN) { rightFill(S); lv_obj_move_foreground(P.track); }
    lv_obj_set_y(P.root, panelY(S, P, st));
    if (st == PANEL_OPEN) {
        arm(S, P);
        carets(P);
    }
}

void rightEnd(Sheet &S) {
    letGo(S.right, S.right.state == PANEL_PEEK);   // peeking, its tab slides away; else it goes at once
    forget(S.right);
    if (S.spec && S.spec->panelGone) S.spec->panelGone();
}

// ---------------------------------------------------------------------------
// The left panel
// ---------------------------------------------------------------------------
static void sideTabCb(lv_event_t *ev) {
    (void)ev;
    Sheet *S = s_open;
    if (!S) return;
    sheetSideSet(*S, S->left.state == PANEL_OPEN ? PANEL_PEEK : PANEL_OPEN);
}

static void sideArmCb(lv_anim_t *a) {
    (void)a;
    Sheet *S = s_open;
    if (!S || S->left.state != PANEL_OPEN) return;
    arm(*S, S->left);
    carets(S->left);
}

// Made the first time its view shows, rows and all.
static void sideCreate(Sheet &S) {
    RowSpec r[ROWS_MAX];
    const uint8_t n = S.left.rows ? S.left.rows(r) : 0;
    const int32_t w = lv_area_get_width(&S.winRect);
    // The left half; its tab stops a millimetre short of SETTINGS' (owner,
    // round 7: two tabs side by side read as two).
    Folder F;
    folder(F, { &S.winRect, true, S.left.title, rowsNeedW(r, n), n, w - w / 2 - UI::mm(UI::shape().S3),
                sideTabCb, trackCb, (void *)1 });
    take(S.left, F);
    S.panelHead = F.head;
    S.panelHide = F.hide;
    rows(F.body, r, n);
}

void sheetSide(Sheet &S, uint8_t kind, const char *title, uint8_t (*rowsOf)(RowSpec *r)) {
    if (S.left.root) return;
    S.left.kind  = kind;
    S.left.title = title;
    S.left.rows  = rowsOf;
    sheetSideSet(S, PANEL_PEEK);
}

void sheetSideSet(Sheet &S, uint8_t state) {
    SheetPanel &P = S.left;
    if (state == PANEL_HIDDEN && !P.root) return;
    // A panel just made has not been laid out: lv_obj_get_y() would say 0, and
    // the first slide came down from the top of the screen (owner, N9).
    const bool made = !P.root;
    if (made) sideCreate(S);
    if (state == P.state) return;
    const uint8_t was = P.state;
    P.state = state;
    const int32_t from = made ? panelY(S, P, PANEL_HIDDEN) : disarm(P);   // as SETTINGS
    carets(P);
    if (state == PANEL_OPEN) {
        lv_obj_move_foreground(P.track);
        if (S.right.state == PANEL_OPEN) sheetRightSet(S, PANEL_PEEK);
    }
    lv_anim_delete(P.root, slideExec);
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var     (&a, P.root);
    lv_anim_set_values  (&a, from, panelY(S, P, state));
    lv_anim_set_duration(&a, (was == PANEL_HIDDEN || state == PANEL_HIDDEN) ? PEEK_MS : OPEN_MS);
    lv_anim_set_path_cb (&a, state == PANEL_HIDDEN ? lv_anim_path_ease_in : lv_anim_path_ease_out);
    lv_anim_set_exec_cb (&a, slideExec);
    if (state == PANEL_OPEN) lv_anim_set_completed_cb(&a, sideArmCb);
    lv_anim_start(&a);
}

void sheetSideFold(Sheet &S) { if (S.left.state == PANEL_OPEN) sheetSideSet(S, PANEL_PEEK); }

// With the sheet, or when its view goes (owner, N11): open, it vanishes in the
// same frame; showing its tab (or on its way down), it slides out of sight on
// its own and deletes itself there.
void sheetSideEnd(Sheet &S) {
    letGo(S.left, S.left.state != PANEL_OPEN);   // open, it goes at once; else (on its way down too) it slides
    S.left = SheetPanel();
    if (S.spec && S.spec->sideGone) S.spec->sideGone();
}

} // namespace Kit
