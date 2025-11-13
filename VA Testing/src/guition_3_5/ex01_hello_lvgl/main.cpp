/*
 * Project: ESP32 Voice Assistant Fleet
 * Example: ex01_guition_hello
 * Target:  Guition S3 3.5" QSPI (JC3248W535)
 * Goal:    Prove that we can bypass the manufacturer's LVGL v8
 * drivers and run LVGL v9.4+ using the bitbank
 * library stack.
 *
 * Status:  CORRECTED IMPLEMENTATION (Nov 13, 2025)
 * - Uses LVGL v9.4 API (lv_display_t, etc.)
 * - Uses MANUAL pin definitions (the pre-defined panel was wrong)
 * - Uses correct "glue" pattern from cyd_demo.ino
 */

#include <Arduino.h>
#include "lvgl.h"
#include "lv_version.h"
#if defined(ARDUINO_ARCH_ESP32)
#include <esp_heap_caps.h>
#endif

#include <bb_spi_lcd.h> // 2. The "F1 Car" (bitbank's driver)

// --- Step 1: Define our "Ground Truth" Pins ---
// This is the manual configuration. We are NOT using a
// pre-defined panel, as it was incorrect.
// Traced from schematic: JC3248W535-2.png
#define LCD_WIDTH 320
#define LCD_HEIGHT 480
#define LCD_SCLK 39
#define LCD_DATA0 40 // MOSI
#define LCD_DATA1 41 // MISO
#define LCD_DATA2 42
#define LCD_DATA3 45
#define LCD_CS 38
#define LCD_DC 46
#define LCD_RST 48
#define LCD_BL 47
// Driver chip is AXS15231B (from spec sheet)
#define LCD_DRIVER LCD_AXS15231B

// --- Step 2: Initialize our "F1 Car" (the driver) ---
// We use the full, manual QSPI constructor to pass
// all our "ground truth" pins.

BB_SPI_LCD lcd;

// --- Step 3: Manually define the "Glue" parts ---
static lv_draw_buf_t disp_buf;
static lv_display_t *disp;
static lv_color_t *buf1;
static uint16_t *dma_buf = nullptr;
static uint16_t dma_buf_static[512]; // Fallback buffer

// LVGL tick callback using Arduino millis()
static uint32_t my_tick(void)
{
    return millis();
}

// Define the size of our buffer. 1/10th of the screen.
#define DRAW_BUF_SIZE (LCD_WIDTH * LCD_HEIGHT / 10) // Size in *pixels*

// --- Step 4: The "Expediter" (Flush Callback) ---
// This is the "recipe" from the bitbank examples.
void my_disp_flush(lv_display_t *disp_ptr, const lv_area_t *area, uint8_t *px_map)
{
    BB_SPI_LCD *lcd = (BB_SPI_LCD *)lv_display_get_user_data(disp_ptr);

    const int w = area->x2 - area->x1 + 1;
    const int h = area->y2 - area->y1 + 1;

    // Position the LCD window and push the pixel block row-by-row
    lcd->setAddrWindow(area->x1, area->y1, w, h);
    uint16_t *src = (uint16_t *)px_map;
    for (int y = 0; y < h; y++)
    {
        for (int x = 0; x < w; x++)
        {
            dma_buf[x] = __builtin_bswap16(src[x]);
        }
        lcd->pushPixels(dma_buf, w, DRAW_TO_LCD | DRAW_WITH_DMA);
        src += w;
    }

    lv_display_flush_ready(disp_ptr);
}

// --- Step 5: A simple "Hello World" UI ---
void create_hello_world_ui()
{
    lv_obj_t *scr = lv_scr_act();
    lv_obj_set_style_bg_color(scr, lv_color_hex(0x222222), LV_PART_MAIN);

    lv_obj_t *label = lv_label_create(scr);
    // Use LVGL's formatting helper and include the LVGL version string
    lv_label_set_text_fmt(label, "Hello, LVGL %s\n\nThis is the MANUAL PIN\n'recipe' test.", lv_version_info());
    lv_obj_set_style_text_color(label, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
    lv_obj_set_style_text_font(label, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_align(label, LV_ALIGN_CENTER, 0, 0);
}

void setup()
{
    Serial.begin(115200);
    delay(2000);
    Serial.println("--- Guition S3 3.5\" LVGL v9 Test (Manual Pin Config) ---");

    // --- Step 6: Manually initialize all components ---

    // 1. Initialize the "Chef" (LVGL)
    lv_init();
    // Provide LVGL with a tick source using Arduino millis()
    lv_tick_set_cb(my_tick);
    Serial.println("LVGL (lv_init) done.");

    // 2. Initialize the "F1 Car" (hardware driver)
    // We call the no-argument begin() because we already
    // configured all the pins in the constructor.
    lcd.begin(31, 0, 40000000, 45, 8, -1, 1); // itype, flags(0), freq, cs(45), dc(8), rst(-1), bl(1), (MISO(D1), MOSI(D0), SCLK)
    // lcd.begin(DISPLAY_CYD_535); // 41
    // lcd.beginQSPI(41, 0, 45, 47, 21, 48, 40, 39, -1, 40000000); // itype(31), flags, cs, clk, d0, d1, d2, d3, rst, freq
    
    // lcd.setRotation(270); // Landscape mode // HW rotation not working on this device?
    Serial.println("Display Driver (bb_spi_lcd) manually initialized.");
    int w = 320; //lcd.width();  // This should now be 320
    int h = 480; //lcd.height(); // This should now be 480
    Serial.printf("LCD dimensions: %d w x %d h\n", w, h);

    //if (w != 480 || h != 320) {
        //Serial.println("CRITICAL: LCD dimensions are incorrect! Check rotation?");
        // Note: DRAW_BUF_SIZE is based on 320x480, but rotation
        // is handled by LVGL. The buffer size should be correct
        // regardless of rotation.
    //}

    // 3. Allocate the DMA row buffer (example calls this dma_buf)
    // We do this *before* allocating the LVGL buffer
    dma_buf = (uint16_t *)heap_caps_malloc(sizeof(uint16_t) * w, MALLOC_CAP_DMA);
    if (dma_buf == nullptr)
    {
        // Fallback: use the static buffer
        dma_buf = dma_buf_static;
        Serial.println("Warning: DMA buffer allocation failed, using static fallback");
    } else {
        Serial.printf("DMA row buffer allocated (%d bytes)\n", sizeof(uint16_t) * w);
    }

    // 4. Allocate LVGL draw buffer in PSRAM
    uint32_t iSize = DRAW_BUF_SIZE; // Use our 320x480-based define
    buf1 = (lv_color_t *)heap_caps_malloc(iSize, MALLOC_CAP_SPIRAM);
    if (buf1 == nullptr)
    {
        Serial.println("FATAL ERROR: Failed to allocate draw buffer in PSRAM!");
        while (1);
    }
    Serial.printf("LVGL draw buffer allocated in PSRAM (%d bytes)\n", iSize);

    // 5. Create and configure the LVGL display (v9 API)
    disp = lv_display_create(w, h); // Use dimensions *after* rotation
    if (disp == nullptr)
    {
        Serial.println("FATAL ERROR: lv_display_create failed");
        while (1);
    }

    // This is the correct v9.4+ buffer init "recipe"
    if (lv_draw_buf_init(&disp_buf, w, iSize / (sizeof(uint16_t) * w), LV_COLOR_FORMAT_NATIVE, LV_STRIDE_AUTO, buf1, iSize) != LV_RESULT_OK) {
        Serial.println("FATAL ERROR: lv_draw_buf_init failed");
        while(1);
    }
    Serial.println("LVGL draw buffer initialized.");

    lv_display_set_draw_buffers(disp, &disp_buf, nullptr);
    lv_display_set_flush_cb(disp, my_disp_flush);
    lv_display_set_user_data(disp, &lcd); // "Glue" the lcd object to the display
    lv_display_set_color_format(disp, LV_COLOR_FORMAT_NATIVE);
    lv_display_set_render_mode(disp, LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_default(disp);
    Serial.println("LVGL display created and configured.");

    // 6. Create our simple UI
    create_hello_world_ui();
    Serial.println("UI created. Starting loop.");
}

void loop()
{
    // --- Step 7: Keep LVGL running ---
    lv_timer_handler();
    delay(5);
}