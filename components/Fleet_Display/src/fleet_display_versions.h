#pragma once
//
// fleet_display_versions - the vendored panel drivers' versions, in ONE place.
//
// IDF's component build injects each driver's ESP_LCD_<CHIP>_VER_* macros;
// PlatformIO does not. Each vendored driver includes this file instead (its
// one local change - components/Fleet_Display/README.md), and the System
// Doctor reports the same numbers through each wrapper's name (fleet_dsi_<chip>.c,
// FLEET_DSI_VER()). Updating a
// driver means copying the new upstream files over AND changing its line here.
//
// Plain C: included from the vendored .c files.
//

// waveshare/esp_lcd_hx8394 - WS_P4_5 (2.9 step 2)
#define ESP_LCD_HX8394_VER_MAJOR  2
#define ESP_LCD_HX8394_VER_MINOR  1
#define ESP_LCD_HX8394_VER_PATCH  0

// waveshare/esp_lcd_st7703 - WS_P4_4B (2.9 step 3)
#define ESP_LCD_ST7703_VER_MAJOR  2
#define ESP_LCD_ST7703_VER_MINOR  0
#define ESP_LCD_ST7703_VER_PATCH  0

// espressif/esp_lcd_ek79007 - WS_P4_7B (vendored, never run)
#define ESP_LCD_EK79007_VER_MAJOR 2
#define ESP_LCD_EK79007_VER_MINOR 0
#define ESP_LCD_EK79007_VER_PATCH 2

// espressif/esp_lcd_st7701 - CYD_P4_4880 (JC4880P443; vendored 2026-09-28, never run).
// Also WS_S3_4B's panel, over RGB, at 2.9 step 4.
#define ESP_LCD_ST7701_VER_MAJOR  2
#define ESP_LCD_ST7701_VER_MINOR  0
#define ESP_LCD_ST7701_VER_PATCH  2

// espressif/esp_lcd_jd9165 - CYD_P4_1060 (vendored, never run)
#define ESP_LCD_JD9165_VER_MAJOR  2
#define ESP_LCD_JD9165_VER_MINOR  0
#define ESP_LCD_JD9165_VER_PATCH  2
