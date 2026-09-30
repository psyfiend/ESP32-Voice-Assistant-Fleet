#pragma once
#ifndef BSP_CYD_P4_4880P443_H
#define BSP_CYD_P4_4880P443_H

#include <Arduino_GFX_Library.h>
#include "Fleet_BSP.h"

// -------------------------------------------------------------------------
// Board: Guition JC4880P443C_I_W (ESP32-P4 + C6), 4.3" portrait
// Driver: ST7701 (MIPI DSI, 2 lanes)
// Resolution: 480x800 (native portrait)
//
// BUILT 2026-09-28 FROM GUITION'S PACK ALONE - NEVER FLASHED.
// reference/Guition Examples/Guition-P4-JC4880P433/ (the folder name says
// P433; everything inside it is the P443). Sources, in the order they were
// trusted:
//   5-Schematic/*.png                        pins (read from the drawings)
//   1-Demo/idf_examples/ESP-IDF_5.5.4/common_components/
//     espressif__esp32_p4_function_ev_board  Guition's modified Espressif
//                                            BSP: DSI bus, LDO, backlight
//     espressif__esp_lcd_st7701             their modified driver: timing
//                                            and the init sequence below
//   1-Demo/arduino_examples/lvgl_v9_sw_rotation/src/lcd/   the Arduino
//                                            port: same init sequence, a
//                                            different clock and lane rate
// Where the two disagree, both values are kept (CLAUDE.md: a commented
// value beside a timing field is a candidate, not clutter).
//
// esp_lcd ONLY (-D DISPLAY_ESPLCD is in its environment from the start):
// there is no Arduino_GFX path for this board and there never needs to be.
// -------------------------------------------------------------------------

#define CYD_P4_4880
#define BSP_PANEL_DRIVER ST7701   // panel chip, a bare name: picks Fleet_Display's driver and sets PANEL_MODEL (Fleet_BSP.h)

// Panel init commands (ST7701), Guition's - identical in their IDF and Arduino
// packs (vendor_specific_init_default in their modified esp_lcd_st7701_mipi.c,
// the live block; the commented-out blocks around it there are older
// attempts). No 0x36 (MADCTL) or 0x3A (COLMOD): the ST7701 driver sends both
// itself before this list, from the panel config (RGB order, 16 bpp).
// Sleep-out and display-on carry one 0x00 parameter byte, as Guition's do.
// NOTE: Must stay here, immediately before the structs below - see
// BSP_WS_P4_TOUCH_LCD_7B.h for why (sizeof() on this array needs its
// complete type at use-site).
static const lcd_init_cmd_t cyd_p4_4880p443_init[] = {
    {0xFF, (uint8_t[]){0x77, 0x01, 0x00, 0x00, 0x13}, 5, 0},
    {0xEF, (uint8_t[]){0x08}, 1, 0},
    {0xFF, (uint8_t[]){0x77, 0x01, 0x00, 0x00, 0x10}, 5, 0},
    {0xC0, (uint8_t[]){0x63, 0x00}, 2, 0},
    {0xC1, (uint8_t[]){0x0D, 0x02}, 2, 0},
    {0xC2, (uint8_t[]){0x10, 0x08}, 2, 0},
    {0xCC, (uint8_t[]){0x10}, 1, 0},

    {0xB0, (uint8_t[]){0x80, 0x09, 0x53, 0x0C, 0xD0, 0x07, 0x0C, 0x09, 0x09, 0x28, 0x06, 0xD4, 0x13, 0x69, 0x2B, 0x71}, 16, 0},
    {0xB1, (uint8_t[]){0x80, 0x94, 0x5A, 0x10, 0xD3, 0x06, 0x0A, 0x08, 0x08, 0x25, 0x03, 0xD3, 0x12, 0x66, 0x6A, 0x0D}, 16, 0},
    {0xFF, (uint8_t[]){0x77, 0x01, 0x00, 0x00, 0x11}, 5, 0},

    {0xB0, (uint8_t[]){0x5D}, 1, 0},
    {0xB1, (uint8_t[]){0x58}, 1, 0},
    {0xB2, (uint8_t[]){0x87}, 1, 0},
    {0xB3, (uint8_t[]){0x80}, 1, 0},
    {0xB5, (uint8_t[]){0x4E}, 1, 0},
    {0xB7, (uint8_t[]){0x85}, 1, 0},
    {0xB8, (uint8_t[]){0x21}, 1, 0},
    {0xB9, (uint8_t[]){0x10, 0x1F}, 2, 0},
    {0xBB, (uint8_t[]){0x03}, 1, 0},
    {0xBC, (uint8_t[]){0x00}, 1, 0},

    {0xC1, (uint8_t[]){0x78}, 1, 0},
    {0xC2, (uint8_t[]){0x78}, 1, 0},
    {0xD0, (uint8_t[]){0x88}, 1, 0},

    {0xE0, (uint8_t[]){0x00, 0x3A, 0x02}, 3, 0},
    {0xE1, (uint8_t[]){0x04, 0xA0, 0x00, 0xA0, 0x05, 0xA0, 0x00, 0xA0, 0x00, 0x40, 0x40}, 11, 0},
    {0xE2, (uint8_t[]){0x30, 0x00, 0x40, 0x40, 0x32, 0xA0, 0x00, 0xA0, 0x00, 0xA0, 0x00, 0xA0, 0x00}, 13, 0},
    {0xE3, (uint8_t[]){0x00, 0x00, 0x33, 0x33}, 4, 0},
    {0xE4, (uint8_t[]){0x44, 0x44}, 2, 0},
    {0xE5, (uint8_t[]){0x09, 0x2E, 0xA0, 0xA0, 0x0B, 0x30, 0xA0, 0xA0, 0x05, 0x2A, 0xA0, 0xA0, 0x07, 0x2C, 0xA0, 0xA0}, 16, 0},
    {0xE6, (uint8_t[]){0x00, 0x00, 0x33, 0x33}, 4, 0},
    {0xE7, (uint8_t[]){0x44, 0x44}, 2, 0},
    {0xE8, (uint8_t[]){0x08, 0x2D, 0xA0, 0xA0, 0x0A, 0x2F, 0xA0, 0xA0, 0x04, 0x29, 0xA0, 0xA0, 0x06, 0x2B, 0xA0, 0xA0}, 16, 0},

    {0xEB, (uint8_t[]){0x00, 0x00, 0x4E, 0x4E, 0x00, 0x00, 0x00}, 7, 0},
    {0xEC, (uint8_t[]){0x08, 0x01}, 2, 0},

    {0xED, (uint8_t[]){0xB0, 0x2B, 0x98, 0xA4, 0x56, 0x7F, 0xFF, 0xFF, 0xFF, 0xFF, 0xF7, 0x65, 0x4A, 0x89, 0xB2, 0x0B}, 16, 0},
    {0xEF, (uint8_t[]){0x08, 0x08, 0x08, 0x45, 0x3F, 0x54}, 6, 0},
    {0xFF, (uint8_t[]){0x77, 0x01, 0x00, 0x00, 0x00}, 5, 0},

    {0x11, (uint8_t[]){0x00}, 1, 120},  // Sleep Out
    {0x29, (uint8_t[]){0x00}, 1, 20},   // Display On
};

const BoardHardware CYD_P4_4880P443_HARDWARE = {
    .device_name  = "CYD P4 JC4880P443",
    .MANUFACTURER = "Guition",
    .MODEL        = "JC4880P443C_I_W",
    .SI_REV       = "unconfirmed",  // Guition's sdkconfig builds for rev >= 1.0; the System Doctor reads the real one

    .SDA_PIN = 7,   // ES_I2C_SDA - codec and touch share it (schematic p.3)
    .SCL_PIN = 8,   // ES_I2C_SCL
    .I2C_CLOCK_SPEED = 400000,

    .BOOT_BUTTON_PIN = 35,  // GPIO35 BOOTMODE, SW1 (schematic p.3)
    .BAT_ADC         = 53,  // BAT+ through 68K/100K (x0.595) -> GPIO53 = ADC2 ch4, as Guition's adc_test (schematic p.4)
    .BAT_DIV_X1000   = 1680, // (68K + 100K) / 100K

    .I2S_AMP_EN = 11,   // PA_CTRL (schematic p.3); BSP_POWER_AMP_IO in Guition's BSP
};
inline const BoardHardware& bsp_hw = CYD_P4_4880P443_HARDWARE;

const DisplayConfig CYD_P4_4880P443_DISPLAY = {
    .PANEL_MODEL = BSP_STR(BSP_PANEL_DRIVER),
    .WIDTH       = 480,
    .HEIGHT      = 800,
    .ROTATION    = 0,     // native portrait (the CYD_S3_3248's job, owner 2026-09-28). 1 = landscape, benched 2026-09-29: as fast as portrait, 4x3 "a great fit"
    .AUTO_FLUSH  = true,

    .BL_PIN      = 23,    // LCD_PWM (schematic p.3)
    .BL_ON_LEVEL = 1,     // Active HIGH
    .BL_FREQ     = 5000,  // Guition's BSP: 5 kHz, 10-bit (the 1060's is 20 kHz)

    // GPIO5 reaches the panel connector (FPC1 pin 23, schematic p.2) and
    // Guition's Arduino port resets through it; their IDF BSP leaves the pin
    // unconnected (-1) and relies on the driver's software reset (SWRESET).
    // Polarity: the driver's default, active LOW.
    .RST = 5,             // -1 in Guition's IDF BSP
    // TE: FPC1 pin 22 is marked not connected (schematic p.2).

    // ---= MIPI Timing =---
    // Guition's IDF pack ("ST7701_480_360_PANEL_60HZ_DPI_CONFIG" - named 480x360,
    // sized 480x800): 28 MHz over 576 x 976 = 49.8 Hz. Their Arduino port:
    // the same porches at 34 MHz = 60.5 Hz. Neither is proven on our unit.
    .HSYNC_PWIDTH = 12,
    .HSYNC_BPORCH = 42,
    .HSYNC_FPORCH = 42,
    .VSYNC_PWIDTH = 2,
    .VSYNC_BPORCH = 8,
    .VSYNC_FPORCH = 166,

    .PCLK_HZ      = 28000000,
    .PREFER_SPEED = 28000000, // 34000000 in Guition's Arduino port (60.5 Hz)

    // ---= DSI Specific =---
    .TEST_MIPI_DSI_PHY_PWR_LDO_CHAN       = 3,     // LDO_VO3 -> VDD_MIPI_DPHY
    .TEST_MIPI_DSI_PHY_PWR_LDO_VOLTAGE_MV = 2500,

    .NUM_DSI_LANES = 2,
    .LANE_BIT_RATE = 750, // Guition's IDF BSP; 500 in their Arduino port. 34 MHz x 16 bpp over 2 lanes is 272 Mbps a lane, so both have room

    // ---= Init Commands =---
    .INIT_CMDS_DSI  = cyd_p4_4880p443_init,
    .INIT_CMDS_SIZE = sizeof(cyd_p4_4880p443_init) / sizeof(lcd_init_cmd_t),

    // Panel diagonal in tenths of an inch (4.3" Guition JC4880P443).
    // 480x800 over 4.3" = ~217 PPI, so bspUiScale() = 1.28.
    .DIAGONAL_IN = 43,
};
inline const DisplayConfig& bsp_display = CYD_P4_4880P443_DISPLAY;

const TouchConfig CYD_P4_4880P443_TOUCH = {
    .NAME            = "GT911",
    .I2C_ADDR        = 0x5D, // or 0x14 - GT911 picks by INT level at reset
    .I2C_BACKUP_ADDR = 0x14,
    .SDA             = 7,
    .SCL             = 8,
    .INT             = 21,   // TOUCH_INT (schematic p.3) - Guition's BSP leaves it NC
    .RST             = 22,   // TOUCH_RST (schematic p.3) - Guition's BSP leaves it NC
    .MAX_TOUCH       = 5,
};
inline const TouchConfig& bsp_touch = CYD_P4_4880P443_TOUCH;

const AudioConfig CYD_P4_4880P443_AUDIO = {
    // ES8311 codec only - no ES7210. The mic comes through the ES8311's own
    // ADC (AudioManager's ES8311-both-ways path, CLAUDE.md). The schematic's
    // "ES7210_SDOUT" net on GPIO48 is the ES8311's ASDOUT, not a second chip.
    .I2S_SDA_PIN = 7,
    .I2S_SCL_PIN = 8,

    .I2S_8311_ADDR = 0x18,   // ES8311_CODEC_DEFAULT_ADDR in Guition's BSP

    .I2S_MCLK = 13,   // CODEC_I2S0_MCLK
    .I2S_BCLK = 12,   // CODEC_I2S0_SCLK
    .I2S_LRCK = 10,   // CODEC_I2S0_LRCK

    .I2S_DIN  = 48,   // ES8311 ASDOUT -> ESP (mic)
    .I2S_DOUT = 9,    // CODEC_I2S0_DSDIN (speaker)

    .AUDIO_INPUT_SAMPLE_RATE  = 16000,
    .AUDIO_OUTPUT_SAMPLE_RATE = 16000,
    .I2S_MCLK_MULTIPLE        = 256,

    .I2S_DATA_BIT_WIDTH = 16, // I2S_DATA_BIT_WIDTH_16BIT
    .I2S_SLOT_BIT_WIDTH = 16, // I2S_SLOT_BIT_WIDTH_16BIT
    .I2S_SLOT_MODE      = 2,  // I2S_SLOT_MODE_STEREO

    // Codec Config (ES8311)
    .DAC_BIT_LENGTH = 16, // ES8311_RESOLUTION_16
};
inline const AudioConfig& bsp_audio = CYD_P4_4880P443_AUDIO;

const StorageConfig CYD_P4_4880P443_STORAGE = {
    // TF card power is always on (R4 0R from LDO VO4; the GPIO45 switch is not fitted).
    .TF_CMD = 44,
    .TF_CLK = 43,
    .TF_D0  = 39,
    .TF_D1  = 40,
    .TF_D2  = 41,
    .TF_D3  = 42,
};
inline const StorageConfig& bsp_storage = CYD_P4_4880P443_STORAGE;

const LvglConfig CYD_P4_4880P443_LVGL = {
    .DOUBLE_BUFFERING = true,
    .DRAW_BUF_HEIGHT  = 50,
    .BUFFER_SIZE_PX   = 480 * 50,   // WIDTH * DRAW_BUF_HEIGHT
};
inline const LvglConfig& bsp_lvgl = CYD_P4_4880P443_LVGL;

/* Also on this board, not in the BSP (FUTURE_IMPROVEMENTS.md):
   - OV02C10 camera on MIPI-CSI (FPC2; datasheet in 4-Driver_IC_Data_Sheet/)
   - RS485 (schematic p.5; Guition's uart_*_rs485 examples)
   - Ethernet RMII pins (GPIO28-35, 49-52) are labelled on schematic p.3 -
     whether the C_I_W variant has a PHY fitted is unconfirmed. */

#endif // BSP_CYD_P4_4880P443_H
