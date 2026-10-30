// Card - the active frames (2.8 rounds 4-7). Split from Card.cpp, which was
// already past the structure check's 1,000 lines; the class is one, its edge's
// "on" states are here. See Card::activeFrame() in include/Cards/Card.h.
#include "Cards/Card.h"
#include "UI/UITokens.h"
#include "src/misc/cache/instance/lv_image_cache.h"   // lv_image_cache_drop(): not in lvgl.h
#include <esp_heap_caps.h>
#include <math.h>
#include <string.h>

// ---------------------------------------------------------------------------
// THE ACTIVE FRAMES (owner, 2.8 rounds 4-7): "on" shown on the card's edge in
// place of a fill, and LIT - "a little lit up colour inside the border",
// "something to make the cards pop"; the futuristic HMI frames and Cyberpunk
// 2077's cursor brackets as the reference, not full cyber-UI.
//   BRACKET  heavy top and bottom bars, thin sides, light falling inward from
//            the bars
//   CORNER   a frame with the top-right and bottom-left corners cut away at
//            45 degrees (clear of the corner icon, top-left), light inward
//            from every edge - the DEFAULT since round 6
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

// Only when a cut corner's image cannot be had (CUT_SLOTS, below): the page's
// ground over the corner, on the cut line run on past both its ends.
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

// ---------------------------------------------------------------------------
// THE CUT CORNERS, PAINTED (round 7). LVGL shadows a rounded rectangle only,
// so where the corner cut takes a corner away, the card's shadow has to be
// made again around the cut - and round 6's pieces (triangles of the page's
// ground, strips of shadow along the cut) met the card's own shadow at two
// visible points, "they don't blend well with the change in angle" (owner),
// and cost ~13 ms a lit card on a page change, all of it the CPU drawing
// triangles and diagonal lines over their whole bounding boxes.
//
// So each cut corner is ONE IMAGE, painted once and kept: the page's ground
// where the card is not, darkened by the shadow the cut card would cast, and
// clear where the card is. Its shadow is LVGL'S OWN, MEASURED - a rectangle
// with the card's shadow drawn offscreen, its alpha read out from the middle
// of each side - and goes by the distance to the card's outline moved down by
// the shadow's offset, so it rounds the two new corners as a blur would, and
// where the image ends, along the card's straight edges, it is exactly LVGL's
// value: no seam. Drawn in two pieces (drawCuts()): under the card's contents
// what lies past the card (DRAW_MAIN_END), over them the cut's own square
// (DRAW_POST). Measured on the P4_5, Linen, a page change: the whole corner
// cut ~8 ms a lit card (round 6: 13.4) - the light bands 4, the cuts 2, their
// two lines about 2.
// ---------------------------------------------------------------------------

// LVGL's shadow at each distance `d` out from the edge of a rectangle, from
// the middle of each side: [d + CAL_HALF]. Inside the rectangle it is masked
// out (0), and never read.
constexpr int CAL_HALF = 24, CAL_N = 2 * CAL_HALF;
struct ShadowCal {
    bool ok = false;
    int32_t sw = -1, oy = 0, r = 0;
    lv_opa_t so = 0;
    uint8_t left[CAL_N] = {}, right[CAL_N] = {}, top[CAL_N] = {}, bottom[CAL_N] = {};
};
ShadowCal s_cal;

// Measured once per shadow (a scheme's, at this board's scale) - outside a
// redraw, where LVGL can draw offscreen (Card::activeFrame()).
bool calibrate(int32_t sw, int32_t oy, lv_opa_t so, int32_t r) {
    if (s_cal.ok && s_cal.sw == sw && s_cal.oy == oy && s_cal.so == so && s_cal.r == r) return true;
    s_cal.ok = false;
    const int32_t m  = LV_MIN(sw / 2 + LV_ABS(oy) + 4, CAL_HALF - 1);
    const int32_t rw = 2 * (sw + r) + 16;   // LVGL's corners are sw + r: a straight middle between them
    const int32_t W  = rw + 2 * m;
    const uint32_t stride = lv_draw_buf_width_to_stride(W, LV_COLOR_FORMAT_ARGB8888);
    const uint32_t bytes  = stride * W;
    uint8_t *mem = (uint8_t *)heap_caps_aligned_alloc(LV_DRAW_BUF_ALIGN, bytes, MALLOC_CAP_SPIRAM);
    if (!mem) return false;
    memset(mem, 0, bytes);
    lv_draw_buf_t db;
    if (lv_draw_buf_init(&db, W, W, LV_COLOR_FORMAT_ARGB8888, stride, mem, bytes) != LV_RESULT_OK) {
        heap_caps_free(mem);
        return false;
    }
    lv_obj_t *cv = lv_canvas_create(lv_layer_top());
    lv_obj_add_flag(cv, LV_OBJ_FLAG_HIDDEN);
    lv_canvas_set_draw_buf(cv, &db);
    lv_layer_t layer;
    lv_canvas_init_layer(cv, &layer);
    lv_draw_rect_dsc_t d;
    lv_draw_rect_dsc_init(&d);
    d.bg_opa = LV_OPA_TRANSP;
    d.border_width = 0;
    d.radius = r;
    d.shadow_width = sw;
    d.shadow_offset_y = oy;
    d.shadow_opa = so;
    d.shadow_color = lv_color_black();
    const lv_area_t a = { m, m, m + rw - 1, m + rw - 1 };
    lv_draw_rect(&layer, &d, &a);
    lv_canvas_finish_layer(cv, &layer);
    lv_obj_delete(cv);
    auto alpha = [&](int32_t x, int32_t y) -> uint8_t {
        return (x < 0 || y < 0 || x >= W || y >= W) ? 0 : mem[y * stride + x * 4 + 3];   // B, G, R, A
    };
    const int32_t mid = m + rw / 2;
    for (int i = 0; i < CAL_N; i++) {
        const int32_t o = i - CAL_HALF;
        s_cal.right[i]  = alpha(a.x2 + o, mid);
        s_cal.left[i]   = alpha(a.x1 - o, mid);
        s_cal.bottom[i] = alpha(mid, a.y2 + o);
        s_cal.top[i]    = alpha(mid, a.y1 - o);
    }
    heap_caps_free(mem);
    s_cal.sw = sw; s_cal.oy = oy; s_cal.so = so; s_cal.r = r;
    s_cal.ok = true;
    return true;
}

uint8_t tab(const uint8_t *t, int32_t d) {
    const int32_t i = d + CAL_HALF;
    return i < 0 ? t[0] : i >= CAL_N ? 0 : t[i];
}

// At a distance `sd` from the moved outline (negative inside it): the bottom
// side's profile, which is measured from the card's own edge, `oy` above the
// moved one - interpolated, a diagonal's distances being no whole pixels.
float shade(float sd) {
    float f = sd + s_cal.oy + CAL_HALF;
    if (f < CAL_HALF + 1) f = CAL_HALF + 1;   // the card's own edge and in: masked when measured
    if (f >= CAL_N - 1) return 0.f;
    const int i = (int)f;
    const float t = f - i;
    return s_cal.bottom[i] * (1.f - t) + s_cal.bottom[i + 1] * t;
}

enum : uint8_t { CUT_TR = 0, CUT_BL = 1 };
struct CutImage {
    uint8_t  corner = 0;
    int32_t  cut = 0, reach = 0;
    uint32_t ground = 0;
    int32_t  sw = 0, oy = 0, r = 0;   // the shadow it was painted with (0: none)
    lv_opa_t so = 0;
    uint32_t used = 0;                // the redraw that last drew it
    uint8_t *buf = nullptr;
    size_t   cap = 0;
    lv_image_dsc_t dsc = {};
};
// Two per card size showing - a page holds two or three sizes at most. A slot
// is painted over only if no redraw in progress has drawn it: an image is drawn
// after its draw call returns, from its buffer.
constexpr uint8_t CUT_SLOTS = 8;
CutImage s_cuts[CUT_SLOTS];
uint32_t s_redraw = 1;   // one per redraw (the display's REFR_START)

// THE ZONE: the cut's corner, `reach` past the card on its two outer sides,
// and on along its two straight edges past the cut's ends, to where the
// card's shadow is LVGL's own straight-edge shadow again.
void paintCut(CutImage &c, bool shadow) {
    const int32_t cut = c.cut, R = c.reach, n = cut + 2 * R;
    const bool bl = c.corner == CUT_BL;
    // The card's two edges in the image: BL its left column and bottom row,
    // TR its right column and top row; the cut from P to Q.
    const int32_t ex = bl ? R : n - 1 - R;            // the card's side
    const int32_t ey = bl ? n - 1 - R : R;            // its bottom (BL) or top (TR)
    const int32_t k  = bl ? (ey - cut - ex) : (ex - cut - ey);
    const int32_t oy = shadow ? c.oy : 0;
    // The outline moved down by the shadow's offset: its two corners on the cut.
    const float Px = bl ? ex : ex - cut, Py = bl ? ey - cut + oy : ey + oy;
    const float Qx = bl ? ex + cut : ex,  Qy = bl ? ey + oy : ey + cut + oy;
    const float L  = cut * 1.41421356f;
    const uint8_t gr = (c.ground >> 16) & 0xFF, gg = (c.ground >> 8) & 0xFF, gb = c.ground & 0xFF;
    uint8_t *px = c.buf;
    for (int32_t y = 0; y < n; y++) {
        for (int32_t x = 0; x < n; x++, px += 4) {
            // How much of this pixel is off the card: wholly past its two
            // straight edges, anti-aliased across the cut.
            float cov;
            if (bl) cov = (x < ex || y > ey) ? 1.f : LV_CLAMP(0.f, ((y - x) - k) * INV_SQRT2 + 0.5f, 1.f);
            else    cov = (x > ex || y < ey) ? 1.f : LV_CLAMP(0.f, ((x - y) - k) * INV_SQRT2 + 0.5f, 1.f);
            if (cov <= 0.f) { px[0] = px[1] = px[2] = px[3] = 0; continue; }
            float a = 0.f;
            if (shadow) {
                // Signed distances to the moved outline's three edges (out > 0).
                const float ds = bl ? (ex - x) : (x - ex);                          // the side
                const float de = bl ? (y - (ey + oy)) : ((ey + oy) - y);            // the bottom / top
                const float dd = (bl ? ((y - x) - (k + oy)) : ((x - y) - (k - oy))) * INV_SQRT2;
                if (ds <= 0.f && de <= 0.f && dd <= 0.f) {
                    // Inside the moved outline: its nearest edge's shade.
                    if (de >= ds && de >= dd && (bl ? y > ey : y < ey))
                        a = tab(bl ? s_cal.bottom : s_cal.top, bl ? y - ey : ey - y);
                    else a = shade(LV_MAX(ds, LV_MAX(de, dd)));
                } else {
                    // Outside: the nearest of its three edges, each a segment -
                    // past its end, the distance is to the corner (the blur's
                    // rounding of it).
                    const bool sideIn = bl ? (y <= Py) : (y >= Qy);
                    const float dS = sideIn ? fabsf(ds) : (bl ? hypotf(x - Px, y - Py) : hypotf(x - Qx, y - Qy));
                    const bool edgeIn = bl ? (x >= Qx) : (x <= Px);
                    const float dE = edgeIn ? fabsf(de) : (bl ? hypotf(x - Qx, y - Qy) : hypotf(x - Px, y - Py));
                    const float t = ((x - Px) + (y - Py)) * INV_SQRT2;   // along the cut, from P
                    const float dD = t < 0.f ? hypotf(x - Px, y - Py) : t > L ? hypotf(x - Qx, y - Qy) : fabsf(dd);
                    if (dS <= dE && dS <= dD)
                        a = (sideIn && ds > 0.f) ? tab(bl ? s_cal.left : s_cal.right, (int32_t)ds) : shade(dS);
                    else if (dE <= dD)
                        a = (edgeIn && (bl ? y > ey : y < ey)) ? tab(bl ? s_cal.bottom : s_cal.top, bl ? y - ey : ey - y)
                                                              : shade(dE);
                    else a = shade(dD);
                }
            }
            // Black over the ground as LVGL blends it into the panel's 16 bits
            // (lv_color_16_16_mix: the mix in 32 steps, each channel rounded
            // down) - in 8 bits this came out a step lighter than the card's
            // own shadow where the image meets it, a line across it on glass.
            const int32_t o = lroundf(a);
            int32_t r5 = gr >> 3, g6 = gg >> 2, b5 = gb >> 3;
            if (o >= LV_OPA_MAX) r5 = g6 = b5 = 0;
            else if (o > LV_OPA_MIN) {
                const int32_t keep = 32 - ((o + 4) >> 3);
                r5 = (r5 * keep) >> 5; g6 = (g6 * keep) >> 5; b5 = (b5 * keep) >> 5;
            }
            px[0] = (uint8_t)((b5 << 3) | (b5 >> 2));
            px[1] = (uint8_t)((g6 << 2) | (g6 >> 4));
            px[2] = (uint8_t)((r5 << 3) | (r5 >> 2));
            px[3] = (uint8_t)lroundf(cov * 255.f);
        }
    }
}

const CutImage *cutImage(uint8_t corner, int32_t cut, int32_t reach, uint32_t ground, bool shadow) {
    const int32_t sw = shadow ? s_cal.sw : 0, oy = shadow ? s_cal.oy : 0, r = shadow ? s_cal.r : 0;
    const lv_opa_t so = shadow ? s_cal.so : 0;
    for (CutImage &c : s_cuts) {
        if (c.buf && c.corner == corner && c.cut == cut && c.reach == reach && c.ground == ground && c.sw == sw &&
            c.oy == oy && c.r == r && c.so == so) {
            c.used = s_redraw;
            return &c;
        }
    }
    CutImage *spare = nullptr;
    for (CutImage &c : s_cuts)
        if (c.used != s_redraw && (!spare || c.used < spare->used)) spare = &c;
    if (!spare) return nullptr;
    const int32_t n = cut + 2 * reach;
    const size_t bytes = (size_t)n * n * 4;
    if (spare->cap < bytes) {
        if (spare->buf) heap_caps_free(spare->buf);
        spare->buf = (uint8_t *)heap_caps_malloc(bytes, MALLOC_CAP_SPIRAM);
        spare->cap = spare->buf ? bytes : 0;
        if (!spare->buf) return nullptr;
    }
    lv_image_cache_drop(&spare->dsc);
    spare->corner = corner; spare->cut = cut; spare->reach = reach; spare->ground = ground;
    spare->sw = sw; spare->oy = oy; spare->r = r; spare->so = so;
    paintCut(*spare, shadow);
    lv_image_dsc_t &d = spare->dsc;
    d = {};
    d.header.magic  = LV_IMAGE_HEADER_MAGIC;
    d.header.cf     = LV_COLOR_FORMAT_ARGB8888;
    d.header.w      = (uint32_t)n;
    d.header.h      = (uint32_t)n;
    d.header.stride = (uint32_t)n * 4;
    d.data_size     = (uint32_t)bytes;
    d.data          = spare->buf;
    spare->used = s_redraw;
    return spare;
}

// The image at (x, y), only within `clip` - the draw takes the layer's clip as
// it stands when it is added (lv_draw_add_task()), so it is narrowed for the
// one call. Every pixel of an image is blended, the clear ones too: drawn whole
// both times, a cut cost twice what these two pieces do.
void drawImage(lv_layer_t *layer, const CutImage &c, int32_t x, int32_t y, const lv_area_t &clip) {
    const lv_area_t keep = layer->_clip_area;
    const lv_area_t narrow = { LV_MAX(keep.x1, clip.x1), LV_MAX(keep.y1, clip.y1), LV_MIN(keep.x2, clip.x2),
                               LV_MIN(keep.y2, clip.y2) };   // lv_area_intersect() is private in 9.5
    if (narrow.x1 > narrow.x2 || narrow.y1 > narrow.y2) return;
    layer->_clip_area = narrow;
    lv_draw_image_dsc_t d;
    lv_draw_image_dsc_init(&d);
    d.src = &c.dsc;
    const lv_area_t at = { x, y, x + (int32_t)c.dsc.header.w - 1, y + (int32_t)c.dsc.header.h - 1 };
    lv_draw_image(layer, &d, &at);
    layer->_clip_area = keep;
}

// Both cut corners of the card at `a`. UNDER the contents (`over` false) only
// what is off the card - its shadow, past its edges (nothing on a scheme
// without one); OVER them only the cut's own square, inside the card, in
// `ground` - what lies behind it: the page's ground, a cluster's surface.
void drawCuts(lv_layer_t *layer, const lv_area_t &a, int32_t cut, bool shadow, bool over, uint32_t ground) {
    if (!over && !shadow) return;
    const int32_t reach = shadow ? s_cal.sw / 2 + LV_ABS(s_cal.oy) + 2 : 2;
    const int32_t n = cut + 2 * reach, far = n;
    if (const CutImage *c = cutImage(CUT_TR, cut, reach, ground, shadow)) {
        const int32_t x = a.x2 - (n - 1 - reach), y = a.y1 - reach;
        if (over) drawImage(layer, *c, x, y, { a.x2 - cut, a.y1, a.x2, a.y1 + cut });
        else {
            drawImage(layer, *c, x, y, { a.x1, a.y1 - far, a.x2 + far, a.y1 - 1 });   // above the card
            drawImage(layer, *c, x, y, { a.x2 + 1, a.y1, a.x2 + far, a.y2 });         // right of it
        }
    } else if (over) {
        tri(layer, { a.x2 - cut - 4, a.y1 - 4 }, { a.x2 + 8, a.y1 - 8 }, { a.x2 + 4, a.y1 + cut + 4 }, UI::c(ground));
    }
    if (const CutImage *c = cutImage(CUT_BL, cut, reach, ground, shadow)) {
        const int32_t x = a.x1 - reach, y = a.y2 - (n - 1 - reach);
        if (over) drawImage(layer, *c, x, y, { a.x1, a.y2 - cut, a.x1 + cut, a.y2 });
        else {
            drawImage(layer, *c, x, y, { a.x1 - far, a.y1, a.x1 - 1, a.y2 + far });   // left of the card
            drawImage(layer, *c, x, y, { a.x1, a.y2 + 1, a.x2, a.y2 + far });         // below it
        }
    } else if (over) {
        tri(layer, { a.x1 - 4, a.y2 - cut - 4 }, { a.x1 - 8, a.y2 + 8 }, { a.x1 + cut + 4, a.y2 + 4 }, UI::c(ground));
    }
}

// The card's own shadow, and whether it is the one measured.
bool cardShadow(const lv_obj_t *s) {
    const int32_t sw = lv_obj_get_style_shadow_width(s, LV_PART_MAIN);
    const lv_opa_t so = lv_obj_get_style_shadow_opa(s, LV_PART_MAIN);
    return sw > 0 && so > LV_OPA_MIN && s_cal.ok && s_cal.sw == sw && s_cal.so == so &&
           s_cal.oy == lv_obj_get_style_shadow_offset_y(s, LV_PART_MAIN) &&
           s_cal.r == lv_obj_get_style_radius(s, LV_PART_MAIN);
}

}  // namespace

void Card::activeFrame(CardFrame f, uint32_t color) {
    if (!_surface) return;
    if (!_frameHooked && f != CardFrame::FRAME_NONE) {
        lv_obj_add_event_cb(_surface, frameDrawCb, LV_EVENT_DRAW_MAIN_END, this);
        lv_obj_add_event_cb(_surface, frameDrawCb, LV_EVENT_DRAW_POST, this);
        _frameHooked = true;
        static bool counted = false;   // the redraws, for the cut images' slots
        if (!counted && lv_display_get_default()) {
            lv_display_add_event_cb(lv_display_get_default(), [](lv_event_t *) { s_redraw++; }, LV_EVENT_REFR_START,
                                    nullptr);
            counted = true;
        }
    }
    // The card's shadow measured here, outside a redraw (calibrate()).
    if (f == CardFrame::FRAME_CORNER) {
        const int32_t sw = lv_obj_get_style_shadow_width(_surface, LV_PART_MAIN);
        const lv_opa_t so = lv_obj_get_style_shadow_opa(_surface, LV_PART_MAIN);
        if (sw > 0 && so > LV_OPA_MIN)
            calibrate(sw, lv_obj_get_style_shadow_offset_y(_surface, LV_PART_MAIN), so,
                      lv_obj_get_style_radius(_surface, LV_PART_MAIN));
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
    const bool shadow = corner && cardShadow(card->_surface);
    // A mini tile is cut to its cluster's surface (2.11 round 11, K116).
    const uint32_t ground = card->mini() ? UI::pal().SURFACE : UI::pal().GROUND;

    if (lv_event_get_code(e) == LV_EVENT_DRAW_MAIN_END) {
        // THE LIGHT, behind the contents: from the bars (bracket), from every
        // edge (corner cut).
        band(layer, { a.x1, a.y1, a.x2, a.y1 + depth }, true, true, light, m.GLOW_OPA, r);
        band(layer, { a.x1, a.y2 - depth, a.x2, a.y2 }, true, false, light, m.GLOW_OPA, r);
        if (!corner) return;
        band(layer, { a.x1, a.y1, a.x1 + depth, a.y2 }, false, true, light, m.GLOW_OPA * 3 / 4, r);
        band(layer, { a.x2 - depth, a.y1, a.x2, a.y2 }, false, false, light, m.GLOW_OPA * 3 / 4, r);
        // THE CUTS' SHADOW, past the card - here, because this is the last
        // event whose clip reaches past it (LVGL 9.5 clips DRAW_POST to it).
        drawCuts(layer, a, cut, shadow, false, ground);
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
    // The cuts, over the contents and the frame, then the cut's own edge, and
    // a softer, wider line inside it, the light along the cut.
    drawCuts(layer, a, cut, shadow, true, ground);
    const int32_t in = depth / 3;   // the soft line, set in from the cut
    line(layer, a.x2 - cut - in, a.y1 + w, a.x2 - w, a.y1 + cut + in, depth / 2, light, m.GLOW_OPA / 2, false);
    line(layer, a.x1 + w, a.y2 - cut - in, a.x1 + cut + in, a.y2 - w, depth / 2, light, m.GLOW_OPA / 2, false);
    line(layer, a.x2 - cut, a.y1 + w / 2, a.x2 - w / 2, a.y1 + cut, w, edge, LV_OPA_COVER, true);
    line(layer, a.x1 + w / 2, a.y2 - cut, a.x1 + cut, a.y2 - w / 2, w, edge, LV_OPA_COVER, true);
}
