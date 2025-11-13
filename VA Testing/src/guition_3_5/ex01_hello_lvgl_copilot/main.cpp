/*
 * Project: ESP32 Voice Assistant Fleet
 * Example: ex01_guition_hello
 * Target:  Guition S3 3.5" QSPI (JC3248W535)
 * Goal:    Prove that we can bypass the manufacturer's LVGL v8
 * drivers and run LVGL v9.3+ using the bitbank
 * library stack.
 *
 * Status:  CORRECTED IMPLEMENTATION (Nov 12, 2025)
 * This version manually implements the "glue" logic
 * inspired by the bb_lvgl example "recipes".
 */

#include <Arduino.h>
#include "lvgl.h"
#include "lv_version.h"
#if defined(ARDUINO_ARCH_ESP32)
#include <esp_heap_caps.h>
#endif

//#include <lvgl/lvgl.h>       // 1. The "Chef" (LVGL Core)
#include <bb_spi_lcd.h> // 2. The "F1 Car" (bitbank's driver)

#define LCD_NAME DISPLAY_CYD_535
#define LCD_ROTATION_270 270

// --- Step 2: Initialize our "F1 Car" (the driver) ---
// This IS a real library, so we create the object.
static BB_SPI_LCD lcd;

// --- Step 3: Manually define the "Glue" parts ---
// LVGL v9 uses `lv_draw_buf_t` and `lv_display_t`. Declare the draw buffer
// and the display pointer here and allocate the actual pixel buffer in setup().
static lv_draw_buf_t disp_buf;
static lv_display_t *disp;

// The actual memory for the buffer. We'll allocate this in setup()
// We use a pointer here.
// The LVGL draw buffer memory (allocated in PSRAM)
static lv_color_t *buf1;

// Temporary row buffer for endian conversion / DMA transfers
// Match the example naming: dma_buf. We'll allocate at runtime to match the
// display width; keep a small static fallback buffer to mirror examples.
static uint16_t *dma_buf = nullptr;
static uint16_t dma_buf_static[512];

// LVGL tick callback using Arduino millis()
static uint32_t my_tick(void) {
    return millis();
}

// Define the size of our buffer in bytes. 1/10th of the screen is a good
// starting point. It will be allocated in PSRAM.
#define DRAW_BUF_SIZE(w, h) ((w * h) / 10 * sizeof(uint16_t))

// --- Step 4: The "Expediter" (Flush Callback) ---
// This is the *most important* part of the "recipe".
// This function is the "glue". LVGL will call this when it
// has pixels ready to be sent to the screen.
// LVGL v9 flush callback: the pixel map passed to the flush callback is a raw
// byte pointer (uint8_t *) according to lv_display_flush_cb_t in v9.
void my_disp_flush(lv_display_t *disp_ptr, const lv_area_t *area, uint8_t *px_map)
{
    // Use the BB_SPI_LCD API to write the area to the display. LVGL's px_map is
    // in the native color format (LV_COLOR_DEPTH == 16 -> RGB565), so we can
    // cast the byte pointer to uint16_t* and push pixels.
    const int w = area->x2 - area->x1 + 1;
    const int h = area->y2 - area->y1 + 1;
    const int count = w * h;

    // Position the LCD window and push the pixel block row-by-row.
    // We need to convert LVGL's little-endian RGB565 to the LCD's big-endian
    // ordering. The bb_lvgl examples do this per-row into a DMA buffer.
    lcd.setAddrWindow(area->x1, area->y1, w, h);
    uint16_t *src = (uint16_t *)px_map;
    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            dma_buf[x] = __builtin_bswap16(src[x]);
        }
        // Use DRAW_WITH_DMA flag as in examples to hint the driver to use DMA
        lcd.pushPixels(dma_buf, w, DRAW_TO_LCD | DRAW_WITH_DMA);
        src += w;
    }

    // Tell LVGL we're done for this area. If the driver is using DMA and you
    // want LVGL to wait for it you can add lcd.waitDMA() before this call.
    lv_display_flush_ready(disp_ptr);
}

// --- Step 5: A simple "Hello World" UI ---
void create_hello_world_ui()
{
    lv_obj_t *scr = lv_scr_act();
    lv_obj_set_style_bg_color(scr, lv_color_hex(0x222222), LV_PART_MAIN);

    lv_obj_t *label = lv_label_create(scr);
    // Use LVGL's formatting helper and include the LVGL version string
    lv_label_set_text_fmt(label, "Hello, LVGL %s\n\nThis is the CORRECTED\nbitbank 'recipe'.", lv_version_info());
    lv_obj_set_style_text_color(label, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
    lv_obj_set_style_text_font(label, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_align(label, LV_ALIGN_CENTER, 0, 0);
}

void setup()
{
    Serial.begin(115200);
    delay(2000);
    Serial.println("--- Guition S3 3.5\" LVGL v9 Test (Corrected) ---");

    // --- Step 6: Manually initialize all components ---

    // Initialize the "Chef" (LVGL)
    lv_init();
    // Provide LVGL with a tick source using Arduino millis()
    lv_tick_set_cb(my_tick);
    Serial.println("LVGL (lv_init) done.");

    // Initialize the LCD
    lcd.begin(LCD_NAME);
    lcd.setRotation(LCD_ROTATION_270);  // Landscape mode
    Serial.println("Display Driver (bb_spi_lcd) initialized.");
    int w = lcd.width();
    int h = lcd.height();

    // 3. Create a draw buffer for LVGL. We'll follow the example naming:
    // iSize = number of bytes for the draw buffer (1/10th of the framebuffer)
    // Compute iSize in bytes, then compute buf_h (height in pixels of the
    // intermediate draw buffer) from the display width.
    uint32_t iSize = DRAW_BUF_SIZE(w, h); // bytes
    uint32_t buf_h = (iSize / sizeof(uint16_t)) / w; // rows
    if (buf_h == 0) buf_h = 1;

    // Allocate pixel memory in PSRAM (RGB565 -> uint16_t per pixel). iSize
    // is in bytes, so allocate that many bytes and cast to lv_color_t*.
    buf1 = (lv_color_t *)heap_caps_malloc(iSize, MALLOC_CAP_SPIRAM);
    if (buf1 == nullptr) {
        Serial.println("FATAL ERROR: Failed to allocate draw buffer in PSRAM!");
        while (1) ;
    }
    Serial.printf("Draw buffer allocated in PSRAM (%d bytes)\n", iSize);

    // Initialize the lv_draw_buf_t with the allocated memory
    if (lv_draw_buf_init(&disp_buf, w, buf_h, LV_COLOR_FORMAT_NATIVE, LV_STRIDE_AUTO, buf1, iSize) != LV_RESULT_OK) {
        Serial.println("FATAL ERROR: lv_draw_buf_init failed");
        while(1);
    }
    Serial.println("LVGL draw buffer initialized in PSRAM.");

    // 4. Create and configure the LVGL display (v9 API)
    disp = lv_display_create(w, h);
    if (disp == nullptr) {
        Serial.println("FATAL ERROR: lv_display_create failed");
        while(1);
    }

    lv_display_set_draw_buffers(disp, &disp_buf, nullptr);
    lv_display_set_flush_cb(disp, my_disp_flush);
    lv_display_set_color_format(disp, LV_COLOR_FORMAT_NATIVE);
    lv_display_set_render_mode(disp, LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_default(disp);
    Serial.println("LVGL display created and configured.");

    // Allocate the DMA row buffer (example calls this dma_buf). We'll try to
    // allocate DMA-capable internal memory sized to the display width. If
    // allocation fails, fall back to the static dma_buf_static[] defined
    // above which mirrors the example's static dma buffer.
    dma_buf = (uint16_t *)heap_caps_malloc(sizeof(uint16_t) * w, MALLOC_CAP_DMA);
    if (dma_buf == nullptr) {
        // Fallback: use the static buffer (matches example usage)
        dma_buf = dma_buf_static;
        Serial.println("Warning: dma_buf allocation failed, using static fallback");
    }

    // 8. Create our simple UI
    create_hello_world_ui();
    Serial.println("UI created. Starting loop.");
}

void loop()
{
    // --- Step 7: Keep LVGL running ---
    lv_timer_handler();
    delay(5);
}