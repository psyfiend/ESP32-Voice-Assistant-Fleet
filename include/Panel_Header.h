#pragma once
#include <lvgl.h>
#include <Arduino.h>
#include "UiToolkit.h"
#include "Widget_ConnStatus.h"

class ConnectivityManager;

class Panel_Header {
public:
    Panel_Header();
    // conn may be null (e.g. a no-connectivity build variant); the status
    // glyph is simply omitted in that case rather than showing a fake state.
    void init(lv_obj_t* parent, const char* title, ConnectivityManager* conn = nullptr);
    void tick(); // Update stats

    // --= NEW: Accessors for System Panel Interaction =--
    lv_obj_t* getContainer() { return container; }
    // Returns the clickable container wrapper, not just the label
    lv_obj_t* getStatusIcon() { return btn_status; } 
    lv_obj_t* getTitleLabel() { return lbl_title; }
    Widget_ConnStatus& getConnStatus() { return connStatus; }

private:
    lv_obj_t* container;
    lv_obj_t* lbl_title;
    lv_obj_t* btn_status; // Wrapper for the icon
    Widget_ConnStatus connStatus;  // replaces the old static LV_SYMBOL_WIFI label
};