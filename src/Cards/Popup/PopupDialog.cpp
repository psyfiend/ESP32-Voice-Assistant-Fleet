// The Hide dialog (K96's Hide).
#include "PopupState.h"

namespace popup {

// "Hide from this page" (K96; K66's remove, for every card) ASKS IN
// A DIALOG YOU CANNOT MISS (a button that changed to "Sure?" would be easy to
// overlook). Over everything - the window, its panels - on the top
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
    // The words for Remove, as K96 has them.
    char buf[200];
    snprintf(buf, sizeof(buf), "The card \"%s\" will be hidden from this page, any other "
                               "instances of this card will be unaffected.", s.name);
    Kit::DialogSpec spec = {};
    spec.role    = Kit::DialogRole::DLG_DESTRUCTIVE;
    spec.title   = "Hide this card?";
    spec.body[0] = buf;
    spec.body[1] = "This card can be restored from the 'Page' \"View hidden cards\" menu.";
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
    spec.countdown = true;             // it closes with the window (closing hook): it says so
    s_confirm = Kit::dialog(spec);
}

// A member's Hide from the group and its dialog went with K160: Unpin this card
// loses nothing, so it asks nothing (PopupVisibility.cpp).

// A COPY IN A CLUSTER, REMOVED (K91): it is made on the glass, so
// removing it deletes it - after the same dialog. The card it was copied from
// is untouched.
void confirmRemoveCopy() {
    confirmClose();
    // The held tile, or - in a cluster's window - the member whose view this is.
    Card *held = CardPopup::isOpen() ? visibilitySubject() : nullptr;
    if (!held || !CardAdded::isMember(held->id())) return;
    char buf[200];
    snprintf(buf, sizeof(buf), "This copy of \"%s\" will be removed from the group and deleted; the card "
                               "it was copied from is unaffected.", held->shownName(CardLabel::LBL_NAME));
    Kit::DialogSpec spec = {};
    spec.role    = Kit::DialogRole::DLG_DESTRUCTIVE;
    spec.title   = "Remove this copy?";
    spec.body[0] = buf;
    spec.buttons[0] = { "Cancel", Kit::ButtonRole::BTN_CANCEL, confirmCancelCb };
    spec.buttons[1] = { "Remove", Kit::ButtonRole::BTN_DESTRUCTIVE, [](lv_event_t *) {
        lv_async_call([](void *) {
            confirmClose();
            Card *held = CardPopup::isOpen() ? visibilitySubject() : nullptr;
            if (!held) return;
            char in[CARD_ID_MAX + 1] = "";
            if (const char *c = CardAdded::clusterOf(held->id())) snprintf(in, sizeof(in), "%s", c);
            if (!CardAdded::removeMember(held->id())) return;
            if (in[0]) CardAdded::emptied(in);   // the last one out: the group goes
            CardPopup::close();
        }, nullptr);
    } };
    spec.nButtons = 2;
    spec.width    = LV_MIN(lv_area_get_width(&s.winRect) * 3 / 4, mm(UI::shape().W_DIALOG_MAX));
    spec.outside  = confirmCancelCb;
    spec.countdown = true;
    s_confirm = Kit::dialog(spec);
}

void removeCopyCb(lv_event_t *ev) {
    (void)ev;
    lv_async_call([](void *) { confirmRemoveCopy(); }, nullptr);
}

// A CARD MADE ON THE GLASS, REMOVED FROM THE PAGE (a card copied out of a
// cluster can go without being hidden): deleted, entry and all - it does not join the hidden cards. A card MOVED out of a
// cluster is never offered this: it is the card itself, and goes back.
void confirmRemoveFromPage() {
    confirmClose();
    Card *held = CardPopup::isOpen() ? cardOf(h.surface) : nullptr;
    if (!held || !held->hasId() || !CardAdded::isAdded(held->id()) || CardAdded::movedHere(held->id())) return;
    char buf[200];
    snprintf(buf, sizeof(buf), "The card \"%s\" was made on this board: it will be deleted with its settings, "
                               "not kept in the hidden cards. Any other card of it is unaffected.", s.name);
    Kit::DialogSpec spec = {};
    spec.role    = Kit::DialogRole::DLG_DESTRUCTIVE;
    spec.title   = "Remove this card?";
    spec.body[0] = buf;
    spec.buttons[0] = { "Cancel", Kit::ButtonRole::BTN_CANCEL, confirmCancelCb };
    spec.buttons[1] = { "Remove", Kit::ButtonRole::BTN_DESTRUCTIVE, [](lv_event_t *) {
        lv_async_call([](void *) {
            confirmClose();
            Card *held = CardPopup::isOpen() ? cardOf(h.surface) : nullptr;
            if (held && CardAdded::drop(held->id())) CardPopup::close();
        }, nullptr);
    } };
    spec.nButtons = 2;
    spec.width    = LV_MIN(lv_area_get_width(&s.winRect) * 3 / 4, mm(UI::shape().W_DIALOG_MAX));
    spec.outside  = confirmCancelCb;
    spec.countdown = true;
    s_confirm = Kit::dialog(spec);
}

void removeFromPageCb(lv_event_t *ev) {
    (void)ev;
    lv_async_call([](void *) { confirmRemoveFromPage(); }, nullptr);
}

// MOVE, COPY, CHANGE OR RESTORE, ASKED FIRST: what will happen is said in words
// before anything changes (K137).
// `go` does it; the window then closes.
static void (*s_go)() = nullptr;
void confirmAsk(Kit::DialogRole role, const char *title, const char *const *lines, uint8_t n, const char *verb,
                Kit::ButtonRole verbRole, void (*go)()) {
    confirmClose();
    if (!CardPopup::isOpen() || !go) return;
    s_go = go;
    Kit::DialogSpec spec = {};
    spec.role    = role;
    spec.title   = title;
    for (uint8_t i = 0; i < n && i < 4; i++) spec.body[i] = lines[i];
    spec.buttons[0] = { "Cancel", Kit::ButtonRole::BTN_CANCEL, confirmCancelCb };
    spec.buttons[1] = { verb, verbRole, [](lv_event_t *) {
        lv_async_call([](void *) {
            confirmClose();
            if (CardPopup::isOpen() && s_go) s_go();
        }, nullptr);
    } };
    spec.nButtons = 2;
    spec.width    = LV_MIN(lv_area_get_width(&s.winRect) * 3 / 4, mm(UI::shape().W_DIALOG_MAX));
    spec.outside  = confirmCancelCb;
    spec.countdown = true;
    s_confirm = Kit::dialog(spec);
}

bool confirmGoDebug() {
    if (!s_confirm || !s_go || !CardPopup::isOpen()) return false;
    confirmClose();
    s_go();
    return true;
}

void hideCardCb(lv_event_t *ev) {
    (void)ev;
    lv_async_call([](void *) { confirmHide(); }, nullptr);
}

} // namespace popup
