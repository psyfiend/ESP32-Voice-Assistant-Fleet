// A stroke glyph (2.8 round 3, D-24, D-27): one of the status glyphs drawn from
// its strokes on the 24-unit grid (UI/GlyphShapes.h) with LVGL's own arcs,
// lines and dots, at any size. See Kit.h.
#include "UI/Kit.h"

#include <math.h>

namespace Kit {

namespace {
int32_t u(float v, int32_t px) { return (int32_t)lroundf(v * (float)px / Glyphs::GRID); }

lv_obj_t *line(lv_obj_t *box, lv_point_precise_t *pts, int32_t w) {
    lv_obj_t *l = lv_line_create(box);
    lv_line_set_points(l, pts, 2);
    lv_obj_set_pos(l, 0, 0);
    lv_obj_set_style_line_width(l, w, 0);
    lv_obj_set_style_line_rounded(l, true, 0);
    lv_obj_remove_flag(l, LV_OBJ_FLAG_CLICKABLE);
    return l;
}
}  // namespace

void strokeGlyph(StrokeGlyph &g, lv_obj_t *parent, Glyphs::Shape shape, int32_t px) {
    g = StrokeGlyph();
    g.box = plain(parent);
    lv_obj_set_size(g.box, px, px);
    lv_obj_remove_flag(g.box, LV_OBJ_FLAG_CLICKABLE);
    const int32_t w = LV_MAX(2, u(Glyphs::STROKE, px));
    const Glyphs::Stroke *st;
    const uint8_t n = Glyphs::strokes(shape, &st);
    for (uint8_t i = 0; i < n && i < Glyphs::STROKES_MAX; i++) {
        const Glyphs::Stroke &k = st[i];
        lv_obj_t *o = nullptr;
        if (k.kind == Glyphs::StrokeKind::LINE) {
            g.pts[i][0] = { (lv_value_precise_t)u(k.x, px), (lv_value_precise_t)u(k.y, px) };
            g.pts[i][1] = { (lv_value_precise_t)u(k.p, px), (lv_value_precise_t)u(k.q, px) };
            o = line(g.box, g.pts[i], w);
        } else if (k.kind == Glyphs::StrokeKind::DOT) {
            const int32_t d = LV_MAX(3, u(2.0f * k.p, px));
            o = plain(g.box);
            lv_obj_set_size(o, d, d);
            lv_obj_set_pos(o, u(k.x, px) - d / 2, u(k.y, px) - d / 2);
            lv_obj_set_style_radius(o, LV_RADIUS_CIRCLE, 0);
            lv_obj_set_style_bg_opa(o, LV_OPA_COVER, 0);
        } else {
            // An lv_arc draws its stroke inside its box, centred on the box's
            // centre: the box is the arc's diameter plus one stroke.
            const int32_t d = u(2.0f * k.p, px) + w;
            o = lv_arc_create(g.box);
            lv_obj_set_size(o, d, d);
            lv_obj_set_pos(o, u(k.x, px) - d / 2, u(k.y, px) - d / 2);
            lv_arc_set_bg_angles(o, (lv_value_precise_t)k.q, (lv_value_precise_t)k.a1);
            lv_obj_set_style_arc_width(o, w, LV_PART_MAIN);
            lv_obj_set_style_arc_rounded(o, true, LV_PART_MAIN);
            lv_obj_set_style_pad_all(o, 0, LV_PART_MAIN);
            // A control by default: no knob, no value, only the sweep.
            lv_obj_set_style_arc_opa(o, LV_OPA_TRANSP, LV_PART_INDICATOR);
            lv_obj_set_style_bg_opa(o, LV_OPA_TRANSP, LV_PART_KNOB);
            lv_obj_set_style_pad_all(o, 0, LV_PART_KNOB);
            lv_obj_remove_flag(o, LV_OBJ_FLAG_CLICKABLE);
        }
        g.obj[g.n] = o;
        g.part[g.n++] = k.part;
    }
    // The strike: corner to corner, on a halo of what it stands on, so it reads
    // across the strokes it crosses ("Fleet Status Glyphs": chosen, not broken).
    g.strikePts[0] = { (lv_value_precise_t)u(4.5f, px),  (lv_value_precise_t)u(19.5f, px) };
    g.strikePts[1] = { (lv_value_precise_t)u(19.5f, px), (lv_value_precise_t)u(4.5f, px) };
    g.halo   = line(g.box, g.strikePts, w * 2);
    g.strike = line(g.box, g.strikePts, w);
    lv_obj_add_flag(g.halo, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(g.strike, LV_OBJ_FLAG_HIDDEN);
}

void strokeGlyphPaint(StrokeGlyph &g, uint8_t part, lv_color_t c, lv_opa_t opa) {
    for (uint8_t i = 0; i < g.n; i++) {
        if (part != ALL_PARTS && g.part[i] != part) continue;
        lv_obj_t *o = g.obj[i];
        if (lv_obj_check_type(o, &lv_arc_class)) {
            lv_obj_set_style_arc_color(o, c, LV_PART_MAIN);
            lv_obj_set_style_arc_opa(o, opa, LV_PART_MAIN);
        } else if (lv_obj_check_type(o, &lv_line_class)) {
            lv_obj_set_style_line_color(o, c, 0);
            lv_obj_set_style_line_opa(o, opa, 0);
        } else {
            lv_obj_set_style_bg_color(o, c, 0);
            lv_obj_set_style_bg_opa(o, opa, 0);
        }
    }
}

void strokeGlyphShow(StrokeGlyph &g, uint8_t part, bool shown) {
    for (uint8_t i = 0; i < g.n; i++) {
        if (part != ALL_PARTS && g.part[i] != part) continue;
        if (shown) lv_obj_remove_flag(g.obj[i], LV_OBJ_FLAG_HIDDEN);
        else       lv_obj_add_flag(g.obj[i], LV_OBJ_FLAG_HIDDEN);
    }
}

void strokeGlyphStrike(StrokeGlyph &g, bool on, lv_color_t c, lv_color_t halo) {
    if (!g.strike) return;
    if (!on) {
        lv_obj_add_flag(g.halo, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(g.strike, LV_OBJ_FLAG_HIDDEN);
        return;
    }
    lv_obj_set_style_line_color(g.halo, halo, 0);
    lv_obj_set_style_line_color(g.strike, c, 0);
    lv_obj_remove_flag(g.halo, LV_OBJ_FLAG_HIDDEN);
    lv_obj_remove_flag(g.strike, LV_OBJ_FLAG_HIDDEN);
}

}  // namespace Kit
