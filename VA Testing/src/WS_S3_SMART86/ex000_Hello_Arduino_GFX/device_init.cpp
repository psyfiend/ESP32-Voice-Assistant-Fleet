//
// hardware_config.cpp
//
// Implementation of hardware initialization for Waveshare ESP32-S3 Smart 86 Box.
// Contains all pin definitions, init sequences, and driver setup.
//
// Engineer: Gemini
// Architect: Eric
//

#include "device_init.h"
#include <Wire.h>

// -------------------------------------------------------------------------
// Private Configuration (Hidden)
// -------------------------------------------------------------------------
#define I2C_SDA_PIN 47
#define I2C_SCL_PIN 48
#define TCA9554_I2C 0x20
#define GFX_BL 4

// -------------------------------------------------------------------------
// Global Object Definitions
// -------------------------------------------------------------------------

// The pointers are defined here, but allocated in initHardware()
Arduino_DataBus *bus = NULL;
// Arduino_RGB_Display *gfx = NULL;
Arduino_XCA9554SWSPI *exio = NULL;

// -------------------------------------------------------------------------
// Hardware Init Implementation
// -------------------------------------------------------------------------

void initHardware() {
    Serial.println("[HW] Initializing Waveshare Smart 86 Box...");

    // 1. Verify PSRAM
    if (ESP.getPsramSize() == 0) {
        Serial.println("[HW] CRITICAL: PSRAM not found! Display buffer cannot be allocated.");
        // return false;
    }

    // 2. Initialize I2C
    Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);
    
    // 3. Check Pin Expander
    Wire.beginTransmission(TCA9554_I2C);
    if (Wire.endTransmission() != 0) {
        Serial.println("[HW] CRITICAL: TCA9554 Not Found!");
        // return false;
    }

    // 4a. Create DataBus (using TCA9554 as GPIO expander over SPI)
    bus = new Arduino_XCA9554SWSPI(
        7 /* RST */, 0 /* CS */, 2 /* SCK */, 1 /* MOSI */,
        &Wire, TCA9554_I2C
    );
    
    // 4b. Create Panel
    Arduino_ESP32RGBPanel *rgbpanel = new Arduino_ESP32RGBPanel(
        17 /* DE */, 3 /* VSYNC */, 46 /* HSYNC */, 9 /* PCLK */,
        10 /* B0 */, 11 /* B1 */, 12 /* B2 */, 13 /* B3 */, 14 /* B4 */,
        21 /* G0 */, 8 /* G1 */, 18 /* G2 */, 45 /* G3 */, 38 /* G4 */, 39 /* G5 */,
        40 /* R0 */, 41 /* R1 */, 42 /* R2 */, 2 /* R3 */, 1 /* R4 */,
        1 /* hsync_polarity */, 10 /* hsync_front_porch */, 8 /* hsync_pulse_width */, 50 /* hsync_back_porch */,
        1 /* vsync_polarity */, 10 /* vsync_front_porch */, 8 /* vsync_pulse_width */, 20 /* vsync_back_porch */
    );

    // 4c. Create Display
    Arduino_RGB_Display*gfx = new Arduino_RGB_Display(
        480 /* width */, 480 /* height */, rgbpanel, 0 /* rotation */, true /* auto_flush */,
        bus, GFX_NOT_DEFINED /* RST */, st7701_type1_init_operations, sizeof(st7701_type1_init_operations)
    );

    // 5. Create Expander Object
    // Store specific pointer for direct access (safe cast since we just created it)
    exio = (Arduino_XCA9554SWSPI*)bus;
    
    // Start the expander explicitly (good practice)
    bus->begin();

    // 5. Start Display
    if (!gfx->begin()) {
        Serial.println("[HW] CRITICAL: gfx->begin() failed.");
        // return false;
    }

    // 6. Enable Backlight
    exio->pinMode(GFX_BL, OUTPUT);
    exio->digitalWrite(GFX_BL, HIGH);

    exio->pinMode(5, OUTPUT);
    exio->pinMode(6, OUTPUT);
    exio->digitalWrite(6, LOW);
    delay(200);
    exio->digitalWrite(5, LOW);
    delay(200);
    exio->digitalWrite(5, HIGH);
    delay(200);

    Serial.println("[HW] Initialization Complete.");
    // return true;
}
