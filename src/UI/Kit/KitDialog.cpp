// The dialog (2.12 P3): a question over everything, from a spec. First made for
// "Remove card" (K66, K69): a dialog you cannot miss, where a button that turned
// into "Sure?" was easy to overlook. Rare, so the dim's one full-screen redraw
// is worth it here.
#include "UI/Kit.h"

namespace Kit {

// A tap on the dim itself - not on the box, which takes its own taps - is the
// spec's `outside`, kept in the dim's user data.
static void dimCb(lv_event_t *e) {
    lv_obj_t *dim = (lv_obj_t *)lv_event_get_current_target(e);
    if (lv_event_get_target(e) != dim) return;
    if (lv_event_cb_t cb = (lv_event_cb_t)lv_obj_get_user_data(dim)) cb(e);
}

lv_obj_t *dialog(const DialogSpec &spec) {
    lv_obj_t *dim = plain(lv_layer_top());
    lv_obj_set_size(dim, lv_pct(100), lv_pct(100));
    add(dim, KitStyle::KS_DIALOG_DIM);
    lv_obj_add_flag(dim, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_user_data(dim, (void *)spec.outside);
    lv_obj_add_event_cb(dim, dimCb, LV_EVENT_CLICKED, nullptr);

    lv_obj_t *box = plain(dim);
    lv_obj_add_flag(box, LV_OBJ_FLAG_CLICKABLE);   // a tap on it is not a tap outside
    lv_obj_set_width(box, spec.width);
    lv_obj_set_height(box, LV_SIZE_CONTENT);
    lv_obj_center(box);
    add(box, KitStyle::KS_DIALOG_BOX_DESTRUCTIVE);   // the only role so far (DialogRole)

    lv_obj_t *title = label(box, KitStyle::KS_LABEL_NAME);
    lv_label_set_text(title, spec.title);
    for (const char *txt : spec.body) {
        if (!txt) break;
        lv_obj_t *body = label(box, KitStyle::KS_DIALOG_BODY);
        lv_label_set_long_mode(body, LV_LABEL_LONG_WRAP);
        lv_obj_set_width(body, lv_pct(100));
        lv_label_set_text(body, txt);
    }

    lv_obj_t *row = plain(box);
    lv_obj_set_size(row, lv_pct(100), LV_SIZE_CONTENT);
    add(row, KitStyle::KS_DIALOG_ROW);
    for (uint8_t i = 0; i < spec.nButtons && i < 2; i++)
        button(row, spec.buttons[i].role, spec.buttons[i].text, UI::mm(UI::shape().W_DIALOG_BTN),
               UI::minTouch(), spec.buttons[i].cb);
    return dim;
}

} // namespace Kit
