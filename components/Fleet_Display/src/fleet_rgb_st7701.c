// fleet_rgb_st7701 - the ST7701 driver (RGB half) as fleet_rgb_panel_new()
// sees it. WS_S3_4B (2.9 step 4). Reached through BSP_PANEL_DRIVER = ST7701
// on an RGB board; see the fleet_rgb_driver_t comment in fleet_rgb_panel.h.
//
// The chip's commands go over its own 3-wire SPI (9-bit, a D/C bit before
// every byte), bit-banged through the I/O expander's pins - what
// Arduino_XCA9554SWSPI did on the Arduino_GFX path. As in Waveshare's BSP the
// command channel is deleted once the init sequence is sent
// (auto_del_panel_io), which hands the expander's three pins back.
#include "fleet_rgb_panel.h"
#if defined(DISPLAY_ESPLCD) && defined(HAS_RGB_PANEL)

#include <stdlib.h>
#include "esp_check.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_io_additions.h"
#include "esp_lcd_st7701.h"
#include "fleet_display_versions.h"

static const char *TAG = "fleet_rgb_st7701";

static esp_err_t create(const fleet_rgb_cfg_t *cfg, const esp_lcd_rgb_panel_config_t *rgb,
                        esp_lcd_panel_handle_t *panel)
{
    ESP_RETURN_ON_FALSE(cfg->expander, ESP_ERR_INVALID_ARG, TAG, "no I/O expander for the 3-wire SPI");
    const spi_line_config_t line = {
        .cs_io_type       = IO_TYPE_EXPANDER,
        .cs_expander_pin  = (esp_io_expander_pin_num_t)(1U << cfg->exp_cs),
        .scl_io_type      = IO_TYPE_EXPANDER,
        .scl_expander_pin = (esp_io_expander_pin_num_t)(1U << cfg->exp_scl),
        .sda_io_type      = IO_TYPE_EXPANDER,
        .sda_expander_pin = (esp_io_expander_pin_num_t)(1U << cfg->exp_sda),
        .io_expander      = cfg->expander,
    };
    const esp_lcd_panel_io_3wire_spi_config_t io_cfg = ST7701_PANEL_IO_3WIRE_SPI_CONFIG(line, 0);
    esp_lcd_panel_io_handle_t io = NULL;
    ESP_RETURN_ON_ERROR(esp_lcd_new_panel_io_3wire_spi(&io_cfg, &io), TAG, "3-wire SPI panel IO");

    st7701_lcd_init_cmd_t *cmds;
    FLEET_RGB_COPY_CMDS(st7701_lcd_init_cmd_t, cfg, cmds);
    const st7701_vendor_config_t vendor = {
        .init_cmds      = cmds,
        .init_cmds_size = (uint16_t)(cmds ? cfg->init_cmds_count : 0),
        .rgb_config     = rgb,
        .flags = {
            .use_mipi_interface = 0,
            .mirror_by_cmd      = 0,
            .auto_del_panel_io  = 1,   // init now, over SPI, then free the lines
        },
    };
    const esp_lcd_panel_dev_config_t dev = {
        .reset_gpio_num = -1,          // reset is an expander pin, pulsed by Fleet_Display
        .rgb_ele_order  = LCD_RGB_ELEMENT_ORDER_RGB,
        .bits_per_pixel = (uint32_t)cfg->panel_bits_per_pixel,
        .vendor_config  = (void *)&vendor,
    };
    // With auto_del_panel_io the driver sends the init sequence and deletes
    // the IO inside this call, before it creates the RGB panel.
    esp_err_t ret = esp_lcd_new_panel_st7701(io, &dev, panel);
    if (ret == ESP_OK) ret = esp_lcd_panel_init(*panel);   // starts the RGB scan
    free(cmds);
    return ret;
}

const fleet_rgb_driver_t fleet_rgb_driver_ST7701 = {
    .name   = "esp_lcd_st7701 " FLEET_RGB_VER(ST7701) " (espressif, RGB)",
    .create = create,
};

#endif // DISPLAY_ESPLCD && HAS_RGB_PANEL
