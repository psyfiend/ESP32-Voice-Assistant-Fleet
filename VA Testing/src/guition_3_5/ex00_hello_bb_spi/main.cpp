#include <Arduino.h>
#include <bb_spi_lcd.h> // Include the high-performance library

// Create our LCD object
BB_SPI_LCD lcd;

// Our screen's dimensions
#define SCREEN_WIDTH 320
#define SCREEN_HEIGHT 480

// Define our framebuffer pointer.
// We will allocate this in PSRAM.
uint16_t *pFrameBuffer;

void setup() {
  Serial.begin(115200);
  delay(1000); // Give serial a moment
  Serial.println("bb_spi_lcd basic GFX test... V4 (The Correct One!)"); // <--- UPDATED

  // 1. Allocate the framebuffer in PSRAM
  pFrameBuffer = (uint16_t *)ps_malloc(SCREEN_WIDTH * SCREEN_HEIGHT * sizeof(uint16_t));

  if (pFrameBuffer == NULL) {
    Serial.println("FATAL: Failed to allocate framebuffer in PSRAM!");
    while(1); // Halt
  } else {
    Serial.printf("Framebuffer allocated in PSRAM at %p\n", pFrameBuffer);
  }

  // 2. Initialize the display
  lcd.begin(DISPLAY_CYD_535);
  Serial.println("lcd.begin() complete.");

  // if (!lcd.allocBuffer()) { // unable to allocate a buffer
  // lcd.setTextColor(TFT_RED);
  // lcd.print("allocBuffer() failed!");
  // Serial.println("allocBuffer() failed!");
  // while (1) {}; // stop
  // }

  // --- Test 1: Direct-to-LCD (Driver-Level) ---
  Serial.println("Test 1: Direct-to-LCD fillScreen (TFT_RED)...");
  lcd.fillScreen(TFT_RED);
  Serial.println("Test 1: Complete.");
  delay(1000);

  // --- Test 2: GFX-to-RAM (Buffered) ---
  Serial.println("Test 2: Configuring for Buffered GFX mode...");

  // 3. CRITICAL: Tell the driver *where* our buffer is
  spilcdSetBuffer(lcd.getLCDStruct(), (uint8_t *)pFrameBuffer);
  Serial.println("spilcdSetBuffer() complete.");

  // 4. CRITICAL: Tell the GFX layer *to use* that buffer
  // This is the global setter function you correctly identified.
  lcd.setPrintFlags(DRAW_TO_RAM); // <--- UPDATED (The real fix!)
  Serial.println("lcd.setPrintFlags(DRAW_TO_RAM) complete.");

  Serial.println("Drawing GFX functions to buffer...");
  // Now, all GFX calls will draw to PSRAM
  lcd.fillScreen(TFT_BLACK); // This now draws to the *buffer*
  
  lcd.drawRect(10, 10, 100, 150, TFT_WHITE);
  lcd.drawLine(10, 10, 110, 160, TFT_GREEN); 
  lcd.setCursor(20, 175);
  lcd.setTextColor(TFT_WHITE);
  lcd.setTextSize(2);
  lcd.print("Hello, Architect!");
  lcd.setTextSize(1);
  lcd.setCursor(20, 200);
  lcd.print("GFX is working!");

  // 5. CRITICAL: Push the buffer (with GFX) to the display
  Serial.println("Pushing buffer to display with lcd.display()...");
  lcd.display();
  Serial.println("Test 2: Complete.");
  delay(2000);

  // --- Test 3: Prove the mode-switch works ---
  Serial.println("Test 3: Switching back to Direct-to-LCD...");
  // We switch back by setting the flags to the default (FLAGS_NONE)
  lcd.setPrintFlags(FLAGS_NONE); // <--- UPDATED
  Serial.println("Drawing TFT_BLUE direct...");
  lcd.fillScreen(TFT_BLUE); // This should work immediately, no display() needed
  Serial.println("Test 3: Complete.");
  delay(1000);
   
  Serial.println("All tests finished.");
}

void loop() {
  // Nothing to do here
  delay(1000);
}