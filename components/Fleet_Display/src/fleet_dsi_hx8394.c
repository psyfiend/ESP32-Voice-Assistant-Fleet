// fleet_dsi_hx8394 - the HX8394 driver as fleet_dsi_panel_new() sees it.
// WS_P4_5 (2.9 step 2). Reached through BSP_PANEL_DRIVER = HX8394; see the
// fleet_dsi_driver_t comment in fleet_dsi_panel.h.
#include "fleet_dsi_panel.h"
#if SOC_MIPI_DSI_SUPPORTED

#include <stdlib.h>
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_hx8394.h"
#include "fleet_display_versions.h"

static esp_err_t create(const fleet_dsi_cfg_t *cfg, esp_lcd_dsi_bus_handle_t bus,
                        esp_lcd_panel_io_handle_t io, const esp_lcd_dpi_panel_config_t *dpi,
                        const esp_lcd_panel_dev_config_t *dev_base, esp_lcd_panel_handle_t *panel)
{
    hx8394_lcd_init_cmd_t *cmds;
    FLEET_DSI_COPY_CMDS(hx8394_lcd_init_cmd_t, cfg, cmds);
    const hx8394_vendor_config_t vendor = {
        .init_cmds = cmds,
        .init_cmds_size = (uint16_t)(cmds ? cfg->init_cmds_count : 0),
        .mipi_config = { .dsi_bus = bus, .dpi_config = dpi, .lane_num = (uint8_t)cfg->num_lanes },
    };
    esp_lcd_panel_dev_config_t dev = *dev_base;
    dev.vendor_config = (void *)&vendor;
    esp_err_t ret = esp_lcd_new_panel_hx8394(io, &dev, panel);
    if (ret == ESP_OK) ret = esp_lcd_panel_reset(*panel);
    if (ret == ESP_OK) ret = esp_lcd_panel_init(*panel);
    free(cmds);
    return ret;
}

const fleet_dsi_driver_t fleet_dsi_driver_HX8394 = {
    .name   = "esp_lcd_hx8394 " FLEET_DSI_VER(HX8394) " (waveshare)",
    .create = create,
};

#endif // SOC_MIPI_DSI_SUPPORTED
