#pragma once
#include <lvgl.h>
#include <Arduino.h>
#include "UI/UIToolkit.h"
#include "UI/Widget_ConnStatus.h"

class ConnectivityManager;

class Panel_Header {
public:
    Panel_Header();
    // conn may be null (e.g. a no-connectivity build variant); the status
    // glyph is simply omitted in that case rather than showing a fake state.
    // Re-apply this bar's colours from the live scheme. Built once at start-up and
// never repainted, so switching to Paper left a dark bar over a light page -
// the same omission the screen background had.
void restyle();

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