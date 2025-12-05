#pragma once
#if defined(WS_P4_SMART86)
    #include "panels/BSP_WS_P4_Smart86_LCD.h"
#elif defined(WS_P4_7B)
    #include "panels/BSP_WS_P4_7B_LCD.h"
#elif defined(WS_S3_SMART86)
    #include "panels/BSP_WS_S3_Smart86_LCD.h"
#elif defined(GUITION_3248W535)
    #include "panels/BSP_Guition_3248W535_LCD.h"
#elif defined(GUITION_8048W550)
    #include "panels/BSP_Guition_8048W550_LCD.h"
#endif
#include <Arduino.h>
#include <Arduino_GFX_Library.h>
#include <Wire.h>

class DisplayManager {
public:
    // Singleton pattern (optional, but good for hardware managers)
    DisplayManager();

    // The Big Red Button: Initializes Wire, Expander, Panel, and GFX
    bool begin();

    // Getters for the libraries (LVGL needs these)
    Arduino_GFX* getGfx() { return _gfx; }
    
    // Helper to turn backlight on/off
    void setBacklight(bool on);

    // Helper for Touch Reset (since it involves the expander)
    void resetTouch();

private:
    // Pointers to the hardware drivers
    // We use "void*" for the bus because it could be RGB or SPI or QSPI
    // but in Arduino_GFX, the base class for bus is Arduino_DataBus
    Arduino_DataBus *_bus;
    
    // The final display driver
    Arduino_GFX *_gfx;

    // Specific to Waveshare S3: The IO Expander acts as the bus AND the GPIO provider
    // We keep a specific pointer to it if defined, so we can do digitalWrite calls
    #ifdef HAS_IO_EXPANDER
      #ifdef WS_S3_SMART86
        Arduino_XCA9554SWSPI *_expander;
      #endif
    #endif
    void initBus();
    void initPanel();
};