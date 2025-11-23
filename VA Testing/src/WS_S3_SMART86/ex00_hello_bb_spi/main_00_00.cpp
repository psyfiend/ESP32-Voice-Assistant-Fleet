#include <Arduino.h>
#include <bb_spi_lcd.h> // Include the high-performance library

// Create our LCD object
BB_SPI_LCD lcd;

#define PANEL_DISPLAY DISPLAY_WS_S3_SMART86

void setup()
{
  Serial.begin(115200);
    // Wait a moment for the serial monitor to connect
    delay(2000);
    Serial.println("--- ex00_hello_bb_spi ---");
    Serial.println("Initializing display...");

    // Initialize the display driver
    lcd.begin(PANEL_DISPLAY);

    Serial.println("Display initialized.");
    Serial.println("Starting color cycle loop...");
}

void loop()
{
    Serial.println("Filling screen RED");
    lcd.fillScreen(0xf800); // 16-bit color (565) - RED
    lcd.display();
    delay(2000);

    Serial.println("Filling screen GREEN");
    lcd.fillScreen(0x07e0); // 16-bit color (565) - GREEN
    lcd.display();
    delay(2000);
   
    Serial.println("Filling screen BLUE");
    lcd.fillScreen(0x001f); // 16-bit color (565) - BLUE
    lcd.display();
    delay(2000);
}