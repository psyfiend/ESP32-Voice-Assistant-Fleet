#include "CH422G.h"
#include <FleetI2C.h>

void CH422G::writeReg(uint8_t reg7, uint8_t data) {
    FleetI2C::beginTransmission(reg7);
    FleetI2C::write(data);
    FleetI2C::endTransmission();
}

void CH422G::begin() {
    // Reset to the chip's power-on defaults (IO0-7 output-high, OC0-3 high)
    _wr_set = 0x01;
    _wr_oc  = 0x0F;
    _wr_io  = 0xFF;
    writeReg(REG_WR_SET, _wr_set);
    writeReg(REG_WR_OC, _wr_oc);
    writeReg(REG_WR_IO, _wr_io);
}

void CH422G::setIOBankDirection(Direction dir) {
    if (dir == IO_BANK_OUTPUT) {
        _wr_set |= BIT_IO_OE;
    } else {
        _wr_set &= ~BIT_IO_OE;
    }
    writeReg(REG_WR_SET, _wr_set);
    if (dir == IO_BANK_INPUT) {
        delay(2); // Let the expander settle into input mode, per reference driver
    }
}

void CH422G::digitalWrite(uint8_t pin, uint8_t level) {
    if (pin <= 7) {
        if (level) _wr_io |= (1 << pin);
        else       _wr_io &= ~(1 << pin);
        writeReg(REG_WR_IO, _wr_io);
    } else if (pin <= 11) {
        uint8_t bit = pin - 8;
        if (level) _wr_oc |= (1 << bit);
        else       _wr_oc &= ~(1 << bit);
        writeReg(REG_WR_OC, _wr_oc);
    }
}

uint8_t CH422G::digitalRead(uint8_t pin) {
    if (pin > 7) return 0;
    FleetI2C::requestFrom(REG_RD_IO, 1);
    if (FleetI2C::available()) {
        uint8_t val = FleetI2C::read();
        return (val >> pin) & 0x01;
    }
    return 0;
}
