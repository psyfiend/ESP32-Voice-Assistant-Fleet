/*
 * SPDX-FileCopyrightText: 2023-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "soc/soc_caps.h"

/* ---- FLEET LOCAL CHANGES (components/Fleet_Display/README.md) --------------
 * Vendored from espressif/esp_lcd_st7701 2.0.2 (Apache-2.0,
 * LICENSE_esp_lcd_st7701.txt): esp_lcd_st7701.{c,h}, _mipi.c, _rgb.c and
 * esp_lcd_st7701_interface.h (upstream's priv_include/, flattened). Two
 * changes, both in this file and _rgb.c only:
 *
 * 1. The version macros IDF's component build would inject come from
 *    fleet_display_versions.h, because PlatformIO does not inject them.
 * 2. This file and _rgb.c compile on MIPI-DSI targets (the P4), and on boards
 *    on the esp_lcd RGB path (WS_S3_4B, 2.9 step 4) - so the S3 boards still
 *    on Arduino_GFX compile nothing of this driver (every Fleet_Display file
 *    is empty on boards that do not use it).
 *
 * No board-level I2C in this driver.
 * ------------------------------------------------------------------------- */
#if SOC_MIPI_DSI_SUPPORTED || (defined(DISPLAY_ESPLCD) && defined(HAS_RGB_PANEL))
#include "fleet_display_versions.h"

#include "esp_check.h"
#include "esp_lcd_types.h"

#include "esp_lcd_st7701_interface.h"
#include "esp_lcd_st7701.h"

static const char *TAG = "st7701";

esp_err_t esp_lcd_new_panel_st7701(const esp_lcd_panel_io_handle_t io, const esp_lcd_panel_dev_config_t *panel_dev_config,
                                   esp_lcd_panel_handle_t *ret_panel)
{
    ESP_LOGI(TAG, "version: %d.%d.%d", ESP_LCD_ST7701_VER_MAJOR, ESP_LCD_ST7701_VER_MINOR, ESP_LCD_ST7701_VER_PATCH);
    ESP_RETURN_ON_FALSE(panel_dev_config && ret_panel, ESP_ERR_INVALID_ARG, TAG, "Invalid arguments");
    st7701_vendor_config_t *vendor_config = (st7701_vendor_config_t *)panel_dev_config->vendor_config;
    ESP_RETURN_ON_FALSE(vendor_config, ESP_ERR_INVALID_ARG, TAG, "`vendor_config` is necessary");

    esp_err_t ret = ESP_ERR_NOT_SUPPORTED;

#if SOC_LCD_RGB_SUPPORTED
    if (!vendor_config->flags.use_mipi_interface) {
        ret = esp_lcd_new_panel_st7701_rgb(io, panel_dev_config, ret_panel);
    }
#endif

#if SOC_MIPI_DSI_SUPPORTED
    if (vendor_config->flags.use_mipi_interface) {
        ret = esp_lcd_new_panel_st7701_mipi(io, panel_dev_config, ret_panel);
    }
#endif

    return ret;
}

#endif // SOC_MIPI_DSI_SUPPORTED || esp_lcd RGB (FLEET LOCAL CHANGE 2)
