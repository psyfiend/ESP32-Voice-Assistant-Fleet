// The hold on a card and the tap catcher (2.12 P1, split from CardPopup.cpp).
#include "PopupState.h"

namespace popup {

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
        if (inDeck(pt) || (!panelOpen() && inWindow(pt))) info->res = false;
    } else if (code == LV_EVENT_PRESSED) {
        s_catcherFolded = false;
        if (panelOpen() && inWindow(s.pressStart)) {
            s_catcherFolded = true;
            if (s.deckState == DECK_OPEN) deckSet(DECK_PEEK);
            chartFold();
        }
    } else if (code == LV_EVENT_CLICKED) {
        if (s_catcherFolded) { s_catcherFolded = false; return; }
        CardPopup::close();
    }
}
} // namespace popup
