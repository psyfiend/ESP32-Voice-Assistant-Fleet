#include "FleetI2C.h"

#ifdef I2C_BACKEND_LEGACY
#include "driver/i2c.h"

#define FLEET_I2C_PORT        I2C_NUM_0
#define FLEET_I2C_TIMEOUT_MS  100
#define FLEET_I2C_BUF_SIZE    32

static uint8_t s_txAddr = 0;
static uint8_t s_txBuf[FLEET_I2C_BUF_SIZE];
static size_t  s_txLen = 0;

static uint8_t s_rxBuf[FLEET_I2C_BUF_SIZE];
static size_t  s_rxLen = 0;
static size_t  s_rxPos = 0;
#endif

#ifdef I2C_BACKEND_BITBANG
// Software (bit-banged) I2C over plain GPIO - no ESP-IDF I2C driver
// involved at all, either generation. Added after WS_S3_TOUCH_LCD_5B's
// GT911 touch controller failed identically on both Wire (the "i2c-ng"
// driver) and ESP-IDF's legacy driver/i2c.h (which also turned out to be
// incompatible with linking alongside Arduino's Wire support at all - see
// docs/BRINGUP_WS_S3_TOUCH_LCD_5B.md for the full story). This backend
// can't hit either of those failure categories by construction, since it
// never calls into either driver - just raw pinMode()/digitalWrite() timed
// with delayMicroseconds().
//
// Standard open-drain emulation: a line is only ever actively driven LOW
// (pinMode OUTPUT, digitalWrite LOW) or released (pinMode INPUT, letting the
// bus's own pull-up bring it high) - it is NEVER actively driven high. This
// matches real I2C electrical behavior and avoids ever fighting another
// device that might be driving the same line low.
#define FLEET_I2C_BUF_SIZE    32

static int s_sdaPin = -1;
static int s_sclPin = -1;
static uint32_t s_halfPeriodUs = 5; // ~100kHz default; begin() recalculates from freq

static uint8_t s_txAddr = 0;
static uint8_t s_txBuf[FLEET_I2C_BUF_SIZE];
static size_t  s_txLen = 0;

static uint8_t s_rxBuf[FLEET_I2C_BUF_SIZE];
static size_t  s_rxLen = 0;
static size_t  s_rxPos = 0;
static bool    s_lastAck = false; // ACK/NACK result of the most recent address/data byte

// INPUT_PULLUP, not plain INPUT: the board's own pull-ups are confirmed real
// (~4.5k, measured directly with a multimeter - see bring-up doc) but adding
// the ESP32's internal weak pull-up in parallel on every release can only
// help the rise time, never hurt it, and costs nothing.
static inline void bbSdaHigh() { pinMode(s_sdaPin, INPUT_PULLUP); }
static inline void bbSdaLow()  { pinMode(s_sdaPin, OUTPUT); digitalWrite(s_sdaPin, LOW); }
static inline void bbSclHigh() { pinMode(s_sclPin, INPUT_PULLUP); }
static inline void bbSclLow()  { pinMode(s_sclPin, OUTPUT); digitalWrite(s_sclPin, LOW); }
static inline int  bbSdaRead() { return digitalRead(s_sdaPin); }
static inline int  bbSclRead() { return digitalRead(s_sclPin); }
static inline void bbDelay()   { delayMicroseconds(s_halfPeriodUs); }

// Waits for SCL to actually read high before proceeding, up to a short
// timeout - this is what lets a slave use clock stretching (holding SCL low
// to say "wait"), which a fixed-delay-only implementation would ignore.
static void bbSclReleaseAndWaitHigh() {
    bbSclHigh();
    uint32_t start = micros();
    while (!bbSclRead()) {
        if (micros() - start > 1000) break; // 1ms clock-stretch timeout, then proceed anyway
    }
}

static void bbStart() {
    bbSdaHigh();
    bbSclHigh();
    bbDelay();
    bbSdaLow();  // SDA falls while SCL is high = START condition
    bbDelay();
    bbSclLow();
    bbDelay();
}

static void bbStop() {
    bbSdaLow();
    bbDelay();
    bbSclReleaseAndWaitHigh();
    bbDelay();
    bbSdaHigh(); // SDA rises while SCL is high = STOP condition
    bbDelay();
}

// Sends one byte MSB-first, returns true if the slave ACKed (pulled SDA low
// on the 9th clock).
static bool bbWriteByte(uint8_t data) {
    for (int i = 7; i >= 0; i--) {
        if (data & (1 << i)) bbSdaHigh(); else bbSdaLow();
        bbDelay();
        bbSclReleaseAndWaitHigh();
        bbDelay();
        bbSclLow();
    }
    // 9th clock: release SDA and check for the slave's ACK
    bbSdaHigh();
    bbDelay();
    bbSclReleaseAndWaitHigh();
    bool ack = (bbSdaRead() == LOW);
    bbDelay();
    bbSclLow();
    return ack;
}

// Reads one byte MSB-first, sends ACK (more bytes coming) or NACK (last byte).
static uint8_t bbReadByte(bool sendAck) {
    uint8_t data = 0;
    bbSdaHigh(); // release SDA so the slave can drive it
    for (int i = 7; i >= 0; i--) {
        bbDelay();
        bbSclReleaseAndWaitHigh();
        if (bbSdaRead()) data |= (1 << i);
        bbDelay();
        bbSclLow();
    }
    if (sendAck) bbSdaLow(); else bbSdaHigh();
    bbDelay();
    bbSclReleaseAndWaitHigh();
    bbDelay();
    bbSclLow();
    bbSdaHigh();
    return data;
}
#endif // I2C_BACKEND_BITBANG

static bool s_began = false;

void FleetI2C::begin(int sda, int scl, uint32_t freq) {
    if (s_began) return; // Idempotent - see header comment.
    s_began = true;

    #ifdef I2C_BACKEND_LEGACY
        i2c_config_t conf = {};
        conf.mode = I2C_MODE_MASTER;
        conf.sda_io_num = (gpio_num_t)sda;
        conf.scl_io_num = (gpio_num_t)scl;
        conf.sda_pullup_en = GPIO_PULLUP_ENABLE;
        conf.scl_pullup_en = GPIO_PULLUP_ENABLE;
        conf.master.clk_speed = freq;
        i2c_param_config(FLEET_I2C_PORT, &conf);
        i2c_driver_install(FLEET_I2C_PORT, conf.mode, 0, 0, 0);
    #elif defined(I2C_BACKEND_BITBANG)
        s_sdaPin = sda;
        s_sclPin = scl;
        // freq is a full-cycle rate; half-period in us = 1,000,000 / freq / 2.
        // Floor of 2us half-period (~250kHz max) - this is software-timed on
        // a shared core, not worth pushing faster than that reliably.
        uint32_t half = 500000UL / (freq ? freq : 100000UL);
        s_halfPeriodUs = (half < 2) ? 2 : half;
        pinMode(s_sdaPin, INPUT_PULLUP); // idle release; external/onboard pull-ups do the real work
        pinMode(s_sclPin, INPUT_PULLUP);
    #else
        Wire.begin(sda, scl);
        Wire.setClock(freq);
    #endif
}

void FleetI2C::beginTransmission(uint8_t addr) {
    #ifdef I2C_BACKEND_LEGACY
        s_txAddr = addr;
        s_txLen = 0;
    #elif defined(I2C_BACKEND_BITBANG)
        s_txAddr = addr;
        s_txLen = 0;
    #else
        Wire.beginTransmission(addr);
    #endif
}

size_t FleetI2C::write(uint8_t data) {
    #if defined(I2C_BACKEND_LEGACY) || defined(I2C_BACKEND_BITBANG)
        if (s_txLen < FLEET_I2C_BUF_SIZE) {
            s_txBuf[s_txLen++] = data;
            return 1;
        }
        return 0;
    #else
        return Wire.write(data);
    #endif
}

size_t FleetI2C::write(const uint8_t *data, size_t len) {
    #if defined(I2C_BACKEND_LEGACY) || defined(I2C_BACKEND_BITBANG)
        size_t n = 0;
        for (size_t i = 0; i < len; i++) n += write(data[i]);
        return n;
    #else
        return Wire.write(data, len);
    #endif
}

uint8_t FleetI2C::endTransmission(bool sendStop) {
    #ifdef I2C_BACKEND_LEGACY
        (void)sendStop; // legacy driver always sends STOP; no partial-transaction mode used here
        esp_err_t err = i2c_master_write_to_device(
            FLEET_I2C_PORT, s_txAddr, s_txBuf, s_txLen, pdMS_TO_TICKS(FLEET_I2C_TIMEOUT_MS)
        );
        return (err == ESP_OK) ? 0 : 4; // 0 = success, matching Wire::endTransmission()'s return convention
    #elif defined(I2C_BACKEND_BITBANG)
        (void)sendStop; // always sends STOP - no repeated-start support needed by any current caller
        bbStart();
        bool ack = bbWriteByte((uint8_t)(s_txAddr << 1)); // address + W bit
        for (size_t i = 0; ack && i < s_txLen; i++) {
            ack = bbWriteByte(s_txBuf[i]);
        }
        bbStop();
        return ack ? 0 : 2; // 0 = success, 2 = "address/data NACKed", matching Wire's convention closely enough
    #else
        return Wire.endTransmission(sendStop);
    #endif
}

uint8_t FleetI2C::requestFrom(uint8_t addr, uint8_t len) {
    #ifdef I2C_BACKEND_LEGACY
        if (len > FLEET_I2C_BUF_SIZE) len = FLEET_I2C_BUF_SIZE;
        esp_err_t err = i2c_master_read_from_device(
            FLEET_I2C_PORT, addr, s_rxBuf, len, pdMS_TO_TICKS(FLEET_I2C_TIMEOUT_MS)
        );
        s_rxLen = (err == ESP_OK) ? len : 0;
        s_rxPos = 0;
        return (uint8_t)s_rxLen;
    #elif defined(I2C_BACKEND_BITBANG)
        if (len > FLEET_I2C_BUF_SIZE) len = FLEET_I2C_BUF_SIZE;
        s_rxLen = 0;
        s_rxPos = 0;
        bbStart();
        bool ack = bbWriteByte((uint8_t)((addr << 1) | 0x01)); // address + R bit
        if (ack) {
            for (uint8_t i = 0; i < len; i++) {
                bool moreToCome = (i < (uint8_t)(len - 1));
                s_rxBuf[i] = bbReadByte(moreToCome);
            }
            s_rxLen = len;
        }
        bbStop();
        return (uint8_t)s_rxLen;
    #else
        return Wire.requestFrom(addr, len);
    #endif
}

int FleetI2C::available() {
    #if defined(I2C_BACKEND_LEGACY) || defined(I2C_BACKEND_BITBANG)
        return (int)(s_rxLen - s_rxPos);
    #else
        return Wire.available();
    #endif
}

int FleetI2C::read() {
    #if defined(I2C_BACKEND_LEGACY) || defined(I2C_BACKEND_BITBANG)
        if (s_rxPos < s_rxLen) return s_rxBuf[s_rxPos++];
        return -1;
    #else
        return Wire.read();
    #endif
}

bool FleetI2C::test(uint8_t addr) {
    #ifdef I2C_BACKEND_LEGACY
        uint8_t dummy = 0;
        esp_err_t err = i2c_master_write_to_device(FLEET_I2C_PORT, addr, &dummy, 0, pdMS_TO_TICKS(10));
        return err == ESP_OK;
    #elif defined(I2C_BACKEND_BITBANG)
        bbStart();
        bool ack = bbWriteByte((uint8_t)(addr << 1));
        bbStop();
        return ack;
    #else
        Wire.beginTransmission(addr);
        return Wire.endTransmission(true) == 0;
    #endif
}

const char* FleetI2C::backendName() {
    #ifdef I2C_BACKEND_LEGACY
        return "Legacy (driver/i2c.h)";
    #elif defined(I2C_BACKEND_BITBANG)
        return "Bit-banged (software)";
    #else
        return "Wire";
    #endif
}
