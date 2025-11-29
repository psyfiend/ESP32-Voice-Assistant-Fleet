#include <Arduino.h>
#include "pin_config.h"
#include "display_init.h"

Arduino_DataBus *bus = new Arduino_XCA9554SWSPI(
    DB_RST, DB_CS, DB_SCK, DB_MOSI,
    &Wire, DB_I2C_ADDR
);

Arduino_ESP32RGBPanel *rgbpanel = new Arduino_ESP32RGBPanel(
    RGB_DE, RGB_VSYNC, RGB_HSYNC, RGB_PCLK,
    RGB_B0, RGB_B1, RGB_B2, RGB_B3, RGB_B4,
    RGB_G0, RGB_G1, RGB_G2, RGB_G3, RGB_G4, RGB_G5,
    RGB_R0, RGB_R1, RGB_R2, RGB_R3, RGB_R4,
    1 /* hsync_polarity */, 10 /* hsync_front_porch */, 8 /* hsync_pulse_width */, 50 /* hsync_back_porch */,
    1 /* vsync_polarity */, 10 /* vsync_front_porch */, 8 /* vsync_pulse_width */, 20 /* vsync_back_porch */
    );


Arduino_RGB_Display *gfx = new Arduino_RGB_Display(
    display_width, display_height, rgbpanel, display_rotation, display_auto_flush,
    bus, display_rst_pin, st7701_type1_init_operations, sizeof(st7701_type1_init_operations)
    );

// Create pin expander object
Arduino_XCA9554SWSPI *exio;

void init_display() {

    Serial.println("Starting display_init");

    Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL);
    Wire.beginTransmission(TCA9554_I2C);
    if (Wire.endTransmission() != 0) {
        Serial.println();
        Serial.println("[HW] CRITICAL: TCA9554 Not Found!");
        // return false;
    }

    // Enable Backlight
    pinMode(GFX_BL, OUTPUT);
    if (GFX_BL_ON_LOW) {
        digitalWrite(GFX_BL, LOW);
    } else {
        digitalWrite(GFX_BL, HIGH);
    }

    // Create Expander Interface Object
    exio = (Arduino_XCA9554SWSPI*)bus;
    // Start the expander explicitly (good practice)
    bus->begin();

    // Reset Touch Panel via Expander
    exio->pinMode(TP_RST, OUTPUT);
    exio->pinMode(TP_INT, OUTPUT);
    exio->digitalWrite(TP_INT, LOW);
    delay(200);
    exio->digitalWrite(TP_RST, LOW);
    delay(200);
    exio->digitalWrite(TP_RST, HIGH);
    delay(200);

    // Init Display
    if (!gfx->begin()) {
        Serial.println("gfx->begin() failed!");
    }

    Serial.println("display_init completed");
}