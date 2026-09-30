// fleet_rgb_panel - see fleet_rgb_panel.h. The sequence is Waveshare's own
// S3-4B BSP (bsp_display_new(), waveshare/esp32_s3_touch_lcd_4b 2.0.0,
// esp32_s3_touch_lcd_4b.c:391-489), with our BSP's timings, pins and init
// commands in place of theirs. The expander, the panel reset and the touch
// reset happen in Fleet_Display.cpp, which owns the expander.
#include "fleet_rgb_panel.h"
#if defined(DISPLAY_ESPLCD) && defined(HAS_RGB_PANEL)

#include "esp_check.h"
#include "esp_log.h"
#include "esp_lcd_panel_ops.h"

static const char *TAG = "fleet_rgb";

esp_err_t fleet_rgb_panel_new(const fleet_rgb_cfg_t *cfg, const fleet_rgb_driver_t *drv,
                              esp_lcd_panel_handle_t *ret_panel, void **fbs)
{
    ESP_RETURN_ON_FALSE(cfg && drv && drv->create && ret_panel && fbs, ESP_ERR_INVALID_ARG, TAG,
                        "invalid argument");
    ESP_RETURN_ON_FALSE(cfg->num_fbs >= 1 && cfg->num_fbs <= 3, ESP_ERR_INVALID_ARG, TAG,
                        "num_fbs must be 1-3");

    // The RGB link. 16 data lines, RGB565 frame buffers in PSRAM; the bounce
    // buffers (internal RAM) are what the DMA actually reads, refilled from
    // PSRAM by the CPU - the S3's defence against PSRAM bandwidth dips.
    esp_lcd_rgb_panel_config_t rgb = {
        .clk_src = LCD_CLK_SRC_DEFAULT,
        .timings = {
            .pclk_hz           = cfg->pclk_hz,
            .h_res             = (uint32_t)cfg->h_res,
            .v_res             = (uint32_t)cfg->v_res,
            .hsync_pulse_width = (uint32_t)cfg->hsync_pulse_width,
            .hsync_back_porch  = (uint32_t)cfg->hsync_back_porch,
            .hsync_front_porch = (uint32_t)cfg->hsync_front_porch,
            .vsync_pulse_width = (uint32_t)cfg->vsync_pulse_width,
            .vsync_back_porch  = (uint32_t)cfg->vsync_back_porch,
            .vsync_front_porch = (uint32_t)cfg->vsync_front_porch,
            .flags = {
                .hsync_idle_low  = cfg->hsync_idle_low ? 1 : 0,
                .vsync_idle_low  = cfg->vsync_idle_low ? 1 : 0,
                .de_idle_high    = cfg->de_idle_high ? 1 : 0,
                .pclk_active_neg = cfg->pclk_active_neg ? 1 : 0,
                .pclk_idle_high  = cfg->pclk_idle_high ? 1 : 0,
            },
        },
        .data_width            = 16,
        .bits_per_pixel        = 16,
        .num_fbs               = (size_t)cfg->num_fbs,
        .bounce_buffer_size_px = cfg->bounce_buffer_px,
        .dma_burst_size        = 64,
        .hsync_gpio_num        = cfg->hsync_gpio,
        .vsync_gpio_num        = cfg->vsync_gpio,
        .de_gpio_num           = cfg->de_gpio,
        .pclk_gpio_num         = cfg->pclk_gpio,
        .disp_gpio_num         = cfg->disp_gpio,
        .flags = {
            .fb_in_psram = 1,
        },
    };
    for (int i = 0; i < 16; i++) rgb.data_gpio_nums[i] = cfg->data_gpio[i];

    esp_lcd_panel_handle_t panel = NULL;
    esp_err_t ret = drv->create(cfg, &rgb, &panel);
    ESP_RETURN_ON_ERROR(ret, TAG, "%s panel", drv->name);

    switch (cfg->num_fbs) {
    case 1:  ret = esp_lcd_rgb_panel_get_frame_buffer(panel, 1, &fbs[0]); break;
    case 2:  ret = esp_lcd_rgb_panel_get_frame_buffer(panel, 2, &fbs[0], &fbs[1]); break;
    default: ret = esp_lcd_rgb_panel_get_frame_buffer(panel, 3, &fbs[0], &fbs[1], &fbs[2]); break;
    }
    if (ret != ESP_OK) {
        esp_lcd_panel_del(panel);
        ESP_RETURN_ON_ERROR(ret, TAG, "frame buffers");
    }

    *ret_panel = panel;
    ESP_LOGI(TAG, "%s up: %dx%d, %.1f MHz, bounce %u px, %d frame buffers", drv->name,
             cfg->h_res, cfg->v_res, (double)cfg->pclk_hz / 1e6, (unsigned)cfg->bounce_buffer_px,
             cfg->num_fbs);
    return ESP_OK;
}

#endif // DISPLAY_ESPLCD && HAS_RGB_PANEL
