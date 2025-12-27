#pragma once
#include <lvgl.h>
#include "AudioManager.h"
#include "UiToolkit.h"

class Panel_Audio {
public:
    Panel_Audio(AudioManager& mgr);
    void init(lv_obj_t* parent);
    void tick();

private:
    AudioManager& _audio;
    lv_obj_t* pnl_content;
    lv_obj_t* row_ctrls;
    lv_obj_t* row_btns;
    lv_obj_t* row_vu_meter;
    lv_obj_t* col_vol;
    
    // Controls
    lv_obj_t* slider_vol;
    lv_obj_t* btn_tone;
    lv_obj_t* btn_pcm;
    lv_obj_t* lbl_tone_text;
    lv_obj_t* lbl_pcm_text;
    lv_obj_t* bar_vu_l;
    lv_obj_t* bar_vu_r;
    
    // Logic Flags
    bool _trigger_rec;
    bool _enabled;

    // Recording Data
    int16_t *rec_buffer;
    size_t rec_buffer_size;

    void process_recording_sequence();

    // Static Callbacks
    static void sw_enable_cb(lv_event_t * e);
    static void slider_vol_cb(lv_event_t * e);
    static void btn_tone_cb(lv_event_t * e);
    static void btn_pcm_cb(lv_event_t * e);
};