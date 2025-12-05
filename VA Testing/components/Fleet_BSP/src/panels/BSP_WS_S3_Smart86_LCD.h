#pragma once
#ifndef BSP_WS_S3_SMART86_LCD_H
#define BSP_WS_S3_SMART86_LCD_H
#include <Arduino_GFX_Library.h>
#include "../Fleet_BSP.h"

// Panel init commands (ST7701)
static const uint8_t waveshare_s3_smart86_init[] = {
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
    WRITE_C8_D8, 0x3A, 0x60, // 0x70 RGB888, 0x60 RGB666, 0x50 RGB565

    WRITE_COMMAND_8, 0x11, // Sleep Out
    END_WRITE,

    DELAY, 120,

    BEGIN_WRITE,
    WRITE_COMMAND_8, 0x29, // Display On
    END_WRITE,
};

// Waveshare S3 Smart86 LCD Configuration
static const Fleet_BSP WS_S3_SMART86_LCD_Config = {

    .device_name       = "Waveshare S3 Smart86",

    // --= Hardware Flags =--
    #define HAS_IO_EXPANDER 1
    #define HAS_RGB_PANEL 1
    //#define HAS_QSPI_PANEL 1
    //#define HAS_MIPI_PANEL 1
    #define HAS_TOUCH 1

    // --=  I2C Bus  =--
    .I2C_SDA_PIN       = 47,
    .I2C_SCL_PIN       = 48,
    .EXPANDER_I2C_ADDR = 0x20,
    
    // #define RST_PIN EXIO_LCD_RST

    // --= Expander Pins =--
    .EXIO_LCD_RST      = 7,
    .EXIO_LCD_CS       = 0,
    .EXIO_LCD_SCK      = 2,
    .EXIO_LCD_MOSI     = 1,
    .EXIO_TP_RST       = 5,
    .EXIO_TP_INT       = 6,

    // ---= LCD Definitions =---
    .LCD_MODEL         = "ST7701",
    .WIDTH             = 480,
    .HEIGHT            = 480,
    .ROTATION          = 2,     // 0 = USB Port on right side
    .AUTO_FLUSH        = true,

    // ---= Backlight =---
    .LCD_BL            = 4,
    .LCD_BL_ON_LEVEL   = 0, // Active LOW for P4 GPIO

    // ---= Touch Panel =---
    .TP_NAME           = "GT911",
//  .TP_CAPTOUCH_NAME  = "TOUCH_WS_S3_SMART86",
    #define TOUCH_PANEL   TOUCH_WS_S3_SMART86
    .TP_I2C_ADDR       = 0x5D,
//  .TP_I2C_CLOCK_SPEED = 100000,
    .TP_SDA            = 47,
    .TP_SCL            = 48,
//  .TP_INT            = EXIO 6,
//  .TP_RST            = EXIO 5,
    .TP_MAX_TOUCH      = 5,

    // ---= LCD Control Pins =---
    .LCD_RST           = -1,    // EXIO 7

    // ---= RGB Interface =---
    .LCD_DE            = 17,
    .LCD_VSYNC         = 3,
    .LCD_HSYNC         = 46,
    .LCD_PCLK          = 9,

    .R0 = 10, .R1 = 11, .R2 = 12, .R3 = 13, .R4 = 14,
    .G0 = 21, .G1 = 8,  .G2 = 18, .G3 = 45, .G4 = 38, .G5 = 39,
    .B0 = 40, .B1 = 41, .B2 = 42, .B3 = 2,  .B4 = 1,

    // --= RGB Timing =---
    .HSYNC_POL      = 1,
    .VSYNC_POL      = 1,
    .HSYNC_PWIDTH   = 8,
    .HSYNC_BPORCH   = 50,
    .HSYNC_FPORCH   = 10,
    .VSYNC_PWIDTH   = 8,
    .VSYNC_BPORCH   = 20,
    .VSYNC_FPORCH   = 10,

    .PCLK_ACTIVE_NEG    = 0,
    .PCLK_HZ            = 12500000,

    // ---= Init Commands =---
    .INIT_CMDS_RGB      = waveshare_s3_smart86_init,
    .INIT_CMDS_SIZE     = sizeof(waveshare_s3_smart86_init),

};

inline const Fleet_BSP& cfg = WS_S3_SMART86_LCD_Config;
#endif // BSP_WS_S3_SMART86_LCD_H