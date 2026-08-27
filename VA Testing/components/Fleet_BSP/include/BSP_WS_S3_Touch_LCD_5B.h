#pragma once
#ifndef BSP_WS_S3_TOUCH_LCD_5B_H
#define BSP_WS_S3_TOUCH_LCD_5B_H

#include <Arduino_GFX_Library.h>
#include "Fleet_BSP.h"

// -------------------------------------------------------------------------
// Board: Waveshare ESP32-S3-Touch-LCD-5B (ESP32-S3 N16R8)
// Driver: ST7262 (RGB) - pure RGB interface, no 3-wire SPI / init command
// sequence (ESP_PANEL_BOARD_LCD_RGB_USE_CONTROL_PANEL=0 in vendor config).
// Resolution: 1024x600 (the "B" suffix distinguishes this from the plain
// 800x480 "ESP32-S3-Touch-LCD-5", which the vendor demo package also covers
// and easy to conflate - pin table below is confirmed against the vendor's
// board-specific file, BOARD_WAVESHARE_ESP32_S3_TOUCH_LCD_5_B.h, not the
// generic example config which defaults to the 800x480 variant).
// Expander: CH422G - NOT XCA9554/TCA9554. See CH422G.h/.cpp for why this
// needs its own driver instead of reusing Arduino_XCA9554SWSPI.
// -------------------------------------------------------------------------

static const Fleet_BSP WS_S3_TouchLCD5B_LCD = {

    .device_name       = "Waveshare S3 Touch LCD 5B",

    // --=  I2C Bus  =-- (shared: CH422G expander + GT911 touch)
    .I2C_SDA_PIN       = 8,
    .I2C_SCL_PIN       = 9,

    // --= Expander Pins (CH422G) =--
    // EXPANDER_I2C_ADDR is unused by CH422G (it responds on 4 fixed
    // pseudo-addresses, not one address with indexed registers) - kept
    // populated only for readability/consistency with other boards.
    .EXPANDER_I2C_ADDR = 0x24,
    .EXIO_TP_RST       = 1,   // IO1 - touch reset
    .EXIO_LCD_RST      = 3,   // IO3 - LCD reset
    .EXIO_LCD_BL       = 2,   // IO2 - backlight enable (on/off only, no PWM)
    .EXIO_SD_CS        = 4,   // IO4 - SD card chip-select (not yet wired into an SD component)
    .EXIO_USB_SEL      = 5,   // IO5 - USB/CAN header mux (HIGH = USB via FSUSB42UMX; LOW frees GPIO19/20 for CAN_TX/RX)

    // ---= LCD Definitions =---
    .LCD_MODEL         = "ST7262",
    .WIDTH             = 1024,
    .HEIGHT            = 600,
    .ROTATION          = 0,
    .AUTO_FLUSH        = true,

    // ---= Backlight =---
    // Driven through the CH422G expander (EXIO_LCD_BL) instead of a native
    // GPIO - on/off only, no PWM brightness control on this board.
    // DisplayManager's HAS_CH422G branch writes EXIO_LCD_BL directly rather
    // than going through the native LEDC backlight path these fields
    // normally configure.
    .LCD_BL            = -1,
    .LCD_BL_ON_LEVEL   = 1,
    .LCD_BL_FREQ       = 0,

    // ---= Touch Panel =---
    .TP_NAME            = "GT911",
    .TP_I2C_ADDR        = 0x5D,
    // 100kHz tested twice (with and without the verbatim CH422G reset port)
    // and made no difference either time - ruled out. Back to 400000,
    // matching Waveshare's own confirmed value for this board.
    .TP_I2C_CLOCK_SPEED = 400000,
    .TP_SDA             = 8,
    .TP_SCL             = 9,
    .TP_INT             = 4,   // Native GPIO
    .TP_RST             = -1,  // Via CH422G EXIO_TP_RST, not a native pin
    .TP_MAX_TOUCH       = 5,

    // ---= LCD Control Pins =---
    .LCD_RST           = -1,  // Via CH422G EXIO_LCD_RST, not a native pin

    // ---= RGB Interface =---
    .LCD_DE            = 5,
    .LCD_VSYNC         = 3,
    .LCD_HSYNC         = 46,
    .LCD_PCLK          = 7,

    .R0 = 1,  .R1 = 2,  .R2 = 42, .R3 = 41, .R4 = 40,
    .G0 = 39, .G1 = 0,  .G2 = 45, .G3 = 48, .G4 = 47, .G5 = 21,
    .B0 = 14, .B1 = 38, .B2 = 18, .B3 = 17, .B4 = 10,

    // --= RGB Timing =---
    // HPW/HBP/HFP: the vendor's own materials disagree with themselves here.
    // BOARD_WAVESHARE_ESP32_S3_TOUCH_LCD_5_B.h (the per-board file, dedicated
    // to this exact 1024x600 variant) says 24/160/160. Two OTHER,
    // independently-authored vendor files - the 1024x600 branch of both
    // esp_panel_board_custom_conf.h (09_lvgl_Porting) and
    // waveshare_lcd_port.h (08_DrawColorBar) - agree with each other on
    // 30/145/170 instead. Going with the 2-source majority; VSYNC values are
    // the one thing all three sources already agree on.
    .HSYNC_POL      = 0,
    .VSYNC_POL      = 0,
    .HSYNC_PWIDTH   = 30,
    .HSYNC_BPORCH   = 145,
    .HSYNC_FPORCH   = 170,
    .VSYNC_PWIDTH   = 2,
    .VSYNC_BPORCH   = 23,
    .VSYNC_FPORCH   = 12,

    .PCLK_ACTIVE_NEG       = 1,
    // 21MHz agrees between BOARD_WAVESHARE_ESP32_S3_TOUCH_LCD_5_B.h and the
    // 1024x600 branch of esp_panel_board_custom_conf.h. 08_DrawColorBar's
    // waveshare_lcd_port.h says 16MHz, but that file never actually branches
    // TIMING_FREQ_HZ by resolution at all (single #define, outside the
    // #if ESP_PANEL_USE_1024_600_LCD block) - almost certainly a leftover
    // 800x480 default rather than a considered value for this variant.
    .PREFER_SPEED          = 21000000,
    .USE_BIG_ENDIAN        = false,
    .BOUNCE_BUFFER_SIZE_PX = 1024 * 10, // Vendor-recommended; divides 1024x600 evenly (N=60)

    // --= LVGL Settings =--
    .DOUBLE_BUFFERING   = true,
    .DRAW_BUF_HEIGHT    = 0,    // 0 = no override; was never actually wired up for this board (see GuiManager.cpp)
    .BUFFER_SIZE_PX     = 1024 * 20, // Width x 20 rows

};
inline const Fleet_BSP& cfg = WS_S3_TouchLCD5B_LCD;


const Fleet_Hardware_Config WS_S3_TouchLCD5B_Hardware = {

    // No onboard audio codec on this board - it's a display/IO dev board,
    // not one of the fleet's voice-assistant boards. AudioManager::begin()
    // still runs unconditionally from the shared test UI, though, and its
    // I2S channel init unconditionally claims hw_cfg.I2S_MCLK/BCLK/LRCK/DOUT/DIN
    // - left at their zero defaults those pins would be GPIO0, which is this
    // board's G1 RGB data line. I2S_SDA/SCL_PIN are pointed at the real
    // (harmless) shared I2C bus rather than left at 0/0 for the same reason -
    // see conversation notes; AudioManager likely needs a HAS_ES7210/HAS_ES8311
    // guard around I2S init before this board can safely run the common test app.
    .I2S_SDA_PIN    = 8,
    .I2S_SCL_PIN    = 9,

    .I2S_AMP_EN = -1, // No amp on this board

    // --= Button =--
    .BOOT_BUTTON_PIN = 0,

    // --= SPI Pins =--
    .SPI_MOSI   = 11,
    .SPI_MISO   = 13,
    .SPI_CLK    = 12,

    // --= SD Card Interface =--
    // CS lives on the CH422G expander (cfg.EXIO_SD_CS), not a native GPIO -
    // no SD component exists yet to consume it.
    .TF_CLK     = 12,
    .TF_MOSI    = 11,
    .TF_MISO    = 13,

};
inline const Fleet_Hardware_Config& hw_cfg = WS_S3_TouchLCD5B_Hardware;

#endif // BSP_WS_S3_TOUCH_LCD_5B_H
