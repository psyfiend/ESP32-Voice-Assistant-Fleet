#pragma once
#ifndef FLEET_BSP_H
#define FLEET_BSP_H
#include <Arduino.h>
#include <Arduino_GFX_Library.h>

#if !(defined(ESP32) && defined(CONFIG_IDF_TARGET_ESP32P4))
// lcd_init_cmd_t is normally provided by Arduino_ESP32DSIPanel.h, but that
// header only defines it when building for the P4 - MIPI-DSI is P4-exclusive
// hardware, so the type doesn't exist at all on S3 builds. Declare an
// identical fallback here so every board's BSP struct has the same shape
// regardless of target; S3 boards never actually populate INIT_CMDS_DSI.
typedef struct {
    int          cmd;
    const void   *data;
    size_t       data_bytes;
    unsigned int delay_ms;
} lcd_init_cmd_t;
#endif

// -----------------------------------------------------------------------
// Fleet BSP - each board header declares up to seven independent, flat
// per-board structs (not nested inside one another) grouped by function:
// BoardHardware, ExpanderConfig, DisplayConfig, TouchConfig, LvglConfig,
// AudioConfig, StorageConfig. A board only declares the ones it actually
// needs (e.g. only 2 of 8 boards populate an ExpanderConfig instance).
//
// Naming convention per board (see any BSP_<NAME>.h for a concrete example):
//   - `#define <BOARD>`               short device-identity macro (e.g. WS_P4_7B),
//                                       used fleet-wide for #ifdef gating
//   - `const BoardHardware <BOARD>_LONGNAME_HARDWARE = {...}`
//     `const DisplayConfig <BOARD>_LONGNAME_DISPLAY  = {...}`   (etc., one per group)
//   - `inline const BoardHardware& bsp_hw = <BOARD>_LONGNAME_HARDWARE;`
//     `inline const DisplayConfig& bsp_display = <BOARD>_LONGNAME_DISPLAY;` (etc.)
// App code only ever reads through the lowercase bsp_* aliases
// (bsp_hw.SDA_PIN, bsp_display.WIDTH, bsp_touch.MAX_TOUCH, ...) - never the
// concrete per-board instance name.
// -----------------------------------------------------------------------

// --= Board Hardware =--
// Physical-interface-level scalars that don't belong to a specific
// peripheral group: shared I2C bus, device identity strings, and the
// handful of standalone GPIOs (button, battery ADC, amp enable) not worth
// their own struct. I2C_CLOCK_SPEED lives here as the single shared value -
// previously duplicated as a dead I2C-bus-level field plus a separately
// consumed per-touch-controller field; only the touch-controller value was
// ever actually read by any code, so this is that same value, unified.
struct BoardHardware {
    const char *device_name;   // Full display string, e.g. "Waveshare ESP32-P4-Touch-LCD-7B" - used for the GUI header label only
    const char *MANUFACTURER;  // e.g. "Waveshare", "Guition"
    const char *MODEL;         // Board model designation, e.g. "ESP32-P4-WIFI6-Touch-LCD-7B" - NOT the LCD driver chip (see DisplayConfig.PANEL_MODEL for that)
    const char *SI_REV;        // P4 chip revision, either "rev1_3" or "rev3_x" - documentary only for now, nothing branches on it yet

    int8_t   SDA_PIN, SCL_PIN;
    uint32_t I2C_CLOCK_SPEED;

    uint8_t BOOT_BUTTON_PIN;
    uint8_t BAT_ADC;      // IP5306 (3248W535 / 8048W550) battery management

    // -1 if managed externally (via Expander) or absent; GPIO number if direct.
    int8_t I2S_AMP_EN;    // NS4150B on WS Smart86 boxes // NS4168 on Guition P4.7
};

// --= IO Expander =--
// EXPANDER_I2C_ADDR / LCD_CS / LCD_MOSI / LCD_SCK / AMP_EN / HDR_14 are
// XCA9554-shaped (WS_S3_4B, where the expander doubles as the panel's
// soft-SPI databus). LCD_BL / SD_CS / USB_SEL are CH422G-shaped
// (WS_S3_5B, where the expander is a side-channel GPIO bank, not the
// databus) - CH422G also ignores I2C_ADDR entirely, see CH422G.h. Only 2 of
// the fleet's boards use this struct at all; each populates its own
// chip-specific subset of fields.
struct ExpanderConfig {
    uint8_t I2C_ADDR;
    int8_t  LCD_CS;    // Header Pin 6  (XCA9554)
    int8_t  LCD_MOSI;  // Header Pin 8  (XCA9554)
    int8_t  LCD_SCK;   // Header Pin 10 (XCA9554)
    int8_t  AMP_EN;    // Header Pin 12 (XCA9554)
    int8_t  HDR_14;    // Header Pin 14 - PWR / Key3 (XCA9554)
    int8_t  TP_RST;    // Header Pin 16
    int8_t  TP_INT;    // Header Pin 18
    int8_t  LCD_RST;   // Header Pin 17
    int8_t  LCD_BL;    // CH422G IO pin - backlight enable
    int8_t  SD_CS;     // CH422G IO pin - SD card chip-select
    int8_t  USB_SEL;   // CH422G IO pin - USB/CAN header mux select
};

// --= Display (Panel + Backlight: RGB / QSPI / MIPI-DSI) =--
// A given board only fills in the pin/timing subset its own interface
// needs - e.g. an RGB board sets DE/VSYNC/HSYNC/PCLK + the R/G/B pin rows
// and leaves the DSI-specific fields at 0; a QSPI board sets
// CS/SCK/MOSI/QSPI_D1-3 and leaves the RGB pins alone.
struct DisplayConfig {
    const char *PANEL_MODEL;   // LCD driver chip, e.g. ST7701, AXS15231B, ST7262, HX8394, EK79007 - NOT the board model (see BoardHardware.MODEL for that)
    uint32_t    WIDTH;
    uint32_t    HEIGHT;
    int8_t      ROTATION;
    bool        AUTO_FLUSH;

    // --- Backlight ---
    int8_t   BL_PIN;
    int8_t   BL_ON_LEVEL; // 1 = Active HIGH, 0 = Active LOW
    uint32_t BL_FREQ;      // PWM frequency in Hz; 0 = on/off only, no PWM

    // --- Control / bus pins ---
    int8_t RST, CS, SCK, MOSI; // MOSI doubles as QSPI_D0
    int8_t QSPI_D1, QSPI_D2, QSPI_D3;
    int8_t TE;

    // --- RGB / DSI parallel interface pins ---
    int8_t DE, VSYNC, HSYNC, PCLK;
    int8_t R0, R1, R2, R3, R4;
    int8_t G0, G1, G2, G3, G4, G5;
    int8_t B0, B1, B2, B3, B4;

    // --- RGB / DSI timing ---
    int8_t   HSYNC_POL, VSYNC_POL;
    uint32_t HSYNC_PWIDTH, HSYNC_BPORCH, HSYNC_FPORCH;
    uint32_t VSYNC_PWIDTH, VSYNC_BPORCH, VSYNC_FPORCH;
    uint8_t  COL_OFFSET1, ROW_OFFSET1;
    uint8_t  COL_OFFSET2, ROW_OFFSET2;
    int8_t   PCLK_ACTIVE_NEG;
    uint32_t PCLK_HZ, PREFER_SPEED;
    bool     USE_BIG_ENDIAN;
    uint16_t DE_IDLE_HIGH, PCLK_IDLE_HIGH;
    size_t   BOUNCE_BUFFER_SIZE_PX;

    // --- MIPI-DSI specific ---
    uint16_t TEST_MIPI_DSI_PHY_PWR_LDO_CHAN;
    uint16_t TEST_MIPI_DSI_PHY_PWR_LDO_VOLTAGE_MV;
    uint8_t  NUM_DSI_LANES;
    uint32_t LANE_BIT_RATE;

    // --- Panel init command tables ---
    // Arduino_GFX's RGB and DSI display classes take two genuinely
    // different init-command encodings - Arduino_RGB_Display wants a raw
    // opcode byte-stream, Arduino_DSI_Display wants a structured
    // lcd_init_cmd_t array - so both fields exist; a board fills in
    // whichever one its own panel class actually needs.
    const uint8_t         *INIT_CMDS_RGB;
    const lcd_init_cmd_t  *INIT_CMDS_DSI;
    size_t                 INIT_CMDS_SIZE;
};

// --= Touch Panel =--
struct TouchConfig {
    const char *NAME;            // e.g., "GT911"
    const char *CAPTOUCH_NAME;   // TOUCH_WS_S3_SMART86 (bb_captouch chip ID name)
    uint8_t     I2C_ADDR;
    uint8_t     I2C_BACKUP_ADDR; // Some GT911 units answer on this address instead, depending on INT level at reset
    int8_t      SDA, SCL;
    int8_t      INT, RST;
    int8_t      MAX_TOUCH;
};

// --= LVGL Settings =--
struct LvglConfig {
    bool   DOUBLE_BUFFERING;
    size_t DRAW_BUF_HEIGHT;
    size_t BUFFER_SIZE_PX;
};

// --= Audio Codec =--
// I2S protocol pins and codec register settings only - the amp-enable GPIO
// lives on BoardHardware.I2S_AMP_EN instead, since it's a plain GPIO output,
// not an I2S signal or codec setting.
struct AudioConfig {
    uint8_t I2S_SDA_PIN, I2S_SCL_PIN; // I2C SDA/SCL shared by ES7210/ES8311
    uint8_t I2S_7210_ADDR;            // ES7210 ADC/Mics I2C Address
    uint8_t I2S_8311_ADDR;            // ES8311 DAC/Amp I2C Address

    uint8_t I2S_MCLK;
    uint8_t I2S_BCLK;
    uint8_t I2S_LRCK;
    uint8_t I2S_SCLK;
    uint8_t I2S_DIN, I2S_DOUT;

    uint32_t AUDIO_INPUT_SAMPLE_RATE;
    uint32_t AUDIO_OUTPUT_SAMPLE_RATE;
    uint32_t I2S_MCLK_MULTIPLE;

    // Using int types to avoid complex dependency chains in BSP headers -
    // these map to the standard ESP-IDF / Audio HAL enums.
    uint8_t I2S_DATA_BIT_WIDTH; // 16, 24, 32
    uint8_t I2S_SLOT_BIT_WIDTH; // 16, 24, 32
    uint8_t I2S_SLOT_MODE;      // matches i2s_slot_mode_t: 1 = Mono, 2 = Stereo

    // Codec Specifics (ES7210)
    uint8_t CODEC_INPUT_MODE;   // Maps to audio_hal_adc_input_t (0=Line1, 2=All)
    uint8_t CODEC_CODEC_MODE;   // Maps to audio_hal_codec_mode_t (0=Encode, 1=Decode, 2=Both)
    uint8_t CODEC_IFACE_I2S_FMT;    // Maps to audio_hal_iface_format_t (0=Normal, 1=Left, etc)
    uint8_t CODEC_IFACE_SAMPLES;    // Maps to audio_hal_iface_samples_t (0=8K, 1=11.025K, 2=16K, etc)
    uint8_t CODEC_IFACE_BIT_LENGTH; // Maps to audio_hal_iface_bits_t (1=16bit, 2=24bit)

    // Output Codec Specifics (ES8311)
    uint8_t DAC_BIT_LENGTH;     // Maps to es8311_resolution_t (16, 24, 32)

    // Mic Gain / Selection (Standard Mode)
    uint8_t MIC_SELECTED;       // Bitmask for ES7210 (Mic1 | Mic2)
    uint8_t MIC_GAIN_DB;        // Gain Enum (e.g. GAIN_36DB)

    // Mic Gain / Selection (AEC Mode - ENABLE_AEC)
    uint8_t AEC_MIC_SELECTED;   // Usually Mic1|2|3|4 (0x0F)
    uint8_t AEC_MIC_GAIN_DB;    // Often lower gain for AEC
};

// --= Storage (SD Card / generic SPI) =--
struct StorageConfig {
    // SD Card Interface (SDMMC 1/4-bit or SPI mode, depending on board)
    uint8_t TF_CMD, TF_CLK, TF_CS, TF_MOSI, TF_MISO;
    uint8_t TF_D0, TF_D1, TF_D2, TF_D3;

    // Generic SPI Pins - shared with SD-over-SPI on boards without SDMMC
    uint8_t SPI_MOSI, SPI_MISO, SPI_CS, SPI_CLK;
};

#endif
