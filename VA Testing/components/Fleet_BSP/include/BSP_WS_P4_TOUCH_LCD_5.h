#pragma once
#ifndef BSP_WS_P4_TOUCH_LCD_5_H
#define BSP_WS_P4_TOUCH_LCD_5_H

#include "Arduino_GFX_Library.h"
#include "Fleet_BSP.h"

// -------------------------------------------------------------------------
// Board: WaveShare P4 Touch LCD 5 (ESP32-P4 + C6)
// Driver: HX8394 (MIPI DSI)
// Resolution: 720x1280
// -------------------------------------------------------------------------

#define WS_P4_5

// Panel init commands (HX8394)
// NOTE: Must stay here, immediately before the structs below - see
// BSP_WS_P4_TOUCH_LCD_7B.h for why (sizeof() on this array needs its
// complete type at use-site).
static const lcd_init_cmd_t ws_p4_touch_lcd_5_init[] = {
    // Pre-table commands sent by the audited esp_lcd_hx8394 driver before the
    // vendor sequence: sleep-out, MADCTL, 16-bit COLMOD and 2-lane DSI config.
    {0x11, (uint8_t[]){0x00}, 1, 120},
    {0x36, (uint8_t[]){0x00}, 1, 0},
    {0x3A, (uint8_t[]){0x55}, 1, 0},
    {0xBA, (uint8_t[]){0x61}, 1, 0},
    {0xB9, (uint8_t[]){0xFF, 0x83, 0x94}, 3, 0},
    {0xB1, (uint8_t[]){0x48, 0x0A, 0x6A, 0x09, 0x33, 0x54, 0x71, 0x71, 0x2E, 0x45}, 10, 0},
    {0xBA, (uint8_t[]){0x61, 0x03, 0x68, 0x6B, 0xB2, 0xC0}, 6, 0},
    {0xB2, (uint8_t[]){0x00, 0x80, 0x64, 0x0C, 0x06, 0x2F}, 6, 0},
    {0xB4, (uint8_t[]){0x1C, 0x78, 0x1C, 0x78, 0x1C, 0x78, 0x01, 0x0C, 0x86, 0x75, 0x00, 0x3F, 0x1C, 0x78, 0x1C, 0x78, 0x1C, 0x78, 0x01, 0x0C, 0x86}, 21, 0},
    {0xD3, (uint8_t[]){0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x08, 0x08, 0x32, 0x10, 0x05, 0x00, 0x05, 0x32, 0x13, 0xC1, 0x00, 0x01, 0x32, 0x10, 0x08, 0x00, 0x00, 0x37, 0x03, 0x07, 0x07, 0x37, 0x05, 0x05, 0x37, 0x0C, 0x40}, 33, 0},
    {0xD5, (uint8_t[]){0x18, 0x18, 0x18, 0x18, 0x22, 0x23, 0x20, 0x21, 0x04, 0x05, 0x06, 0x07, 0x00, 0x01, 0x02, 0x03, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x19, 0x19, 0x19, 0x19}, 44, 0},
    {0xD6, (uint8_t[]){0x18, 0x18, 0x19, 0x19, 0x21, 0x20, 0x23, 0x22, 0x03, 0x02, 0x01, 0x00, 0x07, 0x06, 0x05, 0x04, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x19, 0x19, 0x18, 0x18}, 44, 0},
    {0xE0, (uint8_t[]){0x07, 0x08, 0x09, 0x0D, 0x10, 0x14, 0x16, 0x13, 0x24, 0x36, 0x48, 0x4A, 0x58, 0x6F, 0x76, 0x80, 0x97, 0xA5, 0xA8, 0xB5, 0xC6, 0x62, 0x63, 0x68, 0x6F, 0x72, 0x78, 0x7F, 0x7F, 0x00, 0x02, 0x08, 0x0D, 0x0C, 0x0E, 0x0F, 0x10, 0x24, 0x36, 0x48, 0x4A, 0x58, 0x6F, 0x78, 0x82, 0x99, 0xA4, 0xA0, 0xB1, 0xC0, 0x5E, 0x5E, 0x64, 0x6B, 0x6C, 0x73, 0x7F, 0x7F}, 58, 0},
    {0xCC, (uint8_t[]){0x0B}, 1, 0},
    {0xC0, (uint8_t[]){0x1F, 0x73}, 2, 0},
    {0xB6, (uint8_t[]){0x6B, 0x6B}, 2, 0},
    {0xD4, (uint8_t[]){0x02}, 1, 0},
    {0xBD, (uint8_t[]){0x01}, 1, 0},
    {0xB1, (uint8_t[]){0x00}, 1, 0},
    {0xBD, (uint8_t[]){0x00}, 1, 0},
    {0xBF, (uint8_t[]){0x40, 0x81, 0x50, 0x00, 0x1A, 0xFC, 0x01}, 7, 0},
    {0x3A, (uint8_t[]){0x50}, 1, 0},
    {0x11, (uint8_t[]){0x00}, 1, 200},
    {0xB2, (uint8_t[]){0x00, 0x80, 0x64, 0x0C, 0x06, 0x2F, 0x00, 0x00, 0x00, 0x00, 0xC0, 0x18}, 12, 0},
    {0x29, (uint8_t[]){0x00}, 1, 80},
};

const BoardHardware WS_P4_TOUCH_LCD_5_HARDWARE = {
    .device_name = "Waveshare ESP32-P4-Touch-LCD-5",
    .MANUFACTURER = "Waveshare",
    .MODEL        = "ESP32-P4-WIFI6-Touch-LCD-5",
    .SI_REV       = "unconfirmed",

    .SDA_PIN = 7,
    .SCL_PIN = 8,
    // Vendor GT911 driver uses 400kHz (i2c.h EXAMPLE_I2C_MASTER_FREQUENCY) -
    // the only value that ever actually reached hardware; the vendor's
    // separate "general bus" 100kHz field was confirmed dead (never read by
    // any of their own init code) during this board's bring-up.
    .I2C_CLOCK_SPEED = 400000,

    .I2S_AMP_EN = 53,
};
inline const BoardHardware& bsp_hw = WS_P4_TOUCH_LCD_5_HARDWARE;

const DisplayConfig WS_P4_TOUCH_LCD_5_DISPLAY = {
    .PANEL_MODEL = "HX8394",
    .WIDTH       = 720,
    .HEIGHT      = 1280,
    .ROTATION    = 0,     // Confirmed via vendor displays_config.h (SCREEN_DEFAULT.rotation = 0, swap_xy/mirror all 0)
    .AUTO_FLUSH  = true,

    // LCD backlight is driven by GPIO26 on this board, exactly like the
    // audited BSP: a 5 kHz / 10-bit LEDC PWM signal (duty 1023 = 100%
    // brightness). The panel's backlight path AC-couples the PWM signal,
    // so a static GPIO level does not light the LEDs.
    .BL_PIN      = 26,
    .BL_ON_LEVEL = 1,      // Active HIGH
    .BL_FREQ     = 5000,   // 5 kHz PWM

    .RST = 27,

    // ---= MIPI Timing =---
    // LCD-5 single screen profile. DSI timing matches the audited
    // ESP-IDF BSP: 720x1280 @30Hz, DPI 58 MHz, 2 lanes @ 700 Mbps.
    .HSYNC_PWIDTH = 20,
    .HSYNC_BPORCH = 20,
    .HSYNC_FPORCH = 40,
    .VSYNC_PWIDTH = 4,
    .VSYNC_BPORCH = 10,
    .VSYNC_FPORCH = 24,

    .PREFER_SPEED  = 58000000,
    .LANE_BIT_RATE = 700,

    // ---= Init Commands =---
    .INIT_CMDS_DSI  = ws_p4_touch_lcd_5_init,
    .INIT_CMDS_SIZE = sizeof(ws_p4_touch_lcd_5_init) / sizeof(lcd_init_cmd_t),
};
inline const DisplayConfig& bsp_display = WS_P4_TOUCH_LCD_5_DISPLAY;

const TouchConfig WS_P4_TOUCH_LCD_5_TOUCH = {
    .NAME = "GT911",
    // Vendor driver probes 0x5D first (INT low at reset), falls back to
    // 0x14 (INT high at reset) - same ambiguity as WS_P4_7B. INT is left
    // NC below (matches vendor, which never actively drives it), so both
    // addresses are worth trying if touch isn't detected on hardware.
    .I2C_ADDR        = 0x5D,
    .I2C_BACKUP_ADDR = 0x14,
    .SDA             = 7,
    .SCL             = 8,
    .INT             = -1,
    .RST             = -1,
    .MAX_TOUCH       = 5,
};
inline const TouchConfig& bsp_touch = WS_P4_TOUCH_LCD_5_TOUCH;

const AudioConfig WS_P4_TOUCH_LCD_5_AUDIO = {
    // ES8311 / ES7210 Codec + amp - pins/addresses confirmed identical to
    // WS_P4_7B via vendor 09_Audio_Playback/10_Mic_Record examples.
    .I2S_SDA_PIN   = 7,   // I2C SDA
    .I2S_SCL_PIN   = 8,   // I2C SCL
    .I2S_7210_ADDR = 0x40, // ES7210 I2C Address
    .I2S_8311_ADDR = 0x18, // ES8311 I2C Address

    .I2S_MCLK = 13,
    .I2S_BCLK = 12,
    .I2S_LRCK = 10,

    .I2S_DIN  = 11, // ES7210 Capture
    .I2S_DOUT = 9,  // ES8311 Playback

    .AUDIO_INPUT_SAMPLE_RATE  = 16000,
    .AUDIO_OUTPUT_SAMPLE_RATE = 16000,
    .I2S_MCLK_MULTIPLE        = 256,

    .I2S_DATA_BIT_WIDTH = 16, // I2S_DATA_BIT_WIDTH_16BIT
    .I2S_SLOT_BIT_WIDTH = 16, // I2S_SLOT_BIT_WIDTH_16BIT
    .I2S_SLOT_MODE      = 2,  // I2S_SLOT_MODE_STEREO

    // Codec Config (ES7210)
    .CODEC_INPUT_MODE       = 0,  // AUDIO_HAL_ADC_INPUT_LINE1 (Mic 1/2)
    .CODEC_CODEC_MODE       = 1,  // AUDIO_HAL_CODEC_MODE_ENCODE
    .CODEC_IFACE_I2S_FMT    = 0,  // AUDIO_HAL_I2S_NORMAL
    .CODEC_IFACE_SAMPLES    = 2,  // AUDIO_HAL_16K_SAMPLES
    .CODEC_IFACE_BIT_LENGTH = 1,  // AUDIO_HAL_BIT_LENGTH_16BITS

    // Codec Config (ES8311)
    .DAC_BIT_LENGTH = 16, // ES8311_RESOLUTION_16

    // Mic Settings (Standard, non-AEC path - this is what actually runs,
    // ENABLE_AEC is off fleet-wide). Mirrors WS_P4_7B's tested/working
    // Mic1|Mic2 @ 36dB convention rather than the vendor's own
    // 10_Mic_Record.ino example (see note below) - keeps this board's
    // day-to-day audio path consistent with the rest of the fleet.
    .MIC_SELECTED = 0x03, // Mic1 | Mic2
    .MIC_GAIN_DB  = 13,   // GAIN_36DB

    // Mic Settings (AEC / Loopback - unused while ENABLE_AEC stays off).
    // NOTE: vendor's own 10_Mic_Record.ino for THIS board configures all 4
    // ES7210 channels with asymmetric gain per pair - Mic1|Mic2 @ GAIN_0DB,
    // Mic3|Mic4 @ GAIN_37_5DB - not a single uniform gain like WS_P4_7B's
    // AEC defaults below. bsp_audio only has one AEC_MIC_GAIN_DB scalar
    // (es7210_adc_set_gain applies the same gain to every mic in the mask),
    // so that asymmetric scheme can't be represented here as-is. Left at
    // WS_P4_7B's values for now since AEC work is frozen (see
    // FUTURE_IMPROVEMENTS.md); revisit this if AEC development ever resumes.
    .AEC_MIC_SELECTED = 0x0F, // Mic1 | Mic2 | Mic3 | Mic4
    .AEC_MIC_GAIN_DB  = 4,    // GAIN_12DB
};
inline const AudioConfig& bsp_audio = WS_P4_TOUCH_LCD_5_AUDIO;

const StorageConfig WS_P4_TOUCH_LCD_5_STORAGE = {
    .TF_CMD = 44,
    .TF_CLK = 43,
    .TF_D0  = 39,
    .TF_D1  = 40,
    .TF_D2  = 41,
    .TF_D3  = 42,
};
inline const StorageConfig& bsp_storage = WS_P4_TOUCH_LCD_5_STORAGE;

const LvglConfig WS_P4_TOUCH_LCD_5_LVGL = {
    .DOUBLE_BUFFERING = true,
    .DRAW_BUF_HEIGHT  = 50,
    .BUFFER_SIZE_PX   = 36000, // 720 * 50, // WIDTH * DRAW_BUF_HEIGHT
    // lv_display_set_dpi(lv_display, 150);
};
inline const LvglConfig& bsp_lvgl = WS_P4_TOUCH_LCD_5_LVGL;

/* I2C Scan Results (from vendor docs, not yet confirmed on this project's
   own hardware):
I2C device found at address 0x18  ! // ES8311 DAC/Amp
I2C device found at address 0x40  ! // ES7210 ADC/Mics
I2C device found at address 0x5D  ! // GT911 Touch Panel (or 0x14, see TOUCH.I2C_BACKUP_ADDR)
*/

#endif // BSP_WS_P4_TOUCH_LCD_5_H
