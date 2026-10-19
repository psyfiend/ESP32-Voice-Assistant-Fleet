// The Hide dialog (2.12 P1, split from CardPopup.cpp; 2.11 round 3: K96's Hide).
#include "PopupState.h"

namespace popup {

// "Hide from this page" (K96; K66's remove, for every card since 2.11) ASKS IN
// A DIALOG YOU CANNOT MISS (owner, K17 run 2: a button that changed to "Sure?"
// was easy to overlook). Over everything - the window, its panels - on the top
// layer, the rest dimmed; Cancel, or a tap outside, leaves it; Hide sends the
// card to the page's pool and closes the window, and the page is built again
// without it. Rare, so the dim's one full-screen redraw is worth it here (the
// window itself has none - "Usable first").
lv_obj_t *s_confirm = nullptr;

void confirmClose() {
    if (s_confirm) { lv_obj_delete(s_confirm); s_confirm = nullptr; }
}

void confirmCancelCb(lv_event_t *) {
    lv_async_call([](void *) { confirmClose(); }, nullptr);
}

void confirmHide() {
    confirmClose();
    Card *held = CardPopup::isOpen() ? cardOf(h.surface) : nullptr;
    if (!held || !held->hasId()) return;
    // The owner's words for Remove (K17 round 3), in K96's.
    char buf[200];
    snprintf(buf, sizeof(buf), "The card \"%s\" will be hidden from this page, any other "
                               "instances of this card will be unaffected.", s.name);
    Kit::DialogSpec spec = {};
    spec.role    = Kit::DialogRole::DLG_DESTRUCTIVE;
    spec.title   = "Hide this card?";
    spec.body[0] = buf;
    spec.body[1] = "Hidden cards come back from this page's list: tap the page's name in the header.";
    spec.buttons[0] = { "Cancel", Kit::ButtonRole::BTN_CANCEL, confirmCancelCb };
    spec.buttons[1] = { "Hide", Kit::ButtonRole::BTN_DESTRUCTIVE, [](lv_event_t *) {
        lv_async_call([](void *) {
            confirmClose();
            Card *held = CardPopup::isOpen() ? cardOf(h.surface) : nullptr;
            if (held && CardAdded::hide(held->id())) CardPopup::close();
        }, nullptr);
    } };
    spec.nButtons = 2;
    spec.width    = LV_MIN(lv_area_get_width(&s.winRect) * 3 / 4, mm(UI::shape().W_DIALOG_MAX));
    spec.outside  = confirmCancelCb;   // a tap that misses the box: Cancel
    spec.countdown = true;             // it closes with the window (closing hook): it says so (#23)
    s_confirm = Kit::dialog(spec);
}

void hideCardCb(lv_event_t *ev) {
    (void)ev;
    lv_async_call([](void *) { confirmHide(); }, nullptr);
}

} // namespace popup
