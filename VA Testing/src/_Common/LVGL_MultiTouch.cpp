#ifndef BOARD_HAS_PSRAM
#error "Error: This program requires PSRAM enabled, please enable PSRAM option in 'Tools' menu of Arduino IDE"
#endif

#include <Arduino.h>
#include <Arduino_GFX_Library.h>
#include <lvgl.h>
#include "DisplayManager.h"
#include "TouchManager.h"

// --- Global Objects ---
DisplayManager displayMgr;
TouchManager touchMgr;

// --- LVGL Globals ---
static uint16_t *lv_draw_buf;
#define LV_BUF_SIZE (cfg.WIDTH * cfg.HEIGHT / 10)

// --- Dashboard Globals ---
lv_obj_t *cursors[5];       // The colored circles
lv_obj_t *coord_labels[5];  // The text X,Y readouts
lv_obj_t *count_label;      // "Active Touches: X"

// --- State Management ---
// We no longer need manual debounce variables here!
TouchPoint activePoints[5]; 
uint8_t activeTouchCount = 0;

// -------------------------------------------------------------------------
// LVGL Standard Flushing & Input
// -------------------------------------------------------------------------
void my_disp_flush(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map) {
    uint32_t w = (area->x2 - area->x1 + 1);
    uint32_t h = (area->y2 - area->y1 + 1);

    Arduino_GFX *gfx = displayMgr.getGfx();
    gfx->draw16bitRGBBitmap(area->x1, area->y1, (uint16_t *)px_map, w, h);
    gfx->flush();
    lv_display_flush_ready(disp);
}

void my_touch_read(lv_indev_t *indev, lv_indev_data_t *data) {
    // Simply check the global count populated by our library
    if (activeTouchCount > 0) {
        data->state = LV_INDEV_STATE_PRESSED;
        // Map the first touch point to the mouse cursor for LVGL interaction
        data->point.x = activePoints[0].x;
        data->point.y = activePoints[0].y;
    } else {
        data->state = LV_INDEV_STATE_RELEASED;
    }
}

uint32_t my_tick_get_cb(void) { return millis(); }

// -------------------------------------------------------------------------
// UI Callbacks
// -------------------------------------------------------------------------
static void slider_event_cb(lv_event_t * e) {
    lv_obj_t * slider = (lv_obj_t*)lv_event_get_target(e);
    displayMgr.setBrightness(lv_slider_get_value(slider));
}

static void btn_audio_cb(lv_event_t * e) {
    Serial.println("[Test] Audio Button Clicked");
    lv_obj_t * btn = (lv_obj_t*)lv_event_get_target(e);
    
    // Toggle color for visual feedback (Latching switch logic)
    static bool toggle = false;
    toggle = !toggle;
    
    if(toggle) lv_obj_set_style_bg_color(btn, lv_color_hex(0x00A8FF), 0); // Cyan
    else lv_obj_set_style_bg_color(btn, lv_color_hex(0x404040), 0);       // Grey
}

static void btn_sd_cb(lv_event_t * e) {
    Serial.println("[Test] SD Button Clicked");
    lv_obj_t * btn = (lv_obj_t*)lv_event_get_target(e);
    lv_obj_set_style_bg_color(btn, lv_color_hex(0x00FF00), 0); // Turn Green on click
}

// -------------------------------------------------------------------------
// SETUP
// -------------------------------------------------------------------------
void setup() {
    Serial.begin(115200);
    Serial.println("--- Fleet Hardware Dashboard (Final Test) ---");

    // 1. Hardware Init
    if (!displayMgr.begin()) {
        Serial.println("Display Fail! Halting.");
        while(1) delay(100);
    }
    
    // Pass Strategy A/B handled internally by TouchManager
    if (!touchMgr.begin()) {
        Serial.println("Touch Fail! (Continuing anyway...)");
    }

    // 2. LVGL Init
    lv_init();
    lv_tick_set_cb(my_tick_get_cb);

    Arduino_GFX *gfx = displayMgr.getGfx();
    lv_display_t *disp = lv_display_create(gfx->width(), gfx->height());
    lv_display_set_flush_cb(disp, my_disp_flush);
    
    lv_draw_buf = (uint16_t*)heap_caps_malloc(LV_BUF_SIZE * sizeof(uint16_t), MALLOC_CAP_SPIRAM);
    if (!lv_draw_buf) {
        lv_draw_buf = (uint16_t*)malloc(LV_BUF_SIZE * sizeof(uint16_t));
    }
    lv_display_set_buffers(disp, lv_draw_buf, NULL, LV_BUF_SIZE * sizeof(uint16_t), LV_DISPLAY_RENDER_MODE_PARTIAL);

    lv_indev_t *indev = lv_indev_create();
    lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(indev, my_touch_read);

    // ---------------------------------------------------------------------
    // UI Construction
    // ---------------------------------------------------------------------
    lv_obj_set_style_bg_color(lv_screen_active(), lv_color_hex(0x101010), LV_PART_MAIN);

    // --- Header ---
    lv_obj_t * top_bar = lv_obj_create(lv_screen_active());
    lv_obj_set_size(top_bar, lv_pct(100), 70); 
    lv_obj_set_style_bg_color(top_bar, lv_color_hex(0x202020), 0);
    lv_obj_set_style_border_side(top_bar, LV_BORDER_SIDE_BOTTOM, 0);
    lv_obj_set_style_border_width(top_bar, 2, 0);
    lv_obj_set_style_border_color(top_bar, lv_color_hex(0x00A8FF), 0);
    lv_obj_align(top_bar, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_clear_flag(top_bar, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t * title = lv_label_create(top_bar);
    lv_label_set_text(title, cfg.device_name); 
    lv_obj_align(title, LV_ALIGN_LEFT_MID, 5, 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(title, lv_color_hex(0x00A8FF), 0); 
    
    // --- Specs Overlay ---
    lv_obj_t * specs_panel = lv_obj_create(lv_screen_active());
    lv_obj_set_size(specs_panel, lv_pct(40), LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(specs_panel, LV_OPA_0, 0);
    lv_obj_set_style_border_width(specs_panel, 0, 0);
    lv_obj_align(specs_panel, LV_ALIGN_TOP_LEFT, 10, 80);

    lv_obj_t * lcd_info = lv_label_create(specs_panel);
    lv_label_set_text_fmt(lcd_info, "LCD: %s", cfg.LCD_MODEL);
    lv_obj_set_style_text_color(lcd_info, lv_color_hex(0xAAAAAA), 0);
    
    lv_obj_t * tp_info = lv_label_create(specs_panel);
    lv_label_set_text_fmt(tp_info, "Touch: %s", cfg.TP_NAME);
    lv_obj_align_to(tp_info, lcd_info, LV_ALIGN_OUT_BOTTOM_LEFT, 0, 5);
    lv_obj_set_style_text_color(tp_info, lv_color_hex(0xAAAAAA), 0);

    // --- Right Panel (Data) ---
    lv_obj_t * panel_data = lv_obj_create(lv_screen_active());
    int p_width = (cfg.WIDTH < 500) ? 45 : 25;
    lv_obj_set_size(panel_data, lv_pct(p_width), lv_pct(60));
    lv_obj_align(panel_data, LV_ALIGN_RIGHT_MID, -10, -20);
    lv_obj_set_style_bg_color(panel_data, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(panel_data, LV_OPA_60, 0);
    lv_obj_set_style_border_width(panel_data, 1, 0);
    lv_obj_set_style_border_color(panel_data, lv_color_hex(0x404040), 0);

    lv_obj_t * lbl_header = lv_label_create(panel_data);
    lv_label_set_text(lbl_header, "POINTS");
    lv_obj_align(lbl_header, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_text_color(lbl_header, lv_color_hex(0xAAAAAA), 0);
    lv_obj_set_style_text_font(lbl_header, &lv_font_montserrat_12, 0);

    for(int i=0; i<5; i++) {
        coord_labels[i] = lv_label_create(panel_data);
        lv_label_set_text_fmt(coord_labels[i], "ID%d: --", i);
        lv_obj_align(coord_labels[i], LV_ALIGN_TOP_LEFT, 0, 30 + (i * 25));
        lv_obj_set_style_text_font(coord_labels[i], &lv_font_montserrat_14, 0);
        lv_obj_set_style_text_color(coord_labels[i], lv_color_hex(0x404040), 0); 
    }

    count_label = lv_label_create(panel_data);
    lv_label_set_text(count_label, "Active: 0");
    lv_obj_align(count_label, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_text_font(count_label, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(count_label, lv_color_hex(0xFFFFFF), 0);

    // --- Control Deck ---
    lv_obj_t * deck = lv_obj_create(lv_screen_active());
    int deck_w = 100 - p_width - 10;
    lv_obj_set_size(deck, lv_pct(deck_w), 130);
    lv_obj_align(deck, LV_ALIGN_BOTTOM_LEFT, 10, -10);
    lv_obj_set_style_bg_color(deck, lv_color_hex(0x181818), 0);
    lv_obj_set_style_radius(deck, 12, 0);
    lv_obj_clear_flag(deck, LV_OBJ_FLAG_SCROLLABLE);
    
    // Brightness
    lv_obj_t * slider = lv_slider_create(deck);
    lv_obj_set_size(slider, lv_pct(95), 10);
    lv_obj_align(slider, LV_ALIGN_TOP_MID, 0, 25);
    lv_slider_set_range(slider, 5, 100); // Expanded range for byte
    lv_slider_set_value(slider, displayMgr.getBrightness(), LV_ANIM_OFF); 
    lv_obj_add_event_cb(slider, slider_event_cb, LV_EVENT_VALUE_CHANGED, NULL);
    
    lv_obj_t * lbl_bri = lv_label_create(deck);
    lv_label_set_text(lbl_bri, "Brightness");
    lv_obj_align(lbl_bri, LV_ALIGN_TOP_LEFT, 2, 2);
    lv_obj_set_style_text_font(lbl_bri, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(lbl_bri, lv_color_hex(0xCCCCCC), 0);

    // Buttons
    lv_obj_t * btn_audio = lv_button_create(deck);
    lv_obj_set_size(btn_audio, lv_pct(45), 40);
    lv_obj_align(btn_audio, LV_ALIGN_BOTTOM_LEFT, 0, 0);
    lv_obj_set_style_bg_color(btn_audio, lv_color_hex(0x404040), 0);
    lv_obj_add_event_cb(btn_audio, btn_audio_cb, LV_EVENT_CLICKED, NULL);
    
    lv_obj_t * lbl_audio = lv_label_create(btn_audio);
    lv_label_set_text(lbl_audio, "AUDIO TEST");
    lv_obj_center(lbl_audio);

    lv_obj_t * btn_sd = lv_button_create(deck);
    lv_obj_set_size(btn_sd, lv_pct(45), 40);
    lv_obj_align(btn_sd, LV_ALIGN_BOTTOM_RIGHT, 0, 0);
    lv_obj_set_style_bg_color(btn_sd, lv_color_hex(0x404040), 0);
    lv_obj_add_event_cb(btn_sd, btn_sd_cb, LV_EVENT_CLICKED, NULL); 
    
    lv_obj_t * lbl_sd = lv_label_create(btn_sd);
    lv_label_set_text(lbl_sd, "MOUNT SD");
    lv_obj_center(lbl_sd);

    // --- Visual Cursors ---
    for(int i=0; i<5; i++) {
        cursors[i] = lv_obj_create(lv_screen_active());
        lv_obj_set_size(cursors[i], 60, 60);
        lv_obj_set_style_radius(cursors[i], LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_bg_opa(cursors[i], LV_OPA_0, 0); 
        lv_obj_set_style_border_width(cursors[i], 3, 0);
        
        lv_color_t c;
        switch(i) {
                case 0: c = lv_palette_main((lv_palette_t)i); break;
                case 1: c = lv_palette_main((lv_palette_t)i); break;
                case 2: c = lv_palette_main((lv_palette_t)i); break;
                case 3: c = lv_palette_main((lv_palette_t)i); break;
                case 4: c = lv_palette_main((lv_palette_t)i); break;
            default: c = lv_color_white();
        }
        lv_obj_set_style_border_color(cursors[i], c, 0);
        lv_obj_clear_flag(cursors[i], LV_OBJ_FLAG_CLICKABLE); 
        lv_obj_add_flag(cursors[i], LV_OBJ_FLAG_HIDDEN); 
    }
}

// -------------------------------------------------------------------------
// MAIN LOOP
// -------------------------------------------------------------------------
void loop() {
    // 1. Poll Hardware (Debounce is now INTERNAL to TouchManager)
    activeTouchCount = touchMgr.read(activePoints, 5);
    
    // 2. Update Visual Debug Overlay
    for(int i=0; i<5; i++) {
        if(i < activeTouchCount) {
            TouchPoint &p = activePoints[i]; 
            
            // Show Cursor
            lv_obj_clear_flag(cursors[i], LV_OBJ_FLAG_HIDDEN);
            lv_obj_set_pos(cursors[i], p.x - 30, p.y - 30); 
            
            // Update Text
            lv_label_set_text_fmt(coord_labels[i], "ID%d: %d,%d", p.id, p.x, p.y);
            
            // Highlight text color
            lv_color_t c;
            switch(p.id % 5) {
                case 0: c = lv_palette_main((lv_palette_t)i); break;
                case 1: c = lv_palette_main((lv_palette_t)i); break;
                case 2: c = lv_palette_main((lv_palette_t)i); break;
                case 3: c = lv_palette_main((lv_palette_t)i); break;
                case 4: c = lv_palette_main((lv_palette_t)i); break;
                default: c = lv_color_white();
            }
            lv_obj_set_style_text_color(coord_labels[i], c, 0);

        } else {
            // Hide Inactive
            lv_obj_add_flag(cursors[i], LV_OBJ_FLAG_HIDDEN);
            lv_label_set_text_fmt(coord_labels[i], "ID%d: --", i);
            lv_obj_set_style_text_color(coord_labels[i], lv_color_hex(0x404040), 0);
        }
    }
    
    lv_label_set_text_fmt(count_label, "Active: %d", activeTouchCount);

    // 3. LVGL Task Handler
    lv_timer_handler();
    delay(1); // Reduced delay for smoother performance
}