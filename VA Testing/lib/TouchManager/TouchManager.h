#pragma once

#include <Arduino.h>
#include <bb_captouch.h>
#include "pin_config.h"

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