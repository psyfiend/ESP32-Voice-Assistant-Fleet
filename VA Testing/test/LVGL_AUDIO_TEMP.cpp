// <--- UPDATED: Added Buffer Analysis to debug silence issue
#ifndef BOARD_HAS_PSRAM
#error "Error: This program requires PSRAM enabled, please enable PSRAM option in 'Tools' menu of Arduino IDE"
#endif

#include <Arduino.h>
#include <Arduino_GFX_Library.h>
#include <lvgl.h>
#include <Wire.h>

// --- FORCE DEPENDENCIES ---
#include <bb_captouch.h> 
#ifdef CONFIG_IDF_TARGET_ESP32SP4
    #include "Fleet_BSP_P4.h"
#else
    #include "Fleet_BSP.h"   
#endif
#ifdef BSP_HEADER
    #include BSP_HEADER 
#endif
// --------------------------

#include "DisplayManager.h"
#include "TouchManager.h"
#include "AudioManager.h" 

// --- TOUCH CONFIG ---
#define TOUCH_SWAP_XY  false
#define TOUCH_INV_X    false
#define TOUCH_INV_Y    false

// --- Global Objects ---
DisplayManager displayMgr;
TouchManager touchMgr;
AudioManager audioMgr; 

// --- Audio Test Globals ---
#define REC_LIMIT_SECONDS 4.0f
int16_t *rec_buffer = NULL;
size_t rec_buffer_size = 0;
bool trigger_audio_test = false; // Flag to decouple UI from Processing

// --- LVGL Globals ---
static uint16_t *lv_draw_buf;
#define LV_BUF_SIZE (cfg.WIDTH * cfg.HEIGHT / 10)

// --- UI Constants ---
#define PNL_COLLAPSED_H 85
#define PNL_EXPANDED_H  280

// --- UI Objects ---
lv_obj_t *cursors[5];       
lv_obj_t *coord_labels[5];  
lv_obj_t *count_label;      
lv_obj_t *panel_touch_data; 
lv_obj_t *toast_panel;
lv_obj_t *toast_label;
lv_timer_t *toast_timer_handle = NULL;

lv_obj_t *lbl_stats_ram;    
lv_obj_t *lbl_stats_time;

// Audio Panel Objects
lv_obj_t *pnl_audio;
lv_obj_t *pnl_audio_content; 
lv_obj_t *slider_vol;
lv_obj_t *btn_tone;
lv_obj_t *btn_pcm;
lv_obj_t *lbl_pcm_text; // Pointer to label inside btn_pcm
lv_obj_t *bar_vu_l; 
lv_obj_t *bar_vu_r; 
bool audio_expanded = false;

// Display Panel Objects
lv_obj_t *pnl_disp;
lv_obj_t *pnl_disp_content;
lv_obj_t *slider_bri;
bool disp_expanded = false;

// --- State Management ---
TouchPoint activePoints[5]; 
uint8_t activeTouchCount = 0;
bool showTouches = true; 
uint32_t lastStatsUpdate = 0;

// -------------------------------------------------------------------------
// LVGL Standard Flushing & Input
// -------------------------------------------------------------------------
void my_disp_flush(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map) {
    uint32_t w = (area->x2 - area->x1 + 1);
    uint32_t h = (area->y2 - area->y1 + 1);
    Arduino_GFX *gfx = displayMgr.getGfx();
    gfx->draw16bitRGBBitmap(area->x1, area->y1, (uint16_t *)px_map, w, h);
    gfx->flush();
    lv_display_flush_ready(disp);
}

void my_touch_read(lv_indev_t *indev, lv_indev_data_t *data) {
    if (activeTouchCount > 0) {
        data->state = LV_INDEV_STATE_PRESSED;
        int16_t x = activePoints[0].x;
        int16_t y = activePoints[0].y;
        if(TOUCH_SWAP_XY) { int16_t temp = x; x = y; y = temp; }
        if(TOUCH_INV_X)   x = cfg.WIDTH - 1 - x;
        if(TOUCH_INV_Y)   y = cfg.HEIGHT - 1 - y;
        data->point.x = x;
        data->point.y = y;
    } else {
        data->state = LV_INDEV_STATE_RELEASED;
    }
}

uint32_t my_tick_get_cb(void) { return millis(); }

// -------------------------------------------------------------------------
// UI Helpers
// -------------------------------------------------------------------------
static void drag_event_handler(lv_event_t * e) {
    lv_obj_t * obj = (lv_obj_t *)lv_event_get_target(e);
    lv_indev_t * indev = lv_indev_get_act();
    if(indev == NULL) return;
    
    lv_point_t vect;
    lv_indev_get_vect(indev, &vect);

    int32_t x = lv_obj_get_style_x(obj, LV_PART_MAIN) + vect.x;
    int32_t y = lv_obj_get_style_y(obj, LV_PART_MAIN) + vect.y;

    lv_obj_set_pos(obj, x, y);
}

void update_system_stats() {
    float freeRam = ESP.getFreePsram() / 1024.0 / 1024.0;
    int ram_int = (int)freeRam;
    int ram_dec = (int)((freeRam - ram_int) * 10);
    lv_label_set_text_fmt(lbl_stats_ram, "RAM: %d.%d MB", ram_int, ram_dec);

    uint32_t s = millis() / 1000;
    uint32_t m = s / 60;
    lv_label_set_text_fmt(lbl_stats_time, "UP: %02d:%02d", m, s % 60);
}

// --- Accordion Animation ---
static void anim_height_cb(void * var, int32_t v) {
    lv_obj_set_height((lv_obj_t*)var, v);
}

void toggle_panel(lv_obj_t * panel, bool &expanded) {
    expanded = !expanded;
    
    // --- New Icon Toggling Logic ---
    lv_obj_t * header = lv_obj_get_child(panel, 0);
    if (header) {
        lv_obj_t * icon = lv_obj_get_child(header, 1); 
        if (icon) {
            lv_label_set_text(icon, expanded ? LV_SYMBOL_DOWN : LV_SYMBOL_UP);
        }
    }

    int32_t h_start = lv_obj_get_height(panel);
    int32_t h_end = expanded ? PNL_EXPANDED_H : PNL_COLLAPSED_H; 

    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, panel);
    lv_anim_set_values(&a, h_start, h_end);
    lv_anim_set_time(&a, 300); 
    lv_anim_set_exec_cb(&a, anim_height_cb);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_in_out);
    lv_anim_start(&a);
}

// Logic to ensure only one is open
static void panel_header_click_cb(lv_event_t * e) {
    lv_obj_t * panel = (lv_obj_t*)lv_event_get_user_data(e);
    
    if (panel == pnl_audio) {
        if (!audio_expanded) {
            if (disp_expanded) toggle_panel(pnl_disp, disp_expanded);
        }
        toggle_panel(pnl_audio, audio_expanded);
    } 
    else if (panel == pnl_disp) {
        if (!disp_expanded) {
            if (audio_expanded) toggle_panel(pnl_audio, audio_expanded);
        }
        toggle_panel(pnl_disp, disp_expanded);
    }
}

// -------------------------------------------------------------------------
// Toast System
// -------------------------------------------------------------------------
static void toast_timer_cb(lv_timer_t * t) {
    if (toast_panel) lv_obj_add_flag(toast_panel, LV_OBJ_FLAG_HIDDEN);
    toast_timer_handle = NULL;
}

void show_toast(const char* text, uint32_t duration_ms = 2000) {
    if (!toast_panel || !toast_label) return;

    lv_label_set_text(toast_label, text);
    lv_obj_clear_flag(toast_panel, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(toast_panel); // Ensure top z-index

    // Manage Timer
    if (toast_timer_handle) {
        lv_timer_del(toast_timer_handle);
        toast_timer_handle = NULL;
    }
    toast_timer_handle = lv_timer_create(toast_timer_cb, duration_ms, NULL);
    lv_timer_set_repeat_count(toast_timer_handle, 1); // One-shot
}

// -------------------------------------------------------------------------
// UI Callbacks
// -------------------------------------------------------------------------
static void sw_enable_cb(lv_event_t * e) {
    lv_obj_t * sw = (lv_obj_t*)lv_event_get_target(e);
    bool state = lv_obj_has_state(sw, LV_STATE_CHECKED);
    if (state) {
        lv_obj_remove_state(btn_tone, LV_STATE_DISABLED);
        lv_obj_remove_state(btn_pcm, LV_STATE_DISABLED);
        lv_obj_remove_state(slider_vol, LV_STATE_DISABLED);
        audioMgr.setMute(false);
        show_toast("Audio Enabled");
    } else {
        lv_obj_add_state(btn_tone, LV_STATE_DISABLED);
        lv_obj_add_state(btn_pcm, LV_STATE_DISABLED);
        lv_obj_add_state(slider_vol, LV_STATE_DISABLED);
        audioMgr.setMute(true);
        show_toast("Audio Muted");
    }
}

static void slider_vol_cb(lv_event_t * e) {
    lv_obj_t * slider = (lv_obj_t*)lv_event_get_target(e);
    int32_t val = lv_slider_get_value(slider);
    audioMgr.setVolume(val);
    
    // Show Toast
    char buf[32];
    snprintf(buf, sizeof(buf), "Volume: %d%%", (int)val);
    show_toast(buf, 1000); 
}

static void btn_tone_cb(lv_event_t * e) {
    audioMgr.tone(440, 200); 
    show_toast("Playing Tone...", 500);
}

// <--- UPDATED: Sets flag instead of running logic directly
static void btn_pcm_cb(lv_event_t * e) {
    lv_label_set_text(lbl_pcm_text, "WAIT...");
    trigger_audio_test = true; 
}

// <--- NEW: Test Sequence Processor (Runs in main loop)
void process_audio_test_sequence() {
    // 1. Alloc
    if (rec_buffer == NULL) {
        lv_label_set_text(lbl_pcm_text, "ALLOC");
        show_toast("Allocating Buffer...");
        lv_timer_handler(); // Force UI update
        delay(10);

        // Calculate size: 16k rate * 2 bytes * seconds
        rec_buffer_size = 16000 * 2 * (int)REC_LIMIT_SECONDS; 
        rec_buffer = (int16_t*)heap_caps_malloc(rec_buffer_size, MALLOC_CAP_SPIRAM);
        
        if (rec_buffer == NULL) {
            Serial.println("[UI] Failed to allocate audio buffer!");
            lv_label_set_text(lbl_pcm_text, "NO RAM!");
            show_toast("Alloc Failed!", 3000);
            return;
        }
    }

    // 2. Setup Recording
    lv_label_set_text(lbl_pcm_text, "REC...");
    show_toast("Recording... Speak!", 5000); 
    lv_timer_handler(); // Force UI Update before loop starts
    delay(50); // Give rendering time to finish

    Serial.println("[UI] Starting Safe Record Loop...");
    
    // 3. Safe Recording Loop
    size_t samples_total = 16000 * REC_LIMIT_SECONDS;
    size_t samples_read = 0;
    size_t chunk_size = 128; 
    unsigned long startTime = millis();
    bool timeout = false;

    // Flush RX buffer before starting to clear old data
    // Read ~10ms worth of data and discard
    int16_t trash_buf[128];
    size_t trash_read;
    audioMgr.readRaw(trash_buf, 128); 
    
    while(samples_read < samples_total) {
        size_t remaining = samples_total - samples_read;
        size_t to_read = (remaining < chunk_size) ? remaining : chunk_size;

        size_t read = audioMgr.readRaw(&rec_buffer[samples_read], to_read);
        
        if (read > 0) {
            samples_read += read;
        } else {
            delay(1); 
            if (millis() - startTime > (REC_LIMIT_SECONDS * 1000) + 1000) {
                timeout = true;
                break;
            }
        }
        
        // Keep UI alive occasionally (Update every ~250ms)
        if (samples_read % 4000 == 0) lv_timer_handler();
    }

    if (!timeout) {
        // --- NEW: ANALYZE DATA ---
        long sum_abs = 0;
        int16_t max_val = 0;
        for (size_t i = 0; i < samples_read; i++) {
            int16_t val = abs(rec_buffer[i]);
            sum_abs += val;
            if (val > max_val) max_val = val;
        }
        float avg_vol = (float)sum_abs / samples_read;
        
        Serial.printf("[UI] Buffer Stats: Samples=%d, Max=%d, Avg=%.2f\n", samples_read, max_val, avg_vol);
        
        if (avg_vol < 20.0) { // arbitrary silence threshold
            Serial.println("[UI] WARNING: Recorded data is near silence!");
            show_toast("Silence Recorded?", 3000);
        } else {
            Serial.printf("[UI] Data looks valid (Max: %d). Playing...\n", max_val);
            lv_label_set_text(lbl_pcm_text, "PLAY...");
            show_toast("Playback...", 5000);
        }
        
        lv_timer_handler(); 
        delay(50);
        
        // 4. Playback
        size_t samples_written = 0;
        while(samples_written < samples_total) {
            size_t remaining = samples_total - samples_written;
            size_t to_write = (remaining < chunk_size) ? remaining : chunk_size;
            size_t written = audioMgr.writeRaw(&rec_buffer[samples_written], to_write);
            if (written > 0) samples_written += written;
            else delay(1);
            if (samples_written % 4000 == 0) lv_timer_handler();
        }

        lv_label_set_text(lbl_pcm_text, "REC LOOP");
        show_toast("Test Complete", 1500);
    } else {
        Serial.println("[UI] Recording Timeout! (I2S Bus Dead?)");
        lv_label_set_text(lbl_pcm_text, "TIMEOUT");
        show_toast("I2S Timeout!", 3000);
    }
}

static void slider_bri_cb(lv_event_t * e) {
    lv_obj_t * slider = (lv_obj_t*)lv_event_get_target(e);
    int32_t val = lv_slider_get_value(slider);
    displayMgr.setBrightness(val);
    
    // Show Toast
    char buf[32];
    snprintf(buf, sizeof(buf), "Brightness: %d%%", (int)val);
    show_toast(buf, 1000);
}

static void sw_touch_viz_cb(lv_event_t * e) {
    lv_obj_t * sw = (lv_obj_t*)lv_event_get_target(e);
    showTouches = lv_obj_has_state(sw, LV_STATE_CHECKED);
    if (showTouches) lv_obj_clear_flag(panel_touch_data, LV_OBJ_FLAG_HIDDEN);
    else {
        lv_obj_add_flag(panel_touch_data, LV_OBJ_FLAG_HIDDEN);
        for(int i=0; i<5; i++) lv_obj_add_flag(cursors[i], LV_OBJ_FLAG_HIDDEN);
    }
}

// -------------------------------------------------------------------------
// UI Building Blocks
// -------------------------------------------------------------------------
lv_obj_t* create_collapsible_panel(lv_obj_t* parent, const char* title, lv_obj_t** content_container) {
    lv_obj_t * pnl = lv_obj_create(parent);
    lv_obj_set_width(pnl, lv_pct(48)); 
    lv_obj_set_height(pnl, PNL_COLLAPSED_H); 
    lv_obj_set_style_bg_color(pnl, lv_color_hex(0x181818), 0);
    lv_obj_set_style_radius(pnl, 12, 0);
    lv_obj_set_style_border_width(pnl, 1, 0);
    lv_obj_set_style_border_color(pnl, lv_color_hex(0x606060), 0);
    lv_obj_set_style_pad_all(pnl, 0, 0); 
    lv_obj_set_style_pad_row(pnl, 10, 0);   // Space between header and content
    lv_obj_clear_flag(pnl, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_clip_corner(pnl, true, 0); 

    lv_obj_t * header = lv_obj_create(pnl);
    lv_obj_set_size(header, lv_pct(100), (45));
    lv_obj_set_style_bg_opa(header, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(header, 0, 0);
    lv_obj_set_style_pad_all(header, 15, 0); // Padding inside header
    lv_obj_add_flag(header, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(header, panel_header_click_cb, LV_EVENT_CLICKED, pnl); 

    lv_obj_t * lbl = lv_label_create(header);
    lv_label_set_text(lbl, title);
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(lbl, lv_color_hex(0x00A8FF), 0);
    lv_obj_align(lbl, LV_ALIGN_LEFT_MID, 0, 0);

    lv_obj_t * icon = lv_label_create(header);
    lv_label_set_text(icon, LV_SYMBOL_UP); 
    lv_obj_set_style_text_color(icon, lv_color_hex(0x808080), 0);
    lv_obj_align(icon, LV_ALIGN_RIGHT_MID, 0, 0);

    *content_container = lv_obj_create(pnl);
    lv_obj_set_width(*content_container, lv_pct(100));
    lv_obj_set_height(*content_container, LV_SIZE_CONTENT); 
    lv_obj_set_y(*content_container, PNL_COLLAPSED_H-25); // Start below header
    lv_obj_set_style_bg_opa(*content_container, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(*content_container, 0, 0);
    lv_obj_set_style_pad_all(*content_container, 10, 0);
    lv_obj_set_flex_flow(*content_container, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_gap(*content_container, 10, 0);

    return pnl;
}

// -------------------------------------------------------------------------
// SETUP
// -------------------------------------------------------------------------
void setup() {
    Serial.begin(115200);
    delay(1000); 
    Serial.println("\n\n=== Fleet Hardware Dashboard (Accordion UI Final) ===");

    if (!displayMgr.begin()) { while(1) delay(100); }
    touchMgr.begin();
    displayMgr.powerAmpEnable(true);
    displayMgr.powerAmpSwitch(true);
    delay(100);
    audioMgr.begin();

    lv_init();
    lv_tick_set_cb(my_tick_get_cb);

    Arduino_GFX *gfx = displayMgr.getGfx();
    lv_display_t *disp = lv_display_create(gfx->width(), gfx->height());
    lv_display_set_flush_cb(disp, my_disp_flush);
    
    lv_draw_buf = (uint16_t*)heap_caps_malloc(LV_BUF_SIZE * sizeof(uint16_t), MALLOC_CAP_SPIRAM);
    if (!lv_draw_buf) lv_draw_buf = (uint16_t*)malloc(LV_BUF_SIZE * sizeof(uint16_t));
    lv_display_set_buffers(disp, lv_draw_buf, NULL, LV_BUF_SIZE * sizeof(uint16_t), LV_DISPLAY_RENDER_MODE_PARTIAL);

    lv_indev_t *indev = lv_indev_create();
    lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(indev, my_touch_read);

    // --- MAIN UI LAYOUT ---
    lv_obj_set_style_bg_color(lv_screen_active(), lv_color_hex(0x101010), LV_PART_MAIN);
    lv_obj_clear_flag(lv_screen_active(), LV_OBJ_FLAG_SCROLLABLE);

    // 1. TOP HEADER
    lv_obj_t * top_bar = lv_obj_create(lv_screen_active());
    lv_obj_set_size(top_bar, lv_pct(100), 50); 
    lv_obj_set_style_bg_color(top_bar, lv_color_hex(0x202020), 0);
    lv_obj_set_style_border_side(top_bar, LV_BORDER_SIDE_BOTTOM, 0);
    lv_obj_set_style_border_width(top_bar, 2, 0);
    lv_obj_set_style_border_color(top_bar, lv_color_hex(0x00A8FF), 0);
    lv_obj_align(top_bar, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_clear_flag(top_bar, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t * title = lv_label_create(top_bar);
    lv_label_set_text(title, cfg.device_name); 
    lv_obj_align(title, LV_ALIGN_LEFT_MID, 5, 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(title, lv_color_hex(0x00A8FF), 0); 

    // Stats Labels
    lv_obj_t * stats_cont = lv_obj_create(top_bar);
    lv_obj_set_size(stats_cont, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_align(stats_cont, LV_ALIGN_RIGHT_MID, 0, 0);
    lv_obj_set_style_bg_opa(stats_cont, LV_OPA_0, 0);
    lv_obj_set_style_border_width(stats_cont, 0, 0);
    lv_obj_set_flex_flow(stats_cont, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_column(stats_cont, 15, 0); 

    lbl_stats_ram = lv_label_create(stats_cont);
    lv_label_set_text(lbl_stats_ram, "RAM: --");
    lv_obj_set_style_text_color(lbl_stats_ram, lv_color_hex(0xAAAAAA), 0);

    lbl_stats_time = lv_label_create(stats_cont);
    lv_obj_set_width(lbl_stats_time, 80); 
    lv_label_set_text(lbl_stats_time, "UP: 00:00");
    lv_obj_set_style_text_align(lbl_stats_time, LV_TEXT_ALIGN_RIGHT, 0);
    lv_obj_set_style_text_color(lbl_stats_time, lv_color_hex(0xFFFFFF), 0);

    // --- REORDERED: Touch Data Panel (Created 2nd, so it is below Bottom Deck) ---
    panel_touch_data = lv_obj_create(lv_screen_active());
    lv_obj_set_size(panel_touch_data, 180, 210); 
    lv_obj_align(panel_touch_data, LV_ALIGN_RIGHT_MID, -10, -50);
    lv_obj_set_style_bg_color(panel_touch_data, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(panel_touch_data, LV_OPA_80, 0); 
    lv_obj_set_style_border_width(panel_touch_data, 2, 0);
    lv_obj_set_style_border_color(panel_touch_data, lv_color_hex(0xFFFFFF), 0); 
    lv_obj_add_flag(panel_touch_data, LV_OBJ_FLAG_CLICKABLE); 
    lv_obj_add_flag(panel_touch_data, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_event_cb(panel_touch_data, drag_event_handler, LV_EVENT_PRESSING, NULL);

    lv_obj_t * lbl_h = lv_label_create(panel_touch_data);
    lv_label_set_text(lbl_h, "TOUCH POINTS");
    lv_obj_align(lbl_h, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_text_color(lbl_h, lv_color_hex(0x00A8FF), 0);

    for(int i=0; i<5; i++) {
        coord_labels[i] = lv_label_create(panel_touch_data);
        lv_label_set_text_fmt(coord_labels[i], "ID%d: --", i);
        lv_obj_align(coord_labels[i], LV_ALIGN_TOP_LEFT, 40, 25 + (i * 25));
        lv_obj_set_style_text_color(coord_labels[i], lv_color_hex(0x808080), 0); 
    }

    count_label = lv_label_create(panel_touch_data);
    lv_label_set_text(count_label, "ACTIVE: 0");
    lv_obj_align(count_label, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_text_font(count_label, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(count_label, lv_color_hex(0x404040), 0); 

    // 2. BOTTOM DECK (Created 3rd, so it overlays Touch Panel)
    lv_obj_t * bottom_deck = lv_obj_create(lv_screen_active());
    lv_obj_set_size(bottom_deck, lv_pct(100), lv_pct(100)); 
    lv_obj_set_y(bottom_deck, 50); 
    lv_obj_set_style_bg_opa(bottom_deck, LV_OPA_0, 0); 
    lv_obj_set_style_border_width(bottom_deck, 0, 0);
    lv_obj_set_flex_flow(bottom_deck, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(bottom_deck, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_END); 
    lv_obj_set_style_pad_all(bottom_deck, 10, 0);
    lv_obj_set_style_pad_column(bottom_deck, 10, 0); 
    // <--- UPDATED: Allow clicks to pass through transparent deck to underlying touch panel
    lv_obj_clear_flag(bottom_deck, LV_OBJ_FLAG_CLICKABLE);

    // --- AUDIO PANEL ---
    pnl_audio = create_collapsible_panel(bottom_deck, "AUDIO", &pnl_audio_content);
    
    // ROW1: Controls (Switch on Left, Slider on Right)
    lv_obj_t * row_ctrls = lv_obj_create(pnl_audio_content);
    lv_obj_set_size(row_ctrls, lv_pct(100), 45);
    lv_obj_set_style_bg_opa(row_ctrls, LV_OPA_0, 0);
    lv_obj_set_style_border_width(row_ctrls, 0, 0);
    lv_obj_set_flex_flow(row_ctrls, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row_ctrls, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_all(row_ctrls, 0, 0);
    lv_obj_set_style_pad_gap(row_ctrls, 5, 0);   // Gap between columns
    lv_obj_remove_flag(row_ctrls, LV_OBJ_FLAG_SCROLLABLE);


    // ROW1 1,0: Switch/Enable Column
    // Container for Switch + Label
    lv_obj_t * col_sw = lv_obj_create(row_ctrls);
    lv_obj_set_size(col_sw, 65, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(col_sw, LV_OPA_0, 0);
    lv_obj_set_style_border_width(col_sw, 0, 0);
    lv_obj_set_flex_flow(col_sw, LV_FLEX_FLOW_COLUMN);
    lv_obj_remove_flag(col_sw, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_pad_left(col_sw, 5, 0);
    lv_obj_set_style_pad_right(col_sw, 5, 0);
    lv_obj_set_style_pad_row(col_sw, 5, 0);

    // ROW1 1,1: Label
    lv_obj_t * lbl_en = lv_label_create(col_sw);
    lv_obj_set_size(lbl_en, lv_pct(100), LV_SIZE_CONTENT);
    lv_obj_set_align(lbl_en, LV_ALIGN_CENTER);
    lv_label_set_text(lbl_en, "ENABLE");
    lv_obj_set_style_text_font(lbl_en, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(lbl_en, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_align(lbl_en, LV_TEXT_ALIGN_CENTER, 0);
    
    // ROW1 1,2: Switch
    lv_obj_t * sw_enable = lv_switch_create(col_sw);
    lv_obj_set_size(sw_enable, lv_pct(100), 25);
    lv_obj_set_align(sw_enable, LV_ALIGN_CENTER);
    lv_obj_add_event_cb(sw_enable, sw_enable_cb, LV_EVENT_VALUE_CHANGED, NULL);
    lv_obj_add_state(sw_enable, LV_STATE_CHECKED); 

    // ROW1 2,0: Volume Slider Column
    // Container for Slider + label
    lv_obj_t * col_vol = lv_obj_create(row_ctrls);
    lv_obj_set_flex_grow(col_vol, 1);
    lv_obj_set_height(col_vol, LV_SIZE_CONTENT); // Fixed height for slider column
    lv_obj_set_flex_flow(col_vol, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(col_vol, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_remove_flag(col_vol, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_opa(col_vol, LV_OPA_0, 0);
    lv_obj_set_style_border_width(col_vol, 0, 0);
    lv_obj_set_style_pad_left(col_vol, 15, 0);
    lv_obj_set_style_pad_right(col_vol, 15, 0);
    lv_obj_set_style_pad_bottom(col_vol, 10, 0);
    lv_obj_set_style_pad_row(col_vol, 10, 0);

    // ROW1 2,1: Volume Label
    lv_obj_t * lbl_vol = lv_label_create(col_vol);
    lv_obj_set_size(lbl_vol, lv_pct(100), LV_SIZE_CONTENT);
    lv_label_set_text(lbl_vol, "VOLUME");
    lv_obj_set_style_text_align(lbl_vol, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_color(lbl_vol, lv_color_hex(0x808080), 0);
    lv_obj_set_style_text_font(lbl_vol, &lv_font_montserrat_12, 0);

    // ROW1 2,2: Volume Slider
    slider_vol = lv_slider_create(col_vol);
    lv_obj_set_width(slider_vol, lv_pct(100));
    lv_obj_set_flex_grow(slider_vol, 1);
    lv_obj_set_align(slider_vol, LV_ALIGN_CENTER);
    lv_obj_remove_flag(slider_vol, LV_OBJ_FLAG_SCROLLABLE); // Add padding to knob area
    lv_slider_set_range(slider_vol, 0, 100);
    lv_slider_set_value(slider_vol, audioMgr.getVolume(), LV_ANIM_OFF);
    lv_obj_add_event_cb(slider_vol, slider_vol_cb, LV_EVENT_VALUE_CHANGED, NULL);
    lv_obj_set_style_max_height(slider_vol, 15, 0);

    // ROW2: Buttons Row
    lv_obj_t * row_btns = lv_obj_create(pnl_audio_content);
    lv_obj_set_size(row_btns, lv_pct(100), 45);
    lv_obj_set_style_bg_opa(row_btns, LV_OPA_0, 0);
    lv_obj_set_style_border_width(row_btns, 0, 0);
    lv_obj_set_flex_flow(row_btns, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_all(row_btns, 0, 0);
    lv_obj_set_style_pad_gap(row_btns, 6, 0);   // Gap between buttons
    lv_obj_remove_flag(row_btns, LV_OBJ_FLAG_SCROLLABLE);

    // ROW2 1,1
    btn_tone = lv_button_create(row_btns);
    lv_obj_set_flex_grow(btn_tone, 1);
    lv_obj_set_height(btn_tone, LV_SIZE_CONTENT);
    lv_obj_set_style_max_width(btn_tone, 100, 0);
    lv_obj_t * lbl_tone = lv_label_create(btn_tone);
    lv_label_set_text(lbl_tone, "TONE");
    lv_obj_center(lbl_tone);
    lv_obj_add_event_cb(btn_tone, btn_tone_cb, LV_EVENT_CLICKED, NULL);

    // ROW2 1,2
    btn_pcm = lv_button_create(row_btns);
    lv_obj_set_flex_grow(btn_pcm, 1);
    lv_obj_set_height(btn_pcm, LV_SIZE_CONTENT);
    lv_obj_set_style_max_width(btn_pcm, 100, 0);
    lbl_pcm_text = lv_label_create(btn_pcm); // Use global pointer
    lv_label_set_text(lbl_pcm_text, "REC LOOP");
    lv_obj_center(lbl_pcm_text);
    lv_obj_add_event_cb(btn_pcm, btn_pcm_cb, LV_EVENT_CLICKED, NULL);

    // ROW3: VU Meter
    lv_obj_t * vu_wrapper = lv_obj_create(pnl_audio_content);
    lv_obj_set_size(vu_wrapper, lv_pct(100), 12); // Wrapper container
    lv_obj_set_style_bg_opa(vu_wrapper, LV_OPA_0, 0); // Transparent
    lv_obj_set_style_border_width(vu_wrapper, 0, 0);
    lv_obj_set_flex_flow(vu_wrapper, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_all(vu_wrapper, 0, 0);
    lv_obj_set_style_pad_gap(vu_wrapper, 6, 0); // Gap between L and R meters
    
    // ROW3: 1,1 Left Channel Container (Dark Grey Background)
    lv_obj_t * meter_l_bg = lv_obj_create(vu_wrapper);
    lv_obj_set_flex_grow(meter_l_bg, 1); // Take 50% of width
    lv_obj_set_height(meter_l_bg, lv_pct(100));
    lv_obj_set_style_bg_color(meter_l_bg, lv_color_hex(0x303030), 0);
    lv_obj_set_style_border_width(meter_l_bg, 0, 0);
    lv_obj_set_style_pad_all(meter_l_bg, 0, 0);

    bar_vu_l = lv_obj_create(meter_l_bg);
    lv_obj_set_size(bar_vu_l, lv_pct(0), lv_pct(100)); // Dynamic Width relative to meter_l_bg
    lv_obj_set_style_bg_color(bar_vu_l, lv_palette_main(LV_PALETTE_GREEN), 0);
    lv_obj_set_style_border_width(bar_vu_l, 0, 0);

    // ROW3: 1,2 Right Channel Container (Dark Grey Background)
    lv_obj_t * meter_r_bg = lv_obj_create(vu_wrapper);
    lv_obj_set_flex_grow(meter_r_bg, 1); // Take 50% of width
    lv_obj_set_height(meter_r_bg, lv_pct(100));
    lv_obj_set_style_bg_color(meter_r_bg, lv_color_hex(0x303030), 0);
    lv_obj_set_style_border_width(meter_r_bg, 0, 0);
    lv_obj_set_style_pad_all(meter_r_bg, 0, 0);

    bar_vu_r = lv_obj_create(meter_r_bg);
    lv_obj_set_size(bar_vu_r, lv_pct(0), lv_pct(100)); // Dynamic Width relative to meter_r_bg
    lv_obj_set_style_bg_color(bar_vu_r, lv_palette_main(LV_PALETTE_GREEN), 0);
    lv_obj_set_style_border_width(bar_vu_r, 0, 0);


    // --- DISPLAY PANEL ---
    pnl_disp = create_collapsible_panel(bottom_deck, "DISPLAY", &pnl_disp_content);
    
    // ROW1: Brightness Slider Column
    lv_obj_t * col_bri = lv_obj_create(pnl_disp_content);
    lv_obj_set_size(col_bri, lv_pct(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(col_bri, LV_FLEX_FLOW_COLUMN);
    lv_obj_remove_flag(col_bri, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_opa(col_bri, LV_OPA_0, 0);
    lv_obj_set_style_border_width(col_bri, 0, 0);
    lv_obj_set_style_pad_left(col_bri, 15, 0);
    lv_obj_set_style_pad_right(col_bri, 15, 0);
    lv_obj_set_style_pad_bottom(col_bri, 10, 0);
    lv_obj_set_style_pad_row(col_bri, 5, 0);

    // ROW1 2,1: Brightness Label
    lv_obj_t * lbl_brightness = lv_label_create(col_bri);
    lv_obj_set_size(lbl_brightness, lv_pct(100), LV_SIZE_CONTENT);
    lv_label_set_text(lbl_brightness, "BRIGHTNESS");
    lv_obj_set_style_text_align(lbl_brightness, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_color(lbl_brightness, lv_color_hex(0x808080), 0);
    lv_obj_set_style_text_font(lbl_brightness, &lv_font_montserrat_12, 0);

    // ROW1 2,2: Brightness Slider
    slider_bri = lv_slider_create(col_bri);
    lv_obj_set_size(slider_bri, lv_pct(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_grow(slider_bri, 1);
    lv_obj_set_align(slider_bri, LV_ALIGN_BOTTOM_MID);
    lv_obj_remove_flag(slider_bri, LV_OBJ_FLAG_SCROLLABLE);
    #ifdef WS_S3_SMART86
        lv_slider_set_range(slider_bri, 43, 100); // Glitch where brightness turns off at 43%
    #else
        lv_slider_set_range(slider_bri, 0, 100);
    #endif
    lv_slider_set_value(slider_bri, displayMgr.getBrightness(), LV_ANIM_OFF);
    lv_obj_add_event_cb(slider_bri, slider_bri_cb, LV_EVENT_VALUE_CHANGED, NULL);
    lv_obj_set_style_max_height(slider_bri, 15, 0);

    // ROW2: Touches Visualization Switch
    lv_obj_t * row_viz = lv_obj_create(pnl_disp_content);
    lv_obj_set_size(row_viz, lv_pct(100), 45);
    lv_obj_set_style_bg_opa(row_viz, LV_OPA_0, 0);
    lv_obj_set_style_border_width(row_viz, 0, 0);
    lv_obj_set_flex_flow(row_viz, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row_viz, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(row_viz, 5, 0);
    lv_obj_remove_flag(row_viz, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t * lbl_viz = lv_label_create(row_viz);
    lv_label_set_text(lbl_viz, "Show Touches");
    lv_obj_set_style_text_color(lbl_viz, lv_color_white(), 0);

    lv_obj_t * sw_viz = lv_switch_create(row_viz);
    // lv_obj_add_state(sw_viz, LV_STATE_CHECKED);
    lv_obj_add_event_cb(sw_viz, sw_touch_viz_cb, LV_EVENT_VALUE_CHANGED, NULL);
    // lv_obj_set_size(sw_viz, LV_SIZE_CONTENT, 20);
    lv_obj_set_style_max_width(sw_viz, 70, 0);

    // --- Toast Panel (Created LAST to be on top) ---
    toast_panel = lv_obj_create(lv_screen_active());
    lv_obj_set_size(toast_panel, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_style_pad_all(toast_panel, 15, 0);
    lv_obj_set_style_bg_color(toast_panel, lv_color_hex(0x333333), 0);
    lv_obj_set_style_radius(toast_panel, 10, 0);
    // <--- UPDATED: Top Center alignment
    lv_obj_align(toast_panel, LV_ALIGN_TOP_MID, 0, 60); 
    lv_obj_add_flag(toast_panel, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(toast_panel, LV_OBJ_FLAG_CLICKABLE);

    toast_label = lv_label_create(toast_panel);
    lv_label_set_text(toast_label, "Action Completed");
    lv_obj_set_style_text_color(toast_label, lv_color_white(), 0);
    lv_obj_set_style_text_font(toast_label, &lv_font_montserrat_20, 0);

    // Cursors (Created last so they are always on TOP of everything)
    for(int i=0; i<5; i++) {
        cursors[i] = lv_obj_create(lv_screen_active());
        lv_obj_set_size(cursors[i], 60, 60);
        lv_obj_set_style_radius(cursors[i], LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_bg_opa(cursors[i], LV_OPA_0, 0); 
        lv_obj_set_style_border_width(cursors[i], 3, 0);
        lv_obj_clear_flag(cursors[i], LV_OBJ_FLAG_CLICKABLE); 
        lv_obj_add_flag(cursors[i], LV_OBJ_FLAG_HIDDEN); 
    }
}

// <--- UPDATED: Set flag in callback, return immediately
static void btn_pcm_cb(lv_event_t * e) {
    lv_label_set_text(lbl_pcm_text, "WAIT...");
    trigger_audio_test = true; 
}

// <--- NEW: Test Sequence Processor (Runs in main loop)
void process_audio_test_sequence() {
    // 1. Alloc
    if (rec_buffer == NULL) {
        lv_label_set_text(lbl_pcm_text, "ALLOC");
        show_toast("Allocating Buffer...");
        lv_timer_handler(); // Force UI update
        delay(10);

        // Calculate size: 16k rate * 2 bytes * seconds
        rec_buffer_size = 16000 * 2 * (int)REC_LIMIT_SECONDS; 
        rec_buffer = (int16_t*)heap_caps_malloc(rec_buffer_size, MALLOC_CAP_SPIRAM);
        
        if (rec_buffer == NULL) {
            Serial.println("[UI] Failed to allocate audio buffer!");
            lv_label_set_text(lbl_pcm_text, "NO RAM!");
            show_toast("Alloc Failed!", 3000);
            return;
        }
    }

    // 2. Setup Recording
    lv_label_set_text(lbl_pcm_text, "REC...");
    show_toast("Recording... Speak!", 5000); 
    lv_timer_handler(); // Force UI Update before loop starts
    delay(50); // Give rendering time to finish

    Serial.println("[UI] Starting Safe Record Loop...");
    
    // 3. Safe Recording Loop
    size_t samples_total = 16000 * REC_LIMIT_SECONDS;
    size_t samples_read = 0;
    // <--- FIX: Reduced chunk size to 128 (8ms) to fit inside 10ms readRaw timeout
    size_t chunk_size = 128; 
    unsigned long startTime = millis();
    bool timeout = false;

    // Flush RX buffer before starting to clear old data
    // Read ~10ms worth of data and discard
    int16_t trash_buf[128];
    size_t trash_read;
    audioMgr.readRaw(trash_buf, 128); 
    
    while(samples_read < samples_total) {
        size_t remaining = samples_total - samples_read;
        size_t to_read = (remaining < chunk_size) ? remaining : chunk_size;

        size_t read = audioMgr.readRaw(&rec_buffer[samples_read], to_read);
        
        if (read > 0) {
            samples_read += read;
        } else {
            delay(1); 
            if (millis() - startTime > (REC_LIMIT_SECONDS * 1000) + 1000) {
                timeout = true;
                break;
            }
        }
        
        // Keep UI alive occasionally (Update every ~250ms)
        if (samples_read % 4000 == 0) lv_timer_handler();
    }

    if (!timeout) {
        // --- NEW: ANALYZE DATA ---
        long sum_abs = 0;
        int16_t max_val = 0;
        for (size_t i = 0; i < samples_read; i++) {
            int16_t val = abs(rec_buffer[i]);
            sum_abs += val;
            if (val > max_val) max_val = val;
        }
        float avg_vol = (float)sum_abs / samples_read;
        
        Serial.printf("[UI] Buffer Stats: Samples=%d, Max=%d, Avg=%.2f\n", samples_read, max_val, avg_vol);
        
        if (avg_vol < 20.0) { // arbitrary silence threshold
            Serial.println("[UI] WARNING: Recorded data is near silence!");
            show_toast("Silence Recorded?", 3000);
        } else {
            Serial.printf("[UI] Data looks valid (Max: %d). Playing...\n", max_val);
            lv_label_set_text(lbl_pcm_text, "PLAY...");
            show_toast("Playback...", 5000);
        }
        
        lv_timer_handler(); 
        delay(50);
        
        // 4. Playback
        size_t samples_written = 0;
        while(samples_written < samples_total) {
            size_t remaining = samples_total - samples_written;
            size_t to_write = (remaining < chunk_size) ? remaining : chunk_size;
            size_t written = audioMgr.writeRaw(&rec_buffer[samples_written], to_write);
            if (written > 0) samples_written += written;
            else delay(1);
            if (samples_written % 4000 == 0) lv_timer_handler();
        }

        lv_label_set_text(lbl_pcm_text, "REC LOOP");
        show_toast("Test Complete", 1500);
    } else {
        Serial.println("[UI] Recording Timeout! (I2S Bus Dead?)");
        lv_label_set_text(lbl_pcm_text, "TIMEOUT");
        show_toast("I2S Timeout!", 3000);
    }
}

static void slider_bri_cb(lv_event_t * e) {
    lv_obj_t * slider = (lv_obj_t*)lv_event_get_target(e);
    int32_t val = lv_slider_get_value(slider);
    displayMgr.setBrightness(val);
    
    // Show Toast
    char buf[32];
    snprintf(buf, sizeof(buf), "Brightness: %d%%", (int)val);
    show_toast(buf, 1000);
}

static void sw_touch_viz_cb(lv_event_t * e) {
    lv_obj_t * sw = (lv_obj_t*)lv_event_get_target(e);
    showTouches = lv_obj_has_state(sw, LV_STATE_CHECKED);
    if (showTouches) lv_obj_clear_flag(panel_touch_data, LV_OBJ_FLAG_HIDDEN);
    else {
        lv_obj_add_flag(panel_touch_data, LV_OBJ_FLAG_HIDDEN);
        for(int i=0; i<5; i++) lv_obj_add_flag(cursors[i], LV_OBJ_FLAG_HIDDEN);
    }
}

// -------------------------------------------------------------------------
// UI Building Blocks
// -------------------------------------------------------------------------
lv_obj_t* create_collapsible_panel(lv_obj_t* parent, const char* title, lv_obj_t** content_container) {
    lv_obj_t * pnl = lv_obj_create(parent);
    lv_obj_set_width(pnl, lv_pct(48)); 
    lv_obj_set_height(pnl, PNL_COLLAPSED_H); 
    lv_obj_set_style_bg_color(pnl, lv_color_hex(0x181818), 0);
    lv_obj_set_style_radius(pnl, 12, 0);
    lv_obj_set_style_border_width(pnl, 1, 0);
    lv_obj_set_style_border_color(pnl, lv_color_hex(0x606060), 0);
    lv_obj_set_style_pad_all(pnl, 0, 0); 
    lv_obj_set_style_pad_row(pnl, 10, 0);   // Space between header and content
    lv_obj_clear_flag(pnl, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_clip_corner(pnl, true, 0); 

    lv_obj_t * header = lv_obj_create(pnl);
    lv_obj_set_size(header, lv_pct(100), (45));
    lv_obj_set_style_bg_opa(header, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(header, 0, 0);
    lv_obj_set_style_pad_all(header, 15, 0); // Padding inside header
    lv_obj_add_flag(header, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(header, panel_header_click_cb, LV_EVENT_CLICKED, pnl); 

    lv_obj_t * lbl = lv_label_create(header);
    lv_label_set_text(lbl, title);
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(lbl, lv_color_hex(0x00A8FF), 0);
    lv_obj_align(lbl, LV_ALIGN_LEFT_MID, 0, 0);

    lv_obj_t * icon = lv_label_create(header);
    lv_label_set_text(icon, LV_SYMBOL_UP); 
    lv_obj_set_style_text_color(icon, lv_color_hex(0x808080), 0);
    lv_obj_align(icon, LV_ALIGN_RIGHT_MID, 0, 0);

    *content_container = lv_obj_create(pnl);
    lv_obj_set_width(*content_container, lv_pct(100));
    lv_obj_set_height(*content_container, LV_SIZE_CONTENT); 
    lv_obj_set_y(*content_container, PNL_COLLAPSED_H-25); // Start below header
    lv_obj_set_style_bg_opa(*content_container, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(*content_container, 0, 0);
    lv_obj_set_style_pad_all(*content_container, 10, 0);
    lv_obj_set_flex_flow(*content_container, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_gap(*content_container, 10, 0);

    return pnl;
}

// -------------------------------------------------------------------------
// SETUP
// -------------------------------------------------------------------------
void setup() {
    Serial.begin(115200);
    delay(1000); 
    Serial.println("\n\n=== Fleet Hardware Dashboard (Accordion UI Final) ===");

    if (!displayMgr.begin()) { while(1) delay(100); }
    touchMgr.begin();
    displayMgr.powerAmpEnable(true);
    displayMgr.powerAmpSwitch(true);
    delay(100);
    audioMgr.begin();

    lv_init();
    lv_tick_set_cb(my_tick_get_cb);

    Arduino_GFX *gfx = displayMgr.getGfx();
    lv_display_t *disp = lv_display_create(gfx->width(), gfx->height());
    lv_display_set_flush_cb(disp, my_disp_flush);
    
    lv_draw_buf = (uint16_t*)heap_caps_malloc(LV_BUF_SIZE * sizeof(uint16_t), MALLOC_CAP_SPIRAM);
    if (!lv_draw_buf) lv_draw_buf = (uint16_t*)malloc(LV_BUF_SIZE * sizeof(uint16_t));
    lv_display_set_buffers(disp, lv_draw_buf, NULL, LV_BUF_SIZE * sizeof(uint16_t), LV_DISPLAY_RENDER_MODE_PARTIAL);

    lv_indev_t *indev = lv_indev_create();
    lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(indev, my_touch_read);

    // --- MAIN UI LAYOUT ---
    lv_obj_set_style_bg_color(lv_screen_active(), lv_color_hex(0x101010), LV_PART_MAIN);
    lv_obj_clear_flag(lv_screen_active(), LV_OBJ_FLAG_SCROLLABLE);

    // 1. TOP HEADER
    lv_obj_t * top_bar = lv_obj_create(lv_screen_active());
    lv_obj_set_size(top_bar, lv_pct(100), 50); 
    lv_obj_set_style_bg_color(top_bar, lv_color_hex(0x202020), 0);
    lv_obj_set_style_border_side(top_bar, LV_BORDER_SIDE_BOTTOM, 0);
    lv_obj_set_style_border_width(top_bar, 2, 0);
    lv_obj_set_style_border_color(top_bar, lv_color_hex(0x00A8FF), 0);
    lv_obj_align(top_bar, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_clear_flag(top_bar, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t * title = lv_label_create(top_bar);
    lv_label_set_text(title, cfg.device_name); 
    lv_obj_align(title, LV_ALIGN_LEFT_MID, 5, 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(title, lv_color_hex(0x00A8FF), 0); 

    // Stats Labels
    lv_obj_t * stats_cont = lv_obj_create(top_bar);
    lv_obj_set_size(stats_cont, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_align(stats_cont, LV_ALIGN_RIGHT_MID, 0, 0);
    lv_obj_set_style_bg_opa(stats_cont, LV_OPA_0, 0);
    lv_obj_set_style_border_width(stats_cont, 0, 0);
    lv_obj_set_flex_flow(stats_cont, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_column(stats_cont, 15, 0); 

    lbl_stats_ram = lv_label_create(stats_cont);
    lv_label_set_text(lbl_stats_ram, "RAM: --");
    lv_obj_set_style_text_color(lbl_stats_ram, lv_color_hex(0xAAAAAA), 0);

    lbl_stats_time = lv_label_create(stats_cont);
    lv_obj_set_width(lbl_stats_time, 80); 
    lv_label_set_text(lbl_stats_time, "UP: 00:00");
    lv_obj_set_style_text_align(lbl_stats_time, LV_TEXT_ALIGN_RIGHT, 0);
    lv_obj_set_style_text_color(lbl_stats_time, lv_color_hex(0xFFFFFF), 0);

    // --- REORDERED: Touch Data Panel (Created 2nd, so it is below Bottom Deck) ---
    panel_touch_data = lv_obj_create(lv_screen_active());
    lv_obj_set_size(panel_touch_data, 180, 210); 
    lv_obj_align(panel_touch_data, LV_ALIGN_RIGHT_MID, -10, -50);
    lv_obj_set_style_bg_color(panel_touch_data, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(panel_touch_data, LV_OPA_80, 0); 
    lv_obj_set_style_border_width(panel_touch_data, 2, 0);
    lv_obj_set_style_border_color(panel_touch_data, lv_color_hex(0xFFFFFF), 0); 
    lv_obj_add_flag(panel_touch_data, LV_OBJ_FLAG_CLICKABLE); 
    lv_obj_add_flag(panel_touch_data, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_event_cb(panel_touch_data, drag_event_handler, LV_EVENT_PRESSING, NULL);

    lv_obj_t * lbl_h = lv_label_create(panel_touch_data);
    lv_label_set_text(lbl_h, "TOUCH POINTS");
    lv_obj_align(lbl_h, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_text_color(lbl_h, lv_color_hex(0x00A8FF), 0);

    for(int i=0; i<5; i++) {
        coord_labels[i] = lv_label_create(panel_touch_data);
        lv_label_set_text_fmt(coord_labels[i], "ID%d: --", i);
        lv_obj_align(coord_labels[i], LV_ALIGN_TOP_LEFT, 40, 25 + (i * 25));
        lv_obj_set_style_text_color(coord_labels[i], lv_color_hex(0x808080), 0); 
    }

    count_label = lv_label_create(panel_touch_data);
    lv_label_set_text(count_label, "ACTIVE: 0");
    lv_obj_align(count_label, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_text_font(count_label, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(count_label, lv_color_hex(0x404040), 0); 

    // 2. BOTTOM DECK (Created 3rd, so it overlays Touch Panel)
    lv_obj_t * bottom_deck = lv_obj_create(lv_screen_active());
    lv_obj_set_size(bottom_deck, lv_pct(100), lv_pct(100)); 
    lv_obj_set_y(bottom_deck, 50); 
    lv_obj_set_style_bg_opa(bottom_deck, LV_OPA_0, 0); 
    lv_obj_set_style_border_width(bottom_deck, 0, 0);
    lv_obj_set_flex_flow(bottom_deck, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(bottom_deck, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_END); 
    lv_obj_set_style_pad_all(bottom_deck, 10, 0);
    lv_obj_set_style_pad_column(bottom_deck, 10, 0); 
    // <--- UPDATED: Allow clicks to pass through transparent deck to underlying touch panel
    lv_obj_clear_flag(bottom_deck, LV_OBJ_FLAG_CLICKABLE);

    // --- AUDIO PANEL ---
    pnl_audio = create_collapsible_panel(bottom_deck, "AUDIO", &pnl_audio_content);
    
    // ROW1: Controls (Switch on Left, Slider on Right)
    lv_obj_t * row_ctrls = lv_obj_create(pnl_audio_content);
    lv_obj_set_size(row_ctrls, lv_pct(100), 45);
    lv_obj_set_style_bg_opa(row_ctrls, LV_OPA_0, 0);
    lv_obj_set_style_border_width(row_ctrls, 0, 0);
    lv_obj_set_flex_flow(row_ctrls, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row_ctrls, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_all(row_ctrls, 0, 0);
    lv_obj_set_style_pad_gap(row_ctrls, 5, 0);   // Gap between columns
    lv_obj_remove_flag(row_ctrls, LV_OBJ_FLAG_SCROLLABLE);


    // ROW1 1,0: Switch/Enable Column
    // Container for Switch + Label
    lv_obj_t * col_sw = lv_obj_create(row_ctrls);
    lv_obj_set_size(col_sw, 65, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(col_sw, LV_OPA_0, 0);
    lv_obj_set_style_border_width(col_sw, 0, 0);
    lv_obj_set_flex_flow(col_sw, LV_FLEX_FLOW_COLUMN);
    lv_obj_remove_flag(col_sw, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_pad_left(col_sw, 5, 0);
    lv_obj_set_style_pad_right(col_sw, 5, 0);
    lv_obj_set_style_pad_row(col_sw, 5, 0);

    // ROW1 1,1: Label
    lv_obj_t * lbl_en = lv_label_create(col_sw);
    lv_obj_set_size(lbl_en, lv_pct(100), LV_SIZE_CONTENT);
    lv_obj_set_align(lbl_en, LV_ALIGN_CENTER);
    lv_label_set_text(lbl_en, "ENABLE");
    lv_obj_set_style_text_font(lbl_en, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(lbl_en, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_align(lbl_en, LV_TEXT_ALIGN_CENTER, 0);
    
    // ROW1 1,2: Switch
    lv_obj_t * sw_enable = lv_switch_create(col_sw);
    lv_obj_set_size(sw_enable, lv_pct(100), 25);
    lv_obj_set_align(sw_enable, LV_ALIGN_CENTER);
    lv_obj_add_event_cb(sw_enable, sw_enable_cb, LV_EVENT_VALUE_CHANGED, NULL);
    lv_obj_add_state(sw_enable, LV_STATE_CHECKED); 

    // ROW1 2,0: Volume Slider Column
    // Container for Slider + label
    lv_obj_t * col_vol = lv_obj_create(row_ctrls);
    lv_obj_set_flex_grow(col_vol, 1);
    lv_obj_set_height(col_vol, LV_SIZE_CONTENT); // Fixed height for slider column
    lv_obj_set_flex_flow(col_vol, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(col_vol, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_remove_flag(col_vol, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_opa(col_vol, LV_OPA_0, 0);
    lv_obj_set_style_border_width(col_vol, 0, 0);
    lv_obj_set_style_pad_left(col_vol, 15, 0);
    lv_obj_set_style_pad_right(col_vol, 15, 0);
    lv_obj_set_style_pad_bottom(col_vol, 10, 0);
    lv_obj_set_style_pad_row(col_vol, 10, 0);

    // ROW1 2,1: Volume Label
    lv_obj_t * lbl_vol = lv_label_create(col_vol);
    lv_obj_set_size(lbl_vol, lv_pct(100), LV_SIZE_CONTENT);
    lv_label_set_text(lbl_vol, "VOLUME");
    lv_obj_set_style_text_align(lbl_vol, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_color(lbl_vol, lv_color_hex(0x808080), 0);
    lv_obj_set_style_text_font(lbl_vol, &lv_font_montserrat_12, 0);

    // ROW1 2,2: Volume Slider
    slider_vol = lv_slider_create(col_vol);
    lv_obj_set_width(slider_vol, lv_pct(100));
    lv_obj_set_flex_grow(slider_vol, 1);
    lv_obj_set_align(slider_vol, LV_ALIGN_CENTER);
    lv_obj_remove_flag(slider_vol, LV_OBJ_FLAG_SCROLLABLE); // Add padding to knob area
    lv_slider_set_range(slider_vol, 0, 100);
    lv_slider_set_value(slider_vol, audioMgr.getVolume(), LV_ANIM_OFF);
    lv_obj_add_event_cb(slider_vol, slider_vol_cb, LV_EVENT_VALUE_CHANGED, NULL);
    lv_obj_set_style_max_height(slider_vol, 15, 0);

    // ROW2: Buttons Row
    lv_obj_t * row_btns = lv_obj_create(pnl_audio_content);
    lv_obj_set_size(row_btns, lv_pct(100), 45);
    lv_obj_set_style_bg_opa(row_btns, LV_OPA_0, 0);
    lv_obj_set_style_border_width(row_btns, 0, 0);
    lv_obj_set_flex_flow(row_btns, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_all(row_btns, 0, 0);
    lv_obj_set_style_pad_gap(row_btns, 6, 0);   // Gap between buttons
    lv_obj_remove_flag(row_btns, LV_OBJ_FLAG_SCROLLABLE);

    // ROW2 1,1
    btn_tone = lv_button_create(row_btns);
    lv_obj_set_flex_grow(btn_tone, 1);
    lv_obj_set_height(btn_tone, LV_SIZE_CONTENT);
    lv_obj_set_style_max_width(btn_tone, 100, 0);
    lv_obj_t * lbl_tone = lv_label_create(btn_tone);
    lv_label_set_text(lbl_tone, "TONE");
    lv_obj_center(lbl_tone);
    lv_obj_add_event_cb(btn_tone, btn_tone_cb, LV_EVENT_CLICKED, NULL);

    // ROW2 1,2
    btn_pcm = lv_button_create(row_btns);
    lv_obj_set_flex_grow(btn_pcm, 1);
    lv_obj_set_height(btn_pcm, LV_SIZE_CONTENT);
    lv_obj_set_style_max_width(btn_pcm, 100, 0);
    lbl_pcm_text = lv_label_create(btn_pcm); // Use global pointer
    lv_label_set_text(lbl_pcm_text, "REC LOOP");
    lv_obj_center(lbl_pcm_text);
    lv_obj_add_event_cb(btn_pcm, btn_pcm_cb, LV_EVENT_CLICKED, NULL);

    // ROW3: VU Meter
    lv_obj_t * vu_wrapper = lv_obj_create(pnl_audio_content);
    lv_obj_set_size(vu_wrapper, lv_pct(100), 12); // Wrapper container
    lv_obj_set_style_bg_opa(vu_wrapper, LV_OPA_0, 0); // Transparent
    lv_obj_set_style_border_width(vu_wrapper, 0, 0);
    lv_obj_set_flex_flow(vu_wrapper, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_all(vu_wrapper, 0, 0);
    lv_obj_set_style_pad_gap(vu_wrapper, 6, 0); // Gap between L and R meters
    
    // ROW3: 1,1 Left Channel Container (Dark Grey Background)
    lv_obj_t * meter_l_bg = lv_obj_create(vu_wrapper);
    lv_obj_set_flex_grow(meter_l_bg, 1); // Take 50% of width
    lv_obj_set_height(meter_l_bg, lv_pct(100));
    lv_obj_set_style_bg_color(meter_l_bg, lv_color_hex(0x303030), 0);
    lv_obj_set_style_border_width(meter_l_bg, 0, 0);
    lv_obj_set_style_pad_all(meter_l_bg, 0, 0);

    bar_vu_l = lv_obj_create(meter_l_bg);
    lv_obj_set_size(bar_vu_l, lv_pct(0), lv_pct(100)); // Dynamic Width relative to meter_l_bg
    lv_obj_set_style_bg_color(bar_vu_l, lv_palette_main(LV_PALETTE_GREEN), 0);
    lv_obj_set_style_border_width(bar_vu_l, 0, 0);

    // ROW3: 1,2 Right Channel Container (Dark Grey Background)
    lv_obj_t * meter_r_bg = lv_obj_create(vu_wrapper);
    lv_obj_set_flex_grow(meter_r_bg, 1); // Take 50% of width
    lv_obj_set_height(meter_r_bg, lv_pct(100));
    lv_obj_set_style_bg_color(meter_r_bg, lv_color_hex(0x303030), 0);
    lv_obj_set_style_border_width(meter_r_bg, 0, 0);
    lv_obj_set_style_pad_all(meter_r_bg, 0, 0);

    bar_vu_r = lv_obj_create(meter_r_bg);
    lv_obj_set_size(bar_vu_r, lv_pct(0), lv_pct(100)); // Dynamic Width relative to meter_r_bg
    lv_obj_set_style_bg_color(bar_vu_r, lv_palette_main(LV_PALETTE_GREEN), 0);
    lv_obj_set_style_border_width(bar_vu_r, 0, 0);


    // --- DISPLAY PANEL ---
    pnl_disp = create_collapsible_panel(bottom_deck, "DISPLAY", &pnl_disp_content);
    
    // ROW1: Brightness Slider Column
    lv_obj_t * col_bri = lv_obj_create(pnl_disp_content);
    lv_obj_set_size(col_bri, lv_pct(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(col_bri, LV_FLEX_FLOW_COLUMN);
    lv_obj_remove_flag(col_bri, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_opa(col_bri, LV_OPA_0, 0);
    lv_obj_set_style_border_width(col_bri, 0, 0);
    lv_obj_set_style_pad_left(col_bri, 15, 0);
    lv_obj_set_style_pad_right(col_bri, 15, 0);
    lv_obj_set_style_pad_bottom(col_bri, 10, 0);
    lv_obj_set_style_pad_row(col_bri, 5, 0);

    // ROW1 2,1: Brightness Label
    lv_obj_t * lbl_brightness = lv_label_create(col_bri);
    lv_obj_set_size(lbl_brightness, lv_pct(100), LV_SIZE_CONTENT);
    lv_label_set_text(lbl_brightness, "BRIGHTNESS");
    lv_obj_set_style_text_align(lbl_brightness, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_color(lbl_brightness, lv_color_hex(0x808080), 0);
    lv_obj_set_style_text_font(lbl_brightness, &lv_font_montserrat_12, 0);

    // ROW1 2,2: Brightness Slider
    slider_bri = lv_slider_create(col_bri);
    lv_obj_set_size(slider_bri, lv_pct(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_grow(slider_bri, 1);
    lv_obj_set_align(slider_bri, LV_ALIGN_BOTTOM_MID);
    lv_obj_remove_flag(slider_bri, LV_OBJ_FLAG_SCROLLABLE);
    #ifdef WS_S3_SMART86
        lv_slider_set_range(slider_bri, 43, 100); // Glitch where brightness turns off at 43%
    #else
        lv_slider_set_range(slider_bri, 0, 100);
    #endif
    lv_slider_set_value(slider_bri, displayMgr.getBrightness(), LV_ANIM_OFF);
    lv_obj_add_event_cb(slider_bri, slider_bri_cb, LV_EVENT_VALUE_CHANGED, NULL);
    lv_obj_set_style_max_height(slider_bri, 15, 0);

    // ROW2: Touches Visualization Switch
    lv_obj_t * row_viz = lv_obj_create(pnl_disp_content);
    lv_obj_set_size(row_viz, lv_pct(100), 45);
    lv_obj_set_style_bg_opa(row_viz, LV_OPA_0, 0);
    lv_obj_set_style_border_width(row_viz, 0, 0);
    lv_obj_set_flex_flow(row_viz, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row_viz, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(row_viz, 5, 0);
    lv_obj_remove_flag(row_viz, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t * lbl_viz = lv_label_create(row_viz);
    lv_label_set_text(lbl_viz, "Show Touches");
    lv_obj_set_style_text_color(lbl_viz, lv_color_white(), 0);

    lv_obj_t * sw_viz = lv_switch_create(row_viz);
    // lv_obj_add_state(sw_viz, LV_STATE_CHECKED);
    lv_obj_add_event_cb(sw_viz, sw_touch_viz_cb, LV_EVENT_VALUE_CHANGED, NULL);
    // lv_obj_set_size(sw_viz, LV_SIZE_CONTENT, 20);
    lv_obj_set_style_max_width(sw_viz, 70, 0);

    // --- Toast Panel (Created LAST to be on top) ---
    toast_panel = lv_obj_create(lv_screen_active());
    lv_obj_set_size(toast_panel, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_style_pad_all(toast_panel, 15, 0);
    lv_obj_set_style_bg_color(toast_panel, lv_color_hex(0x333333), 0);
    lv_obj_set_style_radius(toast_panel, 10, 0);
    // <--- UPDATED: Top Center alignment
    lv_obj_align(toast_panel, LV_ALIGN_TOP_MID, 0, 60); 
    lv_obj_add_flag(toast_panel, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(toast_panel, LV_OBJ_FLAG_CLICKABLE);

    toast_label = lv_label_create(toast_panel);
    lv_label_set_text(toast_label, "Action Completed");
    lv_obj_set_style_text_color(toast_label, lv_color_white(), 0);
    lv_obj_set_style_text_font(toast_label, &lv_font_montserrat_20, 0);

    // Cursors (Created last so they are always on TOP of everything)
    for(int i=0; i<5; i++) {
        cursors[i] = lv_obj_create(lv_screen_active());
        lv_obj_set_size(cursors[i], 60, 60);
        lv_obj_set_style_radius(cursors[i], LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_bg_opa(cursors[i], LV_OPA_0, 0); 
        lv_obj_set_style_border_width(cursors[i], 3, 0);
        lv_obj_clear_flag(cursors[i], LV_OBJ_FLAG_CLICKABLE); 
        lv_obj_add_flag(cursors[i], LV_OBJ_FLAG_HIDDEN); 
    }
}

void loop() {
    activeTouchCount = touchMgr.read(activePoints, 5);
    
    // --- Update Stats ---
    if (millis() - lastStatsUpdate > 1000) {
        update_system_stats();
        lastStatsUpdate = millis();
    }

    // --- Check Audio Test Flag ---
    if (trigger_audio_test) {
        trigger_audio_test = false; // Reset
        process_audio_test_sequence();
    }

    // --- Update VU Meter (REAL DATA) ---
    // Only query hardware if the panel is visible to save bus bandwidth
    if (audio_expanded) {
        // Read real RMS level (0-100) from the ES7210 via I2S RX
        int mic_vol = audioMgr.getMicLevel();
        
        // Update the visual bars
        if (bar_vu_l) lv_obj_set_width(bar_vu_l, lv_pct(mic_vol));
        if (bar_vu_r) lv_obj_set_width(bar_vu_r, lv_pct(mic_vol));
    }

    // --- Touch Viz ---
    if (showTouches) {
        if (activeTouchCount > 0) lv_obj_set_style_text_color(count_label, lv_color_hex(0x00FFFF), 0); 
        else lv_obj_set_style_text_color(count_label, lv_color_hex(0xFFFFFF), 0); 
        for(int i=0; i<5; i++) {
            if(i < activeTouchCount) {
                TouchPoint &p = activePoints[i]; 
                lv_obj_clear_flag(cursors[i], LV_OBJ_FLAG_HIDDEN);
                
                int16_t x = p.x;
                int16_t y = p.y;
                if(TOUCH_SWAP_XY) { int16_t temp = x; x = y; y = temp; }
                if(TOUCH_INV_X)   x = cfg.WIDTH - 1 - x;
                if(TOUCH_INV_Y)   y = cfg.HEIGHT - 1 - y;

                lv_obj_set_pos(cursors[i], x - 30, y - 30); 
                lv_label_set_text_fmt(coord_labels[i], "ID%d: %d,%d", p.id, x, y);
                
                lv_color_t c;
                switch(p.id % 5) {
                    case 0: c = lv_palette_main(LV_PALETTE_RED); break;
                    case 1: c = lv_palette_main(LV_PALETTE_GREEN); break;
                    case 2: c = lv_palette_main(LV_PALETTE_BLUE); break;
                    case 3: c = lv_palette_main(LV_PALETTE_YELLOW); break;
                    case 4: c = lv_palette_main(LV_PALETTE_PURPLE); break;
                }
                lv_obj_set_style_text_color(coord_labels[i], c, 0);
                lv_obj_set_style_border_color(cursors[i], c, 0);
            } else {
                lv_obj_add_flag(cursors[i], LV_OBJ_FLAG_HIDDEN);
                lv_label_set_text_fmt(coord_labels[i], "ID%d: --", i);
                lv_obj_set_style_text_color(coord_labels[i], lv_color_hex(0x404040), 0);
            }
        }
    }
    lv_label_set_text_fmt(count_label, "ACTIVE: %d", activeTouchCount);
    
    lv_timer_handler();
    delay(1); 
}