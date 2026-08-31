#pragma once
#include <lvgl.h>
#include <Arduino.h>
#include "UiToolkit.h"

class Panel_Header {
public:
    Panel_Header();
    void init(lv_obj_t* parent, const char* title);
    void tick(); // Update stats

    // --= NEW: Accessors for System Panel Interaction =--
    lv_obj_t* getContainer() { return container; }
    // Returns the clickable container wrapper, not just the label
    lv_obj_t* getStatusIcon() { return btn_status; } 
    lv_obj_t* getTitleLabel() { return lbl_title; }

private:
    lv_obj_t* container;
    lv_obj_t* lbl_title;
    lv_obj_t* btn_status; // Wrapper for the icon
    lv_obj_t* lbl_status_icon; // The actual symbol text
};