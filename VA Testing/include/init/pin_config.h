#pragma once

#include <Arduino.h>

// ==============================================================================
//  BOARD SELECTOR
// ==============================================================================
// These flags come from platformio.ini (build_flags)
// - WS_S3_SMART86     (Waveshare S3 4" 480x480 RGB  ST7701 + TCA9554)
// - GUITION_3248W535  (Guition 3.5"    320x480 QSPI ASX15231)
// - GUITION_8048W550  (Guition 5"      800x480 RGB  ST7262 + GT911)

#define GFX_NOT_DEFINED -1

#if defined(WS_S3_SMART86)
// ==============================================================================
//  WAVESHARE S3 BOX (480x480 RGB + TCA9554)
// ==============================================================================

    #define IO_EXPANDER_I2C_ADDR 0x20
    #define XPOWERS_CHIP_AXP2101

    // --- I2C BUS ---
    #define PIN_I2C_SDA     47
    #define PIN_I2C_SCL     48

    // --- LCD DEFINITIONS ---
    #define DISPLAY_WIDTH   480
    #define DISPLAY_HEIGHT  480
    #define DISPLAY_ROTATION  0    // 0=Portrait, 1=Landscape, 2=Inverted Portrait, 3=Inverted Landscape

    // Backlight active 1 for HIGH, 0 for LOW
    #define PIN_LCD_BL      4      // Real GPIO
    #define LCD_BL_ON_LEVEL 0  // This board's BL is active LOW
    
    // Note: On this board, these are EXPANDER PINS, not GPIOs!
    // The DisplayManager knows this because of HAS_IO_EXPANDER
    #define PIN_LCD_RST     7      // Expander IO7
    #define PIN_LCD_CS      0      // Expander IO0
    #define PIN_LCD_SCK     2      // Expander IO2
    #define PIN_LCD_MOSI    1      // Expander IO1

    // --- TOUCH PANEL ---
    #define TOUCH_PANEL     TOUCH_WS_S3_SMART86   // Defined in bb_captouch.h
    #define TP_I2C_ADDR     0x38
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

    #define RGB_HSYNC_POL       1
    #define RGB_HSYNC_FPORCH    10
    #define RGB_HSYNC_PWIDTH    8
    #define RGB_HSYNC_BPORCH    50
    #define RGB_VSYNC_POL       1
    #define RGB_VSYNC_FPORCH    10
    #define RGB_VSYNC_PWIDTH    8
    #define RGB_VSYNC_BPORCH    20

    #define RGB_PCLK_ACTIVE_NEG     0
    #define RGB_PCLK_HZ            12500000 // aka prefer_speed


#elif defined(GUITION_3248W535) || defined(GUITION_S3_QSPI)
// ==============================================================================
//  GUITION S3 3.5" (320x480 QSPI + ASX15231)
//  JC3248W535 ESP32-S3 N16R8 Module
// ==============================================================================

    // --- I2C Bus ---
    #define PIN_I2C_SDA     4
    #define PIN_I2C_SCL     8

    // Hardware is locked at 320x480 portrait, must leave width and height as-is.
    // DisplayManager will handle visual orientation and TouchManager
    // will handle touch mapping.
    #define DISPLAY_WIDTH   320 // Leave default 320x480
    #define DISPLAY_HEIGHT  480
    #define DISPLAY_ROTATION 1  // 0=Portrait, 1=Landscape, 2=Inverted Portrait, 3=Inverted Landscape 

    // Backlight (Active HIGH for this module)
    #define PIN_LCD_BL      1
    #define LCD_BL_ON_LEVEL 1 

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
    #define TOUCH_PANEL     TOUCH_CYD_535   // Defined in bb_captouch.h
    #define TP_I2C_ADDR     0x38
    #define TP_I2C_FREQ     400000
    #define PIN_I2C_SDA     4 
    #define PIN_I2C_SCL     8
    #define PIN_TP_INT      11 // 3
    #define PIN_TP_RST      12 // -1
    #define MAX_TOUCH       1


#elif defined(JC8048W550C)
// ==============================================================================
//  GUITION S3 5" (800x480 RGB ST7262 + GT911)
//  JC8048W550 ESP32-S3 N16R8 Module
// ==============================================================================
   
    //#define DIRECT_RENDER

    // --- I2C BUS ---
    #define PIN_I2C_SDA     19
    #define PIN_I2C_SCL     20

    // --- LCD DEFINITIONS ---
    #define DISPLAY_WIDTH   800
    #define DISPLAY_HEIGHT  480
    #define DISPLAY_ROTATION 0  // 0=Portrait, 1=Landscape, 2=Inverted Portrait, 3=Inverted Landscape
    
    // Backlight active 1 for HIGH, 0 for LOW
    #define PIN_LCD_BL      2 // 21?
    #define LCD_BL_ON_LEVEL 1  // This board's BL is active HIGH

    // Touch Panel (I2C)
    #define TOUCH_PANEL     TOUCH_CYD_550   // Defined in bb_captouch.h
    #define TP_I2C_ADDR     0x5D
    #define TP_I2C_FREQ     400000
    #define PIN_TP_INT      18
    #define PIN_TP_RST      38
    #define MAX_TOUCH       1
    
    // SD Card Interface
    #define PIN_TF_CS      10
    #define PIN_TF_MOSI    11
    #define PIN_TF_MISO    13
    #define PIN_TF_CLK     12

    // Audio Interface Pins
    #define PIN_I2S_LRCLK   18
    #define PIN_I2S_BCLK    0
    #define PIN_I2S_DIN     17

    // --- RGB INTERFACE ---
    #define PIN_RGB_DE      40
    #define PIN_RGB_VSYNC   41
    #define PIN_RGB_HSYNC   39
    #define PIN_RGB_PCLK    42
    
    // RGB Data Pins (R0-R4, G0-G5, B0-B4)
    #define PIN_RGB_R0  45
    #define PIN_RGB_R1  48
    #define PIN_RGB_R2  47
    #define PIN_RGB_R3  21
    #define PIN_RGB_R4  14

    #define PIN_RGB_G0  5
    #define PIN_RGB_G1  6
    #define PIN_RGB_G2  7
    #define PIN_RGB_G3  15
    #define PIN_RGB_G4  16
    #define PIN_RGB_G5  4

    #define PIN_RGB_B0  8
    #define PIN_RGB_B1  3
    #define PIN_RGB_B2  46
    #define PIN_RGB_B3  9
    #define PIN_RGB_B4  1

    #define RGB_HSYNC_POL       0
    #define RGB_HSYNC_FPORCH    8
    #define RGB_HSYNC_PWIDTH    4
    #define RGB_HSYNC_BPORCH    8
    #define RGB_VSYNC_POL       0
    #define RGB_VSYNC_FPORCH    8
    #define RGB_VSYNC_PWIDTH    4
    #define RGB_VSYNC_BPORCH    8

    #define RGB_HSYNC_IDLE_LOW  0
    #define RGB_VSYNC_IDLE_LOW  0
    #define RGB_DE_IDLE_HIGH    0
    #define RGB_PCLK_IDLE_HIGH  0
    #define RGB_PCLK_ACTIVE_NEG 1
    #define RGB_PCLK_HZ         16000000 // aka prefer_speed
    #define RGB_BIG_ENDIAN      false


// ==============================================================================
//  WAVESHARE P4 SMART86 (4" 720x720 MIPI)
// ==============================================================================
#elif defined(WS_P4_SMART86)

    #define HAS_IO_EXPANDER 0
    #define HAS_RGB_PANEL   0
    #define HAS_QSPI_PANEL  0
    #define HAS_MIPI_PANEL  1

    // Display Orientation
    #define DISPLAY_WIDTH   720
    #define DISPLAY_HEIGHT  720
    #define DISPLAY_ROTATION 0

    // Backlight (Active HIGH usually for P4 GPIO)
    #define LCD_BL_ON_LEVEL 1
    #define PIN_LCD_BL      26

    // MIPI DSI Pins (P4 handles DSI PHY internally)
    #define PIN_LCD_RST     27
    
    // Unused on MIPI
    #define PIN_LCD_CS      -1
    #define PIN_LCD_SCK     -1
    #define PIN_LCD_MOSI    -1

    // Touch Panel (GT911)
    // Schematic: I2C0_SDA (7), I2C0_SCL (8), TP_INT (6), TP_RST (27)
    // Note: TP_RST is shared with LCD_RST on this board
    #define PIN_I2C_SDA     7
    #define PIN_I2C_SCL     8
    #define PIN_TP_INT      6
    #define PIN_TP_RST      27
    
    // Explicitly define the Touch ID for bb_captouch if needed
    // #define TOUCH_PANEL   TOUCH_GT911 

#endif

/*
20 IO0 BOOT I2S-BCLK/L3
21 IO18 I2S-LRCLK/L2
22 IO10 SPICS0 TF(CS)
23 IO11 SPID TF(MISO)
24 IO17 U1TXD IS2-DIN/L1
25 IO38 TP_CS
26 IO21 BL_C

32 IO12 SPICLK TF(SCK)/TP_CLK
33 IO13 SPIQ TF(MOSI)
34 IO42 PCLK x
*/

// Arduino_DataBus *bus = new Arduino_SWSPI(
//    GFX_NOT_DEFINED /* DC */, GFX_NOT_DEFINED /* CS */, GFX_NOT_DEFINED /* SCK */, GFX_NOT_DEFINED /* MOSI */, GFX_NOT_DEFINED /* MISO */);

// v1.4.7 - RGB Panel for GUITION JC8048W543/550 (800x480)
// Arduino_ESP32RGBPanel * bus = new Arduino_ESP32RGBPanel(
//   GFX_NOT_DEFINED /* CS */, GFX_NOT_DEFINED /* SCK */, GFX_NOT_DEFINED /* SDA */,
//   40 /* DE */, 41 /* VSYNC */, 39 /* HSYNC */, 42 /* PCLK */,
//   45 /* R0 */, 48 /* R1 */, 47 /* R2 */, 21 /* R3 */, 14 /* R4 */,
//   5 /* G0 */, 6 /* G1 */, 7 /* G2 */, 15 /* G3 */, 16 /* G4 */, 4 /* G5 */,
//   8 /* B0 */, 3 /* B1 */, 46 /* B2 */, 9 /* B3 */, 1 /* B4 */,
//     0 /* hsync_polarity */, 8 /* hsync_front_porch */, 4 /* hsync_pulse_width */, 8 /* hsync_back_porch */,
//     0 /* vsync_polarity */, 8 /* vsync_front_porch */, 4 /* vsync_pulse_width */, 8 /* vsync_back_porch */
// );

// Arduino_RGB_Display *gfx = new Arduino_RGB_Display(
//      800 /* width */, 480 /* height */, rgbpanel, 0 /* rotation */, true /* auto_flush */);


// v1.4.7 - RGB Panel for GUITION JC8048W543/550 (800x480)    
// Arduino_RPi_DPI_RGBPanel * gfx = new Arduino_RPi_DPI_RGBPanel(
//   bus,
//   800 /* width */, 0 /* hsync_polarity */, 8 /* hsync_front_porch */, 4 /* hsync_pulse_width */, 8 /* hsync_back_porch */,
//   480 /* height */, 0 /* vsync_polarity */, 8 /* vsync_front_porch */, 4 /* vsync_pulse_width */, 8 /* vsync_back_porch */,
//   1 /* pclk_active_neg */, 16000000 /* prefer_speed */, true /* auto_flush */);

// 16-bit RGB panel 800x480 for JC8048W543 and JC8048W550 (4.3"/5.5" 800x480)
// const BB_RGB rgbpanel_800x480 = {
//     -1 /* CS */, -1 /* SCK */, -1 /* SDA */,
//     40 /* DE */, 41 /* VSYNC */, 39 /* HSYNC */, 42 /* PCLK */,
//     45 /* R0 */, 48 /* R1 */, 47 /* R2 */, 21 /* R3 */, 14 /* R4 */,
//     5 /* G0 */, 6 /* G1 */, 7 /* G2 */, 15 /* G3 */, 16 /* G4 */, 4 /* G5 */,
//     8 /* B0 */, 3 /* B1 */, 46 /* B2 */, 9 /* B3 */, 1 /* B4 */,
//     8 /* hsync_back_porch */, 8 /* hsync_front_porch */, 4 /* hsync_pulse_width */,
//     8 /* vsync_back_porch */, 8 /* vsync_front_porch */, 4 /* vsync_pulse_width */,
//     0 /* hsync_polarity */, 0 /* vsync_polarity */,
//     800, 480,
//     14000000 // speed
