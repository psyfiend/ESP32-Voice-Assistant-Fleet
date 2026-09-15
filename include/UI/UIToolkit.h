#pragma once
#include <lvgl.h>
#include <Arduino.h>
#include "bsp_loader.h"

#define ROW_HEIGHT 50

// --- DPI SCALING ---
// UI_SCALE used to be a macro set by `-D HIGH_DPI_DISPLAY`: 1.5 on three
// boards, 1.0 on five. That split tracked real pixel density surprisingly well
// (the fleet falls into 165-187 PPI and 237-294 PPI clusters with a 50 PPI gap)
// but it rounded two boards to the wrong side - CYD_S3_8048 and, more
// importantly, the WS_P4_5 dev target. The scale is now DERIVED from
// bsp_display.DIAGONAL_IN via bspUiScale(). See docs/design/tokens.md.

// Define a simple callback type for closing external panels
typedef void (*UiActionCallback)(void);

class UIToolkit {
public:
    // Scale a logical pixel value to this board's pixel density.
    static int32_t  sc(int32_t val);

    // The derived values behind sc(), exposed for diagnostics and for LVGL's
    // own DPI setting. ppi() returns 0 if the board declares no DIAGONAL_IN.
    static float    scale() { return bspUiScale(); }
    static uint16_t ppi()   { return bspPixelDensity(); }
    
    // Global Init (Styles, Toast layer)
    static void     init();

    // Show a floating toast message
    static void     show_toast(const char* text, uint32_t duration_ms = 2000);

    // Widget Generators
    static lv_obj_t*    create_collapsible_panel(lv_obj_t* parent, const char* title, lv_obj_t** content_container);
    static lv_obj_t*    create_panel_row(lv_obj_t* pnl_content, lv_obj_t** row_container);
    static lv_obj_t*    create_slider_col(lv_obj_t* parent, const char* title, lv_obj_t** out_col, lv_obj_t** out_slider);
    static lv_obj_t*    create_header_label(lv_obj_t* parent, const char* text);

        // --= NEW: Helper to force close bottom panels =--
    static void         closeActiveAccordion(); 
    static lv_obj_t*    getActiveAccordionPanel();

    // -- NEW: Register a callback to close the System Panel --
    static void         registerSystemCloseCb(UiActionCallback cb);

    // --= SEMANTIC FONTS =--
    // Defined by function rather than size
    static const lv_font_t* Font_Caption;     // Tiny details, coords
    static const lv_font_t* Font_Label;       // Slider titles, list items
    static const lv_font_t* Font_Button;      // Action buttons
    static const lv_font_t* Font_PanelHeader; // Accordion titles
    static const lv_font_t* Font_Hero;        // Big status numbers
};