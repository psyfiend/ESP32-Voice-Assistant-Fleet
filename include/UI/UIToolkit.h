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

    // THE SYSTEM HEADER BAR's height, in logical px, and settable.
    //
    // It was UIToolkit::sc(50) in three places. The owner, looking at a 1280x800
    // panel full of short cards: "the height of the header becomes more
    // pronounced... it really doesn't need to be just for some icons, clock,
    // and a title." It is a preference, so it is a value rather than a
    // constant, and the System panel cycles it. 0 means no header at all.
    // THE DECK PANELS DELIBERATELY HANG OFF THE BOTTOM OF THE SCREEN.
    //
    // A collapsed panel is taller than the strip you can see, and the part
    // below the screen edge is the point: the sides of the visible header run
    // straight off the bottom, so it reads as a large panel full of content
    // waiting below rather than as a floating button. The owner: "this defeats
    // the whole point of the effect."
    //
    // The visible strip is therefore COLLAPSED minus the overhang, and it
    // should always equal PANEL_HEADER_H. That used to hold by arithmetic
    // accident - 85 + 10 - 50 = 45 - which is why shrinking the system header
    // to 35 made 60 px visible and left a gap under the title. These are named
    // so the relationship is stated rather than re-derived.
    static constexpr int32_t PANEL_COLLAPSED_H = 85;   // logical px
    static constexpr int32_t PANEL_HEADER_H    = 45;   // the visible strip
    static constexpr int32_t DECK_PAD          = 10;

    // How far past the bottom of the screen the deck must extend for exactly
    // PANEL_HEADER_H of each panel to show.
    static int32_t      deckOverhangPx() {
        return sc(PANEL_COLLAPSED_H - PANEL_HEADER_H) + sc(DECK_PAD);
    }

    static uint8_t      systemHeaderH;
    static int32_t      systemHeaderPx() { return systemHeaderH ? sc(systemHeaderH) : 0; }

    // The derived values behind sc(), exposed for diagnostics and for LVGL's
    // own DPI setting. ppi() returns 0 if the board declares no DIAGONAL_IN.
    static float    scale() { return bspUiScale(); }
    static uint16_t ppi()   { return bspPixelDensity(); }
    
    // Global Init (Styles, Toast layer)
    static void     init();

    // Show a floating toast message
    // `widthOf`, when given, fixes the toast's width to that string's and
    // left-aligns the text, so a readout that repeats while a slider moves
    // stays still. Pass the WIDEST value it can show, e.g. "Volume: 100%".
    static void     show_toast(const char* text, uint32_t duration_ms = 2000,
                               const char* widthOf = nullptr);

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