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
// line. S5 between them until factColumns() centres the two and spaces them.
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

// THE TWO COLUMNS, CENTRED (owner, 2026-10-14, on this branch's first build):
// both left-justified, the words as wide as the longest words, the values as
// wide as the longest value, and the room left over shared equally - the same
// space from each wall and between the two. Values too long to leave room for
// that: the space is S5 each way, and a value wraps in its column.
static int32_t labelW(lv_obj_t *l) {
    return l ? textW(lv_label_get_text(l), lv_obj_get_style_text_font(l, LV_PART_MAIN)) + 1 : 0;
}

void factColumns(lv_obj_t *list) {
    if (!list) return;
    lv_obj_update_layout(list);
    const int32_t full = lv_obj_get_width(list);
    const uint32_t n = lv_obj_get_child_count(list);
    int32_t words = 0, values = 0;
    for (uint32_t i = 0; i < n; i++) {
        lv_obj_t *r = lv_obj_get_child(list, (int32_t)i);
        words  = LV_MAX(words,  labelW(lv_obj_get_child(r, 0)));
        values = LV_MAX(values, labelW(lv_obj_get_child(r, 1)));
    }
    words = LV_MIN(words, full / 2);
    const int32_t space = LV_MAX(UI::mm(UI::shape().S5), (full - words - values) / 3);
    lv_obj_set_style_pad_hor(list, space, 0);
    for (uint32_t i = 0; i < n; i++) {
        lv_obj_t *r = lv_obj_get_child(list, (int32_t)i);
        lv_obj_set_style_pad_column(r, space, 0);
        if (lv_obj_t *a = lv_obj_get_child(r, 0)) lv_obj_set_width(a, words);
    }
}

lv_obj_t *item(lv_obj_t *parent) {
    lv_obj_t *o = plain(parent);
    add(o, KitStyle::KS_ITEM);
    add(o, KitStyle::KS_ITEM_PRESSED, UI::part(LV_PART_MAIN, LV_STATE_PRESSED));
    lv_obj_add_flag(o, LV_OBJ_FLAG_CLICKABLE);
    return o;
}

// COMING's dashed edge, drawn over the item: its four sides dashed (LVGL dashes
// straight lines only), its corners plain arcs, all inside its own area.
static void comingDraw(lv_event_t *ev) {
    lv_obj_t *o = (lv_obj_t *)lv_event_get_current_target(ev);
    lv_layer_t *layer = lv_event_get_layer(ev);
    lv_area_t a;
    lv_obj_get_coords(o, &a);
    const UIShape &k = UI::shape();
    const int32_t w  = LV_MAX(2, UI::mm(k.LINE_MID));
    const int32_t in = w / 2;
    const int32_t r  = LV_MIN(lv_obj_get_style_radius(o, LV_PART_MAIN),
                              LV_MIN(lv_area_get_width(&a), lv_area_get_height(&a)) / 2);
    const lv_color_t c = UI::c(UI::pal().ACCENT);
    lv_draw_line_dsc_t d;
    lv_draw_line_dsc_init(&d);
    d.color = c;
    d.width = w;
    d.dash_width = d.dash_gap = LV_MAX(2, UI::mm(k.DASH) * 2);
    auto side = [&](int32_t x1, int32_t y1, int32_t x2, int32_t y2) {
        d.p1.x = x1; d.p1.y = y1; d.p2.x = x2; d.p2.y = y2;
        lv_draw_line(layer, &d);
    };
    side(a.x1 + r, a.y1 + in, a.x2 - r, a.y1 + in);
    side(a.x1 + r, a.y2 - in, a.x2 - r, a.y2 - in);
    side(a.x1 + in, a.y1 + r, a.x1 + in, a.y2 - r);
    side(a.x2 - in, a.y1 + r, a.x2 - in, a.y2 - r);
    if (r <= 0) return;
    lv_draw_arc_dsc_t arc;
    lv_draw_arc_dsc_init(&arc);
    arc.color  = c;
    arc.width  = w;
    arc.radius = (uint16_t)r;
    const struct { int32_t x, y; int16_t from; } corners[4] = {
        { a.x1 + r, a.y1 + r, 180 }, { a.x2 - r, a.y1 + r, 270 },
        { a.x2 - r, a.y2 - r, 0 },   { a.x1 + r, a.y2 - r, 90 } };
    for (const auto &q : corners) {
        arc.center.x = q.x;
        arc.center.y = q.y;
        arc.start_angle = q.from;
        arc.end_angle   = q.from + 90;
        lv_draw_arc(layer, &arc);
    }
}

void itemMark(lv_obj_t *o, ItemMark m) {
    if (!o) return;
    lv_obj_remove_style(o, style(KitStyle::KS_ITEM_DOOMED), 0);
    lv_obj_remove_style(o, style(KitStyle::KS_ITEM_COMING), 0);
    lv_obj_remove_event_cb(o, comingDraw);
    if (m == ItemMark::MARK_NONE) return;
    unfinish(o);
    add(o, m == ItemMark::MARK_DOOMED ? KitStyle::KS_ITEM_DOOMED : KitStyle::KS_ITEM_COMING);
    if (m == ItemMark::MARK_COMING) lv_obj_add_event_cb(o, comingDraw, LV_EVENT_DRAW_MAIN_END, nullptr);
    lv_obj_invalidate(o);
}

const lv_font_t *fitLabel(lv_obj_t *l, const char *text, int32_t w, const lv_font_t *const *faces, uint8_t n) {
    if (!l || !text || !faces || !n) return nullptr;
    const lv_font_t *f = faces[n - 1];
    for (uint8_t i = 0; i < n; i++)
        if (faces[i] && textW(text, faces[i]) <= w) { f = faces[i]; break; }
    lv_obj_set_style_text_font(l, f, 0);
    lv_label_set_long_mode(l, LV_LABEL_LONG_DOT);
    lv_obj_set_height(l, lv_font_get_line_height(f));
    lv_label_set_text(l, text);
    return f;
}

int32_t textW(const char *txt, const lv_font_t *f) {
    lv_point_t sz = {0, 0};
    lv_text_get_size(&sz, txt, f, 0, 0, LV_COORD_MAX, LV_TEXT_FLAG_NONE);
    return sz.x;
}

} // namespace Kit
