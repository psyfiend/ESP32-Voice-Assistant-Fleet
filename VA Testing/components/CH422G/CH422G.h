#pragma once
#include <Arduino.h>
#include <Wire.h>

// Minimal hand-rolled driver for the WCH CH422G IO expander.
//
// NOT protocol-compatible with the XCA9554 that Arduino_XCA9554SWSPI (used by
// WS_S3_SMART86) expects. CH422G has no single addressable register file -
// it responds on four fixed 7-bit pseudo-addresses, one per register, and
// ignores any I2C address you configure it with. See the datasheet or
// Waveshare's esp_io_expander_ch422g.c (ESP32_IO_Expander lib) for the
// reference implementation this was ported from.
//
// Pin map (matches Waveshare's esp_expander::CH422G numbering):
//   0-7  = IO0-IO7  - bidirectional, but direction is set for the WHOLE bank
//                      at once (CH422G has no per-pin direction control).
//   8-11 = OC0-OC3  - dedicated open-drain/push-pull outputs, always output.
class CH422G {
public:
    enum Direction { IO_BANK_OUTPUT, IO_BANK_INPUT };

    void begin();                              // Resets regs, IO0-7 default to output-high
    void setIOBankDirection(Direction dir);    // Only affects pins 0-7
    void digitalWrite(uint8_t pin, uint8_t level); // pin 0-11
    uint8_t digitalRead(uint8_t pin);          // pin 0-7 only, meaningful when IO_BANK_INPUT

private:
    // Fixed pseudo-addresses (7-bit, already shifted down from the datasheet's 8-bit form)
    static constexpr uint8_t REG_WR_SET = 0x24; // 0x48 >> 1 - direction / mode control
    static constexpr uint8_t REG_WR_OC  = 0x23; // 0x46 >> 1 - OC0-3 output latch
    static constexpr uint8_t REG_WR_IO  = 0x38; // 0x70 >> 1 - IO0-7 output latch
    static constexpr uint8_t REG_RD_IO  = 0x26; // 0x4D >> 1 - IO0-7 input read

    static constexpr uint8_t BIT_IO_OE = 1 << 0; // 1 = IO0-7 output mode, 0 = input mode

    uint8_t _wr_set = 0x01; // IO_OE=1 (output) by reset default
    uint8_t _wr_oc  = 0x0F;
    uint8_t _wr_io  = 0xFF;

    void writeReg(uint8_t reg7, uint8_t data);
};
