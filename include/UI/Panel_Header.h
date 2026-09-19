#pragma once
#include <lvgl.h>
#include <Arduino.h>
#include "UI/UIToolkit.h"
#include "UI/Widget_ConnStatus.h"
#include "UI/Widget_MqttStatus.h"

class ConnectivityManager;
class MqttManager;

class Panel_Header {
public:
    Panel_Header();
    // conn may be null (e.g. a no-connectivity build variant); the status
    // glyph is simply omitted in that case rather than showing a fake state.
    // Re-apply this bar's colours from the live scheme. Built once at start-up and
// never repainted, so switching to Paper left a dark bar over a light page -
// the same omission the screen background had.
void restyle();

    // mqtt may be null on the same terms as conn: an indicator with nothing
    // behind it is omitted rather than shown reporting a made-up state.
    void init(lv_obj_t* parent, const char* title, ConnectivityManager* conn = nullptr,
              MqttManager* mqtt = nullptr);
    void tick(); // Update stats

    // --= NEW: Accessors for System Panel Interaction =--
    lv_obj_t* getContainer() { return container; }
    // Returns the clickable container wrapper, not just the label
    lv_obj_t* getStatusIcon() { return btn_status; } 
    lv_obj_t* getTitleLabel() { return lbl_title; }
    Widget_ConnStatus& getConnStatus() { return connStatus; }
    Widget_MqttStatus& getMqttStatus() { return mqttStatus; }

private:
    lv_obj_t* container;
    lv_obj_t* lbl_title;
    lv_obj_t* btn_status; // Wrapper for the icon
    lv_obj_t* slots;              // right-hand status cluster, laid out as a row
    Widget_ConnStatus connStatus;  // replaces the old static LV_SYMBOL_WIFI label
    Widget_MqttStatus mqttStatus;  // sibling glyph; the two fail independently
};