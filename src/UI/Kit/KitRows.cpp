// Rows (2.12 P3, from the card window's SETTINGS and left panel - owner,
// 2026-10-07): a row's words on the left and its control on the right. Choices
// are DROPDOWNS (a row is shorter than a line of chips, and a list of choices
// can grow); an on/off is a CHECKBOX; a row that does something is a BUTTON.
// A row that does not work yet is shown greyed, so the whole list can be seen.
#include "UI/Kit.h"
#include <string.h>

namespace Kit {

int32_t controlH() { return UI::mm(UI::shape().H_CONTROL); }

// The widest line of a dropdown's choices.
static int32_t widestLine(const char *opts, const lv_font_t *f) {
    char line[48];
    int32_t w = 0;
    while (opts && *opts) {
        const char *e = strchr(opts, '\n');
        const size_t len = e ? (size_t)(e - opts) : strlen(opts);
        const size_t k = len < sizeof(line) - 1 ? len : sizeof(line) - 1;
        memcpy(line, opts, k);
        line[k] = '\0';
        w = LV_MAX(w, textW(line, f));
        opts = e ? e + 1 : nullptr;
    }
    return w;
}

int32_t controlW(const RowSpec &r) {
    const UIShape &k = UI::shape();
    if (r.kind == RowKind::ROW_CHECK) return controlH() + UI::mm(k.S3);
    const lv_font_t *f = UI::type().TAG;
    if (r.kind == RowKind::ROW_BUTTON) return textW(r.opts, f) + 2 * UI::mm(k.S7);
    return widestLine(r.opts, f) + 2 * UI::mm(k.S5) + textW(LV_SYMBOL_DOWN, f) + UI::mm(k.S5);
}

// AS WIDE AS ITS LONGEST ROW (owner): each row's words, a gap, its control -
// and the panel's sides, S7 each.
int32_t rowsNeedW(const RowSpec *r, uint8_t n) {
    int32_t w = 0;
    for (uint8_t i = 0; i < n; i++)
        w = LV_MAX(w, textW(r[i].label, UI::type().NAME) + UI::mm(UI::shape().S9) + controlW(r[i]));
    return w + 2 * UI::mm(UI::shape().S7);
}

lv_obj_t *row(lv_obj_t *parent, const char *what, bool live) {
    lv_obj_t *r = plain(parent);
    add(r, KitStyle::KS_ROW);
    lv_obj_t *l = label(r, live ? KitStyle::KS_LABEL_NAME : KitStyle::KS_LABEL_NAME_DIM);
    lv_label_set_text(l, what);
    return r;
}

lv_obj_t *dropdown(lv_obj_t *parent, const RowSpec &r) {
    lv_obj_t *dd = lv_dropdown_create(parent);
    lv_dropdown_set_options(dd, r.opts);
    lv_dropdown_set_selected(dd, r.sel);
    lv_obj_set_size(dd, controlW(r), controlH());
    add(dd, KitStyle::KS_DROP);
    add(dd, r.live ? KitStyle::KS_DROP_LIVE : KitStyle::KS_DROP_GREYED);
    add(dd, KitStyle::KS_DROP_ARROW, LV_PART_INDICATOR);
    if (lv_obj_t *list = lv_dropdown_get_list(dd)) {
        add(list, KitStyle::KS_LIST);
        UI::tameScroll(list);   // what still has to scroll does not spring back
        add(list, KitStyle::KS_LIST_CHOSEN,  UI::part(LV_PART_SELECTED, LV_STATE_CHECKED));
        add(list, KitStyle::KS_LIST_PRESSED, UI::part(LV_PART_SELECTED, LV_STATE_PRESSED));
    }
    if (!r.live) lv_obj_add_state(dd, LV_STATE_DISABLED);
    // A greyed row may come alive in the same window, so its callback is
    // attached anyway; a disabled dropdown sends none.
    if (r.cb) lv_obj_add_event_cb(dd, r.cb, LV_EVENT_VALUE_CHANGED, nullptr);
    return dd;
}

lv_obj_t *checkbox(lv_obj_t *parent, const RowSpec &r) {
    lv_obj_t *cb = lv_checkbox_create(parent);
    lv_checkbox_set_text(cb, "");
    add(cb, KitStyle::KS_CHECK);
    add(cb, KitStyle::KS_CHECK_BOX, LV_PART_INDICATOR);
    add(cb, KitStyle::KS_CHECK_BOX_TICKED, UI::part(LV_PART_INDICATOR, LV_STATE_CHECKED));
    if (r.sel) lv_obj_add_state(cb, LV_STATE_CHECKED);
    if (r.cb) lv_obj_add_event_cb(cb, r.cb, LV_EVENT_VALUE_CHANGED, nullptr);
    return cb;
}

// A button in a row (2.10e: Entity properties), the dropdown's size and frame.
// Greyed (K17's "Added"): no accent, dim words, nothing to tap.
lv_obj_t *rowButton(lv_obj_t *parent, const RowSpec &r) {
    lv_obj_t *b = plain(parent);
    lv_obj_add_flag(b, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_size(b, controlW(r), controlH());
    add(b, KitStyle::KS_ROWBTN);
    add(b, KitStyle::KS_ROWBTN_PRESSED, UI::part(LV_PART_MAIN, LV_STATE_PRESSED));
    add(b, r.live ? KitStyle::KS_ROWBTN_LIVE : KitStyle::KS_ROWBTN_GREYED);
    lv_obj_t *l = label(b, r.live ? KitStyle::KS_LABEL_TAG : KitStyle::KS_LABEL_TAG_DIM);
    lv_label_set_text(l, r.opts);
    lv_obj_center(l);
    if (!r.live) lv_obj_clear_flag(b, LV_OBJ_FLAG_CLICKABLE);
    else if (r.cb) lv_obj_add_event_cb(b, r.cb, LV_EVENT_CLICKED, nullptr);
    return b;
}

lv_obj_t *rowControl(lv_obj_t *row, const RowSpec &r) {
    if (r.kind == RowKind::ROW_CHECK)  return checkbox(row, r);
    if (r.kind == RowKind::ROW_BUTTON) return rowButton(row, r);
    return dropdown(row, r);
}

void rows(lv_obj_t *body, const RowSpec *r, uint8_t n) {
    for (uint8_t i = 0; i < n; i++) {
        lv_obj_t *ctl = rowControl(row(body, r[i].label, r[i].live), r[i]);
        if (r[i].out) *r[i].out = ctl;
    }
}

} // namespace Kit
