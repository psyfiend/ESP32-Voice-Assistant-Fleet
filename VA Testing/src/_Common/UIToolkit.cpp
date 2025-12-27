#include "UiToolkit.h"

static lv_obj_t *toast_panel = NULL;
static lv_obj_t *toast_label = NULL;
static lv_timer_t *toast_timer_handle = NULL;
static lv_obj_t * _active_accordion_panel = NULL;
static UiActionCallback _system_close_cb = NULL; 

// Initialize Static Fonts
const lv_font_t* UiToolkit::Font_Caption = NULL;
const lv_font_t* UiToolkit::Font_Label = NULL;
const lv_font_t* UiToolkit::Font_Button = NULL;
const lv_font_t* UiToolkit::Font_PanelHeader = NULL;
const lv_font_t* UiToolkit::Font_Hero = NULL;

int32_t UiToolkit::sc(int32_t val) {
    return (int32_t)(val * UI_SCALE);
}

// --- Toast Logic ---
static void toast_timer_cb(lv_timer_t * t) {
    if (toast_panel) lv_obj_add_flag(toast_panel, LV_OBJ_FLAG_HIDDEN);
    toast_timer_handle = NULL;
}

void UiToolkit::init() {

    // --= FONT MAPPING =--
    #ifdef HIGH_DPI_DISPLAY
        // P4 Smart86 (High Res)
        Font_Caption     = &lv_font_montserrat_16;
        Font_Label       = &lv_font_montserrat_20; 
        Font_Button      = &lv_font_montserrat_22;
        Font_PanelHeader = &lv_font_montserrat_22;
        Font_Hero        = &lv_font_montserrat_34;
    #else
        Font_Caption     = &lv_font_montserrat_10;
        Font_Label       = &lv_font_montserrat_12; 
        Font_Button      = &lv_font_montserrat_14;
        Font_PanelHeader = &lv_font_montserrat_14;
        Font_Hero        = &lv_font_montserrat_24;
    #endif

    // Toast Setup - Top Z Index
    toast_panel = lv_obj_create(lv_layer_top()); // Use layer_top to float over everything
    lv_obj_set_size             (toast_panel, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_align                (toast_panel, LV_ALIGN_TOP_MID, 0, sc(60)); 
    lv_obj_set_style_bg_color   (toast_panel, lv_color_hex(0x333333), 0);
    lv_obj_set_style_radius     (toast_panel, sc(20), 0);
    lv_obj_set_style_pad_all    (toast_panel, sc(15), 0);
    lv_obj_add_flag             (toast_panel, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag           (toast_panel, LV_OBJ_FLAG_CLICKABLE);

    toast_label = lv_label_create(toast_panel);
    lv_label_set_text           (toast_label, "Toast");
    lv_obj_set_style_text_color (toast_label, lv_color_white(), 0);
    lv_obj_set_style_text_font  (toast_label, Font_Hero, 0); // Use semantic font
}

void UiToolkit::show_toast(const char* text, uint32_t duration_ms) {
    if (!toast_panel || !toast_label) return;

    lv_label_set_text           (toast_label, text);
    lv_obj_clear_flag           (toast_panel, LV_OBJ_FLAG_HIDDEN);
    
    // Reset Timer
    if (toast_timer_handle) {
        lv_timer_del(toast_timer_handle);
    }
    toast_timer_handle = lv_timer_create(toast_timer_cb, duration_ms, NULL);
    lv_timer_set_repeat_count   (toast_timer_handle, 1);
}

// Expanded Accordion Panel Accessor
lv_obj_t* UiToolkit::getActiveAccordionPanel() {
    return _active_accordion_panel;
}

void UiToolkit::registerSystemCloseCb(UiActionCallback cb) {
    _system_close_cb = cb;
}

// --- Panel Animation ---
static void anim_height_cb(void * var, int32_t v) {
    lv_obj_set_height((lv_obj_t*)var, v);
}

static void execute_panel_toggle(lv_obj_t* panel, bool expand) {
    int32_t h_start = lv_obj_get_height(panel);
    int32_t h_end   = expand ? UiToolkit::sc(280) : UiToolkit::sc(85);

    lv_obj_t * header   = lv_obj_get_child(panel, 0);
    lv_obj_t * icon     = lv_obj_get_child(header, 1);
    if (icon) {
        lv_label_set_text(icon, expand ? LV_SYMBOL_DOWN : LV_SYMBOL_UP);
    }

    lv_anim_t a;
    lv_anim_init        (&a);
    lv_anim_set_var     (&a, panel);
    lv_anim_set_values  (&a, h_start, h_end);
    lv_anim_set_time    (&a, 300); 
    lv_anim_set_exec_cb (&a, anim_height_cb);
    lv_anim_set_path_cb (&a, lv_anim_path_ease_in_out);
    lv_anim_start       (&a);
}

// --= NEW: Implementation =--
void UiToolkit::closeActiveAccordion() {
    if (_active_accordion_panel) {
        execute_panel_toggle(_active_accordion_panel, false);
        _active_accordion_panel = NULL;
    }
}

static void panel_header_click_cb(lv_event_t * e) {
    lv_obj_t * panel = (lv_obj_t*)lv_event_get_user_data(e);

    // Determine if we are expanding or collapsing
    bool currently_collapsed = (lv_obj_get_height(panel) == UiToolkit::sc(85));

    if (currently_collapsed) {
        // --= Expanding =--
        // Collapse any currently active panel
        if (_system_close_cb) _system_close_cb(); // Close System Panel if open
        if (_active_accordion_panel && _active_accordion_panel != panel) {
            execute_panel_toggle(_active_accordion_panel, false);
        }

        execute_panel_toggle(panel, true);
        _active_accordion_panel = panel;
    } else {
        // --= Collapsing =--
        execute_panel_toggle(panel, false);
        if (_active_accordion_panel == panel) {
            _active_accordion_panel = NULL;
        }
    }
}

lv_obj_t* UiToolkit::create_collapsible_panel(lv_obj_t* parent, const char* title, lv_obj_t** content_container) {
    lv_obj_t * pnl = lv_obj_create(parent);
    lv_obj_set_width            (pnl, 0);       // Set base width to 0 so flex takes over completely
    lv_obj_set_height           (pnl, sc(85));  // Collapsed Height
    lv_obj_set_flex_grow        (pnl, 1);       // Tell flex engine to share available space equally (1:1)
    lv_obj_set_style_bg_color   (pnl, lv_color_hex(0x181818), 0);
    lv_obj_set_style_radius     (pnl, sc(12), 0);
    lv_obj_set_style_border_width(pnl, 1, 0);
    lv_obj_set_style_border_color(pnl, lv_color_hex(0x606060), 0);
    lv_obj_set_style_pad_all    (pnl, 0, 0); 
    lv_obj_set_style_pad_row    (pnl, sc(10), 0);   
    lv_obj_set_style_clip_corner(pnl, true, 0); 
    lv_obj_clear_flag           (pnl, LV_OBJ_FLAG_SCROLLABLE);

    // Header
    lv_obj_t * header = lv_obj_create(pnl);
    lv_obj_set_size             (header, lv_pct(100), sc(45));
    lv_obj_set_style_bg_opa     (header, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(header, 0, 0);
    lv_obj_set_style_pad_all    (header, sc(15), 0); 
    lv_obj_add_event_cb         (header, panel_header_click_cb, LV_EVENT_CLICKED, pnl);
    lv_obj_add_flag             (header, LV_OBJ_FLAG_CLICKABLE);

    lv_obj_t * lbl = lv_label_create(header);
    lv_label_set_text           (lbl, title);
    lv_obj_align                (lbl, LV_ALIGN_LEFT_MID, 0, 0);
    lv_obj_set_style_text_font  (lbl, Font_PanelHeader, 0); // Use Semantic Font
    lv_obj_set_style_text_color (lbl, lv_color_hex(0x00A8FF), 0);

    lv_obj_t * icon = lv_label_create(header);
    lv_label_set_text           (icon, LV_SYMBOL_UP); 
    lv_obj_align                (icon, LV_ALIGN_RIGHT_MID, 0, 0);
    lv_obj_set_style_text_color (icon, lv_color_hex(0x808080), 0);

    // Content Container
    *content_container = lv_obj_create(pnl);
    lv_obj_set_width            (*content_container, lv_pct(100));
    lv_obj_set_height           (*content_container, LV_SIZE_CONTENT); 
    lv_obj_set_y                (*content_container, sc(60)); // Start below header
    lv_obj_set_style_bg_opa     (*content_container, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(*content_container, 0, 0);
    lv_obj_set_style_pad_all    (*content_container, sc(10), 0);
    lv_obj_set_flex_flow        (*content_container, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_gap    (*content_container, sc(10), 0);

    return pnl;
}

lv_obj_t* UiToolkit::create_panel_row(lv_obj_t* pnl_content, lv_obj_t** row_container) {
    lv_obj_t * row = lv_obj_create(pnl_content);
    lv_obj_set_size             (row, lv_pct(100), sc(ROW_HEIGHT));
    lv_obj_set_flex_flow        (row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align       (row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_bg_opa     (row, LV_OPA_0, 0);
    lv_obj_set_style_border_width(row, 0, 0);
    lv_obj_set_style_pad_all    (row, 0, 0);
    lv_obj_set_style_pad_gap    (row, sc(6), 0);   // Gap between columns
    lv_obj_remove_flag          (row, LV_OBJ_FLAG_SCROLLABLE);

    if (row_container != NULL) {
        *row_container = row;
    }

    return row;
}

lv_obj_t* UiToolkit::create_slider_col(lv_obj_t* parent, const char* title, lv_obj_t** out_col, lv_obj_t** out_slider) {
    lv_obj_t * col = lv_obj_create(parent);
    
    // lv_obj_set_size(col, lv_pct(100), lv_pct(100));
    
    // Layout
    lv_obj_set_size             (col, 0, lv_pct(100));  // Start at 0 width
    lv_obj_set_flex_grow        (col, 1);  // Grow to fill space
    lv_obj_set_flex_flow        (col, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align       (col, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);  
    lv_obj_set_style_bg_opa     (col, LV_OPA_0, 0);
    lv_obj_set_style_border_width(col, 0, 0);
    lv_obj_set_style_pad_all    (col, 0, 0);
    lv_obj_set_style_pad_left   (col, sc(10), 0);
    lv_obj_set_style_pad_right  (col, sc(10), 0);
    lv_obj_set_style_pad_row    (col, sc(6), 0);
    lv_obj_remove_flag          (col, LV_OBJ_FLAG_SCROLLABLE);

    // Row 1 - Label
    lv_obj_t * lbl = lv_label_create(col);
    lv_obj_set_size             (lbl, lv_pct(100), LV_SIZE_CONTENT);
    lv_label_set_text           (lbl, title);
    lv_obj_set_style_text_align (lbl, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_font  (lbl, Font_Label, 0); // Use Semantic Font
    lv_obj_set_style_text_color (lbl, lv_color_hex(0x808080), 0);

    // Row 2 - Slider
    lv_obj_t * slider = lv_slider_create(col);
    lv_obj_set_style_width      (slider, lv_pct(100), 0);
    lv_obj_set_flex_grow        (slider, 1);
    lv_obj_set_style_max_height (slider, sc(15), 0);
    lv_obj_set_align            (slider, LV_ALIGN_CENTER);
    // lv_obj_set_style_pad_bottom (slider, sc(0), 0);
    lv_slider_set_value         (slider, 50, LV_ANIM_OFF);
    lv_obj_remove_flag          (slider, LV_OBJ_FLAG_SCROLLABLE);

    // --= Assign Pointers =--
    if (out_col != NULL)    *out_col = col;
    if (out_slider != NULL) *out_slider = slider;

    return col;
}