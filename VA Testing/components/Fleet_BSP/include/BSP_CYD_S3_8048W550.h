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

static const Fleet_BSP Guition_8048W550_LCD = {

    .device_name       = "Guition S3 5\" JC8048W550",

    // --=  I2C Bus  =--
    .I2C_SDA_PIN       = 19,
    .I2C_SCL_PIN       = 20,
    
    
    // ---= LCD Definitions =---
    .LCD_MODEL         = "ST7262",
    .WIDTH             = 800,
    .HEIGHT            = 480,
    .ROTATION          = 0, // 0 = Landscape (USB on bottom), 1 = Portrait (USB left side), 2 = Landscape Inverted, 3 = Portrait Inverted
    .AUTO_FLUSH        = true,

    // ---= Backlight =---
    .LCD_BL            = 2,
    .LCD_BL_ON_LEVEL   = 1,     // Active HIGH
    .LCD_BL_FREQ       = 0,     // Not PWM

    // ---= Touch Panel =---
    .TP_NAME            = "GT911",
    .TP_I2C_ADDR        = 0x5D,
    .TP_I2C_CLOCK_SPEED = 400000,
    .TP_SDA             = 19,
    .TP_SCL             = 20,
    .TP_INT             = -1, // 18
    .TP_RST             = 38,
    .TP_MAX_TOUCH       = 5,

    // ---= LCD Control Pins =---
    .LCD_RST           = -1,

    // ---= RGB Interface =---
    .LCD_DE            = 40,
    .LCD_VSYNC         = 41,
    .LCD_HSYNC         = 39,
    .LCD_PCLK          = 42,

    .R0 = 45, .R1 = 48, .R2 = 47, .R3 = 21, .R4 = 14,
    .G0 = 5,  .G1 = 6,  .G2 = 7,  .G3 = 15, .G4 = 16, .G5 = 4,
    .B0 = 8,  .B1 = 3,  .B2 = 46, .B3 = 9,  .B4 = 1,

    // --= RGB Timing =---
    .HSYNC_POL      = 0,
    .VSYNC_POL      = 0,
    .HSYNC_PWIDTH   = 4,
    .HSYNC_BPORCH   = 8,
    .HSYNC_FPORCH   = 8,
    .VSYNC_PWIDTH   = 4,
    .VSYNC_BPORCH   = 8,
    .VSYNC_FPORCH   = 8,

    .PCLK_ACTIVE_NEG    = 1,
    .PREFER_SPEED       = 16000000, // 12000000, // PCLK_HZ
    .USE_BIG_ENDIAN     = false,
    // Explicitly set to match what Arduino_ESP32RGBPanel.cpp used to hardcode
    // unconditionally (480 * 20) before it started respecting this field -
    // 800 * 12 is the same 9600px bounce buffer, still a whole number of
    // scanlines at this board's width. Preserves this board's exact existing
    // (working) behavior.
    .BOUNCE_BUFFER_SIZE_PX = 800 * 12,

    // --= LVGL Settings =--
    .DOUBLE_BUFFERING   = false,
    .DRAW_BUF_HEIGHT    = 0,    // 0 = no override; was never actually wired up for this board (see GuiManager.cpp)
    .BUFFER_SIZE_PX     = 800 * 20, // Width x 20 rows

};
inline const Fleet_BSP& cfg = Guition_8048W550_LCD;


const Fleet_Hardware_Config Guition_8048W550_Hardware = {
    
    // IP5306 battery monitor

    // --= Audio Codec =--
    // --= NS4168 power amp =--
    .I2S_BCLK   = 0,   // 19
    .I2S_LRCK   = 18,
    .I2S_DOUT   = 17,

    .AUDIO_INPUT_SAMPLE_RATE    = 16000,
    .AUDIO_OUTPUT_SAMPLE_RATE   = 16000,
    .I2S_MCLK_MULTIPLE          = 256,

    // --= New Audio Settings =--
    .I2S_DATA_BIT_WIDTH = 16, // I2S_DATA_BIT_WIDTH_16BIT
    .I2S_SLOT_BIT_WIDTH = 16, // I2S_SLOT_BIT_WIDTH_16BIT
    .I2S_SLOT_MODE      = 2,  // 1 = MONO, 2 = Stereo (I2S_SLOT_MODE_STEREO)

    .I2S_AMP_EN = -1, // No direct amp control

    // --= Button =--
    
    .BOOT_BUTTON_PIN = 0,

    // --= SPI Pins =--
    .SPI_MOSI   = 11,   // Demo refers to TF_CMD or TF_MOSI
    .SPI_MISO   = 13,   // Demo refers to TF_D0 or TF_MISO
    .SPI_CS     = 10,   // Demo refers to TF_D3 or TF_CS
    .SPI_CLK    = 12,   // Demo refers to TF_CLK

    // --= SD Card Interface =--
    .TF_CMD     = 11,  // MCU_MOSI && SPI_MOSI
    .TF_CLK     = 12,
    .TF_CS      = 10,  // TF_D3
    .TF_MOSI    = 11,  // TF_CMD && SPI_MOSI
    .TF_MISO    = 13,  // TF_D0 && SPI_MISO
    .TF_D0      = 13,  // TF_MISO && SPI_MISO
    .TF_D3      = 10,  // TF_CS

    // --= IP5306 Battery Management =--
    .BAT_ADC    = 17,

};
inline const Fleet_Hardware_Config& hw_cfg = Guition_8048W550_Hardware;

#endif // BSP_CYD_S3_8048W550_H