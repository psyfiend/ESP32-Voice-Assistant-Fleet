// Folder panels (2.12 P3, from the card window's SETTINGS and its mirror image,
// the left panel - owner, rounds 7-11, 2026-10-05..07). Sliding it up and down,
// dragging it and its carets' meaning are the caller's (PopupPanels.cpp).
#include "UI/Kit.h"
#include "UI/UIToolkit.h"

namespace Kit {

// Built below the bottom of the screen. `left`: the tab at the window's left
// edge and the pane growing rightwards from it; otherwise both on the right.
void folder(Folder &F, const FolderSpec &spec) {
    const UIShape   &k = UI::shape();
    const UIMetrics &m = UI::met();
    const lv_area_t &W = *spec.win;
    const bool left = spec.left;
    const int32_t tabW = spec.tabW;
    const int32_t w  = lv_area_get_width(&W);
    const int32_t r  = UI::mm(k.R_PANEL);
    const int32_t bw = m.BORDER_W ? m.BORDER_W : 1;
    // The page deck's own header height (owner, round 9: its panels "are
    // already the perfect height to sit neatly under the bottom border of the
    // popup").
    F.head = UIToolkit::sc(UIToolkit::PANEL_HEADER_H);
    // A FOLDER TAB (owner, round 10). The pane has a top edge of its own, and
    // the tab rises from it, the join curving in like a file folder's - so an
    // open panel is closed off from the window's body.
    //
    // FOLDED, ONLY THE TAB SHOWS (owner, round 11): the strip above the screen's
    // edge is the page deck's header height, and the pane's edge and the curve
    // sit just below it. So the tab reaches a curve's height further down than
    // what shows. OPEN, the pane's own bottom edge stays below the screen too:
    // the panel is `hide` taller than what it uncovers.
    const int32_t rf       = r;                        // the inner curve
    const int32_t tabAbove = F.head + rf;              // the tab's part above the pane's edge
    F.hide = r + 2 * bw;
    int32_t gapTop = UI::mm(k.S5), gapBot = UI::mm(k.S5);   // above the first row, below the last
    F.h = tabAbove + (rf + gapTop) + spec.rows * UI::mm(k.H_ROW) + gapBot + F.hide;
    // OPENS NO TALLER THAN THE WINDOW (owner, 2026-10-07): open, the tab's top
    // rests at or below the window's top edge. More rows than that: the panel
    // is dragged (the caller's).
    const int32_t sh   = lv_obj_get_height(lv_screen_active());
    const int32_t maxH = sh - W.y1 + F.hide;
    // ONLY JUST TOO TALL: THE GAPS GIVE WAY, NOT A DRAG (owner, 2.10d P1b).
    // SETTINGS' six rows on the P4_5 were ~16 px over - every row in sight, a
    // drag only for the gap under the last one. Up to one gap's worth is
    // taken from the two gaps, half each.
    const int32_t over = F.h - maxH;
    if (over > 0 && over <= gapBot) {
        gapTop -= over / 2;
        gapBot -= over - over / 2;
        F.h = maxH;
    }

    // Built back to front: the pane, the tab over it, a block hiding the
    // tab's bottom where it overlaps the pane, and the inner curve. One colour
    // throughout - the tab used to be the card colour folded and the window's
    // open (owner, round 10). The scheme's lift on the pane and the tab: a
    // shadow on Linen, like the page deck's panels; nothing on the dark ones.
    const int32_t tabXw = left ? 0 : w - tabW;          // in the window's width

    // AS WIDE AS ITS LONGEST ROW (owner, 2026-10-07), its outer edge on the
    // window's, never narrower than its own tab. Within a curve's width of the
    // tab it is made the tab's width: the two then share one straight inner
    // edge, and there is no inner curve to draw.
    int32_t paneW = LV_CLAMP(tabW, spec.needW, w);
    const bool flush = paneW < tabW + 2 * r;
    if (flush) paneW = tabW;
    const int32_t paneXw = left ? 0 : w - paneW;

    // THE HOLDER IS ONLY AS WIDE AS THE TAB AND PANE (round 8, measured on the
    // P4_5: 35-105 ms a frame while sliding). Moving an object redraws its
    // whole area, and a holder as wide as the window, transparent, made LVGL
    // draw the cards, the window and the panel in every strip of every frame.
    // Sized to the panel, most strips lie inside the opaque pane, and LVGL
    // starts drawing there. Widened by the shadow's reach on Linen, so the
    // holder does not clip it.
    const int32_t reach = m.SHADOW ? UI::sc(m.SHADOW) + UI::sc(m.SHADOW_Y) + 2 : 0;
    const int32_t x0 = LV_MAX(0, LV_MIN(tabXw, paneXw) - reach);
    const int32_t x1 = LV_MIN(w, LV_MAX(tabXw + tabW, paneXw + paneW) + reach);
    const int32_t tabX  = tabXw - x0;                   // from here on, in the holder
    const int32_t paneX = paneXw - x0;

    // THE TRACK the holder is dragged in: as wide as the holder, from the
    // screen's top to the hidden strip under its bottom. Not clickable, so a
    // press beside the panel goes through it. Scrolling is switched on only
    // while the panel rests open (the caller's).
    F.track = plain(lv_screen_active());
    lv_obj_set_pos (F.track, W.x1 + x0, 0);
    lv_obj_set_size(F.track, x1 - x0, sh + F.hide);
    lv_obj_set_scroll_dir(F.track, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(F.track, LV_SCROLLBAR_MODE_OFF);
    lv_obj_clear_flag(F.track, LV_OBJ_FLAG_SCROLL_ELASTIC);
    lv_obj_clear_flag(F.track, LV_OBJ_FLAG_SCROLL_MOMENTUM);
    lv_obj_clear_flag(F.track, LV_OBJ_FLAG_SCROLL_CHAIN);   // nothing passed on to what is behind
    if (spec.trackCb) {
        lv_obj_add_event_cb(F.track, spec.trackCb, LV_EVENT_SCROLL, spec.trackUser);
        lv_obj_add_event_cb(F.track, spec.trackCb, LV_EVENT_SCROLL_END, spec.trackUser);
    }

    F.root = plain(F.track);
    lv_obj_set_pos (F.root, 0, sh);   // out of sight
    lv_obj_set_size(F.root, x1 - x0, F.h);

    // THE CARETS (owner): a chevron at the screen's bottom while rows are below
    // it, one at the top while the pane is off the top (`down` and `up` name
    // where they sit, not where they point). Floating, so they stay put while
    // the track scrolls; drawn over the pane, in its middle.
    {
        static lv_point_precise_t ptsDown[3], ptsUp[3];
        const int32_t cw = UI::mm(k.CARET_W), ch = UI::mm(k.CARET_H), lw = LV_MAX(2, UI::mm(k.LINE_PLOT)), o = lw / 2;
        ptsDown[0] = { o, o };      ptsDown[1] = { o + cw / 2, o + ch }; ptsDown[2] = { o + cw, o };
        ptsUp[0]   = { o, o + ch }; ptsUp[1]   = { o + cw / 2, o };      ptsUp[2]   = { o + cw, o + ch };
        auto caret = [&](const lv_point_precise_t *pts, int32_t y) {
            lv_obj_t *l = lv_line_create(F.track);
            lv_obj_remove_style_all(l);
            lv_line_set_points(l, pts, 3);
            lv_obj_set_size(l, cw + lw, ch + lw);
            lv_obj_set_pos (l, paneX + paneW / 2 - (cw + lw) / 2, y);
            add(l, KitStyle::KS_PANEL_CARET);
            lv_obj_add_flag  (l, LV_OBJ_FLAG_FLOATING);
            lv_obj_add_flag  (l, LV_OBJ_FLAG_HIDDEN);
            lv_obj_clear_flag(l, LV_OBJ_FLAG_CLICKABLE);
            return l;
        };
        // POINTING THE WAY TO DRAG (owner, P1b round 2): rows below the screen
        // show a caret pointing UP - "drag me up" - and a panel gone off the
        // top one pointing down.
        F.down = caret(ptsUp, sh - (ch + lw) - UI::mm(k.S4));
        F.up   = caret(ptsDown, UI::mm(k.S4));
    }

    // FLUSH: ONE PIECE (owner, 2026-10-07: a break in the left edge under the
    // title on the P4_5 and the 1060, a line under it on Linen). A tab exactly
    // as wide as its pane met it corner to rounded corner. So the pane starts
    // at the top and is the tab's shape itself; the tab below is only its
    // label and its touch area.
    const int32_t paneY = flush ? 0 : tabAbove;
    lv_obj_t *pane = plain(F.root);
    F.pane = pane;
    lv_obj_set_pos (pane, paneX, paneY);
    lv_obj_set_size(pane, paneW, F.h - paneY);
    add(pane, KitStyle::KS_PANEL_FACE);
    lv_obj_add_style(pane, UI::paint(UIPaint::PAINT_LIFT), 0);
    // The pane's padding keeps its rows below the tab's join. Its right margin
    // is the body's own (below).
    lv_obj_set_style_pad_left(pane, UI::mm(k.S7), 0);
    lv_obj_set_style_pad_top(pane, (tabAbove - paneY) + rf + gapTop, 0);
    lv_obj_set_style_pad_bottom(pane, gapBot + F.hide, 0);   // nothing in the part that stays hidden
    lv_obj_add_flag(pane, LV_OBJ_FLAG_CLICKABLE);   // a tap on the pane stays on the pane

    // The rows sit in a body of their own, which never scrolls: the whole
    // panel is dragged instead (P1b round 2). A press on a row or a gap lands
    // on the body; a drag from there is passed up to the track.
    F.body = plain(pane);
    add(F.body, KitStyle::KS_PANEL_BODY);
    lv_obj_add_flag(F.body, LV_OBJ_FLAG_CLICKABLE);

    // The tab: SETTINGS IN THE RIGHT HALF, ALWAYS (owner, round 9); a second
    // panel (CHART) takes the left half, so SETTINGS is always in the same
    // place. It reaches a radius past the pane's edge, its lower corners
    // hidden by the block below.
    F.tab = plain(F.root);
    lv_obj_set_pos (F.tab, tabX, 0);
    lv_obj_set_size(F.tab, tabW, tabAbove + r + bw);
    add(F.tab, KitStyle::KS_PANEL_FACE);
    lv_obj_add_style(F.tab, UI::paint(UIPaint::PAINT_LIFT), 0);
    lv_obj_add_flag(F.tab, LV_OBJ_FLAG_CLICKABLE);
    if (spec.tabCb) lv_obj_add_event_cb(F.tab, spec.tabCb, LV_EVENT_CLICKED, nullptr);
    F.tabLbl = label(F.tab, KitStyle::KS_PANEL_TITLE);
    lv_label_set_text(F.tabLbl, spec.title);
    // Where round 11 had it, which the owner called right.
    lv_obj_align(F.tabLbl, LV_ALIGN_TOP_MID, 0, (F.head - rf - lv_font_get_line_height(UI::type().TAG)) / 2);
    if (flush) {
        // The pane is the shape; the tab draws nothing of its own.
        lv_obj_remove_style(F.tab, UI::paint(UIPaint::PAINT_LIFT), 0);
        lv_obj_set_style_bg_opa      (F.tab, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(F.tab, 0, 0);
        return;
    }

    // The block: the tab's lower corners, bottom border and (on Linen) the
    // shadow it casts downward, all of which lie inside the pane - painted out
    // in the pane's colour. Kept one border-width clear of the pane's outer
    // edge, which carries on down past the tab.
    // AND THE SHADOW IT CASTS INWARDS: a shadow spreads sideways too, and
    // round the tab's inner lower corner it showed below the pane's edge as a
    // small grey crescent at the foot of the inner curve (owner, 2026-10-06 -
    // "thought it was crud on the glass"). The block reaches in by the
    // shadow's width; the curve's arcs are drawn over it.
    lv_obj_t *block = plain(F.root);
    lv_obj_set_pos (block, left ? tabX + bw : tabX - reach, tabAbove + bw);
    lv_obj_set_size(block, tabW - bw + reach, r + bw + reach);
    add(block, KitStyle::KS_PANEL_FILL);

    // THE SEAM: the tip of the tab's inner border, on the pane's edge line at
    // the foot of the curve. The curve's fill arc covers it only up to its own
    // anti-aliased rim, so a 3-pixel speck was left (owner, round 7: "now that
    // I know about it I can't ignore it"). Painted out here; the arcs below are
    // drawn after, so nothing of the curve is lost.
    lv_obj_t *seam = plain(F.root);
    lv_obj_set_pos (seam, left ? tabX + tabW - 3 * bw - 2 : tabX - 2, tabAbove - bw - 1);
    lv_obj_set_size(seam, 3 * bw + 4, 2 * bw + 2);
    add(seam, KitStyle::KS_PANEL_FILL);

    // The inner curve where the tab meets the pane's edge: a concave corner,
    // which LVGL has no shape for. Two quarter arcs centred a radius out from
    // the join: a thick one in the pane's colour fills the corner outside the
    // curve, and a border-width one draws the edge along it. Their spill onto
    // the tab and the pane is the same colour, so it does not show. Mirrored
    // for a tab on the left: the centre a radius to the tab's right, the
    // quarter from +y round to -x.
    const lv_point_t ctr = { left ? tabX + tabW + rf : tabX - rf, tabAbove - rf };
    const int32_t ro = (rf * 1415 + 999) / 1000 + 1;   // reaches the join, rf * sqrt(2)
    auto quarter = [&](int32_t outer, int32_t width, lv_color_t col) {
        lv_obj_t *a = lv_arc_create(F.root);
        lv_obj_remove_style_all(a);
        lv_obj_clear_flag(a, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_set_pos (a, ctr.x - outer, ctr.y - outer);
        lv_obj_set_size(a, 2 * outer, 2 * outer);
        if (left) lv_arc_set_bg_angles(a, 90, 180);   // from +y round to -x
        else      lv_arc_set_bg_angles(a, 0, 90);     // from +x round to +y: the join's side
        lv_obj_set_style_arc_width  (a, width, LV_PART_MAIN);
        lv_obj_set_style_arc_color  (a, col, LV_PART_MAIN);
        lv_obj_set_style_arc_opa    (a, LV_OPA_COVER, LV_PART_MAIN);
        lv_obj_set_style_arc_rounded(a, false, LV_PART_MAIN);
        lv_obj_set_style_arc_opa    (a, LV_OPA_TRANSP, LV_PART_INDICATOR);
    };
    quarter(ro, ro - rf, UI::c(UI::pal().SURFACE_ALT));
    quarter(rf + bw, bw, UI::border());
}

} // namespace Kit
