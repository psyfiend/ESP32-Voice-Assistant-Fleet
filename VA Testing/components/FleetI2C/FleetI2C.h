#pragma once
#include <Arduino.h>
#include <Wire.h>

// Uniform I2C access for every board in the fleet, regardless of which
// underlying ESP-IDF I2C driver a given board's bus actually needs.
//
// Every board, including WS_S3_TOUCH_LCD_5B, currently uses the default Wire
// backend (built on ESP-IDF's newer "i2c-ng" driver generation - see
// esp32-hal-i2c-ng.c). Two alternate backends exist and are kept available -
// ESP-IDF's legacy driver/i2c.h (I2C_BACKEND_LEGACY) and bit-banged software
// I2C over plain GPIO (I2C_BACKEND_BITBANG) - built while chasing a GT911
// touch bug on WS_S3_TOUCH_LCD_5B that turned out to be an unrelated address
// collision in bb_captouch's chip-detection logic, not an I2C driver issue at
// all (see docs/BRINGUP_WS_S3_TOUCH_LCD_5B.md). Neither alternate backend is
// active on any board today, but both are real, working code, not dead ends.
//
// Consumers (DisplayManager, TouchManager/bb_captouch, and any future
// sensor driver) call this API and never need to know or care which backend
// is active - the board-specific #ifdef lives in FleetI2C.cpp, exactly
// once, not scattered through every file that touches I2C.
//
// begin() is idempotent: safe to call from multiple managers during startup
// (DisplayManager and TouchManager both need the bus up before the other is
// necessarily done) - only the first call does anything; later calls are
// silently ignored. This is what actually prevents the "re-init an
// already-active bus leaves the driver in a bad state" class of bug, for
// either backend, rather than requiring every caller to coordinate who's
// allowed to initialize the bus.
class FleetI2C {
public:
    static void begin(int sda, int scl, uint32_t freq = 400000);

    static void beginTransmission(uint8_t addr);
    static size_t write(uint8_t data);
    static size_t write(const uint8_t *data, size_t len);
    static uint8_t endTransmission(bool sendStop = true);

    static uint8_t requestFrom(uint8_t addr, uint8_t len);
    static int available();
    static int read();

    // Single-shot address probe (ACK/NACK) - matches how bb_captouch's own
    // chip-detection scan, and the boot-time I2C bus scan in
    // LVGL_Test_UI.cpp, already use Wire today.
    static bool test(uint8_t addr);

    // Which backend is actually compiled in for this board - "Wire",
    // "Legacy (driver/i2c.h)", or "Bit-banged (software)". Worth logging
    // wherever a board reports its own hardware config, now that more than
    // one backend genuinely exists in the tree.
    static const char* backendName();
};
