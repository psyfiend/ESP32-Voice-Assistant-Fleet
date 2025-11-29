//
// main_00_00.cpp
//
// Hello World for Waveshare ESP32-S3-Touch-LCD-4 (Smart86)
//
// THE "TROJAN HORSE" STRATEGY:
// 1. Initialize TCA9554 IO Expander via I2C.
// 2. Manually bit-bang the 3-wire SPI init sequence to ST7701S via Expander.
// 3. Call lcd.begin() to start the high-speed RGB driver only.
//
// Engineer: Gemini
// Architect: Eric
//

#include <Arduino.h>
#include <Wire.h>
#include <bb_spi_lcd.h>

// -------------------------------------------------------------------------
// 1. I2C / IO Expander Configuration
// -------------------------------------------------------------------------
// Based on Waveshare S3 layouts, I2C for Touch/Audio/Expander is often on 15/7
#define I2C_SDA_PIN 47
#define I2C_SCL_PIN 48
#define TCA9554_ADDR 0x20

#define PANEL_DISPLAY DISPLAY_WS_S3_SMART86

// Expander Pin Mappings (From your snippet: rst=7, cs=0, sck=2, mosi=1)
#define EXP_RST   7   // P7
#define EXP_CS    0   // P0
#define EXP_SCK   2   // P2
#define EXP_MOSI  1   // P1

// Assuming Backlight is on the expander too, commonly P6 or similar, 
// but let's try enabling ALL unused pins just in case.


static uint8_t ioRegs[8] = {0}; // Cache for TCA9554 registers

// Initialize TCA9554 Expander

void tca_init() {
    Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);
    Wire.setClock(100000); // 100kHz I2C

    // Configure all pins as outputs (0)
    Wire.beginTransmission(TCA9554_ADDR);
    Wire.write(0x03); // Output Port Register
    Wire.write(0x00); // All low
    Wire.endTransmission();

    Serial.println("TCA9554 Initialized");
}

// Set specific pin mode

void tca_mode(uint8_t pin, uint8_t mode) {
    const uint8_t port = pin / 8;

    pin &= 7; // Pin within port
    if (mode == INPUT) {
        ioRegs[6 + port] |= (1 << pin); // Set as input
    } else {
        ioRegs[6 + port] &= ~(1 << pin); // Set as output
    }
    // Write back to config register
    Wire.beginTransmission(TCA9554_ADDR);
    Wire.write(6 + port); // Config Register
    Wire.write(ioRegs[6 + port]);
    Wire.endTransmission();
}

// Write digital value to specific pin

void tca_write(uint8_t pin, uint8_t value) {
    const uint8_t port = pin / 8;

    pin &= 7; // Pin within port
    if (value) {
        ioRegs[2 + port] |= (1 << pin); // Set High
    } else {
        ioRegs[2 + port] &= ~(1 << pin); // Set Low
    }
    // Write back to output register
    Wire.beginTransmission(TCA9554_ADDR);
    Wire.write(2 + port); // Output Port Register
    Wire.write(ioRegs[2 + port]);
    Wire.endTransmission();
}

// -------------------------------------------------------------------------
// 2. 3-Wire SPI Bit-Banging (via Expander)
// -------------------------------------------------------------------------
// ST7701S 9-bit SPI: D/C bit + 8 Data bits. Rising Edge Clock.

void screenInit_BitBang_SPI(const uint8_t *init_data) {

    Wire.end(); // Release I2C for bit-banging

    if (!Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN)) {
        Serial.println("I2C Init Failed for Bit-Bang SPI");
        return;
    } else {
        Serial.println("I2C Re-Initialized for Bit-Bang SPI");
    }

    Wire.setClock(100000); // 100kHz I2C for faster expander access

    unsigned long timer = millis();
    
    const uint8_t RST = EXP_RST;
    const uint8_t MOSI = EXP_MOSI;
    const uint8_t SCK = EXP_SCK;
    const uint8_t CS = EXP_CS;

    // Configure pins as output
    tca_mode(MOSI, OUTPUT);
    tca_mode(SCK, OUTPUT);
    tca_mode(CS, OUTPUT);
    tca_mode(RST, OUTPUT);

    tca_write(RST, HIGH);
    delay(10);
    tca_write(RST, LOW);
    delay(10);
    tca_write(RST, HIGH);
    delay(10);

    // Set initial pin states
    tca_write(CS, HIGH);
    tca_write(SCK, LOW);
    tca_write(MOSI, LOW);

    while (*init_data) {
        uint8_t len = *init_data++;
        if (len == LCD_DELAY) {
            delay(*init_data++);
            continue;
        }
        uint8_t cmd = *init_data++;

        tca_write(CS, LOW); // Start SPI transaction

        // DC LOW for command
        tca_write(SCK, LOW);
        tca_write(MOSI, LOW); // DC = 0
        // delayMicroseconds(1); // Short delay if needed
        tca_write(SCK, HIGH);

        // Send command byte
        for (uint8_t i = 7; i >= 0; i--) {
            tca_write(SCK, LOW);
            tca_write(MOSI, (cmd >> i) & 0x01);
            // delayMicroseconds(1); // Short delay if needed
            tca_write(SCK, HIGH);
            // delayMicroseconds(1); // Short delay if needed
        }

        // Send data bytes
        for (uint8_t i = 0; i < len - 1; i++) {
            uint8_t byte = *init_data++;
            
            // DC HIGH for data
            tca_write(SCK, LOW);
            tca_write(MOSI, HIGH); // DC = 1
            // delayMicroseconds(1); // Short delay if needed
            tca_write(SCK, HIGH);

            for (uint8_t j = 7; j >= 0; j--) {
                tca_write(SCK, LOW);
                tca_write(MOSI, (byte >> j) & 0x01);
                // delayMicroseconds(1); // Short delay if needed
                tca_write(SCK, HIGH);
                // delayMicroseconds(1); // Short delay if needed
            }
        } // For each data byte

        tca_write(CS, HIGH); // End SPI transaction
    }
    Serial.print("Bit-Bang SPI Init Completed in ");
    Serial.print(millis() - timer);

    Wire.end();
}

const uint8_t ST7701_init_sequence[] = {
    1, 0x11,
    LCD_DELAY, 120,
    6, 0xFF, 0x77, 0x01, 0x00, 0x00, 0x10,
    3, 0xC0, 0x3B, 0x00,
    3, 0xC1, 0x0D, 0x02,
    3, 0xC2, 0x21, 0x08,  // type1 0x31, 0x05    // UM-WS 0xC2, 0x21, 0x08
    2, 0xCD, 0x08,

    // Positive Voltage Gamma Control
    17, 0xB0, 0x00, 0x11, 0x18, 0x0E, 0x11, 0x06, 0x07, 0x08, 0x07, 0x22, 0x04, 0x12, 0x0F, 0xAA, 0x31, 0x18,
    // Negative Voltage Gamma Control
    17, 0xB1, 0x00, 0x11, 0x19, 0x0E, 0x12, 0x07, 0x08, 0x08, 0x08, 0x22, 0x04, 0x11, 0x11, 0xA9, 0x32, 0x18,

    6, 0xFF, 0x77, 0x01, 0x00, 0x00, 0x11,
    2, 0xB0, 0x60,  // Vop=4.7375v
    2, 0xB1, 0x30,	// VCOM=32  // type1 0x32    // UM-WS 0x30
    2, 0xB2, 0x87,	// VGH=15v  // type1 0x07    // UM-WS 0x87
    2, 0xB3, 0x80,
    2, 0xB5, 0x49,  // VGL=-10.17v
    2, 0xB7, 0x85,
    2, 0xB8, 0x21,  // AVDD=6.6 & AVCL=-4.6
    2, 0xC1, 0x78,
    2, 0xC2, 0x78,
    LCD_DELAY, 20, 
    4, 0xE0, 0x00, 0x1B, 0x02,
    12, 0xE1, 0x08, 0xA0, 0x00, 0x00, 0x07, 0xA0, 0x00, 0x00, 0x00, 0x44, 0x44,
    13, 0xE2, 0x11, 0x11, 0x44, 0x44, 0xED, 0xA0, 0x00, 0x00, 0xEC, 0xA0, 0x00, 0x00,
    5, 0xE3, 0x00, 0x00, 0x11, 0x11,
    3, 0xE4, 0x44, 0x44,
    17, 0xE5, 0x0A, 0xE9, 0xD8, 0xA0, 0x0C, 0xEB, 0xD8, 0xA0, 0x0E, 0xED, 0xD8, 0xA0, 0x10, 0xEF, 0xD8, 0xA0,
    5, 0xE6, 0x00, 0x00, 0x11, 0x11,
    3, 0xE7, 0x44, 0x44,
    17, 0xE8, 0x09, 0xE8, 0xD8, 0xA0, 0x0B, 0xEA, 0xD8, 0xA0, 0x0D, 0xEC, 0xD8, 0xA0, 0x0F, 0xEE, 0xD8, 0xA0,
    8, 0xEB, 0x02, 0x00, 0xE4, 0xE4, 0x88, 0x00, 0x40,
    3, 0xEC, 0x3C, 0x00,
    17, 0xED, 0xAB, 0x89, 0x76, 0x54, 0x02, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x20, 0x45, 0x67, 0x98, 0xBA,
    //-----------VAP & VAN---------------
    6, 0xFF, 0x77, 0x01, 0x00, 0x00, 0x00,
    2, 0x36, 0x00,  // Rotation?
    2, 0x3A, 0x66,  // 0x70 RGB888, 0x60 RGB666, 0x50 RGB565		//type1 0x60  // UM-WS 0x66
    1, 0x21,        // Inversion On	// Sleep out    // 0x20 normal, 0x21 IPS
    // 1, 0x11,     // type1
    LCD_DELAY, 120,	// Sleep out
    1, 0x29, // Display On
    LCD_DELAY, 120,
    0
};

// -------------------------------------------------------------------------
// 3. Main Application
// -------------------------------------------------------------------------

BB_SPI_LCD lcd;

void setup() {
    Serial.begin(115200);
    delay(2000);
    Serial.println();
    Serial.println("--- Waveshare Smart86 (Trojan Horse Init) ---");
    Serial.println();

    // A. Initialize Expander
    // Serial.println("Init TCA9554...");
    tca_init();

    // B. Initialize Screen via Bit-Bang SPI
    Serial.println("Init Screen via Bit-Bang SPI...");
    screenInit_BitBang_SPI(ST7701_init_sequence);

    // C. Start RGB Driver (Library)
    // Pins are assigned inside the library for this board
    Serial.println("Starting bb_spi_lcd...");
    Serial.println();
    lcd.begin(PANEL_DISPLAY);

    // D. Allocate Buffer
    Serial.println("Allocating Buffer...");
    Serial.println();
    int buffer = lcd.allocBuffer();

    if (buffer == -2) {
        Serial.println("allocBuffer() already alloc!");
    } else if (buffer == -1) {
        Serial.println("allocBuffer() failed!");
        while (1)
            ;
    } else {
        Serial.println("allocBuffer() success!");
    }
    Serial.println();
}

void loop() {
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