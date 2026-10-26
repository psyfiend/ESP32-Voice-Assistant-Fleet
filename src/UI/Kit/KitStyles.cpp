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
static void fillCards();   // below fill(): the cards' and clusters' roles

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
    text(S::KS_LABEL_TAG_ACTIVE, t.TAG,  p.ST_ACTIVE);

    // --- Buttons ---
    fillButton(S::KS_BTN_CANCEL, S::KS_BTN_CANCEL_PRESSED, S::KS_BTN_CANCEL_INK,
               p.SURFACE, UI::mix(p.SURFACE, p.TEXT, 30), p.TEXT);
    fillButton(S::KS_BTN_DESTRUCTIVE, S::KS_BTN_DESTRUCTIVE_PRESSED, S::KS_BTN_DESTRUCTIVE_INK,
               p.ST_BAD, p.ST_BAD, p.SURFACE);
    fillButton(S::KS_BTN_CONFIRM, S::KS_BTN_CONFIRM_PRESSED, S::KS_BTN_CONFIRM_INK,
               p.ACCENT, p.ACCENT, UI::contrastOf(p.ACCENT, p.SURFACE_ALT, p.TEXT));

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

    // --- The keyboard (2.11, K95): a dialog's box with the accent for its edge,
    // a field like a dropdown's, keys like the window's satin surfaces ---
    lv_style_t *kbox = st(S::KS_KBD_BOX);
    lv_style_set_radius      (kbox, UI::sc(UI::met().RADIUS));
    lv_style_set_bg_opa      (kbox, LV_OPA_COVER);
    lv_style_set_bg_color    (kbox, UI::c(p.SURFACE_ALT));
    lv_style_set_border_width(kbox, LV_MAX(2, UI::mm(k.LINE_HEAVY)));
    lv_style_set_border_color(kbox, UI::c(p.ACCENT));
    lv_style_set_pad_all     (kbox, UI::mm(k.S7));
    lv_style_set_pad_row     (kbox, UI::mm(k.S5));
    flex(kbox, LV_FLEX_FLOW_COLUMN, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    lv_style_t *field = st(S::KS_FIELD);
    text(S::KS_FIELD, t.NAME, p.TEXT);
    lv_style_set_bg_opa      (field, LV_OPA_COVER);
    lv_style_set_bg_color    (field, UI::c(p.SURFACE));
    lv_style_set_radius      (field, UI::mm(k.R_CONTROL));
    lv_style_set_border_width(field, LV_MAX(1, UI::mm(k.LINE_MID)));
    lv_style_set_border_color(field, UI::c(p.ACCENT));
    lv_style_set_pad_hor     (field, UI::mm(k.S5));
    lv_style_set_pad_ver     (field, UI::mm(k.S3));
    // A cursor that stays lit: a blink redraws the field twice a second for nothing.
    lv_style_t *cur = st(S::KS_FIELD_CURSOR);
    lv_style_set_border_side (cur, LV_BORDER_SIDE_LEFT);
    lv_style_set_border_width(cur, LV_MAX(2, UI::mm(k.LINE_HEAVY)));
    lv_style_set_border_color(cur, UI::c(p.ACCENT));
    lv_style_set_pad_left    (cur, -1);
    lv_style_set_anim_duration(cur, 0);

    lv_style_t *kb = st(S::KS_KBD);
    lv_style_set_bg_opa    (kb, LV_OPA_COVER);
    lv_style_set_bg_color  (kb, UI::c(p.SURFACE_ALT));
    lv_style_set_pad_all   (kb, UI::mm(k.KEY_GAP));
    lv_style_set_pad_row   (kb, UI::mm(k.KEY_GAP));
    lv_style_set_pad_column(kb, UI::mm(k.KEY_GAP));
    lv_style_t *key = st(S::KS_KEY);
    text(S::KS_KEY, t.NAME, p.TEXT);
    lv_style_set_bg_opa (key, LV_OPA_COVER);
    lv_style_set_bg_color(key, UI::c(p.SURFACE));
    lv_style_set_radius (key, UI::mm(k.R_CONTROL));
    lv_style_set_bg_color(st(S::KS_KEY_CTRL),    UI::c(UI::mix(p.SURFACE, p.TEXT, 12)));
    lv_style_set_bg_color(st(S::KS_KEY_PRESSED), UI::c(UI::mix(p.SURFACE, p.ACCENT, 45)));

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

    // A slider row: a track a third of a chip's height, the accent filling it, a
    // knob a chip's height in the surface ringed in the accent.
    lv_style_t *sl = st(S::KS_SLIDER);
    lv_style_set_height  (sl, LV_MAX(4, UI::mm(k.H_CHIP) / 3));
    lv_style_set_radius  (sl, LV_RADIUS_CIRCLE);
    lv_style_set_bg_opa  (sl, LV_OPA_COVER);
    lv_style_set_bg_color(sl, UI::c(UI::mix(p.SURFACE, p.TEXT, 18)));
    lv_style_t *si = st(S::KS_SLIDER_IND);
    lv_style_set_radius  (si, LV_RADIUS_CIRCLE);
    lv_style_set_bg_opa  (si, LV_OPA_COVER);
    lv_style_set_bg_color(si, UI::c(p.ACCENT));
    lv_style_t *sk = st(S::KS_SLIDER_KNOB);
    lv_style_set_radius      (sk, LV_RADIUS_CIRCLE);
    lv_style_set_bg_opa      (sk, LV_OPA_COVER);
    lv_style_set_bg_color    (sk, UI::c(p.SURFACE));
    lv_style_set_border_width(sk, LV_MAX(1, UI::mm(k.LINE_MID)));
    lv_style_set_border_color(sk, UI::c(p.ACCENT));
    lv_style_set_pad_all     (sk, (UI::mm(k.H_CHIP) - LV_MAX(4, UI::mm(k.H_CHIP) / 3)) / 2);

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

    // --- The finishes (#99) ---
    // Pressed, a finished surface goes down a pixel, its face with it.
    lv_style_set_translate_y(st(S::KS_SINK), 1);

    // --- The page header (2.8) ---
    // The bar's look is the one Panel_Header set locally before it had slots.
    const UIHeader &h = UI::hdr();
    lv_style_t *hb = st(S::KS_HDR_BAR);
    lv_style_set_bg_color    (hb, UI::c(p.SURFACE_ALT));
    lv_style_set_bg_opa      (hb, LV_OPA_COVER);
    lv_style_set_border_side (hb, LV_BORDER_SIDE_BOTTOM);
    lv_style_set_border_width(hb, UI::sc(h.LINE));
    lv_style_set_border_color(hb, UI::c(p.ACCENT));
    lv_style_set_radius      (hb, 0);
    lv_style_set_pad_all     (hb, 0);
    // No flex: where each slot stands is the header plan's (round 2) - the
    // sides at the ends, the middle on the centre.
    // A slot reaches FRAME_R past the bar's foot (Kit::headerSlot()): the bar
    // clips it there, so an open slot's rounded corners show only at its top.
    // Its padding below pays that back, so what it holds sits in the bar.
    lv_style_t *hs = st(S::KS_HDR_SLOT);
    lv_style_set_pad_hor   (hs, UI::sc(h.SLOT_PAD));
    lv_style_set_pad_top   (hs, 0);
    lv_style_set_pad_bottom(hs, UI::sc(h.FRAME_R) + UI::sc(h.LINE));
    lv_style_set_pad_column(hs, UI::sc(h.SLOT_GAP));
    lv_style_set_radius    (hs, UI::sc(h.FRAME_R));
    flex(hs, LV_FLEX_FLOW_ROW, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_style_set_bg_color(st(S::KS_HDR_SLOT_PRESSED), UI::c(UI::mix(p.SURFACE_ALT, p.TEXT, 12)));
    lv_style_set_bg_opa  (st(S::KS_HDR_SLOT_PRESSED), LV_OPA_COVER);
    // OPEN: the tab of one shape with its panel (D-23) - the panel's face and an
    // accent edge on three sides, the bar's accent line covered beneath it, so
    // the panel pours out of the bar itself.
    lv_style_t *ho = st(S::KS_HDR_SLOT_OPEN);
    lv_style_set_bg_color    (ho, UI::c(p.SURFACE));
    lv_style_set_bg_opa      (ho, LV_OPA_COVER);
    lv_style_set_border_color(ho, UI::c(p.ACCENT));
    lv_style_set_border_width(ho, UI::sc(h.LINE));
    lv_style_set_border_side (ho, (lv_border_side_t)(LV_BORDER_SIDE_LEFT | LV_BORDER_SIDE_TOP | LV_BORDER_SIDE_RIGHT));
    // The coves: the fill, and a ring of the accent around the bar outside.
    lv_style_t *jf = st(S::KS_JOIN_FILL);
    lv_style_set_bg_color(jf, UI::c(p.SURFACE));
    lv_style_set_bg_opa  (jf, LV_OPA_COVER);
    lv_style_t *js = st(S::KS_JOIN_RING_BAR);
    lv_style_set_bg_opa      (js, LV_OPA_COVER);
    lv_style_set_radius      (js, LV_RADIUS_CIRCLE);
    lv_style_set_border_color(js, UI::c(p.ACCENT));
    lv_style_set_border_width(js, UI::sc(h.LINE));
    lv_style_set_bg_color    (js, UI::c(p.SURFACE_ALT));
    lv_style_set_bg_color(st(S::KS_RULE), UI::c(UI::mix(p.SURFACE, p.TEXT, 30)));
    lv_style_set_bg_opa  (st(S::KS_RULE), LV_OPA_COVER);
    lv_style_set_radius  (st(S::KS_RULE), UI::sc(UI::hdr().LINE));
    text(S::KS_HDR_TEXT,     UIToolkit::Font_Header, p.TEXT);   // the bar's words (D-26)
    text(S::KS_HDR_TEXT_DIM, UIToolkit::Font_Caption,     p.TEXT_DIM);
    text(S::KS_HDR_ICON,     t.ICON_SM,                   p.TEXT);
    // The time and its am / pm stand on one line, their bottoms together - near
    // enough a shared baseline (flex has no baseline alignment).
    lv_style_t *ck = st(S::KS_HDR_CLOCK);
    lv_style_set_width (ck, LV_SIZE_CONTENT);
    lv_style_set_height(ck, LV_SIZE_CONTENT);
    flex(ck, LV_FLEX_FLOW_ROW, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_END);
    lv_style_t *ds = st(S::KS_HDR_DOTS);
    lv_style_set_width     (ds, LV_SIZE_CONTENT);
    lv_style_set_height    (ds, LV_SIZE_CONTENT);
    lv_style_set_pad_column(ds, UI::sc(h.DOT_GAP));
    flex(ds, LV_FLEX_FLOW_ROW, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    // A glyph's frames: the same size, the square a shade of the bar, the
    // circle's colour the glyph's own (set by the glyph, it is its state).
    for (S f : { S::KS_GLYPH_SQUARE, S::KS_GLYPH_CIRCLE }) {
        lv_style_t *g = st(f);
        lv_style_set_width (g, UI::sc(h.FRAME));
        lv_style_set_height(g, UI::sc(h.FRAME));
        lv_style_set_bg_opa(g, LV_OPA_COVER);
        flex(g, LV_FLEX_FLOW_ROW, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    }
    lv_style_set_radius  (st(S::KS_GLYPH_SQUARE), UI::sc(h.FRAME_R));
    lv_style_set_bg_color(st(S::KS_GLYPH_SQUARE), UI::c(UI::mix(p.SURFACE_ALT, p.TEXT, h.FRAME_MIX)));
    lv_style_set_radius  (st(S::KS_GLYPH_CIRCLE), LV_RADIUS_CIRCLE);

    // The page's dots. This page's is larger and filled, the others smaller
    // rings: told apart by shape and size, never by colour alone (the owner).
    lv_style_t *don = st(S::KS_HDR_DOT_ON);
    lv_style_set_width   (don, UI::sc(h.DOT_ON));
    lv_style_set_height  (don, UI::sc(h.DOT_ON));
    lv_style_set_radius  (don, LV_RADIUS_CIRCLE);
    lv_style_set_bg_color(don, UI::c(p.ACCENT));
    lv_style_set_bg_opa  (don, LV_OPA_COVER);
    lv_style_t *doff = st(S::KS_HDR_DOT_OFF);
    lv_style_set_width       (doff, UI::sc(h.DOT_OFF));
    lv_style_set_height      (doff, UI::sc(h.DOT_OFF));
    lv_style_set_radius      (doff, LV_RADIUS_CIRCLE);
    lv_style_set_bg_opa      (doff, LV_OPA_TRANSP);
    lv_style_set_border_width(doff, UI::sc(h.DOT_RING));
    lv_style_set_border_color(doff, UI::c(p.TEXT));

    // The error mark: "!" on a disc in the stale colour - a shape as well as a
    // colour, beside the dashes it explains.
    lv_style_t *mk = st(S::KS_HDR_MARK);
    text(S::KS_HDR_MARK, UIToolkit::Font_Caption, UI::contrastOf(p.ST_WARN, p.GROUND, p.TEXT));
    lv_style_set_width     (mk, UI::sc(h.MARK));
    lv_style_set_height    (mk, UI::sc(h.MARK));
    lv_style_set_radius    (mk, LV_RADIUS_CIRCLE);
    lv_style_set_bg_color  (mk, UI::c(p.ST_WARN));
    lv_style_set_bg_opa    (mk, LV_OPA_COVER);
    lv_style_set_text_align(mk, LV_TEXT_ALIGN_CENTER);
    lv_style_set_pad_top   (mk, LV_MAX(0, (UI::sc(h.MARK) - lv_font_get_line_height(UIToolkit::Font_Caption)) / 2));

    // --- A slot's drawer (2.8) ---
    // THE WRAPPER IS THE PANEL (2.11 round 5): its surface and its edge, every
    // corner rounded - it lies ON the bar (D-35), its top edge the bar's line,
    // its top corners turning down from it. It is what grows and shrinks, so
    // the top edge and its corners stay whole while the foot moves (the owner:
    // the walls stay joined to the top's curves until the last frame). It clips
    // the face riding inside it; no clip_corner (a layer the wrapper's size,
    // the trap that froze the P4_5 - LESSONS).
    lv_style_t *dr = st(S::KS_DRAWER);
    lv_style_set_bg_color    (dr, UI::c(p.SURFACE));
    lv_style_set_bg_opa      (dr, LV_OPA_COVER);
    // Its edge is the accent, the bar's line's width (D-23): with the open
    // slot's, one outline from the tab's top to the panel's foot. Drawn after
    // the face, so what rides up passes under it.
    lv_style_set_border_width(dr, UI::sc(h.LINE));
    lv_style_set_border_color(dr, UI::c(p.ACCENT));
    lv_style_set_border_post (dr, true);
    lv_style_set_pad_all     (dr, 0);
    lv_style_set_radius      (dr, UI::sc(h.DRAWER_R));
    // The face: the panel's contents, riding the moving foot. Clear; its edge
    // is kept at the line's width, unpainted, so the contents sit where they did.
    lv_style_t *df = st(S::KS_DRAWER_FACE);
    lv_style_set_bg_opa      (df, LV_OPA_TRANSP);
    lv_style_set_border_width(df, UI::sc(h.LINE));
    lv_style_set_border_opa  (df, LV_OPA_TRANSP);
    lv_style_set_pad_hor     (df, UI::sc(h.DRAWER_PAD));
    lv_style_set_pad_bottom  (df, UI::sc(h.DRAWER_PAD));
    lv_style_set_pad_top     (df, UI::sc(h.DRAWER_PAD));
    lv_style_set_pad_row     (df, UI::sc(h.DRAWER_GAP));
    flex(df, LV_FLEX_FLOW_COLUMN, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_style_t *di = st(S::KS_DRAWER_ITEM);
    lv_style_set_width     (di, lv_pct(100));
    lv_style_set_height    (di, UI::minTouch());
    lv_style_set_pad_hor   (di, UI::sc(h.DRAWER_PAD));
    lv_style_set_pad_column(di, UI::sc(h.SLOT_GAP));
    flex(di, LV_FLEX_FLOW_ROW, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    text(S::KS_DRAWER_BIG, UIToolkit::Font_Hero, p.TEXT);

    fillCards();
}

// CARDS (2.11 round 1): each value the one Card, StateCard and ValueCard set
// locally before they had roles, so moving them changed no pixel. The surface's
// colour stays the card's (a paused card mixes it toward the ground).
static void fillCards() {
    const UIPalette &p = UI::pal();
    const UIMetrics &m = UI::met();
    const UIType    &t = UI::type();
    const UIShape   &k = UI::shape();
    using S = KitStyle;

    lv_style_t *root = st(S::KS_CARD_ROOT);
    lv_style_set_bg_opa      (root, LV_OPA_TRANSP);
    lv_style_set_border_width(root, 0);
    lv_style_set_pad_all     (root, 0);
    lv_style_set_pad_gap     (root, 0);
    lv_style_set_layout      (root, LV_LAYOUT_FLEX);
    lv_style_set_flex_flow   (root, LV_FLEX_FLOW_COLUMN);

    lv_style_t *pl = st(S::KS_CARD_PLAIN);
    lv_style_set_bg_opa      (pl, LV_OPA_TRANSP);
    lv_style_set_border_width(pl, 0);
    lv_style_set_pad_all     (pl, 0);

    lv_style_t *sf = st(S::KS_CARD_SURFACE);
    lv_style_set_bg_color        (sf, UI::c(p.SURFACE));
    lv_style_set_bg_opa          (sf, LV_OPA_COVER);
    lv_style_set_radius          (sf, UI::sc(m.RADIUS));
    lv_style_set_border_width    (sf, m.BORDER_W);
    lv_style_set_border_color    (sf, UI::border());
    lv_style_set_shadow_width    (sf, UI::sc(m.SHADOW));
    lv_style_set_shadow_offset_y (sf, UI::sc(m.SHADOW_Y));
    lv_style_set_shadow_opa      (sf, m.SHADOW ? (lv_opa_t)m.SHADOW_OPA : LV_OPA_TRANSP);
    lv_style_set_pad_all         (sf, 0);   // the body owns padding

    lv_style_t *bd = st(S::KS_CARD_BODY);
    lv_style_set_bg_opa      (bd, LV_OPA_TRANSP);
    lv_style_set_border_width(bd, 0);
    lv_style_set_pad_all     (bd, UI::sc(m.PAD));

    lv_style_t *sp = st(S::KS_CARD_STRIP);
    lv_style_set_pad_all     (sp, 0);
    lv_style_set_pad_left    (sp, UI::sc(7));
    lv_style_set_pad_right   (sp, UI::sc(7));
    lv_style_set_border_width(sp, 0);

    lv_style_t *pi = st(S::KS_CARD_PILL);
    lv_style_set_bg_opa(pi, LV_OPA_COVER);
    lv_style_set_radius(pi, UI::sc(m.RADIUS / 2));

    lv_style_t *bg = st(S::KS_CARD_BADGE);
    lv_style_set_bg_opa (bg, LV_OPA_COVER);
    lv_style_set_radius (bg, UI::sc(m.RADIUS / 2));
    lv_style_set_pad_hor(bg, UI::sc(6));
    lv_style_set_pad_ver(bg, UI::sc(3));

    lv_style_t *dc = st(S::KS_CARD_DISC);
    lv_style_set_border_width(dc, 0);
    lv_style_set_pad_all     (dc, 0);
    lv_style_set_radius      (dc, LV_RADIUS_CIRCLE);

    lv_style_set_text_font (st(S::KS_CARD_NAME), t.NAME);
    lv_style_set_text_align(st(S::KS_CARD_NAME), LV_TEXT_ALIGN_CENTER);
    lv_style_set_text_font (st(S::KS_CARD_SMALL), t.TAG);
    text(S::KS_CARD_TAGTEXT, t.TAG, p.SURFACE);
    lv_style_set_text_font (st(S::KS_CARD_UNIT), t.UNIT);
    lv_style_set_text_font (st(S::KS_CARD_ICON_SM), t.ICON_SM);

    lv_style_t *dg = st(S::KS_CARD_DIAG);
    lv_style_set_line_width  (dg, UI::sc(6));
    lv_style_set_line_opa    (dg, LV_OPA_60);
    lv_style_set_line_rounded(dg, true);

    // --- A cluster (2.11): its frame is a card's surface (KS_CARD_SURFACE) ---
    // One line, the name on the left and the values on the right (the header's
    // planner places them); a rule under it in the border's colour - a light
    // scheme draws no card border, so the rule has a hairline of its own.
    lv_style_t *ch = st(S::KS_CLUSTER_HDR);
    lv_style_set_bg_color    (ch, UI::c(p.SURFACE_ALT));   // a faint band, as the page header's (round 10)
    lv_style_set_bg_opa      (ch, LV_OPA_COVER);
    lv_style_set_border_side (ch, LV_BORDER_SIDE_BOTTOM);
    lv_style_set_border_width(ch, LV_MAX(1, UI::mm(k.LINE_HAIR)));
    lv_style_set_border_color(ch, UI::border());
    lv_style_set_pad_all     (ch, 0);
    lv_style_set_radius      (ch, 0);
    text(S::KS_CLUSTER_TITLE, t.NAME, p.TEXT);
    lv_style_t *sw = st(S::KS_CLUSTER_SWATCH);
    lv_style_set_width (sw, LV_MAX(2, UI::mm(k.C_SWATCH_W)));
    lv_style_set_height(sw, LV_MAX(4, UI::mm(k.C_SWATCH_H)));
    lv_style_set_radius(sw, LV_MAX(1, UI::mm(k.C_SWATCH_W) / 3));
    lv_style_set_bg_opa(sw, LV_OPA_COVER);
    text(S::KS_CLUSTER_VALUE, t.NAME, p.TEXT);
    // The page header's glyph frames at the cluster header's size: four fifths of
    // its height, the page header's tint (round 11, K115).
    for (S f : { S::KS_CLUSTER_GLYPH_SQUARE, S::KS_CLUSTER_GLYPH_CIRCLE }) {
        lv_style_t *g = st(f);
        lv_style_set_width (g, UI::mm(k.C_HDR_H) * 4 / 5);
        lv_style_set_height(g, UI::mm(k.C_HDR_H) * 4 / 5);
        lv_style_set_bg_color(g, UI::c(UI::mix(p.SURFACE_ALT, p.TEXT, UI::hdr().FRAME_MIX)));
        lv_style_set_bg_opa(g, LV_OPA_COVER);
        flex(g, LV_FLEX_FLOW_ROW, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    }
    lv_style_set_radius(st(S::KS_CLUSTER_GLYPH_SQUARE), UI::mm(k.C_HDR_H) / 5);
    lv_style_set_radius(st(S::KS_CLUSTER_GLYPH_CIRCLE), LV_RADIUS_CIRCLE);
    lv_style_t *tl = st(S::KS_TILE);
    // A FAINT PANEL EACH (round 11, the owner's E - K116): between the cluster's
    // surface and SURFACE_ALT, so the off disc (SURFACE_ALT) and the header's
    // band still stand apart from it.
    lv_style_set_bg_color    (tl, UI::c(UI::mix(p.SURFACE, p.SURFACE_ALT, 50)));
    lv_style_set_bg_opa      (tl, LV_OPA_COVER);
    lv_style_set_border_width(tl, 0);
    lv_style_set_pad_all     (tl, 0);
    lv_style_set_radius      (tl, UI::sc(m.RADIUS));
    lv_style_set_bg_color(st(S::KS_TILE_PRESSED), UI::c(UI::mix(p.SURFACE, p.TEXT, 12)));
    lv_style_set_bg_opa  (st(S::KS_TILE_PRESSED), LV_OPA_COVER);
    text(S::KS_TILE_NAME, t.TAG, p.TEXT);
    lv_style_set_text_align(st(S::KS_TILE_NAME), LV_TEXT_ALIGN_CENTER);
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
