// Fleet_Display - see Fleet_Display.h. Milestone 2.9 (#67): MIPI-DSI from
// step 2, RGB from step 4 (WS_S3_4B).
#if defined(DISPLAY_ESPLCD)

#include "Fleet_Display.h"

#include <Arduino.h>            // Serial, map()
#include <string.h>
#include "driver/ledc.h"
#include "esp_attr.h"
#include "esp_cache.h"
#include "esp_timer.h"
#include "esp_lcd_panel_ops.h"
#include <FleetI2C.h>
#include "bsp_loader.h"

#if defined(HAS_MIPI_PANEL)
#include "fleet_dsi_panel.h"
#include "esp_lcd_mipi_dsi.h"
#include "esp_lcd_panel_io.h"
#elif defined(HAS_RGB_PANEL)
#include "fleet_rgb_panel.h"
#include "esp_lcd_panel_rgb.h"
#include "esp_io_expander_tca9554.h"
#include "esp32-hal-i2c.h"      // i2cBusHandle(): the i2c_master bus Arduino's Wire runs on
#else
// QSPI arrives at 2.9 step 5. Said at compile time rather than as a dark panel at boot.
#error "DISPLAY_ESPLCD drives MIPI-DSI and RGB panels so far (2.9 step 5 adds QSPI) - remove it from this environment"
#endif
#ifndef BSP_PANEL_DRIVER
#error "This board's BSP header must #define BSP_PANEL_DRIVER (its panel chip, e.g. HX8394) - see Fleet_BSP.h"
#endif

// The board's panel driver, by name: BSP_PANEL_DRIVER HX8394 declares
// fleet_dsi_driver_HX8394, defined in fleet_dsi_hx8394.c (on an RGB board,
// fleet_rgb_driver_ST7701 in fleet_rgb_st7701.c). A chip with no wrapper is a
// LINK error naming it (fleet_dsi_panel.h).
#if defined(HAS_MIPI_PANEL)
extern "C" const fleet_dsi_driver_t FLEET_DSI_DRIVER(BSP_PANEL_DRIVER);
#else
extern "C" const fleet_rgb_driver_t FLEET_RGB_DRIVER(BSP_PANEL_DRIVER);
#endif

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
//
// RGB (step 4): on_frame_buf_complete - "the frame buffer can be reused
// safely" - fires when a frame has been read out of PSRAM in full (with bounce
// buffers, by the bounce ISR). A buffer handed over by present() is read from
// the next frame on, so from this moment it is the one being scanned, and the
// same bookkeeping holds.
struct FleetDisplayIsr {
#if defined(HAS_MIPI_PANEL)
    static bool IRAM_ATTR onFrameComplete(esp_lcd_panel_handle_t panel,
                                          esp_lcd_dpi_panel_event_data_t *edata, void *ctx) {
#else
    static bool IRAM_ATTR onFrameComplete(esp_lcd_panel_handle_t panel,
                                          const esp_lcd_rgb_panel_event_data_t *edata, void *ctx) {
#endif
        (void)panel; (void)edata;
        Fleet_Display *d = static_cast<Fleet_Display *>(ctx);
        d->_scanning = d->_submitted;
        d->_frames   = d->_frames + 1;
        return false;
    }
};

namespace {

// The present modes each bus has a flush for (src/LVGL_Flush_EspLcd*.cpp).
// Anything else is refused in begin(), loudly, in favour of the bus's own.
#if defined(HAS_MIPI_PANEL)
constexpr uint8_t BUILT_MODE = BSP_PRESENT_TRIPLE_PARTIAL;   // LVGL_Flush_EspLcd.cpp
#else
constexpr uint8_t BUILT_MODE = BSP_PRESENT_DOUBLE_DIRECT;    // LVGL_Flush_EspLcdDirect.cpp
#endif

#if defined(HAS_RGB_PANEL)
inline uint32_t expPin(int n) { return 1U << n; }   // expander pin NUMBER -> the API's mask
#endif

} // namespace

#if defined(HAS_RGB_PANEL)
// RGB bring-up (2.9 step 4, WS_S3_4B): the I/O expander, the panel reset on
// it, the panel (fleet_rgb_panel.c), then the two expander jobs
// DisplayManager::begin() used to do on this board - the touch controller's
// reset and the power amp's enable. The sequence is Waveshare's S3-4B BSP plus
// what Arduino_XCA9554SWSPI did on the Arduino_GFX path.
bool Fleet_Display::beginRgb() {
    const fleet_rgb_driver_t &drv = FLEET_RGB_DRIVER(BSP_PANEL_DRIVER);
    esp_io_expander_handle_t exp = nullptr;

#if defined(HAS_IO_EXPANDER)
    // The TCA9554, on the i2c_master bus Arduino's Wire already runs
    // (FleetI2C::begin() in begin() started it) - one bus, one owner, as
    // display-stack.md s7 settled.
    auto bus = static_cast<i2c_master_bus_handle_t>(i2cBusHandle(0));
    if (!bus || esp_io_expander_new_i2c_tca9554(bus, bsp_expander.I2C_ADDR, &exp) != ESP_OK) {
        Serial.printf("[Fleet_Display] I/O expander (TCA9554 @0x%02X) not found\n", bsp_expander.I2C_ADDR);
        return false;
    }
    _expander = exp;
    // Panel reset on the expander, as Arduino_XCA9554SWSPI::begin() pulsed it:
    // low 10 ms, then 100 ms to settle.
    esp_io_expander_set_dir(exp, expPin(bsp_expander.LCD_RST), IO_EXPANDER_OUTPUT);
    esp_io_expander_set_level(exp, expPin(bsp_expander.LCD_RST), 0);
    delay(10);
    esp_io_expander_set_level(exp, expPin(bsp_expander.LCD_RST), 1);
    delay(100);
#endif

    fleet_rgb_cfg_t cfg = {};
    cfg.pclk_hz           = bsp_display.PREFER_SPEED;   // what Arduino_GFX used; PCLK_HZ is unused there too
    cfg.h_res             = _w;
    cfg.v_res             = _h;
    cfg.hsync_pulse_width = bsp_display.HSYNC_PWIDTH;
    cfg.hsync_back_porch  = bsp_display.HSYNC_BPORCH;
    cfg.hsync_front_porch = bsp_display.HSYNC_FPORCH;
    cfg.vsync_pulse_width = bsp_display.VSYNC_PWIDTH;
    cfg.vsync_back_porch  = bsp_display.VSYNC_BPORCH;
    cfg.vsync_front_porch = bsp_display.VSYNC_FPORCH;
    // Arduino_ESP32RGBPanel's mapping of the same BSP fields
    // (Arduino_ESP32RGBPanel.cpp:63-67): polarity 0 = idles low.
    cfg.hsync_idle_low    = bsp_display.HSYNC_POL == 0;
    cfg.vsync_idle_low    = bsp_display.VSYNC_POL == 0;
    cfg.de_idle_high      = bsp_display.DE_IDLE_HIGH ? 1 : 0;
    cfg.pclk_active_neg   = bsp_display.PCLK_ACTIVE_NEG ? 1 : 0;
    cfg.pclk_idle_high    = bsp_display.PCLK_IDLE_HIGH ? 1 : 0;
    cfg.de_gpio           = bsp_display.DE;
    cfg.vsync_gpio        = bsp_display.VSYNC;
    cfg.hsync_gpio        = bsp_display.HSYNC;
    cfg.pclk_gpio         = bsp_display.PCLK;
    cfg.disp_gpio         = -1;
    // Bit order B, G, R - Arduino_ESP32RGBPanel's little-endian order
    // (:131-146), and Waveshare's DATA0-15 exactly (the BSP's "flipped" rows).
    const int8_t data[16] = {
        bsp_display.B0, bsp_display.B1, bsp_display.B2, bsp_display.B3, bsp_display.B4,
        bsp_display.G0, bsp_display.G1, bsp_display.G2, bsp_display.G3, bsp_display.G4, bsp_display.G5,
        bsp_display.R0, bsp_display.R1, bsp_display.R2, bsp_display.R3, bsp_display.R4,
    };
    for (int i = 0; i < 16; i++) cfg.data_gpio[i] = data[i];
    cfg.bounce_buffer_px     = bsp_display.BOUNCE_BUFFER_SIZE_PX;
    cfg.num_fbs              = _numFbs;
    cfg.panel_bits_per_pixel = 18;   // ST7701 RGB666, as Waveshare's BSP; the init list sets COLMOD 0x66 as well
    cfg.expander             = exp;
#if defined(HAS_IO_EXPANDER)
    cfg.exp_cs               = bsp_expander.LCD_CS;
    cfg.exp_scl              = bsp_expander.LCD_SCK;
    cfg.exp_sda              = bsp_expander.LCD_MOSI;
#endif
    cfg.init_cmds            = reinterpret_cast<const fleet_rgb_init_cmd_t *>(bsp_display.INIT_CMDS_ESPLCD);
    cfg.init_cmds_count      = bsp_display.INIT_CMDS_ESPLCD_COUNT;
    static_assert(sizeof(fleet_rgb_init_cmd_t) == sizeof(lcd_init_cmd_t),
                  "BSP init commands and fleet_rgb_init_cmd_t must share a layout");

    const esp_err_t err = fleet_rgb_panel_new(&cfg, &drv, &_panel, _fb);
    if (err != ESP_OK) {
        Serial.printf("[Fleet_Display] Panel bring-up failed: %s\n", esp_err_to_name(err));
        return false;
    }
    _driver  = drv.name;
    _pclkHz  = cfg.pclk_hz;
    _bounce  = (uint32_t)cfg.bounce_buffer_px;

#if defined(HAS_IO_EXPANDER)
    // The GT911's reset, which DisplayManager::resetTouch() did through the
    // same expander: INT held low through the reset pulse picks address 0x5D.
    esp_io_expander_set_dir(exp, expPin(bsp_expander.TP_RST) | expPin(bsp_expander.TP_INT), IO_EXPANDER_OUTPUT);
    esp_io_expander_set_level(exp, expPin(bsp_expander.TP_INT), 0);
    delay(20);
    esp_io_expander_set_level(exp, expPin(bsp_expander.TP_RST), 0);
    delay(20);
    esp_io_expander_set_level(exp, expPin(bsp_expander.TP_RST), 1);
    delay(200);
    // The speaker amp's enable (EXIO3), switched on as DisplayManager::begin() did.
    esp_io_expander_set_dir(exp, expPin(bsp_expander.AMP_EN), IO_EXPANDER_OUTPUT);
    esp_io_expander_set_level(exp, expPin(bsp_expander.AMP_EN), 1);
    Serial.println("[Fleet_Display] Expander: touch reset, amp enabled");
#endif
    return true;
}
#endif // HAS_RGB_PANEL

bool Fleet_Display::begin() {
    Serial.println("[Fleet_Display] Begin (esp_lcd)");
    Serial.flush();

    // The shared I2C bus, as DisplayManager::begin() brings it up; touch and
    // audio expect it. Idempotent.
    FleetI2C::begin(bsp_hw.SDA_PIN, bsp_hw.SCL_PIN);

    // How frames reach the panel: the BSP's override, else the rule
    // (bsp_loader.h). Each bus has one flush built (BUILT_MODE above);
    // anything else is refused here, loudly, so the board still lights up.
    _modeFromBsp   = bspPresentModeOverridden();
    _modeRequested = bspPresentMode();
    if (!_modeFromBsp) bspPresentModeRule(&_modeReason);
    _mode = _modeRequested;
    if (_mode != BUILT_MODE) {
        Serial.printf("[Fleet_Display] WARNING: present mode %s is not built for this bus - running %s\n",
                      bspPresentModeName(_modeRequested), bspPresentModeName(BUILT_MODE));
        _mode = BUILT_MODE;
    }
    _numFbs = bspPresentFrameBuffers(_mode);

    _w = bsp_display.WIDTH;
    _h = bsp_display.HEIGHT;
    esp_err_t err = ESP_OK;

#if defined(HAS_MIPI_PANEL)
    // The panel driver comes from the BSP by name (above). HX8394 (WS_P4_5),
    // ST7703 (WS_P4_4B), EK79007 (WS_P4_7B) and JD9165 (CYD_P4_1060) all run.
    const fleet_dsi_driver_t &drv = FLEET_DSI_DRIVER(BSP_PANEL_DRIVER);

    // PHY power and lane count: the BSP's where it sets them (the 7B and
    // CYD_P4_1060 headers do), today's values where it does not - every board
    // so far agrees on LDO 3 at 2500 mV and two lanes.
    fleet_dsi_cfg_t cfg = {};
    cfg.ldo_chan           = bsp_display.TEST_MIPI_DSI_PHY_PWR_LDO_CHAN ? bsp_display.TEST_MIPI_DSI_PHY_PWR_LDO_CHAN
                                                                        : DSI_PHY_LDO_CHAN;
    cfg.ldo_mv             = bsp_display.TEST_MIPI_DSI_PHY_PWR_LDO_VOLTAGE_MV ? bsp_display.TEST_MIPI_DSI_PHY_PWR_LDO_VOLTAGE_MV
                                                                              : DSI_PHY_LDO_MV;
    cfg.num_lanes          = bsp_display.NUM_DSI_LANES ? bsp_display.NUM_DSI_LANES : 2;
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
    cfg.num_fbs            = _numFbs;
    static_assert(sizeof(fleet_dsi_init_cmd_t) == sizeof(lcd_init_cmd_t),
                  "BSP init commands and fleet_dsi_init_cmd_t must share a layout");

    err = fleet_dsi_panel_new(&cfg, &drv, &_panel, &_io, _fb);
    if (err != ESP_OK) {
        Serial.printf("[Fleet_Display] Panel bring-up failed: %s\n", esp_err_to_name(err));
        return false;
    }
    _driver   = drv.name;
    _lanes    = (uint8_t)cfg.num_lanes;
    _laneMbps = cfg.lane_bit_rate_mbps;
    _pclkHz   = cfg.dpi_clock_hz;
#else
    if (!beginRgb()) return false;
#endif

    // Rotation is the flush's job (the PPA). See softwareRotation() in the
    // header for why no panel does it itself.
    _swRot = (uint8_t)(bsp_display.ROTATION & 3);

    // Nothing on the CPU may hold a dirty cache line over a frame buffer: the
    // PPA writes them by DMA, and a later write-back of a stale line would
    // overwrite its work. The driver cleared them with the CPU; write that
    // back once, and drop the lines.
    for (uint8_t i = 0; i < _numFbs; i++) {
        esp_cache_msync(_fb[i], frameBufferBytes(),
                        ESP_CACHE_MSYNC_FLAG_DIR_C2M | ESP_CACHE_MSYNC_FLAG_INVALIDATE);
    }

    // The panel starts on buffer 0.
    _scanning = 0;
    _submitted = 0;
#if defined(HAS_MIPI_PANEL)
    esp_lcd_dpi_panel_event_callbacks_t cbs = {};
    cbs.on_frame_buf_complete = FleetDisplayIsr::onFrameComplete;
    err = esp_lcd_dpi_panel_register_event_callbacks(_panel, &cbs, this);
#else
    esp_lcd_rgb_panel_event_callbacks_t cbs = {};
    cbs.on_frame_buf_complete = FleetDisplayIsr::onFrameComplete;
    err = esp_lcd_rgb_panel_register_event_callbacks(_panel, &cbs, this);
#endif
    if (err != ESP_OK) {
        Serial.printf("[Fleet_Display] Frame callback refused: %s\n", esp_err_to_name(err));
        return false;
    }
    _frames      = 0;
    _scanStartUs = esp_timer_get_time();   // counting starts here

    initBacklightPWM();
    Serial.printf("[Fleet_Display] Ready: %ux%u, %u frame buffers of %u KB, driver %s\n",
                  (unsigned)_w, (unsigned)_h, (unsigned)_numFbs, (unsigned)(frameBufferBytes() / 1024),
                  _driver);
    Serial.printf("[Fleet_Display] Present mode: %s (%s%s)\n", bspPresentModeName(_mode),
                  _modeFromBsp ? "BSP override" : "rule: ", _modeFromBsp ? "" : _modeReason);
#if defined(HAS_MIPI_PANEL)
    // The panel's own status as it starts: the healthy reading GET /panel
    // compares with (PanelDebug.h).
    _bootStatusOk = readPanelStatus(_bootStatus);
    if (_bootStatusOk)
        Serial.printf("[Fleet_Display] Panel status: power %02X, MADCTL %02X, pixel format %02X, image mode %02X\n",
                      _bootStatus[0], _bootStatus[1], _bootStatus[2], _bootStatus[3]);
#endif
    return true;
}

// ORDER MATTERS. draw_bitmap() makes buffer i current; the interrupt copies
// _submitted into _scanning. Set _submitted AFTER the switch: if the interrupt
// lands between the two, we believe the old buffer is still being scanned
// when the panel has in fact moved to i - a belief that only ever keeps us
// off a buffer, never puts us on the one being shown.
bool Fleet_Display::present(uint8_t i) {
    if (i >= _numFbs || !_panel) return false;
    const esp_err_t err = esp_lcd_panel_draw_bitmap(_panel, 0, 0, _w, _h, _fb[i]);
    _submitted = i;
    return err == ESP_OK;
}

bool Fleet_Display::readPanelStatus(uint8_t out[4]) {
#if defined(HAS_MIPI_PANEL)
    if (!_io) return false;
    static const int REGS[4] = { 0x0A, 0x0B, 0x0C, 0x0D };   // DCS: power, MADCTL, pixel format, image mode
    for (uint8_t i = 0; i < 4; i++) {
        out[i] = 0;
        if (esp_lcd_panel_io_rx_param(_io, REGS[i], &out[i], 1) != ESP_OK) return false;
    }
    return true;
#else
    (void)out;
    return false;
#endif
}

int Fleet_Display::resendPanelInit() {
#if defined(HAS_MIPI_PANEL)
    const lcd_init_cmd_t *cmds = bsp_display.INIT_CMDS_DSI;
    const size_t n = bsp_display.INIT_CMDS_SIZE;
    if (!_io || !cmds || !n) return -1;
    for (size_t i = 0; i < n; i++) {
        if (esp_lcd_panel_io_tx_param(_io, cmds[i].cmd, cmds[i].data, cmds[i].data_bytes) != ESP_OK) return -1;
        if (cmds[i].delay_ms) vTaskDelay(pdMS_TO_TICKS(cmds[i].delay_ms));
    }
    return (int)n;
#else
    return -1;
#endif
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
