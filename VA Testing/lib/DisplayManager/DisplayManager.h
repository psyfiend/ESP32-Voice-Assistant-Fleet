#pragma once

#include <Arduino.h>
#include <Arduino_GFX_Library.h>
#include <Wire.h>
#include "../pin_config.h" // Pin definitions and board config


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
    #if defined(HAS_IO_EXPANDER)
        Arduino_XCA9554SWSPI *_expander;
    #endif

    void initBus();
    void initPanel();
};