// Card - the active frames (2.8 rounds 4-6). Split from Card.cpp, which was
// already past the structure check's 1,000 lines; the class is one, its edge's
// "on" states are here. See Card::activeFrame() in include/Cards/Card.h.
#include "Cards/Card.h"
#include "UI/UITokens.h"
#include <math.h>

// ---------------------------------------------------------------------------
// THE ACTIVE FRAMES (owner, 2.8 rounds 4-6): "on" shown on the card's edge in
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
//
// ON A LIGHT SCHEME THEY GO DARKER (round 6): brighter than the card is what
// makes them glow on the dark schemes, and on Linen's cream it was "not nearly
// as standout obvious". The edge is ACTIVE_INK % of the way to the scheme's
// text, the light half that; 0 on the dark schemes, which are as they were.
// ---------------------------------------------------------------------------
namespace {

constexpr float INV_SQRT2 = 0.70710678f;

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

void tri(lv_layer_t *layer, const lv_point_t &p1, const lv_point_t &p2, const lv_point_t &p3, lv_color_t c) {
    lv_draw_triangle_dsc_t d;
    lv_draw_triangle_dsc_init(&d);
    d.p[0].x = p1.x; d.p[0].y = p1.y;
    d.p[1].x = p2.x; d.p[1].y = p2.y;
    d.p[2].x = p3.x; d.p[2].y = p3.y;
    d.color = c;
    d.opa = LV_OPA_COVER;
    lv_draw_triangle(layer, &d);
}

// One cut, from P to Q along the card's edge, away from the corner C between
// them; (vx, vy) the way out of the card, +-1 each. THE GROUND over what the
// cut takes away: the corner's triangle, and - by `reach`, the card's shadow's
// reach - the shadow its square corner cast, out to the lines square to the
// cut at its two ends. Past those lines the card's own shadow stays: it is
// the straight edges' shadow, and meets the cut's own (below) along them.
void cutGround(lv_layer_t *layer, lv_point_t P, lv_point_t Q, lv_point_t C, int32_t vx, int32_t vy, int32_t reach,
               lv_color_t ground) {
    const lv_point_t Po = { P.x + vx * reach, P.y + vy * reach };
    const lv_point_t Co = { C.x + vx * reach, C.y + vy * reach };
    const lv_point_t Qo = { Q.x + vx * reach, Q.y + vy * reach };
    tri(layer, P, Po, Co, ground);
    tri(layer, P, Co, Qo, ground);
    tri(layer, P, Qo, Q, ground);
    // Their shared edges, covered: each triangle is anti-aliased on its own,
    // and the card showed through between two as a pale line.
    line(layer, P.x, P.y, Co.x, Co.y, 2, ground, LV_OPA_COVER, false);
    line(layer, P.x, P.y, Qo.x, Qo.y, 2, ground, LV_OPA_COVER, false);
}

// THE SHADOW ALONG A CUT (round 6 - the corner cut on Linen, which a drop
// shadow kept off it until now). LVGL shadows a rounded rectangle only, so a
// cut gets its shadow drawn here: strips parallel to it, darkest at the card,
// in the card's own shadow's profile - its blur `sw` wide, centred `oy` below
// the edge, which is oy/sqrt2 out from a 45-degree cut at the bottom-left, and
// as far IN at the top-right, where only the blur's faint outer half shows.
void cutShadow(lv_layer_t *layer, lv_point_t P, lv_point_t Q, int32_t vx, int32_t vy, int32_t sw, int32_t oy,
               lv_opa_t so) {
    const float b = sw * 0.5f;
    const float c = vy * oy * INV_SQRT2;     // the blur's centre, out from the cut
    const float out = c + b;                 // where it has faded to nothing
    if (out < 1.f || b <= 0.f) return;
    const int steps = LV_CLAMP(1, (int)lroundf(out / 1.5f), 5);
    const float step = out / steps;
    const int32_t w = LV_MAX(1, (int32_t)ceilf(step));
    for (int i = 0; i < steps; i++) {
        const float s = step * (i + 0.5f);
        const float k = LV_MIN((c + b - s) / (2.f * b), 1.f);   // a straight edge's shadow at s
        const lv_opa_t opa = (lv_opa_t)LV_CLAMP(0, (int)lroundf(so * k), 255);
        const int32_t dx = lroundf(vx * s * INV_SQRT2), dy = lroundf(vy * s * INV_SQRT2);
        line(layer, P.x + dx, P.y + dy, Q.x + dx, Q.y + dy, w, lv_color_black(), opa, false);
    }
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
    const uint32_t text = UI::pal().TEXT;
    const lv_color_t edge  = UI::c(UI::mix(card->_frameColor, text, m.ACTIVE_INK));
    const lv_color_t light = UI::c(UI::mix(card->_frameColor, text, m.ACTIVE_INK / 2));
    const int32_t r = UI::sc(m.RADIUS);
    const int32_t depth = UI::sc(m.GLOW_DEPTH);
    const bool corner = card->_frame == CardFrame::FRAME_CORNER;
    const int32_t cut = lv_area_get_height(&a) * m.CUT_PCT / 100;
    // The cuts' corners - top-right and bottom-left - and the card's shadow.
    const lv_point_t trP = { a.x2 - cut, a.y1 }, trQ = { a.x2, a.y1 + cut }, trC = { a.x2, a.y1 };
    const lv_point_t blP = { a.x1, a.y2 - cut }, blQ = { a.x1 + cut, a.y2 }, blC = { a.x1, a.y2 };
    const lv_obj_t *s = card->_surface;
    const int32_t sw = lv_obj_get_style_shadow_width(s, LV_PART_MAIN);
    const int32_t oy = lv_obj_get_style_shadow_offset_y(s, LV_PART_MAIN);
    const lv_opa_t so = lv_obj_get_style_shadow_opa(s, LV_PART_MAIN);
    const bool shadow = sw > 0 && so > LV_OPA_MIN;
    const lv_color_t ground = UI::c(UI::pal().GROUND);

    if (lv_event_get_code(e) == LV_EVENT_DRAW_MAIN_END) {
        // THE LIGHT, behind the contents: from the bars (bracket), from every
        // edge (corner cut).
        band(layer, { a.x1, a.y1, a.x2, a.y1 + depth }, true, true, light, m.GLOW_OPA, r);
        band(layer, { a.x1, a.y2 - depth, a.x2, a.y2 }, true, false, light, m.GLOW_OPA, r);
        if (!corner) return;
        band(layer, { a.x1, a.y1, a.x1 + depth, a.y2 }, false, true, light, m.GLOW_OPA * 3 / 4, r);
        band(layer, { a.x2 - depth, a.y1, a.x2, a.y2 }, false, false, light, m.GLOW_OPA * 3 / 4, r);
        // THE CUTS, OUTSIDE THE CARD - here, because this is the last event
        // whose clip reaches past the card (LVGL 9.5 clips DRAW_POST to the
        // card's own area). The ground over the corners and the shadow they
        // cast, then the cuts' own shadow; DRAW_POST does the inside again,
        // over the contents.
        const int32_t reach = shadow ? sw / 2 + LV_ABS(oy) + 1 : 1;
        cutGround(layer, trP, trQ, trC, +1, -1, reach, ground);
        cutGround(layer, blP, blQ, blC, -1, +1, reach, ground);
        if (shadow) {
            cutShadow(layer, trP, trQ, +1, -1, sw, oy, so);
            cutShadow(layer, blP, blQ, -1, +1, sw, oy, so);
        }
        return;
    }

    // THE EDGE, over the contents.
    lv_draw_rect_dsc_t d;
    lv_draw_rect_dsc_init(&d);
    d.bg_opa = LV_OPA_TRANSP;
    d.radius = r;
    d.border_color = edge;
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
    // The cuts, inside the card: the page's ground over the two corners (the
    // card's own corner, its frame, its light, any contents there) and the
    // cuts' shadow again where it falls inside the card's box - then the cut's
    // own edge, and a softer, wider line inside it, the light along the cut.
    // One triangle each, on the cut line run on past both its ends, its point
    // past the corner: only the cut is inside the card - its other two edges,
    // anti-aliased, left a pixel of the frame showing beside the cut's ends.
    const int32_t o = 4;
    tri(layer, { trP.x - o, trP.y - o }, { trC.x + 2 * o, trC.y - 2 * o }, { trQ.x + o, trQ.y + o }, ground);
    tri(layer, { blP.x - o, blP.y - o }, { blC.x - 2 * o, blC.y + 2 * o }, { blQ.x + o, blQ.y + o }, ground);
    if (shadow) {
        cutShadow(layer, trP, trQ, +1, -1, sw, oy, so);
        cutShadow(layer, blP, blQ, -1, +1, sw, oy, so);
    }
    const int32_t in = depth / 3;   // the soft line, set in from the cut
    line(layer, a.x2 - cut - in, a.y1 + w, a.x2 - w, a.y1 + cut + in, depth / 2, light, m.GLOW_OPA / 2, false);
    line(layer, a.x1 + w, a.y2 - cut - in, a.x1 + cut + in, a.y2 - w, depth / 2, light, m.GLOW_OPA / 2, false);
    line(layer, a.x2 - cut, a.y1 + w / 2, a.x2 - w / 2, a.y1 + cut, w, edge, LV_OPA_COVER, true);
    line(layer, a.x1 + w / 2, a.y2 - cut, a.x1 + cut, a.y2 - w / 2, w, edge, LV_OPA_COVER, true);
}
