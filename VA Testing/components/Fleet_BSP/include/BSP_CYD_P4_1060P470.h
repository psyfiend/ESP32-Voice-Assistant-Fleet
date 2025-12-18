#pragma once
#ifndef BSP_GUITION_1060P470_H
#define BSP_GUITION_1060P470_H

#include <Arduino_GFX_Library.h>
#include "Fleet_BSP_P4.h"

// -------------------------------------------------------------------------
// Board: Guition JC1060P470C (ESP32-P4 + C6 + ETH)
// Driver: JD9165BA (MIPI DSI)
// Resolution: 1024x600
// -------------------------------------------------------------------------

// --- Init Sequence (Extracted from MTK_JD9165BA...dtsi.txt) ---
static const lcd_init_cmd_t guition_1060P4709_init[] = {
    //  {cmd, { data }, data_size, delay_ms}
    // 1. Switch to Page 0
    {0x30, (uint8_t[]){0x00}, 1, 0},
    // 2. Password / Unlock (Manufacturer Command Access)
    {0xF7, (uint8_t[]){0x49, 0x61, 0x02, 0x00}, 4, 0},
    // 3. Switch to Page 1
    {0x30, (uint8_t[]){0x01}, 1, 0},
    
    // --- Power & Analog Config ---
    {0x04, (uint8_t[]){0x0C}, 1, 0},
    {0x05, (uint8_t[]){0x00}, 1, 0}, //05=06(xhs)
    {0x06, (uint8_t[]){0x00}, 1, 0}, //06=80(xhs)
    {0x0B, (uint8_t[]){0x11}, 1, 0}, //0x13=4lanes，0x12=3lanes，0x11=2lanes，0x10=1 lanes
    {0x17, (uint8_t[]){0x00}, 1, 0},
    {0x20, (uint8_t[]){0x04}, 1, 0}, //add //r_lansel_sel_reg=1, software charge lane must open
    {0x1F, (uint8_t[]){0x05}, 1, 0}, //add hs_settle time
    {0x23, (uint8_t[]){0x00}, 1, 0}, //add //close gas
    {0x25, (uint8_t[]){0x19}, 1, 0}, 
    {0x28, (uint8_t[]){0x18}, 1, 0},
    {0x29, (uint8_t[]){0x04}, 1, 0}, //revcom
    {0x2A, (uint8_t[]){0x01}, 1, 0}, //revcom
    {0x2B, (uint8_t[]){0x04}, 1, 0}, //vcom
    {0x2C, (uint8_t[]){0x01}, 1, 0}, //vcom

    // --- Switch to Page 2 ---
    {0x30, (uint8_t[]){0x02}, 1, 0},
    {0x01, (uint8_t[]){0x22}, 1, 0},
    {0x03, (uint8_t[]){0x12}, 1, 0},
    {0x04, (uint8_t[]){0x00}, 1, 0},
    {0x05, (uint8_t[]){0x64}, 1, 0},
    {0x0A, (uint8_t[]){0x08}, 1, 0},
    // Gamma / Voltage Settings
    {0x0B, (uint8_t[]){0x0A,0x1A,0x0B,0x0D,0x0D,0x11,0x10,0x06,0x08,0x1F,0x1D}, 11, 0},
    {0x0C, (uint8_t[]){0x0D,0x0D,0x0D,0x0D,0x0D,0x0D,0x0D,0x0D,0x0D,0x0D,0x0D}, 11, 0},
    {0x0D, (uint8_t[]){0x16,0x1B,0x0B,0x0D,0x0D,0x11,0x10,0x07,0x09,0x1E,0x1C}, 11, 0},
    {0x0E, (uint8_t[]){0x0D,0x0D,0x0D,0x0D,0x0D,0x0D,0x0D,0x0D,0x0D,0x0D,0x0D}, 11, 0},
    {0x0F, (uint8_t[]){0x16,0x1B,0x0D,0x0B,0x0D,0x11,0x10,0x1C,0x1E,0x09,0x07}, 11, 0},
    {0x10, (uint8_t[]){0x0D,0x0D,0x0D,0x0D,0x0D,0x0D,0x0D,0x0D,0x0D,0x0D,0x0D}, 11, 0},
    {0x11, (uint8_t[]){0x0A,0x1A,0x0D,0x0B,0x0D,0x11,0x10,0x1D,0x1F,0x08,0x06}, 11, 0},
    {0x12, (uint8_t[]){0x0D,0x0D,0x0D,0x0D,0x0D,0x0D,0x0D,0x0D,0x0D,0x0D,0x0D}, 11, 0},
    {0x14, (uint8_t[]){0x00,0x00,0x11,0x11}, 4, 0}, //CKV_OFF
    {0x18, (uint8_t[]){0x99}, 1, 0},

    // --- Switch to Page 6 ---
    {0x30, (uint8_t[]){0x06}, 1, 0},
    {0x12, (uint8_t[]){0x36,0x2C,0x2E,0x3C,0x38,0x35,0x35,0x32,0x2E,0x1D,0x2B,0x21,0x16,0x29}, 14, 0},
    {0x13, (uint8_t[]){0x36,0x2C,0x2E,0x3C,0x38,0x35,0x35,0x32,0x2E,0x1D,0x2B,0x21,0x16,0x29}, 14, 0},

    //{0x30,  1,{0x08}}, //RGB&LVDS add
    //{0x05,  1,{0x01}}, //RGB&LVDS add
    //{0x0C,  1,{0x1A}}, //RGB&LVDS add
    //{0x0D,  1,{0x0E}}, //RGB&LVDS add

    //{0x30,  1,{0x07}},
    //{0x01,  1,{0x04}}, //P7_r01=0x04(0x06)_r_hster_delay

    // --- Switch to Page 10 (0x0A) ---
    {0x30, (uint8_t[]){0x0A}, 1, 0},
    {0x02, (uint8_t[]){0x4F}, 1, 0},
    {0x0B, (uint8_t[]){0x40}, 1, 0},
    {0x12, (uint8_t[]){0x3E}, 1, 0},
    {0x13, (uint8_t[]){0x78}, 1, 0},

    // --- Switch to Page 13 (0x0D) ---
    {0x30, (uint8_t[]){0x0D}, 1, 0},
    {0x0D, (uint8_t[]){0x04}, 1, 0},
    {0x10, (uint8_t[]){0x0C}, 1, 0},
    {0x11, (uint8_t[]){0x0C}, 1, 0},
    {0x12, (uint8_t[]){0x0C}, 1, 0},
    {0x13, (uint8_t[]){0x0C}, 1, 0},

    // --- End Config / Sleep Out ---
    {0x30, (uint8_t[]){0x00}, 1, 0},    // Page 0
    {0X3A, (uint8_t[]){0x55}, 1, 0},    // Interface Pixel Format: 16bit (0x55) or 24bit (0x77)
    {0x11, (uint8_t[]){0x00}, 0, 120},  // Sleep Out
    {0x29, (uint8_t[]){0x00}, 0, 20},   // Display On
    {0x00, (uint8_t[]){0x00}, 0, 0},    // End
};

const Fleet_BSP guition_1060P4709_LCD = {
    .device_name       = "Guition P4 JC1060P470C",

    // --= Hardware Flags =--
    #define HAS_MIPI_PANEL 1
    #define HAS_TOUCH 1
    
    // --=  I2C Bus (Touch) =--
    .I2C_SDA_PIN       = 7,
    .I2C_SCL_PIN       = 8,

    // ---=  LCD  =---
    .LCD_MODEL         = "JD9165",
    .WIDTH             = 1024,
    .HEIGHT            = 600,
    .ROTATION          = 0,
    .AUTO_FLUSH        = true,

    // ---= Backlight =---
    .LCD_BL            = 23, 
    .LCD_BL_ON_LEVEL   = 1,     // Active HIGH
    .LCD_BL_FREQ       = 20000, // 20 kHz PWM

    // ---= Touch Panel =---
    .TP_NAME            = "GT911",
    #define TOUCH_PANEL TOUCH_1060P470
    .TP_I2C_ADDR        = 0x5D, // Likely 0x5D or 0x14
    .TP_I2C_BACKUP_ADDR = 0x14,
    .TP_I2C_CLOCK_SPEED = 400000,
    .TP_SDA             = 7,
    .TP_SCL             = 8,
    .TP_INT             = 21, 
    .TP_RST             = 22,
    .TP_MAX_TOUCH       = 5,

    // ---= LCD Control Pins =---
    .LCD_RST            = 27,  // 0 in schema, 27 in GFX Library, 5 in examples

    // ---= MIPI Timing (Extracted from .dtsi) =---
    .HSYNC_PWIDTH   = 24,   // 40,
    .HSYNC_BPORCH   = 136,  // 160,
    .HSYNC_FPORCH   = 160,  // 160,
    .VSYNC_PWIDTH   = 2,    // 10,
    .VSYNC_BPORCH   = 21,   // 23,
    .VSYNC_FPORCH   = 12,   // 12,

    // Pixel Clock: 51.2 MHz (from .dtsi JDEVB_DOTCLK)
    .PCLK_HZ        = 48000000,     // 48000000 in GFX Library example

    // ---= DSI Specific =---
    .TEST_MIPI_DSI_PHY_PWR_LDO_CHAN      = 3,
    .TEST_MIPI_DSI_PHY_PWR_LDO_VOLTAGE_MV= 2500, // Standard

    .NUM_DSI_LANES  = 2,
    .PREFER_SPEED   = 48000000, // Guition docs || GFX: 48000000
    .LANE_BIT_RATE  = 750, // Guition 750

    // ---= Init Commands =---
    .INIT_CMDS_DSI     = guition_1060P4709_init,
    .INIT_CMDS_SIZE    = sizeof(guition_1060P4709_init) / sizeof(lcd_init_cmd_t),
};
inline const Fleet_BSP& cfg = guition_1060P4709_LCD;


const Fleet_Hardware_Config Guition_P4_7_Hardware = {
    
    // --- Audio Codec ---
    // NS4150B power amplifier with ES8311 codec
    .I2S_ADDR  = 0x18,  // ES8311 Default I2C Address
    .I2S_LRCK  = 10,
    .I2S_SCLK  = 12,    // BCK_IO in examples
    .I2S_MCLK  = 13,
    .I2S_ASDOUT = 48,

    .I2S_DSDIN = 9,     // I2S_DO_IO in examples
    .AMP_EN   = 11,     // ES8311_PA in examples

    // --- SD Card Interface ---
    .TF_CMD   = 44,
    .TF_CLK   = 43,
    .TF_D0    = 39,
    .TF_D1    = 40,
    .TF_D2    = 41,
    .TF_D3    = 42,    //CD/D3

};
inline const Fleet_Hardware_Config& hw_cfg = Guition_P4_7_Hardware;

#endif // BSP_GUITION_1060P4709_H