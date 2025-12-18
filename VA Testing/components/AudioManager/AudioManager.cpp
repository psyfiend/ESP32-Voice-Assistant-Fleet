#include "AudioManager.h"

// I2C Port used by the ESP-IDF drivers (usually 0 for the default Wire)
#define I2C_PORT_NUM 0

AudioManager::AudioManager() {
    _tx_handle = NULL;
    _rx_handle = NULL;
    _currentVolume = 0;
    _defaultVolume = 50;
    _isMuted = false;
    _es8311_dev = NULL;
}

bool AudioManager::begin() {

    Serial.println("------------------------------");
    Serial.println("[AudioMgr] Initializing...");

    // 1. Initialize Power Amp Pin (Direct GPIO Only)
    // If HAS_IO_EXPANDER is defined (S3 Smart86), this block is skipped entirely.
    // The Expander logic is handled by DisplayManager in setup().
    // If NOT defined (P4), we configure the pin if it's valid.
    #ifndef HAS_IO_EXPANDER
    if (hw_cfg.I2S_AMP_EN != -1) {
        pinMode(hw_cfg.I2S_AMP_EN, OUTPUT);
        digitalWrite(hw_cfg.I2S_AMP_EN, LOW); // Start Muted to prevent pops
    }
    #endif

    // 2. Initialize I2C (Arduino layer)
    Wire.begin(hw_cfg.I2S_SDA_PIN, hw_cfg.I2S_SCL_PIN);

    // 3. Initialize Codecs 
    bool output_ok = initCodecOutput();
    bool input_ok = initCodecInput();

    if (!output_ok) Serial.println("[AudioMgr] Output Codec Skipped or Failed.");
    if (!input_ok)  Serial.println("[AudioMgr] Input Codec Skipped or Failed.");

    // 4. Initialize I2S (ESP32 S3 / P4 Native Driver)
    i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
    chan_cfg.auto_clear = true; 
    chan_cfg.dma_desc_num = 6; 
    chan_cfg.dma_frame_num = 512;
    
    esp_err_t err = i2s_new_channel(&chan_cfg, &_tx_handle, &_rx_handle); 
    if (err != ESP_OK) {
        Serial.printf("[AudioMgr] I2S Channel Creation Failed: %d\n", err);
        return false;
    }

    // Configure the Standard Interface using BSP variables
    i2s_std_config_t std_cfg = {
        .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG((int)hw_cfg.AUDIO_OUTPUT_SAMPLE_RATE), 
        // Use Philips I2S format (Standard)
        .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(
            (i2s_data_bit_width_t)hw_cfg.I2S_DATA_BIT_WIDTH, 
            (i2s_slot_mode_t)hw_cfg.I2S_SLOT_MODE
        ),
        .gpio_cfg = {
            .mclk = (gpio_num_t)hw_cfg.I2S_MCLK,
            .bclk = (gpio_num_t)hw_cfg.I2S_BCLK,
            .ws   = (gpio_num_t)hw_cfg.I2S_LRCK,
            .dout = (gpio_num_t)hw_cfg.I2S_DOUT,
            .din  = (gpio_num_t)hw_cfg.I2S_DIN,
            .invert_flags = {
                .mclk_inv = false, 
                .bclk_inv = false,
                .ws_inv   = false,
            },
        },
    };

    // Apply specific clock and slot sizing from BSP
    std_cfg.clk_cfg.mclk_multiple =   (i2s_mclk_multiple_t)hw_cfg.I2S_MCLK_MULTIPLE;
    std_cfg.slot_cfg.slot_bit_width = (i2s_slot_bit_width_t)hw_cfg.I2S_SLOT_BIT_WIDTH;

    // Initialize Channels
    err = i2s_channel_init_std_mode(_tx_handle, &std_cfg);
    if (err != ESP_OK) return false;

    err = i2s_channel_init_std_mode(_rx_handle, &std_cfg);
    if (err != ESP_OK) return false;

    // Enable I2S Channels
    i2s_channel_enable(_tx_handle);
    i2s_channel_enable(_rx_handle);
    
    // Enable Power Amp (Direct GPIO Only)
    // If HAS_IO_EXPANDER is defined, DisplayManager handles this independently.
    #ifndef HAS_IO_EXPANDER
        setOutputEnable(true); 
    #endif
    
    Serial.println("[AudioMgr] I2S Initialized (Duplex).");

    return true;
}

// --= Initialization Wrappers =--

bool AudioManager::initCodecOutput() {
#ifdef HAS_ES8311
    _es8311_dev = es8311_create(I2C_PORT_NUM, hw_cfg.I2S_8311_ADDR); 
    if (!_es8311_dev) return false;

    es8311_clock_config_t clk_cfg = {
        .mclk_inverted      = false,
        .sclk_inverted      = false,
        .mclk_from_mclk_pin = true,
        .mclk_frequency     = (int)(hw_cfg.AUDIO_OUTPUT_SAMPLE_RATE * hw_cfg.I2S_MCLK_MULTIPLE),
        .sample_frequency   = (int)(hw_cfg.AUDIO_OUTPUT_SAMPLE_RATE)
    };

    // Use Resolution from BSP
    es8311_resolution_t res = (es8311_resolution_t)hw_cfg.DAC_BIT_LENGTH;

    esp_err_t err = es8311_init(
        _es8311_dev, 
        &clk_cfg, 
        res, 
        res
    );

    if (err != ESP_OK) {
        Serial.printf("[AudioMgr] ES8311 Init Failed: %d\n", err);
        return false;
    }

    es8311_sample_frequency_config (_es8311_dev, clk_cfg.mclk_frequency, clk_cfg.sample_frequency);
    es8311_microphone_config       (_es8311_dev, false); 
    es8311_voice_volume_set        (_es8311_dev, _defaultVolume, NULL);
    es8311_microphone_gain_set     (_es8311_dev, ES8311_MIC_GAIN_18DB); 

    Serial.println("[AudioMgr] Output Codec (ES8311) Ready.");
    return true;
#else
    return true; // No HW codec to init
#endif
}

bool AudioManager::initCodecInput() {
#ifdef HAS_ES7210
    
    // Determine Mode (Standard vs AEC)
    #ifdef HAS_EAC
        // AEC Mode: Use 4-channel input settings
        audio_hal_adc_input_t input_mode = AUDIO_HAL_ADC_INPUT_ALL; 
        uint8_t mic_mask = hw_cfg.AEC_MIC_SELECTED;
        uint8_t mic_gain = hw_cfg.AEC_MIC_GAIN_DB;
    #else
        // Standard Mode: Use BSP settings (usually Line1/Stereo)
        audio_hal_adc_input_t input_mode = (audio_hal_adc_input_t)hw_cfg.CODEC_INPUT_MODE;
        uint8_t mic_mask = hw_cfg.MIC_SELECTED;
        uint8_t mic_gain = hw_cfg.MIC_GAIN_DB;
    #endif

    audio_hal_codec_config_t cfg_7210 = {
        .adc_input   = (audio_hal_adc_input_t)hw_cfg.CODEC_INPUT_MODE, 
        .codec_mode  = (audio_hal_codec_mode_t)hw_cfg.CODEC_CODEC_MODE, // AUDIO_HAL_CODEC_MODE_ENCODE,
        .i2s_iface = {
            .mode    = AUDIO_HAL_MODE_SLAVE,
            .fmt     = (audio_hal_iface_format_t)hw_cfg.CODEC_IFACE_I2S_FMT,
            .samples = (audio_hal_iface_samples_t)hw_cfg.CODEC_IFACE_SAMPLES, 
            .bits    = (audio_hal_iface_bits_t)hw_cfg.CODEC_IFACE_BIT_LENGTH
        }
    };

    esp_err_t err = es7210_adc_init(&Wire, &cfg_7210);

    if (err != ESP_OK) {
        Serial.printf("[AudioMgr] ES7210 Init Failed: %d\n", err);
    };

    es7210_adc_config_i2s(cfg_7210.codec_mode, &cfg_7210.i2s_iface);

    // --= Gain Staging from BSP =-- 
    // Apply gain to selected Mics
    es7210_adc_set_gain((es7210_input_mics_t)mic_mask, (es7210_gain_value_t)mic_gain);
    
    // Mute unselected mics 
    uint8_t unselected = mic_mask ^ 0x0F;
    if (unselected > 0) {
        es7210_adc_set_gain((es7210_input_mics_t)unselected, GAIN_0DB);
    }
    
    // Select Mics
    es7210_mic_select((es7210_input_mics_t)mic_mask);

    es7210_adc_ctrl_state(cfg_7210.codec_mode, AUDIO_HAL_CTRL_START);
    
    Serial.println("[AudioMgr] Input Codec (ES7210) Ready.");
    return true;
#else
    return true; // No HW codec to init
#endif
}

// --= Control API =--

void AudioManager::setVolume(uint8_t setVolume) {
    if (setVolume > 100) {
        setVolume = 100;
    } else if (setVolume < 0) {
        setVolume = 0;
    }
    _currentVolume = setVolume;
    if (_es8311_dev) {
        es8311_voice_volume_set(_es8311_dev, setVolume, NULL);
    }
    Serial.printf("[AudioMgr] Set Volume: %d%%\n", setVolume);
}

int AudioManager::getVolume() {
    return _currentVolume;
}

void AudioManager::setMute(bool on) {
    _isMuted = on;
    if (_es8311_dev) {
        es8311_voice_mute(_es8311_dev, on) ? true : false;    // True = Mute, False = Unmute
    }
}

void AudioManager::setOutputEnable(bool on) {
#ifndef HAS_IO_EXPANDER
    digitalWrite(hw_cfg.I2S_AMP_EN, on ? HIGH : LOW);
#else
    if (_es8311_dev) {
        es8311_voice_mute(_es8311_dev, !on);    // True = Mute, False = Unmute
    }
#endif
}

void AudioManager::tone(uint32_t freq, uint32_t durationMs) {    
    if (_tx_handle == NULL) return;

    size_t bytes_written;
    uint32_t samples = (hw_cfg.AUDIO_OUTPUT_SAMPLE_RATE * durationMs) / 1000;
    uint32_t waveLength = hw_cfg.AUDIO_OUTPUT_SAMPLE_RATE / freq;
    
    int16_t *buffer = (int16_t *)malloc(waveLength * 2 * sizeof(int16_t)); 
    if (!buffer) return;

    // Square Wave (Amplitude 3000)
    for (uint32_t i = 0; i < waveLength; i++) {
        int16_t sample = (i < waveLength / 2) ? 3000 : -3000; 
        buffer[i * 2] = sample;     
        buffer[i * 2 + 1] = sample; 
    }

    Serial.printf("[AudioMgr] Playing Tone: %d Hz for %d ms (%d samples, %d waveLength)\n", freq, durationMs, samples, waveLength);

    uint32_t cycles = samples / waveLength;
    for (uint32_t i = 0; i < cycles; i++) {
        i2s_channel_write(_tx_handle, buffer, waveLength * 2 * sizeof(int16_t), &bytes_written, 100);
    }

    free(buffer);
}

int AudioManager::getMicLevel() {
    if (_rx_handle == NULL) return 0;

    size_t bytes_read = 0;
    int16_t samples[256]; 
    
    // Non-blocking read
    esp_err_t err = i2s_channel_read(_rx_handle, samples, sizeof(samples), &bytes_read, 0); 
    
    if (err != ESP_OK || bytes_read == 0) return 0;

    int64_t sum = 0;
    int count = bytes_read / 2; 
    
    for(int i=0; i<count; i++) {
        sum += samples[i] * samples[i];
    }
    
    float rms = sqrt(sum / count);

    // RMS Mapping: Floor 50 (Noise) -> Ceiling 4000 (Loud) (from 100-8000)
    int level = map((int)rms, 50, 4000, 0, 100);
    if (level < 0) level = 0;
    if (level > 100) level = 100;
    
    return level;
}

// --= Primitives for Loopback Testing =--

size_t AudioManager::readRaw(int16_t *data, size_t samples) {
    if (!_rx_handle) return 0;
    size_t bytes_read = 0;
    // Short timeout for non-blocking UI updates
    esp_err_t err = i2s_channel_read(_rx_handle, data, samples * sizeof(int16_t), &bytes_read, 10);
    if (err != ESP_OK) return 0;
    return bytes_read / sizeof(int16_t);
}

size_t AudioManager::writeRaw(int16_t *data, size_t samples) {
    if (!_tx_handle) return 0;
    size_t bytes_written = 0;
    esp_err_t err = i2s_channel_write(_tx_handle, data, samples * sizeof(int16_t), &bytes_written, 100);
    if (err != ESP_OK) return 0;
    return bytes_written / sizeof(int16_t);
}

// --= Blocking helpers for 5-second test =--

bool AudioManager::recordSeconds(int16_t* buffer, float seconds) {
    if (!_rx_handle) return false;
    
    // Calculate exact buffer size needed: Rate * Time
    size_t samplesToRead = (size_t)(seconds * hw_cfg.AUDIO_OUTPUT_SAMPLE_RATE);
    size_t bytesRead = 0;
    
    // Blocking read (portMAX_DELAY) ensures we get the full duration
    // This is safe to do in a test function, but would freeze UI if done on main thread without yielding
    esp_err_t err = i2s_channel_read(_rx_handle, 
                                     buffer, 
                                     samplesToRead * sizeof(int16_t), 
                                     &bytesRead, 
                                     portMAX_DELAY);
                                     
    return (err == ESP_OK && bytesRead == samplesToRead * sizeof(int16_t));
}

bool AudioManager::playSeconds(int16_t* buffer, float seconds) {
    if (!_tx_handle) return false;
    
    size_t samplesToWrite = (size_t)(seconds * hw_cfg.AUDIO_OUTPUT_SAMPLE_RATE);
    size_t bytesWritten = 0;
    
    esp_err_t err = i2s_channel_write(_tx_handle, 
                                      buffer, 
                                      samplesToWrite * sizeof(int16_t), 
                                      &bytesWritten, 
                                      portMAX_DELAY);
                                      
    return (err == ESP_OK);
}


// --= Internal Helpers =--
// Low-level I2C register access for ES8311 Codec
// es8311_write_reg and es8311_read_reg could be used but we
// started out attempting to port es8311 from esp-idf to Arduino.

void AudioManager::writeReg(uint8_t reg, uint8_t value) {
    Wire.beginTransmission(hw_cfg.I2S_8311_ADDR);
    Wire.write(reg);
    Wire.write(value);
    uint8_t error = Wire.endTransmission();
    if (error != 0) {
        Serial.printf("[AudioMgr] I2C Write Error: %d at Reg 0x%02X\n", error, reg);
    }
}

void AudioManager::readReg(uint8_t reg, uint8_t &value) {
    Wire.beginTransmission(hw_cfg.I2S_8311_ADDR);
    Wire.write(reg);
    Wire.endTransmission(false); // Restart for read
    Wire.requestFrom(hw_cfg.I2S_8311_ADDR, (uint8_t)1);
    if (Wire.available()) {
        value = Wire.read();
    } else {
        Serial.printf("[AudioMgr] I2C Read Error at Reg 0x%02X\n", reg);
    }
}

// --- UPDATED: New generic writeReg for arbitrary I2C devices (ES7210 @ 0x41) ---
void AudioManager::writeReg(uint8_t deviceAddr, uint8_t reg, uint8_t value) {
    Wire.beginTransmission(deviceAddr);
    Wire.write(reg);
    Wire.write(value);
    uint8_t error = Wire.endTransmission();
    if (error != 0) {
        Serial.printf("[AudioMgr] I2C Write Error: %d at Addr 0x%02X Reg 0x%02X\n", error, deviceAddr, reg);
    }
}