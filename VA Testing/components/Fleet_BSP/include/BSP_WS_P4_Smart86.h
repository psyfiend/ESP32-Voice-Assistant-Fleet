#pragma once
#ifndef BSP_WS_P4_SMART86_H
#define BSP_WS_P4_SMART86_H
#include <Arduino_GFX_Library.h>
#include "Fleet_BSP_P4.h"

// -------------------------------------------------------------------------
// Board: WaveShare P4 Smart86 Box (ESP32-P4 + C6)
// Driver: ST7703 (MIPI DSI)
// Resolution: 720x720
// -------------------------------------------------------------------------

// Panel init commands (ST7703)
static const lcd_init_cmd_t waveshare_p4_smart86_init[] = {
    {0xB9, (uint8_t[]){0xF1, 0x12, 0x83}, 3, 0},

    {0xBA, (uint8_t[]){0x31, 0x81, 0x05, 0xF9, 0x0E, 0x0E, 0x20, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x44, 0x25, 0x00, 0x90, 0x0A, 0x00, 0x00, 0x01, 0x4F, 0x01, 0x00, 0x00, 0x37}, 27, 0},
    {0xB8, (uint8_t[]){0x25, 0x22, 0xF0, 0x63}, 4, 0},

    {0xBF, (uint8_t[]){0x02, 0x11, 0x00}, 3, 0},

    {0xB3, (uint8_t[]){0x10, 0x10, 0x28, 0x28, 0x03, 0xFF, 0x00, 0x00, 0x00, 0x00}, 10, 0},

    {0xC0, (uint8_t[]){0x73, 0x73, 0x50, 0x50, 0x00, 0x00, 0x12, 0x70, 0x00}, 9, 0},

    {0xBC, (uint8_t[]){0x46}, 1, 0},
    {0xCC, (uint8_t[]){0x0B}, 1, 0},
    {0xB4, (uint8_t[]){0x80}, 1, 0},
    {0xB2, (uint8_t[]){0x3C, 0x12, 0x30}, 3, 0},
    {0xE3, (uint8_t[]){0x07, 0x07, 0x0B, 0x0B, 0x03, 0x0B, 0x00, 0x00, 0x00, 0x00, 0xFF, 0x00, 0xC0, 0x10}, 14, 0},
    {0xC1, (uint8_t[]){0x36, 0x00, 0x32, 0x32, 0x77, 0xF1, 0xCC, 0xCC, 0x77, 0x77, 0x33, 0x33}, 12, 0},
    {0xB5, (uint8_t[]){0x0A, 0x0A}, 2, 0},
    {0xB6, (uint8_t[]){0xB2, 0xB2}, 2, 0},
    {0xE9, (uint8_t[]){0xC8, 0x10, 0x0A, 0x10, 0x0F, 0xA1, 0x80, 0x12, 0x31, 0x23, 0x47, 0x86, 0xA1, 0x80, 0x47, 0x08, 0x00, 0x00, 0x0D, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0D, 0x00, 0x00, 0x00, 0x48, 0x02, 0x8B, 0xAF, 0x46, 0x02, 0x88, 0x88, 0x88, 0x88, 0x88, 0x48, 0x13, 0x8B, 0xAF, 0x57, 0x13, 0x88, 0x88, 0x88, 0x88, 0x88, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, 63, 0},
    {0xEA, (uint8_t[]){0x96, 0x12, 0x01, 0x01, 0x01, 0x78, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x4F, 0x31, 0x8B, 0xA8, 0x31, 0x75, 0x88, 0x88, 0x88, 0x88, 0x88, 0x4F, 0x20, 0x8B, 0xA8, 0x20, 0x64, 0x88, 0x88, 0x88, 0x88, 0x88, 0x23, 0x00, 0x00, 0x01, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x40, 0xA1, 0x80, 0x00, 0x00, 0x00, 0x00}, 61, 0},
    {0xE0, (uint8_t[]){0x00, 0x0A, 0x0F, 0x29, 0x3B, 0x3F, 0x42, 0x39, 0x06, 0x0D, 0x10, 0x13, 0x15, 0x14, 0x15, 0x10, 0x17, 0x00, 0x0A, 0x0F, 0x29, 0x3B, 0x3F, 0x42, 0x39, 0x06, 0x0D, 0x10, 0x13, 0x15, 0x14, 0x15, 0x10, 0x17}, 34, 0},

    {0x11, (uint8_t[]){0x00}, 1, 250},
    {0x29, (uint8_t[]){0x00}, 1, 50},
};

// Waveshare P4 Smart86 LCD Configuration
const Fleet_BSP WS_P4_SMART86_LCD = {
    .device_name       = "Waveshare P4 Smart86",

    // --= Hardware Flags =--
    #define HAS_MIPI_PANEL 1
    #define HAS_TOUCH 1
    #define HIGH_DPI_DISPLAY 1
    #define HAS_ES8311 1
    #define HAS_ES7210 1
    //#define HAS_IO_EXPANDER
    //#define HAS_RGB_PANEL
    //#define HAS_QSPI_PANEL
    
    // --=  I2C Bus  =--
    .I2C_SDA_PIN       = 7,
    .I2C_SCL_PIN       = 8,
    .I2C_CLOCK_SPEED   = 100000,

    // ---=  LCD  =---
    .LCD_MODEL         = "ST7703",
    .WIDTH             = 720,
    .HEIGHT            = 720,
    .ROTATION          = 2,     // USB Port on left side
    .AUTO_FLUSH        = true,

    // ---= Backlight =---
    .LCD_BL            = 26,
    .LCD_BL_ON_LEVEL   = 0, // 0 = active LOW
    .LCD_BL_FREQ       = 5000, // 5 kHz PWM

    // ---= Touch Panel =---
    .TP_NAME           = "GT911",
    #define TOUCH_PANEL   TOUCH_WS_P4_SMART86
    .TP_I2C_ADDR       = 0x5D,
    .TP_I2C_CLOCK_SPEED = 100000,
    .TP_SDA            = 7,
    .TP_SCL            = 8,
    .TP_INT            = -1,
    .TP_RST            = 23,
    .TP_MAX_TOUCH      = 5,

    // ---= LCD Control Pins =---
    .LCD_RST           = 27,
    
    // ---= MIPI Timing =---
    .HSYNC_PWIDTH   = 20,
    .HSYNC_BPORCH   = 80,
    .HSYNC_FPORCH   = 80,   // 40,
    .VSYNC_PWIDTH   = 4,
    .VSYNC_BPORCH   = 12,
    .VSYNC_FPORCH   = 30,

    .PREFER_SPEED      = 46000000,
    .LANE_BIT_RATE     = 1000,

    // --= LVGL Settings =--
    .DRAW_BUF_HEIGHT    = 50,

    // ---= Init Commands =---
    .INIT_CMDS_DSI     = waveshare_p4_smart86_init,
    .INIT_CMDS_SIZE    = sizeof(waveshare_p4_smart86_init) / sizeof(lcd_init_cmd_t),

};
inline const Fleet_BSP& cfg = WS_P4_SMART86_LCD;

const Fleet_Hardware_Config WS_P4_Smart86_Hardware = {

    // --= Audio Codec =--
    // ES8311 / 7210 Codec + NS4150B PA
    
    .I2S_SDA_PIN   = 7,   // I2C SDA
    .I2S_SCL_PIN   = 8,   // I2C SCL
    .I2S_7210_ADDR  = 0x40,   // ES7210 ADC/Mics
    .I2S_8311_ADDR  = 0x18,   // ES8311 DAC/Amp

    .I2S_MCLK   = 13,
    .I2S_BCLK   = 12,   //SCLK
    .I2S_LRCK   = 10,   //WS
    
    .I2S_DIN    = 11,
    .I2S_DOUT   = 9,

    .AUDIO_INPUT_SAMPLE_RATE    = 16000,
    .AUDIO_OUTPUT_SAMPLE_RATE   = 16000,
    .I2S_MCLK_MULTIPLE          = 256,

    // --= New Audio Settings =--
    .I2S_DATA_BIT_WIDTH = 16, // I2S_DATA_BIT_WIDTH_16BIT
    .I2S_SLOT_BIT_WIDTH = 16, // I2S_SLOT_BIT_WIDTH_16BIT
    .I2S_SLOT_MODE      = 2,  // 1 = MONO, 2 = Stereo (I2S_SLOT_MODE_STEREO)
    
    // Codec Config (ES7210)
    .CODEC_INPUT_MODE   = 0,  // AUDIO_HAL_ADC_INPUT_LINE1 (Mic 1/2)
    .CODEC_CODEC_MODE   = 1,  // AUDIO_HAL_CODEC_MODE_ENCODE
    .CODEC_IFACE_I2S_FMT      = 0,  // AUDIO_HAL_I2S_NORMAL
    .CODEC_IFACE_SAMPLES      = 2,  // AUDIO_HAL_16K_SAMPLES
    .CODEC_IFACE_BIT_LENGTH   = 1,  // AUDIO_HAL_BIT_LENGTH_16BITS
    
    // Codec Config (ES8311)
    .DAC_BIT_LENGTH     = 16, // ES8311_RESOLUTION_16
    
    // Mic Settings (Standard)
    .MIC_SELECTED       = 0x03, // Mic1 | Mic2
    .MIC_GAIN_DB        = 13,   // GAIN_36DB

    // Mic Settings (AEC / Loopback)
    .AEC_MIC_SELECTED   = 0x0F, // Mic1 | Mic2 | Mic3 | Mic4
    .AEC_MIC_GAIN_DB    = 4,    // GAIN_12DB (Conservative start for AEC)

    // --= Audio Poweramp =-- // NS4150B on WS Smart86 boxes
    .I2S_AMP_EN         = 53,

    // --= Button =--
    .BOOT_BUTTON_PIN    = 35,

    // --= SD Card Interface =--
    .TF_CMD    = 44,
    .TF_CLK    = 43,
    .TF_D0     = 39,
    .TF_D1     = 40,
    .TF_D2     = 41,
    .TF_D3     = 42,

};
inline const Fleet_Hardware_Config& hw_cfg = WS_P4_Smart86_Hardware;

#endif // BSP_WS_P4_SMART86_H