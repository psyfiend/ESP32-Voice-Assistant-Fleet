#ifndef BOARD_HAS_PSRAM
#error "Error: This program requires PSRAM enabled, please enable PSRAM option in 'Tools' menu of Arduino IDE"
#endif
#include <Arduino.h>
#include <Arduino_GFX_Library.h>
#include "DisplayManager.h"

// The Global Manager
DisplayManager displayMgr;

void HelloWorld_DisplayManager(Arduino_GFX *gfx) {

    gfx->setCursor(random(gfx->width()), random(gfx->height()));
    gfx->setTextColor(random(0xffff), random(0xffff));
    gfx->setTextSize(random(2,10) /* x scale */, random(2,10) /* y scale */, random(2,5) /* pixel_margin */);
    gfx->println("Hello World!");

    gfx->flush();

    delay(500); // 0.5 seconds
}

void setup() {
    Serial.begin(115200);
    // Give the USB Serial time to catch up
    delay(1000);
    Serial.println("--- Booting Voice Assistant Fleet ---");

    Serial.println("Arduino_GFX Hello World DisplayManager demo");

    // 1. Initialize
    if (!displayMgr.begin()) {
        Serial.println("Display Init Failed");
        while(1) delay(100);
    }
    
    Arduino_GFX *gfx = displayMgr.getGfx();

    gfx->fillScreen(RGB565_BLACK);

    Serial.println("Initial Display State:");
    Serial.printf("Display Size: %d x %d\n", gfx->width(), gfx->height());

    // 2. Rotation Test (Landscape)
    gfx->setRotation(cfg.ROTATION);
    Serial.printf("After setRotation(%d):", cfg.ROTATION);
    Serial.printf("Display Size: %d x %d\n", gfx->width(), gfx->height());


    Serial.println("Hardware Ready.");

    gfx->setCursor(gfx->width() / 2 - 20, gfx->height() / 2 - 10);
    gfx->setTextColor(RGB565_RED);
    gfx->println("Hello World!");

    gfx->flush();

    delay(5000); // 5 seconds

    Serial.println("Started HelloWorld_DisplayManager");
}

void loop()
{
    Arduino_GFX *gfx = displayMgr.getGfx();
    HelloWorld_DisplayManager(gfx);
}
