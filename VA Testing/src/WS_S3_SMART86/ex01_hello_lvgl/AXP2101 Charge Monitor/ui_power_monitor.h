#ifndef UI_POWER_MONITOR_H
#define UI_POWER_MONITOR_H

#include <lvgl.h>

// Struct to hold all power data
struct PowerStats {
    bool isVbusIn;
    float vbusVoltage; // mV
    float vsysVoltage; // mV
    float battVoltage; // mV
    float battPercentage; // %
    bool isCharging;
    bool isDischarging;
    
    // Optional: Event message to log (set to NULL if no event)
    const char* eventMessage;
    const char* chargerStatus;
};

// Global UI Objects
extern lv_obj_t *ui_ScreenPower;
extern lv_obj_t *ui_ArcBattery;
extern lv_obj_t *ui_LabelBatPercent;
extern lv_obj_t *ui_LabelVbusVal;   
extern lv_obj_t *ui_LabelVbatVal;
extern lv_obj_t *ui_LabelVsysVal;
extern lv_obj_t *ui_LabelStatus;
extern lv_obj_t *ui_LabelChargerStatus;
extern lv_obj_t *ui_ChartVolts;
extern lv_chart_series_t *ui_SeriesVbus;
extern lv_chart_series_t *ui_SeriesVbat;
extern lv_obj_t *ui_TextLog; // New Log Widget

// Initialize the Power Monitor Screen
void ui_init_power_monitor();

// Update the UI with fresh data
void ui_update_power_stats(const PowerStats& stats);

#endif // UI_POWER_MONITOR_H