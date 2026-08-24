#pragma once
#ifndef BSP_WS_P4_7B_H
#define BSP_WS_P4_7B_H

#include <Arduino_GFX_Library.h>
#include "Fleet_BSP_P4.h"

// #define WS_P4_7B    // Build flag in platformio.ini handles the BSP choice

// -------------------------------------------------------------------------
// Board: WaveShare Touch-LCD-7B (ESP32-P4 + C6)
// Driver: EK79007 (MIPI DSI)
// Resolution: 1024x600
// -------------------------------------------------------------------------

// --= Hardware Flags =--
#define HAS_MIPI_PANEL 1
#define HAS_TOUCH 1
#define HAS_ES8311 1
#define HAS_ES7210 1
//#define HAS_IO_EXPANDER
//#define HAS_RGB_PANEL
//#define HAS_QSPI_PANEL
//#define TOUCH_PANEL  TOUCH_WS_P4_7B

// Panel init commands (EK79007)
// NOTE: Must stay here, immediately before `cfg` below — .INIT_CMDS_SIZE uses
// sizeof() on this array, which requires the array's complete (sized) type.
// A forward declaration would leave the type incomplete at that point and
// fail to compile, so this can't move below cfg/hw_cfg without hardcoding
// the element count as a separate maintained constant instead.
// CMD, DATA ptr, DATA len, DELAY ms
static const lcd_init_cmd_t waveshare_p4_7b_init[] = {
    // 1. Soft Reset
    {0x01, (uint8_t[]){0x00}, 0, 300}, 

    // 2. Gamma / Quality Control (Exact order from HA config)
    {0xE0, (uint8_t[]){0x00}, 1, 0},
    {0xE1, (uint8_t[]){0x93}, 1, 0},
    {0xE2, (uint8_t[]){0x65}, 1, 0},
    {0xE3, (uint8_t[]){0xF8}, 1, 0},
    {0xE6, (uint8_t[]){0x64}, 1, 0},
    {0xE7, (uint8_t[]){0x66}, 1, 0},

    // 3. Power / Analog Config
    {0x80, (uint8_t[]){0x8B}, 1, 0},
    {0x81, (uint8_t[]){0x78}, 1, 0},
    {0x82, (uint8_t[]){0x84}, 1, 0},
    {0x83, (uint8_t[]){0x88}, 1, 0},
    {0x84, (uint8_t[]){0xA8}, 1, 0},
    {0x85, (uint8_t[]){0xE3}, 1, 0},
    {0x86, (uint8_t[]){0x88}, 1, 0},
    
    // Porch Control
    {0xB2, (uint8_t[]){0x10}, 1, 0},

    // Power Control
    {0xC0, (uint8_t[]){0x01, 0x09}, 2, 0},
    {0xC1, (uint8_t[]){0x41}, 1, 0},
    {0xC5, (uint8_t[]){0x00, 0x0A, 0x80}, 3, 0},

    // 4. Interface Pixel Format
    // Panel works with 24 bit color (0x77) but GFX Library defines 16bit in esp_lcd_panel_dev_config_t
    // Using 16bit (0x55) here as per GFX Library expectation
    {0x3A, (uint8_t[]){0x55}, 1, 0},

    // 5. Special Command
    {0xE8, (uint8_t[]){0x84, 0x11, 0x79}, 3, 0},
    {0xEC, (uint8_t[]){0x7B}, 1, 0},

    // 6. Sleep Out
    {0x11, (uint8_t[]){0x00}, 0, 150}, 

    // 7. Display On
    {0x29, (uint8_t[]){0x00}, 0, 50},

    // 8. End
    {0x00, (uint8_t[]){0x00}, 0, 0} 
};

// Waveshare P4 7B LCD Configuration
const Fleet_BSP WS_P4_7B_LCD = {
    .device_name       = "Waveshare P4 7\" Touch-LCD-7B",

    // --=  I2C Bus  =--
    .I2C_SDA_PIN       = 7,
    .I2C_SCL_PIN       = 8,

    // ---=  LCD  =---
    .LCD_MODEL         = "EK79007",
    .WIDTH             = 1024,
    .HEIGHT            = 600,
    .ROTATION          = 2,     // Landscape USB on LEFT
    .AUTO_FLUSH        = true,

    // ---= Backlight =---
    .LCD_BL            = 32,
    .LCD_BL_ON_LEVEL   = 0, // 0 = active LOW, 1 = active HIGH
    .LCD_BL_FREQ       = 5000, // 5 kHz PWM

    // ---= Touch Panel =---
    .TP_NAME           = "GT911",
        // When power-on detects low level of the interrupt gpio, address is 0x5D.
        // Interrupt gpio is high level, address is 0x14.
    .TP_I2C_ADDR       = 0x5D,
    .TP_I2C_BACKUP_ADDR= 0x14,
    .TP_I2C_CLOCK_SPEED = 400000,
    .TP_SDA            = 7,
    .TP_SCL            = 8,
    .TP_INT            = -1,
    .TP_RST            = -1,
    .TP_MAX_TOUCH      = 7,

    // ---= LCD Control Pins =---
    .LCD_RST           = 33,
    
    // ---= MIPI Timing =---
    .HSYNC_PWIDTH   = 10,
    .HSYNC_BPORCH   = 160,
    .HSYNC_FPORCH   = 160,
    .VSYNC_PWIDTH   = 1,
    .VSYNC_BPORCH   = 23,
    .VSYNC_FPORCH   = 12,

    .PCLK_HZ        = 52000000,

    // ---= DSI Specific =---
    .TEST_MIPI_DSI_PHY_PWR_LDO_CHAN      = 3,
    .TEST_MIPI_DSI_PHY_PWR_LDO_VOLTAGE_MV= 2500,

    .NUM_DSI_LANES  = 2,

    .PREFER_SPEED   = 52000000,
    .LANE_BIT_RATE  = 1000, // test 900!

    // --= LVGL Settings =--
    .DOUBLE_BUFFERING   = true,
    .DRAW_BUF_HEIGHT    = 50,
    .BUFFER_SIZE_PX     = 51200, // 1024 * 50, // WIDTH * DRAW_BUF_HEIGHT

    // ---= Init Commands =---
    .INIT_CMDS_DSI     = waveshare_p4_7b_init,
    .INIT_CMDS_SIZE    = sizeof(waveshare_p4_7b_init) / sizeof(lcd_init_cmd_t),

};
inline const Fleet_BSP& cfg = WS_P4_7B_LCD;

const Fleet_Hardware_Config WS_P4_7B_Hardware = {

    // --= Audio Codec =-- 
    // ES8311 / 7210 Codec + NS4150B PA

    .I2S_SDA_PIN    = 7,    // I2C SDA
    .I2S_SCL_PIN    = 8,    // I2C SCL
    .I2S_7210_ADDR  = 0x40, // ES7210 ADC/Mics
    .I2S_8311_ADDR  = 0x18, // ES8311 DAC/Amp

    .I2S_MCLK   = 13,
    .I2S_BCLK   = 12,  //SCLK
    .I2S_LRCK   = 10,  //WS

    .I2S_DIN    = 11,
    .I2S_DOUT   = 9,

    .AUDIO_INPUT_SAMPLE_RATE    = 16000,
    .AUDIO_OUTPUT_SAMPLE_RATE   = 16000,
    .I2S_MCLK_MULTIPLE          = 256,

    // --= New Audio Settings =--
    .I2S_DATA_BIT_WIDTH     = 16, // I2S_DATA_BIT_WIDTH_16BIT
    .I2S_SLOT_BIT_WIDTH     = 16, // I2S_SLOT_BIT_WIDTH_16BIT
    .I2S_SLOT_MODE          = 2,  // 1 = MONO, 2 = Stereo (I2S_SLOT_MODE_STEREO)
    
    // Codec Config (ES7210)
    .CODEC_INPUT_MODE       = 0,  // AUDIO_HAL_ADC_INPUT_LINE1 (Mic 1/2)
    .CODEC_CODEC_MODE       = 1,  // AUDIO_HAL_CODEC_MODE_ENCODE
    .CODEC_IFACE_I2S_FMT    = 0,  // AUDIO_HAL_I2S_NORMAL
    .CODEC_IFACE_SAMPLES    = 2,  // AUDIO_HAL_16K_SAMPLES
    .CODEC_IFACE_BIT_LENGTH = 1,  // AUDIO_HAL_BIT_LENGTH_16BITS
    
    // Codec Config (ES8311)
    .DAC_BIT_LENGTH         = 16, // ES8311_RESOLUTION_16
    
    // Mic Settings (Standard)
    .MIC_SELECTED           = 0x03, // Mic1 | Mic2
    .MIC_GAIN_DB            = 13,   // GAIN_36DB

    // Mic Settings (AEC / Loopback)
    .AEC_MIC_SELECTED       = 0x0F, // Mic1 | Mic2 | Mic3 | Mic4
    .AEC_MIC_GAIN_DB        = 4,    // GAIN_12DB (Conservative start for AEC)

    // --= Audio Amp =--
    .I2S_AMP_EN     = 53,   // WS_P4_7B

    .BOOT_BUTTON_PIN  = 35,

    // --= SD Card Interface =--
    .TF_CMD     = 44,
    .TF_CLK     = 43,
    .TF_D0      = 39,
    .TF_D1      = 40,
    .TF_D2      = 41,
    .TF_D3      = 42,    //CD/D3

    /* I2C Scan Results:
    I2C device found at address 0x18  ! // ES8311 DAC/Amp
    I2C device found at address 0x40  ! // ES7210 ADC/Mics
    I2C device found at address 0x5D  ! // GT911 Touch Panel
    */

};
inline const Fleet_Hardware_Config& hw_cfg = WS_P4_7B_Hardware;

#endif // BSP_WS_P4_7B_H