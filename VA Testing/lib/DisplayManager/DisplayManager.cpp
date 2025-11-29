#include "DisplayManager.h"

DisplayManager::DisplayManager() {
    _bus = nullptr;
    _gfx = nullptr;
    #if defined(HAS_IO_EXPANDER)
        _expander = nullptr;
    #endif
}

bool DisplayManager::begin() {
    // 1. Initialize Common Hardware (I2C)
    // Note: If using a board without I2C, wrap this in #ifdef
    Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL);
    
    // 2. Initialize the Data Bus (The "Pipe" to the screen)
    initBus();

    // 3. Initialize the Panel/Display Driver
    initPanel();

    if (!_gfx) return false;

    // 4. Start the Hardware
    // Note: For the Waveshare, _bus->begin() creates the I2C transaction to the expander
    if (!_gfx->begin()) {
        return false;
    }

    // 5. Turn on Backlight (Standard GPIO on this board)
    pinMode(PIN_LCD_BL, OUTPUT);
    setBacklight(true);

    return true;
}

void DisplayManager::initBus() {
    #if defined(WS_S3_SMART86)
        // The Waveshare uses the IO Expander as a "Software SPI" bus 
        // to send config commands to the ST7701
        _expander = new Arduino_XCA9554SWSPI(
            PIN_LCD_RST, PIN_LCD_CS, PIN_LCD_SCK, PIN_LCD_MOSI,
            &Wire, IO_EXPANDER_I2C_ADDR
        );
        // The expander IS the bus
        _bus = _expander;

    #elif defined(GUITION_S3_QSPI)
        // Example for future QSPI board
        // _bus = new Arduino_ESP32QSPI(...);
    #endif
}

void DisplayManager::initPanel() {
    #if defined(WS_S3_SMART86)
        // 1. Create the RGB Timing Object
        Arduino_ESP32RGBPanel *rgbpanel = new Arduino_ESP32RGBPanel(
            PIN_RGB_DE, PIN_RGB_VSYNC, PIN_RGB_HSYNC, PIN_RGB_PCLK,
            PIN_RGB_B0, PIN_RGB_B1, PIN_RGB_B2, PIN_RGB_B3, PIN_RGB_B4,
            PIN_RGB_G0, PIN_RGB_G1, PIN_RGB_G2, PIN_RGB_G3, PIN_RGB_G4, PIN_RGB_G5,
            PIN_RGB_R0, PIN_RGB_R1, PIN_RGB_R2, PIN_RGB_R3, PIN_RGB_R4,
            1, 10, 8, 50,  // HSYNC timings
            1, 10, 8, 20   // VSYNC timings
        );

        // 2. Create the Final Display Object
        _gfx = new Arduino_RGB_Display(
            DISPLAY_WIDTH, DISPLAY_HEIGHT, rgbpanel, 0, true,
            _bus, GFX_NOT_DEFINED, 
            st7701_type1_init_operations, sizeof(st7701_type1_init_operations)
        );

    #endif
}

void DisplayManager::setBacklight(bool on) {
    // Logic: If we want ON (true), use the ON LEVEL. If OFF (false), invert it.
    int level = on ? LCD_BL_ON_LEVEL : !LCD_BL_ON_LEVEL;
    digitalWrite(PIN_LCD_BL, level);
}

void DisplayManager::resetTouch() {
    #if defined(WS_S3_SMART86) && defined(HAS_IO_EXPANDER)
        // The Touch Reset is on the IO Expander, NOT the main ESP32 GPIO
        _expander->pinMode(PIN_TP_RST, OUTPUT);
        _expander->pinMode(PIN_TP_INT, OUTPUT);
        
        _expander->digitalWrite(PIN_TP_INT, LOW);
        delay(20);
        _expander->digitalWrite(PIN_TP_RST, LOW);
        delay(20);
        _expander->digitalWrite(PIN_TP_RST, HIGH);
        delay(200);
    #endif
}