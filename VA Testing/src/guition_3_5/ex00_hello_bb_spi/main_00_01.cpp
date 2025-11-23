#include <Arduino.h>
#include <bb_spi_lcd.h> 

#define PANEL_DISPLAY    DISPLAY_CYD_535 
#define TEST_DELAY    4000 

BB_SPI_LCD lcd;
uint8_t *pFrameBuffer;
int lcdWidth, lcdHeight;

// Helper to map flags to string for serial debug
const char* getModeName(int iFlag) {
    return (iFlag == DRAW_TO_LCD) ? "DRAW_TO_LCD (Direct)" : "DRAW_TO_RAM (Buffered)";
}

void test_primitives(int iFlag) {
    const char* modeName = getModeName(iFlag);
    Serial.printf("\n--- Testing Mode: %s ---\n", modeName);
    
    lcd.setPrintFlags(iFlag);

    // 1. Fill Screen (Base Canvas)
    // If this fails, everything fails.
    lcd.fillScreen(TFT_BLACK, iFlag);

    // 2. Pixel (White) (Corners)
    // The most atomic operation. Checks basic addressing.
    // Drawing dots in corners.
    lcd.drawPixel(0, 0, TFT_WHITE, iFlag);
    lcd.drawPixel(lcdWidth-1, 0, TFT_WHITE, iFlag);
    lcd.drawPixel(0, lcdHeight-1, TFT_WHITE, iFlag);
    lcd.drawPixel(lcdWidth-1, lcdHeight-1, TFT_WHITE, iFlag);

    // 3. Line (Red) - Diagonal (Top Left)
    // Tests Bresenham algorithm & single-pixel optimization logic.
    lcd.drawLine(10, 10, 110, 110, TFT_RED, iFlag); 
    
    // 4. Rect Empty (Green) (Top Middle)
    // Tests horizontal and vertical line drawing.
    lcd.drawRect(130, 10, 80, 80, TFT_GREEN, iFlag);

    // 5. Rect Filled (Blue) (Top Right)
    // Tests block filling (memset optimization).
    lcd.fillRect(230, 10, 80, 80, TFT_BLUE, iFlag);

    // 6. Circle Empty (Yellow) (Center Left)
    // Tests arc drawing logic.
    lcd.drawCircle(60, 200, 40, TFT_YELLOW, iFlag);

    // 7. Circle Filled (Magenta) (Center Middle)
    // Tests scanline filling logic.
    lcd.fillCircle(170, 200, 40, TFT_MAGENTA, iFlag);

    // 8. Triangle Empty (Cyan) (Center Right)
    // Tests 3x line drawing.
    lcd.drawTriangle(240, 240, 280, 160, 319, 240, TFT_CYAN, iFlag);

    // 9. Triangle Filled (Orange) (Bottom Right)
    // Tests scanline filling logic (similar to fillRect but variable width).
    lcd.fillTriangle(240, 340, 280, 260, 319, 340, TFT_ORANGE, iFlag);

    // 10. Round Rect (White) (Bottom Left)
    // Tests mix of lines and arcs.
    lcd.drawRoundRect(10, 300, 100, 50, 10, TFT_WHITE, iFlag);

    // 11. Text (Standard)
    // Tests font rendering engine.
    lcd.setCursor(10, 400);
    lcd.setTextColor(TFT_WHITE, TFT_BLACK);
    lcd.setTextSize(1); // 8x8
    lcd.setFont(1); // Standard font
    lcd.println("Mode:");
    lcd.setCursor(10, 420);
    lcd.println(modeName);
  
    lcd.setCursor(10, 450);
    lcd.setTextColor(TFT_GREEN, TFT_BLACK);
    lcd.setTextSize(0); // 6x8
    lcd.setFont(0); // Small font
    lcd.println("If readable, Text works");

    lcd.setCursor(130, 300);
    lcd.setTextColor(TFT_YELLOW, TFT_BLACK);
    lcd.setTextSize(3); // 16x16
    lcd.setFont(3); // Large font
    lcd.print("Bigger text! 16x16");

    lcd.setCursor(130, 400);
    lcd.setTextColor(TFT_RED, TFT_BLACK);
    lcd.setTextSize(4); // 16x32
    lcd.setFont(4); // Extra Large font
    lcd.print("Biggest! 16x32");

    // If we are in RAM mode, we MUST flush to see anything
    if (iFlag == DRAW_TO_RAM) {
        unsigned long start = millis();
        lcd.display(); 
        Serial.printf("Buffer Flush Time: %lu ms\n", millis()-start);
    }
    
    delay(TEST_DELAY);
}

void setup() {
  Serial.begin(115200);
  delay(1000); 
  Serial.println("Comprehensive Library Capability Test"); 

  // Initialize Hardware
  if (lcd.begin(PANEL_DISPLAY) != 0) {
    Serial.println("LCD Init Failed");
    while(1);
  }
  lcdWidth = lcd.width();   
  lcdHeight = lcd.height(); 
  
  // Allocate PSRAM buffer
  // We need this for Phase B
  pFrameBuffer = (uint8_t *)ps_malloc(lcdWidth * lcdHeight * sizeof(uint16_t));
  if (!pFrameBuffer) {
      Serial.println("PSRAM Alloc Failed");
      while(1);
  }
  spilcdSetBuffer(lcd.getLCDStruct(), (uint8_t *)pFrameBuffer);
  
  Serial.println("Setup Complete.");
}

void loop() {
  // Cycle 1: Direct to LCD
  // This tests if the addressing bug (8-byte vs 4-byte) is truly fixed.
  test_primitives(DRAW_TO_LCD);

  // Cycle 2: Buffered to RAM
  // This tests if the cache coherency/PSRAM access issues persist.
  test_primitives(DRAW_TO_RAM);
}