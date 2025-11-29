#include <Wire.h>
#include <Arduino.h>
#include "pin_config.h"
#include <lvgl.h>

#include "Arduino_GFX_Library.h"
#include "lv_conf.h"
#include <demos/lv_demos.h>
#include "SensorQMI8658.hpp"

uint32_t screenWidth;
uint32_t screenHeight;
uint32_t bufSize;
lv_display_t *disp;
lv_color_t *disp_draw_buf;

SensorQMI8658 qmi;

IMUdata acc;
IMUdata gyr;

lv_obj_t *label;                  // Global label object
lv_obj_t *chart;                  // Global chart object
lv_chart_series_t *acc_series_x;  // Acceleration X series
lv_chart_series_t *acc_series_y;  // Acceleration Y series
lv_chart_series_t *acc_series_z;  // Acceleration Z series

#define DIRECT_RENDER_MODE
// #define RGB_PANEL

Arduino_XCA9554SWSPI *exio;

Arduino_DataBus *bus = new Arduino_XCA9554SWSPI(
    7 /* RST */, 0 /* CS */, 2 /* SCK */, 1 /* MOSI */,
    &Wire, TCA9554_I2C
);

Arduino_ESP32RGBPanel *rgbpanel = new Arduino_ESP32RGBPanel(
    17 /* DE */, 3 /* VSYNC */, 46 /* HSYNC */, 9 /* PCLK */,
    10 /* B0 */, 11 /* B1 */, 12 /* B2 */, 13 /* B3 */, 14 /* B4 */,
    21 /* G0 */, 8 /* G1 */, 18 /* G2 */, 45 /* G3 */, 38 /* G4 */, 39 /* G5 */,
    40 /* R0 */, 41 /* R1 */, 42 /* R2 */, 2 /* R3 */, 1 /* R4 */,
    1 /* hsync_polarity */, 10 /* hsync_front_porch */, 8 /* hsync_pulse_width */, 50 /* hsync_back_porch */,
    1 /* vsync_polarity */, 10 /* vsync_front_porch */, 8 /* vsync_pulse_width */, 20 /* vsync_back_porch */,
    0 /* pclk_active_neg */, GFX_NOT_DEFINED /* prefer_speed */, false /* useBigEndian */,
    0 /* de_idle_high */, 0 /* pclk_idle_high */, 10*480 /* bounce_buffer_size_px */
);

Arduino_RGB_Display *gfx = new Arduino_RGB_Display(
    480 /* width */, 480 /* height */, rgbpanel, 0 /* rotation */, true /* auto_flush */,
    bus, GFX_NOT_DEFINED /* RST */, st7701_type1_init_operations, sizeof(st7701_type1_init_operations)
);

#if LV_USE_LOG != 0
void my_print(lv_log_level_t level, const char *buf) {
  LV_UNUSED(level);
  Serial.println(buf);
  Serial.flush();
}
#endif

uint32_t millis_cb(void) {
  return millis();
}

/* LVGL calls it when a rendered image needs to copied to the display*/
void my_disp_flush(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map) {
#ifndef DIRECT_RENDER_MODE
  uint32_t w = lv_area_get_width(area);
  uint32_t h = lv_area_get_height(area);

  gfx->draw16bitRGBBitmap(area->x1, area->y1, (uint16_t *)px_map, w, h);
#endif  // #ifndef DIRECT_RENDER_MODE

  /*Call it to tell LVGL you are ready*/
  lv_disp_flush_ready(disp);
}

void rounder_event_cb(lv_event_t *e) {
  lv_area_t *area = (lv_area_t *)lv_event_get_param(e);
  uint16_t x1 = area->x1;
  uint16_t x2 = area->x2;

  uint16_t y1 = area->y1;
  uint16_t y2 = area->y2;

  // round the start of coordinate down to the nearest 2M number
  area->x1 = (x1 >> 1) << 1;
  area->y1 = (y1 >> 1) << 1;
  // round the end of coordinate up to the nearest 2N+1 number
  area->x2 = ((x2 >> 1) << 1) + 1;
  area->y2 = ((y2 >> 1) << 1) + 1;
}

void setup() {
#ifdef DEV_DEVICE_INIT
  DEV_DEVICE_INIT();
#endif

  Serial.begin(115200);
  // Serial.setDebugOutput(true);
  // while(!Serial);
  Serial.println("Arduino_GFX LVGL_Arduino_v9 example ");
  String LVGL_Arduino = String('V') + lv_version_major() + "." + lv_version_minor() + "." + lv_version_patch();
  Serial.println(LVGL_Arduino);

    Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL);
    
    Wire.beginTransmission(TCA9554_I2C);
    if (Wire.endTransmission() != 0) {
        Serial.println("[HW] CRITICAL: TCA9554 Not Found!");
        // return false;
    }

    // Create Expander Object
    exio = (Arduino_XCA9554SWSPI*)bus;
    
    // Start the expander explicitly (good practice)
    bus->begin();

    // Enable Backlight
    exio->pinMode(GFX_BL, OUTPUT);
    exio->digitalWrite(GFX_BL, HIGH);

    exio->pinMode(5, OUTPUT);
    exio->pinMode(6, OUTPUT);
    exio->digitalWrite(6, LOW);
    delay(200);
    exio->digitalWrite(5, LOW);
    delay(200);
    exio->digitalWrite(5, HIGH);
    delay(200);

    Serial.println("[HW] Initialization Complete.");


  // Init Display
  if (!gfx->begin()) {
    Serial.println("gfx->begin() failed!");
  }
  gfx->fillScreen(RGB565_BLACK);

  if (!qmi.begin(Wire, QMI8658_L_SLAVE_ADDRESS, 47, 48)) {
    Serial.println("Failed to find QMI8658 - check your wiring!");
    while (1) {
      delay(1000);
    }
  }

  qmi.configAccelerometer(SensorQMI8658::ACC_RANGE_4G, SensorQMI8658::ACC_ODR_1000Hz, SensorQMI8658::LPF_MODE_0);
  qmi.enableAccelerometer();

  lv_init();

  /*Set a tick source so that LVGL will know how much time elapsed. */
  lv_tick_set_cb(millis_cb);

  /* register print function for debugging */
#if LV_USE_LOG != 0
  lv_log_register_print_cb(my_print);
#endif

  screenWidth = gfx->width();
  screenHeight = gfx->height();

#ifdef DIRECT_RENDER_MODE
  bufSize = screenWidth * screenHeight;
#else
  bufSize = screenWidth * 50;
#endif

#ifdef ESP32
#if defined(DIRECT_RENDER_MODE) && (defined(CANVAS) || defined(RGB_PANEL) || defined(DSI_PANEL))
  disp_draw_buf = (lv_color_t *)gfx->getFramebuffer();
#else   // !(defined(DIRECT_RENDER_MODE) && (defined(CANVAS) || defined(RGB_PANEL) || defined(DSI_PANEL)))
  disp_draw_buf = (lv_color_t *)heap_caps_malloc(bufSize * 2, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
  if (!disp_draw_buf) {
    // remove MALLOC_CAP_INTERNAL flag try again
    disp_draw_buf = (lv_color_t *)heap_caps_malloc(bufSize * 2, MALLOC_CAP_8BIT);
  }
#endif  // !(defined(DIRECT_RENDER_MODE) && (defined(CANVAS) || defined(RGB_PANEL) || defined(DSI_PANEL)))
#else   // !ESP32
  Serial.println("LVGL disp_draw_buf heap_caps_malloc failed! malloc again...");
  disp_draw_buf = (lv_color_t *)malloc(bufSize * 2);
#endif  // !ESP32
  if (!disp_draw_buf) {
    Serial.println("LVGL disp_draw_buf allocate failed!");
  } else {
    disp = lv_display_create(screenWidth, screenHeight);
    lv_display_set_flush_cb(disp, my_disp_flush);
#ifdef DIRECT_RENDER_MODE
    lv_display_set_buffers(disp, disp_draw_buf, NULL, bufSize * 2, LV_DISPLAY_RENDER_MODE_DIRECT);
#else
    lv_display_set_buffers(disp, disp_draw_buf, NULL, bufSize * 2, LV_DISPLAY_RENDER_MODE_PARTIAL);
#endif

    lv_indev_t *indev = lv_indev_create();
    lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
    lv_display_add_event_cb(disp, rounder_event_cb, LV_EVENT_INVALIDATE_AREA, NULL);

    // lv_obj_t *label = lv_label_create(lv_scr_act());
    // lv_label_set_text(label, "Hello Arduino, I'm LVGL!(V" GFX_STR(LVGL_VERSION_MAJOR) "." GFX_STR(LVGL_VERSION_MINOR) "." GFX_STR(LVGL_VERSION_PATCH) ")");
    // lv_obj_align(label, LV_ALIGN_CENTER, 0, 0);

    chart = lv_chart_create(lv_scr_act());
    lv_obj_set_size(chart, 400, 300);
    lv_obj_align(chart, LV_ALIGN_CENTER, 0, 0);
    lv_chart_set_type(chart, LV_CHART_TYPE_LINE);              /* Set the type to line */
    lv_chart_set_range(chart, LV_CHART_AXIS_PRIMARY_Y, -3, 3); /* Set the range of y axis */
    lv_chart_set_point_count(chart, 30);                       /* Set the number of data points */
    acc_series_x = lv_chart_add_series(chart, lv_palette_main(LV_PALETTE_RED), LV_CHART_AXIS_PRIMARY_Y);
    acc_series_y = lv_chart_add_series(chart, lv_palette_main(LV_PALETTE_GREEN), LV_CHART_AXIS_PRIMARY_Y);
    acc_series_z = lv_chart_add_series(chart, lv_palette_main(LV_PALETTE_BLUE), LV_CHART_AXIS_PRIMARY_Y);

    // lv_demo_widgets();
    // lv_demo_benchmark();
    // lv_demo_keypad_encoder();
    // lv_demo_music();
    // lv_demo_stress();
  }

  Serial.println("Setup done");
}

void loop() {
  lv_task_handler(); /* let the GUI do its work */

#ifdef DIRECT_RENDER_MODE
#if defined(CANVAS) || defined(RGB_PANEL) || defined(DSI_PANEL)
  gfx->flush();
#else   // !(defined(CANVAS) || defined(RGB_PANEL) || defined(DSI_PANEL))
  gfx->draw16bitRGBBitmap(0, 0, (uint16_t *)disp_draw_buf, screenWidth, screenHeight);
#endif  // !(defined(CANVAS) || defined(RGB_PANEL) || defined(DSI_PANEL))
#else   // !DIRECT_RENDER_MODE
#ifdef CANVAS
  gfx->flush();
#endif
#endif  // !DIRECT_RENDER_MODE

  delay(5);

  if (qmi.getDataReady()) {
    if (qmi.getAccelerometer(acc.x, acc.y, acc.z)) {
      Serial.print("{ACCEL: ");
      Serial.print(acc.x);
      Serial.print(",");
      Serial.print(acc.y);
      Serial.print(",");
      Serial.print(acc.z);
      Serial.println("}");

      // Update chart with new accelerometer data
      lv_chart_set_next_value(chart, acc_series_x, acc.x);
      lv_chart_set_next_value(chart, acc_series_y, acc.y);
      lv_chart_set_next_value(chart, acc_series_z, acc.z);
    }

    if (qmi.getGyroscope(gyr.x, gyr.y, gyr.z)) {
      Serial.print("{GYRO: ");
      Serial.print(gyr.x);
      Serial.print(",");
      Serial.print(gyr.y);
      Serial.print(",");
      Serial.print(gyr.z);
      Serial.println("}");
    }
  }
}
