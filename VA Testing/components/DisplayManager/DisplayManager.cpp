#include <Arduino_GFX_Library.h>
#include "driver/ledc.h"
#include "DisplayManager.h"
#include <FleetI2C.h>

#define BL_PWM_RES 10
#define BL_MAX_DUTY 1023

// Dedicated LEDC timer/channel for the backlight - this is the only PWM user
// in the project, so these are free to pick without colliding with anything else.
#define BL_LEDC_TIMER   LEDC_TIMER_1
#define BL_LEDC_CHANNEL LEDC_CHANNEL_1

#ifdef HAS_CH422G
// Raw CH422G register write, bypassing our own CH422G class entirely and
// routed through FleetI2C rather than Wire directly (so it transparently
// uses whichever I2C backend this board is configured for - see FleetI2C.h).
// Added while chasing a persistent, unresolved GT911 I2C failure on
// WS_S3_TOUCH_LCD_5B that survived every other fix attempted (RGB timing
// porches, removing the LCD_RST low pulse, tearing down Wire before
// bb_captouch's re-init, lowering the touch I2C clock speed) - this makes
// the backlight-on and touch/LCD reset sequences a byte-for-byte match of
// Waveshare's own proven-working ESP-IDF reference
// (08_lvgl_Porting/main/waveshare_rgb_lcd_port.c), to rule out (or in) a bug
// in our own CH422G class's bit-manipulation logic. See DisplayManager::begin()
// and resetTouch()'s HAS_CH422G branch for the actual sequences.
static void ch422gRawWrite(uint8_t reg7, uint8_t data) {
    FleetI2C::beginTransmission(reg7);
    FleetI2C::write(data);
    FleetI2C::endTransmission();
}
#endif

DisplayManager::DisplayManager() {
    _bus = nullptr;
    _gfx = nullptr;
    #ifdef HAS_IO_EXPANDER
        _expander = nullptr;
    #endif
    #ifdef HAS_CH422G
        _ch422g = nullptr;
    #endif
}

bool DisplayManager::begin() {

    Serial.println();
    Serial.printf("Device init: %s\n", cfg.device_name);
    Serial.printf("Display hardware: %s\n", cfg.LCD_MODEL);
    Serial.printf("Touch panel: %s\n", cfg.TP_NAME);
    Serial.printf("PSRAM Total: %d bytes\n", ESP.getPsramSize());
    if (ESP.getPsramSize() == 0) {
        Serial.println("CRITICAL ERROR: PSRAM not found! Display will fail.");
    }
    Serial.printf("FLASH size : %d kb\r\n", ESP.getFlashChipSize() / 1024);
    
    Serial.println("------------------------------");

    Serial.println("[DisplayMgr] Begin");
    Serial.flush(); // Force output before crash

    // DEBUG_DISPLAY: raw BSP dimensions/rotation, for comparing against the
    // post-rotation gfx dimensions below. Enable with -D DEBUG_DISPLAY.
    #ifdef DEBUG_DISPLAY
    Serial.printf("[DisplayMgr] Display Dimensions: w=%d x h=%d, ROTATION=%d\n", cfg.WIDTH, cfg.HEIGHT, cfg.ROTATION); Serial.flush();
    #endif

    // 1. Initialize  I2C
    // Routed through FleetI2C instead of calling Wire.begin() directly, so
    // this board can transparently use whichever I2C backend it's
    // configured for (see FleetI2C.h). begin() is idempotent, so this is
    // safe even though TouchManager::begin() also calls it later.
    FleetI2C::begin(cfg.I2C_SDA_PIN, cfg.I2C_SCL_PIN);
    Serial.printf("[DisplayMgr] I2C backend: %s\n", FleetI2C::backendName());

    // 2. Initialize Display
    Serial.println("[DisplayMgr] Init Bus..."); Serial.flush();
    initBus();
    initPanel();

    if (!_gfx) {
        Serial.println("[DisplayMgr] GFX is NULL!"); Serial.flush();
        return false;
    }

    // 3. Start GFX
    Serial.println("[DisplayMgr] GFX->begin()..."); Serial.flush();
    if (!_gfx->begin()) {
        Serial.println("[DisplayMgr] GFX->begin() failed!"); Serial.flush();
        return false;
    }

    #ifdef DEBUG_DISPLAY
    // Raw color-fill test, bypassing LVGL entirely - if this doesn't show up
    // on the panel either, the problem is in the RGB pin table/timing (or the
    // panel isn't actually receiving valid sync), not the GUI layer above it.
    Serial.println("[DisplayMgr] DEBUG: Raw fill RED..."); Serial.flush();
    _gfx->fillScreen(RED);
    _gfx->flush();
    delay(1500);
    Serial.println("[DisplayMgr] DEBUG: Raw fill GREEN..."); Serial.flush();
    _gfx->fillScreen(GREEN);
    _gfx->flush();
    delay(1500);
    Serial.println("[DisplayMgr] DEBUG: Raw fill BLUE..."); Serial.flush();
    _gfx->fillScreen(BLUE);
    _gfx->flush();
    delay(1500);
    #endif

    #ifdef DEBUG_DISPLAY
    if (cfg.ROTATION >= 0) {
        Serial.printf("[DisplayMgr] Rotated Dimensions: w=%d x h=%d\n", _gfx->width(), _gfx->height()); Serial.flush();
    }
    #endif

    // 4. Init Backlight
    // We do this AFTER gfx->begin() to override any pinMode() calls
    Serial.println("------------------------------");
    Serial.println("[DisplayMgr] Init Backlight..."); Serial.flush();
    #ifdef HAS_CH422G
        // Verbatim port of Waveshare's own wavesahre_rgb_lcd_bl_on() (see
        // ch422gRawWrite() for why this bypasses our own CH422G class).
        ch422gRawWrite(0x24, 0x01); // WR_SET: IO bank -> output mode
        ch422gRawWrite(0x38, 0x1E); // WR_IO: TP_RST=1, LCD_BL=1, LCD_RST=1, SD_CS=1
        _currentBrightness = 100;
        Serial.println("[BacklightMgr] Backlight on (via CH422G expander, on/off only).");
    #else
        pinMode(cfg.LCD_BL, OUTPUT);
        initBacklightPWM(true);
    #endif

    #ifdef HAS_TOUCH
        resetTouch();
    #endif

    #ifdef HAS_IO_EXPANDER
        powerAmpEnable(true);
        powerAmpSwitch(true);
    #endif

    return true;
}

#ifdef HAS_CH422G
void DisplayManager::initExpander() {
    Serial.println("[DisplayMgr] Init CH422G Expander..."); Serial.flush();
    _ch422g = new CH422G();
    _ch422g->begin();
    _ch422g->setIOBankDirection(CH422G::IO_BANK_OUTPUT);
    // LCD_RST/TP_RST/LCD_BL are now handled by the verbatim ch422gRawWrite()
    // sequences in begin()/resetTouch() below, not through this class - see
    // the comment on ch422gRawWrite() for why.
}
#endif

void DisplayManager::initBus() {
    #ifdef HAS_CH422G
        initExpander();
    #endif
    #ifdef HAS_BUS
        #ifdef HAS_IO_EXPANDER
            Serial.println("[DisplayMgr] Init Expander Bus..."); Serial.flush();
            _expander = new Arduino_XCA9554SWSPI(
                cfg.EXIO_LCD_RST, cfg.EXIO_LCD_CS, cfg.EXIO_LCD_SCK, cfg.EXIO_LCD_MOSI,
                &Wire, cfg.EXPANDER_I2C_ADDR
            );
            _bus = _expander;
            if (!_bus) Serial.println("[DisplayMgr] RGB Panel Alloc Failed!"); Serial.flush();
        #elif HAS_QSPI_PANEL
            // Initialize QSPI Bus
            // CS, SCK, D0, D1, D2, D3
            _bus = new Arduino_ESP32QSPI(
                cfg.LCD_CS, cfg.LCD_SCK, 
                cfg.LCD_MOSI, cfg.QSPI_D1, cfg.QSPI_D2, cfg.QSPI_D3
            );
        #endif
    #endif
}

void DisplayManager::initPanel() {

    #ifdef HAS_RGB_PANEL
        // --- RGB PANEL (Self-buffered) ---
        Serial.println("[DisplayMgr] Creating RGB Panel..."); Serial.flush();
        Arduino_ESP32RGBPanel *rgbpanel = new Arduino_ESP32RGBPanel(
            cfg.LCD_DE, cfg.LCD_VSYNC, cfg.LCD_HSYNC, cfg.LCD_PCLK,
            cfg.R0, cfg.R1, cfg.R2, cfg.R3, cfg.R4,
            cfg.G0, cfg.G1, cfg.G2, cfg.G3, cfg.G4, cfg.G5,
            cfg.B0, cfg.B1, cfg.B2, cfg.B3, cfg.B4,
            cfg.HSYNC_POL, cfg.HSYNC_FPORCH, cfg.HSYNC_PWIDTH, cfg.HSYNC_BPORCH,
            cfg.VSYNC_POL, cfg.VSYNC_FPORCH, cfg.VSYNC_PWIDTH, cfg.VSYNC_BPORCH,
            cfg.PCLK_ACTIVE_NEG, cfg.PREFER_SPEED, cfg.USE_BIG_ENDIAN,
            cfg.DE_IDLE_HIGH, cfg.PCLK_IDLE_HIGH, cfg.BOUNCE_BUFFER_SIZE_PX
        );
        if (!rgbpanel) Serial.println("[DisplayMgr] RGB Panel Alloc Failed!"); Serial.flush();
        // RGB Display IS the canvas, so we assign it directly to _gfx
        Serial.println("[DisplayMgr] Creating RGB Display..."); Serial.flush();
        _gfx = new Arduino_RGB_Display(
            cfg.WIDTH, cfg.HEIGHT, rgbpanel, 
            cfg.ROTATION, true
        // Only WS_S3_SMART86 needs the init operations
            #ifdef WS_S3_SMART86
            , _bus, cfg.LCD_RST, cfg.INIT_CMDS_RGB, cfg.INIT_CMDS_SIZE
            #endif
        );
        if (!_gfx) Serial.println("[DisplayMgr] RGB Display Alloc Failed!"); Serial.flush();

    #elif HAS_QSPI_PANEL
        // --- QSPI PANEL (Requires Canvas Wrapper) ---
        // 1. Create the Hardware Driver (The "Writer")
        // We use a local pointer 'driver' because we don't need to store this globally.
        // The Canvas will remember it for us.
        // Note: We cast _bus back to Arduino_ESP32QSPI* because the constructor expects it.
        
        // Arduino_ESP32QSPI* qspi_bus = (Arduino_ESP32QSPI*)_bus;
        Arduino_AXS15231B *g = new Arduino_AXS15231B(
            _bus, GFX_NOT_DEFINED /* RST */, 0 /* Rotation */, false /* IPS */,
            cfg.WIDTH, cfg.HEIGHT
        );

        // 2. Create the Canvas (The "Frame Buffer" in PSRAM)
        // This solves the rotation and static issues.
        // We assign THIS to _gfx so all drawing calls go to memory first.
        _gfx = new Arduino_Canvas(
            cfg.WIDTH, cfg.HEIGHT, 
            g /* Output GFX */, 0, 0 /* Output X, Y */, cfg.ROTATION /* Rotation */
        );

    #elif HAS_MIPI_PANEL
        // --- MIPI PANEL ---
        Serial.println("[DisplayMgr] Creating DSI Panel..."); Serial.flush();
        Arduino_ESP32DSIPanel *dsipanel = new Arduino_ESP32DSIPanel(
            cfg.HSYNC_PWIDTH, cfg.HSYNC_BPORCH, cfg.HSYNC_FPORCH,
            cfg.VSYNC_PWIDTH, cfg.VSYNC_BPORCH, cfg.VSYNC_FPORCH
            , cfg.PREFER_SPEED, cfg.LANE_BIT_RATE
        );
        if (!dsipanel) Serial.println("[DisplayMgr] DSI Panel Alloc Failed!"); Serial.flush();

        Serial.println("[DisplayMgr] Creating DSI Display...");
        Arduino_DSI_Display *dsidisplay = new Arduino_DSI_Display(
            cfg.WIDTH, cfg.HEIGHT, dsipanel, cfg.ROTATION, cfg.AUTO_FLUSH,
            cfg.LCD_RST, cfg.INIT_CMDS_DSI, cfg.INIT_CMDS_SIZE
        );
        if (!dsidisplay) Serial.println("[DisplayMgr] DSI Display Alloc Failed!"); Serial.flush();
        _gfx = dsidisplay;
    #endif
}


void DisplayManager::initBacklightPWM(bool on) {
    // Check if pin is valid
    if (cfg.LCD_BL < 0) return;

    int level = on ? cfg.LCD_BL_ON_LEVEL : !cfg.LCD_BL_ON_LEVEL;
    digitalWrite(cfg.LCD_BL, level);
    Serial.printf("[BacklightMgr] Backlight pin on level: %s\n", (cfg.LCD_BL_ON_LEVEL ? "HIGH" : "LOW"));

    if (cfg.LCD_BL_FREQ <= 0) {
        Serial.println("[BacklightMgr] No PWM on this board (on/off backlight only) - fixed at full brightness.");
        // No dimming available, so report "full" rather than leaving _currentBrightness
        // at its zero-initialized default - that previously made getBrightness() claim
        // 0% while the backlight pin was actually being driven fully on above.
        _currentBrightness = 100;
        return;
    }

    Serial.printf("[BacklightMgr] Backlight Pin: %d, Freq: %dHz\n", cfg.LCD_BL, cfg.LCD_BL_FREQ);

    const ledc_timer_config_t backlight_timer = {
        .speed_mode      = LEDC_LOW_SPEED_MODE,
        .duty_resolution = (ledc_timer_bit_t)BL_PWM_RES,
        .timer_num       = BL_LEDC_TIMER,
        .freq_hz         = (uint32_t)cfg.LCD_BL_FREQ,
        .clk_cfg         = LEDC_AUTO_CLK,
    };

    const ledc_channel_config_t backlight_channel = {
        .gpio_num   = cfg.LCD_BL,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel    = BL_LEDC_CHANNEL,
        .intr_type  = LEDC_INTR_DISABLE,
        .timer_sel  = BL_LEDC_TIMER,
        .duty       = 0,
        .hpoint     = 0,
        // Active-LOW boards get inverted here in hardware, replacing the manual
        // "BL_MAX_DUTY - raw_duty" math setBrightness() used to do in software.
        .flags      = {.output_invert = (cfg.LCD_BL_ON_LEVEL == 0) ? 1u : 0u},
    };

    esp_err_t err  = ledc_timer_config(&backlight_timer);
               err |= ledc_channel_config(&backlight_channel);

    if (err != ESP_OK) {
        Serial.printf("[BacklightMgr] PWM Config Failed: %d\n", err);
    } else {
        Serial.println("[BacklightMgr] PWM Config Succeeded");
    }

    // Set default brightness
    setBrightness(_defaultBrightness);
}

void DisplayManager::setBrightness(uint8_t pct) {
    if (pct > 100) pct = 100;
    _currentBrightness = pct;

    // Any per-board minimum-brightness floor is enforced by the UI slider range
    // (see Panel_Display.cpp) rather than here, so this stays a plain, direct
    // mapping - active-LOW inversion is handled by the LEDC channel's
    // output_invert flag (set once in initBacklightPWM), not here.
    uint32_t duty = map(pct, 0, 100, 0, BL_MAX_DUTY);

    ledc_set_duty(LEDC_LOW_SPEED_MODE, BL_LEDC_CHANNEL, duty);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, BL_LEDC_CHANNEL);
    Serial.printf("[BacklightMgr] Set Brightness: %d%% (Duty: %d)\n", pct, duty);
}

void DisplayManager::setBacklight(bool on) {
    // Reuse brightness logic for simple on/off
    setBrightness(on ? _currentBrightness : 0);
}

int DisplayManager::getBrightness() {
    Serial.printf("[BacklightMgr] Get Brightness: %d%%\n", _currentBrightness);
    return _currentBrightness;
}

void DisplayManager::resetTouch() {
    #ifdef HAS_CH422G
        // Verbatim port of Waveshare's own waveshare_esp32_s3_touch_reset()
        // (see ch422gRawWrite() above for why this bypasses our CH422G class).
        // TP_INT is a native GPIO here (not on the expander); GT911 samples
        // it during reset to select its I2C address. Note this intentionally
        // does NOT release TP_INT back to INPUT afterward - their reference
        // doesn't either (their touch driver config sets int_gpio_num = -1,
        // i.e. polling-only, same as ours), so matching verbatim leaves it
        // as an output.
        ch422gRawWrite(0x24, 0x01); // WR_SET: IO bank -> output mode
        ch422gRawWrite(0x38, 0x2C); // WR_IO: LCD_BL=1, LCD_RST=1, TP_RST=0, USB_SEL=1
        delay(100);
        pinMode(cfg.TP_INT, OUTPUT);
        digitalWrite(cfg.TP_INT, LOW);
        delay(100);
        ch422gRawWrite(0x38, 0x2E); // WR_IO: TP_RST=1 (rest unchanged)
        delay(200);
    #elif defined(HAS_IO_EXPANDER)
        _expander->pinMode(cfg.EXIO_TP_RST, OUTPUT);
        _expander->pinMode(cfg.EXIO_TP_INT, OUTPUT);
        _expander->digitalWrite(cfg.EXIO_TP_INT, LOW);
        delay(20);
        _expander->digitalWrite(cfg.EXIO_TP_RST, LOW);
        delay(20);
        _expander->digitalWrite(cfg.EXIO_TP_RST, HIGH);
        delay(200);
    #else
    if (cfg.TP_RST >= 0 && cfg.TP_INT >= 0) {
        pinMode(cfg.TP_INT, INPUT_PULLUP);
        pinMode(cfg.TP_RST, OUTPUT);
        digitalWrite(cfg.TP_RST, LOW);
        delay(20);
        digitalWrite(cfg.TP_RST, HIGH);
        delay(200);
    }
    #endif
    Serial.println("[BusMgr] Touch Reset Complete");
}

void DisplayManager::powerAmpEnable(bool on) {
    #ifdef HAS_IO_EXPANDER
        #ifdef WS_S3_SMART86
            if (_expander) {
                _expander->pinMode(cfg.EXIO_AMP_EN, OUTPUT);
                Serial.printf("[BusMgr] Power Amp Pin Enabled (via Expander): %d\n", hw_cfg.I2S_AMP_EN);
            }
        #endif
    #else
        pinMode(hw_cfg.I2S_AMP_EN, OUTPUT);
        Serial.printf("[BusMgr] Power Amp Pin Enabled: %d\n", hw_cfg.I2S_AMP_EN);
    #endif
}

void DisplayManager::powerAmpSwitch(bool on) {
    #ifdef HAS_IO_EXPANDER
        #ifdef WS_S3_SMART86
            if (_expander) {
                _expander->digitalWrite(cfg.EXIO_AMP_EN, on ? HIGH : LOW);
                Serial.printf("[BusMgr] Power Amp Status (via Expander): %s\n", on ? "ON" : "OFF");
                return;
            }
        #endif
    #else
        digitalWrite(hw_cfg.I2S_AMP_EN, on ? HIGH : LOW);
        Serial.printf("[BusMgr] Power Amp Status: %s\n", on ? "ON" : "OFF");
    #endif
}