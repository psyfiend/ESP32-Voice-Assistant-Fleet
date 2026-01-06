#pragma once
#include <Arduino.h>
#include <Wire.h>
#include "driver/i2s_std.h" // Native ESP-IDF driver
#include "driver/i2s_tdm.h" // Needed for 4-channel AEC input
#include "bsp_loader.h"     // Fleet_BSP inclusion

// Include Standard Drivers
#include "es8311.h"
#include "es7210.h"
#include "DisplayManager.h"

// --- CONFIGURATION FLAGS ---
// Uncomment this to enable 4-Channel TDM Input (Mic1/2 + Ref1/2)
#define ENABLE_AEC

#define AUDIO_CODEC_DMA_DESC_NUM 6
#define AUDIO_CODEC_DMA_FRAME_NUM 240

class AudioManager {
public:
    AudioManager();

    // --= Initialize Audio System (I2C + I2S + Codecs) =--
    bool    begin();

    // --= Volume Control (0-100%) =--
    void    setVolume       (uint8_t percent);
    int     getVolume       ();

    // --= Enable/Disable Audio Output (Mute/Unmute) =--
    void    setOutputEnable (bool on);
    void    setMute         (bool on);
    bool    getMute         ();

    // --= Utilities =--
    void    tone            (uint32_t freq, uint32_t durationMs);
    int     getMicLevel();

    // --= Loopback / Passthrough Primitives =--
    size_t  readRaw     (int16_t *data, size_t samples);
    size_t  writeRaw    (int16_t *data, size_t samples);
    
    // --= Blocking Helpers =--
    bool    recordSeconds   (int16_t* buffer, float seconds);
    bool    playSeconds     (int16_t* buffer, float seconds);

    // --= Loopback Control =--
    void    setLoopback    (bool active);
    bool    getLoopback    ();
    void    update         (); // Call this in main loop()

private:
    // Internal Config Constants (Voice Assistant Standard)
    static const uint32_t SAMPLE_RATE = 16000;
    static const uint32_t MCLK_MULTIPLE = 256; 

    // --= Internal Helpers =--
    bool    initCodecOutput(); // ES8311
    bool    initCodecInput();  // ES7210

    // --= State Variables =--
    uint8_t _defaultVolume = 70;
    uint8_t _currentVolume;
    bool _isMuted;

    // --= Loopback State =--
    bool    _isLoopbackActive = false;
    int16_t *_loopbackBuffer = nullptr;
    size_t  _loopbackChunkSize = 512; // 10ms Stereo @ 16kHz

    // --= Driver Handles =--
    es8311_handle_t     _es8311_dev;  // ES8311 Codec Handle
    i2s_chan_handle_t   _tx_handle; // Transmit (Speaker)
    i2s_chan_handle_t   _rx_handle; // Receive (Mic)
};