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

// We need a structure to hold raw multi-touch data locally for the demo
struct TouchPoint {
    uint16_t x, y;
    uint8_t id;
    bool active;
};
TouchPoint touchPoints[5];

// -------------------------------------------------------------------------
// Helper: Raw GT911 Multi-Touch Reader
// (Bypasses TouchManager single-point logic for this specific demo)
// -------------------------------------------------------------------------
#define GT911_ADDR 0x5D 
// Or 0x14 depending on your board - check TouchManager config
// Since we are inside main.cpp and have access to cfg, we can use:
// cfg.TP_I2C_ADDR

void readMultiTouch() {
    uint8_t addr = cfg.TP_I2C_ADDR;
    
    // 1. Read Status
    Wire.beginTransmission(addr);
    Wire.write(0x81); Wire.write(0x4E); // Register 0x814E
    Wire.endTransmission();
    Wire.requestFrom(addr, (uint8_t)1);
    
    if (!Wire.available()) return;
    uint8_t status = Wire.read();
    
    if ((status & 0x80) == 0) return; // Not ready
    uint8_t count = status & 0x0F;
    if (count > 5) count = 0;

    // 2. Read Points
    // ONLY read if count > 0
    if (count > 0) {
        Wire.beginTransmission(addr);
        Wire.write(0x81); Wire.write(0x50); // Start of coordinates
        Wire.endTransmission();
        
        // requestFrom returns the number of bytes actually read.
        // We need 8 bytes per point.
        uint8_t bytesRequest = count * 8;
        if (Wire.requestFrom(addr, bytesRequest) == bytesRequest) {
            for (int i = 0; i < count; i++) {
                uint8_t id = Wire.read();
                uint16_t x = Wire.read() | (Wire.read() << 8);
                uint16_t y = Wire.read() | (Wire.read() << 8);
                uint16_t size = Wire.read() | (Wire.read() << 8);
                Wire.read(); // Reserved
                
                // Rotation Logic (Standard Inverted Landscape for P4-7B)
                if (cfg.ROTATION == 2) {
                    touchPoints[i].x = cfg.WIDTH - x;
                    touchPoints[i].y = cfg.HEIGHT - y;
                } else {
                    touchPoints[i].x = x;
                    touchPoints[i].y = y;
                }
                
                touchPoints[i].id = id;
                touchPoints[i].active = true;
            }
        }
    }

    // Mark remaining slots inactive
    // This clears old data so we don't show "ghost" fingers
    for (int i = count; i < 5; i++) {
        touchPoints[i].active = false;
        touchPoints[i].x = 0;
        touchPoints[i].y = 0;
        touchPoints[i].id = 0;
    }

    // 3. Clear Status
    Wire.beginTransmission(addr);
    Wire.write(0x81); Wire.write(0x4E);
    Wire.write(0);
    Wire.endTransmission();
}

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
    // We still feed the FIRST point to LVGL so the slider works
    if (touchPoints[0].active) {
        data->state = LV_INDEV_STATE_PRESSED;
        data->point.x = touchPoints[0].x;
        data->point.y = touchPoints[0].y;
    } else {
        data->state = LV_INDEV_STATE_RELEASED;
    }
}

uint32_t my_tick_get_cb(void) { return millis(); }

static void slider_event_cb(lv_event_t * e) {
    lv_obj_t * slider = (lv_obj_t*)lv_event_get_target(e);
    displayMgr.setBrightness(lv_slider_get_value(slider));
}

// -------------------------------------------------------------------------
// SETUP
// -------------------------------------------------------------------------
void setup() {
    Serial.begin(115200);
    Serial.println("--- P4 7-inch Multi-Touch Dashboard ---");

    if (!displayMgr.begin()) {
        Serial.println("Display Fail!");
        while(1) delay(100);
    }
    
    if (!touchMgr.begin()) {
        Serial.println("Touch Fail! (Continuing anyway...)");
    }

    lv_init();
    lv_tick_set_cb(my_tick_get_cb);

    Arduino_GFX *gfx = displayMgr.getGfx();
    lv_display_t *disp = lv_display_create(gfx->width(), gfx->height());
    lv_display_set_flush_cb(disp, my_disp_flush);
    lv_draw_buf = (uint16_t*)heap_caps_malloc(LV_BUF_SIZE * sizeof(uint16_t), MALLOC_CAP_SPIRAM);
    lv_display_set_buffers(disp, lv_draw_buf, NULL, LV_BUF_SIZE * sizeof(uint16_t), LV_DISPLAY_RENDER_MODE_PARTIAL);

    lv_indev_t *indev = lv_indev_create();
    lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(indev, my_touch_read);

    // --- UI Construction ---
    lv_obj_set_style_bg_color(lv_screen_active(), lv_color_hex(0x101010), LV_PART_MAIN);

    // 1. Top Bar (Brightness)
    lv_obj_t * top_bar = lv_obj_create(lv_screen_active());
    lv_obj_set_size(top_bar, lv_pct(100), 80);
    lv_obj_set_style_bg_color(top_bar, lv_color_hex(0x202020), 0);
    lv_obj_set_style_border_side(top_bar, LV_BORDER_SIDE_BOTTOM, 0);
    lv_obj_set_style_border_width(top_bar, 2, 0);
    lv_obj_align(top_bar, LV_ALIGN_TOP_MID, 0, 0);

    lv_obj_t * slider = lv_slider_create(top_bar);
    lv_obj_set_width(slider, 400);
    lv_obj_align(slider, LV_ALIGN_RIGHT_MID, -20, 0);
    lv_slider_set_range(slider, 5, 100);
    lv_slider_set_value(slider, 100, LV_ANIM_OFF);
    lv_obj_add_event_cb(slider, slider_event_cb, LV_EVENT_VALUE_CHANGED, NULL);

    lv_obj_t * label_bright = lv_label_create(top_bar);
    lv_label_set_text(label_bright, "Brightness");
    lv_obj_align_to(label_bright, slider, LV_ALIGN_OUT_LEFT_MID, -20, 0);
    lv_obj_set_style_text_color(label_bright, lv_color_hex(0xFFFFFF), 0);
    
    lv_obj_t * title = lv_label_create(top_bar);
    lv_label_set_text(title, "P4 Multi-Touch Test");
    lv_obj_align(title, LV_ALIGN_LEFT_MID, 20, 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(title, lv_color_hex(0x00A8FF), 0); // Cyan title

    // 2. Right Panel (Coordinates)
    lv_obj_t * panel = lv_obj_create(lv_screen_active());
    lv_obj_set_size(panel, 250, 400);
    lv_obj_align(panel, LV_ALIGN_BOTTOM_RIGHT, -20, -20);
    lv_obj_set_style_bg_color(panel, lv_color_hex(0x202020), 0);
    
    lv_obj_t * lbl_header = lv_label_create(panel);
    lv_label_set_text(lbl_header, "Coordinates");
    lv_obj_align(lbl_header, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_text_color(lbl_header, lv_color_hex(0xAAAAAA), 0);

    // Create labels for 5 fingers
    for(int i=0; i<5; i++) {
        coord_labels[i] = lv_label_create(panel);
        lv_label_set_text_fmt(coord_labels[i], "ID%d: --, --", i);
        lv_obj_align(coord_labels[i], LV_ALIGN_TOP_LEFT, 10, 40 + (i * 30));
        lv_obj_set_style_text_font(coord_labels[i], &lv_font_montserrat_16, 0);
        lv_obj_set_style_text_color(coord_labels[i], lv_color_hex(0x606060), 0); // Dim by default
    }

    // 3. Count Label
    count_label = lv_label_create(panel);
    lv_label_set_text(count_label, "Active: 0");
    lv_obj_align(count_label, LV_ALIGN_BOTTOM_MID, 0, -10);
    lv_obj_set_style_text_font(count_label, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(count_label, lv_color_hex(0xFFFFFF), 0);

    // 4. Cursor Circles (Initially Hidden)
    for(int i=0; i<5; i++) {
        cursors[i] = lv_obj_create(lv_screen_active());
        lv_obj_set_size(cursors[i], 80, 80);
        lv_obj_set_style_radius(cursors[i], LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_bg_opa(cursors[i], LV_OPA_0, 0); // Transparent fill
        lv_obj_set_style_border_width(cursors[i], 4, 0);
        lv_obj_set_style_border_color(cursors[i], lv_palette_main((lv_palette_t)i), 0); // Color by ID
        lv_obj_clear_flag(cursors[i], LV_OBJ_FLAG_CLICKABLE); // Pass through
        lv_obj_add_flag(cursors[i], LV_OBJ_FLAG_HIDDEN);
    }
}

// -------------------------------------------------------------------------
// LOOP
// -------------------------------------------------------------------------
void loop() {
    // 1. Poll Hardware
    readMultiTouch();
    
    // 2. Update UI
    int activeCount = 0;
    
    for(int i=0; i<5; i++) {
        if(touchPoints[i].active) {
            activeCount++;
            
            // Move Visual Cursor
            lv_obj_clear_flag(cursors[i], LV_OBJ_FLAG_HIDDEN);
            lv_obj_set_pos(cursors[i], touchPoints[i].x - 40, touchPoints[i].y - 40);
            
            // Update Text
            lv_label_set_text_fmt(coord_labels[i], "ID%d: %d, %d", touchPoints[i].id, touchPoints[i].x, touchPoints[i].y);
            lv_obj_set_style_text_color(coord_labels[i], lv_palette_main((lv_palette_t)i), 0); // Active Color
        } else {
            // Hide
            lv_obj_add_flag(cursors[i], LV_OBJ_FLAG_HIDDEN);
            
            // Dim Text
            lv_obj_set_style_text_color(coord_labels[i], lv_color_hex(0x404040), 0);
        }
    }
    
    lv_label_set_text_fmt(count_label, "Active: %d", activeCount);

    lv_timer_handler();
    delay(5);
}