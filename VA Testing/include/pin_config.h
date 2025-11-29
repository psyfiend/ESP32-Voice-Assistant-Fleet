#pragma once

#include <Arduino.h>

// ==============================================================================
//  BOARD SELECTOR
// ==============================================================================
// These flags come from platformio.ini (build_flags)
// - WS_S3_SMART86
// - GUITION_S3_QSPI
// - GUITION_P4_7

#define GFX_NOT_DEFINED -1

// ==============================================================================
//  WAVESHARE S3 BOX (480x480 RGB + TCA9554)
// ==============================================================================
#if defined(WS_S3_SMART86)

    // --- HARDWARE FEATURE FLAGS ---
    #define HAS_IO_EXPANDER      1
    #define HAS_RGB_PANEL        1
    #define IO_EXPANDER_I2C_ADDR 0x20
    #define XPOWERS_CHIP_AXP2101

    // --- I2C BUS ---
    #define PIN_I2C_SDA     47
    #define PIN_I2C_SCL     48

    // --- LCD DEFINITIONS ---
    #define DISPLAY_WIDTH   480
    #define DISPLAY_HEIGHT  480
    // Backlight active 1 for HIGH, 0 for LOW
    #define LCD_BL_ON_LEVEL 0  // This board's BL is active LOW
    
    // Note: On this board, these are EXPANDER PINS, not GPIOs!
    // The DisplayManager knows this because of HAS_IO_EXPANDER
    #define PIN_LCD_BL      4      // Real GPIO
    #define PIN_LCD_RST     7      // Expander IO7
    #define PIN_LCD_CS      0      // Expander IO0
    #define PIN_LCD_SCK     2      // Expander IO2
    #define PIN_LCD_MOSI    1      // Expander IO1

    // --- TOUCH PANEL ---
    #define PIN_TP_RST      5      // Expander IO5
    #define PIN_TP_INT      6      // Expander IO6

    // ---= Audio Codec pins =---
    #define PIN_ES7210_BCLK     16
    #define PIN_ES7210_LRCK     7
    #define PIN_ES7210_DIN      15
    #define PIN_ES7210_MCLK     5
    #define PIN_ES8311_DOUT     6

    // --- RGB INTERFACE ---
    #define PIN_RGB_DE      17
    #define PIN_RGB_VSYNC   3
    #define PIN_RGB_HSYNC   46
    #define PIN_RGB_PCLK    9
    
    // RGB Data Pins (R0-R4, G0-G5, B0-B4)
    #define PIN_RGB_R0 10
    #define PIN_RGB_R1 11
    #define PIN_RGB_R2 12
    #define PIN_RGB_R3 13
    #define PIN_RGB_R4 14
    
    #define PIN_RGB_G0 21
    #define PIN_RGB_G1 8
    #define PIN_RGB_G2 18
    #define PIN_RGB_G3 45
    #define PIN_RGB_G4 38
    #define PIN_RGB_G5 39
    
    #define PIN_RGB_B0 40
    #define PIN_RGB_B1 41
    #define PIN_RGB_B2 42
    #define PIN_RGB_B3 2
    #define PIN_RGB_B4 1

// ==============================================================================
//  GUITION S3 3.5" (320x480 QSPI + AXS15321)
//  JC3248W535 ESP32-S3 N16R8 Module
// ==============================================================================
#elif defined(GUITION_3_5)

    #define HAS_IO_EXPANDER 0
    #define HAS_RGB_PANEL   0
    #define HAS_QSPI_PANEL  1

    // Orientation (Swap W/H if you change rotation to 1 or 3)
    // Starting with Portrait to match native panel scan
    #define DISPLAY_WIDTH   320
    #define DISPLAY_HEIGHT  480
    #define DISPLAY_ROTATION 0 

    // Backlight (Active HIGH for this module)
    #define LCD_BL_ON_LEVEL 1 
    #define PIN_LCD_BL      1

    // QSPI Interface Pins (AXS15321)
    // CS:45, SCK:47, D0:21, D1:48, D2:40, D3:39
    #define PIN_LCD_CS      45
    #define PIN_LCD_SCK     47
    #define PIN_LCD_SD0     21
    #define PIN_LCD_SD1     48
    #define PIN_LCD_SD2     40
    #define PIN_LCD_SD3     39
    #define PIN_LCD_RST     -1 

    // Touch Panel (I2C)
    // Confirmed: SDA=4, SCL=8 (Schematic might say reversed, but this works)
    #define PIN_I2C_SDA     4 
    #define PIN_I2C_SCL     8
    #define PIN_TP_INT      3
    #define PIN_TP_RST      -1 

#endif