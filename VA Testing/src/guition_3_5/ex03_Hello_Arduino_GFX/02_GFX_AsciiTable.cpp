#include <Arduino.h>
#include <Arduino_GFX_Library.h>
#include <lvgl.h>
#include "pin_config.h"
#include "DisplayManager.h"

// The Global Manager
DisplayManager displayMgr;


void setup() {
    Serial.begin(115200);
    // Give the USB Serial time to catch up
    delay(1000);
    Serial.println("--- Booting Voice Assistant Fleet ---");

    // 1. Initialize Display Hardware via Manager
    if (!displayMgr.begin()) {
        Serial.println("CRITICAL: Display Init Failed!");
        // Blink error code forever
        pinMode(PIN_LCD_BL, OUTPUT);
        while(1) {
            digitalWrite(PIN_LCD_BL, !digitalRead(PIN_LCD_BL));
            delay(100);
        }
    }
    
    // 2. Specific Touch Reset (Handled internally by manager now!)
    displayMgr.resetTouch();
    
    Serial.println("Hardware Ready.");

  int charWidth = 8;
  int charHeight = 10;

  int numCols = DISPLAY_WIDTH / charWidth;
  int numRows = DISPLAY_HEIGHT / charHeight;

#ifdef GFX_EXTRA_PRE_INIT
  GFX_EXTRA_PRE_INIT();
#endif



  Serial.println("[HW] Initialization Complete.");
  
  displayMgr.getGfx()->fillScreen(BLACK);

  displayMgr.getGfx()->setTextColor(GREEN);
  for (int x = 0; x < numCols; x++) {
    if (x % 4 == 0) {
      displayMgr.getGfx()->setCursor(10 + x * charWidth, 2);
      displayMgr.getGfx()->print(x, 16);
    }
  }


  displayMgr.getGfx()->setTextColor(BLUE);
  for (int y = 0; y < numCols; y++) {
    displayMgr.getGfx()->setCursor(2, 12 + y * charHeight);
    displayMgr.getGfx()->print(y, 16);
  }

  char c = 0;
  for (int y = 0; y < numRows; y++) {
    for (int x = 0; x < numCols; x++) {
      displayMgr.getGfx()->drawChar(10 + x * charWidth, 12 + y * charHeight, c++, WHITE, BLACK);
    }
  }

  delay(5000);
}

void loop() {
}
