#pragma once
#ifndef BSP_WS_S3_TOUCH_LCD_4B_H
#define BSP_WS_S3_TOUCH_LCD_4B_H
#include <Arduino_GFX_Library.h>
#include "Fleet_BSP.h"

// -------------------------------------------------------------------------
// Board: WaveShare S3 Smart86 Box (ESP32-S3 N16R8)
// Driver: ST7701 (RGB)
// Resolution: 480x480
// -------------------------------------------------------------------------

#define WS_S3_4B

// Panel init commands (ST7701)
// NOTE: Must stay here, immediately before the structs below - see
// BSP_WS_P4_TOUCH_LCD_7B.h for why (sizeof() on this array needs its
// complete type at use-site).
static const uint8_t ws_s3_touch_lcd_4b_init[] = {
//  {cmd, { data }, data_size, delay_ms}
    BEGIN_WRITE,
    WRITE_COMMAND_8, 0xFF,
    WRITE_BYTES, 5, 0x77, 0x01, 0x00, 0x00, 0x10,

    WRITE_C8_D16, 0xC0, 0x3B, 0x00,
    WRITE_C8_D16, 0xC1, 0x0D, 0x02,
    WRITE_C8_D16, 0xC2, 0x31, 0x05,
    WRITE_C8_D8, 0xCD, 0x08,

    WRITE_COMMAND_8, 0xB0, // Positive Voltage Gamma Control
    WRITE_BYTES, 16,
    0x00, 0x11, 0x18, 0x0E,
    0x11, 0x06, 0x07, 0x08,
    0x07, 0x22, 0x04, 0x12,
    0x0F, 0xAA, 0x31, 0x18,

    WRITE_COMMAND_8, 0xB1, // Negative Voltage Gamma Control
    WRITE_BYTES, 16,
    0x00, 0x11, 0x19, 0x0E,
    0x12, 0x07, 0x08, 0x08,
    0x08, 0x22, 0x04, 0x11,
    0x11, 0xA9, 0x32, 0x18,

    // PAGE1
    WRITE_COMMAND_8, 0xFF,
    WRITE_BYTES, 5, 0x77, 0x01, 0x00, 0x00, 0x11,

    WRITE_C8_D8, 0xB0, 0x60, // Vop=4.7375v
    WRITE_C8_D8, 0xB1, 0x32, // VCOM=32
    WRITE_C8_D8, 0xB2, 0x07, // VGH=15v
    WRITE_C8_D8, 0xB3, 0x80,
    WRITE_C8_D8, 0xB5, 0x49, // VGL=-10.17v
    WRITE_C8_D8, 0xB7, 0x85,
    WRITE_C8_D8, 0xB8, 0x21, // AVDD=6.6 & AVCL=-4.6
    WRITE_C8_D8, 0xC1, 0x78,
    WRITE_C8_D8, 0xC2, 0x78,

    WRITE_COMMAND_8, 0xE0,
    WRITE_BYTES, 3, 0x00, 0x1B, 0x02,

    WRITE_COMMAND_8, 0xE1,
    WRITE_BYTES, 11,
    0x08, 0xA0, 0x00, 0x00,
    0x07, 0xA0, 0x00, 0x00,
    0x00, 0x44, 0x44,

    WRITE_COMMAND_8, 0xE2,
    WRITE_BYTES, 12,
    0x11, 0x11, 0x44, 0x44,
    0xED, 0xA0, 0x00, 0x00,
    0xEC, 0xA0, 0x00, 0x00,

    WRITE_COMMAND_8, 0xE3,
    WRITE_BYTES, 4, 0x00, 0x00, 0x11, 0x11,

    WRITE_C8_D16, 0xE4, 0x44, 0x44,

    WRITE_COMMAND_8, 0xE5,
    WRITE_BYTES, 16,
    0x0A, 0xE9, 0xD8, 0xA0,
    0x0C, 0xEB, 0xD8, 0xA0,
    0x0E, 0xED, 0xD8, 0xA0,
    0x10, 0xEF, 0xD8, 0xA0,

    WRITE_COMMAND_8, 0xE6,
    WRITE_BYTES, 4, 0x00, 0x00, 0x11, 0x11,

    WRITE_C8_D16, 0xE7, 0x44, 0x44,

    WRITE_COMMAND_8, 0xE8,
    WRITE_BYTES, 16,
    0x09, 0xE8, 0xD8, 0xA0,
    0x0B, 0xEA, 0xD8, 0xA0,
    0x0D, 0xEC, 0xD8, 0xA0,
    0x0F, 0xEE, 0xD8, 0xA0,

    WRITE_COMMAND_8, 0xEB,
    WRITE_BYTES, 7,
    0x02, 0x00, 0xE4, 0xE4,
    0x88, 0x00, 0x40,

    WRITE_C8_D16, 0xEC, 0x3C, 0x00,

    WRITE_COMMAND_8, 0xED,
    WRITE_BYTES, 16,
    0xAB, 0x89, 0x76, 0x54,
    0x02, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0x20,
    0x45, 0x67, 0x98, 0xBA,

    //-----------VAP & VAN---------------
    WRITE_COMMAND_8, 0xFF,
    WRITE_BYTES, 5, 0x77, 0x01, 0x00, 0x00, 0x13,

    WRITE_C8_D8, 0xE5, 0xE4,

    WRITE_COMMAND_8, 0xFF,
    WRITE_BYTES, 5, 0x77, 0x01, 0x00, 0x00, 0x00,

    WRITE_COMMAND_8, 0x21,   // 0x20 normal, 0x21 IPS
    WRITE_C8_D8, 0x3A, 0x66, // 0x70 RGB888, 0x60 RGB666-18bit, 0x50 RGB565-16bit - WS: 0x66

    WRITE_COMMAND_8, 0x11, // Sleep Out
    END_WRITE,

    DELAY, 120,

    BEGIN_WRITE,
    WRITE_COMMAND_8, 0x29, // Display On
    END_WRITE,
};

const BoardHardware WS_S3_TOUCH_LCD_4B_HARDWARE = {
    .device_name  = "Waveshare S3-Touch-LCD-4B",
    .MANUFACTURER = "Waveshare",
    .MODEL        = "ESP32-S3-Touch-LCD-4B",
    .SI_REV       = "n/a", // ESP32-S3, not P4 - the rev1_3/rev3_x silicon-revision distinction doesn't apply here

    .SDA_PIN = 47,    // Header Pin 14
    .SCL_PIN = 48,    // Header Pin 3
    .I2C_CLOCK_SPEED = 400000,

    .BOOT_BUTTON_PIN = 0,     // Header Pin 14 - PWR / Key3
    // Set on this board because this board uses the IO Expander (EXIO3),
    // which is managed by DisplayManager, not AudioManager.
    .I2S_AMP_EN      = 3,    // = EXIO3 - Must use expander to control
};
inline const BoardHardware& bsp_hw = WS_S3_TOUCH_LCD_4B_HARDWARE;

const ExpanderConfig WS_S3_TOUCH_LCD_4B_EXPANDER = {
    .I2C_ADDR = 0x20,
    .LCD_CS   = 0,     // Header Pin 6
    .LCD_MOSI = 1,     // Header Pin 8
    .LCD_SCK  = 2,     // Header Pin 10
    .AMP_EN   = 3,     // Header Pin 12
    .HDR_14   = 4,     // Header Pin 14 - PWR / Key3
    .TP_RST   = 5,     // Header Pin 16
    .TP_INT   = 6,     // Header Pin 18
    .LCD_RST  = 7,     // Header Pin 17
};
inline const ExpanderConfig& bsp_expander = WS_S3_TOUCH_LCD_4B_EXPANDER;

const DisplayConfig WS_S3_TOUCH_LCD_4B_DISPLAY = {
    .PANEL_MODEL = "ST7701",
    .WIDTH       = 480,
    .HEIGHT      = 480,
    .ROTATION    = 0,     // 0 = USB Port on right side
    .AUTO_FLUSH  = true,

    .BL_PIN      = 4,
    .BL_ON_LEVEL = 0, // Active LOW for P4 GPIO
    .BL_FREQ     = 25000, // WS: 5000 Hz PWM

    .RST   = -1,    // EXIO 7

    // ---= RGB Interface =---
    .DE    = 17,
    .VSYNC = 3,
    .HSYNC = 46,
    .PCLK  = 9,

    /*
        .R0 = 40, .R1 = 41, .R2 = 42, .R3 = 2,  .R4 = 1,
        .G0 = 21, .G1 = 8,  .G2 = 18, .G3 = 45, .G4 = 38, .G5 = 39,
        .B0 = 10, .B1 = 11, .B2 = 12, .B3 = 13, .B4 = 14,
    */

    // Flipped R & B rows
    .R0 = 10, .R1 = 11, .R2 = 12, .R3 = 13, .R4 = 14,
    .G0 = 21, .G1 = 8,  .G2 = 18, .G3 = 45, .G4 = 38, .G5 = 39,
    .B0 = 40, .B1 = 41, .B2 = 42, .B3 = 2,  .B4 = 1,

    // --= RGB Timing =---
    .HSYNC_POL    = 1,
    .VSYNC_POL    = 1,
    .HSYNC_PWIDTH = 10,   // 8,    // 10,
    .HSYNC_BPORCH = 10,   // 50,   // 10,
    .HSYNC_FPORCH = 20,   // 10,   // 20,
    .VSYNC_PWIDTH = 10,   // 8,    // 10,
    .VSYNC_BPORCH = 20,
    .VSYNC_FPORCH = 10,

    .PCLK_ACTIVE_NEG = 0,
    .PCLK_HZ         = 12500000,
    .PREFER_SPEED    = 16000000, // 16000000,
    .USE_BIG_ENDIAN  = false,
    .BOUNCE_BUFFER_SIZE_PX = 9600, // 480x20

    // ---= Init Commands =---
    .INIT_CMDS_RGB  = ws_s3_touch_lcd_4b_init,
    .INIT_CMDS_SIZE = sizeof(ws_s3_touch_lcd_4b_init),
};
inline const DisplayConfig& bsp_display = WS_S3_TOUCH_LCD_4B_DISPLAY;

const TouchConfig WS_S3_TOUCH_LCD_4B_TOUCH = {
    .NAME      = "GT911",
    .I2C_ADDR  = 0x5D,
    .SDA       = 47,
    .SCL       = 48,
    // .INT    = EXIO 6,
    // .RST    = EXIO 5,
    .MAX_TOUCH = 5,
};
inline const TouchConfig& bsp_touch = WS_S3_TOUCH_LCD_4B_TOUCH;

const AudioConfig WS_S3_TOUCH_LCD_4B_AUDIO = {
    // ES8311 / 7210 Codec + NS4150B PA
    .I2S_SDA_PIN    = 47,   // I2C SDA
    .I2S_SCL_PIN    = 48,   // I2C SCL
    .I2S_7210_ADDR  = 0x40, // ES7210 ADC/Mics
    .I2S_8311_ADDR  = 0x18, // ES8311 DAC/Amp

    .I2S_MCLK = 5,
    .I2S_BCLK = 16,   //SCLK
    .I2S_LRCK = 7,    //WS

    .I2S_DIN  = 15,   // From ADC ES7210
    .I2S_DOUT = 6,    // To Codec ES8311

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

    // Mic Settings (Standard)
    .MIC_SELECTED = 0x03, // Mic1 | Mic2
    .MIC_GAIN_DB  = 13,   // GAIN_36DB

    // Mic Settings (AEC / Loopback)
    .AEC_MIC_SELECTED = 0x0F, // Mic1 | Mic2 | Mic3 | Mic4
    .AEC_MIC_GAIN_DB  = 4,    // GAIN_12DB (Conservative start for AEC)
};
inline const AudioConfig& bsp_audio = WS_S3_TOUCH_LCD_4B_AUDIO;

const StorageConfig WS_S3_TOUCH_LCD_4B_STORAGE = {
    .TF_CMD  = 44,
    .TF_CLK  = 43,
    .TF_CS   = 45,
    .TF_MOSI = 42,
    .TF_MISO = 41,
    .TF_D0   = 40,
    // .TF_D1 = -1, // Not used
    // .TF_D2 = -1, // Not used

    .SPI_MOSI = 42,
    .SPI_MISO = 41,
    .SPI_CS   = 45,
    .SPI_CLK  = 43,
};
inline const StorageConfig& bsp_storage = WS_S3_TOUCH_LCD_4B_STORAGE;

const LvglConfig WS_S3_TOUCH_LCD_4B_LVGL = {
    .DOUBLE_BUFFERING = true,
    .DRAW_BUF_HEIGHT  = 0,    // 0 = no override; was never actually wired up for this board (see GuiManager.cpp)
    .BUFFER_SIZE_PX   = 480 * 20, // Width x 20 rows
};
inline const LvglConfig& bsp_lvgl = WS_S3_TOUCH_LCD_4B_LVGL;

#endif // BSP_WS_S3_TOUCH_LCD_4B_H
