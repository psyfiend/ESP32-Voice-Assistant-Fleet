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
//
// ITS STRENGTH (2.11 round 4, the owner: "the effect itself is outstanding",
// but at full strength it stops reading as a shadow - "around 50%", Linen
// lighter still): painted into the images (strengthOf()), each scheme's own
// (UIMetrics::HALO_PCT: 50 dark, 30 Linen) or the owner's from UI Tweaks'
// slider, kept as schemes.<name>.halo.
//
// WHERE A PANEL MEETS THE BAR it is one shape - the open slot's tab, the two
// coves and the pane's top - and its halo there is painted for that shape as a
// whole (haloOverBar()): two halos, the pane's and the tab's, darkened twice
// where they overlapped, and the coves' insides were painted in the bar's
// colour over them (round 4, the owner's close-ups of Linen).
#include "UI/Kit.h"
#include "Settings.h"
#include "src/misc/cache/instance/lv_image_cache.h"   // lv_image_cache_drop(): not in lvgl.h
#include <ctype.h>
#include <esp_heap_caps.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
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
    uint8_t  str = 0;   // the strength painted in (%)
    bool     ok = false;
    Img      corner[4];   // TL, TR, BL, BR
    Img      edge[4];     // top, bottom, left, right
};
HaloSet s_sets[3];
uint8_t s_next = 0;

// --- Its strength, per scheme ---------------------------------------------------
constexpr uint8_t STRENGTH_SCHEMES = 4;   // UITokens' schemes, with room for one more
int16_t s_strength[STRENGTH_SCHEMES] = { -1, -1, -1, -1 };   // -1: the scheme's own
uint8_t s_strengthLoaded = 0;                                 // bit per scheme: read from the file

void schemeKey(char *out, size_t cap) {
    const char *n = UI::pal().name ? UI::pal().name : "scheme";
    size_t k = 0;
    for (; n[k] && k + 1 < cap; k++) out[k] = (char)tolower((unsigned char)n[k]);
    out[k] = '\0';
}

int16_t &strengthSlot() {
    uint8_t i = UI::schemeIndex();
    if (i >= STRENGTH_SCHEMES) i = 0;
    if (!(s_strengthLoaded & (1u << i))) {
        s_strengthLoaded |= (uint8_t)(1u << i);
        char key[16], v[8];
        schemeKey(key, sizeof(key));
        if (Settings::scheme(key, "halo", v, sizeof(v))) s_strength[i] = (int16_t)LV_CLAMP(0, atoi(v), 100);
    }
    return s_strength[i];
}

// PAINTED IN, not drawn at an opacity: the dither must straddle a 16-bit step
// of the alpha as drawn, and an opacity scaled the dither down with it - at 30%
// the steps came back (round 4, Linen). A change of strength repaints the sets.
float strengthOf(float a, uint8_t str) { return a * (float)str / 100.f; }

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
            *p = dithered(strengthOf(profile(dist(x, y, s), (float)s.h), s.str), x, y, s.amp);
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
    const uint8_t amp = ditherAmp(), str = haloStrength();
    for (const HaloSet &s : s_sets)
        if (s.ok && s.h == h && s.r == r && s.amp == amp && s.str == str) return &s;
    HaloSet &s = s_sets[s_next];
    s_next = (uint8_t)((s_next + 1) % (sizeof(s_sets) / sizeof(s_sets[0])));
    s.ok = false;
    s.h = h; s.r = r; s.amp = amp; s.str = str;
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
    if (!haloStrength()) return;
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

// --- Its strength ---------------------------------------------------------------
uint8_t haloStrength() {
    const int16_t s = strengthSlot();
    return s >= 0 ? (uint8_t)s : UI::met().HALO_PCT;
}

void keepHaloStrength(uint8_t pct) {
    if (pct > 100) pct = 100;
    int16_t &s = strengthSlot();
    s = pct == UI::met().HALO_PCT ? -1 : (int16_t)pct;   // the scheme's own is stored as nothing
    char key[16], v[8];
    schemeKey(key, sizeof(key));
    snprintf(v, sizeof(v), "%u", (unsigned)pct);
    Settings::setScheme(key, "halo", s < 0 ? nullptr : v);
    // Everything floating shows it at once: the screen and the top layer.
    lv_obj_invalidate(lv_screen_active());
    lv_obj_invalidate(lv_layer_top());
}

void forgetHaloStrengths() {
    for (int16_t &s : s_strength) s = -1;
    s_strengthLoaded = 0;
}

// --- One shape's halo, over the bar ------------------------------------------------
namespace {
// The shape's mask for the bar's share, painted when the shape changes (a panel
// opening, another slot) and kept; drawn as one image. Two: the system panel's
// and a slot's may both be on screen while one closes.
struct BarMask {
    HaloShape key = {};
    lv_area_t at  = {};
    int32_t   h   = 0;
    uint8_t   amp = 0, str = 0;
    bool      ok  = false;
    Img       im;
};
BarMask s_barMask[2];
uint8_t s_barNext = 0;

// How far (px centre) is outside a rounded rectangle: <= 0 inside.
float rrDist(float px, float py, const lv_area_t &a, float r) {
    const float cx = LV_CLAMP((float)a.x1 + r, px, (float)a.x2 + 1.f - r);
    const float cy = LV_CLAMP((float)a.y1 + r, py, (float)a.y2 + 1.f - r);
    const float dx = px - cx, dy = py - cy;
    const float d  = hypotf(dx, dy);
    if (d > 0.f) return d - r;
    // In the straight part: how far in, as a negative.
    return -LV_MIN(LV_MIN(px - (float)a.x1, (float)a.x2 + 1.f - px), LV_MIN(py - (float)a.y1, (float)a.y2 + 1.f - py));
}

// The distance from the shape - the pane, the tab, the coves' fill - or <= 0 in it.
float shapeDist(float px, float py, const HaloShape &s) {
    float d = rrDist(px, py, s.pane, (float)s.paneR);
    if (s.hasTab) d = LV_MIN(d, rrDist(px, py, s.tab, (float)s.tabR));
    for (uint8_t k = 0; k < s.nCove; k++) {
        const lv_area_t &q = s.coveSq[k];
        if (px < q.x1 || px > q.x2 + 1 || py < q.y1 || py > q.y2 + 1) continue;
        const float c = hypotf(px - (float)s.coveC[k].x, py - (float)s.coveC[k].y);
        if (c >= (float)s.coveR) return 0.f;               // the cove's fill: in the shape
        d = LV_MIN(d, (float)s.coveR - c);                 // its inside: the bar, beside the shape
    }
    return d;
}
}  // namespace

void haloOverBar(lv_layer_t *layer, const HaloShape &s, const lv_area_t &bar) {
    const int32_t h = haloWidth();
    if (h <= 0 || lv_area_get_width(&s.pane) < 2 * s.paneR) return;
    const uint8_t amp = ditherAmp(), str = haloStrength();
    if (!str) return;
    const int32_t scrW = lv_display_get_horizontal_resolution(nullptr);
    lv_area_t at = { LV_MAX(0, LV_MIN(s.pane.x1, s.hasTab ? s.tab.x1 : s.pane.x1) - h), bar.y1,
                     LV_MIN(scrW - 1, LV_MAX(s.pane.x2, s.hasTab ? s.tab.x2 : s.pane.x2) + h), bar.y2 };
    if (at.x1 > at.x2 || at.y1 > at.y2) return;
    BarMask *m = nullptr;
    for (BarMask &b : s_barMask)
        if (b.ok && b.h == h && b.amp == amp && b.str == str && !memcmp(&b.key, &s, sizeof(s)) &&
            !memcmp(&b.at, &at, sizeof(at))) m = &b;
    if (!m) {
        m = &s_barMask[s_barNext];
        s_barNext = (uint8_t)((s_barNext + 1) % 2);
        m->ok = false;
        const int32_t w = lv_area_get_width(&at), ht = lv_area_get_height(&at);
        if (!alloc(m->im, w, ht)) return;
        uint8_t *p = m->im.buf;
        for (int32_t y = at.y1; y <= at.y2; y++)
            for (int32_t x = at.x1; x <= at.x2; x++, p++) {
                const float d = shapeDist(x + .5f, y + .5f, s);
                *p = d <= 0.f ? 0 : dithered(strengthOf(profile(d, (float)h), str), x, y, amp);
            }
        m->key = s; m->at = at; m->h = h; m->amp = amp; m->str = str; m->ok = true;
    }
    put(layer, m->im, at, at.x1, at.y1, false);
}

}  // namespace Kit
