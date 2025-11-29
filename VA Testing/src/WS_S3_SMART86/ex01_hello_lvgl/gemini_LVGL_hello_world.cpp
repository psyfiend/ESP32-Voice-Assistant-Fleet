//
// gemini_LVGL_hello_world.cpp
//
// LVGL v9 Hello World for Waveshare Smart 86 Box
// Architecture:
// - Hardware: Initialized via hardware_config (Expander + RGB Panel)
// - UI: LVGL 9.4 using Partial Refresh (Safe Mode)
//
// Engineer: Gemini
// Architect: Eric
//

#include <Arduino.h>
#include <lvgl.h>
#include "pin_config.h"
#include "Arduino_GFX_Library.h"

//-----------------------------------------------------------------
// Create Display objects
//-----------------------------------------------------------------

Arduino_DataBus *bus = new Arduino_XCA9554SWSPI(
    7 /* RST */, 0 /* CS */, 2 /* SCK */, 1 /* MOSI */,
    &Wire, IO_EXPANDER_I2C_ADDR
);

Arduino_ESP32RGBPanel *rgbpanel = new Arduino_ESP32RGBPanel(
    17 /* DE */, 3 /* VSYNC */, 46 /* HSYNC */, 9 /* PCLK */,
    10 /* B0 */, 11 /* B1 */, 12 /* B2 */, 13 /* B3 */, 14 /* B4 */,
    21 /* G0 */, 8 /* G1 */, 18 /* G2 */, 45 /* G3 */, 38 /* G4 */, 39 /* G5 */,
    40 /* R0 */, 41 /* R1 */, 42 /* R2 */, 2 /* R3 */, 1 /* R4 */,
    1 /* hsync_polarity */, 10 /* hsync_front_porch */, 8 /* hsync_pulse_width */, 50 /* hsync_back_porch */,
    1 /* vsync_polarity */, 10 /* vsync_front_porch */, 8 /* vsync_pulse_width */, 20 /* vsync_back_porch */
);

Arduino_RGB_Display *gfx = new Arduino_RGB_Display(
    480 /* width */, 480 /* height */, rgbpanel, 0 /* rotation */, true /* auto_flush */,
    bus, GFX_NOT_DEFINED /* RST */, st7701_type1_init_operations, sizeof(st7701_type1_init_operations)
);

// Create pin expander object
Arduino_XCA9554SWSPI *exio;



// -------------------------------------------------------------------------
// LVGL Configuration
// -------------------------------------------------------------------------
#define SCREEN_WIDTH 480
#define SCREEN_HEIGHT 480

// Define a buffer size (1/10th of screen is a good balance of RAM vs Perf)
// 480 * 480 / 10 = 23,040 pixels * 2 bytes = ~46KB
#define LV_BUF_SIZE (SCREEN_WIDTH * SCREEN_HEIGHT)

// Buffer pointer
static uint16_t *lv_draw_buf;

// -------------------------------------------------------------------------
// LVGL
// -------------------------------------------------------------------------
void my_disp_flush(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map) {
    uint32_t w = (area->x2 - area->x1 + 1);
    uint32_t h = (area->y2 - area->y1 + 1);

    gfx->draw16bitRGBBitmap(area->x1, area->y1, (uint16_t *)px_map, w, h);

    lv_display_flush_ready(disp);
}

uint32_t my_tick_get_cb(void) {
    return millis();
}

// -------------------------------------------------------------------------
// Main Program
// -------------------------------------------------------------------------

void setup() {
    Serial.begin(115200);
    delay(2000);
    Serial.println("--- Waveshare Smart86 LVGL Test ---");

    Serial.printf("PSRAM Total: %d bytes\n", ESP.getPsramSize());
    if (ESP.getPsramSize() == 0) {
        Serial.println("CRITICAL ERROR: PSRAM not found! Display will fail.");
        return;
    }

    Serial.println("Initializing I2C...");
    Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL);
    Wire.beginTransmission(IO_EXPANDER_I2C_ADDR);
    if (Wire.endTransmission() != 0) {
        Serial.println();
        Serial.println("[HW] CRITICAL: TCA9554 Not Found!");
        // return false;
    }
    Serial.println("TCA9554 Found!");

    // Backlight (GPIO 4)
    pinMode(4, OUTPUT);
    digitalWrite(4, LOW);       // Backlight LOW is ON

    // Create pin expander interface
    exio = (Arduino_XCA9554SWSPI*)bus;

    // Start the expander explicitly (good practice)
    bus->begin();

    exio->pinMode(5, OUTPUT);       // EXIO5 - TP_RST
    exio->pinMode(6, OUTPUT);       // EXIO6 - TP_INT
    exio->digitalWrite(6, LOW);     // TP_INT LOW
    delay(200);
    exio->digitalWrite(5, LOW);     // TP_RST LOW
    delay(200);
    exio->digitalWrite(5, HIGH);    // TP_RST HIGH
    delay(200);


    // Init Display
    if (!gfx->begin()) {
    Serial.println("gfx->begin() failed!");
    }
    gfx->fillScreen(RGB565_ALICEBLUE);
    gfx->setCursor(10, 10);
    gfx->setTextColor(RGB565_RED);
    gfx->println("Hello World!");
    
    Serial.println("Hardware Initialized.");

    // Red Screen Test
    gfx->fillScreen(RGB565_RED);
    delay(500);
    gfx->fillScreen(RGB565_GREEN);
    delay(500);
    gfx->fillScreen(RGB565_BLUE);
    delay(500);

    // LVGL Setup
    lv_init();
    lv_tick_set_cb(my_tick_get_cb);

    lv_display_t *disp = lv_display_create(SCREEN_WIDTH, SCREEN_HEIGHT);
    lv_display_set_flush_cb(disp, my_disp_flush);

    // Allocate display buffer(s)
    size_t buffer_size = (gfx->width() * gfx->height() / 10);
    lv_draw_buf = (uint16_t*)heap_caps_malloc(buffer_size * 2, MALLOC_CAP_SPIRAM);
    lv_display_set_buffers(disp, lv_draw_buf, NULL, buffer_size * 2, LV_DISPLAY_RENDER_MODE_PARTIAL);

    // UI
    lv_obj_set_style_bg_color(lv_screen_active(), lv_color_hex(0xFF0000), LV_PART_MAIN);
    lv_obj_t *label = lv_label_create(lv_screen_active());
    lv_label_set_text(label, "IT WORKS!");
    lv_obj_set_style_text_color(label, lv_color_white(), 0);
    lv_obj_align(label, LV_ALIGN_CENTER, 0, 0);
}

void loop() {
    lv_timer_handler();
    delay(5);
}