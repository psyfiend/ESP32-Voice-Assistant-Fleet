#include "AudioManager.h"

// I2C Port used by the ESP-IDF drivers
#define I2C_PORT_NUM 0

// --= Sanity check: BSP raw values vs. es7210 driver enums =--
// hw_cfg.MIC_SELECTED / MIC_GAIN_DB / AEC_MIC_SELECTED / AEC_MIC_GAIN_DB are stored as
// plain uint8_t in Fleet_Hardware_Config (the BSP headers deliberately avoid including
// es7210.h to keep their dependency chain simple), but the raw numbers are meant to equal
// these es7210 enum values exactly. This can't be a static_assert: hw_cfg/WS_P4_7B_Hardware
// (etc.) are declared `const`, not `constexpr`, so they aren't usable in a constant
// expression as-is - making them constexpr would mean requalifying every BSP struct
// instance across all 6 device headers, a bigger change than this warrants right now.
// So this runs once at startup instead: if es7210_gain_value_t or es7210_input_mics_t is
// ever reordered or extended, this logs a loud, specific error instead of silently
// misconfiguring mic gain/selection.
#ifdef HAS_ES7210
static void checkAudioBspSanity() {
    if (hw_cfg.MIC_SELECTED != (uint8_t)(ES7210_INPUT_MIC1 | ES7210_INPUT_MIC2)) {
        Serial.println("[AudioMgr] CONFIG WARNING: hw_cfg.MIC_SELECTED no longer matches Mic1|Mic2 - check es7210_input_mics_t");
    }
    if (hw_cfg.MIC_GAIN_DB != (uint8_t)GAIN_36DB) {
        Serial.println("[AudioMgr] CONFIG WARNING: hw_cfg.MIC_GAIN_DB no longer matches GAIN_36DB - check es7210_gain_value_t");
    }
    if (hw_cfg.AEC_MIC_SELECTED != (uint8_t)(ES7210_INPUT_MIC1 | ES7210_INPUT_MIC2 | ES7210_INPUT_MIC3 | ES7210_INPUT_MIC4)) {
        Serial.println("[AudioMgr] CONFIG WARNING: hw_cfg.AEC_MIC_SELECTED no longer matches Mic1|2|3|4 - check es7210_input_mics_t");
    }
    if (hw_cfg.AEC_MIC_GAIN_DB != (uint8_t)GAIN_12DB) {
        Serial.println("[AudioMgr] CONFIG WARNING: hw_cfg.AEC_MIC_GAIN_DB no longer matches GAIN_12DB - check es7210_gain_value_t");
    }
}
#endif

AudioManager::AudioManager() {
    _tx_handle = NULL;
    _rx_handle = NULL;
    _currentVolume = 0;
    _defaultVolume = 63;
    _isMuted = false;
    _es8311_dev = NULL;
}

bool AudioManager::begin() {

    Serial.println("------------------------------");
    Serial.println("[AudioMgr] Initializing...");

    #ifdef HAS_ES7210
        checkAudioBspSanity();
    #endif

    // 1. --= Initialize Power Amp Pin =--
    #ifndef HAS_IO_EXPANDER
    if (hw_cfg.I2S_AMP_EN != -1) {
        pinMode     (hw_cfg.I2S_AMP_EN, OUTPUT);
        digitalWrite(hw_cfg.I2S_AMP_EN, LOW); // Start Muted to prevent pops
    }
    #endif

    // 2. --= Initialize I2C (Arduino layer) =--
    Wire.begin      (hw_cfg.I2S_SDA_PIN, hw_cfg.I2S_SCL_PIN);

    // 3. --= Initialize Codecs =--
    bool output_ok = initCodecOutput();
    bool input_ok  = initCodecInput();

    if (!output_ok) Serial.println("[AudioMgr] Output Codec Skipped or Failed.");
    if (!input_ok)  Serial.println("[AudioMgr] Input Codec Skipped or Failed.");

    // 4. --= Initialize I2S =--
    i2s_chan_config_t chan_cfg  = {     // I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER)};
        chan_cfg.id             = I2S_NUM_0,
        chan_cfg.role           = I2S_ROLE_MASTER, 
        chan_cfg.dma_desc_num   = AUDIO_CODEC_DMA_DESC_NUM, 
        chan_cfg.dma_frame_num  = AUDIO_CODEC_DMA_FRAME_NUM, // 240 is default
        chan_cfg.auto_clear     = true
    };

    esp_err_t err = i2s_new_channel(&chan_cfg, &_tx_handle, &_rx_handle); 
    if (err != ESP_OK) {
        Serial.printf("[AudioMgr] I2S Channel Creation Failed: %d\n", err);
        return false;
    }

    // --= I2S Configuration Logic =--

    // --= TDM MODE (Philips I2S) =--
    i2s_tdm_config_t tdm_cfg = {
        .clk_cfg  = {       // I2S_TDM_CLK_DEFAULT_CONFIG((uint32_t)(SAMPLE_RATE)),
            .sample_rate_hz = (uint32_t)SAMPLE_RATE,
            .clk_src        = I2S_CLK_SRC_DEFAULT,
            .ext_clk_freq_hz = 0,
            .mclk_multiple  = I2S_MCLK_MULTIPLE_256,
            .bclk_div       = 8,    // Xiaozhi setting
        },
        .slot_cfg = {       // I2S_TDM_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT,I2S_SLOT_MODE_STEREO,(i2s_tdm_slot_mask_t)(I2S_TDM_SLOT0 | I2S_TDM_SLOT1 | I2S_TDM_SLOT2 | I2S_TDM_SLOT3)), // Use all 4 slots
            .data_bit_width = I2S_DATA_BIT_WIDTH_16BIT,
            .slot_bit_width = I2S_SLOT_BIT_WIDTH_AUTO,  // I2S_SLOT_BIT_WIDTH_16BIT
            .slot_mode      = I2S_SLOT_MODE_STEREO,
            .slot_mask      = (i2s_tdm_slot_mask_t)(I2S_TDM_SLOT0 | I2S_TDM_SLOT1 | I2S_TDM_SLOT2 | I2S_TDM_SLOT3),
            .ws_width       = I2S_TDM_AUTO_WS_WIDTH,
            .ws_pol         = false,
            .bit_shift      = true,
            .left_align     = false,
            .big_endian     = false,
            .bit_order_lsb  = false,
            .skip_mask      = false,
            .total_slot     = I2S_TDM_AUTO_SLOT_NUM
        },
        .gpio_cfg = {
            .mclk = (gpio_num_t)hw_cfg.I2S_MCLK,
            .bclk = (gpio_num_t)hw_cfg.I2S_BCLK,
            .ws   = (gpio_num_t)hw_cfg.I2S_LRCK,
            .dout = I2S_GPIO_UNUSED,
            .din  = (gpio_num_t)hw_cfg.I2S_DIN,
            .invert_flags = {0}
        },
    };

    // --= STANDARD MODE (Philips I2S) =--
    i2s_std_config_t std_cfg = {
        .clk_cfg  = {       // I2S_STD_CLK_DEFAULT_CONFIG((uint32_t)(SAMPLE_RATE)),
            .sample_rate_hz = (uint32_t)SAMPLE_RATE,
            .clk_src        = I2S_CLK_SRC_DEFAULT,
            .ext_clk_freq_hz = 0,
            .mclk_multiple  = I2S_MCLK_MULTIPLE_256
        },
        .slot_cfg = {       // I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT,I2S_SLOT_MODE_STEREO),
            .data_bit_width = I2S_DATA_BIT_WIDTH_16BIT,
            .slot_bit_width = I2S_SLOT_BIT_WIDTH_AUTO,
            .slot_mode      = I2S_SLOT_MODE_STEREO,
            .slot_mask      = I2S_STD_SLOT_BOTH,
            .ws_width       = I2S_DATA_BIT_WIDTH_16BIT,
            .ws_pol         = false,
            .bit_shift      = true,
            .left_align     = true,    // this is set to true in Xiaozhi config
            .big_endian     = false,
            .bit_order_lsb  = false,
        },
        .gpio_cfg = {
            .mclk = (gpio_num_t)hw_cfg.I2S_MCLK,
            .bclk = (gpio_num_t)hw_cfg.I2S_BCLK,
            .ws   = (gpio_num_t)hw_cfg.I2S_LRCK,
            .dout = (gpio_num_t)hw_cfg.I2S_DOUT,
            .din  = I2S_GPIO_UNUSED,
            .invert_flags = {0},
        },
    };

    #ifdef ENABLE_AEC
        Serial.println("[AudioMgr] Mode: AEC (Mixed TDM/STD)");
        
        // RX: TDM Mode (4 Channels)
        err  = i2s_channel_init_tdm_mode    (_rx_handle, &tdm_cfg);

        // TX: STD Mode (Stereo) with Left Align
        err |= i2s_channel_init_std_mode    (_tx_handle, &std_cfg); // TX in standard mode to match codec config
    #else
        Serial.println("[AudioMgr] Mode: Standard Voice (Stereo)");
        std_cfg.gpio_cfg.din = (gpio_num_t)hw_cfg.I2S_DIN;  // In case es8311 is the only codec used
        // left_align stays true (see std_cfg init above) - that matches ESP-IDF's own
        // I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG default for standard 2-slot stereo mode.
        err  = i2s_channel_init_std_mode    (_tx_handle, &std_cfg);
        err |= i2s_channel_init_std_mode    (_rx_handle, &std_cfg);
    #endif

    if (err != ESP_OK) {
        Serial.printf("[AudioMgr] I2S Channel Init Failed: %d\n", err);
        return false;
    }

    i2s_channel_enable(_tx_handle);
    i2s_channel_enable(_rx_handle);
    
    // Enable Power Amp (Direct GPIO Only)
    #ifndef HAS_IO_EXPANDER
        if (hw_cfg.I2S_AMP_EN != -1) {
            setOutputEnable(true); 
        }
    #endif
    
    Serial.println("[AudioMgr] I2S Initialized (Duplex).");
    
    #ifndef HAS_ES8311
        Serial.println("[AudioMgr] No Output Codec Defined; Volume Control Disabled.");
    #else
        setVolume(_defaultVolume);
    #endif

    return true;
}

// --= Initialization Wrappers =--

bool AudioManager::initCodecOutput() {
#ifdef HAS_ES8311

    _es8311_dev = es8311_create(I2C_PORT_NUM, hw_cfg.I2S_8311_ADDR); 
    if (!_es8311_dev) return false;

    // Use unified audio_hal configuration struct
    audio_hal_codec_config_t cfg = {
        .adc_input  = AUDIO_HAL_ADC_INPUT_LINE1, // Not used for 8311
        .dac_output = AUDIO_HAL_DAC_OUTPUT_ALL,
        .codec_mode = AUDIO_HAL_CODEC_MODE_DECODE,
        .i2s_iface = {
            .mode    = AUDIO_HAL_MODE_SLAVE,
            .fmt     = AUDIO_HAL_I2S_NORMAL, // Default
            .samples = AUDIO_HAL_16K_SAMPLES,
            .bits    = AUDIO_HAL_BIT_LENGTH_16BITS
        }
    };
    #ifndef HAS_ES7210
        // No separate ADC codec on this board - the ES8311's own mic input is the
        // only input path available, so it has to run DAC (speaker) and ADC (mic)
        // simultaneously instead of DAC-only. On boards with ES7210 present, ES7210
        // handles all mic input and ES8311 stays output-only, so this is skipped.
        cfg.codec_mode = AUDIO_HAL_CODEC_MODE_BOTH;
    #endif

    #ifdef ENABLE_AEC
        // If AEC is enabled, we assume the system clock is TDM/DSP.
        // We set the codec to DSP mode so it understands the clock.
        cfg.i2s_iface.fmt = AUDIO_HAL_I2S_DSP;
    #endif

    esp_err_t   err  = es8311_codec_init        (&cfg);
                err |= es8311_codec_config_i2s  (cfg.codec_mode, &cfg.i2s_iface);
                err |= es8311_codec_ctrl_state  (cfg.codec_mode, AUDIO_HAL_CTRL_START);
    
    if (err != ESP_OK) {
        Serial.printf("[AudioMgr] ES8311 Init Failed: %d\n", err);
        return false;
    }

    // Set Gains (handled by updated driver functions)
    es8311_codec_set_voice_volume   (_defaultVolume);
    // es8311_set_mic_gain             (ES8311_MIC_GAIN_6DB); 

    Serial.println("[AudioMgr] Output Codec (ES8311) Ready.");
    return true;
#else
    Serial.println("[AudioMgr] No Output Codec Defined.");    
    return true; // No HW codec to init
#endif
}

bool AudioManager::initCodecInput() {
#ifdef HAS_ES7210

    audio_hal_codec_config_t cfg = {
        .adc_input   = AUDIO_HAL_ADC_INPUT_LINE1,
        .dac_output  = AUDIO_HAL_DAC_OUTPUT_ALL,
        .codec_mode  = AUDIO_HAL_CODEC_MODE_ENCODE,
        .i2s_iface = {
            .mode    = AUDIO_HAL_MODE_SLAVE,
            .fmt     = AUDIO_HAL_I2S_NORMAL,
            .samples = AUDIO_HAL_16K_SAMPLES,
            .bits    = AUDIO_HAL_BIT_LENGTH_16BITS
        }
    };

    // Note: when HAS_ES8311 is also present, ES8311 stays output-only (DECODE) - see
    // initCodecOutput(). ES7210 is always the mic path whenever it's present.

    #ifdef ENABLE_AEC
        cfg.adc_input     = AUDIO_HAL_ADC_INPUT_ALL;
        cfg.i2s_iface.fmt = AUDIO_HAL_I2S_DSP;
        // Note: The driver automatically handles TDM reg 0x12 if input count > 2
    #endif

    esp_err_t   err  = es7210_adc_init        (&cfg);
                err |= es7210_adc_config_i2s  (cfg.codec_mode, &cfg.i2s_iface);
    // Note: es7210_adc_init calls es7210_mic_select()/es7210_adc_set_gain() internally,
    // using hw_cfg.MIC_SELECTED/MIC_GAIN_DB (or the AEC_ variants if ENABLE_AEC is on).
    // Note: es7210_adc_config_i2s sets codec mode internally

    if (err != ESP_OK) {
        Serial.printf("[AudioMgr] ES7210 Init Failed: %d\n", err);
    };

    // es7210_adc_set_gain(ES7210_INPUT_MIC3, GAIN_6DB);

    es7210_adc_ctrl_state  (cfg.codec_mode, AUDIO_HAL_CTRL_START);

    Serial.println("[AudioMgr] Input Codec (ES7210) Ready.");
    return true;
#else
    #ifdef HAS_ES8311
        Serial.println("[AudioMgr] No ES7210; mic input handled by ES8311 (see initCodecOutput).");
    #else
        Serial.println("[AudioMgr] No Input Codec Defined.");
    #endif
    return true;
#endif
}

// --= Control API =--

void AudioManager::setVolume(uint8_t setVolume) {
    if (setVolume > 100) setVolume = 100;
    _currentVolume = setVolume;
    if (_es8311_dev) es8311_codec_set_voice_volume(setVolume);
    Serial.printf("[AudioMgr] Set Volume: %d%%\n", setVolume);
}

int AudioManager::getVolume() {
    return _currentVolume;
}

void AudioManager::setMute(bool on) {
    _isMuted = on;
    if (_es8311_dev) es8311_set_voice_mute(on);
}

bool AudioManager::getMute() {
    return _isMuted;
}

void AudioManager::setOutputEnable(bool on) {
#ifndef HAS_IO_EXPANDER
    digitalWrite(hw_cfg.I2S_AMP_EN, on ? HIGH : LOW);
#else
    if (_es8311_dev) es8311_set_voice_mute(!on);
#endif
}

void AudioManager::tone(uint32_t freq, uint32_t durationMs) {    
    if (_tx_handle == NULL) return;

    size_t      bytes_written;
    uint32_t    waveLength  = SAMPLE_RATE / freq;
    int16_t     *buffer     = (int16_t *)malloc(waveLength * 2 * sizeof(int16_t)); 
    if (!buffer) return;

    // Square Wave (Amplitude 3000)
    for (uint32_t i = 0; i < waveLength; i++) {
        int16_t sample = (i < waveLength / 2) ? 3000 : -3000; 
        buffer[i * 2] = sample;     
        buffer[i * 2 + 1] = sample; 
    }

    Serial.printf("[AudioMgr] Playing Tone: %d Hz for %d ms \n", freq, durationMs);

    uint32_t cycles = (SAMPLE_RATE * durationMs / 1000) / waveLength;
    for (uint32_t i = 0; i < cycles; i++) {
        i2s_channel_write(_tx_handle, buffer, waveLength * 2 * sizeof(int16_t), &bytes_written, 100);
    }

    free(buffer);
}

int AudioManager::getMicLevel() {
    if (_rx_handle == NULL) return 0;

    size_t  bytes_read = 0;
    int16_t samples[256]; 
    
    // Non-blocking read
    esp_err_t err = i2s_channel_read(_rx_handle, samples, sizeof(samples), &bytes_read, 0); 
    
    if (err != ESP_OK || bytes_read == 0) return 0;

    int64_t sum = 0;
    // Note: If in TDM mode this still works roughly (RMS of 4 channels is still RMS)
    // Ideally we would only count every 4th sample but for a VU meter this is sufficient.
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

// --= Raw I2S Read/Write Primitives =--

size_t AudioManager::readRaw(int16_t *data, size_t samples) {
    if (!_rx_handle) return 0;
    size_t bytes_read = 0;
    // Non-blocking read (0 timeout) to prevent freezing if I2S stalls
    esp_err_t err = i2s_channel_read(_rx_handle,
                                     data, samples * sizeof(int16_t),
                                     &bytes_read,
                                     0);
    return bytes_read / sizeof(int16_t);
}

size_t AudioManager::writeRaw(int16_t *data, size_t samples) {
    if (!_tx_handle) return 0;
    size_t bytes_written = 0;
    esp_err_t err = i2s_channel_write(_tx_handle,
                                     data, samples * sizeof(int16_t),
                                     &bytes_written,
                                     100);
    if (err != ESP_OK) return 0;
    return bytes_written / sizeof(int16_t);
}

// --= Blocking helpers for 5-second test =--

bool AudioManager::recordSeconds(int16_t* buffer, float seconds) {
    if (!_rx_handle) return false;
    
    // Calculate exact buffer size needed: Rate * Time
    size_t samplesToRead = (size_t)(seconds * SAMPLE_RATE);
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
    
    size_t samplesToWrite = (size_t)(seconds * SAMPLE_RATE);
    size_t bytesWritten = 0;
    
    esp_err_t err = i2s_channel_write(_tx_handle, 
                                      buffer, 
                                      samplesToWrite * sizeof(int16_t), 
                                      &bytesWritten, 
                                      portMAX_DELAY);
                                      
    return (err == ESP_OK);
}

// Note: Manual writeReg/readReg helpers removed as they are no longer needed
// because es8311_codec_init and es8311_set_mic_gain handle register writes.