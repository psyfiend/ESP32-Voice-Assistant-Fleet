#include <Arduino.h>
#include "Arduino_GFX_Library.h"
#include "pin_config.h"
#include <Wire.h>

Arduino_XCA9554SWSPI *exio;

Arduino_DataBus *bus = new Arduino_XCA9554SWSPI(
    7 /* RST */, 0 /* CS */, 2 /* SCK */, 1 /* MOSI */,
    &Wire, TCA9554_I2C
);

Arduino_ESP32RGBPanel *rgbpanel = new Arduino_ESP32RGBPanel(
    17 /* DE */, 3 /* VSYNC */, 46 /* HSYNC */, 9 /* PCLK */,
    10 /* B0 */, 11 /* B1 */, 12 /* B2 */, 13 /* B3 */, 14 /* B4 */,
    21 /* G0 */, 8 /* G1 */, 18 /* G2 */, 45 /* G3 */, 38 /* G4 */, 39 /* G5 */,
    40 /* R0 */, 41 /* R1 */, 42 /* R2 */, 2 /* R3 */, 1 /* R4 */,
    1 /* hsync_polarity */, 10 /* hsync_front_porch */, 8 /* hsync_pulse_width */, 50 /* hsync_back_porch */,
    1 /* vsync_polarity */, 10 /* vsync_front_porch */, 8 /* vsync_pulse_width */, 20 /* vsync_back_porch */
);

Arduino_RGB_Display *gfx = new Arduino_RGB_Display(
    480 /* width */, 480 /* height */, rgbpanel, 0 /* rotation */, true /* auto_flush */,
    bus, GFX_NOT_DEFINED /* RST */, st7701_type1_init_operations, sizeof(st7701_type1_init_operations)
);

void setup(void) {
  Serial.begin(115200);
  // Serial.setDebugOutput(true);
  // while(!Serial);
  Serial.println("Arduino_GFX Hello World example");

  Wire.begin(47, 48);

  // 3. Scan for Expander
  Serial.println("Checking for TCA9554...");
  Wire.beginTransmission(TCA9554_I2C);
  if (Wire.endTransmission() != 0) {
      Serial.println("CRITICAL ERROR: TCA9554 Expander not found! Init will fail.");
      return; 
  }
  Serial.println("TCA9554 Found!");

#ifdef GFX_EXTRA_PRE_INIT
  GFX_EXTRA_PRE_INIT();
#endif

  // Create Expander Object
  exio = (Arduino_XCA9554SWSPI*)bus;
  
  // Start the expander explicitly (good practice)
  bus->begin();

  exio->pinMode(5, OUTPUT);
  exio->pinMode(6, OUTPUT);
  exio->digitalWrite(6, LOW);
  delay(200);
  exio->digitalWrite(5, LOW);
  delay(200);
  exio->digitalWrite(5, HIGH);
  delay(200);


  // Init Display
  if (!gfx->begin()) {
    Serial.println("gfx->begin() failed!");
  }
  gfx->fillScreen(RGB565_WHITE);
  gfx->setCursor(10, 10);
  gfx->setTextColor(RGB565_RED);
  gfx->println("Hello World!");

  delay(2000);  // 5 seconds
}

void loop() {
  gfx->setCursor(random(gfx->width()), random(gfx->height()));
  gfx->setTextColor(random(0xffff), random(0xffff));
  gfx->setTextSize(random(6) /* x scale */, random(6) /* y scale */, random(2) /* pixel_margin */);
  gfx->println("Hello World!");

  delay(200);
}
