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

lv_obj_t *s_bar = nullptr;   // the header bar the panels lie on (drawerOverBar() learns it)

void slide(void *var, int32_t v) {
    Drawer *d = (Drawer *)var;
    if (!d || !d->root) return;
    // The wrapper is the panel: its top edge stays, its foot moves (round 5).
    const int32_t was = lv_obj_get_style_height(d->root, LV_PART_MAIN);   // as last set
    lv_obj_set_height(d->root, v);
    // The face rides the foot: its bottom stays on the wrapper's bottom, so it
    // slides down rather than being uncovered (the owner's "window blinds"),
    // and back up under the panel's top edge as it closes.
    lv_obj_set_y(d->face, v - d->openH);
    // Hidden at zero, not merely zero-high: a zero-high panel would leave its
    // edge as a line under the bar.
    if (v <= 0) {
        lv_obj_add_flag(d->root, LV_OBJ_FLAG_HIDDEN);
        // Closing only (an opening slide's first step is 0 too): its shading
        // on the bar goes with it.
        if (!d->open && s_bar) lv_obj_invalidate(s_bar);
    } else {
        lv_obj_clear_flag(d->root, LV_OBJ_FLAG_HIDDEN);
        // Its first frame with a height: the bar draws its shading now. Drawn
        // when the panel was asked to open, it had none yet, and nothing else
        // redraws that part of the bar - the owner saw it only in screenshots,
        // which draw everything afresh (round 6).
        if (was <= 0 && s_bar) lv_obj_invalidate(s_bar);
    }
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

// The coves made (joinCorner()), for the shape's shading on the bar.
constexpr uint8_t JOINS_MAX = 4;
lv_obj_t *s_joins[JOINS_MAX] = {};
uint8_t   s_joinN = 0;

// THE BAR'S DROP SHADOW STAYS WHILE A PANEL SHOWS (round 6, the owner: it
// disappeared). Round 4 took it away because it fell over the panel's halo at
// its top corners; a header panel has no halo below the bar since round 5.
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
// 3. THE SHADING WHERE THE PANEL MEETS THE BAR - in the bar's DRAW_POST, after
//    its slots (round 4): the halo of one shape, the panel's top, its slot's tab
//    and the two coves together (Kit::haloOverBar()), so nothing is darkened
//    twice and the coves' insides are shaded as the bar beside them. It falls on
//    the slots' words beside the panel too, as a shadow would. The ONLY halo a
//    header panel has (owner, round 5: below the bar it made the slide
//    "dreadfully chunky" at any strength, and the panel's accent edge needs no
//    more than the shadow where it meets the line).
//
// The pane's top edge is drawn over the line again in its own colours (round 4):
// the line's cover used to cut the top of its corners' arcs.
static void haloOverBarAll(lv_obj_t *bar, lv_layer_t *layer, const lv_area_t &b) {
    const UIHeader &h = UI::hdr();
    for (uint8_t i = 0; i < s_drawerN; i++) {
        const Drawer &d = *s_drawers[i];
        if (!d.root || !d.face || lv_obj_has_flag(d.root, LV_OBJ_FLAG_HIDDEN)) continue;
        HaloShape s;
        memset(&s, 0, sizeof(s));   // compared byte for byte to find its painted mask
        lv_obj_get_coords(d.root, &s.pane);
        s.paneR = UI::sc(h.DRAWER_R);
        // Over the bar only the pane's top matters, so its foot is set where the
        // shading cannot reach: the mask stays the same while the panel slides,
        // where a true foot made it repaint on every frame of the slide.
        s.pane.y2 = b.y2 + haloWidth() + s.paneR + 1;
        // The open slot's tab: the bar's child in LV_STATE_CHECKED, over this panel.
        const uint32_t n = lv_obj_get_child_count(bar);
        for (uint32_t k = 0; k < n && !s.hasTab; k++) {
            lv_obj_t *c = lv_obj_get_child(bar, (int32_t)k);
            if (!lv_obj_has_state(c, LV_STATE_CHECKED)) continue;
            lv_area_t t;
            lv_obj_get_coords(c, &t);
            if (t.x2 < s.pane.x1 || t.x1 > s.pane.x2) continue;
            s.tab = t;
            s.tabR = UI::sc(h.FRAME_R);
            s.hasTab = true;
        }
        // Its coves: each a square with a disc of the bar's colour in it.
        s.coveR = UI::sc(h.JOIN_R);
        for (uint8_t k = 0; k < s_joinN && s.nCove < 2; k++) {
            lv_obj_t *sq = s_joins[k];
            if (!sq || lv_obj_get_parent(sq) != bar || lv_obj_has_flag(sq, LV_OBJ_FLAG_HIDDEN)) continue;
            lv_area_t q, ring;
            lv_obj_get_coords(sq, &q);
            lv_obj_get_coords(lv_obj_get_child(sq, 0), &ring);
            s.coveSq[s.nCove] = q;
            s.coveC[s.nCove]  = { (ring.x1 + ring.x2 + 1) / 2, (ring.y1 + ring.y2 + 1) / 2 };
            s.nCove++;
        }
        haloOverBar(layer, s, b);
    }
}

// A PANEL FLUSH WITH ITS TAB (2.11 round 4): pushed to the screen's edge, a panel
// shares that side with its slot's tab - one straight line - so its top corner
// on that side is square, the tab's side running straight down into the
// panel's (the owner's close-up of the system panel: a notch there). In the
// wrapper's DRAW_POST, after its edge (round 5: the wrapper is the panel and
// draws its edge last): the corner is inside its area.
static void frameSquareCorner(lv_event_t *e) {
    lv_obj_t *face = lv_event_get_current_target_obj(e);
    if (!s_bar) return;
    const UIHeader &h = UI::hdr();
    const int32_t line = UI::sc(h.LINE), R = UI::sc(h.DRAWER_R);
    lv_area_t f;
    lv_obj_get_coords(face, &f);
    if (lv_area_get_height(&f) < 2 * R) return;   // the slide's last frames: a pill, no corner
    const uint32_t n = lv_obj_get_child_count(s_bar);
    for (uint32_t k = 0; k < n; k++) {
        lv_obj_t *c = lv_obj_get_child(s_bar, (int32_t)k);
        if (!lv_obj_has_state(c, LV_STATE_CHECKED)) continue;
        lv_area_t t;
        lv_obj_get_coords(c, &t);
        if (t.x2 < f.x1 || t.x1 > f.x2) continue;
        lv_layer_t *layer = lv_event_get_layer(e);
        for (int side = 0; side < 2; side++) {
            const bool right = side == 1;
            if (LV_ABS(right ? t.x2 - f.x2 : t.x1 - f.x1) > line) continue;
            const lv_area_t a = { right ? f.x2 - R : f.x1, f.y1, right ? f.x2 : f.x1 + R, f.y1 + R };
            lv_draw_rect_dsc_t dsc;
            lv_draw_rect_dsc_init(&dsc);
            dsc.bg_color     = lv_obj_get_style_bg_color(face, LV_PART_MAIN);
            dsc.bg_opa       = LV_OPA_COVER;
            dsc.border_color = lv_obj_get_style_border_color(face, LV_PART_MAIN);
            dsc.border_width = line;
            dsc.border_opa   = LV_OPA_COVER;
            dsc.border_side  = right ? LV_BORDER_SIDE_RIGHT : LV_BORDER_SIDE_LEFT;
            lv_draw_rect(layer, &dsc, &a);
        }
        return;
    }
}

void drawerOverBar(lv_event_t *e) {
    lv_obj_t *bar = lv_event_get_current_target_obj(e);
    lv_area_t b;
    lv_obj_get_coords(bar, &b);
    lv_layer_t *layer = lv_event_get_layer(e);
    const UIHeader &h = UI::hdr();
    s_bar = bar;
    if (lv_event_get_code(e) == LV_EVENT_DRAW_POST) { haloOverBarAll(bar, layer, b); return; }

    for (uint8_t i = 0; i < s_drawerN; i++) {
        const Drawer &d = *s_drawers[i];
        if (!d.root || !d.face || lv_obj_has_flag(d.root, LV_OBJ_FLAG_HIDDEN)) continue;
        lv_area_t r;
        lv_obj_get_coords(d.root, &r);
        const int32_t line = UI::sc(h.LINE), R = UI::sc(h.DRAWER_R);
        const int32_t fade = UI::mm(UI::shape().LINE_FADE), gap = R / 4;
        const lv_color_t face = lv_obj_get_style_bg_color(bar, LV_PART_MAIN);
        const int32_t y1 = b.y2 - line + 1, y2 = b.y2;
        coverLine(layer, face, r.x1 - gap - fade, r.x1 - gap - 1, y1, y2, Fade::FADE_IN);
        coverLine(layer, face, r.x1 - gap, r.x2 + gap, y1, y2, Fade::FADE_NONE);
        coverLine(layer, face, r.x2 + gap + 1, r.x2 + gap + fade, y1, y2, Fade::FADE_OUT);
        // The pane's top edge over the line: its face and border, corners and
        // all, within the line's rows (the open tab covers its share).
        const lv_area_t keep = layer->_clip_area;
        lv_area_t c = keep;
        c.y1 = LV_MAX(c.y1, y1);
        c.y2 = LV_MIN(c.y2, y2);
        if (c.y1 <= c.y2) {
            layer->_clip_area = c;
            lv_draw_rect_dsc_t dsc;
            lv_draw_rect_dsc_init(&dsc);
            dsc.bg_color     = lv_obj_get_style_bg_color(d.root, LV_PART_MAIN);
            dsc.bg_opa       = LV_OPA_COVER;
            dsc.radius       = R;
            dsc.border_color = lv_obj_get_style_border_color(d.root, LV_PART_MAIN);
            dsc.border_width = line;
            dsc.border_opa   = LV_OPA_COVER;
            lv_draw_rect(layer, &dsc, &r);
            layer->_clip_area = keep;
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
        // The panel's own inside, its top corners turning down from the line
        // (D-35): a rounded rectangle inside its walls, drawn below the bar only.
        // Its edge is the line's width (KS_DRAWER); read back from the style it
        // once came out 0 here, and the cover hid the top of the edge's curve.
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
        dsc.bg_color = lv_obj_get_style_bg_color(d.root, LV_PART_MAIN);
        dsc.bg_opa   = LV_OPA_COVER;
        dsc.radius   = LV_MAX(0, R - wall);
        lv_draw_rect(layer, &dsc, &a);
        // A CORNER UNDER THE TAB IS SQUARE (2.11 round 11, the owner on Linen: the
        // status panel at the screen's edge showed the bar's shadow in its top
        // corner): where the open slot's tab runs straight down into the wall,
        // the panel has no rounded corner there, so neither does the cover.
        const uint32_t nb = lv_obj_get_child_count(bar);
        for (uint32_t k = 0; k < nb; k++) {
            lv_obj_t *c2 = lv_obj_get_child(bar, (int32_t)k);
            if (!lv_obj_has_state(c2, LV_STATE_CHECKED)) continue;
            lv_area_t t;
            lv_obj_get_coords(c2, &t);
            if (t.x2 < r.x1 || t.x1 > r.x2) continue;
            dsc.radius = 0;
            const lv_area_t right = { a.x2 - R, a.y1, a.x2, a.y1 + R };
            const lv_area_t left  = { a.x1, a.y1, a.x1 + R, a.y1 + R };
            if (t.x2 >= r.x2 - wall - R) lv_draw_rect(layer, &dsc, &right);
            if (t.x1 <= r.x1 + wall + R) lv_draw_rect(layer, &dsc, &left);
            break;
        }
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
    if (s_joinN < JOINS_MAX) s_joins[s_joinN++] = sq;   // for the shape's halo
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
    // No halo below the bar (owner, round 5): the shading where it meets the
    // bar is the bar's (drawerOverBar()), and the accent edge does the rest.
    lv_obj_add_event_cb(d.root, frameSquareCorner, LV_EVENT_DRAW_POST, nullptr);
    lv_obj_set_size(d.root, w, 0);
    // A tap on the drawer is the drawer's: without this it would fall through
    // to the card under it.
    lv_obj_add_flag(d.root, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(d.root, LV_OBJ_FLAG_HIDDEN);

    d.face = plain(d.root);
    add(d.face, KitStyle::KS_DRAWER_FACE);
    lv_obj_set_size(d.face, lv_pct(100), LV_SIZE_CONTENT);
    lv_obj_add_flag(d.face, LV_OBJ_FLAG_CLICKABLE);
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

// One rule for where it hangs (round 5): the system panel's own re-placing put
// it at the bar's foot instead, a line's width low, every time it closed - its
// top edge showed under the status icons for the slide.
void drawerSetTop(Drawer &d, int32_t top) {
    d.top = top;
    if (d.root) lv_obj_set_y(d.root, top - UI::sc(UI::hdr().LINE));
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
