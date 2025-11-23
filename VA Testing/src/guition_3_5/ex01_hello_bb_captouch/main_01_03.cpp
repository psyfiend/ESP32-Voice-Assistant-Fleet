#include <Arduino.h>
#include <bb_spi_lcd.h>
#include <bb_captouch.h>
#include <Wire.h>
#include "font8x8.h" // Uses our software font renderer

// --- CONFIGURATION ---
#define PANEL_DISPLAY DISPLAY_CYD_535 
#define PANEL_TOUCH   TOUCH_CYD_535

BB_SPI_LCD lcd;
BBCapTouch touch;
uint8_t *pFrameBuffer;
int lcdWidth, lcdHeight;

int currentRot = 0;
bool needsUpdate = true;

// --- Software Renderer Helpers (Copied from main_01_01) ---
void myDrawPixel(int x, int y, uint16_t color) {
    if (x < 0 || x >= lcdWidth || y < 0 || y >= lcdHeight) return;
    lcd.drawPixel(x, y, color, DRAW_TO_RAM);
}

void myDrawLine(int x0, int y0, int x1, int y1, uint16_t color) {
  int dx = abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
  int dy = -abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
  int err = dx + dy, e2;
  while (true) {
    myDrawPixel(x0, y0, color);
    if (x0 == x1 && y0 == y1) break;
    e2 = 2 * err;
    if (e2 >= dy) { err += dy; x0 += sx; }
    if (e2 <= dx) { err += dx; y0 += sy; }
  }
}

void myDrawRect(int x, int y, int w, int h, uint16_t color) {
    myDrawLine(x, y, x+w-1, y, color);         
    myDrawLine(x, y+h-1, x+w-1, y+h-1, color); 
    myDrawLine(x, y, x, y+h-1, color);         
    myDrawLine(x+w-1, y, x+w-1, y+h-1, color); 
}

void myDrawChar(int x, int y, char c, uint16_t color, uint16_t bg, int scale) {
    if (c < 32 || c > 127) return;
    const uint8_t *bitmap = font8x8_basic[c - 32]; 
    for(int row=0; row<8; row++) {
        uint8_t bits = bitmap[row];
        for(int col=0; col<8; col++) {
            // Use MSB-first logic (7-col)
            bool drawFG = (bits & (1 << (7-col))); 
            int sx = x + (col * scale); 
            int sy = y + (row * scale);
            if (drawFG) {
                for (int i=0; i<scale; i++)
                    for (int j=0; j<scale; j++) myDrawPixel(sx+j, sy+i, color);
            } else if (bg != color) { 
                for (int i=0; i<scale; i++)
                    for (int j=0; j<scale; j++) myDrawPixel(sx+j, sy+i, bg);
            }
        }
    }
}

void myDrawString(int x, int y, const char* str, uint16_t color, uint16_t bg, int scale) {
    while(*str) {
        myDrawChar(x, y, *str, color, bg, scale);
        x += (8 * scale);
        str++;
    }
}

// --- Rotation & Mapping Logic ---

// Map raw physical coordinates (always 320x480) to logical screen coordinates
void mapTouch(int rawX, int rawY, int *outX, int *outY) {
    // GT911/CST820 typically return 0..320 (X) and 0..480 (Y) in physical portrait
    // These values assume the touch panel is aligned natively with rotation 0
    
    switch(currentRot) {
        case 0: // Portrait
            *outX = rawX;
            *outY = rawY;
            break;
        case 1: // Landscape (90)
            *outX = rawY;
            *outY = 320 - rawX;
            break;
        case 2: // Inverted Portrait (180)
            *outX = 320 - rawX;
            *outY = 480 - rawY;
            break;
        case 3: // Inverted Landscape (270)
            *outX = 480 - rawY;
            *outY = rawX;
            break;
    }
}

void drawUI() {
    lcd.fillScreen(TFT_BLACK, DRAW_TO_RAM);
    
    // Draw Border (Visual check for correct resolution)
    myDrawRect(0, 0, lcdWidth, lcdHeight, TFT_BLUE);
    myDrawRect(5, 5, lcdWidth-10, lcdHeight-10, TFT_RED);
    
    // Draw Orientation Arrow (Up relative to screen text)
    myDrawLine(lcdWidth/2, 20, lcdWidth/2, 60, TFT_YELLOW);
    myDrawLine(lcdWidth/2, 20, lcdWidth/2 - 10, 30, TFT_YELLOW);
    myDrawLine(lcdWidth/2, 20, lcdWidth/2 + 10, 30, TFT_YELLOW);

    char buf[32];
    sprintf(buf, "ROTATION: %d", currentRot);
    myDrawString(20, 80, buf, TFT_WHITE, TFT_BLACK, 2);
    
    sprintf(buf, "RES: %dx%d", lcdWidth, lcdHeight);
    myDrawString(20, 110, buf, TFT_GREEN, TFT_BLACK, 2);
    
    myDrawString(20, lcdHeight/2, "TAP TO ROTATE", TFT_CYAN, TFT_BLACK, 2);
    
    // Draw corner dots to help verify orientation
    myDrawRect(0, 0, 10, 10, TFT_WHITE); // TL
    myDrawRect(lcdWidth-10, 0, 10, 10, TFT_RED); // TR
    myDrawRect(0, lcdHeight-10, 10, 10, TFT_GREEN); // BL
    myDrawRect(lcdWidth-10, lcdHeight-10, 10, 10, TFT_BLUE); // BR

    lcd.display();
}

void setup() {
  Serial.begin(115200);
  delay(1000); 
  Serial.println("Rotation Test Init (ex02)"); 

  lcd.begin(PANEL_DISPLAY);
  
  // Allocate MAX buffer size (always enough for 320x480 regardless of orientation)
  pFrameBuffer = (uint8_t *)ps_malloc(320 * 480 * sizeof(uint16_t));
  spilcdSetBuffer(lcd.getLCDStruct(), (uint8_t *)pFrameBuffer);
  
  if (touch.init(PANEL_TOUCH)) {
      Serial.println("Touch Init Success");
  } else {
      Serial.println("Touch Init Failed");
  }
  
  // Start at Rotation 0
  lcd.setRotation(0);
  lcdWidth = lcd.width();
  lcdHeight = lcd.height();
  
  drawUI();
}

void loop() {
    TOUCHINFO ti;
    char buf[64];
    static unsigned long lastTap = 0;

    if (touch.getSamples(&ti) && ti.count > 0) {
        int rawX = ti.x[0];
        int rawY = ti.y[0];
        int mapX, mapY;
        
        mapTouch(rawX, rawY, &mapX, &mapY);

        // Draw Marker at Mapped location
        // If mapping is correct, this appears UNDER your finger
        int r = 5;
        for(int y=mapY-r; y<=mapY+r; y++)
             for(int x=mapX-r; x<=mapX+r; x++)
                 myDrawPixel(x, y, TFT_MAGENTA);

        // Print debug info
        sprintf(buf, "R:%d,%d M:%d,%d", rawX, rawY, mapX, mapY);
        // Clear area first
        lcd.fillRect(0, lcdHeight-30, lcdWidth, 30, TFT_BLACK, DRAW_TO_RAM);
        myDrawString(10, lcdHeight-25, buf, TFT_WHITE, TFT_BLACK, 2);
        lcd.display();
        
        // Tap to Rotate logic (Debounced)
        if (millis() - lastTap > 500) {
             currentRot++;
             if (currentRot > 3) currentRot = 0;
             
             // CHANGE ROTATION
             // Note: re-allocating buffer isn't needed if we allocated max size
             lcd.fillScreen(TFT_BLACK, DRAW_TO_RAM); // Clear before rotate
             lcd.setRotation(currentRot);
             lcdWidth = lcd.width();
             lcdHeight = lcd.height();
             
             Serial.printf("Rotated to %d. New Res: %dx%d\n", currentRot, lcdWidth, lcdHeight);
             drawUI();
             
             lastTap = millis();
        }
    }
    delay(10);
}