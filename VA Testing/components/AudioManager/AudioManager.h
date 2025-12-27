#pragma once
#include <Arduino.h>
#include <Wire.h>
#include "driver/i2s_std.h" // Native ESP-IDF driver for P4 stability
#include "bsp_loader.h"  // Fleet_BSP inclusion

// Include Standard Drivers
#include "es8311.h"
#include "es7210.h"
#include "DisplayManager.h"

class AudioManager {
public:
    AudioManager();

    // Initialize Audio System (I2C + I2S + Codecs)
    bool begin();

    // Volume Control (0-100%)
    void setVolume(uint8_t percent);
    int getVolume();

    // Enable/Disable Audio Output (Mute/Unmute)
    void setOutputEnable(bool on);
    // Soft Mute Control
    void setMute(bool on);
    bool getMute();

    // Play a simple test tone
    void tone(uint32_t freq, uint32_t durationMs);

    // Read Microphone Level (RMS 1-100)
    int getMicLevel();

    // Loopback Primitives
    size_t readRaw(int16_t *data, size_t samples);
    size_t writeRaw(int16_t *data, size_t samples);
    
    // Blocking Helpers
    bool recordSeconds(int16_t* buffer, float seconds);
    bool playSeconds(int16_t* buffer, float seconds);

private:
    // --- Internal Helpers ---
    // Initializations are handled by the respective codec drivers
    // These wrapper functions configure the specific structs
    bool initCodecOutput(); // ES8311
    bool initCodecInput();  // ES7210

    // Low-level I2C register access
    // --- UPDATED: Added overload to support specific device addresses (e.g. 0x41) ---
    void writeReg(uint8_t reg, uint8_t value); // Legacy (uses default ES8311 addr)
    void writeReg(uint8_t deviceAddr, uint8_t reg, uint8_t value); // New generic
    void readReg (uint8_t reg, uint8_t &value);

    // State Variables
    uint8_t _defaultVolume = 70;
    uint8_t _currentVolume;
    bool _isMuted;

    // Driver Handles
    es8311_handle_t _es8311_dev;  // ES8311 Codec Handle
    
    // I2S Handle (ESP-IDF style)
    i2s_chan_handle_t _tx_handle; // Transmit (Speaker)
    i2s_chan_handle_t _rx_handle; // Receive (Mic)
};