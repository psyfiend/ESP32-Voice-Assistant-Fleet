#pragma once
//
// LVGL_Startup - the LVGL engine, and nothing that draws.
//
// Buffer allocation, lv_init(), the tick and log callbacks, the flush and
// touch-read bridges, and display + indev registration. What used to be all of
// GUIManager.cpp despite the name.
//
// The signature deliberately mirrors Espressif's lvgl_port_init(lcd, tp) and
// Waveshare's bsp_display_start(): hardware is initialised by SystemCore first
// and handed in here. LVGL_Startup borrows the managers, it never owns them.
//
#include <lvgl.h>

class DisplayManager;
class TouchManager;

namespace LVGL_Startup {

// Call after SystemCore::begin() has brought up display and touch.
bool begin(DisplayManager &display, TouchManager &touch);

// lv_timer_handler(). Call every loop() from the LVGL thread - which, with
// LV_USE_OS == LV_OS_NONE, is loop() itself.
void tick();

// --= Locking =--
//
// No-ops today, and deliberately so. LVGL runs on loop() with
// LV_USE_OS == LV_OS_NONE; thread safety comes from ROADMAP 4.2's rule that
// providers never touch LVGL, plus EntityRegistry's own mutex and dirty set.
// That is a stronger guarantee than a display mutex and it is enforced by the
// architecture rather than by everyone remembering to lock.
//
// They exist anyway because they are the only thing that makes reversing that
// decision cheap. If every caller outside the LVGL thread goes through them
// from the start, moving to LV_OS_FREERTOS and a dedicated LVGL task later is
// a change to this one file instead of an audit of the whole tree. They also
// match the shape of esp_lvgl_port and esp_lvgl_adapter, which is what keeping
// an ESP-IDF port open actually costs at this stage.
//
// See docs/design/startup.md section 5.
bool lock(int timeout_ms = -1);
void unlock();

// RAII form. Prefer this over bare lock()/unlock() so an early return cannot
// leak the lock once these stop being no-ops.
class ScopedLock {
public:
    explicit ScopedLock(int timeout_ms = -1) { _held = lock(timeout_ms); }
    ~ScopedLock() { if (_held) unlock(); }
    bool held() const { return _held; }
    ScopedLock(const ScopedLock &) = delete;
    ScopedLock &operator=(const ScopedLock &) = delete;
private:
    bool _held;
};

lv_display_t *display();
lv_indev_t   *indev();

} // namespace LVGL_Startup
