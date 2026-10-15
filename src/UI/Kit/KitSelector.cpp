// The selector and the round icon buttons (2.12 P3, moved from the card
// window's control deck and corner icons).
//
// THE SELECTOR (owner, 2026-10-07 - after HTC's TouchFLO 3D): a RIBBON a little
// shorter than the choices, and on it a SELECTOR as tall as the whole, which
// can be grabbed and slid along it. The selector carries the chosen choice's
// icon at the next size up, in the accent; its icon changes to a neighbour's
// once the selector is more than halfway over it, and on release it snaps to
// the nearest choice, which is then chosen. A tap on a choice glides the
// selector there. An ACTION (the window's Power) is a plain button at the
// start, on a ribbon of its own, and the selector never lands on it.
#include "UI/Kit.h"

namespace Kit {

constexpr uint32_t GLIDE_MS = 140;

static uint32_t quietHex(bool quiet, uint32_t hex) {
    return quiet ? UI::mix(hex, UI::pal().SURFACE_ALT, 65) : hex;
}

// ---------------------------------------------------------------------------
// Round icon buttons
// ---------------------------------------------------------------------------

// Pressed feedback for a button whose face is an image: the face tinted
// towards the text colour while a finger is on it.
static void facePressCb(lv_event_t *ev) {
    lv_obj_t *b = (lv_obj_t *)lv_event_get_current_target(ev);
    if (lv_obj_get_child_count(b) == 0) return;
    lv_obj_t *img = lv_obj_get_child(b, 0);
    if (!lv_obj_check_type(img, &lv_image_class)) return;
    const bool down = lv_event_get_code(ev) == LV_EVENT_PRESSED;
    lv_obj_set_style_image_recolor    (img, UI::c(UI::pal().TEXT), 0);
    lv_obj_set_style_image_recolor_opa(img, down ? LV_OPA_20 : LV_OPA_TRANSP, 0);
}

lv_obj_t *iconButton(lv_obj_t *parent, const char *glyph, const lv_font_t *f, lv_event_cb_t cb,
                     lv_obj_t **outLabel) {
    const UIPalette &p = UI::pal();
    const int32_t sz = UI::minTouch();
    lv_obj_t *b = plain(parent);
    lv_obj_set_size  (b, sz, sz);
    lv_obj_add_flag  (b, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_radius(b, LV_RADIUS_CIRCLE, 0);
    lv_obj_t *l = label(b, f, p.TEXT);
    lv_label_set_text(l, glyph);
    lv_obj_center(l);
    setFace(b, sz, sz, sz / 2, FaceKind::FACE_DENT, p.SURFACE);
    lv_obj_add_event_cb(b, facePressCb, LV_EVENT_PRESSED,    nullptr);
    lv_obj_add_event_cb(b, facePressCb, LV_EVENT_RELEASED,   nullptr);
    lv_obj_add_event_cb(b, facePressCb, LV_EVENT_PRESS_LOST, nullptr);
    lv_obj_add_event_cb(b, cb, LV_EVENT_CLICKED, nullptr);
    if (outLabel) *outLabel = l;
    return b;
}

// Lit as the selector is (owner, 2026-10-07): the same metal, the icon a face
// up and in the accent. Unlit: a soft dent in the surface (round 6).
void iconButtonLit(lv_obj_t *b, bool lit) {
    if (!b) return;
    const UIPalette &p = UI::pal();
    const int32_t sz = UI::minTouch();
    setFace(b, sz, sz, sz / 2, lit ? metalKind() : FaceKind::FACE_DENT, p.SURFACE);
    if (lv_obj_t *l = labelIn(b)) {
        lv_obj_set_style_text_color(l, UI::c(lit ? selectorColours().chosen : p.TEXT), 0);
        lv_obj_set_style_text_font (l, lit ? UI::type().ICON : UI::type().ICON_MD, 0);
        lv_obj_center(l);
    }
}

// ---------------------------------------------------------------------------
// The selector
// ---------------------------------------------------------------------------
int32_t selectorH() { return UI::minTouch() + 2 * UI::mm(UI::shape().S1); }

// The action on a ribbon of its own, a break, then the choices' ribbon.
static int32_t selectorBreak() { return UI::mm(UI::shape().S6); }

int32_t selectorW(uint8_t n) {
    const int32_t g = UI::mm(UI::shape().S1), slot = UI::minTouch();
    const int32_t action = slot + 2 * g;
    return n ? action + selectorBreak() + n * slot + (n + 1) * g : action;
}

static Selector *selOf(lv_obj_t *o) { return o ? (Selector *)lv_obj_get_user_data(o) : nullptr; }

static int32_t selX(const Selector &S, uint8_t i) { return S.x[i] - S.selW / 2; }

// The choice nearest the selector's centre - more than halfway over it.
static uint8_t nearest(const Selector &S, int32_t x) {
    const int32_t c = x + S.selW / 2;
    uint8_t best = 0;
    for (uint8_t i = 1; i < S.n; i++)
        if (LV_ABS(S.x[i] - c) < LV_ABS(S.x[best] - c)) best = i;
    return best;
}

// Move the selector; its icon follows the nearest choice.
static void place(Selector &S, int32_t x) {
    if (!S.sel) return;
    lv_obj_set_x(S.sel, x);
    const uint8_t i = nearest(S, x);
    if (i != S.shown && S.selIcon) { S.shown = i; lv_label_set_text(S.selIcon, S.glyph[i]); }
}

static void glideExec(void *var, int32_t v) {
    if (Selector *S = selOf((lv_obj_t *)var)) place(*S, v);
}

// The selector has arrived: that choice is chosen.
static void glideDone(lv_anim_t *a) {
    Selector *S = selOf((lv_obj_t *)a->var);
    if (S && S->shown < S->n && S->onChosen) S->onChosen(S->shown);
}

static void glideTo(Selector &S, uint8_t i) {
    if (!S.sel || i >= S.n) return;
    lv_anim_delete(S.sel, glideExec);
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var         (&a, S.sel);
    lv_anim_set_values      (&a, lv_obj_get_x(S.sel), selX(S, i));
    lv_anim_set_duration    (&a, GLIDE_MS);
    lv_anim_set_path_cb     (&a, lv_anim_path_ease_out);
    lv_anim_set_exec_cb     (&a, glideExec);
    lv_anim_set_completed_cb(&a, glideDone);
    lv_anim_start(&a);
}

// A tap on a choice.
static void tapCb(lv_event_t *ev) {
    Selector *S = selOf((lv_obj_t *)lv_event_get_current_target(ev));
    if (!S || (S->blocked && S->blocked())) return;
    glideTo(*S, (uint8_t)(uintptr_t)lv_event_get_user_data(ev));
}

// The finger on the selector: it follows along the ribbon, clamped to the
// first and last choice, and snaps on release.
static void dragCb(lv_event_t *ev) {
    Selector *S = selOf((lv_obj_t *)lv_event_get_current_target(ev));
    if (!S) return;
    const lv_event_code_t code = lv_event_get_code(ev);
    lv_indev_t *in = lv_indev_active();
    lv_point_t pt = {0, 0};
    if (in) lv_indev_get_point(in, &pt);
    if (code == LV_EVENT_PRESSED) {
        lv_anim_delete(S->sel, glideExec);
        S->pressX   = pt.x;
        S->selX0    = lv_obj_get_x(S->sel);
        S->dragging = false;
    } else if (code == LV_EVENT_PRESSING) {
        const int32_t dx = pt.x - S->pressX;
        if (!S->dragging && LV_ABS(dx) < UI::mm(UI::shape().SLOP_SELECTOR)) return;
        if (!S->dragging && S->blocked && S->blocked()) { if (in) lv_indev_wait_release(in); return; }
        S->dragging = true;
        place(*S, LV_CLAMP(selX(*S, 0), S->selX0 + dx, selX(*S, S->n - 1)));
    } else if (code == LV_EVENT_RELEASED || code == LV_EVENT_PRESS_LOST) {
        if (!S->dragging) return;
        S->dragging = false;
        glideTo(*S, nearest(*S, lv_obj_get_x(S->sel)));
    }
}

void selector(Selector &S, lv_obj_t *parent, const SelectorSpec &spec) {
    const UIType &t = UI::type();
    const SelectorColours col = selectorColours();
    const bool q = spec.quiet;
    const bool compact = spec.compact;
    const int32_t g = UI::mm(UI::shape().S1);

    S.obj = S.sel = S.selIcon = S.action = nullptr;
    S.n = 0;
    for (uint8_t i = 0; i < spec.n && i < SELECTOR_MAX; i++) { S.glyph[S.n] = spec.glyph[i]; S.item[S.n++] = nullptr; }
    S.shown = 0; S.dragging = false;
    S.blocked  = spec.blocked;
    S.onChosen = spec.onChosen;

    // Its size. Full: a touch target per choice, the action before them.
    // COMPACT (#92): a span chip's height; each choice as wide as the widest
    // word and its sides (S7, as a chip's), the sides shrinking to fit `room`,
    // as far as S1; no action.
    const bool hasAction = !compact && spec.actionGlyph;
    int32_t H, slotW;
    if (compact) {
        H = UI::mm(UI::shape().H_CHIP);
        int32_t widest = 0;
        for (uint8_t i = 0; i < S.n; i++) widest = LV_MAX(widest, textW(S.glyph[i], t.TAG));
        int32_t side = UI::mm(UI::shape().S7);
        if (spec.room > 0 && S.n) {
            const int32_t fit = ((spec.room - (S.n + 1) * g) / S.n - widest) / 2;
            side = LV_CLAMP(g, fit, side);
        }
        slotW = widest + 2 * side;
    } else {
        H = selectorH();
        slotW = UI::minTouch();
    }
    const int32_t slotH   = H - 2 * g;
    const int32_t actionW = hasAction ? slotW + 2 * g : 0;
    const int32_t brk     = hasAction ? selectorBreak() : 0;
    const int32_t W = actionW + brk + (S.n ? S.n * slotW + (S.n + 1) * g : 0);

    S.obj = plain(parent);
    lv_obj_set_size(S.obj, W, H);

    // The ribbon: three quarters of a choice tall, centred - IN TWO PIECES
    // (owner: no divider line - the selector never goes to the action anyway).
    // The action has a short ribbon of its own; a break; then the choices'.
    const int32_t rh = slotH * 72 / 100;
    auto ribbon = [&](int32_t x0, int32_t w) {
        lv_obj_t *rib = plain(S.obj);
        lv_obj_set_pos (rib, x0, (H - rh) / 2);
        lv_obj_set_size(rib, w, rh);
        lv_obj_set_style_radius  (rib, selectorRadius(), 0);
        lv_obj_set_style_bg_color(rib, UI::c(quietHex(q, col.ribbon)), 0);
        lv_obj_set_style_bg_opa  (rib, LV_OPA_COVER, 0);
    };
    if (hasAction) ribbon(0, actionW);
    if (S.n) ribbon(actionW + brk, W - actionW - brk);

    // A choice: an icon, or a word in the label face.
    auto iconAt = [&](int32_t x, const char *glyph, lv_event_cb_t cb, void *user) {
        lv_obj_t *b = plain(S.obj);
        lv_obj_set_pos (b, x, 0);
        lv_obj_set_size(b, slotW, H);
        lv_obj_add_flag(b, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_set_user_data(b, &S);
        lv_obj_add_event_cb(b, cb, LV_EVENT_CLICKED, user);
        lv_obj_t *l = label(b, compact ? t.TAG : t.ICON_MD, quietHex(q, col.icon));
        lv_label_set_text(l, glyph);
        lv_obj_center(l);
        return b;
    };

    if (hasAction) S.action = iconAt(g, spec.actionGlyph, spec.actionCb, nullptr);
    int32_t x = actionW + brk + g;
    const uint8_t chosen = spec.chosen < S.n ? spec.chosen : 0;
    for (uint8_t i = 0; i < S.n; i++) {
        S.item[i] = iconAt(x, S.glyph[i], tapCb, (void *)(uintptr_t)i);
        S.x[i] = x + slotW / 2;
        x += slotW + g;
    }
    if (!S.n) return;

    // The selector: the whole height, a little wider than a choice, over the
    // ribbon - with the chosen choice's icon, a size up, in the accent.
    S.selW = slotW + 2 * g;
    S.sel = plain(S.obj);
    lv_obj_set_user_data(S.sel, &S);
    lv_obj_set_size(S.sel, S.selW, H);
    lv_obj_set_style_radius(S.sel, selectorRadius(), 0);
    // Its face is metal, curved and brushed (KitFaces.cpp). Greyed when quiet,
    // as everything else is.
    const int32_t selR = (selectorLookNow() & 1) ? LV_MIN(S.selW, H) / 2 : UI::mm(UI::shape().R_CONTROL);
    if (lv_obj_t *img = setFace(S.sel, S.selW, H, selR, metalKind(), quietHex(q, UI::pal().GROUND))) {
        if (q) {
            lv_obj_set_style_image_recolor    (img, UI::c(UI::pal().SURFACE_ALT), 0);
            lv_obj_set_style_image_recolor_opa(img, (lv_opa_t)166, 0);
        }
    }
    lv_obj_add_flag  (S.sel, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(S.sel, LV_OBJ_FLAG_GESTURE_BUBBLE);
    lv_obj_clear_flag(S.sel, LV_OBJ_FLAG_SCROLL_CHAIN);
    lv_obj_add_event_cb(S.sel, dragCb, LV_EVENT_PRESSED,    nullptr);
    lv_obj_add_event_cb(S.sel, dragCb, LV_EVENT_PRESSING,   nullptr);
    lv_obj_add_event_cb(S.sel, dragCb, LV_EVENT_RELEASED,   nullptr);
    lv_obj_add_event_cb(S.sel, dragCb, LV_EVENT_PRESS_LOST, nullptr);
    // Compact: the chosen word in the name face, a size up from the ribbon's.
    S.selIcon = label(S.sel, compact ? t.NAME : t.ICON, quietHex(q, col.chosen));
    lv_obj_center(S.selIcon);
    // LARGER, TO FILL MORE OF THE SELECTOR (owner, round 7). ICON is the
    // largest icon face there is, so it is scaled up 1.4x about its centre -
    // drawn through a layer, the size of the icon. If that is visibly soft on
    // glass, the answer is a face of just these few glyphs at this size (a few
    // KB), from scripts/gen_icon_font.py.
    if (!compact) {
        lv_obj_set_style_transform_scale  (S.selIcon, 358, 0);
        lv_obj_set_style_transform_pivot_x(S.selIcon, lv_pct(50), 0);
        lv_obj_set_style_transform_pivot_y(S.selIcon, lv_pct(50), 0);
    }
    S.shown = chosen;
    lv_label_set_text(S.selIcon, S.glyph[chosen]);
    lv_obj_set_pos(S.sel, selX(S, chosen), 0);
}

} // namespace Kit
