// Card - the active frames (2.8 rounds 4-5). Split from Card.cpp, which was
// already past the structure check's 1,000 lines; the class is one, its edge's
// "on" states are here. See Card::activeFrame() in include/Cards/Card.h.
#include "Cards/Card.h"
#include "UI/UITokens.h"

// ---------------------------------------------------------------------------
// THE ACTIVE FRAMES (owner, 2.8 rounds 4-5): "on" shown on the card's edge in
// place of a fill, and LIT - "a little lit up colour inside the border",
// "something to make the cards pop"; the futuristic HMI frames and Cyberpunk
// 2077's cursor brackets as the reference, not full cyber-UI.
//   BRACKET  heavy top and bottom bars, thin sides, light falling inward from
//            the bars
//   CORNER   a frame with the top-right and bottom-left corners cut away at
//            45 degrees (clear of the corner icon, top-left), light inward
//            from every edge
// DRAWN, never set as the card's border: a border is part of the layout, and
// round 4's thick bracket pushed the hero down. The light goes behind the
// card's contents (DRAW_MAIN_END), so text over it is never tinted; the bars,
// the frame and the cuts on top of them (DRAW_POST).
// ---------------------------------------------------------------------------
namespace {

// A band of light along one edge: the colour at `opa` on that edge, fading to
// nothing `depth` in. The card's own radius, so it follows its corners; where
// two bands cross, at a corner, the light is brighter - as it would be.
void band(lv_layer_t *layer, const lv_area_t &a, bool vertical, bool fromStart, lv_color_t c, lv_opa_t opa,
          int32_t r) {
    lv_draw_rect_dsc_t d;
    lv_draw_rect_dsc_init(&d);
    d.radius = r;
    d.bg_opa = LV_OPA_COVER;
    d.bg_grad.dir = vertical ? LV_GRAD_DIR_VER : LV_GRAD_DIR_HOR;
    d.bg_grad.stops_count = 2;
    d.bg_grad.stops[0] = { c, fromStart ? opa : (lv_opa_t)LV_OPA_TRANSP, 0 };
    d.bg_grad.stops[1] = { c, fromStart ? (lv_opa_t)LV_OPA_TRANSP : opa, 255 };
    d.border_width = 0;
    lv_draw_rect(layer, &d, &a);
}

// `round`: round ends (the cut's edge, meeting the frame); square for the soft
// line, whose round ends showed as dots where it met the edges.
void line(lv_layer_t *layer, int32_t x1, int32_t y1, int32_t x2, int32_t y2, int32_t w, lv_color_t c, lv_opa_t opa,
          bool round) {
    lv_draw_line_dsc_t d;
    lv_draw_line_dsc_init(&d);
    d.p1.x = x1; d.p1.y = y1; d.p2.x = x2; d.p2.y = y2;
    d.width = w;
    d.color = c;
    d.opa = opa;
    d.round_start = d.round_end = round ? 1 : 0;
    lv_draw_line(layer, &d);
}

void tri(lv_layer_t *layer, int32_t x1, int32_t y1, int32_t x2, int32_t y2, int32_t x3, int32_t y3, lv_color_t c) {
    lv_draw_triangle_dsc_t d;
    lv_draw_triangle_dsc_init(&d);
    d.p[0].x = x1; d.p[0].y = y1;
    d.p[1].x = x2; d.p[1].y = y2;
    d.p[2].x = x3; d.p[2].y = y3;
    d.color = c;
    d.opa = LV_OPA_COVER;
    lv_draw_triangle(layer, &d);
}

}  // namespace

void Card::activeFrame(CardFrame f, uint32_t color) {
    if (!_surface) return;
    if (!_frameHooked && f != CardFrame::FRAME_NONE) {
        lv_obj_add_event_cb(_surface, frameDrawCb, LV_EVENT_DRAW_MAIN_END, this);
        lv_obj_add_event_cb(_surface, frameDrawCb, LV_EVENT_DRAW_POST, this);
        _frameHooked = true;
    }
    if (f == _frame && color == _frameColor) return;
    _frame = f;
    _frameColor = color;
    lv_obj_invalidate(_surface);
}

void Card::frameDrawCb(lv_event_t *e) {
    Card *card = (Card *)lv_event_get_user_data(e);
    if (!card || card->_pressed || card->_frame == CardFrame::FRAME_NONE) return;
    lv_layer_t *layer = lv_event_get_layer(e);
    lv_area_t a;
    lv_obj_get_coords(card->_surface, &a);
    const UIMetrics &m = UI::met();
    const lv_color_t c = UI::c(card->_frameColor);
    const int32_t r = UI::sc(m.RADIUS);
    const int32_t depth = UI::sc(m.GLOW_DEPTH);
    const bool corner = card->_frame == CardFrame::FRAME_CORNER;
    const int32_t cut = lv_area_get_height(&a) * m.CUT_PCT / 100;

    if (lv_event_get_code(e) == LV_EVENT_DRAW_MAIN_END) {
        // THE LIGHT, behind the contents: from the bars (bracket), from every
        // edge (corner cut).
        band(layer, { a.x1, a.y1, a.x2, a.y1 + depth }, true, true, c, m.GLOW_OPA, r);
        band(layer, { a.x1, a.y2 - depth, a.x2, a.y2 }, true, false, c, m.GLOW_OPA, r);
        if (corner) {
            band(layer, { a.x1, a.y1, a.x1 + depth, a.y2 }, false, true, c, m.GLOW_OPA * 3 / 4, r);
            band(layer, { a.x2 - depth, a.y1, a.x2, a.y2 }, false, false, c, m.GLOW_OPA * 3 / 4, r);
        }
        return;
    }

    // THE EDGE, over the contents.
    lv_draw_rect_dsc_t d;
    lv_draw_rect_dsc_init(&d);
    d.bg_opa = LV_OPA_TRANSP;
    d.radius = r;
    d.border_color = c;
    d.border_opa = LV_OPA_COVER;
    if (!corner) {
        d.border_side  = (lv_border_side_t)(LV_BORDER_SIDE_TOP | LV_BORDER_SIDE_BOTTOM);
        d.border_width = UI::sc(m.BRACKET_W);
        lv_draw_rect(layer, &d, &a);
        d.border_side  = (lv_border_side_t)(LV_BORDER_SIDE_LEFT | LV_BORDER_SIDE_RIGHT);
        d.border_width = UI::sc(m.ACTIVE_EDGE);
        d.border_opa   = LV_OPA_70;
        lv_draw_rect(layer, &d, &a);
        return;
    }
    const int32_t w = UI::sc(m.ACTIVE_RING);
    d.border_side  = LV_BORDER_SIDE_FULL;
    d.border_width = w;
    lv_draw_rect(layer, &d, &a);
    // The cuts: the page's ground over the top-right and bottom-left corners
    // (the card's own corner, its frame, its light), then the cut's own edge -
    // and a softer, wider line inside it, the light along the cut.
    const lv_color_t ground = UI::c(UI::pal().GROUND);
    tri(layer, a.x2 - cut, a.y1 - 1, a.x2 + 1, a.y1 - 1, a.x2 + 1, a.y1 + cut, ground);
    tri(layer, a.x1 - 1, a.y2 - cut, a.x1 - 1, a.y2 + 1, a.x1 + cut, a.y2 + 1, ground);
    const int32_t in = depth / 3;   // the soft line, set in from the cut
    line(layer, a.x2 - cut - in, a.y1 + w, a.x2 - w, a.y1 + cut + in, depth / 2, c, m.GLOW_OPA / 2, false);
    line(layer, a.x1 + w, a.y2 - cut - in, a.x1 + cut + in, a.y2 - w, depth / 2, c, m.GLOW_OPA / 2, false);
    line(layer, a.x2 - cut, a.y1 + w / 2, a.x2 - w / 2, a.y1 + cut, w, c, LV_OPA_COVER, true);
    line(layer, a.x1 + w / 2, a.y2 - cut, a.x1 + cut, a.y2 - w / 2, w, c, LV_OPA_COVER, true);
}
