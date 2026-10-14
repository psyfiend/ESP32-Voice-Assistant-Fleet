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

lv_obj_t *factRow(lv_obj_t *list, const char *what, const char *value) {
    lv_obj_t *r = plain(list);
    lv_obj_set_size(r, lv_pct(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(r, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(r, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(r, UI::mm(UI::shape().S8), 0);
    lv_obj_t *a = label(r, KitStyle::KS_LABEL_TAG_DIM);
    lv_label_set_text(a, what);
    lv_obj_t *b = label(r, KitStyle::KS_LABEL_TAG);
    lv_label_set_long_mode(b, LV_LABEL_LONG_WRAP);
    lv_obj_set_flex_grow(b, 1);
    lv_obj_set_style_text_align(b, LV_TEXT_ALIGN_RIGHT, 0);
    lv_label_set_text(b, (value && value[0]) ? value : "-");
    return b;
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
