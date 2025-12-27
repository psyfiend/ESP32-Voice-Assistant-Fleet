#include "Panel_System.h"

Panel_System::Panel_System() {
    container = NULL;
    txt_log = NULL;
    _headerRef = NULL;
    _expanded = false;
    _onToggle = nullptr;
}

void Panel_System::setOnToggleCallback(ToggleCallback cb) {
    _onToggle = cb;
}

void Panel_System::anim_height_cb(void * var, int32_t v) {
    Panel_System* p = (Panel_System*)var;
    if (!p || !p->container) return; 

    // Simple Height Animation
    lv_obj_set_height(p->container, v);
}

void Panel_System::init(lv_obj_t* parent, Panel_Header* headerRef) {

    
    _headerRef = headerRef;

    container = lv_obj_create(parent);
    lv_obj_set_width                (container, lv_pct(98)); 
    lv_obj_set_height               (container, UiToolkit::sc(40));  // Start Collapsed

    // Start hidden behind the 50px tall header
    lv_obj_align                    (container, LV_ALIGN_TOP_MID, 0, UiToolkit::sc(0)); 
    
    // Standard Radius (Fixed - top corners hidden by header overlap)
    lv_obj_set_style_radius         (container, UiToolkit::sc(12), 0);
    lv_obj_set_style_bg_color       (container, lv_color_hex(0x181818), 0); 
    lv_obj_set_style_bg_opa         (container, LV_OPA_COVER, 0); 
    
    // Standard Borders
    lv_obj_set_style_border_width   (container, UiToolkit::sc(1), 0); 
    lv_obj_set_style_border_color   (container, lv_color_hex(0x606060), 0); 
    
    // lv_obj_set_style_pad_all        (container, UiToolkit::sc(10), 0);
    lv_obj_set_style_clip_corner    (container, true, 0); 
    
    // Interaction
    // lv_obj_add_flag                 (container, LV_OBJ_FLAG_CLICKABLE);

    // Content
    // lv_obj_add_flag                 (container, LV_OBJ_FLAG_SCROLLABLE);
    // lv_obj_set_scrollbar_mode       (container, LV_SCROLLBAR_MODE_OFF);

    /*
    lv_obj_t * txt_log =    lv_obj_create(container);
    lv_obj_set_size                 (txt_log, lv_pct(100), LV_SIZE_CONTENT);    
    lv_obj_set_style_bg_opa         (txt_log, LV_OPA_TRANSP, 0); 
    lv_obj_set_style_border_width   (txt_log, 0, 0);
    lv_obj_set_style_pad_all        (txt_log, 0, 0);
    
    lv_obj_t * log_label =  lv_label_create(txt_log);
    lv_obj_align                    (log_label, LV_ALIGN_TOP_MID, 0, 0);
    lv_label_set_text               (log_label, "--- SYSTEM LOG STARTED ---\n");
    // lv_label_set_long_mode          (log_label, LV_LABEL_LONG_CLIP); 
    lv_obj_set_style_text_font      (log_label, UiToolkit::Font_Caption, 0);
    lv_obj_set_style_text_color     (log_label, lv_color_hex(0xAAAAAA), 0);
    */

    /*
    // 1. Top Static Label
    lv_obj_t* lbl_top = lv_label_create(container);
    lv_label_set_text           (lbl_top, "SYSTEM PANEL START");
    lv_obj_set_style_text_font  (lbl_top, UiToolkit::Font_Label, 0);
    lv_obj_set_style_text_color (lbl_top, lv_color_hex(0x00FF00), 0); // Green
    lv_obj_align                (lbl_top, LV_ALIGN_BOTTOM_MID, 0, 0);
    
    // 2. Bottom Static Label
    lv_obj_t* lbl_bottom = lv_label_create(container);
    lv_label_set_text           (lbl_bottom, "SYSTEM PANEL END");
    lv_obj_set_style_text_font  (lbl_bottom, UiToolkit::Font_Label, 0);
    lv_obj_set_style_text_color (lbl_bottom, lv_color_hex(0xFF0000), 0); // Red
    lv_obj_align                (lbl_bottom, LV_ALIGN_BOTTOM_MID, 0, 0);
    */

    /*
    txt_log = lv_label_create(container);
    lv_obj_set_width(txt_log, lv_pct(100));
    lv_label_set_long_mode(txt_log, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_font(txt_log, UiToolkit::Font_Caption, 0);
    lv_obj_set_style_text_color(txt_log, lv_color_hex(0xAAAAAA), 0);
    lv_label_set_text(txt_log, "--- SYSTEM LOG STARTED ---\n");
    */
}

void Panel_System::close() {
    if (_expanded) toggle();
}

void Panel_System::toggle() {
    if (!container) return;
    
    _expanded = !_expanded;
    
    // 1. Notify listeners (Hide Touch Window)
    if (_onToggle) _onToggle(_expanded);

    // 2. Close bottom panels if opening
    if (_expanded) UiToolkit::closeActiveAccordion();
    
    int32_t start_h = lv_obj_get_height(container);
    int32_t end_h = _expanded ? UiToolkit::sc(350) : 0;

    lv_anim_del(this, anim_height_cb); 

    lv_anim_t a;
    lv_anim_init        (&a);
    lv_anim_set_var     (&a, this); 
    lv_anim_set_values  (&a, start_h, end_h);
    lv_anim_set_time    (&a, 350); // Faster slide
    lv_anim_set_exec_cb (&a, anim_height_cb);
    lv_anim_set_path_cb (&a, lv_anim_path_ease_out); 
    lv_anim_start       (&a);
}

void Panel_System::log(const char* fmt, ...) {
    /* -- LOGGING DISABLED FOR ANIMATION TESTING --
    if (!txt_log) return;
    
    char buf[256];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);

    if (txt_log) {
        // Safety: Limit log size to ~4KB to prevent allocation lag
        const char* old_txt = lv_label_get_text(txt_log);
        if (old_txt && strlen(old_txt) > 4000) { 
            lv_label_set_text(txt_log, "--- LOG TRUNCATED ---\n");
        }
        
        lv_label_ins_text(txt_log, LV_LABEL_POS_LAST, buf);
        lv_label_ins_text(txt_log, LV_LABEL_POS_LAST, "\n");
        
        // Auto-scroll only if visible to prevent layout crashes on 0-height
        if (container && lv_obj_get_height(container) > 50) {
            lv_obj_scroll_to_y(container, 10000, LV_ANIM_OFF);
        }
    }
    
    Serial.print("[SYS] ");
    Serial.println(buf);
    */
}