#pragma once
#include <Arduino.h>
#include <Arduino_GFX_Library.h>
#include <Wire.h>
#include "bsp_loader.h"  // Fleet_BSP inclusion

class DisplayManager {
public:
    DisplayManager();

    // Initializes the display hardware
    bool begin();

    // Getters for the libraries (LVGL needs these)
    Arduino_GFX* getGfx() { return _gfx; }
    // Arduino_XCA9554SWSPI* getExpander() { return _exio; }
    
    // Backlight Control
    void setBacklight(bool on);      // On/Off
    void setBrightness(uint8_t pct); // 0-100% PWM
    int getBrightness();

    // Helpers for external components
    void resetTouch();
    void powerAmpEnable(bool on);
    void powerAmpSwitch(bool on);

private:
    Arduino_DataBus *_bus;
    Arduino_GFX *_gfx;
    

    // Initial backlight level
    int _defaultBrightness = 75;
    int _currentBrightness;

    void initBus();
    void initPanel();
    void initBacklightPWM(bool on);

    #ifdef HAS_IO_EXPANDER
      #ifdef WS_S3_SMART86
        Arduino_XCA9554SWSPI *_expander;
      #endif
    #endif
};