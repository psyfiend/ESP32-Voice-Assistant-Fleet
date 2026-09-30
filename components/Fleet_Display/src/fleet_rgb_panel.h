#pragma once
//
// fleet_rgb_panel - RGB (parallel) panel bring-up on raw esp_lcd. Milestone
// 2.9, step 4: WS_S3_4B's ST7701. The RGB counterpart of fleet_dsi_panel.
//
// C, like fleet_dsi_panel, for the same reason: the panel drivers' config
// macros are designated initialisers C++ rejects. Fleet_Display.cpp reads the
// BSP and passes plain values in.
//
// Only on boards on the esp_lcd RGB path (every Fleet_Display file is empty
// on boards that do not use it).
//
#if defined(DISPLAY_ESPLCD) && defined(HAS_RGB_PANEL)

#include <stddef.h>
#include <stdint.h>
#include "esp_err.h"
#include "esp_lcd_types.h"
#include "esp_lcd_panel_rgb.h"
#include "esp_io_expander.h"

#ifdef __cplusplus
extern "C" {
#endif

// One command of a panel's init sequence. Same layout as the BSP's
// lcd_init_cmd_t and the drivers' own types. (fleet_dsi_init_cmd_t's twin;
// the DSI header is not built on an S3. Step 6 merges them.)
typedef struct {
    int          cmd;
    const void  *data;
    size_t       data_bytes;
    unsigned int delay_ms;
} fleet_rgb_init_cmd_t;

typedef struct {
    // --- The RGB link ---
    uint32_t pclk_hz;
    int      h_res, v_res;
    int      hsync_pulse_width, hsync_back_porch, hsync_front_porch;
    int      vsync_pulse_width, vsync_back_porch, vsync_front_porch;
    int      hsync_idle_low, vsync_idle_low, de_idle_high, pclk_active_neg, pclk_idle_high;
    int      de_gpio, vsync_gpio, hsync_gpio, pclk_gpio, disp_gpio;
    int      data_gpio[16];        // bit order: B0-B4, G0-G5, R0-R4
    size_t   bounce_buffer_px;     // 0 = none
    int      num_fbs;              // 1-3, in PSRAM
    // --- The panel controller ---
    int      panel_bits_per_pixel; // what the panel is told (ST7701: 18 -> COLMOD 0x60)
    // Where its init commands go: a 3-wire SPI bit-banged through an I/O
    // expander's pins (pin NUMBERS, 0-7), as on WS_S3_4B.
    esp_io_expander_handle_t expander;
    int      exp_cs, exp_scl, exp_sda;
    const fleet_rgb_init_cmd_t *init_cmds;   // NULL = the driver's own default
    size_t   init_cmds_count;
} fleet_rgb_cfg_t;

// --= One vendored panel driver, as fleet_rgb_panel_new() sees it =--
// The RGB twin of fleet_dsi_driver_t (fleet_dsi_panel.h, which explains the
// scheme): a wrapper fleet_rgb_<chip>.c defines fleet_rgb_driver_<CHIP>, and
// Fleet_Display pastes BSP_PANEL_DRIVER into the name.
typedef struct {
    const char *name;
    // Open the command channel (the chip's own 3-wire SPI, via cfg->expander),
    // send the init sequence, and create the RGB panel from `rgb`.
    esp_err_t (*create)(const fleet_rgb_cfg_t *cfg, const esp_lcd_rgb_panel_config_t *rgb,
                        esp_lcd_panel_handle_t *panel);
} fleet_rgb_driver_t;

#define FLEET_RGB_DRIVER_(chip) fleet_rgb_driver_##chip
#define FLEET_RGB_DRIVER(chip)  FLEET_RGB_DRIVER_(chip)

// Builds the RGB panel config from `cfg`, has `drv` create and initialise the
// panel, and returns it with its frame buffers (fbs[0 .. num_fbs-1]).
esp_err_t fleet_rgb_panel_new(const fleet_rgb_cfg_t *cfg, const fleet_rgb_driver_t *drv,
                              esp_lcd_panel_handle_t *ret_panel, void **fbs);

// For the wrappers: as FLEET_DSI_COPY_CMDS (fleet_dsi_panel.h).
#define FLEET_RGB_COPY_CMDS(type, cfg, out)                                     \
    do {                                                                        \
        (out) = NULL;                                                           \
        if ((cfg)->init_cmds && (cfg)->init_cmds_count) {                       \
            (out) = (type *)calloc((cfg)->init_cmds_count, sizeof(type));       \
            if (!(out)) return ESP_ERR_NO_MEM;                                  \
            for (size_t i_ = 0; i_ < (cfg)->init_cmds_count; i_++) {            \
                (out)[i_].cmd        = (cfg)->init_cmds[i_].cmd;                \
                (out)[i_].data       = (cfg)->init_cmds[i_].data;               \
                (out)[i_].data_bytes = (cfg)->init_cmds[i_].data_bytes;         \
                (out)[i_].delay_ms   = (cfg)->init_cmds[i_].delay_ms;           \
            }                                                                   \
        }                                                                       \
    } while (0)

#define FLEET_RGB_STR_(x) #x
#define FLEET_RGB_STR(x)  FLEET_RGB_STR_(x)
#define FLEET_RGB_VER(chip) FLEET_RGB_STR(ESP_LCD_##chip##_VER_MAJOR) "." \
                            FLEET_RGB_STR(ESP_LCD_##chip##_VER_MINOR) "." \
                            FLEET_RGB_STR(ESP_LCD_##chip##_VER_PATCH)

#ifdef __cplusplus
}
#endif

#endif // DISPLAY_ESPLCD && HAS_RGB_PANEL
