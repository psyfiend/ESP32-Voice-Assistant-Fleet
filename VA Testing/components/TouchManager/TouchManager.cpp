#include "TouchManager.h"
#include "bb_captouch.h"

TouchManager::TouchManager() {
    _lastTouchTime = 0;
    _debounceMs = 50; // 50ms is usually the sweet spot for capacitive jitter
    _isTouchActive = false;
    _lastTouchCount = 0;
}

bool TouchManager::begin() {
    
    Serial.println("------------------------------");

    // If bb_cap_touch has a specific configuration for the device 
    // and it is in the BSP as #define TOUCH_PANEL, use that.
    #ifdef TOUCH_PANEL
        Serial.printf("[TouchMgr] Initializing %s using bb_captouch ID: %d\n", cfg.TP_NAME, TOUCH_PANEL);
        
        // Assumes your fork has an overloaded init(int type)
        int touchtest = _touch.init(TOUCH_PANEL);
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
        Serial.println("[TouchMgr] No bb_captouch ID defined, using manual pin config...");
        Serial.printf("[TouchMgr] Init SDA:%d SCL:%d IRQ:%d RST:%d\n", sda, scl, irq, rst);
        int touchtest = _touch.init(sda, scl, irq, rst);
    #endif

    if (touchtest == CT_SUCCESS) {
        Serial.println("[TouchMgr] Initialization Success");
    } else {
        Serial.printf("[TouchMgr] Initialization Failed. Error Code: %d\n", touchtest);
        return false;
    }

    return true;
}

// <--- ADDED: New Multitouch Implementation
// Fetches all available touch points up to maxPoints
uint8_t TouchManager::read(TouchPoint* points, uint8_t maxPoints) {
    TOUCHINFO ti;
    
    // 1. Fetch Raw Samples
    // We check the hardware state first
    bool hardwareAvailable = _touch.getSamples(&ti);

    // 2. State Machine Logic
    if (hardwareAvailable && ti.count > 0) {
        // --- CASE A: Valid Signal Detected ---
        _isTouchActive = true;
        _lastTouchCount = ti.count;
        _lastTouchTime = millis();
        
        // Safety: don't overflow internal cache
        if(_lastTouchCount > 5) _lastTouchCount = 5;

        // Process and Cache this valid frame
        for (uint8_t i = 0; i < _lastTouchCount; i++) {
            _lastValidPoints[i].x = ti.x[i];
            _lastValidPoints[i].y = ti.y[i];
            _lastValidPoints[i].strength = ti.area[i];
            _lastValidPoints[i].id = i; 

            // Apply rotation map IMMEDIATELY so cache contains valid screen coords
            mapCoordinates(&_lastValidPoints[i]);
        }
    } 
    else {
        // --- CASE B: Signal Loss (or Release) ---
        // We only accept "Released" if the signal has been gone longer than _debounceMs
        if (_isTouchActive && (millis() - _lastTouchTime < _debounceMs)) {
            // It's a glitch! (Signal drop < 50ms)
            // Restore state from CACHE (do not update time)
            // This bridges the gap in the I2C stream
        } else {
            // It's a real release (or we've been released for a while)
            _isTouchActive = false;
            _lastTouchCount = 0;
            return 0;
        }
    }

    // 3. Output Population
    // Whether fresh or cached, we copy the valid state to the user's buffer
    uint8_t returnCount = (_lastTouchCount > maxPoints) ? maxPoints : _lastTouchCount;
    
    for (uint8_t i = 0; i < returnCount; i++) {
        points[i] = _lastValidPoints[i];
    }

    return returnCount;
}

// <--- UPDATED: Legacy Wrapper
// Maintains backward compatibility by calling the new reader
bool TouchManager::read(int *x, int *y) {
    TouchPoint p; // temporary container
    
    // We only ask for 1 point here
    if (read(&p, 1) > 0) {
        *x = p.x;
        *y = p.y;
        return true;
    }
    return false;
}

void TouchManager::mapCoordinates(TouchPoint *point) {
    int rawX = point->x;
    int rawY = point->y;

    // ==========================================================
    // ROTATION LOGIC
    // GUITION 3.5" (Native 320x480)
    // ==========================================================
    // #ifdef GUITION_3248W535
    #ifndef WS_P4_7B
    if (cfg.ROTATION >= 0) 
        {
        // Serial.println("[TouchMgr] Initializing GUITION 3248W535 rotation mapping..."); 
        // (Commented out Serial to prevent log spam in loop)

        int SW_ROTATION = cfg.ROTATION;

        if (cfg.ROTATION > 3 || cfg.ROTATION < 0) {
            SW_ROTATION = 0;    // Fallback to 0 if invalid
        }

        switch (SW_ROTATION) {
            case 0: // Portrait (Native)
                point->x = rawX;
                point->y = rawY;
                break;

            case 1: // Landscape (90 deg CW)
                point->x = rawY;
                point->y = cfg.WIDTH - rawX;
                break;

            case 2: // Inverted Portrait (180 deg)
                point->x = cfg.WIDTH - rawX;
                point->y = cfg.HEIGHT - rawY;
                break;

            case 3: // Inverted Landscape (270 deg CW)
                point->x = cfg.HEIGHT - rawY;
                point->y = rawX;
                break;
            }
        }   
    // ==========================================================
    // WAVESHARE S3/P4 BOXES (Native Square/Landscape)
    // ==========================================================
    //elif defined(WS_S3_SMART86) || defined(WS_P4_SMART86) || defined(WS_P4_7B)
    // Pass through for now as hardware is often 1:1 mapped on these panels
    #else
         point->x = rawX;
         point->y = rawY;
    #endif


    // Sanity Clip (keep inside logical bounds)
    if (point->x < 0) point->x = 0;
    if (point->y < 0) point->y = 0;
    if (point->x >= cfg.WIDTH) point->x = cfg.WIDTH - 1;
    if (point->y >= cfg.HEIGHT) point->y = cfg.HEIGHT - 1;
}