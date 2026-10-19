// A slot's drawer (2.8): the panel that slides down from a header slot. Its
// shape and motion are the system panel's (Panel_System, the status slot's own
// drawer), which it was built to match: square where it meets the bar, rounded
// below, 350 ms ease-out, its contents riding the growing edge (#81) - so the
// four slots' panels read as one family.
#include "UI/Kit.h"
#include "UI/UIToolkit.h"

namespace Kit {

namespace {
constexpr uint32_t SLIDE_MS = 350;   // the system panel's

void slide(void *var, int32_t v) {
    Drawer *d = (Drawer *)var;
    if (!d || !d->root) return;
    lv_obj_set_height(d->root, v);
    // The face rides the edge: its bottom stays on the wrapper's bottom, so it
    // slides down rather than being uncovered (the owner's "window blinds").
    lv_obj_set_y(d->face, v - d->openH);
    // Hidden at zero, not merely zero-high: a drawer casts the scheme's shadow,
    // and a zero-high one would leave a line of it under the bar.
    if (v <= 0) lv_obj_add_flag  (d->root, LV_OBJ_FLAG_HIDDEN);
    else        lv_obj_clear_flag(d->root, LV_OBJ_FLAG_HIDDEN);
}

// Closed all the way: tell the caller (its tab may close now).
void closedCb(lv_anim_t *a) {
    Drawer *d = (Drawer *)a->var;
    if (d && !d->open && d->onClosed) d->onClosed(d->closedUser);
}

void animate(Drawer &d, int32_t to) {
    lv_anim_delete(&d, slide);   // a close cut short says nothing (no completed_cb on delete)
    lv_anim_t a;
    lv_anim_init        (&a);
    lv_anim_set_var     (&a, &d);
    lv_anim_set_values  (&a, lv_obj_get_height(d.root), to);
    lv_anim_set_duration(&a, SLIDE_MS);
    lv_anim_set_exec_cb (&a, slide);
    lv_anim_set_path_cb (&a, lv_anim_path_ease_out);
    if (to == 0) lv_anim_set_completed_cb(&a, closedCb);
    lv_anim_start       (&a);
}

// Every drawer made (the system panel's and the slots'), for the bar's shadow.
constexpr uint8_t DRAWERS_MAX = 4;
Drawer *s_drawers[DRAWERS_MAX] = {};
uint8_t s_drawerN = 0;
}  // namespace

// Over the bar's accent line, from x1 to x2: the bar's own face, at `opa`, or
// fading from clear at x1 to full at x2 (`in`) or the other way (`out`).
enum class Fade : uint8_t { FADE_NONE, FADE_IN, FADE_OUT };
static void coverLine(lv_layer_t *layer, lv_color_t face, int32_t x1, int32_t x2, int32_t y1, int32_t y2,
                      Fade f) {
    if (x2 < x1) return;
    lv_draw_rect_dsc_t dsc;
    lv_draw_rect_dsc_init(&dsc);
    dsc.bg_color = face;
    dsc.bg_opa   = LV_OPA_COVER;
    if (f != Fade::FADE_NONE) {
        dsc.bg_grad.dir         = LV_GRAD_DIR_HOR;
        dsc.bg_grad.stops_count = 2;
        dsc.bg_grad.stops[0].color = face;
        dsc.bg_grad.stops[1].color = face;
        dsc.bg_grad.stops[0].opa   = f == Fade::FADE_IN ? LV_OPA_TRANSP : LV_OPA_COVER;
        dsc.bg_grad.stops[1].opa   = f == Fade::FADE_IN ? LV_OPA_COVER : LV_OPA_TRANSP;
        dsc.bg_grad.stops[0].frac  = 0;
        dsc.bg_grad.stops[1].frac  = 255;
    }
    const lv_area_t a = { x1, y1, x2, y2 };
    lv_draw_rect(layer, &dsc, &a);
}

// WHAT THE BAR DRAWS OVER ITSELF FOR AN OPEN PANEL - in its DRAW_MAIN_END, right
// after its own face, line and shadow and before its slots, and the last event
// whose clip reaches past the bar (LVGL 9.5 clips DRAW_POST to an object's own
// area). The bar is drawn over the panels.
//
// 1. THE BAR'S SHADOW, NOT ON AN OPEN PANEL (owner, 2.8 round 6: on Linen "the
//    drop shadow line under the header stays in view where the slot opens up").
//    Each open panel's face is painted back over it, inside its walls: the
//    face's top padding is deeper than the shadow reaches, so only plain face is
//    ever covered. Short of its bottom corners, so a panel still opening keeps
//    its edge. Nothing on a scheme without a shadow.
// 2. THE PANEL LIES ON THE BAR (D-35, the owner's "Panel Edges" choice): its top
//    edge is the bar's line and its top corners turn down from it, so the line
//    outside the panel stops a quarter of a corner short of its walls, fading
//    out over LINE_FADE before that - "so that the header bar accent lines
//    outside the panel don't actually contact the panel".
// 3. THE HALO, where it falls on the bar: round the panel's top, and round its
//    slot's tab (the tab's own above the line only, so the panel's top edge
//    beside the tab stays lit). Below the bar the panel's wrapper casts its own.
void drawerOverBar(lv_event_t *e) {
    lv_obj_t *bar = lv_event_get_current_target_obj(e);
    lv_area_t b;
    lv_obj_get_coords(bar, &b);
    lv_layer_t *layer = lv_event_get_layer(e);
    const UIHeader &h = UI::hdr();

    bool anyOpen = false;
    for (uint8_t i = 0; i < s_drawerN; i++) {
        const Drawer &d = *s_drawers[i];
        if (!d.root || !d.face || lv_obj_has_flag(d.root, LV_OBJ_FLAG_HIDDEN)) continue;
        anyOpen = true;
        lv_area_t r;
        lv_obj_get_coords(d.root, &r);
        const int32_t line = UI::sc(h.LINE), R = UI::sc(h.DRAWER_R);
        const int32_t fade = UI::mm(UI::shape().LINE_FADE), gap = R / 4;
        const lv_color_t face = lv_obj_get_style_bg_color(bar, LV_PART_MAIN);
        const int32_t y1 = b.y2 - line + 1, y2 = b.y2;
        coverLine(layer, face, r.x1 - gap - fade, r.x1 - gap - 1, y1, y2, Fade::FADE_IN);
        coverLine(layer, face, r.x1 - gap, r.x1 + R / 2, y1, y2, Fade::FADE_NONE);
        coverLine(layer, face, r.x2 - R / 2, r.x2 + gap, y1, y2, Fade::FADE_NONE);
        coverLine(layer, face, r.x2 + gap + 1, r.x2 + gap + fade, y1, y2, Fade::FADE_OUT);
        haloDraw(layer, r, R, &b);
    }
    if (anyOpen) {
        // The open slot's tab: the bar's child in LV_STATE_CHECKED.
        const uint32_t n = lv_obj_get_child_count(bar);
        for (uint32_t i = 0; i < n; i++) {
            lv_obj_t *c = lv_obj_get_child(bar, (int32_t)i);
            if (!lv_obj_has_state(c, LV_STATE_CHECKED)) continue;
            lv_area_t t, above = b;
            lv_obj_get_coords(c, &t);
            above.y2 = b.y2 - UI::sc(h.LINE);
            haloDraw(layer, t, UI::sc(h.FRAME_R), &above);
        }
    }

    const int32_t sw = lv_obj_get_style_shadow_width(bar, LV_PART_MAIN);
    if (sw <= 0 || lv_obj_get_style_shadow_opa(bar, LV_PART_MAIN) == LV_OPA_TRANSP) return;
    const int32_t reach = sw / 2 + lv_obj_get_style_shadow_offset_y(bar, LV_PART_MAIN) +
                          lv_obj_get_style_shadow_spread(bar, LV_PART_MAIN) + 1;
    for (uint8_t i = 0; i < s_drawerN; i++) {
        const Drawer &d = *s_drawers[i];
        if (!d.root || !d.face || lv_obj_has_flag(d.root, LV_OBJ_FLAG_HIDDEN)) continue;
        lv_area_t r;
        lv_obj_get_coords(d.root, &r);
        // The face's own inside, its top corners turning down from the line
        // (D-35): a rounded rectangle inside its walls, drawn below the bar only.
        // Its edge is the line's width (KS_DRAWER_FACE); read back from the face
        // it came out 0 here, and the cover hid the top of the edge's curve.
        const int32_t R    = UI::sc(UI::hdr().DRAWER_R);
        const int32_t wall = UI::sc(UI::hdr().LINE);
        // Drawn whole (LVGL clamps a radius to half the height) and clipped to the
        // shadow's reach below the bar.
        const lv_area_t a  = { r.x1 + wall, r.y1 + wall, r.x2 - wall, r.y2 - wall };
        if (a.x1 > a.x2 || a.y1 > a.y2) continue;
        const lv_area_t keep = layer->_clip_area;
        lv_area_t c = keep;
        c.y1 = LV_MAX(c.y1, b.y2 + 1);
        c.y2 = LV_MIN(c.y2, LV_MIN(b.y2 + reach, r.y2 - R));
        if (c.y1 > c.y2) continue;
        layer->_clip_area = c;
        lv_draw_rect_dsc_t dsc;
        lv_draw_rect_dsc_init(&dsc);
        dsc.bg_color = lv_obj_get_style_bg_color(d.face, LV_PART_MAIN);
        dsc.bg_opa   = LV_OPA_COVER;
        dsc.radius   = LV_MAX(0, R - wall);
        lv_draw_rect(layer, &dsc, &a);
        layer->_clip_area = keep;
    }
}

// The system panel's width rule (Panel_System::init: half a wide screen, three
// quarters of a middling one, all of a narrow one), capped at DRAWER_W: a
// slot's panel holds a few lines, not a grid of buttons.
int32_t drawerWidth() {
    const int32_t screenW  = lv_obj_get_width(lv_screen_active());
    const int32_t logicalW = (int32_t)((float)screenW / UIToolkit::scale());
    int32_t w = logicalW >= 700 ? screenW / 2 : logicalW >= 400 ? (screenW * 3) / 4 : screenW;
    const int32_t cap = UI::sc(UI::hdr().DRAWER_W);
    return w > cap ? cap : w;
}

// ONE SHAPE (D-23). A cove is a square of the shape's fill holding a ring of the
// accent - a circle whose inside is the bar outside the shape - clipped to the
// square, so a quarter of it shows: the tab's side turning out into the line,
// rounded. Its radius is JOIN_R and the line's width; R0 is both.
static int32_t joinR0() { return UI::sc(UI::hdr().JOIN_R) + UI::sc(UI::hdr().LINE); }

lv_obj_t *joinCorner(lv_obj_t *parent) {
    const int32_t r0 = joinR0();
    lv_obj_t *sq = plain(parent);
    add(sq, KitStyle::KS_JOIN_FILL);
    lv_obj_set_size(sq, r0, r0);
    lv_obj_t *ring = plain(sq);
    add(ring, KitStyle::KS_JOIN_RING_BAR);
    lv_obj_set_size(ring, 2 * r0, 2 * r0);
    lv_obj_add_flag(sq, LV_OBJ_FLAG_HIDDEN);
    return sq;
}

// Beside a tab, in the bar: the tab's side curving out into the line.
void coveAt(lv_obj_t *j, int32_t edgeX, bool right, int32_t barH) {
    if (!j) return;
    const int32_t r0 = joinR0(), b = UI::sc(UI::hdr().LINE);
    lv_obj_set_pos(j, right ? edgeX - b : edgeX + b - r0, barH - r0);
    lv_obj_set_pos(lv_obj_get_child(j, 0), right ? 0 : -r0, -r0);
}

void drawer(Drawer &d, lv_obj_t *parent, int32_t w) {
    bool known = false;
    for (uint8_t i = 0; i < s_drawerN; i++) known |= s_drawers[i] == &d;
    if (!known && s_drawerN < DRAWERS_MAX) s_drawers[s_drawerN++] = &d;
    d.w = w;
    d.root = plain(parent);
    add(d.root, KitStyle::KS_DRAWER);
    // D-35: it floats over the page on the halo, which takes the place of the
    // scheme's lift (the shadow it cast on Linen).
    haloAttach(d.root);
    lv_obj_set_size(d.root, w, 0);
    // A tap on the drawer is the drawer's: without this it would fall through
    // to the card under it.
    lv_obj_add_flag(d.root, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(d.root, LV_OBJ_FLAG_HIDDEN);

    d.face = plain(d.root);
    add(d.face, KitStyle::KS_DRAWER_FACE);
    lv_obj_set_size(d.face, lv_pct(100), LV_SIZE_CONTENT);
    lv_obj_add_flag(d.face, LV_OBJ_FLAG_CLICKABLE);
    // Every corner of it shows: it lies on the bar (D-35), its top corners
    // turning down from the line - no longer pushed above the wrapper's clip.
    lv_obj_set_y(d.face, 0);
    d.open = false;
    d.openH = 0;
}

void drawerPlace(Drawer &d, int32_t centreX, int32_t top) {
    if (!d.root) return;
    const int32_t screenW = lv_obj_get_width(lv_screen_active());
    // EDGE from the screen's sides: where the outermost slot's tab stands, so a
    // panel pushed to an edge shares that side with its tab - one straight line.
    const int32_t margin  = d.w >= screenW ? 0 : UI::sc(UI::hdr().EDGE);
    int32_t x = centreX - d.w / 2;
    if (x + d.w > screenW - margin) x = screenW - margin - d.w;
    if (x < margin) x = margin;
    // ON the bar's line (D-35): its top edge is the line, under the bar, which
    // is drawn over it - the corners turn down from the line's foot.
    lv_obj_set_pos(d.root, x, top - UI::sc(UI::hdr().LINE));
    d.x = x;
    d.top = top;
}

void drawerOpen(Drawer &d) {
    if (!d.root) return;
    // Its side walls rise straight to the bar's line and stop there (owner,
    // round 4: the flares that curved the line down into them are gone).
    // Measured, never guessed (the system panel's #50 lesson): the face is
    // content-sized, so it has a true height while the wrapper is closed.
    lv_obj_clear_flag(d.root, LV_OBJ_FLAG_HIDDEN);
    lv_obj_update_layout(d.face);
    int32_t h = lv_obj_get_height(d.face);
    const int32_t avail = lv_obj_get_height(lv_screen_active()) - d.top - UI::sc(UI::hdr().DRAWER_PAD);
    if (h > avail) h = avail;
    d.openH = h;
    d.open = true;
    lv_obj_move_foreground(d.root);
    animate(d, h);
}

void drawerClose(Drawer &d, bool anim) {
    if (!d.root) return;
    d.open = false;
    if (anim) { animate(d, 0); return; }
    lv_anim_delete(&d, slide);
    slide(&d, 0);
    if (d.onClosed) d.onClosed(d.closedUser);
}

lv_obj_t *drawerItem(lv_obj_t *face, const char *words, bool marked, lv_event_cb_t cb, void *user) {
    lv_obj_t *it = item(face);   // the card's surface, tinted while pressed
    add(it, KitStyle::KS_DRAWER_ITEM);
    add(plain(it), marked ? KitStyle::KS_HDR_DOT_ON : KitStyle::KS_HDR_DOT_OFF);
    lv_label_set_text(label(it, KitStyle::KS_LABEL_NAME), words ? words : "");
    if (cb) lv_obj_add_event_cb(it, cb, LV_EVENT_CLICKED, user);
    return it;
}

lv_obj_t *drawerNote(lv_obj_t *face, const char *words) {
    lv_obj_t *l = label(face, KitStyle::KS_LABEL_TAG_DIM);
    lv_obj_set_width(l, lv_pct(100));
    lv_label_set_long_mode(l, LV_LABEL_LONG_WRAP);
    lv_label_set_text(l, words ? words : "");
    return l;
}

lv_obj_t *drawerBig(lv_obj_t *face, const char *words) {
    lv_obj_t *l = label(face, KitStyle::KS_DRAWER_BIG);
    lv_label_set_text(l, words ? words : "");
    return l;
}

lv_obj_t *drawerRow(lv_obj_t *face) {
    lv_obj_t *r = plain(face);
    lv_obj_set_size(r, lv_pct(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(r, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(r, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(r, UI::sc(UI::hdr().DRAWER_GAP), 0);
    UI::unclipShadows(r);
    return r;
}

lv_obj_t *statusRow(lv_obj_t *face, const char *what, lv_obj_t **glyphRoom) {
    lv_obj_t *r = drawerRow(face);
    lv_obj_set_style_min_height(r, UI::sc(UI::hdr().FRAME), 0);
    lv_obj_t *g = plain(r);
    lv_obj_set_size(g, UI::sc(UI::hdr().FRAME), UI::sc(UI::hdr().FRAME));
    lv_obj_set_flex_flow(g, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(g, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    if (glyphRoom) *glyphRoom = g;
    lv_obj_t *w = label(r, KitStyle::KS_LABEL_NAME);
    lv_label_set_text(w, what ? what : "");
    // The value a face down (SSID, dBm and IP fit the P4_5's panel), on ONE
    // line: a DOT label with a content height wraps instead of ellipsising.
    lv_obj_t *v = label(r, KitStyle::KS_LABEL_TAG_DIM);
    lv_obj_set_flex_grow(v, 1);
    lv_obj_set_height(v, lv_font_get_line_height(UI::type().TAG));
    lv_label_set_long_mode(v, LV_LABEL_LONG_DOT);
    lv_obj_set_style_text_align(v, LV_TEXT_ALIGN_RIGHT, 0);
    lv_label_set_text(v, "-");
    return v;
}

lv_obj_t *vrule(lv_obj_t *row, int32_t h) {
    lv_obj_t *v = plain(row);
    lv_obj_set_size(v, UI::sc(UI::hdr().LINE), h);
    add(v, KitStyle::KS_RULE);
    return v;
}

lv_obj_t *drawerTitle(lv_obj_t *face, const char *words) {
    lv_obj_t *l = label(face, KitStyle::KS_PANEL_TITLE);
    lv_label_set_text(l, words ? words : "");
    return l;
}

lv_obj_t *toggleButton(lv_obj_t *parent, const char *words, int32_t w, int32_t h, bool on, lv_event_cb_t cb) {
    lv_obj_t *b = plain(parent);
    lv_obj_set_size(b, w, h);
    lv_obj_add_flag(b, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_t *l = label(b, KitStyle::KS_LABEL_NAME);
    lv_label_set_text(l, words ? words : "");
    lv_obj_center(l);
    if (cb) lv_obj_add_event_cb(b, cb, LV_EVENT_CLICKED, nullptr);
    toggleButtonSet(b, on);
    return b;
}

// Glass while off, lit glass while on (owner, round 5) - as Power.
void toggleButtonSet(lv_obj_t *b, bool on) {
    if (!b) return;
    // Its size as set (its coordinates are not laid out yet when it is built).
    finish(b, lv_obj_get_style_width(b, LV_PART_MAIN), lv_obj_get_style_height(b, LV_PART_MAIN),
           UI::mm(UI::shape().R_CONTROL), on ? Finish::FINISH_LIT : Finish::FINISH_GLASS);
    if (lv_obj_t *l = labelIn(b)) lv_obj_center(l);
}

} // namespace Kit
