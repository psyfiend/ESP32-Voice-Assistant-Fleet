#include <Arduino_GFX_Library.h>
#include <esp32-hal-ledc.h>
#include "DisplayManager.h"

#define BL_PWM_RES 10
#define BL_MAX_DUTY 1023

DisplayManager::DisplayManager() {
    _bus = nullptr;
    _gfx = nullptr;
    #ifdef HAS_IO_EXPANDER
        _expander = nullptr;
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

    // 1. Initialize  I2C
    Wire.begin(cfg.I2C_SDA_PIN, cfg.I2C_SCL_PIN);
    
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

    /*if (cfg.ROTATION != 0) {
        Serial.println("[DisplayMgr] Initial Display Size: ");
        Serial.printf("Display Size: %d x %d\n", _gfx->width(), _gfx->height());
        _gfx->setRotation(cfg.ROTATION);
        Serial.printf("[DisplayMgr] Setting Rotation: %d\n", cfg.ROTATION); Serial.flush();
        Serial.println("[DisplayMgr] After Rotation Size: ");
        Serial.printf("%d x %d\n", _gfx->height(), _gfx->width());
    }*/  

    // 4. Init Backlight
    // We do this AFTER gfx->begin() to override any pinMode() calls
    Serial.println("------------------------------");
    Serial.println("[DisplayMgr] Init Backlight..."); Serial.flush();
    pinMode(cfg.LCD_BL, OUTPUT);
    initBacklightPWM(true);

    #ifdef HAS_TOUCH
        resetTouch();
    #endif

    #ifdef HAS_IO_EXPANDER
        powerAmpEnable(true);
        powerAmpSwitch(true);
    #endif

    return true;
}

void DisplayManager::initBus() {
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
            cfg.VSYNC_POL, cfg.VSYNC_FPORCH, cfg.VSYNC_PWIDTH, cfg.VSYNC_BPORCH
            , cfg.PCLK_ACTIVE_NEG, cfg.PREFER_SPEED
            #ifdef WS_S3_SMART86
            , cfg.USE_BIG_ENDIAN
            #endif
        );
        if (!rgbpanel) Serial.println("[DisplayMgr] RGB Panel Alloc Failed!"); Serial.flush();
        // RGB Display IS the canvas, so we assign it directly to _gfx
        Serial.println("[DisplayMgr] Creating RGB Display..."); Serial.flush();
        _gfx = new Arduino_RGB_Display(
            cfg.WIDTH, cfg.HEIGHT, rgbpanel, 
            cfg.ROTATION, true, _bus, cfg.LCD_RST
        // Only WS_S3_SMART86 needs the init operations
            #ifdef WS_S3_SMART86
            , cfg.INIT_CMDS_RGB, cfg.INIT_CMDS_SIZE
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

    Serial.printf("[BacklightMgr] Backlight Pin: %d, Freq: %dHz\n", cfg.LCD_BL, cfg.LCD_BL_FREQ);

    // Attach Pin to LEDC Channel
    // Arduino 3.0+: ledcAttach(pin, freq, res)
    if (!ledcAttach(cfg.LCD_BL, cfg.LCD_BL_FREQ, BL_PWM_RES)) {
        Serial.println("[BacklightMgr] PWM Attach Failed!");
    } else {
        Serial.println("[BacklightMgr] PWM Attach Succeeded");
    }

    // Set default brightness
    setBrightness(_defaultBrightness);
}

void DisplayManager::setBrightness(uint8_t pct) {
    if (pct > 100) pct = 100;
    _currentBrightness = pct;

    // Calculate Raw Duty (0 to 1023)
    uint32_t raw_duty = map(pct, 0, 100, 0, BL_MAX_DUTY);
    uint32_t final_duty = raw_duty;

    // Handle Inversion based on BSP Config
    // If ON_LEVEL is 0 (Active Low), we must invert the duty cycle
    if (cfg.LCD_BL_ON_LEVEL == 0) {
        final_duty = BL_MAX_DUTY - raw_duty;
    }

    // Write to Hardware
    ledcWrite(cfg.LCD_BL, final_duty);
    Serial.printf("[BacklightMgr] Set Brightness: %d%% (Raw: %d, Final: %d)\n", pct, raw_duty, final_duty);
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
    #ifdef HAS_IO_EXPANDER
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