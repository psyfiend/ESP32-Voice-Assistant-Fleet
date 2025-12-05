#include <Arduino_GFX_Library.h>
#include "DisplayManager.h"


DisplayManager::DisplayManager() {
    _bus = nullptr;
    _gfx = nullptr;
    #ifdef HAS_IO_EXPANDER
        _expander = nullptr;
    #endif
}

bool DisplayManager::begin() {
    Serial.println("[DisplayManager] Begin");
    Serial.flush(); // Force output before crash

    // 1. Initialize Common Hardware (I2C)
    Wire.begin(cfg.I2C_SDA_PIN, cfg.I2C_SCL_PIN);
    
    // 2. Initialize the Data Bus
    Serial.println("[DisplayManager] Init Bus..."); Serial.flush();
    initBus();

    // 3. Initialize the Panel/Display Driver
    initPanel();

    if (!_gfx) {
        Serial.println("[DisplayManager] GFX is NULL!"); Serial.flush();
        return false;
    }

    // 4. Start the Hardware
    // Note: When using Arduino_Canvas, begin() recursively starts the output driver too.
    Serial.println("[DisplayManager] GFX->begin()..."); Serial.flush();
    if (!_gfx->begin()) {
        Serial.println("[DisplayManager] GFX->begin() failed!"); Serial.flush();
        return false;
    }

    // 5. Turn on Backlight using the smart logic
    pinMode(cfg.LCD_BL, OUTPUT);
    setBacklight(true);

    return true;
}

void DisplayManager::initBus() {
    #ifdef WS_S3_SMART86
        _expander = new Arduino_XCA9554SWSPI(
            cfg.EXIO_LCD_RST, cfg.EXIO_LCD_CS, cfg.EXIO_LCD_SCK, cfg.EXIO_LCD_MOSI,
            &Wire, cfg.EXPANDER_I2C_ADDR
        );
        _bus = _expander;

    #elif GUITION_3248W535
        // Initialize QSPI Bus
        // CS, SCK, D0, D1, D2, D3
        _bus = new Arduino_ESP32QSPI(
            cfg.LCD_CS, cfg.LCD_SCK, 
            cfg.LCD_MOSI, cfg.QSPI_D1, cfg.QSPI_D2, cfg.QSPI_D3
        );
    #endif
}

void DisplayManager::initPanel() {

    #ifdef HAS_RGB_PANEL
        // --- RGB PANEL (Self-buffered) ---
        Arduino_ESP32RGBPanel *rgbpanel = new Arduino_ESP32RGBPanel(
            cfg.LCD_DE, cfg.LCD_VSYNC, cfg.LCD_HSYNC, cfg.LCD_PCLK,
            cfg.B0, cfg.B1, cfg.B2, cfg.B3, cfg.B4,
            cfg.G0, cfg.G1, cfg.G2, cfg.G3, cfg.G4, cfg.G5,
            cfg.R0, cfg.R1, cfg.R2, cfg.R3, cfg.R4,
            cfg.HSYNC_POL, cfg.HSYNC_FPORCH, cfg.HSYNC_PWIDTH, cfg.HSYNC_BPORCH,
            cfg.VSYNC_POL, cfg.VSYNC_FPORCH, cfg.VSYNC_PWIDTH, cfg.VSYNC_BPORCH
            , cfg.PCLK_ACTIVE_NEG, cfg.PCLK_HZ
        );
        // RGB Display IS the canvas, so we assign it directly to _gfx
        _gfx = new Arduino_RGB_Display(
            cfg.WIDTH, cfg.HEIGHT, rgbpanel, 
            cfg.ROTATION, true, _bus, cfg.LCD_RST
        // Only WS_S3_SMART86 needs the init operations
            #ifdef WS_S3_SMART86
            , cfg.INIT_CMDS_RGB, cfg.INIT_CMDS_SIZE
            #endif
        );

    #elif GUITION_3248W535
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
        Serial.println("[DisplayManager] Creating DSI Panel..."); Serial.flush();
        Arduino_ESP32DSIPanel *dsipanel = new Arduino_ESP32DSIPanel(
            cfg.HSYNC_PWIDTH, cfg.HSYNC_BPORCH, cfg.HSYNC_FPORCH,
            cfg.VSYNC_PWIDTH, cfg.VSYNC_BPORCH, cfg.VSYNC_FPORCH
            , cfg.PREFER_SPEED, cfg.LANE_BIT_RATE
        );
        if (!dsipanel) Serial.println("[DisplayManager] DSI Panel Alloc Failed!"); Serial.flush();

        Serial.println("[DisplayManager] Creating DSI Display...");
        Arduino_DSI_Display *dsidisplay = new Arduino_DSI_Display(
            cfg.WIDTH, cfg.HEIGHT, dsipanel, cfg.ROTATION, cfg.AUTO_FLUSH,
            cfg.LCD_RST, cfg.INIT_CMDS_DSI, cfg.INIT_CMDS_SIZE
        );
        if (!dsidisplay) Serial.println("[DisplayManager] DSI Display Alloc Failed!"); Serial.flush();
        _gfx = dsidisplay;
    #endif
}

void DisplayManager::setBacklight(bool on) {
    int level = on ? cfg.LCD_BL_ON_LEVEL : !cfg.LCD_BL_ON_LEVEL;
    digitalWrite(cfg.LCD_BL, level);
}

void DisplayManager::resetTouch() {
  #ifdef HAS_TOUCH
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
  #endif
}