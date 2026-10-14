// Buttons (2.12 P3): a face, a pressed tint and the words' ink, per role.
#include "UI/Kit.h"

namespace Kit {

struct ButtonLook { KitStyle face, pressed, ink; };

static ButtonLook lookOf(ButtonRole role) {
    switch (role) {
        case ButtonRole::BTN_DESTRUCTIVE:
            return { KitStyle::KS_BTN_DESTRUCTIVE, KitStyle::KS_BTN_DESTRUCTIVE_PRESSED, KitStyle::KS_BTN_DESTRUCTIVE_INK };
        case ButtonRole::BTN_CANCEL:
        default:
            return { KitStyle::KS_BTN_CANCEL, KitStyle::KS_BTN_CANCEL_PRESSED, KitStyle::KS_BTN_CANCEL_INK };
    }
}

lv_obj_t *button(lv_obj_t *parent, ButtonRole role, const char *text, int32_t w, int32_t h,
                 lv_event_cb_t cb) {
    const ButtonLook look = lookOf(role);
    lv_obj_t *b = plain(parent);
    lv_obj_add_flag(b, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_size(b, w, h);
    add(b, look.face);
    add(b, look.pressed, UI::part(LV_PART_MAIN, LV_STATE_PRESSED));
    lv_obj_t *l = label(b, look.ink);
    lv_label_set_text(l, text);
    lv_obj_center(l);
    if (cb) lv_obj_add_event_cb(b, cb, LV_EVENT_CLICKED, nullptr);
    return b;
}

} // namespace Kit
