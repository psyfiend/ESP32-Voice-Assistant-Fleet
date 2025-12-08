#pragma once
#ifndef BSP_WS_P4_7B_H
#define BSP_WS_P4_7B_H
#include <Arduino_GFX_Library.h>
#include "Fleet_BSP_P4.h"

// Panel init commands (EK79007)
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

    // --= Hardware Flags =--
    //#define HAS_IO_EXPANDER
    //#define HAS_RGB_PANEL
    //#define HAS_QSPI_PANEL
    #define HAS_MIPI_PANEL 1
    #define HAS_TOUCH 1
    
    // --=  I2C Bus  =--
    .I2C_SDA_PIN       = 7,
    .I2C_SCL_PIN       = 8,

    .BOOT_BUTTON_PIN  = 35,

    // ---=  LCD  =---
    .LCD_MODEL         = "EK79007",
    .WIDTH             = 1024,
    .HEIGHT            = 600,
    .ROTATION          = 2,
    .AUTO_FLUSH        = true,

    // ---= Backlight =---
    .LCD_BL            = 32,
    .LCD_BL_ON_LEVEL   = 0, // 0 = active LOW, 1 = active HIGH
    .LCD_BL_FREQ       = 5000, // 5 kHz PWM

    // ---= Touch Panel =---
    .TP_NAME           = "GT911",
    #define TOUCH_PANEL  TOUCH_WS_P4_7B
    .TP_I2C_ADDR       = 0x5D,
            // When power-on detects low level of the interrupt gpio, address is 0x5D.
            // Interrupt gpio is high level, address is 0x14.
    .TP_I2C_BACKUP_ADDR= 0x14,
    .TP_I2C_CLOCK_SPEED = 400000,
    .TP_SDA            = 7,
    .TP_SCL            = 8,
    .TP_INT            = -1,
    .TP_RST            = -1,
    .TP_MAX_TOUCH      = 4,

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

    // ---= Init Commands =---
    .INIT_CMDS_DSI     = waveshare_p4_7b_init,
    .INIT_CMDS_SIZE    = sizeof(waveshare_p4_7b_init) / sizeof(lcd_init_cmd_t),

};
inline const Fleet_BSP& cfg = WS_P4_7B_LCD;

const Fleet_Hardware_Config WS_P4_7B_Hardware = {

    // --= Audio Codec =-- 

    .I2S_ADDR     = 0x1A,  // 0x40?

    .I2S_LRCK     = 10,  //WS
    .I2S_SCLK     = 12,  //BCLK
    .I2S_MCLK     = 13,
    .I2S_ASDOUT   = 11,  //DIN

    .I2S_DSDIN    = 9,   //DOUT

    .AMP_EN      = 53, //25,  // WS_P4_7B

    // --= SD Card Interface =--
    .TF_CMD            = 44,
    .TF_CLK            = 43,
    .TF_D0             = 39,
    .TF_D1             = 40,
    .TF_D2             = 41,
    .TF_D3             = 42,    //CD/D3

    /*
    // --= WS_P4_7B Experimental SDKconfig =--
    .SDIO_PIN_CMD      = 19,
    .SDIO_PIN_CLK      = 18,
    .SDIO_PIN_D0       = 14,
    .SDIO_PIN_D1       = 15,
    .SDIO_PIN_D2       = 16,
    .SDIO_PIN_D3       = 17,

    // I2S random pins
    .WS_IO             = 10,
    .DO_IO             = 9,
    .DI_IO             = 11,
    */
};
inline const Fleet_Hardware_Config& hw_cfg = WS_P4_7B_Hardware;

#endif // BSP_WS_P4_7B_H