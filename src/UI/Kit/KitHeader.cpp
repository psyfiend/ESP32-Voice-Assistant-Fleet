// The page header's pieces (2.8): the bar, a slot, a glyph's frame. Their looks
// are the KS_HDR_* and KS_GLYPH_* roles (KitStyles.cpp); what goes in a slot is
// Panel_Header's, by its kind (UI/HeaderSlots.h).
#include "UI/Kit.h"
#include "UI/UIToolkit.h"

namespace Kit {

lv_obj_t *headerBar(lv_obj_t *parent) {
    lv_obj_t *b = plain(parent);
    add(b, KitStyle::KS_HDR_BAR);
    // The scheme's drop shadow onto the page below; nothing on a scheme
    // without one (the owner's Linen request, 2026-09-23).
    lv_obj_add_style(b, UI::paint(UIPaint::PAINT_LIFT), 0);
    lv_obj_set_size(b, lv_pct(100), UIToolkit::systemHeaderFullPx());
    return b;
}

lv_obj_t *headerSlot(lv_obj_t *bar, lv_event_cb_t cb, void *user) {
    lv_obj_t *s = plain(bar);
    add(s, KitStyle::KS_HDR_SLOT);
    add(s, KitStyle::KS_HDR_SLOT_PRESSED, LV_PART_MAIN | LV_STATE_PRESSED);
    add(s, KitStyle::KS_HDR_SLOT_OPEN,    LV_PART_MAIN | LV_STATE_CHECKED);
    // The bar's full height and FRAME_R more, clipped by the bar - so an open
    // slot's rounded corners show at its top only, and its face covers the
    // bar's accent line (D-23); KS_HDR_SLOT's padding below keeps what it holds
    // in the bar. Its width and x are the header plan's (Layout/HeaderPlan.h).
    lv_obj_set_height(s, UIToolkit::systemHeaderFullPx() + UI::sc(UI::hdr().FRAME_R));
    lv_obj_add_flag(s, LV_OBJ_FLAG_CLICKABLE);
    if (cb) lv_obj_add_event_cb(s, cb, LV_EVENT_CLICKED, user);
    return s;
}

lv_obj_t *glyphFrame(lv_obj_t *parent, GlyphStyle style) {
    lv_obj_t *f = plain(parent);
    switch (style) {
        case GlyphStyle::GLYPH_SQUARE: add(f, KitStyle::KS_GLYPH_SQUARE); break;
        case GlyphStyle::GLYPH_CIRCLE: add(f, KitStyle::KS_GLYPH_CIRCLE); break;
        default:
            // Bare: a holder the glyph's size (UIHeader GLYPH), so a tap falls
            // through to the slot.
            lv_obj_set_size(f, UI::sc(UI::hdr().GLYPH), UI::sc(UI::hdr().GLYPH));
            lv_obj_set_flex_flow(f, LV_FLEX_FLOW_ROW);
            lv_obj_set_flex_align(f, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
            break;
    }
    return f;
}

} // namespace Kit
