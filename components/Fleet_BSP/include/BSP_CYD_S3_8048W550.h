#pragma once
#ifndef BSP_CYD_S3_8048W550_H
#define BSP_CYD_S3_8048W550_H

#include <Arduino_GFX_Library.h>
#include "Fleet_BSP.h"

#define DEBUG_DISPLAY 1

// -------------------------------------------------------------------------
// Board: Guition JC8048W550 (ESP32-S3 N16R8)
// Driver: ST7262 (RGB)
// Resolution: 800x480
// -------------------------------------------------------------------------

#define CYD_S3_8048

const BoardHardware CYD_S3_8048W550_HARDWARE = {
    .device_name  = "CYD S3 JC8048W550",
    .MANUFACTURER = "Guition",
    .MODEL        = "JC8048W550",
    .SI_REV       = "n/a", // ESP32-S3, not P4

    .SDA_PIN = 19,
    .SCL_PIN = 20,
    .I2C_CLOCK_SPEED = 400000,

    .BOOT_BUTTON_PIN = 0,
    .BAT_ADC         = 17,  // IP5306 battery monitor
    .I2S_AMP_EN      = -1,  // No direct amp control
};
inline const BoardHardware& bsp_hw = CYD_S3_8048W550_HARDWARE;

const DisplayConfig CYD_S3_8048W550_DISPLAY = {
    .PANEL_MODEL = "ST7262",
    .WIDTH       = 800,
    .HEIGHT      = 480,
    .ROTATION    = 0, // 0 = Landscape (USB on bottom), 1 = Portrait (USB left side), 2 = Landscape Inverted, 3 = Portrait Inverted
    .AUTO_FLUSH  = true,

    .BL_PIN      = 2,
    .BL_ON_LEVEL = 1,     // Active HIGH
    .BL_FREQ     = 0,     // Not PWM

    .RST = -1,

    // ---= RGB Interface =---
    .DE    = 40,
    .VSYNC = 41,
    .HSYNC = 39,
    .PCLK  = 42,

    .R0 = 45, .R1 = 48, .R2 = 47, .R3 = 21, .R4 = 14,
    .G0 = 5,  .G1 = 6,  .G2 = 7,  .G3 = 15, .G4 = 16, .G5 = 4,
    .B0 = 8,  .B1 = 3,  .B2 = 46, .B3 = 9,  .B4 = 1,

    // --= RGB Timing =---
    .HSYNC_POL    = 0,
    .VSYNC_POL    = 0,
    .HSYNC_PWIDTH = 4,
    .HSYNC_BPORCH = 8,
    .HSYNC_FPORCH = 8,
    .VSYNC_PWIDTH = 4,
    .VSYNC_BPORCH = 8,
    .VSYNC_FPORCH = 8,

    .PCLK_ACTIVE_NEG = 1,
    .PREFER_SPEED    = 16000000, // 12000000, // PCLK_HZ
    .USE_BIG_ENDIAN  = false,
    // Explicitly set to match what Arduino_ESP32RGBPanel.cpp used to hardcode
    // unconditionally (480 * 20) before it started respecting this field -
    // 800 * 12 is the same 9600px bounce buffer, still a whole number of
    // scanlines at this board's width. Preserves this board's exact existing
    // (working) behavior.
    .BOUNCE_BUFFER_SIZE_PX = 800 * 12,
};
inline const DisplayConfig& bsp_display = CYD_S3_8048W550_DISPLAY;

const TouchConfig CYD_S3_8048W550_TOUCH = {
    .NAME      = "GT911",
    .I2C_ADDR  = 0x5D,
    .SDA       = 19,
    .SCL       = 20,
    .INT       = -1, // 18
    .RST       = 38,
    .MAX_TOUCH = 5,
};
inline const TouchConfig& bsp_touch = CYD_S3_8048W550_TOUCH;

const AudioConfig CYD_S3_8048W550_AUDIO = {
    // NS4168 power amp
    .I2S_BCLK = 0,   // 19
    .I2S_LRCK = 18,
    .I2S_DOUT = 17,

    .AUDIO_INPUT_SAMPLE_RATE  = 16000,
    .AUDIO_OUTPUT_SAMPLE_RATE = 16000,
    .I2S_MCLK_MULTIPLE        = 256,

    .I2S_DATA_BIT_WIDTH = 16, // I2S_DATA_BIT_WIDTH_16BIT
    .I2S_SLOT_BIT_WIDTH = 16, // I2S_SLOT_BIT_WIDTH_16BIT
    .I2S_SLOT_MODE      = 2,  // I2S_SLOT_MODE_STEREO
};
inline const AudioConfig& bsp_audio = CYD_S3_8048W550_AUDIO;

const StorageConfig CYD_S3_8048W550_STORAGE = {
    .TF_CMD  = 11,  // MCU_MOSI && SPI_MOSI
    .TF_CLK  = 12,
    .TF_CS   = 10,  // TF_D3
    .TF_MOSI = 11,  // TF_CMD && SPI_MOSI
    .TF_MISO = 13,  // TF_D0 && SPI_MISO
    .TF_D0   = 13,  // TF_MISO && SPI_MISO
    .TF_D3   = 10,  // TF_CS

    .SPI_MOSI = 11,   // Demo refers to TF_CMD or TF_MOSI
    .SPI_MISO = 13,   // Demo refers to TF_D0 or TF_MISO
    .SPI_CS   = 10,   // Demo refers to TF_D3 or TF_CS
    .SPI_CLK  = 12,   // Demo refers to TF_CLK
};
inline const StorageConfig& bsp_storage = CYD_S3_8048W550_STORAGE;

const LvglConfig CYD_S3_8048W550_LVGL = {
    .DOUBLE_BUFFERING = false,
    .DRAW_BUF_HEIGHT  = 0,    // 0 = no override; was never actually wired up for this board (see GuiManager.cpp)
    .BUFFER_SIZE_PX   = 800 * 20, // Width x 20 rows
};
inline const LvglConfig& bsp_lvgl = CYD_S3_8048W550_LVGL;

#endif // BSP_CYD_S3_8048W550_H
