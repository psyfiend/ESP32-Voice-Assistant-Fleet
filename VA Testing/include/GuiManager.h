#pragma once
#include <lvgl.h>
#include <Arduino_GFX_Library.h>
#include "DisplayManager.h"
#include "TouchManager.h"

class GuiManager {
public:
    GuiManager();
    
    // Core Managers
    DisplayManager displayMgr;
    TouchManager touchMgr;

    // Lifecycle
    void begin();
    void update(); // Call in loop()

    // Accessors
    lv_display_t* getLvDisplay() { return _disp; }
    lv_indev_t* getLvIndev() { return _indev; }

private:
    lv_display_t *_disp;
    lv_indev_t *_indev;
    uint16_t *_draw_buf;
    uint16_t *_draw_buf2;
};