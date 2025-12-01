#include "DisplayManager.h"
#include "pin_config.h"

DisplayManager::DisplayManager() {
    _bus = nullptr;
    _gfx = nullptr;
    #ifdef BOARD_HAS_IO_EXPANDER
        _expander = nullptr;
    #endif
}

bool DisplayManager::begin() {
    // 1. Initialize Common Hardware (I2C)
    Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL);
    
    // 2. Initialize the Data Bus
    initBus();

    // 3. Initialize the Panel/Display Driver
    initPanel();

    if (!_gfx) return false;

    // 4. Start the Hardware
    // Note: When using Arduino_Canvas, begin() recursively starts the output driver too.
    if (!_gfx->begin()) {
        return false;
    }

    // 5. Turn on Backlight using the smart logic
    pinMode(PIN_LCD_BL, OUTPUT);
    setBacklight(true);

    return true;
}

void DisplayManager::initBus() {
    //No DataBus for Guition 8048W550C
    #ifdef WS_S3_SMART86
        _expander = new Arduino_XCA9554SWSPI(
            PIN_LCD_RST, PIN_LCD_CS, PIN_LCD_SCK, PIN_LCD_MOSI,
            &Wire, IO_EXPANDER_I2C_ADDR
        );
        _bus = _expander;

    #elif GUITION_3248W535
        // Initialize QSPI Bus
        // CS, SCK, D0, D1, D2, D3
        _bus = new Arduino_ESP32QSPI(
            PIN_LCD_CS, PIN_LCD_SCK, 
            PIN_LCD_SD0, PIN_LCD_SD1, PIN_LCD_SD2, PIN_LCD_SD3
        );
    #endif
}

void DisplayManager::initPanel() {

    #if defined(WS_S3_SMART86) || defined(JC8048W550C)
        // --- RGB PANEL (Self-buffered) ---
        Arduino_ESP32RGBPanel *rgbpanel = new Arduino_ESP32RGBPanel(
            PIN_RGB_DE, PIN_RGB_VSYNC, PIN_RGB_HSYNC, PIN_RGB_PCLK,
            PIN_RGB_B0, PIN_RGB_B1, PIN_RGB_B2, PIN_RGB_B3, PIN_RGB_B4,
            PIN_RGB_G0, PIN_RGB_G1, PIN_RGB_G2, PIN_RGB_G3, PIN_RGB_G4, PIN_RGB_G5,
            PIN_RGB_R0, PIN_RGB_R1, PIN_RGB_R2, PIN_RGB_R3, PIN_RGB_R4,
            RGB_HSYNC_POL, RGB_HSYNC_FPORCH, RGB_HSYNC_PWIDTH, RGB_HSYNC_BPORCH,
            RGB_VSYNC_POL, RGB_VSYNC_FPORCH, RGB_VSYNC_PWIDTH, RGB_VSYNC_BPORCH
            //, RGB_PCLK_ACTIVE_NEG, RGB_PCLK_HZ
        );
        // RGB Display IS the canvas, so we assign it directly to _gfx
        _gfx = new Arduino_RGB_Display(
            DISPLAY_WIDTH, DISPLAY_HEIGHT, rgbpanel, 
            DISPLAY_ROTATION, true, 
            _bus, GFX_NOT_DEFINED
        // Only WS_S3_SMART86 needs the init operations
            #ifdef WS_S3_SMART86
            , st7701_type1_init_operations, sizeof(st7701_type1_init_operations)
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
            DISPLAY_WIDTH, DISPLAY_HEIGHT
        );

        // 2. Create the Canvas (The "Frame Buffer" in PSRAM)
        // This solves the rotation and static issues.
        // We assign THIS to _gfx so all drawing calls go to memory first.
        _gfx = new Arduino_Canvas(
            DISPLAY_WIDTH, DISPLAY_HEIGHT, 
            g /* Output GFX */, 0, 0 /* Output X, Y */, DISPLAY_ROTATION /* Rotation */
        );

    #endif
}

void DisplayManager::setBacklight(bool on) {
    int level = on ? LCD_BL_ON_LEVEL : !LCD_BL_ON_LEVEL;
    digitalWrite(PIN_LCD_BL, level);
}

void DisplayManager::resetTouch() {
  #ifdef BOARD_HAS_TOUCH
    #ifdef BOARD_HAS_IO_EXPANDER
        _expander->pinMode(PIN_TP_RST, OUTPUT);
        _expander->pinMode(PIN_TP_INT, OUTPUT);
        
        _expander->digitalWrite(PIN_TP_INT, LOW);
        delay(20);
        _expander->digitalWrite(PIN_TP_RST, LOW);
        delay(20);
        _expander->digitalWrite(PIN_TP_RST, HIGH);
        delay(200);
    #else
        pinMode(PIN_TP_INT, INPUT_PULLUP);
        pinMode(PIN_TP_RST, OUTPUT);
        digitalWrite(PIN_TP_RST, LOW);
        delay(20);
        digitalWrite(PIN_TP_RST, HIGH);
        delay(200);
    #endif
  #endif
}