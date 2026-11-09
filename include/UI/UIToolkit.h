#pragma once
#include <lvgl.h>
#include <Arduino.h>
#include "bsp_loader.h"

#define ROW_HEIGHT 50

// --- DPI SCALING ---
// The scale is DERIVED from bsp_display.DIAGONAL_IN via bspUiScale(): the fleet
// falls into 165-187 PPI and 237-294 PPI clusters with a 50 PPI gap, and a
// blanket two-value split put CYD_S3_8048 and WS_P4_5 on the wrong side. See
// docs/design/tokens.md.

// Define a simple callback type for closing external panels
typedef void (*UiActionCallback)(void);

class UIToolkit {
public:
    // Scale a logical pixel value to this board's pixel density.
    static int32_t  sc(int32_t val);

    // THE SYSTEM HEADER BAR's height, in logical px: SYSTEM_HEADER_FULL_H while
    // the header shows, 0 while it is hidden (GUIManager sets it).
    //
    // THE DECK PANELS DELIBERATELY HANG OFF THE BOTTOM OF THE SCREEN.
    //
    // A collapsed panel is taller than the strip you can see, and the part
    // below the screen edge is the point: the sides of the visible header run
    // straight off the bottom, so it reads as a large panel full of content
    // waiting below rather than as a floating button.     //
    // The visible strip is therefore COLLAPSED minus the overhang, and it always
    // equals PANEL_HEADER_H. These are named so the relationship is stated
    // rather than re-derived.
    static constexpr int32_t PANEL_COLLAPSED_H = 85;   // logical px
    static constexpr int32_t PANEL_HEADER_H    = 45;   // the visible strip
    static constexpr int32_t PANEL_EXPANDED_H  = 280;  // an open panel, overhang included
    static constexpr int32_t DECK_PAD          = 10;

    // How far past the bottom of the screen the deck must extend for exactly
    // PANEL_HEADER_H of each panel to show.
    static int32_t      deckOverhangPx() {
        return sc(PANEL_COLLAPSED_H - PANEL_HEADER_H) + sc(DECK_PAD);
    }

    static uint8_t      systemHeaderH;
    static int32_t      systemHeaderPx() { return systemHeaderH ? sc(systemHeaderH) : 0; }
    // Its height when shown. For anything that must not change size when the
    // header is hidden - the card popup's width is worked out from it
    // (CardPopup::open()).
    static constexpr uint8_t SYSTEM_HEADER_FULL_H = 35;
    static int32_t      systemHeaderFullPx() { return sc(SYSTEM_HEADER_FULL_H); }

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
    // A NOTICE (K142; NINA's toasts): queued rather than replacing what
    // shows - each for its time, a bar running down under its words, the next
    // after it. Info notices only until 2.14's alerts join the queue. A readout
    // (show_toast) still shows at once; the queue carries on after it.
    static void     show_notice(const char* text, uint32_t duration_ms = 5000);
    // A toast that stays until it is tapped (K176).
    static void     show_sticky(const char* text);
    // A toast held while something works, gone when end_held() says it is done
    // ("Optimizing page view..." while the arranger runs). Not
    // over a sticky one, which waits for its tap; the queue carries on after.
    static void     show_held(const char* text);
    static void     end_held();

    // Widget Generators
    static lv_obj_t*    create_collapsible_panel(lv_obj_t* parent, const char* title, lv_obj_t** content_container);
    static lv_obj_t*    create_panel_row(lv_obj_t* pnl_content, lv_obj_t** row_container);
    static lv_obj_t*    create_slider_col(lv_obj_t* parent, const char* title, lv_obj_t** out_col, lv_obj_t** out_slider);
    static lv_obj_t*    create_header_label(lv_obj_t* parent, const char* text);

        // --= Helper to force close bottom panels =--
    static void         closeActiveAccordion(); 
    static lv_obj_t*    getActiveAccordionPanel();

    // -- Register a callback to close the System Panel --
    static void         registerSystemCloseCb(UiActionCallback cb);
    // Called with true when a deck panel opens, false when none is open.
    static void         registerAccordionChangeCb(void (*cb)(bool open));

    // --= SEMANTIC FONTS =--
    // Defined by function rather than size
    static const lv_font_t* Font_Caption;     // Tiny details, coords
    static const lv_font_t* Font_Label;       // Slider titles, list items
    static const lv_font_t* Font_Button;      // Action buttons
    static const lv_font_t* Font_PanelHeader; // Accordion titles
    static const lv_font_t* Font_Hero;        // Big status numbers
    // THE HEADER BAR'S WORDS (D-26): the larger of the
    // type scale's HEADER face (TAG's, millimetres per board; the P4_5 its own,
    // 26) and Font_PanelHeader - so no board's bar ever shrinks.
    static const lv_font_t* Font_Header;
};