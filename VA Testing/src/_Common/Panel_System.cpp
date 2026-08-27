#include "Panel_System.h"

// -- EXTERN DECLARATION --
extern void debug_dump_config(bool manualTrigger);

Panel_System::Panel_System() {
    _ui_root    = NULL;
    _ui_content = NULL;
    _ui_actions = NULL;
    txt_log     = NULL;
    lbl_stats   = NULL;
    _headerRef  = NULL;
    _expanded   = false;
    _onToggle   = nullptr;
    
    _ui_timer   = NULL;
    _log_dirty  = false;
    _stats_dirty = false;
}

Panel_System::~Panel_System() {
    if (_ui_timer) lv_timer_delete      (_ui_timer);
}

void Panel_System::setOnToggleCallback  (ToggleCallback cb) {
    _onToggle = cb;
}

void Panel_System::anim_height_cb(void * var, int32_t v) {
    Panel_System* p = (Panel_System*)var;
    if (!p || !p->_ui_root) return; 
    lv_obj_set_height(p->_ui_root, v);
}

void Panel_System::btn_action_cb(lv_event_t* e) {
    Panel_System* p = (Panel_System*)lv_event_get_user_data(e);
    
    if (p) {
        p->log("> Action: Dump Config...");
        debug_dump_config(true); // manually triggered - mirror to Serial too
    }
}

void Panel_System::init(lv_obj_t* parent, Panel_Header* headerRef) {
    _headerRef = headerRef;

    // 1. Create the WRAPPER (_ui_root)
    // Acts as the "Viewmask". Positioned explicitly below the header.
    _ui_root = lv_obj_create    (parent);
    lv_obj_set_width            (_ui_root, lv_pct(100)); // Full width
    lv_obj_set_height           (_ui_root, 0); 
    lv_obj_set_pos              (_ui_root, 0, UiToolkit::sc(30)); // Offset Y by Header Height
    
    // Wrapper Style (Invisible, Clipping)
    lv_obj_set_style_bg_opa         (_ui_root, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width   (_ui_root, 0, 0);
    lv_obj_set_style_pad_all        (_ui_root, 0, 0); 
    lv_obj_set_style_radius         (_ui_root, 0, 0);
    lv_obj_set_scrollbar_mode       (_ui_root, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_style_clip_corner    (_ui_root, true, 0); 

    // 2. Create the CONTENT CONTAINER (_ui_content)
    _ui_content = lv_obj_create     (_ui_root);
    // Fill the wrapper completely. The wrapper controls the visible height.
    lv_obj_set_size                 (_ui_content, lv_pct(100), lv_pct(100));
    
    // Content Style
    lv_obj_set_style_bg_color       (_ui_content, lv_color_hex(0x181818), 0);
    
    // -- CORNER HACK --
    // We want square top corners (to connect to header) and rounded bottom corners.
    // LVGL radius applies to all corners.
    // Trick: Move content UP by the radius amount to clip the top rounded corners off.
    int32_t radius = UiToolkit::sc(15);
    lv_obj_set_style_radius     (_ui_content, UiToolkit::sc(15), 0);
    lv_obj_set_y                (_ui_content, -(UiToolkit::sc(15))); // Shift up to hide top curves
    
    // Resetting size to account for the shift isn't strictly necessary if we use flex grow inside, 
    // but effectively the bottom area will be "Radius" pixels shorter than visual. 
    // Actually simpler: Just set height to 100% + radius.
    lv_obj_set_height           (_ui_content, lv_pct(100)); // It will clip bottom if we aren't careful, but since we animate wrapper, it's fine.
    // Let's stick to standard alignment for now, just radius.
    // If you really want square top, we can accept rounded top or use the hack. 
    // Let's use the hack:
    
    // lv_obj_set_style_margin_top (_ui_content, -(UiToolkit::sc(15)), 0);
    // lv_obj_set_style_pad_top    (_ui_content, UiToolkit::sc(15) + UiToolkit::sc(10), 0); // Radius + padding
    
    lv_obj_set_style_border_color   (_ui_content, lv_color_hex(0x404040), 0);
    lv_obj_set_style_border_width   (_ui_content, UiToolkit::sc(2), 0);
    // lv_obj_set_style_pad_all        (_ui_content, UiToolkit::sc(10), 0);
    lv_obj_set_style_pad_left       (_ui_content, UiToolkit::sc(10), 0);
    lv_obj_set_style_pad_right      (_ui_content, UiToolkit::sc(10), 0);
    lv_obj_set_style_pad_bottom     (_ui_content, UiToolkit::sc(10), 0);
    lv_obj_clear_flag               (_ui_content, LV_OBJ_FLAG_SCROLLABLE); // Static background

    lv_obj_set_flex_flow            (_ui_content, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align           (_ui_content, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_row        (_ui_content, UiToolkit::sc(8), 0);

    // -- ROW 1: Stats --
    lbl_stats = lv_label_create     (_ui_content);
    lv_obj_set_width                (lbl_stats, lv_pct(100));
    lv_label_set_text               (lbl_stats, "System Ready.");
    lv_obj_set_style_text_color     (lbl_stats, lv_color_hex(0x00FF00), 0);
    lv_obj_set_style_text_font      (lbl_stats, UiToolkit::Font_Label, 0);

    // -- ROW 2: Actions --
    _ui_actions = lv_obj_create     (_ui_content);
    lv_obj_set_width                (_ui_actions, lv_pct(100));
    lv_obj_set_height               (_ui_actions, LV_SIZE_CONTENT);
    lv_obj_set_layout               (_ui_actions, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow            (_ui_actions, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align           (_ui_actions, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_bg_opa         (_ui_actions, LV_OPA_TRANSP, 0);
    lv_obj_set_style_pad_all        (_ui_actions, 0, 0);
    lv_obj_set_style_border_width   (_ui_actions, 0, 0);
    lv_obj_set_style_pad_gap        (_ui_actions, UiToolkit::sc(10), 0);

    // Button: Dump Config
    lv_obj_t* btn = lv_button_create(_ui_actions);
    lv_obj_set_height               (btn, UiToolkit::sc(32));
    lv_obj_add_event_cb             (btn, btn_action_cb, LV_EVENT_CLICKED, this);
    lv_obj_set_style_bg_color       (btn, lv_color_hex(0x00A8FF), 0); // Cyan
    
    lv_obj_t* lbl = lv_label_create(btn);
    lv_label_set_text               (lbl, "Dump Config");
    lv_obj_center                   (lbl);
    lv_obj_set_style_text_font      (lbl, UiToolkit::Font_Button, 0);

    // -- ROW 3: Log Container --
    lv_obj_t* log_box = lv_obj_create(_ui_content);
    lv_obj_set_width                (log_box, lv_pct(100));
    
    // FLEX GROW: Take all remaining space!
    lv_obj_set_flex_grow            (log_box, 1); 
    
    lv_obj_set_style_bg_color       (log_box, lv_color_hex(0x000000), 0);
    lv_obj_set_style_pad_all        (log_box, UiToolkit::sc(8), 0);
    lv_obj_set_style_radius         (log_box, UiToolkit::sc(4), 0);
    lv_obj_set_scrollbar_mode       (log_box, LV_SCROLLBAR_MODE_AUTO); // Enable scrolling here
    
    txt_log = lv_label_create       (log_box);
    lv_obj_set_width                (txt_log, lv_pct(100));
    lv_label_set_long_mode          (txt_log, LV_LABEL_LONG_WRAP);
    lv_label_set_text               (txt_log, "> Init...");
    lv_obj_set_style_text_color     (txt_log, lv_color_hex(0xDDDDDD), 0); 
    
    if(UiToolkit::Font_Caption) {
        lv_obj_set_style_text_font  (txt_log, UiToolkit::Font_Caption, 0);
    } else {
        lv_obj_set_style_text_font  (txt_log, &lv_font_montserrat_14, 0);
    }

    _ui_timer = lv_timer_create     (_ui_timer_cb, 50, this); // 50ms for faster log flushing
}

void Panel_System::toggle() {
    _expanded = !_expanded;
    if (_onToggle) _onToggle(_expanded);
    if (_expanded) UiToolkit::closeActiveAccordion();
    
    int32_t start_h = lv_obj_get_height(_ui_root);
    
    // Calculate Safe Height: Screen - Header(50) - BottomGap(100)
    int32_t screen_h = lv_display_get_vertical_resolution(lv_display_get_default());
    int32_t max_h = screen_h - UiToolkit::sc(50) - UiToolkit::sc(100); 
    
    int32_t end_h = _expanded ? max_h : 0;

    lv_anim_del(this, anim_height_cb); 

    lv_anim_t a;
    lv_anim_init        (&a);
    lv_anim_set_var     (&a, this); 
    lv_anim_set_values  (&a, start_h, end_h);
    lv_anim_set_time    (&a, 350); 
    lv_anim_set_exec_cb (&a, anim_height_cb);
    lv_anim_set_path_cb (&a, lv_anim_path_ease_out); 
    lv_anim_start       (&a);
}

void Panel_System::close() {
    if (_expanded) toggle();
}

void Panel_System::log(const char* fmt, ...) {
    char buf[256];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);

    // Mirrors to Serial only when explicitly turned on (see setSerialEcho) -
    // debug_dump_config() controls this: off for a routine automatic boot
    // run (most of its content already duplicates the boot dashboard's own
    // direct Serial prints), on for a manually-triggered run (the "Dump
    // Config" button), or on for boot too if -D DUMP_CONFIG is set.
    if (_echoToSerial) Serial.println(buf);

    // Push to Queue
    if (_log_queue.size() < 100) { // Limit queue depth
        _log_queue.push_back(std::string(buf));
        _log_dirty = true;
    }
}

void Panel_System::updateSystemStats(float voltage, float current, int wifi_rssi) {
    _batt_volts = voltage;
    _batt_amps = current;
    _rssi = wifi_rssi;
    _stats_dirty = true;
}

void Panel_System::_ui_timer_cb(lv_timer_t* timer) {
    Panel_System* p = (Panel_System*)lv_timer_get_user_data(timer);
    if(p) p->_tick();
}

void Panel_System::_tick() {
    // Process LOG Queue
    if (_log_dirty) {
        // Process up to 5 messages per tick to keep UI responsive
        int processed = 0;
        while(!_log_queue.empty() && processed < 5) {
            std::string& msg = _log_queue.front();
            
            lv_label_ins_text   (txt_log, LV_LABEL_POS_LAST, "\n");
            lv_label_ins_text   (txt_log, LV_LABEL_POS_LAST, msg.c_str());
            
            _log_queue.erase    (_log_queue.begin());
            processed++;
        }

        // Clean up label if it gets too huge
        const char* current_txt = lv_label_get_text(txt_log);
        if (strlen(current_txt) > 4000) {
             lv_label_set_text(txt_log, "Log Cleared (Buffer Full)...\n");
        }

        // Auto Scroll
        lv_obj_t* parent = lv_obj_get_parent(txt_log);
        lv_obj_scroll_to_y(parent, LV_COORD_MAX, LV_ANIM_ON);
        
        if (_log_queue.empty()) _log_dirty = false;
    }

    if (_stats_dirty && lbl_stats) {
        lv_label_set_text_fmt(lbl_stats, "Bat: %.2fV  %.0fmA  |  WiFi: %d dBm", 
                              _batt_volts, _batt_amps, _rssi);
        _stats_dirty = false;
    }
}