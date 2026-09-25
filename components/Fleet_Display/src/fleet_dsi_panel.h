#pragma once
//
// fleet_dsi_panel - MIPI-DSI panel bring-up on raw esp_lcd. Milestone 2.9,
// step 2: docs/design/esplcd-step2.md.
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
    // Reset line and its polarity (1 = assert HIGH; the HX8394's case).
    int      reset_gpio;
    int      reset_active_high;
    // The panel's init sequence; NULL = the driver's own default.
    const fleet_dsi_init_cmd_t *init_cmds;
    size_t   init_cmds_count;
    // Frame buffers the panel allocates and owns (1-3).
    int      num_fbs;
} fleet_dsi_cfg_t;

// Powers the PHY, creates the DSI bus, the command IO and an HX8394 panel,
// resets and initialises it, and returns the panel with its frame buffers
// (fbs[0 .. num_fbs-1]). On failure, everything created so far is released.
esp_err_t fleet_dsi_panel_new_hx8394(const fleet_dsi_cfg_t *cfg,
                                     esp_lcd_panel_handle_t *ret_panel,
                                     void **fbs);

#ifdef __cplusplus
}
#endif

#endif // SOC_MIPI_DSI_SUPPORTED
