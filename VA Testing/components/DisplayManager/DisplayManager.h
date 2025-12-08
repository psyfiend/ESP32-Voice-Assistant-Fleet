#pragma once
#include <Arduino.h>
#include <Arduino_GFX_Library.h>
#include <Wire.h>
#if defined(WS_P4_SMART86)
    #include "BSP_WS_P4_Smart86_LCD.h"
#elif defined(WS_P4_7B)
    #include "BSP_WS_P4_7B.h"
#elif defined(WS_S3_SMART86)
    #include "BSP_WS_S3_Smart86.h"
#elif defined(GUITION_3248W535)
    #include "BSP_Guition_3248W535.h"
#elif defined(GUITION_8048W550)
    #include "BSP_Guition_8048W550.h"
#elif defined(GUITION_1060P470)
    #include "BSP_Guition_1060P470.h"
#endif

class DisplayManager {
public:
    DisplayManager();

    // Initializes the display hardware
    bool begin();

    // Getters for the libraries (LVGL needs these)
    Arduino_GFX* getGfx() { return _gfx; }
    
    // Backlight Control
    void setBacklight(bool on);      // On/Off
    void setBrightness(uint8_t pct); // 0-100% PWM
    int getBrightness();

    // Helper for Touch Reset (since it involves the expander)
    void resetTouch();

private:
    Arduino_DataBus *_bus;
    Arduino_GFX *_gfx;

    // Initial backlight level
    int _defaultBrightness = 100;
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