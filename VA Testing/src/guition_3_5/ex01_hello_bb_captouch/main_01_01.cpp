#include <Arduino.h>
#include <bb_spi_lcd.h>
#include <bb_captouch.h>
#include <Wire.h>

// --- HARDWARE CONFIGURATION ---
#define PANEL_DISPLAY  DISPLAY_CYD_535 
#define PANEL_TOUCH    TOUCH_CYD_535

BB_SPI_LCD lcd;
BBCapTouch touch;
uint8_t *pFrameBuffer;
TOUCHINFO ti;
int lcdWidth, lcdHeight;

void safe_flush() {
  lcd.display(); 
}

// Helper to draw text safely
void drawStatus(const char* msg, uint16_t color, int y) {
    lcd.setPrintFlags(DRAW_TO_RAM); // Ensure text goes to buffer
    lcd.setTextColor(color, TFT_BLACK);
    lcd.setFont(FONT_12x16);
    lcd.setCursor(10, y);
    lcd.print("                    "); // Erase previous line (simple way)
    lcd.setCursor(10, y);
    lcd.print(msg);
}

void setup() {
  Serial.begin(115200);
  delay(1000); 
  Serial.println("Touch Test Init (ex01)"); 

  int touchtest;

  // 1. Init Display (Proven Config)
  lcd.begin(PANEL_DISPLAY); 
  lcdWidth = lcd.width();   
  lcdHeight = lcd.height(); 
  
  pFrameBuffer = (uint8_t *)ps_malloc(lcdWidth * lcdHeight * sizeof(uint16_t));
  if (!pFrameBuffer) {
      Serial.println("PSRAM Alloc Failed");
      while(1);
  }
  spilcdSetBuffer(lcd.getLCDStruct(), (uint8_t *)pFrameBuffer);
  
  // Initial Clear
  lcd.fillScreen(TFT_BLACK, DRAW_TO_RAM);
  drawStatus("Init Display... OK", TFT_GREEN, 20);
  safe_flush();

  // 2. Init Touch
  // bb_captouch auto-detects the controller type
  touchtest = touch.init(PANEL_TOUCH);
  
  if (touchtest == CT_SUCCESS) {
      Serial.println("Touch Init Success");
      drawStatus("Touch Init: OK", TFT_GREEN, 50);
  } else {
      Serial.println("Touch Init Failed");
      drawStatus("Touch Init: FAIL", TFT_RED, 50);
      // Try to scan I2C to debug
      Wire.begin(4, 8); // CYD535 default I2C pins
      for (byte i = 8; i < 120; i++) {
        Wire.beginTransmission(i);
        if (Wire.endTransmission() == 0) {
            Serial.printf("I2C Device found at 0x%02X\n", i);
        }
      }
  }
  
  // Print sensorType
  Serial.println("Detected Touch Controller: " + String(touch.sensorType()));
  drawStatus(("Controller: " + String(touch.sensorType())).c_str(), TFT_YELLOW, 80);

  // Draw UI Frame
  lcd.drawRect(0, 0, lcdWidth, lcdHeight, TFT_BLUE, DRAW_TO_RAM);
  drawStatus("Touch Screen to Test", TFT_WHITE, 110);
  safe_flush();
}

void loop() {
    TOUCHINFO ti;
    char buf[32];

    // Poll the touch controller
    if (touch.getSamples(&ti)) {
        if (ti.count > 0) {
            int x = ti.x[0];
            int y = ti.y[0];
            
            Serial.printf("Touch: %d, %d (Area: %d)\n", x, y, ti.area[0]);

            // Visual Feedback
            // 1. Draw a marker at the touch point
            lcd.fillCircle(x, y, 10, TFT_MAGENTA, DRAW_TO_RAM);
            
            // 2. Update Coordinates Text
            sprintf(buf, "X:%03d Y:%03d", x, y);
            drawStatus(buf, TFT_CYAN, lcdHeight - 40);
            
            // 3. Flush immediately to see responsiveness
            safe_flush();
        }
    }
    delay(10); // Prevent bus flooding
}