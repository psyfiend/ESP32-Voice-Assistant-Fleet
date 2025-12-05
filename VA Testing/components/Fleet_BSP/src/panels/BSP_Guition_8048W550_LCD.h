#pragma once
#ifndef BSP_GUITION_8048W550_H
#define BSP_GUITION_8048W550_H
#include <Arduino_GFX_Library.h>
#include "../Fleet_BSP.h"


// Waveshare S3 Smart86 LCD Configuration
static const Fleet_BSP Guition_8048W550_LCD_Config = {

    .device_name       = "Guition 8048W550",

    // --= Hardware Flags =--
    //#define HAS_IO_EXPANDER 1
    #define HAS_RGB_PANEL 1
    //#define HAS_QSPI_PANEL 1
    //#define HAS_MIPI_PANEL 1
    #define HAS_TOUCH 1

    // --=  I2C Bus  =--
    .I2C_SDA_PIN       = 19,
    .I2C_SCL_PIN       = 20,
    
    
    // ---= LCD Definitions =---
    .LCD_MODEL         = "ST7262",
    .WIDTH             = 800,
    .HEIGHT            = 480,
    .ROTATION          = 0,     // 0=Portrait, 1=Landscape, 2=Inverted Portrait, 3=Inverted Landscape
    .AUTO_FLUSH        = true,

    // ---= Backlight =---
    .LCD_BL            = 2,
    .LCD_BL_ON_LEVEL   = 1, // Active HIGH

    // ---= Touch Panel =---
    .TP_NAME            = "GT911",
    #define TOUCH_PANEL   TOUCH_CYD_550
    .TP_I2C_ADDR        = 0x5D,
    .TP_I2C_CLOCK_SPEED = 400000,
    .TP_SDA             = 19,
    .TP_SCL             = 20,
    .TP_INT             = 18,
    .TP_RST             = 38,
    .TP_MAX_TOUCH       = 1,

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
    .PCLK_HZ            = 16000000, // prefer_speed
    .USE_BIG_ENDIAN     = false,

    // ---= Init Commands =---
    // .INIT_CMDS_RGB      = waveshare_s3_smart86_init,
    // .INIT_CMDS_SIZE     = sizeof(waveshare_s3_smart86_init),

};

inline const Fleet_BSP& cfg = Guition_8048W550_LCD_Config;
#endif // BSP_GUITION_8048W550_H