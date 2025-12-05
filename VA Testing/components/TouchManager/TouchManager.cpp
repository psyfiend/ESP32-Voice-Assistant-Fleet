#include "TouchManager.h"
#include "bb_captouch.h"

TouchManager::TouchManager() {
}

bool TouchManager::begin() {
    

    // -----------------------------------------------------------
    // STRATEGY A: Specific Panel ID (Fleet Architecture)
    // -----------------------------------------------------------
    // If bb_cap_touch has a specific configuration for the device 
    // and it is in the BSP as #define TOUCH_PANEL, use that.
    // The library uses its internal lookup table for pins.
    #ifdef TOUCH_PANEL
        Serial.printf("[TouchManager] Initializing Panel ID: %d\n", TOUCH_PANEL);
        
        // Assumes your fork has an overloaded init(int type)
        int touchtest = _touch.init(TOUCH_PANEL);

    // -----------------------------------------------------------
    // STRATEGY B: Manual Pin Config (Legacy/Generic)
    // -----------------------------------------------------------
    #else
        // Manual touch init using explicit pin numbers from Fleet_BSP
        int sda, scl, rst, irq;

        if (cfg.I2C_SDA_PIN >= 0 && cfg.I2C_SCL_PIN >= 0) {
            sda = cfg.TP_SDA;
            scl = cfg.TP_SCL;
        }

        if (cfg.TP_RST >= 0) {
            rst = cfg.TP_RST;
        } else {
            rst = -1; // Unused
        }
        
        if (cfg.TP_INT >= 0) {
            irq = cfg.TP_INT;
        } else {
            irq = -1; // Unused
        }

        // Initialize bb_captouch with explicit pins
        Serial.printf("[Touch] Init SDA:%d SCL:%d IRQ:%d RST:%d\n", sda, scl, irq, rst);
        int touchtest = _touch.init(sda, scl, irq, rst);
    #endif

    // -----------------------------------------------------------
    // Validation
    // -----------------------------------------------------------
    if (touchtest == CT_SUCCESS) {
        Serial.println("[TouchManager] Initialization Success");
    } else {
        Serial.printf("[TouchManager] Initialization Failed. Error Code: %d\n", touchtest);
        return false;
    }

    // Serial.printf("[TouchManager] Controller Found. Type ID: %d\n", touchtest);
    return true;
}

bool TouchManager::read(int *x, int *y) {
    TOUCHINFO ti;
    
    // bb_captouch stores state internally, we just ask for samples
    if (_touch.getSamples(&ti)) {
        if (ti.count > 0) {
            // Get the RAW physical coordinates
            int rawX = ti.x[0];
            int rawY = ti.y[0];
            
            // Map them to the current display rotation
            mapCoordinates(&rawX, &rawY);
            
            // Return the processed values
            *x = rawX;
            *y = rawY;
            return true;
        }
    }
    return false;
}

void TouchManager::mapCoordinates(int *x, int *y) {
    int rawX = *x;
    int rawY = *y;

    // ==========================================================
    // GUITION 3.5" (Native 320x480)
    // ==========================================================
    #ifdef GUITION_3248W535

        int SW_ROTATION = cfg.ROTATION;

        if (cfg.ROTATION > 3 || cfg.ROTATION < 0) {
            SW_ROTATION = 0;    // Fallback to 0 if invalid
        }

        // The AXS15231/CST816 usually reports 0..320 (X) and 0..480 (Y)
        // irrespective of what the screen is doing.
        // We assume NATIVE_WIDTH = 320, NATIVE_HEIGHT = 480

        switch (SW_ROTATION) {
            case 0: // Portrait (Native)
                // No change needed usually
                *x = rawX;
                *y = rawY;
                break;

            case 1: // Landscape (90 deg CW)
                // Top-Left of Portrait becomes Top-Right of Landscape
                // New X = Raw Y
                // New Y = NativeWidth - Raw X
                *x = rawY;
                *y = 320 - rawX;
                break;

            case 2: // Inverted Portrait (180 deg)
                // Top-Left becomes Bottom-Right
                *x = 320 - rawX;
                *y = 480 - rawY;
                break;

            case 3: // Inverted Landscape (270 deg CW)
                // Top-Left becomes Bottom-Left
                // New X = NativeHeight - Raw Y
                // New Y = Raw X
                *x = 480 - rawY;
                *y = rawX;
                break;
        }

    // ==========================================================
    // WAVESHARE S3 BOX (Native 480x480)
    // ==========================================================
    #elif WS_S3_SMART86
        // Square display is easier, but still needs rotation logic
        // if we ever rotate the UI.
        // For now, pass through as hardware is often 1:1 mapped
        *x = rawX;
        *y = rawY;
    #endif

    // Sanity Clip (keep inside logical bounds)
    if (*x < 0) *x = 0;
    if (*y < 0) *y = 0;
    if (*x >= cfg.WIDTH) *x = cfg.WIDTH - 1;
    if (*y >= cfg.HEIGHT) *y = cfg.HEIGHT - 1;
}
