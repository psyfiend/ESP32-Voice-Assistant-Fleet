// THE HALO (D-35, 2.11 round 1): what floats over the page - a header slot's
// panel, every window - stands on a black rim at its edge fading to clear,
// UIShape HALO millimetres out ("Panel Edges", the owner's choice).
//
// PAINTED, NOT LVGL'S SHADOW. Built first as a shadow (spread half its width),
// it cost a full redraw with a window open 45 ms more on the P4_5 (129 -> 175 ms)
// and banded into visible steps: black to the dark ground is a handful of 16-bit
// levels (LESSONS: LVGL's gradients band on these panels). So it is four corner
// images and four edge strips, painted once per size and scheme into PSRAM:
// black, its alpha falling off with the distance from the object's rounded edge,
// ORDERED-DITHERED (4 x 4 Bayer) so the panel's 16 bits mix two neighbouring
// levels pixel by pixel instead of stepping. The strips are 16 px long and tiled
// along the edges - the dither's period divides it, so the tiles join unseen.
// Drawn under the object, in its DRAW_MAIN_BEGIN; nothing inside the object's
// edge is drawn twice (alpha 0 there).
#include "UI/Kit.h"
#include "src/misc/cache/instance/lv_image_cache.h"   // lv_image_cache_drop(): not in lvgl.h
#include <esp_heap_caps.h>
#include <math.h>
#include <string.h>

namespace Kit {

namespace {
constexpr int32_t STRIP = 16;   // an edge strip's length: a multiple of the dither's 4

const uint8_t BAYER[4][4] = { { 0, 8, 2, 10 }, { 12, 4, 14, 6 }, { 3, 11, 1, 9 }, { 15, 7, 13, 5 } };

struct Img {
    uint8_t       *buf = nullptr;
    size_t         cap = 0;
    lv_image_dsc_t dsc = {};
};

// One set per (width, radius, dither): the windows' and the panels' differ in radius.
struct HaloSet {
    int32_t  h = 0, r = 0;
    uint8_t  amp = 0;
    bool     ok = false;
    Img      corner[4];   // TL, TR, BL, BR
    Img      edge[4];     // top, bottom, left, right
};
HaloSet s_sets[3];
uint8_t s_next = 0;

// Black at the edge (d = 0), clear at h: eased, so it ends softly.
float profile(float d, float h) {
    if (d <= -1.f || d >= h) return 0.f;
    if (d <= 0.f) return 255.f;   // under the object's anti-aliased edge: no gap
    const float t = 1.f - d / h;
    return 255.f * t * t;
}

uint8_t dithered(float a, int32_t x, int32_t y, uint8_t amp) {
    if (a <= 0.5f) return 0;
    const float n = ((float)BAYER[y & 3][x & 3] - 7.5f) / 16.f * (float)amp;
    const float v = a + n;
    return (uint8_t)(v <= 0.f ? 0 : v >= 255.f ? 255 : lroundf(v));
}

// A8: an alpha byte a pixel, drawn in black (the draw's recolour) - LVGL blends
// it as a masked fill, a third of ARGB8888's time from PSRAM.
bool alloc(Img &im, int32_t w, int32_t h) {
    const size_t bytes = (size_t)w * h;
    if (im.cap < bytes) {
        if (im.buf) heap_caps_free(im.buf);
        im.buf = (uint8_t *)heap_caps_malloc(bytes, MALLOC_CAP_SPIRAM);
        im.cap = im.buf ? bytes : 0;
        if (!im.buf) return false;
    }
    lv_image_cache_drop(&im.dsc);
    im.dsc = {};
    im.dsc.header.magic  = LV_IMAGE_HEADER_MAGIC;
    im.dsc.header.cf     = LV_COLOR_FORMAT_A8;
    im.dsc.header.w      = (uint32_t)w;
    im.dsc.header.h      = (uint32_t)h;
    im.dsc.header.stride = (uint32_t)w;
    im.dsc.data_size     = (uint32_t)bytes;
    im.dsc.data          = im.buf;
    return true;
}

// Each pixel: black, at the alpha its distance out of the edge gives, dithered
// by its place in the image (a strip's length is a multiple of the pattern's).
void paint(Img &im, int32_t w, int32_t h, float (*dist)(int32_t x, int32_t y, const HaloSet &s),
           const HaloSet &s) {
    uint8_t *p = im.buf;
    for (int32_t y = 0; y < h; y++)
        for (int32_t x = 0; x < w; x++, p++)
            *p = dithered(profile(dist(x, y, s), (float)s.h), x, y, s.amp);
}

// The corners: the arc's centre is the image's corner nearest the object.
float dTL(int32_t x, int32_t y, const HaloSet &s) {
    const float c = (float)(s.h + s.r);
    return hypotf(c - (x + .5f), c - (y + .5f)) - (float)s.r;
}
float dTR(int32_t x, int32_t y, const HaloSet &s) {
    const float c = (float)(s.h + s.r);
    return hypotf((x + .5f), c - (y + .5f)) - (float)s.r;
}
float dBL(int32_t x, int32_t y, const HaloSet &s) {
    const float c = (float)(s.h + s.r);
    return hypotf(c - (x + .5f), (y + .5f)) - (float)s.r;
}
float dBR(int32_t x, int32_t y, const HaloSet &s) {
    (void)s;
    return hypotf((x + .5f), (y + .5f)) - (float)s.r;
}
float dTop(int32_t x, int32_t y, const HaloSet &s)    { (void)x; return (float)s.h - (y + .5f); }
float dBottom(int32_t x, int32_t y, const HaloSet &s) { (void)x; (void)s; return y + .5f; }
float dLeft(int32_t x, int32_t y, const HaloSet &s)   { (void)y; return (float)s.h - (x + .5f); }
float dRight(int32_t x, int32_t y, const HaloSet &s)  { (void)y; (void)s; return x + .5f; }

// The dither's strength: enough to straddle one 16-bit step of the darkest
// ground the halo falls on - more over a dark scheme, little over Linen.
uint8_t ditherAmp() {
    const uint32_t g = UI::pal().GROUND;
    const uint32_t m = LV_MAX(LV_MAX((g >> 16) & 0xFF, (g >> 8) & 0xFF), g & 0xFF);
    const uint32_t a = (8u * 255u) / (m + 1u);
    return (uint8_t)LV_MIN(a, 64u);
}

const HaloSet *haloSet(int32_t h, int32_t r) {
    const uint8_t amp = ditherAmp();
    for (const HaloSet &s : s_sets)
        if (s.ok && s.h == h && s.r == r && s.amp == amp) return &s;
    HaloSet &s = s_sets[s_next];
    s_next = (uint8_t)((s_next + 1) % (sizeof(s_sets) / sizeof(s_sets[0])));
    s.ok = false;
    s.h = h; s.r = r; s.amp = amp;
    const int32_t c = h + r;
    float (*const cd[4])(int32_t, int32_t, const HaloSet &) = { dTL, dTR, dBL, dBR };
    for (int i = 0; i < 4; i++) {
        if (!alloc(s.corner[i], c, c)) return nullptr;
        paint(s.corner[i], c, c, cd[i], s);
    }
    if (!alloc(s.edge[0], STRIP, h) || !alloc(s.edge[1], STRIP, h) ||
        !alloc(s.edge[2], h, STRIP) || !alloc(s.edge[3], h, STRIP)) return nullptr;
    paint(s.edge[0], STRIP, h, dTop, s);
    paint(s.edge[1], STRIP, h, dBottom, s);
    paint(s.edge[2], h, STRIP, dLeft, s);
    paint(s.edge[3], h, STRIP, dRight, s);
    s.ok = true;
    return &s;
}

// Within `fill` only: a tiled draw finishes its last tile past the area it was
// given (a darker band where an edge strip ran on over a corner), so the layer's
// clip is narrowed to it for the one call.
void put(lv_layer_t *layer, const Img &im, const lv_area_t &fill, int32_t ox, int32_t oy, bool tile) {
    const lv_area_t keep = layer->_clip_area;
    const lv_area_t c = { LV_MAX(keep.x1, fill.x1), LV_MAX(keep.y1, fill.y1),
                          LV_MIN(keep.x2, fill.x2), LV_MIN(keep.y2, fill.y2) };
    if (c.x1 > c.x2 || c.y1 > c.y2) return;
    layer->_clip_area = c;
    lv_draw_image_dsc_t d;
    lv_draw_image_dsc_init(&d);
    d.src  = &im.dsc;
    d.recolor     = lv_color_black();   // an A8 image's pixels are drawn in this
    d.recolor_opa = LV_OPA_COVER;
    d.tile = tile ? 1 : 0;
    d.image_area = { ox, oy, ox + (int32_t)im.dsc.header.w - 1, oy + (int32_t)im.dsc.header.h - 1 };
    lv_draw_image(layer, &d, &fill);
    layer->_clip_area = keep;
}

void drawMainBegin(lv_event_t *e) {
    lv_obj_t *o = lv_event_get_current_target_obj(e);
    lv_area_t a;
    lv_obj_get_coords(o, &a);
    haloDraw(lv_event_get_layer(e), a, lv_obj_get_style_radius(o, LV_PART_MAIN), nullptr);
}

void extDrawSize(lv_event_t *e) { lv_event_set_ext_draw_size(e, haloWidth()); }
}  // namespace

int32_t haloWidth() { return UI::mm(UI::shape().HALO); }

void haloDraw(lv_layer_t *layer, const lv_area_t &a, int32_t radius, const lv_area_t *clip) {
    const int32_t h = haloWidth();
    const int32_t w = lv_area_get_width(&a), ht = lv_area_get_height(&a);
    const int32_t r = LV_MAX(0, radius);
    // Not while it is smaller than its own corners (a panel's first frames
    // sliding open): a set per passing size would be painted every frame.
    if (h <= 0 || w < 2 * r || ht < 2 * r) return;
    const HaloSet *s = haloSet(h, r);
    if (!s) return;
    const lv_area_t keep = layer->_clip_area;
    if (clip) {
        const lv_area_t c = { LV_MAX(keep.x1, clip->x1), LV_MAX(keep.y1, clip->y1),
                              LV_MIN(keep.x2, clip->x2), LV_MIN(keep.y2, clip->y2) };
        if (c.x1 > c.x2 || c.y1 > c.y2) return;
        layer->_clip_area = c;
    }
    // The corners, each a whole image.
    const lv_area_t tl = { a.x1 - h, a.y1 - h, a.x1 + r - 1, a.y1 + r - 1 };
    const lv_area_t tr = { a.x2 - r + 1, a.y1 - h, a.x2 + h, a.y1 + r - 1 };
    const lv_area_t bl = { a.x1 - h, a.y2 - r + 1, a.x1 + r - 1, a.y2 + h };
    const lv_area_t br = { a.x2 - r + 1, a.y2 - r + 1, a.x2 + h, a.y2 + h };
    put(layer, s->corner[0], tl, tl.x1, tl.y1, false);
    put(layer, s->corner[1], tr, tr.x1, tr.y1, false);
    put(layer, s->corner[2], bl, bl.x1, bl.y1, false);
    put(layer, s->corner[3], br, br.x1, br.y1, false);
    // The edges, tiled from a multiple of the strip so the dither stays in phase.
    const lv_area_t top    = { a.x1 + r, a.y1 - h, a.x2 - r, a.y1 - 1 };
    const lv_area_t bottom = { a.x1 + r, a.y2 + 1, a.x2 - r, a.y2 + h };
    const lv_area_t left   = { a.x1 - h, a.y1 + r, a.x1 - 1, a.y2 - r };
    const lv_area_t right  = { a.x2 + 1, a.y1 + r, a.x2 + h, a.y2 - r };
    put(layer, s->edge[0], top,    top.x1, top.y1, true);
    put(layer, s->edge[1], bottom, bottom.x1, bottom.y1, true);
    put(layer, s->edge[2], left,   left.x1, left.y1, true);
    put(layer, s->edge[3], right,  right.x1, right.y1, true);
    layer->_clip_area = keep;
}

void haloAttach(lv_obj_t *o) {
    lv_obj_add_event_cb(o, drawMainBegin, LV_EVENT_DRAW_MAIN_BEGIN, nullptr);
    lv_obj_add_event_cb(o, extDrawSize, LV_EVENT_REFR_EXT_DRAW_SIZE, nullptr);
    lv_obj_refresh_ext_draw_size(o);
}

}  // namespace Kit
