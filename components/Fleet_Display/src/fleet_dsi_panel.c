// fleet_dsi_panel - see fleet_dsi_panel.h. The sequence is Waveshare's own
// P4_5 BSP (bsp_display_new_with_handles(), waveshare/esp32_p4_wifi6_touch_lcd_5
// 1.0.4, esp32_p4_wifi6_touch_lcd_5.c:427-505), with our BSP's values and init
// commands in place of theirs. docs/research/waveshare-esp-lcd-survey.md §1.
// Every chip goes through the same steps; only the driver's constructor
// differs, and that lives in its wrapper (fleet_dsi_<chip>.c).
#include "fleet_dsi_panel.h"
#if SOC_MIPI_DSI_SUPPORTED

#include <stdlib.h>
#include "esp_check.h"
#include "esp_log.h"
#include "esp_ldo_regulator.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"

static const char *TAG = "fleet_dsi";

esp_err_t fleet_dsi_panel_new(const fleet_dsi_cfg_t *cfg, const fleet_dsi_driver_t *drv,
                              esp_lcd_panel_handle_t *ret_panel, void **fbs)
{
    ESP_RETURN_ON_FALSE(cfg && drv && drv->create && ret_panel && fbs, ESP_ERR_INVALID_ARG, TAG,
                        "invalid argument");
    ESP_RETURN_ON_FALSE(cfg->num_fbs >= 1 && cfg->num_fbs <= 3, ESP_ERR_INVALID_ARG, TAG,
                        "num_fbs must be 1-3");
    const char *name = drv->name;

    esp_err_t ret = ESP_OK;
    esp_ldo_channel_handle_t  ldo   = NULL;
    esp_lcd_dsi_bus_handle_t  bus   = NULL;
    esp_lcd_panel_io_handle_t io    = NULL;
    esp_lcd_panel_handle_t    panel = NULL;

    // 1. DSI PHY power: the PHY goes from "no power" to "shutdown" once its
    //    LDO is on. Arduino_GFX did this inside Arduino_ESP32DSIPanel; on this
    //    path nobody else will.
    const esp_ldo_channel_config_t ldo_cfg = {
        .chan_id = cfg->ldo_chan, .voltage_mv = cfg->ldo_mv,
    };
    ESP_GOTO_ON_ERROR(esp_ldo_acquire_channel(&ldo_cfg, &ldo), err, TAG, "DSI PHY LDO");

    // 2. The bus. phy_clk_src 0 = let IDF choose for this silicon revision,
    //    as Waveshare's BSP does.
    const esp_lcd_dsi_bus_config_t bus_cfg = {
        .bus_id = 0,
        .num_data_lanes = cfg->num_lanes,
        .phy_clk_src = 0,
        .lane_bit_rate_mbps = cfg->lane_bit_rate_mbps,
    };
    ESP_GOTO_ON_ERROR(esp_lcd_new_dsi_bus(&bus_cfg, &bus), err, TAG, "DSI bus");

    // 3. Command IO (DBI), for the init sequence.
    const esp_lcd_dbi_io_config_t dbi_cfg = {
        .virtual_channel = 0, .lcd_cmd_bits = 8, .lcd_param_bits = 8,
    };
    ESP_GOTO_ON_ERROR(esp_lcd_new_panel_io_dbi(bus, &dbi_cfg, &io), err, TAG, "DBI IO");

    // 4. Video. use_dma2d as the vendor config sets it: the panel's own
    //    draw_bitmap copies by DMA instead of CPU when it has to copy.
    esp_lcd_dpi_panel_config_t dpi = {
        .virtual_channel = 0,
        .dpi_clk_src = MIPI_DSI_DPI_CLK_SRC_DEFAULT,
        .dpi_clock_freq_mhz = (float)cfg->dpi_clock_hz / 1000000.0f,
        .pixel_format = LCD_COLOR_PIXEL_FORMAT_RGB565,
        .num_fbs = (uint8_t)cfg->num_fbs,
        .video_timing = {
            .h_size = cfg->h_res,
            .v_size = cfg->v_res,
            .hsync_pulse_width = cfg->hsync_pulse_width,
            .hsync_back_porch  = cfg->hsync_back_porch,
            .hsync_front_porch = cfg->hsync_front_porch,
            .vsync_pulse_width = cfg->vsync_pulse_width,
            .vsync_back_porch  = cfg->vsync_back_porch,
            .vsync_front_porch = cfg->vsync_front_porch,
        },
        .flags.use_dma2d = 1,
    };

    // 5. The panel, through its wrapper: fed our BSP's init sequence
    //    (converted to the driver's type), then reset and initialised.
    const esp_lcd_panel_dev_config_t dev = {
        .reset_gpio_num = cfg->reset_gpio,
        .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB,
        .bits_per_pixel = 16,
        .vendor_config = NULL,   // each wrapper sets its own
        .flags.reset_active_high = cfg->reset_active_high ? 1 : 0,
    };
    ret = drv->create(cfg, bus, io, &dpi, &dev, &panel);
    ESP_GOTO_ON_ERROR(ret, err, TAG, "%s panel", name);

    // 6. The frame buffers the panel allocated.
    switch (cfg->num_fbs) {
    case 1:  ret = esp_lcd_dpi_panel_get_frame_buffer(panel, 1, &fbs[0]); break;
    case 2:  ret = esp_lcd_dpi_panel_get_frame_buffer(panel, 2, &fbs[0], &fbs[1]); break;
    default: ret = esp_lcd_dpi_panel_get_frame_buffer(panel, 3, &fbs[0], &fbs[1], &fbs[2]); break;
    }
    ESP_GOTO_ON_ERROR(ret, err, TAG, "frame buffers");

    *ret_panel = panel;
    ESP_LOGI(TAG, "%s up: %dx%d, %d lanes @ %lu Mbps, %.1f MHz, %d frame buffers",
             name, cfg->h_res, cfg->v_res, cfg->num_lanes, (unsigned long)cfg->lane_bit_rate_mbps,
             (double)dpi.dpi_clock_freq_mhz, cfg->num_fbs);
    return ESP_OK;

err:
    if (panel) esp_lcd_panel_del(panel);
    if (io)    esp_lcd_panel_io_del(io);
    if (bus)   esp_lcd_del_dsi_bus(bus);
    if (ldo)   esp_ldo_release_channel(ldo);
    return ret;
}

#endif // SOC_MIPI_DSI_SUPPORTED
