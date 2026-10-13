// The control deck and the faces drawn by hand (2.12 P1, split from CardPopup.cpp).
#include "PopupState.h"

namespace popup {

Deck d;

// ---------------------------------------------------------------------------
// THE CONTROL DECK (owner, 2026-10-07 - after HTC's TouchFLO 3D)
//
// A RIBBON a little shorter than the switches, and on it a SELECTOR as tall as
// the deck, which can be grabbed and slid along it. The selector carries the
// chosen switch's icon at the next size up, in the accent; its icon changes
// to a neighbour's once the selector is more than halfway over it, and on
// release it snaps to the nearest switch, which is then chosen. A tap on a
// switch glides the selector there. Power is an action, not a mode, so it is
// a plain button at the start, and the selector never lands on it.
//
// The deck is the same size as before - the ribbon is what got shorter - so
// nothing around it moves.
// ---------------------------------------------------------------------------
constexpr uint32_t DECK_GLIDE_MS = 140;

// FOUR LOOKS, ALL KEPT (owner, round 6: "they all look so good"): Black or
// Silver, Square or Round. Unchosen, each scheme has its own: Black - Square
// on Midnight and Fleet, Silver - Round on Linen (owner, round 9). Chosen in
// SETTINGS' "Selector" row (every build; it belongs on the device's own
// settings page, 4.1) - and chosen FOR THE SCHEME SHOWING (owner, round 9:
// "the selector should follow the scheme"). Schemes are per page, so a page in
// Linen and a page in Fleet each keep their own; a page that changes scheme
// takes that scheme's.
constexpr uint8_t LOOK_SCHEMES = 4;   // UITokens' schemes, with room for one more
int8_t s_deckLook[LOOK_SCHEMES] = { -1, -1, -1, -1 };   // -1: the scheme's own; else bit 0 round, bit 1 silver

// Saved as a setting of the scheme (2.10d, DECISIONS K36): schemes.<name>.
// selector, by name, in the order of SETTINGS' row. The scheme's name is its
// palette's, lower case ("linen").
const char *const LOOK_NAMES[4] = { "black_square", "black_round", "silver_square", "silver_round" };
uint8_t s_lookLoaded = 0;   // bit per scheme index: read from the settings file yet

void schemeKey(char *out, size_t cap) {
    const char *n = UI::pal().name ? UI::pal().name : "scheme";
    size_t k = 0;
    for (; n[k] && k + 1 < cap; k++) out[k] = (char)tolower((unsigned char)n[k]);
    out[k] = '\0';
}

int8_t &chosenLook() {
    uint8_t i = UI::schemeIndex();
    if (i >= LOOK_SCHEMES) i = 0;
    if (!(s_lookLoaded & (1u << i))) {
        s_lookLoaded |= (uint8_t)(1u << i);
        char key[16], v[20];
        schemeKey(key, sizeof(key));
        if (Settings::scheme(key, "selector", v, sizeof(v)))
            for (int8_t k = 0; k < 4; k++)
                if (!strcmp(v, LOOK_NAMES[k])) s_deckLook[i] = k;
    }
    return s_deckLook[i];
}

// Choose the look for the scheme showing, and keep it; -1 is the scheme's
// own, which removes the saved choice (the Reset).
void keepLook(int8_t look) {
    chosenLook() = look;
    char key[16];
    schemeKey(key, sizeof(key));
    Settings::setScheme(key, "selector", look < 0 ? nullptr : LOOK_NAMES[look & 3]);
}

uint8_t deckLook() {
    if (chosenLook() >= 0) return (uint8_t)chosenLook();
    return lumOf(UI::pal().SURFACE_ALT) < 128 ? 0 : 3;
}


// From the scheme, never written down (tokens.md). The ribbon: lighter than
// the window on the dark schemes, darker on Linen. Icons in the text colour;
// the chosen one in the accent. The selector's own colours are in its face
// (metalFace()).
DeckColours deckColours() {
    const UIPalette &p = UI::pal();
    const bool dark = lumOf(p.SURFACE_ALT) < 128;
    DeckColours c;
    c.ribbon = UI::mix(p.SURFACE_ALT, p.TEXT, dark ? 14 : 12);
    c.icon   = p.TEXT;
    c.chosen = p.ACCENT;
    return c;
}

// Rounded squares or round, for the deck and its selector.
int32_t deckRadius() { return (deckLook() & 1) ? LV_RADIUS_CIRCLE : mm(UI::shape().R_BUTTON); }

// ---------------------------------------------------------------------------
// FACES DRAWN BY HAND (owner, round 6): the selector's metal, and the chips'.
//
// LVGL's gradient has two stops and runs straight across; the owner asked for
// the line between the selector's light and dark halves to CURVE UP in the
// middle - "a slight raised appearance" - in the upper two fifths, and for
// the unlit chips to look like soft DENTS in the surface rather than flat
// discs. So these faces are images, painted pixel by pixel when a window
// first needs them and kept in PSRAM (a few, ~40 KB each): a raised metal
// face - light above a curved line, dark below, faint horizontal streaks for
// a brushed finish, a fine edge - and a dent - darker at the top, fading out
// downwards, no edge. Black or silver (the owner's four looks); the corners
// anti-aliased in the alpha, so no clip_corner and no layer.
// ---------------------------------------------------------------------------
struct FaceSlot {
    int16_t  w = 0, h = 0, r = 0;
    FaceKind kind = FaceKind::FACE_BLACK;
    uint32_t key = 0;               // the colours it was painted with
    uint32_t used = 0;              // the window (s_faceEpoch) that last asked for it
    size_t   cap = 0;               // bytes held
    uint8_t *buf = nullptr;
    lv_image_dsc_t dsc = {};
};
constexpr uint8_t FACE_SLOTS = 12;
FaceSlot s_faces[FACE_SLOTS];
uint32_t s_faceEpoch = 1;           // one per window opened (showWindow())
uint32_t s_faceFlat  = 0;           // faces that could not be had and came out flat (debug readout)

inline uint8_t ch8(uint32_t hex, int sh) { return (uint8_t)((hex >> sh) & 0xFF); }

// a..b by t (0..1), per channel.
uint32_t lerpHex(uint32_t a, uint32_t b, float t) {
    t = t < 0.f ? 0.f : t > 1.f ? 1.f : t;
    auto c = [&](int sh) { return (uint32_t)lroundf(ch8(a, sh) + (ch8(b, sh) - ch8(a, sh)) * t) << sh; };
    return c(16) | c(8) | c(0);
}

void paintFace(FaceSlot &f) {
    const UIPalette &p = UI::pal();
    // The darkest and lightest the scheme has, whichever way round it is.
    const bool darkScheme = lumOf(p.SURFACE_ALT) < 128;
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
        uint32_t hs = (uint32_t)y * 2654435761u;
        const int streak = (f.kind == FaceKind::FACE_DENT) ? 0 : (int)((hs >> 28) & 7) - 3;
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
            } else {
                // The line: two fifths down at the sides, rising in the middle.
                const float u = (cx - w * 0.5f) / (w * 0.5f);
                const float line = h * 0.40f - h * 0.09f * (1.f - u * u);
                const uint32_t above = lerpHex(topA, topB, cy / line);
                const uint32_t below = lerpHex(botA, botB, (cy - line) / (h - line));
                const float d = cy - line;   // anti-aliased across one pixel
                col = d <= -0.5f ? above : d >= 0.5f ? below : lerpHex(above, below, d + 0.5f);
                if (inside < 1.3f) col = lerpHex(col, edge, 0.75f);   // the fine edge
            }
            const float th = (BAYER[(y & 3) * 4 + (x & 3)] + 0.5f) / 16.f;
            auto chan = [&](int sh, int step) {
                const int v = ch8(col, sh) + streak;
                const int q = (int)floorf(v / (float)step + th) * step;
                return (uint8_t)(q < 0 ? 0 : q > 256 - step ? 256 - step : q);
            };
            px[0] = chan(0, 8); px[1] = chan(8, 4); px[2] = chan(16, 8); px[3] = a;   // B, G, R, A
            px += 4;
        }
    }
}

// A face of this size and kind in the scheme's colours: found, or painted.
// TWELVE SLOTS, REUSED (round 8, the 1060 on Linen): each scheme, look and
// size takes one, and trying the looks in two schemes filled all twelve -
// after which every face came out flat (a white disc for a lit chip on
// Linen). A slot is taken back only if no object can be showing it: one
// painted in another scheme's colours first (the window that used it has
// closed - a scheme changes from the dashboard), else the one asked for
// longest ago by a window that is not this one. Full of this window's own:
// null, and the caller paints flat.
const lv_image_dsc_t *face(int32_t w, int32_t h, int32_t r, FaceKind kind) {
    const UIPalette &p = UI::pal();
    const uint32_t key = p.GROUND ^ (p.SURFACE << 1) ^ (p.TEXT << 2) ^ (p.SURFACE_ALT << 3);
    FaceSlot *spare = nullptr;
    for (FaceSlot &f : s_faces) {
        if (f.buf && f.w == w && f.h == h && f.r == r && f.kind == kind && f.key == key) {
            f.used = s_faceEpoch;
            return &f.dsc;
        }
        if (!f.buf && !spare) spare = &f;
    }
    if (!spare)
        for (FaceSlot &f : s_faces)
            if (f.key != key && f.used < s_faceEpoch && (!spare || f.used < spare->used)) spare = &f;
    if (!spare)
        for (FaceSlot &f : s_faces)
            if (f.used < s_faceEpoch && (!spare || f.used < spare->used)) spare = &f;
    if (!spare) { s_faceFlat++; return nullptr; }
    const size_t bytes = (size_t)w * h * 4;
    if (spare->buf) {
        // LVGL keeps what it decoded from a source, by its address: forget
        // it before the same descriptor shows something else.
        lv_image_cache_drop(&spare->dsc);
        if (spare->cap < bytes) { heap_caps_free(spare->buf); spare->buf = nullptr; spare->cap = 0; }
    }
    if (!spare->buf) {
        spare->buf = (uint8_t *)heap_caps_malloc(bytes, MALLOC_CAP_SPIRAM);
        if (!spare->buf) { s_faceFlat++; return nullptr; }
        spare->cap = bytes;
    }
    spare->used = s_faceEpoch;
    spare->w = (int16_t)w; spare->h = (int16_t)h; spare->r = (int16_t)r;
    spare->kind = kind; spare->key = key;
    paintFace(*spare);
    lv_image_dsc_t &d = spare->dsc;
    d = lv_image_dsc_t{};
    d.header.magic  = LV_IMAGE_HEADER_MAGIC;
    d.header.cf     = LV_COLOR_FORMAT_ARGB8888;
    d.header.w      = (uint32_t)w;
    d.header.h      = (uint32_t)h;
    d.header.stride = (uint32_t)w * 4;
    d.data_size     = (uint32_t)bytes;
    d.data          = spare->buf;
    return &d;
}

FaceKind metalKind() { return (deckLook() & 2) ? FaceKind::FACE_SILVER : FaceKind::FACE_BLACK; }

// Put a face behind an object's contents: an image as its first child, the
// object's own background off. Returns the image (null if none could be had,
// and the object keeps a flat colour).
lv_obj_t *setFace(lv_obj_t *o, int32_t w, int32_t h, int32_t r, FaceKind kind, uint32_t flat) {
    lv_obj_t *img = nullptr;
    for (uint32_t i = 0; i < lv_obj_get_child_count(o); i++) {
        lv_obj_t *c = lv_obj_get_child(o, (int32_t)i);
        if (lv_obj_check_type(c, &lv_image_class)) { img = c; break; }
    }
    const lv_image_dsc_t *src = face(w, h, r, kind);
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
    lv_image_set_src(img, src);
    lv_obj_set_style_bg_opa(o, LV_OPA_TRANSP, 0);
    return img;
}

// The label inside a chip or button (its face image sits before it).
lv_obj_t *labelIn(lv_obj_t *o) {
    for (uint32_t i = 0; i < lv_obj_get_child_count(o); i++) {
        lv_obj_t *c = lv_obj_get_child(o, (int32_t)i);
        if (lv_obj_check_type(c, &lv_label_class)) return c;
    }
    return nullptr;
}


void forgetDeck() { d = Deck{}; }

int32_t deckSelX(uint8_t i) { return d.x[i] - d.selW / 2; }

// The switch nearest the selector's centre - more than halfway over it.
uint8_t deckNearest(int32_t selX) {
    const int32_t c = selX + d.selW / 2;
    uint8_t best = 0;
    for (uint8_t i = 1; i < d.n; i++)
        if (LV_ABS(d.x[i] - c) < LV_ABS(d.x[best] - c)) best = i;
    return best;
}

// Move the selector; its icon follows the nearest switch.
void deckPlace(int32_t x) {
    if (!d.sel) return;
    lv_obj_set_x(d.sel, x);
    const uint8_t i = deckNearest(x);
    if (i != d.shown && d.selIcon) { d.shown = i; lv_label_set_text(d.selIcon, d.glyph[i]); }
}

void deckGlideExec(void *var, int32_t v) { (void)var; deckPlace(v); }

// The selector has arrived: that switch is chosen. Only what is showing is
// built, so a new control rebuilds the view - after the glide, never during.
void deckChoose(uint8_t i) {
    if (i >= d.n || d.ctl[i] == s.lightCtl) return;
    s.lightCtl = d.ctl[i];
    lv_async_call(rebuildMainAsync, nullptr);
}

void deckGlideDone(lv_anim_t *a) { (void)a; deckChoose(d.shown); }

void deckGlideTo(uint8_t i) {
    if (!d.sel || i >= d.n) return;
    lv_anim_delete(d.sel, deckGlideExec);
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var        (&a, d.sel);
    lv_anim_set_values     (&a, lv_obj_get_x(d.sel), deckSelX(i));
    lv_anim_set_duration   (&a, DECK_GLIDE_MS);
    lv_anim_set_path_cb    (&a, lv_anim_path_ease_out);
    lv_anim_set_exec_cb    (&a, deckGlideExec);
    lv_anim_set_completed_cb(&a, deckGlideDone);
    lv_anim_start(&a);
}

// A tap on a mode switch.
void deckTapCb(lv_event_t *ev) {
    if (pausedBlocks()) return;
    deckGlideTo((uint8_t)(uintptr_t)lv_event_get_user_data(ev));
}

// The finger on the selector: it follows along the ribbon, clamped to the
// first and last switch, and snaps on release.
void deckDragCb(lv_event_t *ev) {
    const lv_event_code_t code = lv_event_get_code(ev);
    lv_indev_t *in = lv_indev_active();
    lv_point_t pt = {0, 0};
    if (in) lv_indev_get_point(in, &pt);
    if (code == LV_EVENT_PRESSED) {
        lv_anim_delete(d.sel, deckGlideExec);
        d.pressX = pt.x;
        d.selX0  = lv_obj_get_x(d.sel);
        d.dragging = false;
    } else if (code == LV_EVENT_PRESSING) {
        const int32_t dx = pt.x - d.pressX;
        if (!d.dragging && LV_ABS(dx) < mm(UI::shape().SLOP_SELECTOR)) return;
        if (!d.dragging && pausedBlocks()) { if (in) lv_indev_wait_release(in); return; }
        d.dragging = true;
        deckPlace(LV_CLAMP(deckSelX(0), d.selX0 + dx, deckSelX(d.n - 1)));
    } else if (code == LV_EVENT_RELEASED || code == LV_EVENT_PRESS_LOST) {
        if (!d.dragging) return;
        d.dragging = false;
        deckGlideTo(deckNearest(lv_obj_get_x(d.sel)));
    }
}

// Power: a plain button.
void deckPowerCb(lv_event_t *ev) {
    (void)ev;
    if (!pausedBlocks()) commandAll(!aggregate().on);
}

void buildControlDeck(lv_obj_t *parent, uint8_t caps) {
    const UIType &t = UI::type();
    const DeckColours col = deckColours();
    const int32_t slot = UI::minTouch(), g = mm(UI::shape().S1);
    const int32_t H = controlDeckH();

    forgetDeck();
    if (caps & LIGHT_CAN_DIM)    { d.ctl[d.n] = LCTL_DIM;    d.glyph[d.n++] = MDI_BRIGHTNESS_5; }
    if (caps & LIGHT_CAN_TEMP)   { d.ctl[d.n] = LCTL_TEMP;   d.glyph[d.n++] = MDI_SUN_THERMOMETER; }
    if (caps & LIGHT_CAN_COLOUR) { d.ctl[d.n] = LCTL_COLOUR; d.glyph[d.n++] = MDI_PALETTE; }
    const int32_t W = selectorW(caps);

    d.obj = plain(parent);
    lv_obj_set_size(d.obj, W, H);

    // The ribbon: three quarters of a switch tall, centred - IN TWO PIECES
    // (owner: no divider line - the selector never goes to Power anyway).
    // Power has a short ribbon of its own; a break; then the modes'.
    const int32_t rh = slot * 72 / 100;
    const int32_t powerW = slot + 2 * g;
    auto ribbon = [&](int32_t x0, int32_t w) {
        lv_obj_t *rib = plain(d.obj);
        lv_obj_set_pos (rib, x0, (H - rh) / 2);
        lv_obj_set_size(rib, w, rh);
        lv_obj_set_style_radius  (rib, deckRadius(), 0);
        lv_obj_set_style_bg_color(rib, UI::c(quiet(col.ribbon)), 0);
        lv_obj_set_style_bg_opa  (rib, LV_OPA_COVER, 0);
    };
    ribbon(0, powerW);
    if (d.n) ribbon(powerW + deckBreak(), W - powerW - deckBreak());

    auto iconAt = [&](int32_t x, const char *glyph, lv_event_cb_t cb, void *user) {
        lv_obj_t *b = plain(d.obj);
        lv_obj_set_pos (b, x, 0);
        lv_obj_set_size(b, slot, H);
        lv_obj_add_flag(b, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(b, cb, LV_EVENT_CLICKED, user);
        lv_obj_t *l = makeLabel(b, t.ICON_MD, quiet(col.icon));
        lv_label_set_text(l, glyph);
        lv_obj_center(l);
        return b;
    };

    int32_t x = g;
    s.btnCtl[0] = iconAt(x, MDI_POWER, deckPowerCb, nullptr);
    x = powerW + deckBreak() + g;
    uint8_t chosen = 0;
    for (uint8_t i = 0; i < d.n; i++) {
        s.btnCtl[1 + d.ctl[i]] = iconAt(x, d.glyph[i], deckTapCb, (void *)(uintptr_t)i);
        d.x[i] = x + slot / 2;
        if (d.ctl[i] == s.lightCtl) chosen = i;
        x += slot + g;
    }
    if (!d.n) return;

    // The selector: the deck's height, a little wider than a switch, over the
    // ribbon - with the chosen switch's icon, a size up, in the accent.
    d.selW = slot + 2 * g;
    d.sel = plain(d.obj);
    lv_obj_set_size(d.sel, d.selW, H);
    lv_obj_set_style_radius(d.sel, deckRadius(), 0);
    // Its face is metal, curved and brushed (faces, above). Greyed when the
    // window is paused, as everything else is.
    const int32_t selR = (deckLook() & 1) ? LV_MIN(d.selW, H) / 2 : mm(UI::shape().R_BUTTON);
    if (lv_obj_t *img = setFace(d.sel, d.selW, H, selR, metalKind(), quiet(UI::pal().GROUND))) {
        if (s.builtPaused) {
            lv_obj_set_style_image_recolor    (img, UI::c(UI::pal().SURFACE_ALT), 0);
            lv_obj_set_style_image_recolor_opa(img, (lv_opa_t)166, 0);
        }
    }
    lv_obj_add_flag  (d.sel, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(d.sel, LV_OBJ_FLAG_GESTURE_BUBBLE);
    lv_obj_clear_flag(d.sel, LV_OBJ_FLAG_SCROLL_CHAIN);
    lv_obj_add_event_cb(d.sel, deckDragCb, LV_EVENT_PRESSED,    nullptr);
    lv_obj_add_event_cb(d.sel, deckDragCb, LV_EVENT_PRESSING,   nullptr);
    lv_obj_add_event_cb(d.sel, deckDragCb, LV_EVENT_RELEASED,   nullptr);
    lv_obj_add_event_cb(d.sel, deckDragCb, LV_EVENT_PRESS_LOST, nullptr);
    d.selIcon = makeLabel(d.sel, t.ICON, quiet(col.chosen));
    lv_obj_center(d.selIcon);
    // LARGER, TO FILL MORE OF THE SELECTOR (owner, round 7). ICON is the
    // largest icon face there is, so it is scaled up 1.4x about its centre -
    // drawn through a layer, the size of the icon. If that is visibly soft on
    // glass, the answer is a face of just the deck's few glyphs at this size
    // (a few KB), from scripts/gen_icon_font.py.
    lv_obj_set_style_transform_scale  (d.selIcon, 358, 0);
    lv_obj_set_style_transform_pivot_x(d.selIcon, lv_pct(50), 0);
    lv_obj_set_style_transform_pivot_y(d.selIcon, lv_pct(50), 0);
    d.shown = chosen;
    lv_label_set_text(d.selIcon, d.glyph[chosen]);
    lv_obj_set_pos(d.sel, deckSelX(chosen), 0);
}

} // namespace popup
