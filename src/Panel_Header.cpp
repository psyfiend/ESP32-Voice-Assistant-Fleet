#include "Panel_Header.h"
#include "UIToolkit.h"  // Semantic fonts
#include "ConnectivityManager.h"

Panel_Header::Panel_Header() {
    lbl_title  = NULL;
    btn_status = NULL;
}

void Panel_Header::init(lv_obj_t* parent, const char* title, ConnectivityManager* conn) {
    // Top Bar Container
    container = lv_obj_create(parent);
    lv_obj_set_size             (container, lv_pct(100), UiToolkit::sc(50));
    lv_obj_set_style_bg_color   (container, lv_color_hex(0x202020), 0);

    // Bottom Border Only (Blue Line)
    lv_obj_set_style_border_width(container, 0, 0);
    lv_obj_set_style_border_width(container, UiToolkit::sc(2), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_side (container, LV_BORDER_SIDE_BOTTOM, 0);
    lv_obj_set_style_border_color(container, lv_color_hex(0x00A8FF), 0); // Cyan Blue

    lv_obj_set_style_radius     (container, 0, 0);
    lv_obj_set_style_pad_all    (container, 0, 0); // Remove padding so button hits edge
    lv_obj_set_style_pad_left   (container, UiToolkit::sc(15), 0); // Restore left pad for title
    lv_obj_set_style_pad_right  (container, UiToolkit::sc(5), 0); // Small pad for button

    lv_obj_set_flex_flow        (container, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align       (container, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_clear_flag           (container, LV_OBJ_FLAG_SCROLLABLE);

    // Title Label
    lbl_title = lv_label_create(container);
    lv_label_set_text           (lbl_title, title);
    lv_obj_set_style_text_font  (lbl_title, UiToolkit::Font_PanelHeader, 0); // Semantic Font
    lv_obj_set_style_text_color (lbl_title, lv_color_hex(0x00A8FF), 0);
    
    // Status Button Wrapper (Touch Hotspot) --
    btn_status = lv_obj_create(container);
    lv_obj_set_size             (btn_status, UiToolkit::sc(80), lv_pct(100)); // Wide touch target
    lv_obj_set_style_bg_opa     (btn_status, LV_OPA_TRANSP, 0); // Invisible
    lv_obj_set_style_border_width(btn_status, 0, 0);
    lv_obj_set_style_pad_all    (btn_status, 0, 0);

    // Enable Clicking on the wrapper
    lv_obj_add_flag             (btn_status, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag           (btn_status, LV_OBJ_FLAG_SCROLLABLE);

    // Live connectivity glyph, inside the existing touch hotspot. Replaces the
    // old always-green LV_SYMBOL_WIFI label, which reported nothing.
    if (conn) {
        connStatus.init(btn_status, conn);
        lv_obj_center(connStatus.getRoot());
    }
}

void Panel_Header::tick() {
    // Self-throttling and change-guarded internally, so calling this every
    // loop costs a millis() compare in the common case.
    connStatus.tick();
}