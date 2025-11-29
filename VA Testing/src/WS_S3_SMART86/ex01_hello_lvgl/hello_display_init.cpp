#include <Wire.h>
#include <Arduino.h>

#include "Arduino_GFX_Library.h"
#include "pin_config.h"
#include "display_init.h"

Arduino_RGB_Display *gfx;
Arduino_XCA9554SWSPI *exio;

void setup() {

    Serial.println("Main starting");

    init_display();

    Serial.println("Main init_display() complete.");

    // Init Display
    if (!gfx->begin()) {
        Serial.println("gfx->begin() failed!");
    }

    gfx->fillScreen(RGB565_WHITE);
    gfx->setCursor(10, 10);
    gfx->setTextColor(RGB565_RED);
    gfx->println("Hello World!");

    delay(2000);  // 5 seconds

    Serial.println("Main completed");

}

void loop() {

    gfx->setCursor(random(gfx->width()), random(gfx->height()));
    gfx->setTextColor(random(0xffff), random(0xffff));
    gfx->setTextSize(random(6) /* x scale */, random(6) /* y scale */, random(2) /* pixel_margin */);
    gfx->println("Hello World!");

  delay(200);
}