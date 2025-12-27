#pragma once
#include <lvgl.h>
#include <Arduino.h>
#include "Panel_Header.h"
#include "UiToolkit.h"

class Panel_System {
public:
    Panel_System();
    
    // Init needs the header to link the animation
    void init(lv_obj_t* parent, Panel_Header* headerRef);
    
    // Public logging
    void log(const char* fmt, ...);
    
    // Toggle the drawer
    void close(); // NEW: Explicit close needed for external control
    void toggle();

    // Callback registration
    using ToggleCallback = std::function<void(bool isOpen)>;
    void setOnToggleCallback(ToggleCallback cb);
    
    lv_obj_t* getContainer() { return container; }

private:
    lv_obj_t* container;
    lv_obj_t* txt_log;
    Panel_Header* _headerRef;
    bool _expanded;
    ToggleCallback _onToggle;

    // Animation Callbacks
    static void anim_height_cb(void * var, int32_t v);
};