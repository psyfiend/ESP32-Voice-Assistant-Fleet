#ifndef BOARD_HAS_PSRAM
#error "Error: This program requires PSRAM enabled, please enable PSRAM option in 'Tools' menu of Arduino IDE"
#endif
#include <Arduino.h>
#include <Arduino_GFX_Library.h>
#include "DisplayManager.h"
#if defined(WS_P4_4B)
    #include "panels/BSP_WS_P4_Smart86_LCD.h"
#elif defined(WS_S3_4B)
    #include "panels/BSP_WS_S3_Smart86_LCD.h"
#elif defined(GUITION_3248W535)
    #include "panels/BSP_Guition_3248W535_LCD.h"
#elif defined(GUITION_8048W550)
    #include "panels/BSP_Guition_8048W550_LCD.h"
#endif

// The Global Manager
DisplayManager displayMgr;

void HelloWorld_DisplayManager(Arduino_GFX *gfx) {

    gfx->setCursor(random(gfx->width()), random(gfx->height()));
    gfx->setTextColor(random(0xffff), random(0xffff));
    gfx->setTextSize(random(6) /* x scale */, random(6) /* y scale */, random(2) /* pixel_margin */);
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
    gfx->setRotation(bsp_display.ROTATION);
    Serial.printf("After setRotation(%d):", bsp_display.ROTATION);
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
