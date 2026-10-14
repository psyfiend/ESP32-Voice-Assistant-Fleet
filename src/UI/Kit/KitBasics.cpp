// The kit's basics (2.12 P3): the bare object everything starts from, and labels.
#include "UI/Kit.h"

namespace Kit {

lv_obj_t *plain(lv_obj_t *parent) {
    lv_obj_t *o = lv_obj_create(parent);
    lv_obj_remove_style_all(o);
    lv_obj_clear_flag(o, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(o, LV_OBJ_FLAG_CLICKABLE);
    return o;
}

lv_obj_t *label(lv_obj_t *parent, const lv_font_t *f, uint32_t hex) {
    lv_obj_t *l = lv_label_create(parent);
    lv_obj_set_style_text_font (l, f, 0);
    lv_obj_set_style_text_color(l, UI::c(hex), 0);
    lv_label_set_text(l, "");
    return l;
}

lv_obj_t *label(lv_obj_t *parent, KitStyle look) {
    lv_obj_t *l = lv_label_create(parent);
    add(l, look);
    lv_label_set_text(l, "");
    return l;
}

lv_obj_t *factList(lv_obj_t *parent) {
    const UIShape &k = UI::shape();
    lv_obj_t *list = plain(parent);
    lv_obj_set_size(list, lv_pct(100), lv_pct(100));
    lv_obj_set_style_pad_hor(list, UI::mm(k.S10), 0);
    lv_obj_add_flag(list, LV_OBJ_FLAG_SCROLLABLE);
    UI::tameScroll(list);
    lv_obj_set_flex_flow(list, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(list, UI::mm(k.S5), 0);
    return list;
}

// TWO LEFT-ALIGNED COLUMNS (owner, TEST_2.12): the words, then the value
// beside them - a value that wraps keeps to its column, the words at its first
// line. The gap between them is half what it was (S8 3.0 mm, now S5 1.6, the
// nearest step). factColumns() then gives every row's words one width.
lv_obj_t *factRow(lv_obj_t *list, const char *what, const char *value) {
    lv_obj_t *r = plain(list);
    lv_obj_set_size(r, lv_pct(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(r, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(r, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_column(r, UI::mm(UI::shape().S5), 0);
    lv_obj_t *a = label(r, KitStyle::KS_LABEL_TAG_DIM);
    lv_label_set_text(a, what);
    lv_obj_t *b = label(r, KitStyle::KS_LABEL_TAG);
    lv_label_set_long_mode(b, LV_LABEL_LONG_WRAP);
    lv_obj_set_flex_grow(b, 1);
    lv_label_set_text(b, (value && value[0]) ? value : "-");
    return b;
}

void factColumns(lv_obj_t *list) {
    if (!list) return;
    lv_obj_update_layout(list);
    const int32_t most = lv_obj_get_content_width(list) / 2;
    const uint32_t n = lv_obj_get_child_count(list);
    int32_t w = 0;
    for (uint32_t i = 0; i < n; i++) {
        lv_obj_t *a = lv_obj_get_child(lv_obj_get_child(list, (int32_t)i), 0);
        if (a) w = LV_MAX(w, textW(lv_label_get_text(a), lv_obj_get_style_text_font(a, LV_PART_MAIN)) + 1);
    }
    w = LV_MIN(w, most);
    for (uint32_t i = 0; i < n; i++)
        if (lv_obj_t *a = lv_obj_get_child(lv_obj_get_child(list, (int32_t)i), 0)) lv_obj_set_width(a, w);
}

lv_obj_t *item(lv_obj_t *parent) {
    lv_obj_t *o = plain(parent);
    add(o, KitStyle::KS_ITEM);
    add(o, KitStyle::KS_ITEM_PRESSED, UI::part(LV_PART_MAIN, LV_STATE_PRESSED));
    lv_obj_add_flag(o, LV_OBJ_FLAG_CLICKABLE);
    return o;
}

int32_t textW(const char *txt, const lv_font_t *f) {
    lv_point_t sz = {0, 0};
    lv_text_get_size(&sz, txt, f, 0, 0, LV_COORD_MAX, LV_TEXT_FLAG_NONE);
    return sz.x;
}

} // namespace Kit
