#include <Arduino.h>
#include <Arduino_GFX_Library.h>
#include <lvgl.h>
#include "pin_config.h"
#include "DisplayManager.h"

Arduino_ESP32QSPI *bus = new Arduino_ESP32QSPI(45, 47, 21, 48, 40, 39);
Arduino_AXS15231B *g = new Arduino_AXS15231B(bus, GFX_NOT_DEFINED, 0, false, 320, 480);
Arduino_Canvas *gfx = new Arduino_Canvas(320, 480, g, 0, 0, 0);


void setup() {
    Serial.begin(115200);
    // Give the USB Serial time to catch up
    delay(1000);
    Serial.println("--- Booting Voice Assistant Fleet ---");


    Serial.println("Hardware Ready.");

    Serial.println("Arduino_GFX Hello World example");

    // Init Display

    if (!gfx->begin())
    {
      Serial.println("gfx->begin() failed!");
    }
    gfx->fillScreen(RGB565_BLACK);

    // Set landscape mode
    gfx->setRotation(1);

    pinMode(PIN_LCD_BL, OUTPUT); // Back Light On
    digitalWrite(PIN_LCD_BL, HIGH);

    gfx->setCursor(10, 10);
    gfx->setTextColor(RGB565_RED);
    gfx->println("Hello World!");

    gfx->flush();

    delay(5000); // 5 seconds
}

void loop()
{
    gfx->setCursor(random(gfx->width()), random(gfx->height()));
    gfx->setTextColor(random(0xffff), random(0xffff));
    gfx->setTextSize(random(6) /* x scale */, random(6) /* y scale */, random(2) /* pixel_margin */);
    gfx->println("Hello World!");

    gfx->flush();

    delay(1000); // 1 second
}
