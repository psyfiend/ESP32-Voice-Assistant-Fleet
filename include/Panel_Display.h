#pragma once
#include <lvgl.h>
#include "DisplayManager.h"
#include "TouchManager.h"
#include "UIToolkit.h"

class Panel_Display {
public:
    // Takes the two managers it actually uses rather than reaching through
    // GUIManager for them. Hardware is owned by SystemCore; this panel borrows.
    Panel_Display(DisplayManager& display, TouchManager& touch);
    void init(lv_obj_t* parent);
    void tick();

    // -- NEW: External Visibility Control --
    void setTouchWindowVisibility(bool visible);

private:
    DisplayManager& _display;
    TouchManager&   _touch;
    int32_t _briFloor; // Slider's real minimum (per-board artificial brightness floor) - the
                        // toast display remaps [_briFloor, 100] back to a user-facing [0, 100].
    lv_obj_t* slider_bri;
    lv_obj_t* pnl_content;
    lv_obj_t* row_bri;
    lv_obj_t* row_viz;
    lv_obj_t* col_bri;
    
    // Touch Viz
    bool showTouches;
    lv_obj_t *panel_touch_data;
    lv_obj_t *cursors[5];
    lv_obj_t *coord_labels[5];
    lv_obj_t *count_label;

    static void slider_bri_cb(lv_event_t * e);
    static void sw_touch_viz_cb(lv_event_t * e);
};