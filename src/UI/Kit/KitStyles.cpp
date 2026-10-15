// The kit's shared styles (2.12 P3): filled from the tokens at Kit::begin(), and
// again on every repaint - a scheme or accent change - after which LVGL restyles
// every object using them (UI::onRepaint()). Each value is the one the component
// set locally before it was a component, so moving a component onto these
// changed no pixel. See include/UI/Kit.h; docs/design/components.md lists them.
#include "UI/Kit.h"
#include "UI/UIToolkit.h"

namespace Kit {

static lv_style_t s_style[(int)KitStyle::KS_COUNT];
static bool       s_ready = false;

static lv_style_t *st(KitStyle s) { return &s_style[(int)s]; }

static void flex(lv_style_t *x, lv_flex_flow_t flow, lv_flex_align_t main, lv_flex_align_t cross,
                 lv_flex_align_t track) {
    lv_style_set_layout          (x, LV_LAYOUT_FLEX);
    lv_style_set_flex_flow       (x, flow);
    lv_style_set_flex_main_place (x, main);
    lv_style_set_flex_cross_place(x, cross);
    lv_style_set_flex_track_place(x, track);
}

static void text(KitStyle s, const lv_font_t *f, uint32_t col) {
    lv_style_set_text_font (st(s), f);
    lv_style_set_text_color(st(s), UI::c(col));
}

// A button's three styles from its colours (Kit::button()).
static void fillButton(KitStyle face, KitStyle pressed, KitStyle ink, uint32_t bg, uint32_t edge, uint32_t ink_) {
    const UIShape &k = UI::shape();
    lv_style_t *f = st(face);
    lv_style_set_radius      (f, UI::mm(k.R_CONTROL));
    lv_style_set_bg_opa      (f, LV_OPA_COVER);
    lv_style_set_bg_color    (f, UI::c(bg));
    lv_style_set_border_width(f, LV_MAX(1, UI::mm(k.LINE_MID)));
    lv_style_set_border_color(f, UI::c(edge));
    lv_style_set_bg_color    (st(pressed), UI::c(UI::mix(bg, UI::pal().TEXT, 20)));
    text(ink, UI::type().NAME, ink_);
}

static void fill() {
    const UIPalette &p = UI::pal();
    const UIType    &t = UI::type();
    const UIShape   &k = UI::shape();
    using S = KitStyle;

    // --- Label roles ---
    text(S::KS_LABEL_NAME,       t.NAME, p.TEXT);
    text(S::KS_LABEL_NAME_DIM,   t.NAME, p.TEXT_DIM);
    text(S::KS_LABEL_TAG,        t.TAG,  p.TEXT);
    text(S::KS_LABEL_TAG_DIM,    t.TAG,  p.TEXT_DIM);
    text(S::KS_LABEL_TAG_ACCENT, t.TAG,  p.ACCENT);

    // --- Buttons ---
    fillButton(S::KS_BTN_CANCEL, S::KS_BTN_CANCEL_PRESSED, S::KS_BTN_CANCEL_INK,
               p.SURFACE, UI::mix(p.SURFACE, p.TEXT, 30), p.TEXT);
    fillButton(S::KS_BTN_DESTRUCTIVE, S::KS_BTN_DESTRUCTIVE_PRESSED, S::KS_BTN_DESTRUCTIVE_INK,
               p.ST_BAD, p.ST_BAD, p.SURFACE);

    // --- The dialog ---
    lv_style_t *dim = st(S::KS_DIALOG_DIM);
    lv_style_set_bg_color(dim, UI::c(p.GROUND));
    lv_style_set_bg_opa  (dim, LV_OPA_60);

    lv_style_t *box = st(S::KS_DIALOG_BOX_DESTRUCTIVE);
    lv_style_set_radius      (box, UI::sc(UI::met().RADIUS));
    lv_style_set_bg_opa      (box, LV_OPA_COVER);
    lv_style_set_bg_color    (box, UI::c(p.SURFACE_ALT));
    lv_style_set_border_width(box, LV_MAX(2, UI::mm(k.LINE_HEAVY)));
    lv_style_set_border_color(box, UI::c(p.ST_BAD));
    lv_style_set_pad_all     (box, UI::mm(k.S9));
    lv_style_set_pad_row     (box, UI::mm(k.S7));
    flex(box, LV_FLEX_FLOW_COLUMN, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    text(S::KS_DIALOG_BODY, t.TAG, p.TEXT_DIM);
    lv_style_set_text_align(st(S::KS_DIALOG_BODY), LV_TEXT_ALIGN_CENTER);

    lv_style_t *drow = st(S::KS_DIALOG_ROW);
    flex(drow, LV_FLEX_FLOW_ROW, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_style_set_pad_top(drow, UI::mm(k.S4));

    // --- Rows ---
    const int32_t ctlH = controlH();
    lv_style_t *row = st(S::KS_ROW);
    lv_style_set_width (row, lv_pct(100));
    lv_style_set_height(row, UI::mm(k.H_ROW));
    flex(row, LV_FLEX_FLOW_ROW, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    lv_style_t *dd = st(S::KS_DROP);
    lv_style_set_text_font   (dd, t.TAG);
    lv_style_set_bg_color    (dd, UI::c(p.SURFACE));
    lv_style_set_bg_opa      (dd, LV_OPA_COVER);
    lv_style_set_radius      (dd, UI::mm(k.R_CONTROL));
    lv_style_set_border_width(dd, LV_MAX(1, UI::mm(k.LINE_HAIR)));
    lv_style_set_pad_hor     (dd, UI::mm(k.S5));
    lv_style_set_pad_ver     (dd, LV_MAX(0, (ctlH - lv_font_get_line_height(t.TAG)) / 2 - 1));
    lv_style_set_shadow_width(dd, 0);
    lv_style_set_text_color  (st(S::KS_DROP_LIVE),   UI::c(p.TEXT));
    lv_style_set_border_color(st(S::KS_DROP_LIVE),   UI::c(p.ACCENT));
    lv_style_set_text_color  (st(S::KS_DROP_GREYED), UI::c(p.TEXT_DIM));
    lv_style_set_border_color(st(S::KS_DROP_GREYED), UI::c(UI::mix(p.SURFACE_ALT, p.TEXT, 25)));
    lv_style_set_text_font   (st(S::KS_DROP_ARROW),  t.TAG);

    lv_style_t *list = st(S::KS_LIST);
    lv_style_set_text_font   (list, t.TAG);
    lv_style_set_text_color  (list, UI::c(p.TEXT));
    lv_style_set_bg_color    (list, UI::c(p.SURFACE));
    lv_style_set_border_color(list, UI::c(p.ACCENT));
    lv_style_set_border_width(list, LV_MAX(1, UI::mm(k.LINE_HAIR)));
    lv_style_set_radius      (list, UI::mm(k.R_CONTROL));
    lv_style_set_shadow_width(list, 0);
    // EVERY CHOICE IN SIGHT (owner, 2.10d): the default theme caps a list at
    // 260 px, which on the P4_5 and the 4B is five rows. Only the screen limits
    // it (lv_dropdown_open() fits it above or below).
    lv_style_set_max_height  (list, lv_display_get_vertical_resolution(NULL));
    lv_style_set_bg_color    (st(S::KS_LIST_CHOSEN),  UI::c(p.ACCENT));
    lv_style_set_text_color  (st(S::KS_LIST_CHOSEN),  UI::c(UI::contrastOf(p.ACCENT, p.SURFACE_ALT, p.TEXT)));
    lv_style_set_bg_color    (st(S::KS_LIST_PRESSED), UI::c(UI::mix(p.SURFACE, p.TEXT, 20)));

    lv_style_set_text_font  (st(S::KS_CHECK), t.NAME);
    lv_style_set_pad_column (st(S::KS_CHECK), 0);
    // As tall as a dropdown (owner, S2): the box is the font's line height plus
    // its own padding.
    lv_style_t *box2 = st(S::KS_CHECK_BOX);
    lv_style_set_pad_all     (box2, LV_MAX(0, (ctlH - lv_font_get_line_height(t.NAME)) / 2));
    lv_style_set_radius      (box2, UI::mm(k.R_CONTROL));
    lv_style_set_border_width(box2, LV_MAX(2, UI::mm(k.LINE_MID)));
    lv_style_set_border_color(box2, UI::c(p.ACCENT));
    lv_style_set_bg_color    (box2, UI::c(p.SURFACE));
    lv_style_set_bg_opa      (box2, LV_OPA_COVER);
    lv_style_set_bg_color    (st(S::KS_CHECK_BOX_TICKED), UI::c(p.ACCENT));
    lv_style_set_text_color  (st(S::KS_CHECK_BOX_TICKED), UI::c(UI::contrastOf(p.ACCENT, p.SURFACE_ALT, p.TEXT)));

    lv_style_t *rb = st(S::KS_ROWBTN);
    lv_style_set_radius      (rb, UI::mm(k.R_CONTROL));
    lv_style_set_bg_opa      (rb, LV_OPA_COVER);
    lv_style_set_bg_color    (rb, UI::c(p.SURFACE));
    lv_style_set_border_width(rb, LV_MAX(1, UI::mm(k.LINE_HAIR)));
    lv_style_set_bg_color    (st(S::KS_ROWBTN_PRESSED), UI::c(UI::mix(p.SURFACE, p.TEXT, 15)));
    lv_style_set_border_color(st(S::KS_ROWBTN_LIVE),    UI::c(p.ACCENT));
    lv_style_set_border_color(st(S::KS_ROWBTN_GREYED),  UI::c(UI::mix(p.SURFACE, p.TEXT, 25)));

    // --- Chips ---
    lv_style_t *chip = st(S::KS_CHIP);
    lv_style_set_width       (chip, LV_SIZE_CONTENT);
    lv_style_set_height      (chip, UI::mm(k.H_CHIP));
    lv_style_set_radius      (chip, UI::mm(k.R_CHIP));
    lv_style_set_bg_opa      (chip, LV_OPA_COVER);
    lv_style_set_border_color(chip, UI::c(p.ACCENT));
    lv_style_set_bg_color    (st(S::KS_CHIP_UNLIT),   UI::c(p.SURFACE));
    lv_style_set_border_width(st(S::KS_CHIP_UNLIT),   0);
    lv_style_set_bg_color    (st(S::KS_CHIP_LIT),     UI::c(UI::mix(p.SURFACE, p.ACCENT, 25)));
    lv_style_set_border_width(st(S::KS_CHIP_LIT),     LV_MAX(1, UI::mm(k.LINE_MID)));
    lv_style_set_bg_color    (st(S::KS_CHIP_PRESSED), UI::c(UI::mix(p.SURFACE, p.TEXT, 15)));

    // --- List items ---
    lv_style_t *item = st(S::KS_ITEM);
    lv_style_set_radius  (item, UI::sc(UI::met().RADIUS));
    lv_style_set_bg_opa  (item, LV_OPA_COVER);
    lv_style_set_bg_color(item, UI::c(p.SURFACE));
    lv_style_set_bg_color(st(S::KS_ITEM_PRESSED), UI::c(UI::mix(p.SURFACE, p.TEXT, 15)));

    // --- Pills (scene buttons) and swatches ---
    lv_style_t *pill = st(S::KS_PILL);
    lv_style_set_radius     (pill, LV_RADIUS_CIRCLE);
    lv_style_set_bg_opa     (pill, LV_OPA_COVER);
    lv_style_set_bg_color   (pill, UI::c(p.SURFACE));
    lv_style_set_outline_pad(pill, LV_MAX(2, UI::mm(k.RING_PAD)));   // room for the ring round a loaded one
    lv_style_set_bg_color   (st(S::KS_PILL_PRESSED), UI::c(UI::mix(p.SURFACE, p.TEXT, 20)));
    lv_style_t *sw = st(S::KS_SWATCH);
    lv_style_set_radius      (sw, LV_RADIUS_CIRCLE);
    lv_style_set_bg_opa      (sw, LV_OPA_COVER);
    lv_style_set_border_width(sw, LV_MAX(1, UI::mm(k.LINE_HAIR)));
    lv_style_set_border_color(sw, UI::border());
    lv_style_set_outline_pad (sw, LV_MAX(2, UI::mm(k.RING_PAD)));   // room for the ring round the chosen one
    // The card's own PAUSED badge, in the idle state's colour.
    lv_style_t *badge = st(S::KS_BADGE_PAUSED);
    text(S::KS_BADGE_PAUSED, t.TAG, UI::contrastOf(p.ST_IDLE, p.GROUND, p.TEXT));
    lv_style_set_bg_color          (badge, UI::c(p.ST_IDLE));
    lv_style_set_bg_opa            (badge, LV_OPA_COVER);
    lv_style_set_radius            (badge, LV_RADIUS_CIRCLE);
    lv_style_set_pad_hor           (badge, UI::mm(k.S4));
    lv_style_set_text_letter_space (badge, UI::mm(k.TRACK_PILL));

    // --- Folder panels ---
    lv_style_t *face = st(S::KS_PANEL_FACE);
    lv_style_set_radius      (face, UI::mm(k.R_PANEL));
    lv_style_set_bg_color    (face, UI::c(p.SURFACE_ALT));
    lv_style_set_bg_opa      (face, LV_OPA_COVER);
    lv_style_set_border_color(face, UI::border());
    lv_style_set_border_width(face, UI::met().BORDER_W ? UI::met().BORDER_W : 1);
    lv_style_set_bg_color    (st(S::KS_PANEL_FILL), UI::c(p.SURFACE_ALT));
    lv_style_set_bg_opa      (st(S::KS_PANEL_FILL), LV_OPA_COVER);
    text(S::KS_PANEL_TITLE, t.TAG, p.ACCENT);
    lv_style_set_text_letter_space(st(S::KS_PANEL_TITLE), UI::mm(k.TRACK_TAB));
    lv_style_t *body = st(S::KS_PANEL_BODY);
    lv_style_set_width    (body, lv_pct(100));
    lv_style_set_height   (body, lv_pct(100));
    lv_style_set_pad_right(body, UI::mm(k.S7));
    lv_style_set_layout   (body, LV_LAYOUT_FLEX);
    lv_style_set_flex_flow(body, LV_FLEX_FLOW_COLUMN);
    lv_style_t *caret = st(S::KS_PANEL_CARET);
    lv_style_set_line_width  (caret, LV_MAX(2, UI::mm(k.LINE_PLOT)));
    lv_style_set_line_rounded(caret, true);
    lv_style_set_line_color  (caret, UI::c(UI::mix(p.SURFACE_ALT, p.TEXT, 60)));

    // --- The sheet ---
    // The scheme's raised surface (the mock's window is Midnight's SURFACE_ALT
    // exactly) and the border token.
    lv_style_t *win = st(S::KS_SHEET_FACE);
    lv_style_set_radius      (win, UI::mm(k.R_WINDOW));
    lv_style_set_bg_color    (win, UI::c(p.SURFACE_ALT));
    lv_style_set_bg_opa      (win, LV_OPA_COVER);
    lv_style_set_border_color(win, UI::border());
    lv_style_set_border_width(win, UI::met().BORDER_W ? UI::met().BORDER_W : 1);
    lv_style_t *bar = st(S::KS_SHEET_BAR);
    lv_style_set_radius  (bar, LV_RADIUS_CIRCLE);
    lv_style_set_bg_color(bar, UI::c(p.ACCENT));
    lv_style_set_bg_opa  (bar, LV_OPA_COVER);
}

void begin() {
    if (s_ready) return;
    for (lv_style_t &x : s_style) lv_style_init(&x);
    s_ready = true;
    fill();
    UI::onRepaint(fill);   // from now on, refilled with every scheme
}

lv_style_t *style(KitStyle s) {
    begin();
    const int i = (int)s;
    return (i >= 0 && i < (int)KitStyle::KS_COUNT) ? &s_style[i] : &s_style[0];
}

void add(lv_obj_t *o, KitStyle s, lv_style_selector_t sel) { lv_obj_add_style(o, style(s), sel); }

} // namespace Kit
