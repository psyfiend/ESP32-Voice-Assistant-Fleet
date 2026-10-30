// The dialog (2.12 P3): a question over everything, from a spec. First made for
// "Remove card" (K66, K69): a dialog you cannot miss, where a button that turned
// into "Sure?" was easy to overlook. Rare, so the dim's one full-screen redraw
// is worth it here.
#include "UI/Kit.h"
#include "UI/Sheet.h"

namespace Kit {

// ANYTHING THAT CLOSES ITSELF SAYS SO THE SAME WAY (owner, 2026-10-14; #23): a
// dialog that goes with the sheet under it shows that sheet's countdown bar,
// kept in step by a timer of its own that goes with the bar.
static void barTick(lv_timer_t *t) {
    if (const Sheet *S = sheetShowing()) countdownSet((lv_obj_t *)lv_timer_get_user_data(t), *S);
}

static void barGone(lv_event_t *e) {
    if (lv_timer_t *t = (lv_timer_t *)lv_event_get_user_data(e)) lv_timer_delete(t);
}

// Only while the sheet shows its own (the system settings can hide it, T6).
lv_obj_t *countdownFollow(lv_obj_t *box, int32_t boxW) {
    const Sheet *S = sheetShowing();
    if (!S || !S->autoBar) return nullptr;
    lv_obj_t *bar = countdownBar(box, boxW);
    countdownSet(bar, *S);
    lv_timer_t *t = lv_timer_create(barTick, SHEET_TICK_MS, bar);
    lv_obj_add_event_cb(bar, barGone, LV_EVENT_DELETE, t);
    return bar;
}

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
    // THE BUTTONS SHARE THE ROW (#96): W_DIALOG_BTN each where it fits, else an
    // equal share of the row. The row spaces them evenly, so what is left over
    // goes into n + 1 spaces: a share leaves S7 between and beside them. The
    // row is the box less its padding and border, read from the box's role.
    const uint8_t n    = LV_MIN(spec.nButtons, (uint8_t)2);
    const int32_t rowW = spec.width - lv_obj_get_style_space_left(box, LV_PART_MAIN)
                                    - lv_obj_get_style_space_right(box, LV_PART_MAIN);
    const int32_t gap  = UI::mm(UI::shape().S7);
    const int32_t btnW = n ? LV_MIN(UI::mm(UI::shape().W_DIALOG_BTN), (rowW - (n + 1) * gap) / n) : 0;
    for (uint8_t i = 0; i < n; i++)
        button(row, spec.buttons[i].role, spec.buttons[i].text, btnW, UI::minTouch(), spec.buttons[i].cb);

    if (spec.countdown) countdownFollow(box, spec.width);
    return dim;
}

} // namespace Kit
