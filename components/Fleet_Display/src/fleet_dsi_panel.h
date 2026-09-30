#pragma once
//
// fleet_dsi_panel - MIPI-DSI panel bring-up on raw esp_lcd. Milestone 2.9,
// step 2: docs/archive/display/esplcd-step2.md.
//
// C, not C++, on purpose: the panel drivers' configuration macros use
// designated initialisers out of declaration order, which C accepts and C++
// rejects (design §4a). Fleet_Display.cpp reads the BSP (C++) and passes plain
// values in here.
//
#include "soc/soc_caps.h"
#if SOC_MIPI_DSI_SUPPORTED

#include <stddef.h>
#include <stdint.h>
#include "esp_err.h"
#include "esp_lcd_types.h"
#include "esp_lcd_mipi_dsi.h"
#include "esp_lcd_panel_dev.h"

#ifdef __cplusplus
extern "C" {
#endif

// One command of a panel's init sequence. Same layout as the BSP's
// lcd_init_cmd_t and the vendor drivers' own types.
typedef struct {
    int          cmd;
    const void  *data;
    size_t       data_bytes;
    unsigned int delay_ms;
} fleet_dsi_init_cmd_t;

typedef struct {
    // DSI PHY power: an on-chip LDO channel and its voltage.
    int      ldo_chan;
    int      ldo_mv;
    // Link.
    int      num_lanes;
    uint32_t lane_bit_rate_mbps;
    // Video timing (physical, unrotated).
    int      h_res, v_res;
    uint32_t dpi_clock_hz;
    int      hsync_pulse_width, hsync_back_porch, hsync_front_porch;
    int      vsync_pulse_width, vsync_back_porch, vsync_front_porch;
    // Reset line and its polarity (1 = assert HIGH; the HX8394's case, not
    // the ST7703's).
    int      reset_gpio;
    int      reset_active_high;
    // The panel's init sequence; NULL = the driver's own default.
    const fleet_dsi_init_cmd_t *init_cmds;
    size_t   init_cmds_count;
    // Frame buffers the panel allocates and owns (1-3).
    int      num_fbs;
} fleet_dsi_cfg_t;

// --= One vendored panel driver, as fleet_dsi_panel_new() sees it =--
//
// Each vendored driver has a small wrapper file, fleet_dsi_<chip>.c, that
// defines ONE of these, named fleet_dsi_driver_<CHIP> - CHIP spelled exactly
// as the BSP's BSP_PANEL_DRIVER (fleet_dsi_driver_HX8394). Fleet_Display
// pastes that macro into the name, so it reaches the right driver with no
// list of chips anywhere. Adding a DSI panel: vendor its driver, write its
// wrapper (copy one of the four), add its version line to
// fleet_display_versions.h, and set BSP_PANEL_DRIVER in the board's BSP.
//
// A BSP naming a chip with no wrapper fails at LINK time:
//     undefined reference to `fleet_dsi_driver_<CHIP>'
// Wrappers nothing names are dropped by the linker, so a board carries only
// its own driver.
//
// The wrapper exists because the drivers are not interchangeable: each has
// its own vendor-config struct (the HX8394 and EK79007 take a lane count,
// the ST7703 and JD9165 do not) and its own init-command type.
typedef struct {
    // "esp_lcd_hx8394 2.1.0 (waveshare)", for reports. Versions from
    // fleet_display_versions.h, the same numbers the driver itself logs.
    const char *name;
    // Create the panel on `bus`/`io` with the video timing in `dpi` and our
    // BSP's init sequence (cfg->init_cmds), then reset and initialise it.
    esp_err_t (*create)(const fleet_dsi_cfg_t *cfg, esp_lcd_dsi_bus_handle_t bus,
                        esp_lcd_panel_io_handle_t io, const esp_lcd_dpi_panel_config_t *dpi,
                        const esp_lcd_panel_dev_config_t *dev, esp_lcd_panel_handle_t *panel);
} fleet_dsi_driver_t;

#define FLEET_DSI_DRIVER_(chip) fleet_dsi_driver_##chip
#define FLEET_DSI_DRIVER(chip)  FLEET_DSI_DRIVER_(chip)   // expands BSP_PANEL_DRIVER first

// Powers the PHY, creates the DSI bus, the command IO and the panel through
// `drv`, and returns the panel with its frame buffers (fbs[0 .. num_fbs-1]).
// Everything but the driver itself is the same for every chip. On failure,
// everything created so far is released.
esp_err_t fleet_dsi_panel_new(const fleet_dsi_cfg_t *cfg, const fleet_dsi_driver_t *drv,
                              esp_lcd_panel_handle_t *ret_panel, void **fbs);

// For the wrappers: `out` = a calloc'd copy of cfg's init sequence in the
// driver's own init-command `type`, field by field (so a driver whose type
// differs fails to compile rather than misreading memory - "copied rather
// than cast", as step 2 decided). NULL when there is no sequence; the driver
// then uses its own default. Returns ESP_ERR_NO_MEM from the CALLER when out
// of memory. The drivers read it only during init(): free() it straight after.
#define FLEET_DSI_COPY_CMDS(type, cfg, out)                                     \
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

// For the wrappers' `name`: "2.1.0" from fleet_display_versions.h's
// ESP_LCD_<CHIP>_VER_* lines.
#define FLEET_DSI_STR_(x) #x
#define FLEET_DSI_STR(x)  FLEET_DSI_STR_(x)
#define FLEET_DSI_VER(chip) FLEET_DSI_STR(ESP_LCD_##chip##_VER_MAJOR) "." \
                            FLEET_DSI_STR(ESP_LCD_##chip##_VER_MINOR) "." \
                            FLEET_DSI_STR(ESP_LCD_##chip##_VER_PATCH)

#ifdef __cplusplus
}
#endif

#endif // SOC_MIPI_DSI_SUPPORTED
