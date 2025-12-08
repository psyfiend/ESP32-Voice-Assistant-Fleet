#pragma once
#ifndef FLEET_BSP_H
#define FLEET_BSP_H
#include <Arduino.h>
#include <Arduino_GFX_Library.h>

typedef struct
{
    int          CMD;         /*<! The specific LCD command */
    const void   *DATA;       /*<! Buffer that holds the command specific data */
    size_t       DATA_BYTES;  /*<! Size of `data` in memory, in bytes */
    unsigned int DELAY_MS;    /*<! Delay in milliseconds after this command */
} screen_init_cmd;

struct Fleet_BSP
{
    const char *device_name;   // e.g., "Waveshare S3 Smart86"

    // --- Hardware Flags ---
    // These determine which code templates are enabled
    /*
    #define HAS_RGB_PANEL
    #define HAS_QSPI_PANEL
    #define HAS_DSI_PANEL
    #define HAS_TOUCH
    #define HAS_IO_EXPANDER
    */

    // --- I2C Bus ---
    int8_t   I2C_SDA_PIN, I2C_SCL_PIN;
    uint32_t I2C_CLOCK_SPEED;
    uint8_t  EXPANDER_I2C_ADDR;
    
    uint8_t  BOOT_BUTTON_PIN;

    // --- Expander Pins ---
    int8_t EXIO_LCD_RST;
    int8_t EXIO_LCD_CS;
    int8_t EXIO_LCD_SCK;
    int8_t EXIO_LCD_MOSI;
    int8_t EXIO_TP_RST;
    int8_t EXIO_TP_INT;
    int8_t EXIO_AMP_EN;

    // --- LCD Definitions ---
    const char  *LCD_MODEL;   // ST7701, AXS15231B,, ST7262, ILI9xxx, etc.
    uint16_t    WIDTH;
    uint16_t    HEIGHT;
    int8_t      ROTATION;
    bool        AUTO_FLUSH;

    // --- Backlight ---
    int8_t LCD_BL;
    int8_t LCD_BL_ON_LEVEL; // 1 = Active HIGH, 0 = Active LOW
    uint32_t LCD_BL_FREQ;   // PWM Frequency in Hz

    // --- Touch Panel ---
    const char *TP_NAME;            // e.g., "GT911"
    const char *TP_CAPTOUCH_NAME;   // TOUCH_WS_S3_SMART86
    uint8_t     TP_I2C_ADDR;
    uint32_t    TP_I2C_CLOCK_SPEED;
    int8_t      TP_SDA, TP_SCL;
    int8_t      TP_INT, TP_RST;
    int8_t      TP_MAX_TOUCH;

    // --- LCD Control Pins ---
    int8_t LCD_RST;
    int8_t LCD_CS;
    int8_t LCD_SCK;
    int8_t LCD_MOSI;    // QSPI_D0
    int8_t QSPI_D1;
    int8_t QSPI_D2;
    int8_t QSPI_D3;
    int8_t LCD_TE;

    // --- RGB / DSI / QSPI Specific ---
    int8_t LCD_DE, LCD_VSYNC, LCD_HSYNC, LCD_PCLK;

    int8_t R0, R1, R2, R3, R4;
    int8_t G0, G1, G2, G3, G4, G5;
    int8_t B0, B1, B2, B3, B4;

    // --= RGB / DSI Specific Timing =--
    int8_t      HSYNC_POL, VSYNC_POL;
    uint16_t    HSYNC_PWIDTH, HSYNC_BPORCH, HSYNC_FPORCH;
    uint16_t    VSYNC_PWIDTH, VSYNC_BPORCH, VSYNC_FPORCH;

    uint8_t COL_OFFSET1, ROW_OFFSET1;
    uint8_t COL_OFFSET2, ROW_OFFSET2;

    int8_t      PCLK_ACTIVE_NEG;
    uint32_t    PCLK_HZ;
    bool        USE_BIG_ENDIAN;
    uint16_t    DE_IDLE_HIGH, PCLK_IDLE_HIGH;
    size_t      BOUNCE_BUFFER_SIZE_PX;

    uint32_t    PREFER_SPEED, LANE_BIT_RATE;
    
    #ifdef HAS_MIPI_PANEL
    const lcd_init_cmd_t *INIT_CMDS_DSI;
    #endif

    const uint8_t        *INIT_CMDS_RGB;
    size_t      INIT_CMDS_SIZE;

};

struct Fleet_Hardware_Config
{
    // --- Audio Codec ---
    uint8_t I2S_BCLK;
    uint8_t I2S_LRCK;      // 8311_LRCK
    uint8_t I2S_SCLK;      // 8311_SCLK/DMIC_SCL
    uint8_t I2S_DIN;
    uint8_t I2S_MCLK;      // 8311_MCLK
    uint8_t I2S_ASDOUT;    // TDMOUT   I2S_ASDOUT
    uint8_t I2S_SDOUT2;    // TMDMIN

    uint8_t I2S_DSDIN;    // DMIC_DATA

    // --- Audio Amp ---        // NS4150B on WS Smart86 boxes // NS4168 on Guition P4.7
    uint8_t AMP_EN; // CTRL (EXIO3 on Waveshare S3 Smart86)
    uint8_t AMP_LRCK;
    uint8_t AMP_BCLK;
    uint8_t AMP_DIN;

    // --- SD Card Interface ---
    uint8_t TF_CMD;
    uint8_t TF_CLK;
    uint8_t TF_CS;
    uint8_t TF_MOSI;
    uint8_t TF_MISO;
    uint8_t TF_D0;
    uint8_t TF_D1;
    uint8_t TF_D2;
    uint8_t TF_D3;
    uint8_t SPI_MOSI;
    uint8_t SPI_MISO;

    // --= IP5306 (3248W535) Battery Management =--
    uint8_t BAT_ADC;
};

static const screen_init_cmd panel_init_cmds[] = {
//  {cmd, { data }, data_size, delay_ms}
};

#endif