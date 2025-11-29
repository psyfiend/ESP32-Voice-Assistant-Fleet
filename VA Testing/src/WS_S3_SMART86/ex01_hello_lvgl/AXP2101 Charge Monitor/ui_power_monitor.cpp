#include <Arduino.h>
#include <stdio.h> // For snprintf
#include <lvgl.h>
#include "ui_power_monitor.h"

// Global Definitions
lv_obj_t *ui_ScreenPower;
lv_obj_t *ui_ArcBattery;
lv_obj_t *ui_LabelBatPercent;
lv_obj_t *ui_LabelVbusVal;
lv_obj_t *ui_LabelVbatVal;
lv_obj_t *ui_LabelVsysVal;
lv_obj_t *ui_LabelStatus;
lv_obj_t *ui_LabelChargerStatus;
lv_obj_t *ui_ChartVolts;
lv_chart_series_t *ui_SeriesVbus;
lv_chart_series_t *ui_SeriesVbat;
lv_obj_t *ui_TextLog; // New Log Widget

static lv_style_t style_label_large;
static lv_style_t style_chart;
static lv_style_t style_log;

void ui_init_power_monitor() {

    // Initialize Styles
    lv_style_init(&style_label_large);
    lv_style_set_text_font(&style_label_large, &lv_font_montserrat_24); 
    lv_style_set_text_color(&style_label_large, lv_color_white());

    lv_style_init(&style_chart);
    lv_style_set_border_width(&style_chart, 0);
    lv_style_set_bg_color(&style_chart, lv_color_hex(0x202020));
    lv_style_set_bg_opa(&style_chart, LV_OPA_COVER);

    lv_style_init(&style_log);
    lv_style_set_bg_color(&style_log, lv_color_hex(0x101010));
    lv_style_set_text_color(&style_log, lv_color_hex(0x00FF00)); // Matrix Green Text
    lv_style_set_text_font(&style_log, &lv_font_montserrat_14);  // Smaller font for logs
    lv_style_set_border_color(&style_log, lv_color_hex(0x404040));
    lv_style_set_border_width(&style_log, 1);

    // Create Screen
    ui_ScreenPower = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(ui_ScreenPower, lv_color_black(), LV_PART_MAIN);

    // --- 1. Battery Gauge (Top Left) ---
    ui_ArcBattery = lv_arc_create(ui_ScreenPower);
    lv_obj_set_size(ui_ArcBattery, 150, 150);
    lv_arc_set_rotation(ui_ArcBattery, 135);
    lv_arc_set_bg_angles(ui_ArcBattery, 0, 270);
    lv_arc_set_value(ui_ArcBattery, 0);
    lv_obj_align(ui_ArcBattery, LV_ALIGN_TOP_LEFT, 20, 20);
    lv_obj_remove_style(ui_ArcBattery, NULL, LV_PART_KNOB); 
    lv_obj_set_style_arc_color(ui_ArcBattery, lv_color_hex(0x00FF00), LV_PART_INDICATOR);

    ui_LabelBatPercent = lv_label_create(ui_ScreenPower);
    lv_label_set_text(ui_LabelBatPercent, "0%");
    lv_obj_add_style(ui_LabelBatPercent, &style_label_large, 0);
    lv_obj_align_to(ui_LabelBatPercent, ui_ArcBattery, LV_ALIGN_CENTER, 0, 0);

    // --- 2. Text Stats (Top Right) ---
    lv_obj_t *text_cont = lv_obj_create(ui_ScreenPower);
    lv_obj_set_size(text_cont, 260, 180);
    lv_obj_align(text_cont, LV_ALIGN_TOP_RIGHT, -10, 10);
    lv_obj_set_style_bg_opa(text_cont, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(text_cont, 0, 0);
    lv_obj_set_flex_flow(text_cont, LV_FLEX_FLOW_COLUMN);
    
    // Main status
    ui_LabelStatus = lv_label_create(text_cont);
    lv_label_set_text(ui_LabelStatus, "Status: Init...");
    lv_obj_set_style_text_color(ui_LabelStatus, lv_color_hex(0xFFA500), 0);

    // Charger Status
    ui_LabelChargerStatus = lv_label_create(text_cont);
    lv_label_set_text(ui_LabelChargerStatus, "Mode: --");
    lv_obj_set_style_text_color(ui_LabelChargerStatus, lv_color_hex(0xAAAAAA), 0);
    lv_obj_set_style_text_font(ui_LabelChargerStatus, &lv_font_montserrat_14, 0); 

    // Voltage Readings
    ui_LabelVbusVal = lv_label_create(text_cont);
    lv_label_set_text(ui_LabelVbusVal, "VBUS: 0.00 V");
    lv_obj_set_style_text_font(ui_LabelVbusVal, &lv_font_montserrat_20, 0);

    ui_LabelVbatVal = lv_label_create(text_cont);
    lv_label_set_text(ui_LabelVbatVal, "VBAT: 0.00 V");
    lv_obj_set_style_text_font(ui_LabelVbatVal, &lv_font_montserrat_20, 0);

    ui_LabelVsysVal = lv_label_create(text_cont);
    lv_label_set_text(ui_LabelVsysVal, "VSYS: 0.00 V");
    lv_obj_set_style_text_font(ui_LabelVsysVal, &lv_font_montserrat_20, 0);

    // --- 3. History Chart (Bottom Left) ---
    // Shrunk slightly to make room for log
    ui_ChartVolts = lv_chart_create(ui_ScreenPower);
    lv_obj_set_size(ui_ChartVolts, 280, 240); 
    lv_obj_align(ui_ChartVolts, LV_ALIGN_BOTTOM_LEFT, 20, -20);
    lv_chart_set_type(ui_ChartVolts, LV_CHART_TYPE_LINE);
    lv_chart_set_point_count(ui_ChartVolts, 50); 
    lv_chart_set_range(ui_ChartVolts, LV_CHART_AXIS_PRIMARY_Y, 3000, 5200); 
    lv_chart_set_div_line_count(ui_ChartVolts, 5, 5); 
    lv_obj_add_style(ui_ChartVolts, &style_chart, 0);

    ui_SeriesVbus = lv_chart_add_series(ui_ChartVolts, lv_color_hex(0x00FFFF), LV_CHART_AXIS_PRIMARY_Y); 
    ui_SeriesVbat = lv_chart_add_series(ui_ChartVolts, lv_color_hex(0x00FF00), LV_CHART_AXIS_PRIMARY_Y); 

    // --- 4. Event Log (Bottom Right) ---
    ui_TextLog = lv_textarea_create(ui_ScreenPower);
    lv_obj_set_size(ui_TextLog, 160, 240);
    lv_obj_align(ui_TextLog, LV_ALIGN_BOTTOM_RIGHT, -20, -20);
    lv_textarea_set_text(ui_TextLog, "System Started.\n");
    lv_obj_add_style(ui_TextLog, &style_log, 0);
    lv_textarea_set_cursor_click_pos(ui_TextLog, false); // Read only
    
    lv_screen_load(ui_ScreenPower);
}

void ui_update_power_stats(const PowerStats& stats) {
        
    char buf[32];

    // Update Battery Arc & Label
    lv_arc_set_value(ui_ArcBattery, (int)(stats.battPercentage));
    snprintf(buf, sizeof(buf), "%d%%", (int)stats.battPercentage);
    lv_label_set_text(ui_LabelBatPercent, buf);

    // Update Status Color
    if (stats.isCharging) {
        lv_label_set_text(ui_LabelStatus, "CHARGING");
        lv_obj_set_style_text_color(ui_LabelStatus, lv_color_hex(0x00FF00), 0);
        lv_obj_set_style_arc_color(ui_ArcBattery, lv_color_hex(0xFFFF00), LV_PART_INDICATOR); 
    } else if (stats.isDischarging) {
        lv_label_set_text(ui_LabelStatus, "DISCHARGING");
        lv_obj_set_style_text_color(ui_LabelStatus, lv_color_hex(0xFF4444), 0);
        lv_obj_set_style_arc_color(ui_ArcBattery, lv_color_hex(0x00FF00), LV_PART_INDICATOR);
    } else {
        lv_label_set_text(ui_LabelStatus, "STANDBY");
        lv_obj_set_style_text_color(ui_LabelStatus, lv_color_hex(0x8888FF), 0);
    }

    // **Update Detailed Charger Status**
    // If the string is NULL (e.g. not provided), we clear the label or show default
    if (stats.chargerStatus != NULL) {
        lv_label_set_text(ui_LabelChargerStatus, stats.chargerStatus);
    } else {
        lv_label_set_text(ui_LabelChargerStatus, "Mode: --");
    }

    // Voltage Labels
    snprintf(buf, sizeof(buf), "VBUS: %.2f V", stats.vbusVoltage / 1000.0f);
    lv_label_set_text(ui_LabelVbusVal, buf);

    snprintf(buf, sizeof(buf), "VBAT: %.2f V", stats.battVoltage / 1000.0f);
    lv_label_set_text(ui_LabelVbatVal, buf);
    
    snprintf(buf, sizeof(buf), "VSYS: %.2f V", stats.vsysVoltage / 1000.0f);
    lv_label_set_text(ui_LabelVsysVal, buf);

    // VBUS Status
    if (!stats.isVbusIn) {
        lv_obj_set_style_text_color(ui_LabelVbusVal, lv_color_hex(0x808080), 0);
    } else {
        lv_obj_set_style_text_color(ui_LabelVbusVal, lv_color_white(), 0);
    }

    // Update Chart
    lv_chart_set_next_value(ui_ChartVolts, ui_SeriesVbus, (int)stats.vbusVoltage);
    lv_chart_set_next_value(ui_ChartVolts, ui_SeriesVbat, (int)stats.battVoltage);

    // **Update Log**
    // If the main loop passed a message string, append it to the text area
    if (stats.eventMessage != NULL) {
        lv_textarea_add_text(ui_TextLog, stats.eventMessage);
        lv_textarea_add_char(ui_TextLog, '\n'); // Auto newline
    }
}