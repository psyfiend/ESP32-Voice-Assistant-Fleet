#include <Arduino.h>
#include <bb_spi_lcd.h> // Include the high-performance library

// --- 1. Portability Defines ---
#define PANEL_TYPE    DISPLAY_CYD_535 
// #define SCREEN_WIDTH  320             // <--- REMOVED
// #define SCREEN_HEIGHT 480             // <--- REMOVED
#define TEST_DELAY    2000            // 2-second pause

// Create our LCD object
BB_SPI_LCD lcd;

// Define our framebuffer pointer.
uint16_t *pFrameBuffer;

// --- Dynamic Screen Dimensions ---
// We'll populate these in setup() after lcd.begin()
int lcdWidth;  // <--- ADDED
int lcdHeight; // <--- ADDED

void alignment_tests(BB_SPI_LCD *pLCD) {
    Serial.println("Running Alignment Tests...");

    // Get the SPILCD struct
    SPILCD *lcdStruct = pLCD->getLCDStruct();

    // Print SPILCD struct values
    Serial.println("SPILCD Debug Info:");
    Serial.print("iOffset: ");
    Serial.println(lcdStruct->iOffset);
    Serial.print("iWindowX: ");
    Serial.println(lcdStruct->iWindowX);
    Serial.print("iWindowY: ");
    Serial.println(lcdStruct->iWindowY);
    Serial.print("iScreenPitch: ");
    Serial.println(lcdStruct->iScreenPitch);

    // Check back buffer alignment
    Serial.print("BackBuffer Address: ");
    Serial.println((uintptr_t)lcdStruct->pBackBuffer, HEX);
    if ((uintptr_t)lcdStruct->pBackBuffer % 4 == 0) {
        Serial.println("BackBuffer is 32-bit aligned.");
    } else if ((uintptr_t)lcdStruct->pBackBuffer % 2 == 0) {
        Serial.println("BackBuffer is 16-bit aligned.");
    } else {
        Serial.println("BackBuffer is not aligned.");
    }

    Serial.println("Alignment Tests Completed.");
}


// --- GFX Primitives Test Function ---
// This draws one of everything from the Adafruit_GFX library
// to our framebuffer.
void runGfxTests() {
  
  // --- Test 1: Direct-to-LCD (GFX "teaspoon" calls will fail) ---
  Serial.println("\n--- Test 1: Direct-to-LCD Mode (DRAW_TO_LCD) ---");
  lcd.setPrintFlags(DRAW_TO_LCD); // Set to direct-to-LCD mode
  
  Serial.println("Calling fillScreen(TFT_BLACK)... (This is a driver-level call and WILL work)");
  lcd.fillScreen(TFT_BLACK); // This works because it's a 'dump truck' op
  delay(TEST_DELAY);

  Serial.println("Calling GFX functions (drawRect, print)...");
  lcd.drawRect(10, 10, 100, 100, TFT_RED);
  lcd.setCursor(10, 120);
  lcd.setTextColor(TFT_WHITE);
  lcd.setTextSize(2);
  lcd.print("This text will\nNOT appear!");
  Serial.println("GFX calls sent. Screen will NOT update.");
  Serial.println("This proves GFX functions can't draw 1-pixel commands in direct mode.");
  delay(TEST_DELAY);

  // --- Test 2: DRAW_TO_RAM (GFX "teaspoon" calls now work) ---
  Serial.println("\n--- Test 2: Buffered Mode (DRAW_TO_RAM) ---");
  lcd.setPrintFlags(DRAW_TO_RAM); // Set to buffered mode
  
  Serial.println("Calling fillScreen(TFT_BLUE)... (draws to LCD))");
  lcd.fillScreen(TFT_BLUE, DRAW_TO_LCD); // This now draws to PSRAM
  
  // Serial.println("Calling lcd.display()...");
  // lcd.display(); // Push buffer to screen
  Serial.println("Screen should now be BLUE.");
  delay(TEST_DELAY);

  Serial.println("Calling all GFX functions... (draws to buffer)");
  // Clear buffer first
  lcd.fillScreen(TFT_BLACK, DRAW_TO_LCD);

  // --- Lines ---
  lcd.drawLine(10, 150, 110, 40, TFT_BLUE); 
  lcd.display(10, 40, 100, 110); // Update only the line area

  // --- Rects ---
  lcd.drawRect(10, 160, 50, 50, TFT_CYAN);   
  lcd.display(10, 160, 50, 50); // Update only the rectangle area

  lcd.fillRect(70, 160, 50, 50, TFT_CYAN);   
  lcd.display(70, 160, 50, 50); // Update only the filled rectangle area

  // --- Rounded Rects ---
  lcd.drawRoundRect(10, 220, 50, 50, 10, TFT_MAGENTA); 
  lcd.display(10, 220, 50, 50); // Update only the rounded rectangle area

  lcd.fillRoundRect(70, 220, 50, 50, 10, TFT_MAGENTA); 
  lcd.display(70, 220, 50, 50); // Update only the filled rounded rectangle area

  // --- Circles ---
  lcd.drawCircle(170, 65, 25, TFT_YELLOW);  
  lcd.display(145, 40, 50, 50); // Update only the circle area

  lcd.fillCircle(230, 65, 25, TFT_YELLOW);  
  lcd.display(205, 40, 50, 50); // Update only the filled circle area
  
  // --- Triangles ---
  lcd.drawTriangle(170, 140, 195, 115, 220, 140, TFT_ORANGE); 
  lcd.display(170, 115, 50, 25); // Update only the triangle area

  lcd.fillTriangle(230, 140, 255, 115, 280, 140, TFT_ORANGE); 
  lcd.display(230, 115, 50, 25); // Update only the filled triangle area

  // --- Text (Standard GFX) ---
  lcd.setCursor(10, 290);
  lcd.setTextColor(TFT_WHITE);
  lcd.setTextSize(2);
  lcd.println("This text\nWILL appear!");
  lcd.display(10, 290, 200, 50); // Update only the text area

  // Serial.println("GFX drawn to buffer. Calling lcd.display()...");
  // lcd.display(); // Push all GFX to the screen at once
  Serial.println("All previous updates drawn and updated individually.");
  delay(TEST_DELAY);

  // --- Test 3: Per-Call Flag (Advanced) ---
  Serial.println("\n--- Test 3: Per-Call Flag Test ---");
  lcd.setPrintFlags(DRAW_TO_LCD); // Back to direct-to-LCD mode
  delay(TEST_DELAY);

  Serial.println("Calling fillScreen(TFT_GREY)... (direct mode, will work)"); 
  lcd.fillScreen(TFT_GREY, DRAW_TO_LCD); 
  delay(TEST_DELAY);
  
  Serial.println("Calling fillRect with DRAW_TO_RAM flag...");
  // This is the overloaded function you spotted. It will draw to the buffer.
  // Now uses our dynamic width/height variables!
  lcd.fillRect(lcdWidth/2 - 25, lcdHeight/2 - 25, 50, 50, TFT_MAGENTA, DRAW_TO_RAM); // <--- UPDATED
  Serial.println("...drew to buffer, but screen is unchanged.");
  delay(TEST_DELAY);
  
  Serial.println("Calling lcd.display() only on updated area");
  lcd.display(lcdWidth/2 - 25, lcdHeight/2 - 25, 50, 50); // Push the buffer (which only contains the purple square)
  Serial.println("Magenta square should now be visible on grey background."); 
  delay(TEST_DELAY);
}

void setup() {
  Serial.begin(115200);
  delay(1000); // Give serial a moment
  Serial.println("ex01_01_main.cpp: GFX Primitives & Draw Mode Test"); 
  delay(TEST_DELAY);

  // 1. Initialize the display
  lcd.begin(PANEL_TYPE); 
  Serial.println("lcd.begin() complete.");
  delay(TEST_DELAY);

  // 2. Get dynamic screen dimensions *after* begin()
  lcdWidth = lcd.width();   // <--- ADDED
  lcdHeight = lcd.height(); // <--- ADDED
  Serial.printf("Screen dimensions retrieved: %d x %d\n", lcdWidth, lcdHeight);
  delay(TEST_DELAY);
  
  // 3. Allocate the framebuffer in PSRAM
  pFrameBuffer = (uint16_t *)ps_malloc(lcdWidth * lcdHeight * sizeof(uint16_t)); // <--- UPDATED

  if (pFrameBuffer == NULL) {
    Serial.println("FATAL: Failed to allocate framebuffer in PSRAM!");
    while(1); // Halt
  } else {
    Serial.printf("Framebuffer allocated in PSRAM at %p\tSize: %d bytes\n", pFrameBuffer, (lcdWidth * lcdHeight * sizeof(uint16_t)));
  }
  delay(TEST_DELAY);

  if ((uintptr_t)pFrameBuffer % 4 != 0) {
    Serial.println("Buffer is not 4-byte aligned!");
    while (1); // Halt
}

  // 4. CRITICAL: Tell the driver *where* our buffer is
  spilcdSetBuffer(lcd.getLCDStruct(), (uint8_t *)pFrameBuffer);
  Serial.println("spilcdSetBuffer() complete.");
  delay(TEST_DELAY);

  // Note: We set the print flags inside the test function now
  Serial.println("Setup complete. Running tests...");
  
  // alignment_tests(&lcd);
}

void loop() {
  runGfxTests();
  Serial.println("All tests finished.");
  delay(5000);
}