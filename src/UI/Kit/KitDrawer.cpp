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

void drawer(Drawer &d, lv_obj_t *parent, int32_t w) {
    d.w = w;
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
    const int32_t margin  = d.w >= screenW ? 0 : UI::sc(UI::hdr().DRAWER_PAD);
    int32_t x = centreX - d.w / 2;
    if (x + d.w > screenW - margin) x = screenW - margin - d.w;
    if (x < margin) x = margin;
    lv_obj_set_pos(d.root, x, top);
    d.top = top;
}

void drawerOpen(Drawer &d) {
    if (!d.root) return;
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

} // namespace Kit
