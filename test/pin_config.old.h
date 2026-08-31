#ifndef PIN_CONFIG_H
#define PIN_CONFIG_H

#define GFX_NOT_DEFINED -1

// Define board type
#if defined(WS_S3_SMART86)

// st7701 display driver with TCA9554 I2C port expander and RGB panel

    #define XPOWERS_CHIP_AXP2101

    // I2C pins and address
    #define PIN_I2C_SDA     47
    #define PIN_I2C_SCL     48
    #define TCA9554_I2C     0x20

    // TCA9554 pin definitions - These are the IO names used on the schematic
    const uint8_t EXIO7 = 7;  // TCA9554 - LCD RST
    const uint8_t EXIO0 = 0;  // TCA9554 - LCD CS
    const uint8_t EXIO2 = 2;  // TCA9554 - LCD SCL/SCK
    const uint8_t EXIO1 = 1;  // TCA9554 - LCD SDA/MOSI
    const uint8_t EXIO5 = 5;  // TCA9554 - TP RST
    const uint8_t EXIO6 = 6;  // TCA9554 - TP INT

    // LCD and Touch Panel pins
    const uint8_t GFX_BL = 4;    // LOW to turn ON backlight
    #define LCD_RST     EXIO7    // TCA9554 - LCD RST
    #define LCD_CS      EXIO0    // TCA9554 - LCD CS
    #define LCD_SCK     EXIO2    // TCA9554 - LCD SCL/SCK
    #define LCD_MOSI    EXIO1    // TCA9554 - LCD SDA
    #define TP_RST      EXIO5    // TCA9554 - Touch Panel RST
    #define TP_INT      EXIO6    // TCA9554 - Touch Panel INT
        
    #define GFX_BL_ON_LOW   true  // Backlight ON when pin is LOW


    // Audio Codec pins
    #define PIN_ES7210_BCLK     16
    #define PIN_ES7210_LRCK     7
    #define PIN_ES7210_DIN      15
    #define PIN_ES7210_MCLK     5
    #define PIN_ES8311_DOUT     6


// GFX configuration

    // DataBus pins
    const uint8_t DB_RST = LCD_RST;
    const uint8_t DB_CS = LCD_CS;
    const uint8_t DB_SCK = LCD_SCK;
    const uint8_t DB_MOSI = LCD_MOSI;
    // const TwoWire* wire = &Wire;
    const uint8_t DB_I2C_ADDR = TCA9554_I2C;


    // RGBPanel pins
    const uint8_t RGB_DE = 17;
    const uint8_t RGB_VSYNC = 3;
    const uint8_t RGB_HSYNC = 46;
    const uint8_t RGB_PCLK = 9;
    const uint8_t RGB_R0 = 10;
    const uint8_t RGB_R1 = 11;
    const uint8_t RGB_R2 = 12;
    const uint8_t RGB_R3 = 13;
    const uint8_t RGB_R4 = 14;
    const uint8_t RGB_G0 = 21;
    const uint8_t RGB_G1 = 8;
    const uint8_t RGB_G2 = 18;
    const uint8_t RGB_G3 = 45;
    const uint8_t RGB_G4 = 38;
    const uint8_t RGB_G5 = 39;
    const uint8_t RGB_B0 = 40;
    const uint8_t RGB_B1 = 41;
    const uint8_t RGB_B2 = 42;
    const uint8_t RGB_B3 = 2;
    const uint8_t RGB_B4 = 1;
    // Timing parameters
    const uint16_t RGB_HSYNC_POLARITY = 1;
    const uint16_t RGB_HSYNC_FRONT_PORCH = 10;
    const uint16_t RGB_HSYNC_PULSE_WIDTH = 8;
    const uint16_t RGB_HSYNC_BACK_PORCH = 50;
    const uint16_t RGB_VSYNC_POLARITY = 1;
    const uint16_t RGB_VSYNC_FRONT_PORCH = 10;
    const uint16_t RGB_VSYNC_PULSE_WIDTH = 8;
    const uint16_t RGB_VSYNC_BACK_PORCH = 20;


    // RGB Display pin array
    const uint16_t display_width = 480;
    const uint16_t display_height = 480;
    const uint8_t display_rotation = 0;
    const bool display_auto_flush = true;
    const uint8_t display_rst_pin = GFX_NOT_DEFINED;
    // const uint8_t init_commands = *st7701_type1_init_operations;


#endif // defined(WS_S3_SMART86)

#endif // PIN_CONFIG_H