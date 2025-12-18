#pragma once
#include <Arduino.h>
#include "DisplayManager.h"
#include <bb_captouch.h>
#include "bsp_loader.h"  // Fleet_BSP inclusion

// This structure is essential for LVGL 9.4 to track gestures (pinch/zoom) accurately.
struct TouchPoint {
    uint16_t x;
    uint16_t y;
    uint16_t strength; // Pressure or Area (useful for threshold filtering)
    uint8_t id;        // Hardware Tracking ID (0-4) - Vital for tracking *which* finger moved
};

class TouchManager {
public:
    TouchManager();

    // Returns true if a touch controller was found
    bool begin();

    // --= Primary Multitouch Reader =--
    // Populates the provided array 'points'. 
    // Defaults to the hardware limit (cfg.TP_MAX_TOUCH) if not specified.
    // Returns the number of active touches found.
    uint8_t read(TouchPoint* points, uint8_t maxPoints = cfg.TP_MAX_TOUCH);

    // <--- UPDATED: Legacy Single-Touch Wrapper
    // Reads the primary touch point only.
    // Returns true if touched, populates x and y.
    // Kept for backward compatibility with existing main logic.
    bool read(int *x, int *y);

private:
    BBCapTouch _touch;
    
    // Internal Helper to map raw coordinates based on rotation
    void mapCoordinates(TouchPoint *point);

    // --- Debounce State Variables (ADDED) ---
    TouchPoint _lastValidPoints[5]; // Cache for the last good frame
    uint32_t _lastTouchTime;        // Timestamp of last valid hardware signal
    uint32_t _debounceMs;           // Hysteresis window
    bool _isTouchActive;            // Smoothed state
    uint8_t _lastTouchCount;        // Count from last good frame
};