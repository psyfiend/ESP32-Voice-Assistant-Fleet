#pragma once
#include <lvgl.h>
#include <Arduino.h>
#include <string>
#include <vector>
#include <functional> // Required for std::function
#include "Panel_Header.h"
#include "UiToolkit.h"

class Panel_System {
public:
    Panel_System();
    ~Panel_System();

    // Init needs the header to link the animation
    void init(lv_obj_t* parent, Panel_Header* headerRef);
    
    // Public logging - SAFE to call from anywhere (buffers data)
    void log(const char* fmt, ...);

    // Controls whether log() also mirrors to Serial. Off by default - most
    // of what debug_dump_config() logs already duplicates the boot-time
    // dashboard's own direct Serial prints. debug_dump_config() turns this
    // on for the duration of a manually-triggered ("Dump Config" button) run,
    // or for an automatic boot run if -D DUMP_CONFIG is set.
    void setSerialEcho(bool on) { _echoToSerial = on; }

    // Update system stats - SAFE to call from anywhere
    void updateSystemStats(float voltage, float current, int wifi_rssi);

    // Toggle the drawer
    void close(); 
    void toggle();

    // Callback registration
    using ToggleCallback = std::function<void(bool isOpen)>;
    void setOnToggleCallback(ToggleCallback cb);
    
    // Returns the main wrapper (for parenting checks if needed)
    lv_obj_t* getContainer() { return _ui_root; }

private:
    // -- UI Objects --
    lv_obj_t* _ui_root;    // The Outer Wrapper (Animates Height, No Padding)
    lv_obj_t* _ui_content; // The Inner Container (Has Padding & Style, Auto Height)
    lv_obj_t* _ui_actions; // Button Row
    
    lv_obj_t* txt_log;     // The log text label
    lv_obj_t* lbl_stats;   // The stats header label

    Panel_Header* _headerRef;
    
    // -- Logic --
    bool _expanded;
    ToggleCallback _onToggle;

    // -- Animation --
    static void anim_height_cb(void * var, int32_t v);

    // -- Callbacks --
    static void btn_action_cb(lv_event_t* e);
    
    // -- Safe Data Buffering --
    std::vector<std::string> _log_queue;
    bool _log_dirty;
    bool _echoToSerial = false;
    
    float _batt_volts = 0;
    float _batt_amps = 0;
    int _rssi = 0;
    bool _stats_dirty;

    // -- Timer for UI Updates --
    lv_timer_t* _ui_timer;
    static void _ui_timer_cb(lv_timer_t* timer);
    void _tick(); // Instance method called by timer
};