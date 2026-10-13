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

lv_obj_t *confirmButton(lv_obj_t *parent, const char *txt, uint32_t bg, uint32_t ink, uint32_t edge,
                        lv_event_cb_t cb) {
    const UIPalette &p = UI::pal();
    lv_obj_t *b = plain(parent);
    lv_obj_add_flag(b, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_size(b, mm(24.0f), UI::minTouch());
    lv_obj_set_style_radius(b, mm(1.6f), 0);
    lv_obj_set_style_bg_opa(b, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(b, UI::c(bg), 0);
    lv_obj_set_style_bg_color(b, UI::c(UI::mix(bg, p.TEXT, 20)), UI::part(LV_PART_MAIN, LV_STATE_PRESSED));
    lv_obj_set_style_border_width(b, LV_MAX(1, mm(0.25f)), 0);
    lv_obj_set_style_border_color(b, UI::c(edge), 0);
    lv_obj_t *l = makeLabel(b, UI::type().NAME, ink);
    lv_label_set_text(l, txt);
    lv_obj_center(l);
    lv_obj_add_event_cb(b, cb, LV_EVENT_CLICKED, nullptr);
    return b;
}

void confirmRemove() {
    confirmClose();
    if (!CardPopup::isOpen() || !cardOf(h.surface)) return;
    const UIPalette &p = UI::pal();
    const UIType    &t = UI::type();
    // The dim, which also takes every tap that misses the box: Cancel.
    s_confirm = plain(lv_layer_top());
    lv_obj_set_size(s_confirm, lv_pct(100), lv_pct(100));
    lv_obj_set_style_bg_color(s_confirm, UI::c(p.GROUND), 0);
    lv_obj_set_style_bg_opa(s_confirm, LV_OPA_60, 0);
    lv_obj_add_flag(s_confirm, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(s_confirm, [](lv_event_t *e) {
        if (lv_event_get_target(e) == lv_event_get_current_target(e)) lv_async_call([](void *) { confirmClose(); }, nullptr);
    }, LV_EVENT_CLICKED, nullptr);

    lv_obj_t *box = plain(s_confirm);
    lv_obj_add_flag(box, LV_OBJ_FLAG_CLICKABLE);   // a tap on it is not a tap outside
    lv_obj_set_width(box, LV_MIN(lv_area_get_width(&s.winRect) * 3 / 4, mm(90.0f)));
    lv_obj_set_height(box, LV_SIZE_CONTENT);
    lv_obj_center(box);
    lv_obj_set_style_radius(box, UI::sc(UI::met().RADIUS), 0);
    lv_obj_set_style_bg_opa(box, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(box, UI::c(p.SURFACE_ALT), 0);
    lv_obj_set_style_border_width(box, LV_MAX(2, mm(0.4f)), 0);
    lv_obj_set_style_border_color(box, UI::c(p.ST_BAD), 0);
    lv_obj_set_style_pad_all(box, mm(4.0f), 0);
    lv_obj_set_style_pad_row(box, mm(2.4f), 0);
    lv_obj_set_flex_flow(box, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(box, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    lv_obj_t *title = makeLabel(box, t.NAME, p.TEXT);
    lv_label_set_text(title, "Remove this card?");
    // The owner's words (K17 round 3).
    char buf[200];
    snprintf(buf, sizeof(buf), "The card \"%s\" will be removed from the dashboard, any other "
                               "instances of this card will be unaffected.", s.name);
    for (const char *txt : { (const char *)buf, "Removed cards can be restored at any time." }) {
        lv_obj_t *body = makeLabel(box, t.TAG, p.TEXT_DIM);
        lv_label_set_long_mode(body, LV_LABEL_LONG_WRAP);
        lv_obj_set_width(body, lv_pct(100));
        lv_obj_set_style_text_align(body, LV_TEXT_ALIGN_CENTER, 0);
        lv_label_set_text(body, txt);
    }

    lv_obj_t *row = plain(box);
    lv_obj_set_size(row, lv_pct(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_top(row, mm(1.2f), 0);
    confirmButton(row, "Cancel", p.SURFACE, p.TEXT, UI::mix(p.SURFACE, p.TEXT, 30), [](lv_event_t *) {
        lv_async_call([](void *) { confirmClose(); }, nullptr);
    });
    confirmButton(row, "Remove", p.ST_BAD, p.SURFACE, p.ST_BAD, [](lv_event_t *) {
        lv_async_call([](void *) {
            confirmClose();
            Card *held = CardPopup::isOpen() ? cardOf(h.surface) : nullptr;
            if (held && CardAdded::remove(held->id())) CardPopup::close();
        }, nullptr);
    });
}

void removeCardCb(lv_event_t *ev) {
    (void)ev;
    lv_async_call([](void *) { confirmRemove(); }, nullptr);
}

} // namespace popup
