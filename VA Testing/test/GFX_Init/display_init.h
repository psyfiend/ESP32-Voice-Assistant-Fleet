#ifndef DISPLAY_INIT_H
#define DISPLAY_INIT_H

#include <Arduino.h>
#include "pin_config.h"
#include "Arduino_GFX_Library.h"

void init_display();

extern Arduino_RGB_Display *gfx;


// DataBus pin array

extern const uint8_t DB_RST;
extern const uint8_t DB_CS;
extern const uint8_t DB_SCK;
extern const uint8_t DB_MOSI;
extern const TwoWire *wire;
extern const uint8_t DB_I2C_ADDR;


// RGB Panel pin array

extern const uint8_t RGB_DE;
extern const uint8_t RGB_VSYNC;
extern const uint8_t RGB_HSYNC;
extern const uint8_t RGB_PCLK;
extern const uint8_t RGB_R0;
extern const uint8_t RGB_R1;
extern const uint8_t RGB_R2;
extern const uint8_t RGB_R3;
extern const uint8_t RGB_R4;
extern const uint8_t RGB_G0;
extern const uint8_t RGB_G1;
extern const uint8_t RGB_G2;
extern const uint8_t RGB_G3;
extern const uint8_t RGB_G4;
extern const uint8_t RGB_G5;
extern const uint8_t RGB_B0;
extern const uint8_t RGB_B1;
extern const uint8_t RGB_B2;
extern const uint8_t RGB_B3;
extern const uint8_t RGB_B4;
extern const uint8_t RGB_B5;
extern const uint16_t RGB_HSYNC_POLARITY;
extern const uint16_t RGB_HSYNC_FRONT_PORCH;
extern const uint16_t RGB_HSYNC_PULSE_WIDTH;
extern const uint16_t RGB_HSYNC_BACK_PORCH;
extern const uint16_t RGB_VSYNC_POLARITY;
extern const uint16_t RGB_VSYNC_FRONT_PORCH;
extern const uint16_t RGB_VSYNC_PULSE_WIDTH;
extern const uint16_t RGB_VSYNC_BACK_PORCH;



// RGB Display pin array

extern const uint16_t display_width;
extern const uint16_t display_height;
extern const uint8_t display_rotation;
extern const bool display_auto_flush;
// bus
extern const uint8_t display_rst_pin;
// extern const uint8_t init_commands;
// sizeof init_operations
extern const uint8_t display_col_offset1;
extern const uint8_t display_row_offset1;
extern const uint8_t display_col_offset2;
extern const uint8_t display_row_offset2;


#endif // DISPLAY_INIT_H