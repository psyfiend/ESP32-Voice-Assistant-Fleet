#pragma once
#include <lvgl.h>
#include <Arduino.h>
#include "GuiManager.h"

#define ROW_HEIGHT 50

// --- DPI SCALING CONFIG ---
#ifdef HIGH_DPI_DISPLAY
    #define UI_SCALE 1.5f 
#else
    #define UI_SCALE 1.0f
#endif

// Define a simple callback type for closing external panels
typedef void (*UiActionCallback)(void);

class UiToolkit {
public:
    // Scale a pixel value based on the target device
    static int32_t  sc(int32_t val);
    
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