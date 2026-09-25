// Fleet_Display - see Fleet_Display.h. Milestone 2.9 (#67), step 2.
#if defined(DISPLAY_ESPLCD)

#include "Fleet_Display.h"
#include "fleet_dsi_panel.h"

#include <Arduino.h>            // Serial, map()
#include <string.h>
#include "driver/ledc.h"
#include "esp_attr.h"
#include "esp_cache.h"
#include "esp_lcd_mipi_dsi.h"
#include "esp_lcd_panel_ops.h"
#include <FleetI2C.h>
#include "bsp_loader.h"

namespace {

// DSI PHY power. The same values Arduino_GFX hardcodes for every P4 board
// (Arduino_ESP32DSIPanel.h:23-24) and Waveshare's P4_5 BSP uses (display.h:33-34):
// LDO_VO3 feeds VDD_MIPI_DPHY.
constexpr int DSI_PHY_LDO_CHAN = 3;
constexpr int DSI_PHY_LDO_MV   = 2500;

// Backlight: DisplayManager's LEDC timer/channel and resolution, so a board
// behaves the same on either display library.
constexpr ledc_timer_t   BL_LEDC_TIMER   = LEDC_TIMER_1;
constexpr ledc_channel_t BL_LEDC_CHANNEL = LEDC_CHANNEL_1;
constexpr uint32_t       BL_MAX_DUTY     = 1023;   // 10-bit

} // namespace

// The frame-complete interrupt. IDF calls it at the end of every scanned
// frame, having just restarted the DMA on whichever buffer is current
// (esp_lcd_panel_dpi.c:71-95, IDF 5.5.5) - so from this moment the panel is
// scanning the buffer we last handed over. IDF refuses to register it unless
// it is in IRAM (esp_lcd_panel_dpi.c:640).
struct FleetDisplayIsr {
    static bool IRAM_ATTR onFrameComplete(esp_lcd_panel_handle_t panel,
                                          esp_lcd_dpi_panel_event_data_t *edata, void *ctx) {
        (void)panel; (void)edata;
        Fleet_Display *d = static_cast<Fleet_Display *>(ctx);
        d->_scanning = d->_submitted;
        d->_frames   = d->_frames + 1;
        return false;
    }
};

bool Fleet_Display::begin() {
    Serial.println("[Fleet_Display] Begin (esp_lcd)");
    Serial.flush();

    // The shared I2C bus, as DisplayManager::begin() brings it up; touch and
    // audio expect it. Idempotent.
    FleetI2C::begin(bsp_hw.SDA_PIN, bsp_hw.SCL_PIN);

    if (strcmp(bsp_display.PANEL_MODEL, "HX8394") != 0) {
        // Step 2 brings up WS_P4_5 only. Each further panel is its own step.
        Serial.printf("[Fleet_Display] No esp_lcd driver for panel %s yet\n", bsp_display.PANEL_MODEL);
        return false;
    }

    _w = bsp_display.WIDTH;
    _h = bsp_display.HEIGHT;

    fleet_dsi_cfg_t cfg = {};
    cfg.ldo_chan           = DSI_PHY_LDO_CHAN;
    cfg.ldo_mv             = DSI_PHY_LDO_MV;
    cfg.num_lanes          = 2;
    cfg.lane_bit_rate_mbps = bsp_display.LANE_BIT_RATE;
    cfg.h_res              = _w;
    cfg.v_res              = _h;
    cfg.dpi_clock_hz       = bsp_display.PREFER_SPEED;
    cfg.hsync_pulse_width  = bsp_display.HSYNC_PWIDTH;
    cfg.hsync_back_porch   = bsp_display.HSYNC_BPORCH;
    cfg.hsync_front_porch  = bsp_display.HSYNC_FPORCH;
    cfg.vsync_pulse_width  = bsp_display.VSYNC_PWIDTH;
    cfg.vsync_back_porch   = bsp_display.VSYNC_BPORCH;
    cfg.vsync_front_porch  = bsp_display.VSYNC_FPORCH;
    cfg.reset_gpio         = bsp_display.RST;
    cfg.reset_active_high  = bsp_display.RST_ACTIVE_HIGH;
    // Our proven init sequence (the BSP's), not the driver's default.
    cfg.init_cmds          = reinterpret_cast<const fleet_dsi_init_cmd_t *>(bsp_display.INIT_CMDS_DSI);
    cfg.init_cmds_count    = bsp_display.INIT_CMDS_SIZE;
    cfg.num_fbs            = NUM_FBS;
    static_assert(sizeof(fleet_dsi_init_cmd_t) == sizeof(lcd_init_cmd_t),
                  "BSP init commands and fleet_dsi_init_cmd_t must share a layout");

    esp_err_t err = fleet_dsi_panel_new_hx8394(&cfg, &_panel, _fb);
    if (err != ESP_OK) {
        Serial.printf("[Fleet_Display] Panel bring-up failed: %s\n", esp_err_to_name(err));
        return false;
    }

    // Nothing on the CPU may hold a dirty cache line over a frame buffer: the
    // PPA writes them by DMA, and a later write-back of a stale line would
    // overwrite its work. The driver cleared them with the CPU; write that
    // back once, and drop the lines.
    for (uint8_t i = 0; i < NUM_FBS; i++) {
        esp_cache_msync(_fb[i], frameBufferBytes(),
                        ESP_CACHE_MSYNC_FLAG_DIR_C2M | ESP_CACHE_MSYNC_FLAG_INVALIDATE);
    }

    // The panel starts on buffer 0.
    _scanning = 0;
    _submitted = 0;
    esp_lcd_dpi_panel_event_callbacks_t cbs = {};
    cbs.on_frame_buf_complete = FleetDisplayIsr::onFrameComplete;
    err = esp_lcd_dpi_panel_register_event_callbacks(_panel, &cbs, this);
    if (err != ESP_OK) {
        Serial.printf("[Fleet_Display] Frame callback refused: %s\n", esp_err_to_name(err));
        return false;
    }

    initBacklightPWM();
    Serial.printf("[Fleet_Display] Ready: %ux%u, %u frame buffers of %u KB\n",
                  (unsigned)_w, (unsigned)_h, (unsigned)NUM_FBS, (unsigned)(frameBufferBytes() / 1024));
    return true;
}

// ORDER MATTERS. draw_bitmap() makes buffer i current; the interrupt copies
// _submitted into _scanning. Set _submitted AFTER the switch: if the interrupt
// lands between the two, we believe the old buffer is still being scanned
// when the panel has in fact moved to i - a belief that only ever keeps us
// off a buffer, never puts us on the one being shown.
bool Fleet_Display::present(uint8_t i) {
    if (i >= NUM_FBS || !_panel) return false;
    const esp_err_t err = esp_lcd_panel_draw_bitmap(_panel, 0, 0, _w, _h, _fb[i]);
    _submitted = i;
    return err == ESP_OK;
}

void Fleet_Display::initBacklightPWM() {
    if (bsp_display.BL_PIN < 0) return;
    if (bsp_display.BL_FREQ <= 0) {
        pinMode(bsp_display.BL_PIN, OUTPUT);
        digitalWrite(bsp_display.BL_PIN, bsp_display.BL_ON_LEVEL);
        _currentBrightness = 100;
        return;
    }
    ledc_timer_config_t timer = {};
    timer.speed_mode      = LEDC_LOW_SPEED_MODE;
    timer.duty_resolution = LEDC_TIMER_10_BIT;
    timer.timer_num       = BL_LEDC_TIMER;
    timer.freq_hz         = (uint32_t)bsp_display.BL_FREQ;
    timer.clk_cfg         = LEDC_AUTO_CLK;

    ledc_channel_config_t ch = {};
    ch.gpio_num   = bsp_display.BL_PIN;
    ch.speed_mode = LEDC_LOW_SPEED_MODE;
    ch.channel    = BL_LEDC_CHANNEL;
    ch.intr_type  = LEDC_INTR_DISABLE;
    ch.timer_sel  = BL_LEDC_TIMER;
    ch.duty       = 0;
    ch.hpoint     = 0;
    ch.flags.output_invert = (bsp_display.BL_ON_LEVEL == 0) ? 1u : 0u;

    esp_err_t err = ledc_timer_config(&timer);
    err |= ledc_channel_config(&ch);
    if (err != ESP_OK) Serial.printf("[Fleet_Display] Backlight PWM failed: %d\n", err);
    setBrightness(DEFAULT_BRIGHTNESS);
}

void Fleet_Display::setBrightness(uint8_t pct) {
    if (pct > 100) pct = 100;
    _currentBrightness = pct;
    const uint32_t duty = map(pct, 0, 100, 0, BL_MAX_DUTY);
    ledc_set_duty(LEDC_LOW_SPEED_MODE, BL_LEDC_CHANNEL, duty);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, BL_LEDC_CHANNEL);
}

void Fleet_Display::setBacklight(bool on) { setBrightness(on ? _currentBrightness : 0); }

int Fleet_Display::getBrightness() { return _currentBrightness; }

#endif // DISPLAY_ESPLCD
