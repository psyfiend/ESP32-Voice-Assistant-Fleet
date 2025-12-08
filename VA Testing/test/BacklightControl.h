#pragma once

#include <Arduino.h>
#if defined(WS_P4_SMART86)
    #include "panels/BSP_WS_P4_Smart86_LCD.h"
#elif defined(WS_P4_7B)
    #include "panels/BSP_WS_P4_7B.h"
#elif defined(WS_S3_SMART86)
    #include "panels/BSP_WS_S3_Smart86.h"
#elif defined(GUITION_3248W535)
    #include "panels/BSP_Guition_3248W535.h"
#elif defined(GUITION_8048W550)
    #include "panels/BSP_Guition_8048W550.h"
#elif defined(GUITION_1060P470)
    #include "panels/BSP_Guition_1060P470.h"
#endif

// --- Configuration matching Waveshare Hardware ---
//      cfg.LCD_BL      32      // GPIO 32 for P4 7-inch
#define BL_PWM_FREQ     5000    // 5kHz frequency
#define BL_PWM_RES      10      // 10-bit resolution (0-1023 steps)
#define BL_MAX_DUTY     1023    // Max value for 10-bit

// State tracking
static int _currentBrightness = 100;

/**
 * Initialize the Backlight LEDC channel
 */
void initBacklight() {
    // Arduino 3.x+ API: ledcAttach(pin, freq, resolution)
    // This sets up the timer and channel automatically
    if (!ledcAttach(cfg.LCD_BL, BL_PWM_FREQ, BL_PWM_RES)) {
        Serial.println("ERROR: Failed to attach Backlight PWM!");
    } else {
        Serial.printf("Backlight initialized on Pin %d at %dHz\n", cfg.LCD_BL, BL_PWM_FREQ);
    }

    // Initialize to full brightness
    // We call the function to handle the inversion logic immediately
    void setBrightness(uint8_t percent); // Forward declaration
    setBrightness(100); 
}

/**
 * Set Backlight Brightness
 * @param percent: 0 to 100
 */
void setBrightness(uint8_t percent) {
    // Clamp input
    if (percent > 100) percent = 100;
    
    _currentBrightness = percent;

    // --- Math Explanation ---
    // The Hardware is "Active LOW" (0V = On, 3.3V = Off).
    // The Arduino PWM is "Active HIGH" (Duty 0 = 0V, Duty Max = 3.3V).
    
    // Step 1: Calculate the "Standard" duty for this percentage
    // e.g., 100% -> 1023, 50% -> 511
    uint32_t raw_duty = map(percent, 0, 100, 0, BL_MAX_DUTY);

    // Step 2: Invert it for the Hardware
    // If we want 100% Brightness, we need Pin LOW (0V).
    // If we want 0% Brightness, we need Pin HIGH (3.3V).
    
    uint32_t inverted_duty = BL_MAX_DUTY - raw_duty;

    // Apply
    ledcWrite(cfg.LCD_BL, inverted_duty);

    // Debug output (optional, remove for production)
    // Serial.printf("Set Brightness: %d%% (Duty: %d)\n", percent, inverted_duty);
}

/**
 * Get current brightness level
 */
int getBrightness() {
    return _currentBrightness;
}