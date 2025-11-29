#include <Arduino.h>
#include "Arduino_GFX_Library.h"
#include "pin_config.h"
#include <Wire.h>


// -------------------------------------------------------------------------
// 1. Hardware Definitions (Verified)
// -------------------------------------------------------------------------
#define I2C_SDA_PIN 47
#define I2C_SCL_PIN 48
#define TCA9554_ADDR 0x20


// -------------------------------------------------------------------------
// 2. Display Objects
// -------------------------------------------------------------------------

// 3a. IO Expander SPI Bridge
// Pins based on Waveshare schematic/demos
Arduino_DataBus *bus = new Arduino_XCA9554SWSPI(
    7 /* RST */, 0 /* CS */, 2 /* SCK */, 1 /* MOSI */,
    &Wire, TCA9554_ADDR
);

// 3b. RGB Panel Driver
Arduino_ESP32RGBPanel *rgbpanel = new Arduino_ESP32RGBPanel(
    17 /* DE */, 3 /* VSYNC */, 46 /* HSYNC */, 9 /* PCLK */,
    10 /* B0 */, 11 /* B1 */, 12 /* B2 */, 13 /* B3 */, 14 /* B4 */,
    21 /* G0 */, 8 /* G1 */, 18 /* G2 */, 45 /* G3 */, 38 /* G4 */, 39 /* G5 */,
    40 /* R0 */, 41 /* R1 */, 42 /* R2 */, 2 /* R3 */, 1 /* R4 */,
    1 /* hsync_polarity */, 10 /* hsync_front_porch */, 8 /* hsync_pulse_width */, 50 /* hsync_back_porch */,
    1 /* vsync_polarity */, 10 /* vsync_front_porch */, 8 /* vsync_pulse_width */, 20 /* vsync_back_porch */
);

// 3c. The Display Object
Arduino_RGB_Display *gfx = new Arduino_RGB_Display(
    480 /* width */, 480 /* height */, rgbpanel, 0 /* rotation */, true /* auto_flush */,
    bus, GFX_NOT_DEFINED /* RST */, st7701_type1_init_operations, sizeof(st7701_type1_init_operations)
);

// -------------------------------------------------------------------------
// 3. Main Logic
// -------------------------------------------------------------------------

void setup() {
    Serial.begin(115200);
    delay(2000);
    Serial.println("--- Waveshare S3 Smart86 GOLD MASTER ---");

    // 1. Verify PSRAM
    Serial.printf("PSRAM Total: %d bytes\n", ESP.getPsramSize());
    if (ESP.getPsramSize() == 0) {
        Serial.println("CRITICAL ERROR: PSRAM not found! Display will fail.");
        return;
    }

    // 2. Initialize I2C (CRITICAL: Must use 47/48)
    Serial.println("Initializing I2C (47, 48)...");
    Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);

    // 3. Scan for Expander
    Serial.println("Checking for TCA9554...");
    Wire.beginTransmission(TCA9554_ADDR);
    if (Wire.endTransmission() != 0) {
        Serial.println("CRITICAL ERROR: TCA9554 Expander not found! Init will fail.");
        return; 
    }
    Serial.println("TCA9554 Found!");

    // 4. Initialize Display
    Serial.println("Initializing Display Driver...");
    // gfx->begin() internally calls bus->begin(), which sends the init sequence above
    if (!gfx->begin()) {
        Serial.println("CRITICAL ERROR: gfx->begin() failed!");
        return;
    }
    Serial.println("Display Initialized Successfully!");

    // 5. Enable Backlight (via Expander)
    // We manually talk to the expander to ensure the backlight pin is HIGH.
    // We set ALL pins to OUTPUT and HIGH to be safe.
     Wire.beginTransmission(TCA9554_ADDR);
     Wire.write(0x03); // Config Register
     Wire.write(0x00); // 0x00 = All Pins are OUTPUT
     Wire.endTransmission();
    
     Wire.beginTransmission(TCA9554_ADDR);
     Wire.write(0x01); // Output Register
     Wire.write(0xFF); // 0xFF = All Pins HIGH
     Wire.endTransmission();
     Serial.println("Backlight Enabled.");
}

void loop() {
    // Test Pattern
    gfx->fillScreen(RED);
    delay(1000);
    gfx->fillScreen(GREEN);
    delay(1000);
    gfx->fillScreen(BLUE);
    delay(1000);
    
    gfx->fillScreen(BLACK);
    gfx->setCursor(100, 200);
    gfx->setTextSize(3);
    gfx->setTextColor(WHITE);
    gfx->println("Success!");
    delay(2000);
}