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

    // --- Expander Pins ---
    uint8_t  EXPANDER_I2C_ADDR;
    int8_t   EXIO_LCD_CS;   // Header Pin 6
    int8_t   EXIO_LCD_MOSI; // Header Pin 8
    int8_t   EXIO_LCD_SCK;  // Header Pin 10
    int8_t   EXIO_AMP_EN;   // Header Pin 12
    int8_t   EXIO_HDR_14;   // Header Pin 14 - PWR / Key3
    int8_t   EXIO_TP_RST;   // Header Pin 16
    int8_t   EXIO_TP_INT;   // Header Pin 18
    int8_t   EXIO_LCD_RST;  // Header Pin 17

    // --- LCD Definitions ---
    const char  *LCD_MODEL;   // ST7701, AXS15231B,, ST7262, ILI9xxx, etc.
    uint32_t    WIDTH;
    uint32_t    HEIGHT;
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
    uint32_t    PCLK_HZ, PREFER_SPEED;
    bool        USE_BIG_ENDIAN;
    uint16_t    DE_IDLE_HIGH, PCLK_IDLE_HIGH;

    size_t      BOUNCE_BUFFER_SIZE_PX;

    // --= LVGL Settings =--
    bool        DOUBLE_BUFFERING;
    size_t      DRAW_BUF_HEIGHT;
    size_t      BUFFER_SIZE_PX;

    uint32_t    LANE_BIT_RATE;
    
    const uint8_t        *INIT_CMDS_RGB;
    size_t      INIT_CMDS_SIZE;

};

struct Fleet_Hardware_Config
{
    // --- Audio Codec ---
    uint8_t I2S_SDA_PIN, I2S_SCL_PIN;    // I2C SDA and SCL
    uint8_t I2S_7210_ADDR;               // ES7210 ADC/Mics I2C Address
    uint8_t I2S_8311_ADDR;               // ES8311 DAC/Amp I2C Address

    uint8_t I2S_MCLK;
    uint8_t I2S_BCLK;
    uint8_t I2S_LRCK;
    uint8_t I2S_SCLK;

    uint8_t I2S_DIN, I2S_DOUT;

    uint32_t AUDIO_INPUT_SAMPLE_RATE;
    uint32_t AUDIO_OUTPUT_SAMPLE_RATE;

    uint32_t I2S_MCLK_MULTIPLE;

    // --= New Audio Configuration Fields =--
    // Using int types to avoid complex dependency chains in BSP headers
    // These map to the standard ESP-IDF / Audio HAL enums
    uint8_t I2S_DATA_BIT_WIDTH; // 16, 24, 32
    uint8_t I2S_SLOT_BIT_WIDTH; // 16, 24, 32
    uint8_t I2S_SLOT_MODE;      // 0 = Mono, 1 = Stereo
    
    // Codec Specifics (ES7210)
    uint8_t CODEC_INPUT_MODE;   // Maps to audio_hal_adc_input_t (0=Line1, 2=All)
    uint8_t CODEC_CODEC_MODE;   // Maps to audio_hal_codec_mode_t (0=Encode, 1=Decode, 2=Both)
    uint8_t CODEC_IFACE_I2S_FMT;      // Maps to audio_hal_iface_format_t (0=Normal, 1=Left, etc)
    uint8_t CODEC_IFACE_SAMPLES;      // Maps to audio_hal_iface_samples_t (0=8K, 1=11.025K, 2=16K, etc)
    uint8_t CODEC_IFACE_BIT_LENGTH;   // Maps to audio_hal_iface_bits_t (1=16bit, 2=24bit)
    
    // Output Codec Specifics (ES8311)
    uint8_t DAC_BIT_LENGTH;     // Maps to es8311_resolution_t (16, 24, 32)

    // Mic Gain / Selection (Standard Mode)
    uint8_t MIC_SELECTED;       // Bitmask for ES7210 (Mic1 | Mic2)
    uint8_t MIC_GAIN_DB;        // Gain Enum (e.g. GAIN_36DB)

    // Mic Gain / Selection (AEC Mode - HAS_EAC)
    uint8_t AEC_MIC_SELECTED;   // Usually Mic1|2|3|4 (0x0F)
    uint8_t AEC_MIC_GAIN_DB;    // Often lower gain for AEC

    // --- Audio Amp --- // NS4150B on WS Smart86 boxes // NS4168 on Guition P4.7
    // Set to -1 if managed externally (Expander) or if not present. 
    // Set to GPIO number if directly connected.
    int8_t I2S_AMP_EN;            // EXIO3 on Waveshare S3 Smart86
    uint8_t AMP_LRCK;
    uint8_t AMP_BCLK;
    uint8_t AMP_DIN;

    // --= Button =--
    uint8_t  BOOT_BUTTON_PIN;

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