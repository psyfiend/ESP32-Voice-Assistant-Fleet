// FACES DRAWN BY HAND, and the selector's look (2.12 P3, moved from the card
// window's control deck - owner, round 6, 2026-10-06).
//
// LVGL's gradient has two stops and runs straight across; the owner asked for
// the line between the selector's light and dark halves to CURVE UP in the
// middle - "a slight raised appearance" - in the upper two fifths, and for
// the unlit buttons to look like soft DENTS in the surface rather than flat
// discs. So these faces are images, painted pixel by pixel when a window
// first needs them and kept in PSRAM (a few, ~40 KB each): a raised metal
// face - light above a curved line, dark below, faint horizontal streaks for
// a brushed finish, a fine edge - and a dent - darker at the top, fading out
// downwards, no edge. Black or silver (the owner's four looks); the corners
// anti-aliased in the alpha, so no clip_corner and no layer.
#include "UI/Kit.h"
#include "Settings.h"
#include "esp_heap_caps.h"                              // the faces live in PSRAM
#include "src/misc/cache/instance/lv_image_cache.h"     // lv_image_cache_drop(): not in lvgl.h
#include <ctype.h>
#include <math.h>
#include <string.h>

namespace Kit {

// Rough luminance, 0-255.
static int32_t lum(uint32_t hex) {
    return (int32_t)(((hex >> 16) & 0xFF) * 299 + ((hex >> 8) & 0xFF) * 587 + (hex & 0xFF) * 114) / 1000;
}

// ---------------------------------------------------------------------------
// THE SELECTOR'S LOOK. FOUR, ALL KEPT (owner, round 6: "they all look so
// good"): Black or Silver, Square or Round. Unchosen, each scheme has its own:
// Black - Square on Midnight and Fleet, Silver - Round on Linen (owner, round
// 9). Chosen FOR THE SCHEME SHOWING (owner, round 9: "the selector should
// follow the scheme"), from the system panel (K45). Schemes are per page, so a
// page in Linen and a page in Fleet each keep their own.
// ---------------------------------------------------------------------------
constexpr uint8_t LOOK_SCHEMES = 4;   // UITokens' schemes, with room for one more
static int8_t  s_look[LOOK_SCHEMES] = { -1, -1, -1, -1 };   // -1: the scheme's own; else bit 0 round, bit 1 silver
static uint8_t s_lookLoaded = 0;      // bit per scheme index: read from the settings file yet

// Saved as a setting of the scheme (2.10d, DECISIONS K36): schemes.<name>.
// selector, by name. The scheme's name is its palette's, lower case ("linen").
static const char *const LOOK_NAMES[4] = { "black_square", "black_round", "silver_square", "silver_round" };

static void schemeKey(char *out, size_t cap) {
    const char *n = UI::pal().name ? UI::pal().name : "scheme";
    size_t k = 0;
    for (; n[k] && k + 1 < cap; k++) out[k] = (char)tolower((unsigned char)n[k]);
    out[k] = '\0';
}

static int8_t &lookSlot() {
    uint8_t i = UI::schemeIndex();
    if (i >= LOOK_SCHEMES) i = 0;
    if (!(s_lookLoaded & (1u << i))) {
        s_lookLoaded |= (uint8_t)(1u << i);
        char key[16], v[20];
        schemeKey(key, sizeof(key));
        if (Settings::scheme(key, "selector", v, sizeof(v)))
            for (int8_t k = 0; k < 4; k++)
                if (!strcmp(v, LOOK_NAMES[k])) s_look[i] = k;
    }
    return s_look[i];
}

int8_t selectorLook() { return lookSlot(); }

void keepSelectorLook(int8_t look) {
    lookSlot() = look;
    char key[16];
    schemeKey(key, sizeof(key));
    Settings::setScheme(key, "selector", look < 0 ? nullptr : LOOK_NAMES[look & 3]);
}

// Every scheme's look read again from the settings file when next asked for
// (UI Tweaks' Reset all has just cleared them there).
void forgetSelectorLooks() {
    for (int8_t &l : s_look) l = -1;
    s_lookLoaded = 0;
}

uint8_t selectorLookNow() {
    if (lookSlot() >= 0) return (uint8_t)lookSlot();
    return lum(UI::pal().SURFACE_ALT) < 128 ? 0 : 3;
}

FaceKind metalKind() { return (selectorLookNow() & 2) ? FaceKind::FACE_SILVER : FaceKind::FACE_BLACK; }

int32_t selectorRadius() { return (selectorLookNow() & 1) ? LV_RADIUS_CIRCLE : UI::mm(UI::shape().R_CONTROL); }

// From the scheme, never written down (tokens.md). The ribbon: lighter than
// the window on the dark schemes, darker on Linen. Icons in the text colour;
// the chosen one in the accent. The selector's own colours are in its face.
SelectorColours selectorColours() {
    const UIPalette &p = UI::pal();
    const bool dark = lum(p.SURFACE_ALT) < 128;
    SelectorColours c;
    c.ribbon = UI::mix(p.SURFACE_ALT, p.TEXT, dark ? 14 : 12);
    c.icon   = p.TEXT;
    c.chosen = p.ACCENT;
    return c;
}

// ---------------------------------------------------------------------------
// The faces
// ---------------------------------------------------------------------------
struct FaceSlot {
    int16_t  w = 0, h = 0, r = 0;
    FaceKind kind = FaceKind::FACE_BLACK;
    uint32_t key = 0;               // the colours it was painted with
    uint32_t tint = 0;              // a satin face's own colour
    int16_t  band = 0;              // a metal face's rail seen through it (0: none)
    uint32_t used = 0;              // the window (s_epoch) that last asked for it
    size_t   cap = 0;               // bytes held
    uint8_t *buf = nullptr;
    lv_image_dsc_t dsc = {};
};
constexpr uint8_t FACE_SLOTS = 16;   // 12 until #99's satin and glass faces
static FaceSlot s_faces[FACE_SLOTS];
static uint32_t s_epoch = 1;        // one per window opened (facesNextWindow())
static uint32_t s_flat  = 0;        // faces that could not be had and came out flat (debug readout)

void     facesNextWindow() { s_epoch++; }
uint32_t facesFlat()       { return s_flat; }

static inline uint8_t ch8(uint32_t hex, int sh) { return (uint8_t)((hex >> sh) & 0xFF); }

// a..b by t (0..1), per channel.
static uint32_t lerpHex(uint32_t a, uint32_t b, float t) {
    t = t < 0.f ? 0.f : t > 1.f ? 1.f : t;
    auto c = [&](int sh) { return (uint32_t)lroundf(ch8(a, sh) + (ch8(b, sh) - ch8(a, sh)) * t) << sh; };
    return c(16) | c(8) | c(0);
}

static void paintFace(FaceSlot &f) {
    const UIPalette &p = UI::pal();
    // The darkest and lightest the scheme has, whichever way round it is.
    const bool darkScheme = lum(p.SURFACE_ALT) < 128;
    const uint32_t dk = darkScheme ? p.GROUND : p.TEXT;
    const uint32_t lt = darkScheme ? p.TEXT : p.SURFACE;
    uint32_t topA, topB, botA, botB, edge;
    if (f.kind == FaceKind::FACE_SILVER) {
        topA = UI::mix(lt, dk, 4);  topB = UI::mix(lt, dk, 16);
        botA = UI::mix(lt, dk, 42); botB = UI::mix(lt, dk, 30);
        edge = UI::mix(lt, dk, 52);
    } else {   // black (and the dent's colours are set below)
        topA = UI::mix(dk, lt, 40); topB = UI::mix(dk, lt, 24);
        botA = UI::mix(dk, lt, 6);  botB = UI::mix(dk, lt, 12);
        edge = UI::mix(dk, lt, 52);
    }
    const uint32_t base = p.SURFACE_ALT;   // a dent is pressed into the window
    const uint32_t rail = UI::mix(selectorColours().ribbon, dk, 25);   // the rail through the lens

    // The panels are 16-bit: red and blue in steps of 8, green in steps of 4.
    // A smooth grey ramp truncated to that grid bands, and the bands go pink
    // and green where green steps and the others do not (seen on glass, round
    // 7). So each pixel is rounded to the grid itself, by a 4x4 ordered
    // dither, the same threshold for every channel.
    static const uint8_t BAYER[16] = { 0, 8, 2, 10, 12, 4, 14, 6, 3, 11, 1, 9, 15, 7, 13, 5 };

    const int w = f.w, h = f.h, r = LV_MIN(f.r, LV_MIN(w, h) / 2);
    uint8_t *px = f.buf;
    for (int y = 0; y < h; y++) {
        // Brushed: each row a touch lighter or darker, the same every time.
        // Metal only - a dent is soft, and satin smooth.
        uint32_t hs = (uint32_t)y * 2654435761u;
        const bool brushed = f.kind == FaceKind::FACE_BLACK || f.kind == FaceKind::FACE_SILVER;
        const int streak = brushed ? (int)((hs >> 28) & 7) - 3 : 0;
        const float cy = y + 0.5f;
        for (int x = 0; x < w; x++) {
            const float cx = x + 0.5f;
            // Distance inside the rounded rectangle's edge (negative: outside).
            const float qx = cx < r ? r - cx : cx > w - r ? cx - (w - r) : 0.f;
            const float qy = cy < r ? r - cy : cy > h - r ? cy - (h - r) : 0.f;
            float inside;
            if (qx > 0.f && qy > 0.f) inside = r - sqrtf(qx * qx + qy * qy);
            else inside = LV_MIN(LV_MIN(cx, w - cx), LV_MIN(cy, h - cy));
            const float cover = inside + 0.5f;
            const uint8_t a = cover <= 0.f ? 0 : cover >= 1.f ? 255 : (uint8_t)(cover * 255.f);

            uint32_t col;
            if (f.kind == FaceKind::FACE_DENT) {
                // Pressed in: shade at the top, gone by the middle, a whisper
                // of light at the bottom - the selector's light and dark the
                // other way up, and softer.
                const float t = cy / h;
                col = (t < 0.5f) ? lerpHex(UI::mix(base, dk, 45), base, t / 0.5f)
                                 : lerpHex(base, UI::mix(base, lt, 7), (t - 0.5f) / 0.5f);
            } else if (f.kind == FaceKind::FACE_SATIN) {
                // SATIN (#99, the mock's numbers): light at the top fading by
                // the middle, a shade growing below it - a faint step between
                // the two - and a soft edge. A bright colour (the Remove
                // button's red) carries a stronger light; on a light scheme the
                // edge is a shade, or it would vanish into the window.
                const bool bright = lum(f.tint) >= 128;
                const uint8_t hiTop = bright ? 22 : 10, hiMid = bright ? 7 : 3, shade = bright ? 10 : 12;
                const float t = cy / h;
                col = (t < 0.5f) ? lerpHex(UI::mix(f.tint, lt, hiTop), UI::mix(f.tint, lt, hiMid), t / 0.5f)
                                 : lerpHex(f.tint, UI::mix(f.tint, dk, shade), (t - 0.5f) / 0.5f);
                const uint32_t rim = darkScheme ? UI::mix(f.tint, lt, bright ? 22 : 11) : UI::mix(f.tint, dk, 15);
                if (inside < 1.3f) col = lerpHex(col, rim, 0.75f);
            } else {
                // The line: two fifths down at the sides, rising in the middle.
                const float u = (cx - w * 0.5f) / (w * 0.5f);
                const float line = h * 0.40f - h * 0.09f * (1.f - u * u);
                const uint32_t above = lerpHex(topA, topB, cy / line);
                const uint32_t below = lerpHex(botA, botB, (cy - line) / (h - line));
                const float d = cy - line;   // anti-aliased across one pixel
                col = d <= -0.5f ? above : d >= 0.5f ? below : lerpHex(above, below, d + 0.5f);
                // THE RAIL THROUGH THE LENS (owner, #99): the selector magnifies
                // what it slides over, so the rail shows through it - taller
                // than the rail outside, darker and faded (the rail's own
                // colour, a quarter of the way to the scheme's darkest, half
                // into the metal), behind the chosen icon, edge to edge (the
                // face's own outline cuts it). Lighter than black glass and
                // darker than silver, so it shows on both.
                if (f.band > 0) {
                    const float in = f.band * 0.5f - fabsf(cy - h * 0.5f);   // > 0: inside the band
                    const float k = in <= -0.75f ? 0.f : in >= 0.75f ? 1.f : (in + 0.75f) / 1.5f;
                    if (k > 0.f) col = lerpHex(col, rail, 0.5f * k);
                }
                if (inside < 1.3f) col = lerpHex(col, edge, 0.75f);   // the fine edge
            }
            const float th = (BAYER[(y & 3) * 4 + (x & 3)] + 0.5f) / 16.f;
            auto chan = [&](int sh, int step) {
                const int v = ch8(col, sh) + streak;
                const int q = (int)floorf(v / (float)step + th) * step;
                return (uint8_t)(q < 0 ? 0 : q > 256 - step ? 256 - step : q);
            };
            if (f.kind == FaceKind::FACE_SATIN) {
                // 16 bits, the panel's own (see face()): already on its grid.
                const uint16_t c565 = (uint16_t)((chan(16, 8) >> 3) << 11 | (chan(8, 4) >> 2) << 5 | chan(0, 8) >> 3);
                px[0] = (uint8_t)(c565 & 0xFF); px[1] = (uint8_t)(c565 >> 8);
                px += 2;
                continue;
            }
            px[0] = chan(0, 8); px[1] = chan(8, 4); px[2] = chan(16, 8); px[3] = a;   // B, G, R, A
            px += 4;
        }
    }
}

// SATIN IS PAINTED IN 16 BITS (#99): no alpha, its corners cut by LVGL
// (the image's radius). Measured on the P4_5, a full redraw of Members with
// three ARGB8888 rows took 22 ms more than flat rows - an image with alpha is
// blended pixel by pixel - where an opaque 16-bit one is copied a line at a
// time, half the bytes from PSRAM. The metal and the dents keep their alpha.
static bool opaqueFace(FaceKind kind) { return kind == FaceKind::FACE_SATIN; }

// A face of this size and kind in the scheme's colours: found, or painted.
// TWELVE SLOTS, REUSED (round 8, the 1060 on Linen): each scheme, look and
// size takes one, and trying the looks in two schemes filled all twelve -
// after which every face came out flat (a white disc for a lit chip on
// Linen). A slot is taken back only if no object can be showing it: one
// painted in another scheme's colours first (the window that used it has
// closed - a scheme changes from the dashboard), else the one asked for
// longest ago by a window that is not this one. Full of this window's own:
// null, and the caller paints flat.
static const lv_image_dsc_t *face(int32_t w, int32_t h, int32_t r, FaceKind kind, uint32_t tint, int32_t band) {
    const UIPalette &p = UI::pal();
    const uint32_t key = p.GROUND ^ (p.SURFACE << 1) ^ (p.TEXT << 2) ^ (p.SURFACE_ALT << 3);
    FaceSlot *spare = nullptr;
    for (FaceSlot &f : s_faces) {
        if (f.buf && f.w == w && f.h == h && f.r == r && f.kind == kind && f.key == key && f.tint == tint &&
            f.band == band) {
            f.used = s_epoch;
            return &f.dsc;
        }
        if (!f.buf && !spare) spare = &f;
    }
    if (!spare)
        for (FaceSlot &f : s_faces)
            if (f.key != key && f.used < s_epoch && (!spare || f.used < spare->used)) spare = &f;
    if (!spare)
        for (FaceSlot &f : s_faces)
            if (f.used < s_epoch && (!spare || f.used < spare->used)) spare = &f;
    if (!spare) { s_flat++; return nullptr; }
    const uint32_t bpp = opaqueFace(kind) ? 2 : 4;
    const size_t bytes = (size_t)w * h * bpp;
    if (spare->buf) {
        // LVGL keeps what it decoded from a source, by its address: forget
        // it before the same descriptor shows something else.
        lv_image_cache_drop(&spare->dsc);
        if (spare->cap < bytes) { heap_caps_free(spare->buf); spare->buf = nullptr; spare->cap = 0; }
    }
    if (!spare->buf) {
        spare->buf = (uint8_t *)heap_caps_malloc(bytes, MALLOC_CAP_SPIRAM);
        if (!spare->buf) { s_flat++; return nullptr; }
        spare->cap = bytes;
    }
    spare->used = s_epoch;
    spare->w = (int16_t)w; spare->h = (int16_t)h; spare->r = (int16_t)r;
    spare->kind = kind; spare->key = key; spare->tint = tint; spare->band = (int16_t)band;
    paintFace(*spare);
    lv_image_dsc_t &d = spare->dsc;
    d = lv_image_dsc_t{};
    d.header.magic  = LV_IMAGE_HEADER_MAGIC;
    d.header.cf     = opaqueFace(kind) ? LV_COLOR_FORMAT_RGB565 : LV_COLOR_FORMAT_ARGB8888;
    d.header.w      = (uint32_t)w;
    d.header.h      = (uint32_t)h;
    d.header.stride = (uint32_t)w * bpp;
    d.data_size     = (uint32_t)bytes;
    d.data          = spare->buf;
    return &d;
}

static lv_obj_t *faceIn(lv_obj_t *o) {
    for (uint32_t i = 0; i < lv_obj_get_child_count(o); i++) {
        lv_obj_t *c = lv_obj_get_child(o, (int32_t)i);
        if (lv_obj_check_type(c, &lv_image_class)) return c;
    }
    return nullptr;
}

lv_obj_t *setFace(lv_obj_t *o, int32_t w, int32_t h, int32_t r, FaceKind kind, uint32_t flat, uint32_t tint,
                  int32_t band) {
    lv_obj_t *img = faceIn(o);
    const bool metal = kind == FaceKind::FACE_BLACK || kind == FaceKind::FACE_SILVER;
    const lv_image_dsc_t *src = face(w, h, r, kind, kind == FaceKind::FACE_SATIN ? tint : 0, metal ? band : 0);
    if (!src) {
        if (img) lv_obj_add_flag(img, LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_style_bg_color(o, UI::c(flat), 0);
        lv_obj_set_style_bg_opa  (o, LV_OPA_COVER, 0);
        return nullptr;
    }
    if (!img) {
        img = lv_image_create(o);
        lv_obj_clear_flag(img, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_flag  (img, LV_OBJ_FLAG_IGNORE_LAYOUT);
        lv_obj_move_to_index(img, 0);
        lv_obj_center(img);
    }
    lv_obj_clear_flag(img, LV_OBJ_FLAG_HIDDEN);
    // The same face again is no change - and setting it would redraw it, every
    // time a window refreshes its state (a scene's finish, Power's).
    if (lv_image_get_src(img) != src) lv_image_set_src(img, src);
    // An opaque face's corners are cut by LVGL; one with alpha has its own.
    const int32_t clip = opaqueFace(kind) ? LV_MIN(r, LV_MIN(w, h) / 2) : 0;
    if (lv_obj_get_style_radius(img, LV_PART_MAIN) != clip) lv_obj_set_style_radius(img, clip, 0);
    lv_obj_set_style_bg_opa(o, LV_OPA_TRANSP, 0);
    return img;
}

// ---------------------------------------------------------------------------
// THE FINISHES (#99, DECISIONS K77): satin for what can be tapped and is not
// chosen, glass - the selector's metal - for what is chosen or active.
// ---------------------------------------------------------------------------

// Between the scheme's two surfaces: the mock's satin on Midnight exactly.
uint32_t satinBase() { return UI::mix(UI::pal().SURFACE, UI::pal().SURFACE_ALT, 50); }

// INK ON GLASS (owner, #99 T6): Linen's olive "on" vanished into its silver.
// The metal's middle tone is about a fifth of the way from its own end of the
// scheme; a colour within 90 of it in lightness is pushed, a tenth at a time,
// toward the scheme's other end. Midnight's orange on black glass already
// stands 100 apart and is left as it is.
uint32_t glassInk(uint32_t hex) {
    const UIPalette &p = UI::pal();
    const bool darkScheme = lum(p.SURFACE_ALT) < 128;
    const uint32_t dk = darkScheme ? p.GROUND : p.TEXT;
    const uint32_t lt = darkScheme ? p.TEXT : p.SURFACE;
    const bool silver = metalKind() == FaceKind::FACE_SILVER;
    const int32_t face = silver ? lum(UI::mix(lt, dk, 23)) : lum(UI::mix(dk, lt, 20));
    uint32_t c = hex;
    for (uint8_t pct = 10; pct <= 90 && LV_ABS(lum(c) - face) < 90; pct += 10)
        c = UI::mix(hex, silver ? dk : lt, pct);
    return c;
}

// The finish a face shows, kept in its image's user data for a press.
enum : uintptr_t { FIN_NONE = 0, FIN_SATIN = 1, FIN_GLASS = 2 };

// Pressed: satin darkens - its light turned under - and glass lightens, as the
// round icon buttons did; the object itself sinks 1 px (KS_SINK).
//
// THE TINT IS A VEIL, A ROUNDED OBJECT OVER THE FACE IN THE FACE'S SHAPE - not
// the image's recolor. LVGL draws an image that is both recoloured and
// corner-clipped through a path that drops the clip (lv_draw_sw_img.c:
// radius_only() wants no recolor, recolor_only() no radius), so a pressed satin
// face showed square - a box over a header button, a square outline round
// Remove (owner, #99 T9).
static const char VEIL_TAG = 0;

static lv_obj_t *veilIn(lv_obj_t *o) {
    for (uint32_t i = 0; i < lv_obj_get_child_count(o); i++) {
        lv_obj_t *c = lv_obj_get_child(o, (int32_t)i);
        if (lv_obj_get_user_data(c) == (void *)&VEIL_TAG) return c;
    }
    return nullptr;
}

static void finishPressCb(lv_event_t *ev) {
    lv_obj_t *o = (lv_obj_t *)lv_event_get_current_target(ev);
    lv_obj_t *veil = veilIn(o);
    if (!veil) return;
    lv_obj_t *img = faceIn(o);
    const bool glass = img && (uintptr_t)lv_obj_get_user_data(img) == FIN_GLASS;
    const bool down = lv_event_get_code(ev) == LV_EVENT_PRESSED;
    const UIPalette &p = UI::pal();
    const bool darkScheme = lum(p.SURFACE_ALT) < 128;
    lv_obj_set_style_bg_color(veil, UI::c(glass ? p.TEXT : (darkScheme ? p.GROUND : p.TEXT)), 0);
    lv_obj_set_style_bg_opa(veil, down ? (glass ? LV_OPA_20 : LV_OPA_30) : LV_OPA_TRANSP, 0);
}

static bool hasCb(lv_obj_t *o, lv_event_cb_t cb) {
    for (uint32_t i = 0; i < lv_obj_get_event_count(o); i++)
        if (lv_event_dsc_get_cb(lv_obj_get_event_dsc(o, i)) == cb) return true;
    return false;
}

void finish(lv_obj_t *o, int32_t w, int32_t h, int32_t r, Finish f, uint32_t tint) {
    if (!o || w <= 0 || h <= 0) return;
    const bool glass = f == Finish::FINISH_GLASS;
    const uint32_t base = tint ? tint : satinBase();
    lv_obj_t *img = setFace(o, w, h, r, glass ? metalKind() : FaceKind::FACE_SATIN, base, base);
    if (img) {
        lv_obj_set_user_data(img, (void *)(glass ? FIN_GLASS : FIN_SATIN));
        // FLOATING, not only out of the layout: an object sized to its content
        // (a Members row) otherwise counts the face, which reaches into its
        // padding, and grows by that padding - 68 px rows became 106.
        lv_obj_add_flag(img, LV_OBJ_FLAG_FLOATING);
    }
    if (!hasCb(o, finishPressCb)) {
        add(o, KitStyle::KS_SINK, UI::part(LV_PART_MAIN, LV_STATE_PRESSED));
        lv_obj_add_event_cb(o, finishPressCb, LV_EVENT_PRESSED,    nullptr);
        lv_obj_add_event_cb(o, finishPressCb, LV_EVENT_RELEASED,   nullptr);
        lv_obj_add_event_cb(o, finishPressCb, LV_EVENT_PRESS_LOST, nullptr);
        // The veil (above): over the face, under the words, clear until pressed.
        lv_obj_t *veil = plain(o);
        lv_obj_set_user_data(veil, (void *)&VEIL_TAG);
        lv_obj_add_flag(veil, LV_OBJ_FLAG_FLOATING);
        lv_obj_set_size(veil, w, h);
        lv_obj_set_style_radius(veil, r, 0);
        lv_obj_set_style_bg_opa(veil, LV_OPA_TRANSP, 0);
        lv_obj_move_to_index(veil, faceIn(o) ? 1 : 0);
        lv_obj_center(veil);
    }
}

lv_obj_t *labelIn(lv_obj_t *o) {
    for (uint32_t i = 0; i < lv_obj_get_child_count(o); i++) {
        lv_obj_t *c = lv_obj_get_child(o, (int32_t)i);
        if (lv_obj_check_type(c, &lv_label_class)) return c;
    }
    return nullptr;
}

} // namespace Kit
