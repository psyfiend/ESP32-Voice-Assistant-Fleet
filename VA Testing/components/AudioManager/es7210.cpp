/*
 * ESPRESSIF MIT License
 *
 * Copyright (c) 2021 <ESPRESSIF SYSTEMS (SHANGHAI) CO., LTD>
 *
 * Permission is hereby granted for use on all ESPRESSIF SYSTEMS products, in which case,
 * it is free of charge, to any person obtaining a copy of this software and associated
 * documentation files (the "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the Software is furnished
 * to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all copies or
 * substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 */

#include <string.h>
#include <Arduino.h>
#include <Wire.h>
#include "esp_log.h"
#include "es7210.h"
#include "bsp_loader.h"     // Fleet_BSP inclusion

// Note: Removed board.h dependency.

/* ES7210 address */
// If not defined externally, default to 0x40 (AD1=0, AD0=0)
#ifndef ES7210_ADDR
#define ES7210_ADDR (0x40) 
#endif

#define I2S_DSP_MODE          (0)
#define MCLK_DIV_FRE          (256)
#define FROM_PAD_PIN          (0)
#define FROM_CLOCK_DOUBLE_PIN (1)
#define ENABLE_TDM_MAX_NUM    (3)
#define ES7210_MCLK_SOURCE    (FROM_CLOCK_DOUBLE_PIN)  /* In master mode. 0 : MCLK from pad; 1 : MCLK from clock doubler */

static const char *TAG = "ES7210_HAL";

// Static handle to maintain state
static struct {
    es7210_input_mics_t mic_select;
    es7210_gain_value_t gain;
} es7210_handle;

// I2C Helper for Arduino
static int es7210_read_reg(uint8_t reg_addr)
{
    Wire.beginTransmission(ES7210_ADDR);
    Wire.write(reg_addr);
    Wire.endTransmission(false);
    Wire.requestFrom(ES7210_ADDR, (uint8_t)1);
    if (Wire.available()) {
        return Wire.read();
    }
    ESP_LOGE(TAG, "Failed to es7210 read 0x%x reg", reg_addr);
    return -1;
}

static esp_err_t es7210_write_reg(uint8_t reg_addr, uint8_t data)
{
    Wire.beginTransmission(ES7210_ADDR);
    Wire.write(reg_addr);
    Wire.write(data);
    return (Wire.endTransmission() == 0) ? ESP_OK : ESP_FAIL;
}

static esp_err_t es7210_update_reg_bit(uint8_t reg_addr, uint8_t update_bits, uint8_t data)
{
    int regv = es7210_read_reg(reg_addr);
    if (regv == -1) return ESP_FAIL;
    
    uint8_t val = (uint8_t)regv;
    val = (val & (~update_bits)) | (update_bits & data);
    return es7210_write_reg(reg_addr, val);
}

static int i2c_init()
{
    // Wire.begin() is assumed to be called externally
    return ESP_OK;
}

/*
 * Clock coefficient structer
 */
struct _coeff_div {
    uint32_t mclk;            /* mclk frequency */
    uint32_t lrck;            /* lrck */
    uint8_t  ss_ds;
    uint8_t  adc_div;         /* adcclk divider */
    uint8_t  dll;             /* dll_bypass */
    uint8_t  doubler;         /* doubler enable */
    uint8_t  osr;             /* adc osr */
    uint8_t  mclk_src;        /* select mclk  source */
    uint32_t lrck_h;          /* The high 4 bits of lrck */
    uint32_t lrck_l;          /* The low 8 bits of lrck */
};

/* Codec hifi mclk clock divider coefficients */
static const struct _coeff_div coeff_div[] = {
    //mclk      lrck    ss_ds adc_div  dll  doubler osr  mclk_src  lrckh   lrckl
    /* 8k */
    {12288000,  8000 ,  0x00,  0x03,  0x01,  0x00,  0x20,  0x00,    0x06,  0x00},
    {16384000,  8000 ,  0x00,  0x04,  0x01,  0x00,  0x20,  0x00,    0x08,  0x00},
    {19200000,  8000 ,  0x00,  0x1e,  0x00,  0x01,  0x28,  0x00,    0x09,  0x60},
    {4096000,   8000 ,  0x00,  0x01,  0x01,  0x00,  0x20,  0x00,    0x02,  0x00},

    /* 11.025k */
    {11289600,  11025,  0x00,  0x02,  0x01,  0x00,  0x20,  0x00,    0x01,  0x00},

    /* 12k */
    {12288000,  12000,  0x00,  0x02,  0x01,  0x00,  0x20,  0x00,    0x04,  0x00},
    {19200000,  12000,  0x00,  0x14,  0x00,  0x01,  0x28,  0x00,    0x06,  0x40},

    /* 16k */
    {4096000,   16000,  0x00,  0x01,  0x01,  0x01,  0x20,  0x00,    0x01,  0x00},
    {19200000,  16000,  0x00,  0x0a,  0x00,  0x00,  0x1e,  0x00,    0x04,  0x80},
    {16384000,  16000,  0x00,  0x02,  0x01,  0x00,  0x20,  0x00,    0x04,  0x00},
    {12288000,  16000,  0x00,  0x03,  0x01,  0x01,  0x20,  0x00,    0x03,  0x00},

    /* 22.05k */
    {11289600,  22050,  0x00,  0x01,  0x01,  0x00,  0x20,  0x00,    0x02,  0x00},

    /* 24k */
    {12288000,  24000,  0x00,  0x01,  0x01,  0x00,  0x20,  0x00,    0x02,  0x00},
    {19200000,  24000,  0x00,  0x0a,  0x00,  0x01,  0x28,  0x00,    0x03,  0x20},

    /* 32k */
    {12288000,  32000,  0x00,  0x03,  0x00,  0x00,  0x20,  0x00,    0x01,  0x80},
    {16384000,  32000,  0x00,  0x01,  0x01,  0x00,  0x20,  0x00,    0x02,  0x00},
    {19200000,  32000,  0x00,  0x05,  0x00,  0x00,  0x1e,  0x00,    0x02,  0x58},

    /* 44.1k */
    {11289600,  44100,  0x00,  0x01,  0x01,  0x01,  0x20,  0x00,    0x01,  0x00},

    /* 48k */
    {12288000,  48000,  0x00,  0x01,  0x01,  0x01,  0x20,  0x00,    0x01,  0x00},
    {19200000,  48000,  0x00,  0x05,  0x00,  0x01,  0x28,  0x00,    0x01,  0x90},

    /* 64k */
    {16384000,  64000,  0x01,  0x01,  0x01,  0x00,  0x20,  0x00,    0x01,  0x00},
    {19200000,  64000,  0x00,  0x05,  0x00,  0x01,  0x1e,  0x00,    0x01,  0x2c},

    /* 88.2k */
    {11289600,  88200,  0x01,  0x01,  0x01,  0x01,  0x20,  0x00,    0x00,  0x80},

    /* 96k */
    {12288000,  96000,  0x01,  0x01,  0x01,  0x01,  0x20,  0x00,    0x00,  0x80},
    {19200000,  96000,  0x01,  0x05,  0x00,  0x01,  0x28,  0x00,    0x00,  0xc8},
};

static int get_coeff(uint32_t mclk, uint32_t lrck)
{
    // In production, paste full table. For now, matching standard rates.
    for (int i = 0; i < (sizeof(coeff_div) / sizeof(coeff_div[0])); i++) {
        if (coeff_div[i].lrck == lrck && coeff_div[i].mclk == mclk)
            return i;
    }
    return -1; // Fail if exact match not found (or improve lookup logic)
}

int8_t get_es7210_mclk_src(void)
{
    return ES7210_MCLK_SOURCE;
}

esp_err_t es7210_config_sample(audio_hal_iface_samples_t sample)
{
    int coeff;
    int sample_fre = 0;
    int mclk_fre = 0;
    esp_err_t ret = ESP_OK;
    switch (sample) {
        case AUDIO_HAL_08K_SAMPLES: sample_fre = 8000; break;
        case AUDIO_HAL_11K_SAMPLES: sample_fre = 11025; break;
        case AUDIO_HAL_16K_SAMPLES: sample_fre = 16000; break;
        case AUDIO_HAL_22K_SAMPLES: sample_fre = 22050; break;
        case AUDIO_HAL_24K_SAMPLES: sample_fre = 24000; break;
        case AUDIO_HAL_32K_SAMPLES: sample_fre = 32000; break;
        case AUDIO_HAL_44K_SAMPLES: sample_fre = 44100; break;
        case AUDIO_HAL_48K_SAMPLES: sample_fre = 48000; break;
        default:
            ESP_LOGE(TAG, "Unable to configure sample rate %dHz", sample_fre);
            break;
    }
    mclk_fre = sample_fre * MCLK_DIV_FRE;
    coeff = get_coeff(mclk_fre, sample_fre);
    if (coeff < 0) {
        ESP_LOGE(TAG, "Unable to configure sample rate %dHz with %dHz MCLK", sample_fre, mclk_fre);
        return ESP_FAIL;
    }
    /* Set clock parammeters */
    if (coeff >= 0) {
        uint8_t regv;
        regv = es7210_read_reg(ES7210_MAINCLK_REG02) & 0x00;
        regv |= coeff_div[coeff].adc_div;
        regv |= coeff_div[coeff].doubler << 6;
        regv |= coeff_div[coeff].dll << 7;
        ret |= es7210_write_reg(ES7210_MAINCLK_REG02, regv);
        regv = coeff_div[coeff].osr;
        ret |= es7210_write_reg(ES7210_OSR_REG07, regv);
        regv = coeff_div[coeff].lrck_h;
        ret |= es7210_write_reg(ES7210_LRCK_DIVH_REG04, regv);
        regv = coeff_div[coeff].lrck_l;
        ret |= es7210_write_reg(ES7210_LRCK_DIVL_REG05, regv);
    }
    return ret;
}

esp_err_t es7210_mic_select(es7210_input_mics_t mic_select)
{
    esp_err_t ret = ESP_OK;
    uint16_t mic_num = 0;
    es7210_handle.mic_select = mic_select;
    if (es7210_handle.mic_select & (ES7210_INPUT_MIC1 | ES7210_INPUT_MIC2 | ES7210_INPUT_MIC3 | ES7210_INPUT_MIC4)) {
        for (int i = 0; i < 4; i++) {
            ret |= es7210_update_reg_bit(ES7210_MIC1_GAIN_REG43 + i, 0x10, 0x00);
        }
        ret |= es7210_write_reg(ES7210_MIC12_POWER_REG4B, 0xff);
        ret |= es7210_write_reg(ES7210_MIC34_POWER_REG4C, 0xff);
        if (es7210_handle.mic_select & ES7210_INPUT_MIC1) {
            ret |= es7210_update_reg_bit(ES7210_CLOCK_OFF_REG01, 0x0b, 0x00);
            ret |= es7210_write_reg(ES7210_MIC12_POWER_REG4B, 0x00);
            ret |= es7210_update_reg_bit(ES7210_MIC1_GAIN_REG43, 0x10, 0x10);
            ret |= es7210_update_reg_bit(ES7210_MIC1_GAIN_REG43, 0x0f, es7210_handle.gain);
        }
        if (es7210_handle.mic_select & ES7210_INPUT_MIC2) {
            ret |= es7210_update_reg_bit(ES7210_CLOCK_OFF_REG01, 0x0b, 0x00);
            ret |= es7210_write_reg(ES7210_MIC12_POWER_REG4B, 0x00);
            ret |= es7210_update_reg_bit(ES7210_MIC2_GAIN_REG44, 0x10, 0x10);
            ret |= es7210_update_reg_bit(ES7210_MIC2_GAIN_REG44, 0x0f, es7210_handle.gain);
        }
        if (es7210_handle.mic_select & ES7210_INPUT_MIC3) {
            ret |= es7210_update_reg_bit(ES7210_CLOCK_OFF_REG01, 0x15, 0x00);
            ret |= es7210_write_reg(ES7210_MIC34_POWER_REG4C, 0x00);
            ret |= es7210_update_reg_bit(ES7210_MIC3_GAIN_REG45, 0x10, 0x10);
            ret |= es7210_update_reg_bit(ES7210_MIC3_GAIN_REG45, 0x0f, es7210_handle.gain);
        }
        if (es7210_handle.mic_select & ES7210_INPUT_MIC4) {
            ret |= es7210_update_reg_bit(ES7210_CLOCK_OFF_REG01, 0x15, 0x00);
            ret |= es7210_write_reg(ES7210_MIC34_POWER_REG4C, 0x00);
            ret |= es7210_update_reg_bit(ES7210_MIC4_GAIN_REG46, 0x10, 0x10);
            ret |= es7210_update_reg_bit(ES7210_MIC4_GAIN_REG46, 0x0f, es7210_handle.gain);
        }
    } else {
        ESP_LOGE(TAG, "Microphone selection error");
        return ESP_FAIL;
    }

    for (int i = ES7210_INPUT_MIC1; i <= ES7210_INPUT_MIC4; i = i << 1) {
        if (es7210_handle.mic_select & i) {
            mic_num++;
        }
    }
    // AUTOMATIC TDM ENABLER Logic from Driver:
    if (mic_num >= ENABLE_TDM_MAX_NUM) {
        ret |= es7210_write_reg(ES7210_SDP_INTERFACE2_REG12, 0x02);
        ESP_LOGW(TAG, "Enable TDM mode. ES7210_SDP_INTERFACE2_REG12: %X", es7210_read_reg(ES7210_SDP_INTERFACE2_REG12));
    } else {
        ret |= es7210_write_reg(ES7210_SDP_INTERFACE2_REG12, 0x00);
        ESP_LOGW(TAG, "Disable TDM mode. ES7210_SDP_INTERFACE2_REG12: %X", es7210_read_reg(ES7210_SDP_INTERFACE2_REG12));
    }
    return ret;
}

esp_err_t es7210_adc_init(audio_hal_codec_config_t *codec_cfg)
{
    esp_err_t ret = ESP_OK;
    i2c_init();
    ret |= es7210_write_reg(ES7210_RESET_REG00, 0xff);
    ret |= es7210_write_reg(ES7210_RESET_REG00, 0x41);
    ret |= es7210_write_reg(ES7210_CLOCK_OFF_REG01, 0x3f);
    ret |= es7210_write_reg(ES7210_TIME_CONTROL0_REG09, 0x30);  /* Set chip state cycle */
    ret |= es7210_write_reg(ES7210_TIME_CONTROL1_REG0A, 0x30);  /* Set power on state cycle */
    ret |= es7210_write_reg(ES7210_ADC12_HPF2_REG23, 0x2a);     /* Quick setup */
    ret |= es7210_write_reg(ES7210_ADC12_HPF1_REG22, 0x0a);
    ret |= es7210_write_reg(ES7210_ADC34_HPF2_REG20, 0x0a);
    ret |= es7210_write_reg(ES7210_ADC34_HPF1_REG21, 0x2a);
    
    audio_hal_codec_i2s_iface_t *i2s_cfg = &(codec_cfg->i2s_iface);
    switch (i2s_cfg->mode) {
        case AUDIO_HAL_MODE_MASTER:
            ESP_LOGI(TAG, "ES7210 in Master mode");
            ret |= es7210_update_reg_bit(ES7210_MODE_CONFIG_REG08, 0x01, 0x01);
            switch (get_es7210_mclk_src()) {
                case FROM_PAD_PIN:
                    ret |= es7210_update_reg_bit(ES7210_MASTER_CLK_REG03, 0x80, 0x00);
                    break;
                case FROM_CLOCK_DOUBLE_PIN:
                    ret |= es7210_update_reg_bit(ES7210_MASTER_CLK_REG03, 0x80, 0x80);
                    break;
                default:
                    ret |= es7210_update_reg_bit(ES7210_MASTER_CLK_REG03, 0x80, 0x00);
                    break;
            }
            break;
        case AUDIO_HAL_MODE_SLAVE:
            ESP_LOGI(TAG, "ES7210 in Slave mode");
            ret |= es7210_update_reg_bit(ES7210_MODE_CONFIG_REG08, 0x01, 0x00);
            break;
        default:
            ret |= es7210_update_reg_bit(ES7210_MODE_CONFIG_REG08, 0x01, 0x00);
    }
    ret |= es7210_write_reg(ES7210_ANALOG_REG40, 0x43);      /* Select power off analog, vdda = 3.3V, close vx20ff, VMID select 5KΩ start */
    ret |= es7210_write_reg(ES7210_MIC12_BIAS_REG41, 0x70);  /* Select 2.87v */
    ret |= es7210_write_reg(ES7210_MIC34_BIAS_REG42, 0x70);  /* Select 2.87v */
    ret |= es7210_write_reg(ES7210_OSR_REG07, 0x20);
    ret |= es7210_write_reg(ES7210_MAINCLK_REG02, 0xc1);  
    ret |= es7210_config_sample(i2s_cfg->samples);

    // Mic selection/gain come from the active board's BSP (Fleet_Hardware_Config), not a
    // hardcoded default, so each device can use its own tested mic configuration.
    #ifdef ENABLE_AEC
        es7210_input_mics_t mic_sel  = (es7210_input_mics_t)bsp_audio.AEC_MIC_SELECTED;
        es7210_gain_value_t mic_gain = (es7210_gain_value_t)bsp_audio.AEC_MIC_GAIN_DB;
    #else
        es7210_input_mics_t mic_sel  = (es7210_input_mics_t)bsp_audio.MIC_SELECTED;
        es7210_gain_value_t mic_gain = (es7210_gain_value_t)bsp_audio.MIC_GAIN_DB;
    #endif
    ret |= es7210_mic_select(mic_sel);
    ret |= es7210_adc_set_gain(mic_sel, mic_gain);
    return ESP_OK;
}

esp_err_t es7210_adc_deinit()
{
    // i2c_bus_delete(es7210_handle.i2c_handle);
    return ESP_OK;
}

esp_err_t es7210_config_fmt(audio_hal_iface_format_t fmt)
{
    esp_err_t ret = ESP_OK;
    uint8_t adc_iface = 0;
    adc_iface = es7210_read_reg(ES7210_SDP_INTERFACE1_REG11);
    adc_iface &= 0xfc;
    switch (fmt) {
        case AUDIO_HAL_I2S_NORMAL:
            ESP_LOGD(TAG, "ES7210 in I2S Format");
            adc_iface |= 0x00;
            break;
        case AUDIO_HAL_I2S_LEFT:
        case AUDIO_HAL_I2S_RIGHT:
            ESP_LOGD(TAG, "ES7210 in LJ Format");
            adc_iface |= 0x01;
            break;
        case AUDIO_HAL_I2S_DSP:
            if (I2S_DSP_MODE) { // DSP-A
                adc_iface |= 0x13;
            } else {            // DSP-B
                adc_iface |= 0x03;
            }
            break;
        default:
            adc_iface &= 0xfc;
            break;
    }
    ret |= es7210_write_reg(ES7210_SDP_INTERFACE1_REG11, adc_iface);
    ESP_LOGI(TAG, "config fmt %X", adc_iface);
    return ret;
}

esp_err_t es7210_set_bits(audio_hal_iface_bits_t bits)
{
    esp_err_t ret = ESP_OK;
    uint8_t adc_iface = 0;
    adc_iface = es7210_read_reg(ES7210_SDP_INTERFACE1_REG11);
    adc_iface &= 0x1f;
    switch (bits) {
        case AUDIO_HAL_BIT_LENGTH_16BITS:
            adc_iface |= 0x60;
            break;
        case AUDIO_HAL_BIT_LENGTH_24BITS:
            adc_iface |= 0x00;
            break;
        case AUDIO_HAL_BIT_LENGTH_32BITS:
            adc_iface |= 0x80;
            break;
        default:
            adc_iface |= 0x60;
            break;
    }
    ret |= es7210_write_reg(ES7210_SDP_INTERFACE1_REG11, adc_iface);
    return ret;
}

esp_err_t es7210_adc_config_i2s(audio_hal_codec_mode_t mode, audio_hal_codec_i2s_iface_t *iface)
{
    esp_err_t ret = ESP_OK;
    ret |= es7210_set_bits(iface->bits);
    ret |= es7210_config_fmt(iface->fmt);
    ret |= es7210_config_sample(iface->samples);
    return ret;
}

esp_err_t es7210_start(uint8_t clock_reg_value)
{
    esp_err_t ret = ESP_OK;
    ret |= es7210_write_reg(ES7210_CLOCK_OFF_REG01, clock_reg_value);
    ret |= es7210_write_reg(ES7210_POWER_DOWN_REG06, 0x00);
    ret |= es7210_write_reg(ES7210_ANALOG_REG40, 0x43);
    ret |= es7210_write_reg(ES7210_MIC1_POWER_REG47, 0x00);
    ret |= es7210_write_reg(ES7210_MIC2_POWER_REG48, 0x00);
    ret |= es7210_write_reg(ES7210_MIC3_POWER_REG49, 0x00);
    ret |= es7210_write_reg(ES7210_MIC4_POWER_REG4A, 0x00);
    ret |= es7210_mic_select(es7210_handle.mic_select);
    return ret;
}

esp_err_t es7210_stop(void)
{
    esp_err_t ret = ESP_OK;
    ret |= es7210_write_reg(ES7210_MIC1_POWER_REG47, 0xff);
    ret |= es7210_write_reg(ES7210_MIC2_POWER_REG48, 0xff);
    ret |= es7210_write_reg(ES7210_MIC3_POWER_REG49, 0xff);
    ret |= es7210_write_reg(ES7210_MIC4_POWER_REG4A, 0xff);
    ret |= es7210_write_reg(ES7210_MIC12_POWER_REG4B,0xff);
    ret |= es7210_write_reg(ES7210_MIC34_POWER_REG4C, 0xff);
    ret |= es7210_write_reg(ES7210_CLOCK_OFF_REG01, 0x7f);
    ret |= es7210_write_reg(ES7210_POWER_DOWN_REG06, 0x07);
    return ret;
}

esp_err_t es7210_adc_ctrl_state(audio_hal_codec_mode_t mode, audio_hal_ctrl_t ctrl_state)
{
    static uint8_t regv;
    esp_err_t ret = ESP_OK;
    ret = es7210_read_reg(ES7210_CLOCK_OFF_REG01);
    if ((ret != 0x7f) && (ret != 0xff)) {
        regv = es7210_read_reg(ES7210_CLOCK_OFF_REG01);
    }
    if (ctrl_state == AUDIO_HAL_CTRL_START) {
        ret |= es7210_start(regv);
    } else {
        regv = es7210_read_reg(ES7210_CLOCK_OFF_REG01);
        ret |= es7210_stop();
    }
    return ret;
}

esp_err_t es7210_adc_set_gain(es7210_input_mics_t mic_select, es7210_gain_value_t gain)
{
    esp_err_t ret = ESP_OK;
    if (gain < GAIN_0DB) gain = GAIN_0DB;
    if (gain > GAIN_37_5DB) gain = GAIN_37_5DB;

    if (mic_select & ES7210_INPUT_MIC1) ret |= es7210_update_reg_bit(ES7210_MIC1_GAIN_REG43, 0x0f, gain);
    if (mic_select & ES7210_INPUT_MIC2) ret |= es7210_update_reg_bit(ES7210_MIC2_GAIN_REG44, 0x0f, gain);
    if (mic_select & ES7210_INPUT_MIC3) ret |= es7210_update_reg_bit(ES7210_MIC3_GAIN_REG45, 0x0f, gain);
    if (mic_select & ES7210_INPUT_MIC4) ret |= es7210_update_reg_bit(ES7210_MIC4_GAIN_REG46, 0x0f, gain);
    
    es7210_handle.gain = gain;
    return ret;
}

esp_err_t es7210_adc_set_volume(int volume)
{
    if (volume > GAIN_37_5DB) volume = GAIN_37_5DB;
    return es7210_adc_set_gain(es7210_handle.mic_select, (es7210_gain_value_t)volume);
}

esp_err_t es7210_set_mute(bool enable)
{
    esp_err_t ret = 0;
    if (enable) {
        ret |= es7210_update_reg_bit(ES7210_ADC34_MUTERANGE_REG14, 0x03, 0x03);
        ret |= es7210_update_reg_bit(ES7210_ADC12_MUTERANGE_REG15, 0x03, 0x03);
    } else {
        ret |= es7210_update_reg_bit(ES7210_ADC34_MUTERANGE_REG14, 0x03, 0x00);
        ret |= es7210_update_reg_bit(ES7210_ADC12_MUTERANGE_REG15, 0x03, 0x00);
    }
    return ret == 0 ? ESP_OK : ESP_FAIL;
}

void es7210_read_all(void)
{
    for (int i = 0; i <= 0x4E; i++) {
        uint8_t reg = es7210_read_reg(i);
        ESP_LOGI(TAG, "REG:%02x, %02x", i, reg);
    }
}