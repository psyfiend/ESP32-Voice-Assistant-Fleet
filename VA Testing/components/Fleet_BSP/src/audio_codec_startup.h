Wire.begin(hw_cfg.I2S_SDA_PIN, hw_cfg.I2S_SCL_PIN);
int initCodecOutput();
    _es8311_dev = es8311_create(I2C_PORT_NUM, ES8311_ADDRESS_0);
    es8311_clock_config_t clk_cfg = {
        .mclk_inverted      = false,
        .sclk_inverted      = false,
        .mclk_from_mclk_pin = true,
        .mclk_frequency     = 16000 * 256,  //4,096,000 Hz
        .sample_frequency   = 16000,
    };
    es8311_init(_es8311_dev, &clk_cfg, ES8311_RESOLUTION_16, ES8311_RESOLUTION_16);
        es8311_write_reg(dev, ES8311_RESET_REG00, 0x1F)         // 0x00 // Reset ES8311
        es8311_write_reg(dev, ES8311_RESET_REG00, 0x00)         // 0x00 // Release reset
        es8311_write_reg(dev, ES8311_RESET_REG00, 0x80)         // 0x00 // Power up all blocks
        es8311_clock_config(dev, clk_cfg, res_out)  // Configure clock settings
            es8311_write_reg(dev, ES8311_CLK_MANAGER_REG01, reg01)  // 0x01 // mclk source and sclk inversion
            es8311_read_reg(dev, ES8311_CLK_MANAGER_REG06, &reg06)  // 0x06 // bclk inverter and divider
            es8311_write_reg(dev, ES8311_CLK_MANAGER_REG06, reg06)  // 0x06 // bclk inverter and divider
            es8311_sample_frequency_config(dev, mclk_hz, clk_cfg->sample_frequency)
                yadda yadda
        es8311_fmt_config(dev, res_in, res_out)  // Configure data format: master/slave, resolution, I2S format
            yadda yadda
        es8311_write_reg(dev, ES8311_SYSTEM_REG0D, 0x01);       // 0x0D // system, power up/down
        es8311_write_reg(dev, ES8311_SYSTEM_REG0E, 0x00);       // 0x0E // system, power up/down
        es8311_write_reg(dev, ES8311_SYSTEM_REG12, 0x00);       // 0x12 // system, enable DAC
        es8311_write_reg(dev, ES8311_SYSTEM_REG13, 0x10);       // 0x13 // system
        es8311_write_reg(dev, ES8311_ADC_REG1C, 0x6A);          // 0x1C // adc, equalizer, hpf s2
        es8311_write_reg(dev, ES8311_DAC_REG37, 0x08);          // 0x37 // dac, ramp rate

    es8311_sample_frequency_config(_es8311_dev, clk_cfg.mclk_frequency, clk_cfg.sample_frequency);
        get_coeff(mclk_frequency, samples frequency);
        es8311_read_reg(dev, ES8311_CLK_MANAGER_REG02, &regv);  // 0x02 // clk divider and clk multiplier
        es8311_write_reg(dev, ES8311_CLK_MANAGER_REG02, regv);
        es8311_write_reg(dev, ES8311_CLK_MANAGER_REG03, reg03); // 0x03 // adc fsmode and osr
        es8311_write_reg(dev, ES8311_CLK_MANAGER_REG04, selected_coeff->dac_osr);   // 0x04 // dac osr
        es8311_write_reg(dev, ES8311_CLK_MANAGER_REG05, reg05); // 0x05 // clk divider for adc and dac
        es8311_read_reg(dev, ES8311_CLK_MANAGER_REG06, &regv);  // 0x06 // bclk inverter and divider
        es8311_write_reg(dev, ES8311_CLK_MANAGER_REG06, regv);
        es8311_read_reg(dev, ES8311_CLK_MANAGER_REG07, &regv);  // 0x07 // tri-state, lrck divider
        es8311_write_reg(dev, ES8311_CLK_MANAGER_REG07, regv);
        es8311_write_reg(dev, ES8311_CLK_MANAGER_REG08, selected_coeff->lrck_l);    // 0x08 // lrck divider low byte
    es8311_microphone_config(_es8311_dev, false); 
        es8311_write_reg(dev, ES8311_ADC_REG17, 0xC8);          // 0x17 // Set ADC gain, volume and high pass filter
        es8311_write_reg(dev, ES8311_SYSTEM_REG14, 0x1A);       // 0x14 // enable analog MIC and max PGA gain
    es8311_voice_volume_set(_es8311_dev, _defaultVolume, NULL);
    es8311_microphone_gain_set(_es8311_dev, ES8311_MIC_GAIN_18DB); // Example uses gain 3 (~18dB)

int initCodecInput();
    audio_hal_codec_config_t cfg_7210 = {
        .adc_input   = AUDIO_HAL_ADC_INPUT_ALL,
        .codec_mode  = AUDIO_HAL_CODEC_MODE_ENCODE,
        .i2s_iface = {
            .mode    = AUDIO_HAL_MODE_SLAVE,
            .fmt     = AUDIO_HAL_I2S_NORMAL,
            .samples = AUDIO_HAL_16K_SAMPLES,
            .bits    = AUDIO_HAL_BIT_LENGTH_16BITS}};
    es7210_adc_init(&Wire, &cfg_7210);
        es7210_write_reg(ES7210_RESET_REG00, 0xff);             // 0x00 // Reset ES7210
        es7210_write_reg(ES7210_RESET_REG00, 0x41);             // 0x00 // Release reset and set to normal mode
        es7210_write_reg(ES7210_CLOCK_OFF_REG01, 0x1f);         // 0x01 // Disable clock off function
        es7210_write_reg(ES7210_TIME_CONTROL0_REG09, 0x30);     // 0x09 // Set chip initial state period
        es7210_write_reg(ES7210_TIME_CONTROL1_REG0A, 0x30);     // 0x0A // Set power on state period
        audio_hal_codec_i2s_iface_t *i2s_cfg = & (codec_cfg->i2s_iface);
            ESP_LOGI(TAG, "ES7210 in Slave mode");
        es7210_write_reg(ES7210_ANALOG_REG40, 0xC3);            // 0x40 // Select power off analog, vdda = 3.3V, close vx20ff, VMID select 5KΩ start
        es7210_write_reg(ES7210_MIC12_BIAS_REG41, 0x70);        // 0x41 // Select 2.87v
        es7210_write_reg(ES7210_MIC34_BIAS_REG42, 0x70);        // 0x42 // Select 2.87v
        es7210_write_reg(ES7210_OSR_REG07, 0x20);               // 0x07 // Set osr to 64
        es7210_write_reg(ES7210_MAINCLK_REG02, 0xc1);           // 0x02 // Set the frequency division coefficient and use dll except clock doubler, and need to set 0xc1 to clear the state */
        es7210_config_sample(i2s_cfg->samples);  // AUDIO_HAL_16K_SAMPLES
            sample_fre = 16000;
            coeff = get_coeff(mclk_fre, sample_fre);
                    es7210_read_reg(ES7210_MAINCLK_REG02) & 0x00;   // 0x02 // Set adc_div & doubler & dll
                    coeff_div[coeff].adc_div;
                    coeff_div[coeff].doubler << 6;
                    coeff_div[coeff].dll << 7;
                    es7210_write_reg(ES7210_MAINCLK_REG02, regv);   // 0x02 // Set adc_div & doubler & dll
                    regv = coeff_div[coeff].osr;    // Set osr
                    es7210_write_reg(ES7210_OSR_REG07, regv);       // 0x07 // Set osr
                    regv = coeff_div[coeff].lrck_h; // Set lrck
                    es7210_write_reg(ES7210_LRCK_DIVH_REG04, regv); // 0x04 // lrck high byte
                    regv = coeff_div[coeff].lrck_l;
                    es7210_write_reg(ES7210_LRCK_DIVL_REG05, regv); // 0x05 // lrck low byte
        es7210_mic_select(mic_select);
        es7210_adc_set_gain_all(GAIN_0DB);  // GAIN_0DB = 0
            es7210_update_reg_bit(ES7210_MIC1_GAIN_REG43, 0x0f, GAIN_0DB);  // if MIC1 // 0x43
            es7210_update_reg_bit(ES7210_MIC2_GAIN_REG44, 0x0f, GAIN_0DB);  // if MIC2 // 0x44
            es7210_update_reg_bit(ES7210_MIC3_GAIN_REG45, 0x0f, GAIN_0DB);  // if MIC3 // 0x45
            es7210_update_reg_bit(ES7210_MIC4_GAIN_REG46, 0x0f, GAIN_0DB);  // if MIC4 // 0x46

    es7210_adc_config_i2s(cfg_7210.codec_mode, &cfg_7210.i2s_iface);
            es7210_set_bits(iface->bits);   // bits = AUDIO_HAL_BIT_LENGTH_16BITS
                es7210_write_reg(ES7210_SDP_INTERFACE1_REG11, 0x60); // 0x11 // AUDIO_HAL_BIT_LENGTH_16BITS (0x60)
            es7210_config_fmt(iface->fmt);  // fmt = AUDIO_HAL_I2S_NORMAL
                es7210_write_reg(ES7210_SDP_INTERFACE1_REG11, 0x00); // 0x11 // AUDIO_HAL_I2S_NORMAL (0x00)
                es7210_write_reg(ES7210_SDP_INTERFACE2_REG12, 0x00); // 0x12 // Force ADC1/2 output to SDOUT1 and ADC3/4 output to SDOUT2
            es7210_config_sample(AUDIO_HAL_16K_SAMPLES); // AUDIO_HAL_16K_SAMPLES

    es7210_adc_set_gain((es7210_input_mics_t)(ES7210_INPUT_MIC1 | ES7210_INPUT_MIC2), GAIN_30DB);
    es7210_adc_set_gain((es7210_input_mics_t)(ES7210_INPUT_MIC3 | ES7210_INPUT_MIC4), GAIN_0DB);
    es7210_mic_select((es7210_input_mics_t)(ES7210_INPUT_MIC1 | ES7210_INPUT_MIC2));
    es7210_adc_ctrl_state(cfg_7210.codec_mode, AUDIO_HAL_CTRL_START);   // AUDIO_HAL_MODE_SLAVE // AUDIO_HAL_CTRL_START
            regv = es7210_read_reg(ES7210_CLOCK_OFF_REG01);
            es7210_start(regv);

i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
chan_cfg.auto_clear = true; 
chan_cfg.dma_desc_num = 6; 
chan_cfg.dma_frame_num = 512;
i2s_new_channel(&chan_cfg, &_tx_handle, &_rx_handle); 
i2s_std_config_t std_cfg = {
    .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(16000), 
    .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO),
    .gpio_cfg = {
        .mclk = (gpio_num_t)hw_cfg.I2S_MCLK,
        .bclk = (gpio_num_t)hw_cfg.I2S_BCLK,
        .ws   = (gpio_num_t)hw_cfg.I2S_LRCK,
        .dout = (gpio_num_t)hw_cfg.I2S_DOUT,
        .din  = (gpio_num_t)hw_cfg.I2S_DIN,
        .invert_flags = {
            .mclk_inv = false, 
            .bclk_inv = false,
            .ws_inv = false,
        },
    },
};
std_cfg.clk_cfg.mclk_multiple = I2S_MCLK_MULTIPLE_256;
std_cfg.slot_cfg.slot_bit_width = I2S_SLOT_BIT_WIDTH_32BIT;
std_cfg.slot_cfg.data_bit_width = I2S_DATA_BIT_WIDTH_16BIT;
i2s_channel_init_std_mode(_tx_handle, &std_cfg);
i2s_channel_init_std_mode(_rx_handle, &std_cfg);
i2s_channel_enable(_tx_handle);
i2s_channel_enable(_rx_handle);