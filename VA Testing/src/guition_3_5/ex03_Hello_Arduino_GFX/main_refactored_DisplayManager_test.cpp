#include <Arduino.h>
#include <Arduino_GFX_Library.h>
#include <lvgl.h>
#include "pin_config.h"
#include "DisplayManager.h"

// The Global Manager
DisplayManager displayMgr;

// LVGL Buffers
static uint16_t *lv_draw_buf;
#define LV_BUF_SIZE (DISPLAY_WIDTH * DISPLAY_HEIGHT / 10)

// -------------------------------------------------------------------------
// LVGL Flush Callback
// -------------------------------------------------------------------------
void my_disp_flush(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map) {
    uint32_t w = (area->x2 - area->x1 + 1);
    uint32_t h = (area->y2 - area->y1 + 1);
    
    // We access the driver via the manager
    displayMgr.getGfx()->draw16bitRGBBitmap(area->x1, area->y1, (uint16_t *)px_map, w, h);
    lv_display_flush_ready(disp);
}

uint32_t my_tick_get_cb(void) { return millis(); }

void setup() {
    Serial.begin(115200);
    // Give the USB Serial time to catch up
    delay(1000);
    Serial.println("--- Booting Voice Assistant Fleet ---");

    // 1. Initialize Display Hardware via Manager
    if (!displayMgr.begin()) {
        Serial.println("CRITICAL: Display Init Failed!");
        // Blink error code forever
        pinMode(PIN_LCD_BL, OUTPUT);
        while(1) {
            digitalWrite(PIN_LCD_BL, !digitalRead(PIN_LCD_BL));
            delay(100);
        }
    }
    
    // 2. Specific Touch Reset (Handled internally by manager now!)
    displayMgr.resetTouch();
    
    Serial.println("Hardware Ready.");

    // 3. LVGL Setup
    lv_init();
    lv_tick_set_cb(my_tick_get_cb);

    lv_display_t *disp = lv_display_create(DISPLAY_WIDTH, DISPLAY_HEIGHT);
    lv_display_set_flush_cb(disp, my_disp_flush);

    // Allocate buffer in PSRAM
    lv_draw_buf = (uint16_t*)heap_caps_malloc(LV_BUF_SIZE * 2, MALLOC_CAP_SPIRAM);
    lv_display_set_buffers(disp, lv_draw_buf, NULL, LV_BUF_SIZE * 2, LV_DISPLAY_RENDER_MODE_PARTIAL);

    // 4. Draw UI
    lv_obj_set_style_bg_color(lv_screen_active(), lv_color_hex(0x000000), LV_PART_MAIN);
    
    lv_obj_t *label = lv_label_create(lv_screen_active());
    lv_label_set_text(label, "FLEET COMMAND\nREADY");
    lv_obj_set_style_text_color(label, lv_color_hex(0x00FF00), 0);
    lv_obj_align(label, LV_ALIGN_CENTER, 0, 0);
}

void loop() {
    lv_timer_handler();
    delay(5);
}