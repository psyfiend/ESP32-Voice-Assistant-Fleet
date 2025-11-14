/*
 * ex00_hello_bb_spi
 *
 * Target: Guition S3 3.5" (JC3248W535)
 *
 * Purpose: A minimal "Hello, Screen!" test to verify the bb_spi_lcd
 * driver, hardware connections, and display controller (AXS15231B).
 *
 * This test does NOT use LVGL.
 * It only initializes the display and fills the screen with solid
 * colors (Red, Green, Blue) in a loop.
 */

#include <Arduino.h>
#include <bb_spi_lcd.h> // Our "secret weapon" driver

#define PANEL DISPLAY_CYD_535 // Guition 3.5" AXS15231B

// Instantiate the driver object
BB_SPI_LCD lcd;

void setup()
{
    Serial.begin(115200);
    // Wait a moment for the serial monitor to connect
    delay(2000);
    Serial.println("--- ex00_hello_bb_spi ---");
    Serial.println("Initializing display...");

    // Initialize the display driver
    lcd.begin(PANEL);

    Serial.println("Display initialized.");
    Serial.println("Starting color cycle loop...");
}

void loop()
{
    Serial.println("Filling screen RED");
    lcd.fillScreen(0xf800); // 16-bit color (565) - RED
    delay(2000);

    Serial.println("Filling screen GREEN");
    lcd.fillScreen(0x07e0); // 16-bit color (565) - GREEN
    delay(2000);

    Serial.println("Filling screen BLUE");
    lcd.fillScreen(0x001f); // 16-bit color (565) - BLUE
    delay(2000);
}