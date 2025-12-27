#include "Panel_Audio.h"
#include "UIToolkit.h"

#define REC_LIMIT_SECONDS 5.0f

Panel_Audio::Panel_Audio(AudioManager& mgr) : _audio(mgr) {
    _trigger_rec = false;
    _enabled = true; // Default state of Switch
    rec_buffer = NULL;
}

void Panel_Audio::sw_enable_cb(lv_event_t * e) {
    Panel_Audio* p = (Panel_Audio*)lv_event_get_user_data(e);
    lv_obj_t* target = (lv_obj_t*)lv_event_get_target(e);
    bool state = lv_obj_has_state(target, LV_STATE_CHECKED);
    p->_enabled = state;
    
    if(state) {
        p->_audio.setMute(false);
        UiToolkit::show_toast("Audio Enabled");
        lv_obj_remove_state (p->btn_tone, LV_STATE_DISABLED);
        lv_obj_remove_state (p->btn_pcm, LV_STATE_DISABLED);
    } else {
        p->_audio.setMute(true);
        UiToolkit::show_toast("Audio Muted");
        lv_obj_add_state (p->btn_tone, LV_STATE_DISABLED);
        lv_obj_add_state (p->btn_pcm, LV_STATE_DISABLED);
    }
}

void Panel_Audio::slider_vol_cb(lv_event_t * e) {
    Panel_Audio* p = (Panel_Audio*)lv_event_get_user_data(e);
    lv_obj_t* target = (lv_obj_t*)lv_event_get_target(e);
    int32_t val = lv_slider_get_value(target);
    p->_audio.setVolume(val);
    
    char buf[32];
    snprintf(buf, sizeof(buf), "Vol: %d%%", (int)val);
    UiToolkit::show_toast(buf, 800);
}

void Panel_Audio::btn_tone_cb(lv_event_t * e) {
    Panel_Audio* p = (Panel_Audio*)lv_event_get_user_data(e);
    p->_audio.tone(440, 200);
    UiToolkit::show_toast("Playing Test Tone", 650);
}

void Panel_Audio::btn_pcm_cb(lv_event_t * e) {
    Panel_Audio* p = (Panel_Audio*)lv_event_get_user_data(e);
    lv_label_set_text(p->lbl_pcm_text, "WAIT...");
    p->_trigger_rec = true; 
}

void Panel_Audio::init(lv_obj_t* parent) {
    
    UiToolkit::create_collapsible_panel(parent, "AUDIO", &pnl_content);

    // ROW 1: Controls (Switch on Left, Slider on Right)
    UiToolkit::create_panel_row(pnl_content, &row_ctrls);

    // ROW 1,1 COL 1 - Switch/Enable Column
    // Container for Switch + Label
    lv_obj_t * col_sw = lv_obj_create(row_ctrls);
    lv_obj_set_size             (col_sw, UiToolkit::sc(70), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow        (col_sw, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align       (col_sw, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_bg_opa     (col_sw, LV_OPA_0, 0);
    lv_obj_set_style_border_width(col_sw, 0, 0);
    lv_obj_set_style_pad_left   (col_sw, UiToolkit::sc(5), 0);
    lv_obj_set_style_pad_right  (col_sw, UiToolkit::sc(5), 0);
    lv_obj_set_style_pad_row    (col_sw, UiToolkit::sc(6), 0);
    lv_obj_remove_flag          (col_sw, LV_OBJ_FLAG_SCROLLABLE);

    // ROW 1,1 COL 1,1 - Label
    lv_obj_t * lbl_en = lv_label_create(col_sw);
    lv_obj_set_size             (lbl_en, lv_pct(100), LV_SIZE_CONTENT);
    lv_label_set_text           (lbl_en, "ENABLE");
    lv_obj_set_align            (lbl_en, LV_ALIGN_CENTER);
    lv_obj_set_style_text_font  (lbl_en, UiToolkit::Font_Label, 0); // Semantic Font
    lv_obj_set_style_text_color (lbl_en, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_align (lbl_en, LV_TEXT_ALIGN_CENTER, 0);

    // ROW 1,1 COL 1,2 - Switch
    lv_obj_t * sw = lv_switch_create(col_sw);
    lv_obj_set_size     (sw, UiToolkit::sc(60), UiToolkit::sc(25));
    lv_obj_set_align    (sw, LV_ALIGN_CENTER);
    lv_obj_add_state    (sw, LV_STATE_CHECKED);
    lv_obj_add_event_cb (sw, sw_enable_cb, LV_EVENT_VALUE_CHANGED, this);

    // ROW 1,2 COL 2 - Volume Slider Column
    // Column container with 2 rows: Label + Slider
    UiToolkit::create_slider_col(row_ctrls, "VOLUME", &col_vol, &slider_vol);
    // lv_obj_set_style_pad_bottom (col_vol, UiToolkit::sc(10), 0);

    lv_slider_set_value         (slider_vol, _audio.getVolume(), LV_ANIM_OFF);
    lv_obj_add_event_cb         (slider_vol, slider_vol_cb, LV_EVENT_VALUE_CHANGED, this);

    // ROW 2 - Buttons Row
    UiToolkit::create_panel_row (pnl_content, &row_btns);

    // ROW 2,1 - Test Tone Button
    btn_tone = lv_button_create (row_btns);
    lv_obj_set_height           (btn_tone, UiToolkit::sc(35));
    lv_obj_set_style_max_width  (btn_tone, UiToolkit::sc(100), 0);
    lv_obj_set_flex_grow        (btn_tone, 1);
    lv_obj_add_event_cb         (btn_tone, btn_tone_cb, LV_EVENT_CLICKED, this);
    lbl_tone_text             = lv_label_create(btn_tone); 
    lv_label_set_text           (lbl_tone_text, "TONE"); 
    lv_obj_set_style_text_font  (lbl_tone_text, UiToolkit::Font_Button, 0); // Semantic Font
    lv_obj_center               (lbl_tone_text);

    // ROW 2,2 - Record Loop Button
    btn_pcm = lv_button_create(row_btns);
    lv_obj_set_height           (btn_pcm, UiToolkit::sc(35));
    lv_obj_set_style_max_width  (btn_pcm, UiToolkit::sc(100), 0);
    lv_obj_set_flex_grow        (btn_pcm, 1);
    lv_obj_add_event_cb         (btn_pcm, btn_pcm_cb, LV_EVENT_CLICKED, this);
    lbl_pcm_text              = lv_label_create(btn_pcm); 
    lv_label_set_text           (lbl_pcm_text, "REC LOOP"); 
    lv_obj_set_style_text_font  (lbl_pcm_text, UiToolkit::Font_Button, 0); // Semantic Font
    lv_obj_center               (lbl_pcm_text);

    // ROW 3 - VU Meter
    UiToolkit::create_panel_row(pnl_content, &row_vu_meter);
    lv_obj_set_height(row_vu_meter, UiToolkit::sc(ROW_HEIGHT / 2));

    // ROW 3,1 - Left Channel Container (Dark Grey Background)
    lv_obj_t* meter_l_bg = lv_obj_create(row_vu_meter); 
    lv_obj_set_height               (meter_l_bg, lv_pct(100));
    lv_obj_set_flex_grow            (meter_l_bg, 1);
    lv_obj_set_style_bg_color       (meter_l_bg, lv_color_hex(0x303030), 0);
    lv_obj_set_style_border_width   (meter_l_bg, 0, 0);
    lv_obj_set_style_pad_all        (meter_l_bg, 0, 0);

    bar_vu_l = lv_obj_create(meter_l_bg);
    lv_obj_set_size               (bar_vu_l, lv_pct(0), lv_pct(100));
    lv_obj_set_style_bg_color     (bar_vu_l, lv_palette_main(LV_PALETTE_GREEN), 0);
    lv_obj_set_style_border_width (bar_vu_l, 0, 0);

    // ROW 3,2 - Right Channel Container (Dark Grey Background)
    lv_obj_t* meter_r_bg = lv_obj_create(row_vu_meter); 
    lv_obj_set_height               (meter_r_bg, lv_pct(100));
    lv_obj_set_flex_grow            (meter_r_bg, 1);
    lv_obj_set_style_bg_color       (meter_r_bg, lv_color_hex(0x303030), 0);
    lv_obj_set_style_border_width   (meter_r_bg, 0,0);
    lv_obj_set_style_pad_all        (meter_r_bg, 0, 0);
    
    bar_vu_r = lv_obj_create(meter_r_bg);
    lv_obj_set_size               (bar_vu_r, lv_pct(0), lv_pct(100));
    lv_obj_set_style_bg_color     (bar_vu_r, lv_palette_main(LV_PALETTE_GREEN), 0);
    lv_obj_set_style_border_width (bar_vu_r,0,0);
}

void Panel_Audio::tick() {
    if (_trigger_rec) {
        _trigger_rec = false;
        process_recording_sequence();
    }

    if (_enabled) {
        int mic_vol = _audio.getMicLevel();
        // Smoothing
        static int smooth_vol = 0;
        if (mic_vol > smooth_vol) smooth_vol = mic_vol;
        else smooth_vol = (smooth_vol * 90 + mic_vol * 10) / 100;

        if (bar_vu_l) lv_obj_set_width(bar_vu_l, lv_pct(smooth_vol));
        if (bar_vu_r) lv_obj_set_width(bar_vu_r, lv_pct(smooth_vol));
    }
}

void Panel_Audio::process_recording_sequence() {
    // 1. Alloc
    if (rec_buffer == NULL) {
        rec_buffer_size = 16000 * 2 * 2 * (int)REC_LIMIT_SECONDS; 
        rec_buffer = (int16_t*)heap_caps_malloc(rec_buffer_size, MALLOC_CAP_SPIRAM);
        if (!rec_buffer) {
            UiToolkit::show_toast("Alloc Failed!", 3000);
            lv_label_set_text(lbl_pcm_text, "ERR RAM");
            return;
        }
    }

    lv_label_set_text(lbl_pcm_text, "REC...");
    UiToolkit::show_toast("Recording...", (REC_LIMIT_SECONDS * 1000));
    lv_timer_handler();
    delay(50);

    // 2. Record
    size_t samples_total = 16000 * 2 * REC_LIMIT_SECONDS;
    size_t samples_read = 0;
    
    // Clear trash
    int16_t trash[128]; _audio.readRaw(trash, 128);

    while(samples_read < samples_total) {
        size_t to_read = (samples_total - samples_read > 128) ? 128 : (samples_total - samples_read);
        size_t read = _audio.readRaw(&rec_buffer[samples_read], to_read);
        if (read > 0) {
            // Digital Gain
            for (size_t i = 0; i < read; i++) {
                int32_t val = (int32_t)rec_buffer[samples_read + i] * 4;
                if (val > 32767) val = 32767; else if (val < -32768) val = -32768;
                rec_buffer[samples_read + i] = (int16_t)val;
            }
            samples_read += read;
        } else {
            delay(1);
        }
        if (samples_read % 4000 == 0) lv_timer_handler(); // Keep UI alive
    }

    lv_label_set_text(lbl_pcm_text, "PLAY...");
    UiToolkit::show_toast("Playback...", (REC_LIMIT_SECONDS * 1000));
    lv_timer_handler();

    // 3. Play
    size_t samples_written = 0;
    while(samples_written < samples_total) {
        size_t to_write = (samples_total - samples_written > 128) ? 128 : (samples_total - samples_written);
        size_t written = _audio.writeRaw(&rec_buffer[samples_written], to_write);
        if (written > 0) samples_written += written;
        else delay(1);
        if (samples_written % 4000 == 0) lv_timer_handler();
    }

    lv_label_set_text(lbl_pcm_text, "REC LOOP");
    UiToolkit::show_toast("Done", 1000);
}