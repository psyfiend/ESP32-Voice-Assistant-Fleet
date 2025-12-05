#include <Arduino.h>
#include <Arduino_GFX_Library.h>
#include <lvgl.h>
#include "DisplayManager.h"
#include "TouchManager.h"
#if defined(WS_P4_SMART86)
    #include "panels/BSP_WS_P4_Smart86_LCD.h"
#elif defined(WS_S3_SMART86)
    #include "panels/BSP_WS_S3_Smart86_LCD.h"
#elif defined(GUITION_3248W535)
    #include "panels/BSP_Guition_3248W535_LCD.h"
#elif defined(GUITION_8048W550)
    #include "panels/BSP_Guition_8048W550_LCD.h"
#endif

// Global Managers
DisplayManager displayMgr;
TouchManager touchMgr;

// LVGL Globals
static uint16_t *lv_draw_buf;
#define LV_BUF_SIZE (cfg.WIDTH * cfg.HEIGHT / 10)

// -------------------------------------------------------------------------
// LVGL Flushing (Output)
// -------------------------------------------------------------------------
void my_disp_flush(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map) {
    uint32_t w = (area->x2 - area->x1 + 1);
    uint32_t h = (area->y2 - area->y1 + 1);
    
    Arduino_GFX *gfx = displayMgr.getGfx();
    gfx->draw16bitRGBBitmap(area->x1, area->y1, (uint16_t *)px_map, w, h);
    
    // CRITICAL: Flush the canvas to the screen
    gfx->flush();

    lv_display_flush_ready(disp);
}

// -------------------------------------------------------------------------
// LVGL Input Reading (Touch)
// -------------------------------------------------------------------------
void my_touch_read(lv_indev_t *indev, lv_indev_data_t *data) {
    int x, y;
    if (touchMgr.read(&x, &y)) {
        data->state = LV_INDEV_STATE_PRESSED;
        data->point.x = x;
        data->point.y = y;
        // Serial.printf("Touch: %d, %d\n", x, y); // Debugging
    } else {
        data->state = LV_INDEV_STATE_RELEASED;
    }
}

uint32_t my_tick_get_cb(void) { return millis(); }

// -------------------------------------------------------------------------
// UI Logic
// -------------------------------------------------------------------------
static void btn_event_cb(lv_event_t * e) {
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t * btn = (lv_obj_t*)lv_event_get_target(e);
    if(code == LV_EVENT_CLICKED) {
        static uint8_t cnt = 0;
        cnt++;
        lv_obj_t * label = lv_obj_get_child(btn, 0);
        lv_label_set_text_fmt(label, "Clicks: %d", cnt);
    }
}

void setup() {
    Serial.begin(115200);
    delay(1000);
    Serial.println("--- Fleet Command Boot Sequence ---");

    // 1. Init Display
    if (!displayMgr.begin()) {
        Serial.println("Display Fail!");
        while(1) delay(100);
    }
    Arduino_GFX *gfx = displayMgr.getGfx();

    // 2. Init Touch
    if (!touchMgr.begin()) {
        Serial.println("Touch Fail! (Continuing anyway...)");
    }

    // 3. Init LVGL
    lv_init();
    lv_tick_set_cb(my_tick_get_cb);

    // 4. Register Display Driver
    lv_display_t *disp = lv_display_create(gfx->width(), gfx->height());
    lv_display_set_flush_cb(disp, my_disp_flush);
    
    lv_draw_buf = (uint16_t*)heap_caps_malloc(LV_BUF_SIZE * 2, MALLOC_CAP_SPIRAM);
    lv_display_set_buffers(disp, lv_draw_buf, NULL, LV_BUF_SIZE * 2, LV_DISPLAY_RENDER_MODE_PARTIAL);

    // 5. Register Touch Driver
    lv_indev_t *indev = lv_indev_create();
    lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(indev, my_touch_read);

    // 6. Create UI
    lv_obj_set_style_bg_color(lv_screen_active(), lv_color_hex(0x202020), LV_PART_MAIN);

    // Create a Button
    lv_obj_t *btn = lv_button_create(lv_screen_active());
    lv_obj_set_size(btn, 120, 50);
    lv_obj_align(btn, LV_ALIGN_CENTER, 0, 0);
    lv_obj_add_event_cb(btn, btn_event_cb, LV_EVENT_ALL, NULL);

    lv_obj_t *label = lv_label_create(btn);
    lv_label_set_text(label, "Touch Me");
    lv_obj_center(label);

    Serial.println("System Ready.");
}

void loop() {
    lv_timer_handler();
    delay(5);
}