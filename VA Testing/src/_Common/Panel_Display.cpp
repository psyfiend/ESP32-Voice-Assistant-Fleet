#include "Panel_Display.h"

Panel_Display::Panel_Display(GuiManager& gui) : _gui(gui) {
    showTouches = false;
}

// --= DRAG HANDLER for Touch Point Window =--
static void drag_event_handler(lv_event_t * e) {
    lv_obj_t * obj = (lv_obj_t*)lv_event_get_target(e);
    lv_indev_t * indev = lv_indev_get_act();
    if(indev == NULL) return;
    
    lv_point_t vect;
    lv_indev_get_vect(indev, &vect);

    // Calculate new position based on drag vector
    int32_t x = lv_obj_get_style_x(obj, LV_PART_MAIN) + vect.x;
    int32_t y = lv_obj_get_style_y(obj, LV_PART_MAIN) + vect.y;

    lv_obj_set_pos(obj, x, y);
}

void Panel_Display::slider_bri_cb(lv_event_t * e) {
    Panel_Display* pThis = (Panel_Display*)lv_event_get_user_data(e);
    lv_obj_t * slider = (lv_obj_t*)lv_event_get_target(e);
    int32_t val = lv_slider_get_value(slider);
    
    pThis->_gui.displayMgr.setBrightness(val);

    char buf[32];
    snprintf(buf, sizeof(buf), "Brightness: %d%%", (int)val);
    UiToolkit::show_toast(buf, 1000);
}

void Panel_Display::sw_touch_viz_cb(lv_event_t * e) {
    Panel_Display* pThis = (Panel_Display*)lv_event_get_user_data(e);
    bool active = lv_obj_has_state((lv_obj_t*)lv_event_get_target(e), LV_STATE_CHECKED);
    pThis->showTouches = active;

    if (active) lv_obj_clear_flag(pThis->panel_touch_data, LV_OBJ_FLAG_HIDDEN);
    else {
        lv_obj_add_flag(pThis->panel_touch_data, LV_OBJ_FLAG_HIDDEN);
        for(int i=0; i<5; i++) lv_obj_add_flag(pThis->cursors[i], LV_OBJ_FLAG_HIDDEN);
    }
    // Use the new helper to enforce state
    pThis->setTouchWindowVisibility(active);
}

// -- NEW: Implementation --
void Panel_Display::setTouchWindowVisibility(bool visible) {
    // Only show if the master switch 'showTouches' is explicitly ON
    bool actual_state = visible && showTouches;
    
    if (actual_state) {
        lv_obj_clear_flag(panel_touch_data, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(panel_touch_data, LV_OBJ_FLAG_HIDDEN);
        // Also hide cursors
        for(int i=0; i<5; i++) lv_obj_add_flag(cursors[i], LV_OBJ_FLAG_HIDDEN);
    }
}

void Panel_Display::init(lv_obj_t* parent) {
    
    UiToolkit::create_collapsible_panel(parent, "DISPLAY", &pnl_content);

    // ROW 1 - Brightness
    UiToolkit::create_panel_row(pnl_content, &row_bri);

    // ROW 1 COL 1 - Brightness
    // Column container with 2 rows: Label + Slider
    UiToolkit::create_slider_col(row_bri, "BRIGHTNESS", &col_bri, &slider_bri);

    lv_slider_set_value         (slider_bri, _gui.displayMgr.getBrightness(), LV_ANIM_OFF);
    lv_slider_set_range         (slider_bri, 2, 100);
    lv_obj_add_event_cb         (slider_bri, slider_bri_cb, LV_EVENT_VALUE_CHANGED, this);

    // ROW 2 - Touch Visualization Switch
    UiToolkit::create_panel_row (pnl_content, &row_viz);
    lv_obj_set_flex_align       (row_viz, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);  

    // ROW 2,1 - Label
    lv_obj_t * lbl_viz = lv_label_create(row_viz);
    lv_label_set_text           (lbl_viz, "Show Touches");
    lv_obj_set_style_text_color (lbl_viz, lv_color_white(), 0);
    lv_obj_set_style_text_font  (lbl_viz, UiToolkit::Font_Label, 0); // Semantic Font

    // ROW 2,2 - Switch
    lv_obj_t * sw_viz = lv_switch_create(row_viz);
    lv_obj_set_size     (sw_viz, UiToolkit::sc(60), UiToolkit::sc(25));  // To match enable switch
    lv_obj_set_align    (sw_viz, LV_ALIGN_CENTER);
    lv_obj_add_event_cb (sw_viz, sw_touch_viz_cb, LV_EVENT_VALUE_CHANGED, this);


    // Touch Overlay Logic
    panel_touch_data = lv_obj_create(lv_screen_active());
    lv_obj_set_size             (panel_touch_data, UiToolkit::sc(180), UiToolkit::sc(210)); 
    lv_obj_align                (panel_touch_data, LV_ALIGN_RIGHT_MID, UiToolkit::sc(-10), UiToolkit::sc(-50));
    lv_obj_set_style_bg_color   (panel_touch_data, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa     (panel_touch_data, LV_OPA_80, 0); 
    lv_obj_set_style_border_width(panel_touch_data, 2, 0);
    lv_obj_set_style_border_color(panel_touch_data, lv_color_hex(0xFFFFFF), 0); 
    lv_obj_add_flag             (panel_touch_data, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag           (panel_touch_data, LV_OBJ_FLAG_SCROLLABLE);
    
    // Move Z behind panels but above header bar
    lv_obj_move_to_index    (panel_touch_data, 0); // Order described in LVGL_Test_UI init()
    lv_obj_add_flag         (panel_touch_data, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb     (panel_touch_data, drag_event_handler, LV_EVENT_PRESSING, NULL);

    lv_obj_t * lbl_h = lv_label_create(panel_touch_data);
    lv_label_set_text           (lbl_h, "TOUCH POINTS");
    lv_obj_align                (lbl_h, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_text_font  (lbl_h, UiToolkit::Font_PanelHeader, 0); // Semantic Font
    lv_obj_set_style_text_color (lbl_h, lv_color_hex(0x00A8FF), 0);

    count_label = lv_label_create(panel_touch_data);
    lv_label_set_text           (count_label, "ACTIVE: 0");
    lv_obj_align                (count_label, LV_ALIGN_BOTTOM_MID, 0, UiToolkit::sc(7));
    lv_obj_set_style_text_font  (count_label, UiToolkit::Font_Hero, 0); // Semantic Font
    lv_obj_set_style_text_color (count_label, lv_color_hex(0x404040), 0); 

    for(int i=0; i<5; i++) {
        coord_labels[i] = lv_label_create(panel_touch_data);
        lv_label_set_text_fmt       (coord_labels[i], "ID%d: --", i);
        lv_obj_align                (coord_labels[i], LV_ALIGN_TOP_LEFT, UiToolkit::sc(25), UiToolkit::sc(25 + (i * 25)));
        lv_obj_set_style_text_color (coord_labels[i], lv_color_hex(0x808080), 0); 
        lv_obj_set_style_text_font  (coord_labels[i], UiToolkit::Font_Caption, 0); // Semantic Font

        cursors[i] = lv_obj_create(lv_screen_active());
        lv_obj_set_size            (cursors[i], UiToolkit::sc(60), UiToolkit::sc(60));
        lv_obj_set_style_radius    (cursors[i], LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_bg_opa    (cursors[i], LV_OPA_0, 0); 
        lv_obj_set_style_border_width(cursors[i], 3, 0);
        lv_obj_clear_flag          (cursors[i], LV_OBJ_FLAG_CLICKABLE); 
        lv_obj_add_flag            (cursors[i], LV_OBJ_FLAG_HIDDEN); 
    }
}

void Panel_Display::tick() {
    if (!showTouches) return;

    TouchPoint points[5];
    uint8_t count = _gui.touchMgr.read(points, 5);

    // Turns "ACTIVE:" bright when touches are present
    if (count > 0)  lv_obj_set_style_text_color(count_label, lv_color_hex(0xFFFFFF), 0); 
    else            lv_obj_set_style_text_color(count_label, lv_color_hex(0x404040), 0); 
    
    lv_label_set_text_fmt(count_label, "ACTIVE: %d", count);

    for(int i=0; i<5; i++) {
        if(i < count) {
            lv_obj_clear_flag       (cursors[i], LV_OBJ_FLAG_HIDDEN);
            lv_obj_set_pos          (cursors[i], points[i].x - UiToolkit::sc(30), points[i].y - UiToolkit::sc(30));
            lv_label_set_text_fmt   (coord_labels[i], "ID%d: %d,%d", points[i].id, points[i].x, points[i].y);
            
            // Color Logic
            lv_color_t c = lv_palette_main  ((lv_palette_t)(i % 5)); // Cycle colors
            lv_obj_set_style_border_color   (cursors[i], c, 0);
            lv_obj_set_style_text_color     (coord_labels[i], c, 0);
        } else {
            lv_obj_add_flag            (cursors[i], LV_OBJ_FLAG_HIDDEN);
            lv_label_set_text_fmt      (coord_labels[i], "ID%d: --", i);
            lv_obj_set_style_text_color(coord_labels[i], lv_color_hex(0x404040), 0);
        }
    }
}