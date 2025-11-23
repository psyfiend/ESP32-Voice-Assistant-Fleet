#include <Arduino.h>
#include <bb_spi_lcd.h>
#include <bb_captouch.h>
#include <Wire.h>

// --- HARDWARE CONFIGURATION ---
#define PANEL_DISPLAY   DISPLAY_CYD_535 
#define PANEL_TOUCH     TOUCH_CYD_535

BB_SPI_LCD lcd;
BBCapTouch touch;
TOUCHINFO ti;
uint8_t *pFrameBuffer;
int lcdWidth, lcdHeight;
int currentRotation = 0;
unsigned long lastRotationTime = 0;

void safe_flush() {
  lcd.display(); 
}

// Helper to draw text safely
void drawStatus(const char* msg, uint16_t color, int y) {
    lcd.setPrintFlags(DRAW_TO_RAM); 
    lcd.setTextColor(color, TFT_BLACK);
    lcd.setFont(FONT_12x16);
    lcd.setCursor(10, y);
    lcd.print("                    "); 
    lcd.setCursor(10, y);
    lcd.print(msg);
}

void updateRotation() {
    lcd.fillScreen(TFT_BLACK, DRAW_TO_RAM);
    lcd.setRotation(currentRotation);
    
    // Update dimensions after rotation
    lcdWidth = lcd.width();
    lcdHeight = lcd.height();
    
    // Draw Orientation Markers
    lcd.drawRect(0, 0, lcdWidth, lcdHeight, TFT_BLUE, DRAW_TO_RAM);
    
    // Draw an "UP" arrow marker at the top-center
    int cx = lcdWidth / 2;
    lcd.fillTriangle(cx, 10, cx-20, 40, cx+20, 40, TFT_YELLOW, DRAW_TO_RAM);
    
    char buf[32];
    sprintf(buf, "Rotation: %d", currentRotation);
    drawStatus(buf, TFT_WHITE, 60);
    
    // Corner markers to verify extent
    lcd.fillRect(0, 0, 10, 10, TFT_RED, DRAW_TO_RAM); // TL
    lcd.fillRect(lcdWidth-10, 0, 10, 10, TFT_GREEN, DRAW_TO_RAM); // TR
    lcd.fillRect(0, lcdHeight-10, 10, 10, TFT_BLUE, DRAW_TO_RAM); // BL
    lcd.fillRect(lcdWidth-10, lcdHeight-10, 10, 10, TFT_WHITE, DRAW_TO_RAM); // BR

    safe_flush();
}

void setup() {
  Serial.begin(115200);
  delay(1000); 
  Serial.println("Rotation Test Init"); 

  if (lcd.begin(PANEL_DISPLAY) != 0) {
      Serial.println("Display Init Failed");
      while(1);
  }
  
  // Allocate max possible buffer (portrait)
  // Rotation might swap width/height, but max pixels remains constant
  pFrameBuffer = (uint8_t *)ps_malloc(320 * 480 * sizeof(uint16_t)); 
  spilcdSetBuffer(lcd.getLCDStruct(), (uint8_t *)pFrameBuffer);
  
  int touchtest;

  touchtest = touch.init(PANEL_TOUCH);

  if (touchtest == CT_SUCCESS) {
      Serial.println("Touch Init Success");
  } else {
      Serial.println("Touch Init Failed");
  }
  
  updateRotation();
}

void loop() {
    TOUCHINFO ti;
    char buf[32];

    // Handle Touch
    if (touch.getSamples(&ti)) {
        if (ti.count > 0) {
            int x = ti.x[0];
            int y = ti.y[0];
            
            // Map touch coordinates based on rotation?
            // bb_captouch usually returns raw physical coordinates.
            // Let's see if they match the visual rotation.
            
            Serial.printf("Rot:%d Raw Touch: %d, %d\n", currentRotation, x, y);

            // Draw dot where the code THINKS the touch is
            lcd.fillCircle(x, y, 10, TFT_MAGENTA, DRAW_TO_RAM);
            
            sprintf(buf, "T: %d,%d", x, y);
            drawStatus(buf, TFT_CYAN, lcdHeight/2);
            safe_flush();
        }
    }
    
    // Cycle rotation every 5 seconds
    if (millis() - lastRotationTime > 8000) {
        currentRotation++;
        if (currentRotation > 3) currentRotation = 0;
        updateRotation();
        lastRotationTime = millis();
    }
    
    delay(10);
}