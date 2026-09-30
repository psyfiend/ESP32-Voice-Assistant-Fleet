// fleet_dsi_st7701 - the ST7701 driver (MIPI-DSI half) as fleet_dsi_panel_new()
// sees it. CYD_P4_4880 (Guition JC4880P443) - vendored and compiles, NEVER
// RUN. Reached through BSP_PANEL_DRIVER = ST7701; see the fleet_dsi_driver_t
// comment in fleet_dsi_panel.h.
//
// The same chip drives WS_S3_4B over RGB; that half of the driver is not
// reached from here (2.9 step 4).
#include "fleet_dsi_panel.h"
#if SOC_MIPI_DSI_SUPPORTED

#include <stdlib.h>
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_st7701.h"
#include "fleet_display_versions.h"

static esp_err_t create(const fleet_dsi_cfg_t *cfg, esp_lcd_dsi_bus_handle_t bus,
                        esp_lcd_panel_io_handle_t io, const esp_lcd_dpi_panel_config_t *dpi,
                        const esp_lcd_panel_dev_config_t *dev_base, esp_lcd_panel_handle_t *panel)
{
    st7701_lcd_init_cmd_t *cmds;
    FLEET_DSI_COPY_CMDS(st7701_lcd_init_cmd_t, cfg, cmds);
    const st7701_vendor_config_t vendor = {
        .init_cmds = cmds,
        .init_cmds_size = (uint16_t)(cmds ? cfg->init_cmds_count : 0),
        .mipi_config = { .dsi_bus = bus, .dpi_config = dpi },   // no lane count in this driver
        .flags = { .use_mipi_interface = 1 },                    // the driver's default is RGB
    };
    esp_lcd_panel_dev_config_t dev = *dev_base;
    dev.vendor_config = (void *)&vendor;
    esp_err_t ret = esp_lcd_new_panel_st7701(io, &dev, panel);
    if (ret == ESP_OK) ret = esp_lcd_panel_reset(*panel);
    if (ret == ESP_OK) ret = esp_lcd_panel_init(*panel);
    free(cmds);
    return ret;
}

const fleet_dsi_driver_t fleet_dsi_driver_ST7701 = {
    .name   = "esp_lcd_st7701 " FLEET_DSI_VER(ST7701) " (espressif)",
    .create = create,
};

#endif // SOC_MIPI_DSI_SUPPORTED
