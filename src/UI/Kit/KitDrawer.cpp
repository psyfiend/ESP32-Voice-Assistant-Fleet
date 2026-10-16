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
    lv_obj_set_y(d->face, v - d->openH - UI::sc(UI::hdr().DRAWER_R));
    // Hidden at zero, not merely zero-high: a drawer casts the scheme's shadow,
    // and a zero-high one would leave a line of it under the bar.
    if (v <= 0) lv_obj_add_flag  (d->root, LV_OBJ_FLAG_HIDDEN);
    else        lv_obj_clear_flag(d->root, LV_OBJ_FLAG_HIDDEN);
}

void animate(Drawer &d, int32_t to) {
    lv_anim_delete(&d, slide);
    lv_anim_t a;
    lv_anim_init        (&a);
    lv_anim_set_var     (&a, &d);
    lv_anim_set_values  (&a, lv_obj_get_height(d.root), to);
    lv_anim_set_duration(&a, SLIDE_MS);
    lv_anim_set_exec_cb (&a, slide);
    lv_anim_set_path_cb (&a, lv_anim_path_ease_out);
    lv_anim_start       (&a);
}
}  // namespace

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

// ONE SHAPE (D-23). A join is a square of the shape's fill holding a ring of the
// accent - a circle whose inside is what lies outside the shape - clipped to the
// square, so a quarter of it shows: the outline turning from the line into a
// side, rounded. Its radius is JOIN_R and the line's width; R0 is both.
static int32_t joinR0() { return UI::sc(UI::hdr().JOIN_R) + UI::sc(UI::hdr().LINE); }

lv_obj_t *joinCorner(lv_obj_t *parent, bool flare) {
    const int32_t r0 = joinR0();
    lv_obj_t *sq = plain(parent);
    add(sq, KitStyle::KS_JOIN_FILL);
    lv_obj_set_size(sq, r0, r0);
    lv_obj_t *ring = plain(sq);
    add(ring, flare ? KitStyle::KS_JOIN_RING_GROUND : KitStyle::KS_JOIN_RING_BAR);
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

// Beside a panel, below the bar: the line curving down into the panel's side.
void flareAt(lv_obj_t *j, int32_t edgeX, bool right, int32_t top) {
    if (!j) return;
    const int32_t r0 = joinR0(), b = UI::sc(UI::hdr().LINE);
    lv_obj_set_pos(j, right ? edgeX - b : edgeX + b - r0, top - b);
    lv_obj_set_pos(lv_obj_get_child(j, 0), right ? 0 : -r0, 0);
}

void drawer(Drawer &d, lv_obj_t *parent, int32_t w) {
    d.w = w;
    d.flare[0] = joinCorner(parent, true);
    d.flare[1] = joinCorner(parent, true);
    d.root = plain(parent);
    add(d.root, KitStyle::KS_DRAWER);
    lv_obj_add_style(d.root, UI::paint(UIPaint::PAINT_LIFT), 0);
    lv_obj_set_size(d.root, w, 0);
    // A tap on the drawer is the drawer's: without this it would fall through
    // to the card under it.
    lv_obj_add_flag(d.root, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(d.root, LV_OBJ_FLAG_HIDDEN);

    d.face = plain(d.root);
    add(d.face, KitStyle::KS_DRAWER_FACE);
    lv_obj_set_size(d.face, lv_pct(100), LV_SIZE_CONTENT);
    lv_obj_add_flag(d.face, LV_OBJ_FLAG_CLICKABLE);
    // Its top corners pushed above the wrapper's clip, so it is square where it
    // meets the bar (the face's top padding pays the push back).
    lv_obj_set_y(d.face, -UI::sc(UI::hdr().DRAWER_R));
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
    lv_obj_set_pos(d.root, x, top);
    d.x = x;
    d.top = top;
}

void drawerOpen(Drawer &d, int32_t tabL, int32_t tabR) {
    if (!d.root) return;
    // The flares: where the panel reaches past its tab, the bar's line curves
    // down into the panel's side; where they share a side, a straight line.
    const bool fl = tabL >= 0 && d.x < tabL;
    const bool fr = tabR >= 0 && d.x + d.w > tabR;
    flareAt(d.flare[0], d.x, false, d.top);
    flareAt(d.flare[1], d.x + d.w, true, d.top);
    for (uint8_t i = 0; i < 2; i++) {
        if (!d.flare[i]) continue;
        if (i ? fr : fl) { lv_obj_remove_flag(d.flare[i], LV_OBJ_FLAG_HIDDEN); lv_obj_move_foreground(d.flare[i]); }
        else             lv_obj_add_flag(d.flare[i], LV_OBJ_FLAG_HIDDEN);
    }
    // Measured, never guessed (the system panel's #50 lesson): the face is
    // content-sized, so it has a true height while the wrapper is closed.
    lv_obj_clear_flag(d.root, LV_OBJ_FLAG_HIDDEN);
    lv_obj_update_layout(d.face);
    int32_t h = lv_obj_get_height(d.face) - UI::sc(UI::hdr().DRAWER_R);
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
    for (lv_obj_t *f : d.flare) if (f) lv_obj_add_flag(f, LV_OBJ_FLAG_HIDDEN);
    if (anim) { animate(d, 0); return; }
    lv_anim_delete(&d, slide);
    slide(&d, 0);
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

void toggleButtonSet(lv_obj_t *b, bool on) {
    if (!b) return;
    // Its size as set (its coordinates are not laid out yet when it is built).
    finish(b, lv_obj_get_style_width(b, LV_PART_MAIN), lv_obj_get_style_height(b, LV_PART_MAIN),
           UI::mm(UI::shape().R_CONTROL), on ? Finish::FINISH_GLASS : Finish::FINISH_SATIN);
    if (lv_obj_t *l = labelIn(b)) lv_obj_center(l);
}

} // namespace Kit
