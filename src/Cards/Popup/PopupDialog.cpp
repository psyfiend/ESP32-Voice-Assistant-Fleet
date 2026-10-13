// The Remove dialog (2.12 P1, split from CardPopup.cpp).
#include "PopupState.h"

namespace popup {

// "Remove card" (K66) ASKS IN A DIALOG YOU CANNOT MISS (owner, K17 run 2: a
// button that changed to "Sure?" was easy to overlook). Over everything - the
// window, its panels - on the top layer, the rest dimmed; Cancel, or a tap
// outside, leaves it; Remove removes and closes the window, and the page is
// built again without the card. Rare, so the dim's one full-screen redraw is
// worth it here (the window itself has none - "Usable first").
lv_obj_t *s_confirm = nullptr;

void confirmClose() {
    if (s_confirm) { lv_obj_delete(s_confirm); s_confirm = nullptr; }
}

void confirmCancelCb(lv_event_t *) {
    lv_async_call([](void *) { confirmClose(); }, nullptr);
}

void confirmRemove() {
    confirmClose();
    if (!CardPopup::isOpen() || !cardOf(h.surface)) return;
    // The owner's words (K17 round 3).
    char buf[200];
    snprintf(buf, sizeof(buf), "The card \"%s\" will be removed from the dashboard, any other "
                               "instances of this card will be unaffected.", s.name);
    Kit::DialogSpec spec = {};
    spec.role    = Kit::DialogRole::DLG_DESTRUCTIVE;
    spec.title   = "Remove this card?";
    spec.body[0] = buf;
    spec.body[1] = "Removed cards can be restored at any time.";
    spec.buttons[0] = { "Cancel", Kit::ButtonRole::BTN_CANCEL, confirmCancelCb };
    spec.buttons[1] = { "Remove", Kit::ButtonRole::BTN_DESTRUCTIVE, [](lv_event_t *) {
        lv_async_call([](void *) {
            confirmClose();
            Card *held = CardPopup::isOpen() ? cardOf(h.surface) : nullptr;
            if (held && CardAdded::remove(held->id())) CardPopup::close();
        }, nullptr);
    } };
    spec.nButtons = 2;
    spec.width    = LV_MIN(lv_area_get_width(&s.winRect) * 3 / 4, mm(UI::shape().W_DIALOG_MAX));
    spec.outside  = confirmCancelCb;   // a tap that misses the box: Cancel
    s_confirm = Kit::dialog(spec);
}

void removeCardCb(lv_event_t *ev) {
    (void)ev;
    lv_async_call([](void *) { confirmRemove(); }, nullptr);
}

} // namespace popup
