#include "UI/Panel_System.h"
#include "UI/ReferencePage.h"
#include "UI/UITokens.h"

// The single System panel, so the SystemReport sink (a plain function pointer)
// can reach it. One panel exists by construction.
static Panel_System *s_self = nullptr;

Panel_System::Panel_System() {
    _ui_root    = NULL;
    _ui_content = NULL;
    _ui_actions = NULL;
    _ui_grid    = NULL;
    _lbl_hdr    = NULL;
    _lbl_scheme = NULL;
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
        if (p->_onDumpRequested) p->_onDumpRequested();
    }
}

void Panel_System::reportSink(const char *line) {
    if (s_self) s_self->log("%s", line);
}

// One knob button. Capture-less lambdas only - an lv_event_cb_t is a plain
// function pointer, so the panel arrives as user_data rather than in a capture.
void Panel_System::setHeaderLabel(const char *text) {
    if (_lbl_hdr && text) lv_label_set_text(_lbl_hdr, text);
}

void Panel_System::setSchemeLabel(const char *text) {
    if (_lbl_scheme && text) lv_label_set_text(_lbl_scheme, text);
}

static lv_obj_t *knobButton(lv_obj_t *parent, Panel_System *self,
                            const char *text, lv_event_cb_t cb) {
    lv_obj_t *b = lv_button_create(parent);
    lv_obj_set_height             (b, UIToolkit::sc(32));
    lv_obj_set_width              (b, LV_SIZE_CONTENT);
    lv_obj_set_flex_grow          (b, 1);
    lv_obj_add_event_cb           (b, cb, LV_EVENT_CLICKED, self);
    lv_obj_set_style_bg_color     (b, UI::c(UI::pal().SURFACE_ALT), 0);
    lv_obj_set_style_border_width (b, 1, 0);
    lv_obj_set_style_border_color (b, UI::border(), 0);

    lv_obj_t *l = lv_label_create(b);
    lv_label_set_text             (l, text);
    lv_obj_center                 (l);
    lv_obj_set_style_text_font    (l, UIToolkit::Font_Button, 0);
    lv_obj_set_style_text_color   (l, UI::c(UI::pal().TEXT), 0);
    return b;
}

void Panel_System::init(lv_obj_t* parent, Panel_Header* headerRef) {
    _headerRef = headerRef;

    // Receive the System Doctor's output. Registering rather than being
    // written into is what lets SystemReport stay LVGL-free.
    s_self = this;
    SystemReport::addSink(reportSink);

    // 1. Create the WRAPPER (_ui_root)
    // Acts as the "Viewmask". Positioned explicitly below the header.
    _ui_root = lv_obj_create    (parent);
    lv_obj_set_width            (_ui_root, lv_pct(100)); // Full width
    lv_obj_set_height           (_ui_root, 0); 
    lv_obj_set_pos              (_ui_root, 0, UIToolkit::sc(30)); // Offset Y by Header Height
    
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
    lv_obj_set_style_bg_color       (_ui_content, UI::c(UI::pal().SURFACE), 0);
    
    // -- CORNER HACK --
    // We want square top corners (to connect to header) and rounded bottom corners.
    // LVGL radius applies to all corners.
    // Trick: Move content UP by the radius amount to clip the top rounded corners off.
    int32_t radius = UIToolkit::sc(15);
    lv_obj_set_style_radius     (_ui_content, UIToolkit::sc(15), 0);
    lv_obj_set_y                (_ui_content, -(UIToolkit::sc(15))); // Shift up to hide top curves
    
    // Resetting size to account for the shift isn't strictly necessary if we use flex grow inside, 
    // but effectively the bottom area will be "Radius" pixels shorter than visual. 
    // Actually simpler: Just set height to 100% + radius.
    lv_obj_set_height           (_ui_content, lv_pct(100)); // It will clip bottom if we aren't careful, but since we animate wrapper, it's fine.
    // Let's stick to standard alignment for now, just radius.
    // If you really want square top, we can accept rounded top or use the hack. 
    // Let's use the hack:
    
    // lv_obj_set_style_margin_top (_ui_content, -(UIToolkit::sc(15)), 0);
    // lv_obj_set_style_pad_top    (_ui_content, UIToolkit::sc(15) + UIToolkit::sc(10), 0); // Radius + padding
    
    lv_obj_set_style_border_color   (_ui_content, UI::border(), 0);
    lv_obj_set_style_border_width   (_ui_content, UIToolkit::sc(2), 0);
    // lv_obj_set_style_pad_all        (_ui_content, UIToolkit::sc(10), 0);
    lv_obj_set_style_pad_left       (_ui_content, UIToolkit::sc(10), 0);
    lv_obj_set_style_pad_right      (_ui_content, UIToolkit::sc(10), 0);
    lv_obj_set_style_pad_bottom     (_ui_content, UIToolkit::sc(10), 0);
    lv_obj_clear_flag               (_ui_content, LV_OBJ_FLAG_SCROLLABLE); // Static background

    lv_obj_set_flex_flow            (_ui_content, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align           (_ui_content, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_row        (_ui_content, UIToolkit::sc(8), 0);

    // -- ROW 1: Stats --
    lbl_stats = lv_label_create     (_ui_content);
    lv_obj_set_width                (lbl_stats, lv_pct(100));
    lv_label_set_text               (lbl_stats, "System Ready.");
    lv_obj_set_style_text_color     (lbl_stats, UI::c(UI::pal().ST_OK), 0);
    lv_obj_set_style_text_font      (lbl_stats, UIToolkit::Font_Label, 0);

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
    lv_obj_set_style_pad_gap        (_ui_actions, UIToolkit::sc(8), 0);
    lv_obj_clear_flag               (_ui_actions, LV_OBJ_FLAG_SCROLLABLE);
    UI::tameScroll                  (_ui_actions);

    // Button: Dump Config
    lv_obj_t* btn = lv_button_create(_ui_actions);
    lv_obj_set_height               (btn, UIToolkit::sc(32));
    lv_obj_set_width                (btn, LV_SIZE_CONTENT);
    lv_obj_set_flex_grow            (btn, 1);   // two buttons split the row evenly
    lv_obj_add_event_cb             (btn, btn_action_cb, LV_EVENT_CLICKED, this);
    lv_obj_set_style_bg_color       (btn, UI::c(UI::pal().ACCENT), 0); // Cyan
    
    lv_obj_t* lbl = lv_label_create(btn);
    lv_label_set_text               (lbl, "Dump Config");
    lv_obj_center                   (lbl);
    lv_obj_set_style_text_font      (lbl, UIToolkit::Font_Button, 0);

    // Button: Design Tokens — opens the 2.2 reference page as its own screen.
    // Lives here rather than behind a build flag so the schemes can be flipped
    // and compared without reflashing, and so the page survives into the Phase
    // 4 settings surface instead of being throwaway scaffolding.
    lv_obj_t* btnRef = lv_button_create(_ui_actions);
    lv_obj_set_height               (btnRef, UIToolkit::sc(32));
    lv_obj_set_width                (btnRef, LV_SIZE_CONTENT);
    lv_obj_set_flex_grow            (btnRef, 1);
    lv_obj_add_event_cb             (btnRef, [](lv_event_t *e) {
                                        Panel_System *self = (Panel_System *)lv_event_get_user_data(e);
                                        if (self) self->requestTokens();
                                     }, LV_EVENT_CLICKED, this);
    lv_obj_set_style_bg_color       (btnRef, UI::c(UI::pal().SURFACE_ALT), 0);
    lv_obj_set_style_border_width   (btnRef, 1, 0);
    lv_obj_set_style_border_color   (btnRef, UI::border(), 0);

    lv_obj_t* lblRef = lv_label_create(btnRef);
    lv_label_set_text               (lblRef, "Tokens");
    lv_obj_center                   (lblRef);
    lv_obj_set_style_text_font      (lblRef, UIToolkit::Font_Button, 0);
    lv_obj_set_style_text_color     (lblRef, UI::c(UI::pal().TEXT), 0);

    // Button: Cards - opens the 2.4 card demo as its own screen.
    //
    // Same shape as Tokens above and for the same reason: the dashboard
    // underneath stays untouched, so a card layout can be judged without
    // disturbing anything that already works. Unlike Tokens it goes through a
    // registered callback, because the page it opens needs the entity registry
    // and the card binder and this panel must not know about either.
    lv_obj_t* btnCards = lv_button_create(_ui_actions);
    lv_obj_set_height               (btnCards, UIToolkit::sc(32));
    lv_obj_set_width                (btnCards, LV_SIZE_CONTENT);
    lv_obj_set_flex_grow            (btnCards, 1);
    lv_obj_add_event_cb             (btnCards, [](lv_event_t *e) {
                                        Panel_System *self = (Panel_System *)lv_event_get_user_data(e);
                                        if (self) self->requestCards();
                                     }, LV_EVENT_CLICKED, this);
    lv_obj_set_style_bg_color       (btnCards, UI::c(UI::pal().SURFACE_ALT), 0);
    lv_obj_set_style_border_width   (btnCards, 1, 0);
    lv_obj_set_style_border_color   (btnCards, UI::border(), 0);

    lv_obj_t* lblCards = lv_label_create(btnCards);
    lv_label_set_text               (lblCards, "Cards");
    lv_obj_center                   (lblCards);
    lv_obj_set_style_text_font      (lblCards, UIToolkit::Font_Button, 0);
    lv_obj_set_style_text_color     (lblCards, UI::c(UI::pal().TEXT), 0);

    // -- ROW 2b: the grid knobs -------------------------------------------
    //
    // A second row rather than five more buttons in the first: CYD_S3_3248 is
    // 320 px wide and eight flex-grown buttons on one row would each be about
    // a finger-width too narrow to hit.
    _ui_grid = lv_obj_create        (_ui_content);
    lv_obj_set_width                (_ui_grid, lv_pct(100));
    lv_obj_set_height               (_ui_grid, LV_SIZE_CONTENT);
    lv_obj_set_layout               (_ui_grid, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow            (_ui_grid, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align           (_ui_grid, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_bg_opa         (_ui_grid, LV_OPA_TRANSP, 0);
    lv_obj_set_style_pad_all        (_ui_grid, 0, 0);
    lv_obj_set_style_border_width   (_ui_grid, 0, 0);
    lv_obj_set_style_pad_gap        (_ui_grid, UIToolkit::sc(8), 0);
    lv_obj_clear_flag               (_ui_grid, LV_OBJ_FLAG_SCROLLABLE);
    UI::tameScroll                  (_ui_grid);

    knobButton(_ui_grid, this, "Col -", [](lv_event_t *e) {
        Panel_System *p = (Panel_System *)lv_event_get_user_data(e);
        if (p) p->requestGrid(Panel_System::GridAction::CARD_W_UP);   // wider card, fewer columns
    });
    knobButton(_ui_grid, this, "Col +", [](lv_event_t *e) {
        Panel_System *p = (Panel_System *)lv_event_get_user_data(e);
        if (p) p->requestGrid(Panel_System::GridAction::CARD_W_DOWN);
    });
    knobButton(_ui_grid, this, "Row -", [](lv_event_t *e) {
        Panel_System *p = (Panel_System *)lv_event_get_user_data(e);
        if (p) p->requestGrid(Panel_System::GridAction::ASPECT_UP);   // taller hint, fewer rows
    });
    knobButton(_ui_grid, this, "Row +", [](lv_event_t *e) {
        Panel_System *p = (Panel_System *)lv_event_get_user_data(e);
        if (p) p->requestGrid(Panel_System::GridAction::ASPECT_DOWN);
    });
    knobButton(_ui_grid, this, "Deck", [](lv_event_t *e) {
        Panel_System *p = (Panel_System *)lv_event_get_user_data(e);
        if (p) p->requestGrid(Panel_System::GridAction::DECK_TOGGLE);
    });

    // Header mode, on the LIVE dashboard rather than on the bench. All three
    // modes ship - cards.md treats this as "what a card looks like when nobody
    // chose", not an elimination - so this is the owner picking a default by
    // looking at it, and it is the same control the device settings page will
    // eventually own.
    lv_obj_t *btnHdr = knobButton(_ui_grid, this, "Tag", [](lv_event_t *e) {
        Panel_System *p = (Panel_System *)lv_event_get_user_data(e);
        if (p) p->requestGrid(Panel_System::GridAction::HDR_CYCLE);
    });
    _lbl_hdr = lv_obj_get_child(btnHdr, 0);

    // Scheme, on the dashboard rather than on the reference page. The owner
    // asked for it, and it is also the fix for WHY he was on that page: the
    // only reason to open Tokens during normal use was to change the scheme.
    lv_obj_t *btnScheme = knobButton(_ui_grid, this, "Fleet", [](lv_event_t *e) {
        Panel_System *p = (Panel_System *)lv_event_get_user_data(e);
        if (p) p->requestScheme();
    });
    _lbl_scheme = lv_obj_get_child(btnScheme, 0);

    // -- ROW 3: Log Container --
    lv_obj_t* log_box = lv_obj_create(_ui_content);
    lv_obj_set_width                (log_box, lv_pct(100));
    
    // FLEX GROW: Take all remaining space!
    lv_obj_set_flex_grow            (log_box, 1); 
    
    lv_obj_set_style_bg_color       (log_box, UI::c(UI::pal().GROUND), 0);
    lv_obj_set_style_pad_all        (log_box, UIToolkit::sc(8), 0);
    lv_obj_set_style_radius         (log_box, UIToolkit::sc(4), 0);
    lv_obj_set_scrollbar_mode       (log_box, LV_SCROLLBAR_MODE_AUTO); // Enable scrolling here
    
    txt_log = lv_label_create       (log_box);
    lv_obj_set_width                (txt_log, lv_pct(100));
    lv_label_set_long_mode          (txt_log, LV_LABEL_LONG_WRAP);
    lv_label_set_text               (txt_log, "> Init...");
    lv_obj_set_style_text_color     (txt_log, UI::c(UI::pal().TEXT), 0); 
    
    if(UIToolkit::Font_Caption) {
        lv_obj_set_style_text_font  (txt_log, UIToolkit::Font_Caption, 0);
    } else {
        lv_obj_set_style_text_font  (txt_log, &lv_font_montserrat_14, 0);
    }

    _ui_timer = lv_timer_create     (_ui_timer_cb, 50, this); // 50ms for faster log flushing
}

void Panel_System::toggle() {
    _expanded = !_expanded;
    if (_onToggle) _onToggle(_expanded);
    if (_expanded) UIToolkit::closeActiveAccordion();
    
    int32_t start_h = lv_obj_get_height(_ui_root);
    
    // Calculate Safe Height: Screen - Header(50) - BottomGap(100)
    int32_t screen_h = lv_display_get_vertical_resolution(lv_display_get_default());
    int32_t max_h = screen_h - UIToolkit::sc(50) - UIToolkit::sc(100); 
    
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

    // No Serial mirroring here. SystemReport owns that decision now, and
    // doing it in both places would double every report line.

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