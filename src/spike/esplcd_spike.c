// SPIKE, branch spike/67-esplcd-compile only - never merged.
//
// Question it answers: does everything 2.9 step 2 needs from ESP-IDF compile
// AND link inside our PlatformIO + Arduino build for WS_P4_5? It calls each
// API once, in the order docs/design/esplcd-step2.md uses them. It is linked
// into the firmware but guarded by a condition that is never true at runtime
// (main.cpp), so the linker must resolve every symbol and nothing ever runs.
//
// Written in C on purpose: the vendor's config macros use designated
// initialisers out of declaration order, which C accepts and C++ rejects.
#if defined(DISPLAY_ESPLCD_SPIKE)

#include "esp_err.h"
#include "esp_cache.h"
#include "esp_ldo_regulator.h"
#include "esp_lcd_mipi_dsi.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "driver/ppa.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "esp_attr.h"
#include "esp_lcd_hx8394.h"

static SemaphoreHandle_t s_fb_free;

// IDF 5.5.5 refuses to register this unless it is in IRAM (esp_lcd_panel_dpi.c:640).
static IRAM_ATTR bool on_fb_complete(esp_lcd_panel_handle_t panel,
                                     esp_lcd_dpi_panel_event_data_t *edata, void *ctx)
{
    (void)panel; (void)edata; (void)ctx;
    BaseType_t woken = pdFALSE;
    xSemaphoreGiveFromISR(s_fb_free, &woken);
    return woken == pdTRUE;
}

esp_err_t esplcd_spike_bringup(void)
{
    s_fb_free = xSemaphoreCreateBinary();

    // 1. DSI PHY power: LDO channel 3 at 2500 mV (Waveshare BSP display.h:33-34).
    esp_ldo_channel_handle_t ldo = NULL;
    esp_ldo_channel_config_t ldo_cfg = { .chan_id = 3, .voltage_mv = 2500 };
    ESP_ERROR_CHECK(esp_ldo_acquire_channel(&ldo_cfg, &ldo));

    // 2. DSI bus, 2 lanes at 700 Mbps, PHY clock chosen by IDF (0).
    esp_lcd_dsi_bus_handle_t bus = NULL;
    esp_lcd_dsi_bus_config_t bus_cfg = {
        .bus_id = 0, .num_data_lanes = 2, .phy_clk_src = 0, .lane_bit_rate_mbps = 700,
    };
    ESP_ERROR_CHECK(esp_lcd_new_dsi_bus(&bus_cfg, &bus));

    // 3. DBI command IO.
    esp_lcd_panel_io_handle_t io = NULL;
    esp_lcd_dbi_io_config_t dbi_cfg = { .virtual_channel = 0, .lcd_cmd_bits = 8, .lcd_param_bits = 8 };
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_dbi(bus, &dbi_cfg, &io));

    // 4. Panel: vendor timing macro, three framebuffers, DMA2D on.
    esp_lcd_dpi_panel_config_t dpi = HX8394_720_1280_PANEL_30HZ_DPI_CONFIG(LCD_COLOR_PIXEL_FORMAT_RGB565);
    dpi.num_fbs = 3;
    hx8394_vendor_config_t vendor = {
        .init_cmds = NULL, .init_cmds_size = 0,
        .mipi_config = { .dsi_bus = bus, .dpi_config = &dpi, .lane_num = 2 },
    };
    esp_lcd_panel_dev_config_t dev = {
        .reset_gpio_num = 27, .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB,
        .bits_per_pixel = 16, .vendor_config = &vendor, .flags.reset_active_high = 1,
    };
    esp_lcd_panel_handle_t panel = NULL;
    ESP_ERROR_CHECK(esp_lcd_new_panel_hx8394(io, &dev, &panel));
    ESP_ERROR_CHECK(esp_lcd_panel_reset(panel));
    ESP_ERROR_CHECK(esp_lcd_panel_init(panel));

    // 5. The three framebuffers, and the "buffer is free again" callback.
    void *fb[3] = { NULL, NULL, NULL };
    ESP_ERROR_CHECK(esp_lcd_dpi_panel_get_frame_buffer(panel, 3, &fb[0], &fb[1], &fb[2]));
    esp_lcd_dpi_panel_event_callbacks_t cbs = { .on_frame_buf_complete = on_fb_complete };
    ESP_ERROR_CHECK(esp_lcd_dpi_panel_register_event_callbacks(panel, &cbs, NULL));

    // 6. PPA: rotate a 1280x50 landscape chunk into the portrait back buffer.
    ppa_client_handle_t ppa = NULL;
    ppa_client_config_t ppa_cfg = { .oper_type = PPA_OPERATION_SRM, .max_pending_trans_num = 1 };
    ESP_ERROR_CHECK(ppa_register_client(&ppa_cfg, &ppa));
    ppa_srm_oper_config_t op = {
        .in.buffer = fb[0], .in.pic_w = 1280, .in.pic_h = 50,
        .in.block_w = 1280, .in.block_h = 50, .in.srm_cm = PPA_SRM_COLOR_MODE_RGB565,
        .out.buffer = fb[1], .out.buffer_size = 720 * 1280 * 2,
        .out.pic_w = 720, .out.pic_h = 1280, .out.srm_cm = PPA_SRM_COLOR_MODE_RGB565,
        .rotation_angle = PPA_SRM_ROTATION_ANGLE_270,
        .scale_x = 1.0f, .scale_y = 1.0f, .mode = PPA_TRANS_MODE_BLOCKING,
    };
    ESP_ERROR_CHECK(esp_cache_msync(fb[0], 1280 * 50 * 2, ESP_CACHE_MSYNC_FLAG_DIR_C2M));
    ESP_ERROR_CHECK(ppa_do_scale_rotate_mirror(ppa, &op));

    // 7. Hand the finished buffer to the panel (switches, no copy), wait for the old one.
    ESP_ERROR_CHECK(esp_lcd_panel_draw_bitmap(panel, 0, 0, 720, 1280, fb[1]));
    xSemaphoreTake(s_fb_free, portMAX_DELAY);
    return ESP_OK;
}

#endif // DISPLAY_ESPLCD_SPIKE
