#pragma once
#include <Arduino.h>
#include <bb_captouch.h>
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

class TouchManager {
public:
    TouchManager();

    // Returns true if a touch controller was found
    bool begin();

    // Reads the latest touch point. 
    // Returns true if touched, populates x and y.
    bool read(int *x, int *y);

private:
    BBCapTouch _touch;
    // Helper to map coordinates if orientation doesn't match display
    void mapCoordinates(int *x, int *y);
};