// Card - the active frames (2.8 round 4). Split from Card.cpp, which was
// already past the structure check's 1,000 lines; the class is one, its edge's
// "on" states are here. See Card::activeFrame() in include/Cards/Card.h.
#include "Cards/Card.h"
#include "UI/UITokens.h"

// ---------------------------------------------------------------------------
// THE ACTIVE FRAMES (owner, 2.8 round 4): "on" shown on the card's edge in
// place of a fill - "an attempt to move away from the standard Fill option".
//   GLOW     a ring in the state colour and, outside it, a line in the
//            scheme's darkest: an illuminated instrument, a tight ring of light
//   BRACKET  top and bottom thick, the sides thin: framing, not boxing
//   CORNER   a ring, and the top-left and bottom-right corners twice as round:
//            the shape itself changes, seen from across the room
// LVGL's border and outline draw the first two. It has one radius for all four
// corners, so the corner cut lays a CAP over each of the two corners: a square
// of the page's ground with the larger corner's arc - a ring of the ground
// outside it (covering the card's own corner) and the state's ring on it.
// ---------------------------------------------------------------------------
void Card::makeCornerCaps() {
    if (!_surface || _cap[0]) return;
    const int32_t r2 = 2 * UI::sc(UI::met().RADIUS), w = r2 / 2;
    for (uint8_t i = 0; i < 2; i++) {
        lv_obj_t *box = lv_obj_create(_surface);
        lv_obj_remove_style_all(box);
        lv_obj_set_size(box, r2, r2);
        lv_obj_add_flag(box, (lv_obj_flag_t)(LV_OBJ_FLAG_FLOATING | LV_OBJ_FLAG_IGNORE_LAYOUT | LV_OBJ_FLAG_HIDDEN));
        lv_obj_remove_flag(box, (lv_obj_flag_t)(LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));
        // The ground outside the larger corner: a ring from r2 outward.
        lv_obj_t *g = lv_obj_create(box);
        lv_obj_remove_style_all(g);
        lv_obj_set_size(g, 2 * (r2 + w), 2 * (r2 + w));
        lv_obj_set_style_radius(g, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_border_width(g, w, 0);
        lv_obj_set_pos(g, i ? -(r2 + w) : -w, i ? -(r2 + w) : -w);
        // The state's ring, on the larger corner.
        lv_obj_t *r = lv_obj_create(box);
        lv_obj_remove_style_all(r);
        lv_obj_set_size(r, 2 * r2, 2 * r2);
        lv_obj_set_style_radius(r, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_pos(r, i ? -r2 : 0, i ? -r2 : 0);
        for (lv_obj_t *o : { g, r }) lv_obj_remove_flag(o, (lv_obj_flag_t)(LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));
        _cap[i] = box;
    }
}

void Card::activeFrame(CardFrame f, uint32_t color) {
    if (!_surface || _pressed) return;   // the held look owns the edge
    const UIMetrics &m = UI::met();
    const bool corner = f == CardFrame::FRAME_CORNER && _cap[0];
    for (lv_obj_t *c : _cap) if (c) { if (corner) lv_obj_remove_flag(c, LV_OBJ_FLAG_HIDDEN);
                                      else        lv_obj_add_flag(c, LV_OBJ_FLAG_HIDDEN); }
    // The surface's border is drawn after its children (the theme's); under the
    // caps it must be drawn first, or it paints over them.
    if (_cap[0]) lv_obj_set_style_border_post(_surface, !corner, 0);
    if (f == CardFrame::FRAME_NONE) {
        lv_obj_set_style_border_side  (_surface, LV_BORDER_SIDE_FULL, 0);
        lv_obj_set_style_border_width (_surface, m.BORDER_W, 0);
        lv_obj_set_style_border_color (_surface, UI::border(), 0);
        lv_obj_set_style_outline_width(_surface, 0, 0);
        return;
    }
    const lv_color_t c = UI::c(color);
    const bool bracket = f == CardFrame::FRAME_BRACKET;
    lv_obj_set_style_border_side (_surface, bracket ? (lv_border_side_t)(LV_BORDER_SIDE_TOP | LV_BORDER_SIDE_BOTTOM)
                                                    : LV_BORDER_SIDE_FULL, 0);
    lv_obj_set_style_border_width(_surface, UI::sc(bracket ? m.BRACKET_W : m.ACTIVE_RING), 0);
    lv_obj_set_style_border_color(_surface, c, 0);
    // Outside the border: the glow's dark line, the bracket's thin sides.
    const bool edge = f == CardFrame::FRAME_GLOW || bracket;
    lv_obj_set_style_outline_width(_surface, edge ? UI::sc(m.ACTIVE_EDGE) : 0, 0);
    lv_obj_set_style_outline_pad  (_surface, 0, 0);
    lv_obj_set_style_outline_color(_surface, bracket ? c : UI::c(UI::darkest()), 0);
    if (!corner) return;
    // The caps sit on the surface's outer corners, past its border and padding.
    const int32_t sl = lv_obj_get_style_space_left(_surface, LV_PART_MAIN);
    const int32_t st = lv_obj_get_style_space_top(_surface, LV_PART_MAIN);
    const int32_t sr = lv_obj_get_style_space_right(_surface, LV_PART_MAIN);
    const int32_t sb = lv_obj_get_style_space_bottom(_surface, LV_PART_MAIN);
    lv_obj_align(_cap[0], LV_ALIGN_TOP_LEFT, -sl, -st);
    lv_obj_align(_cap[1], LV_ALIGN_BOTTOM_RIGHT, sr, sb);
    for (lv_obj_t *box : _cap) {
        lv_obj_t *g = lv_obj_get_child(box, 0), *r = lv_obj_get_child(box, 1);
        lv_obj_set_style_border_color(g, UI::c(UI::pal().GROUND), 0);
        lv_obj_set_style_border_color(r, c, 0);
        lv_obj_set_style_border_width(r, UI::sc(m.ACTIVE_RING), 0);
    }
}
