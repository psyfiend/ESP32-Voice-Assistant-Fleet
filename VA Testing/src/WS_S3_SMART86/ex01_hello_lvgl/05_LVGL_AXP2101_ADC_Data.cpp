#include <Wire.h>
#include <Arduino.h>
#include "pin_config.h"
#include <lvgl.h>

#include "Arduino_GFX_Library.h"
#include "lv_conf.h"
#include <demos/lv_demos.h>

#include "XPowersLib.h"

uint32_t screenWidth;
uint32_t screenHeight;
uint32_t bufSize;
lv_display_t *disp;
lv_color_t *disp_draw_buf;

XPowersPMU power;

bool adc_switch = false;
lv_obj_t *info_label;

#define DIRECT_RENDER_MODE

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
    1 /* vsync_polarity */, 10 /* vsync_front_porch */, 8 /* vsync_pulse_width */, 20 /* vsync_back_porch */
);

Arduino_RGB_Display *gfx = new Arduino_RGB_Display(
    480 /* width */, 480 /* height */, rgbpanel, 0 /* rotation */, true /* auto_flush */,
    bus, GFX_NOT_DEFINED /* RST */, st7701_type1_init_operations, sizeof(st7701_type1_init_operations)
);



void adcOn() {
  // power.enableTemperatureMeasure();
  // Enable internal ADC detection
  power.enableBattDetection();
  power.enableVbusVoltageMeasure();
  power.enableBattVoltageMeasure();
  power.enableSystemVoltageMeasure();
}

void adcOff() {
  power.disableTemperatureMeasure();
  // Enable internal ADC detection
  power.disableBattDetection();
  power.disableVbusVoltageMeasure();
  power.disableBattVoltageMeasure();
  power.disableSystemVoltageMeasure();
}

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

  Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);
  
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

// Init Display
if (!gfx->begin()) {
  Serial.println("gfx->begin() failed!");
}
gfx->fillScreen(RGB565_BLACK);


Serial.printf("GetID: 0x%x\n", power.getChipID());

// Set the minimum common working voltage of the PMU VBUS input,
// below this value will turn off the PMU
power.setVbusVoltageLimit(XPOWERS_AXP2101_VBUS_VOL_LIM_4V36);

// Set the maximum current that the PMU VBUS input can provide to the system,
// higher than this will turn off the PMU
power.setVbusCurrentLimit(XPOWERS_AXP2101_VBUS_CUR_LIM_1000MA);

// Get the VSYS shutdown voltage
uint16_t vol = power.getSysPowerDownVoltage();
Serial.printf("-> getSysPowerDownVoltage: %u\n", vol);

// Set the VSYS shutdown voltage to 2.6V
power.setSysPowerDownVoltage(2600);
  
// It is necessary to disable the detection function of the TS pin on the board
// without the battery temperature detection function, otherwise it will cause abnormal charging
power.disableTSPinMeasure();

  /*
    The default setting is CHGLED is automatically controlled by the PMU.
  - XPOWERS_CHG_LED_OFF,
  - XPOWERS_CHG_LED_BLINK_1HZ,
  - XPOWERS_CHG_LED_BLINK_4HZ,
  - XPOWERS_CHG_LED_ON,
  - XPOWERS_CHG_LED_CTRL_CHG,
  * */
power.setChargingLedMode(XPOWERS_CHG_LED_CTRL_CHG);


// Disable all interrupt functions
power.disableIRQ(XPOWERS_AXP2101_ALL_IRQ);

// Clear all interrupt flags
power.clearIrqStatus();
  
// Enable the required interrupt function
  power.enableIRQ(
      XPOWERS_AXP2101_BAT_INSERT_IRQ    | XPOWERS_AXP2101_BAT_REMOVE_IRQ      |   //BATTERY
      XPOWERS_AXP2101_VBUS_INSERT_IRQ   | XPOWERS_AXP2101_VBUS_REMOVE_IRQ     |   //VBUS
      XPOWERS_AXP2101_PKEY_SHORT_IRQ    | XPOWERS_AXP2101_PKEY_LONG_IRQ       |   //POWER KEY
      XPOWERS_AXP2101_BAT_CHG_DONE_IRQ  | XPOWERS_AXP2101_BAT_CHG_START_IRQ   |   //CHARGE
      XPOWERS_AXP2101_WARNING_LEVEL1_IRQ | XPOWERS_AXP2101_WARNING_LEVEL2_IRQ     //Low battery warning
  );

  // Set pre-charge current to 200mA
  power.setPrechargeCurr(XPOWERS_AXP2101_PRECHARGE_200MA);

  // Set charge termination current to 25mA
  power.setChargerTerminationCurr(XPOWERS_AXP2101_CHG_ITERM_25MA);

  // Set constant charge current to 1000mA
  if (!power.setChargerConstantCurr(XPOWERS_AXP2101_CHG_CUR_1000MA)) {
    Serial.println("-> setChargerConstantCurr failed!");
  }

  const uint16_t currTable[] = {
    0, 0, 0, 0, 100, 125, 150, 175, 200, 300, 400, 500, 600, 700, 800, 900, 1000
  };
  uint8_t val = power.getChargerConstantCurr();
  Serial.print("Setting Charge Target Current : ");
  Serial.println(currTable[val]);

  // Set charge cut-off voltage to 4.2V
  power.setChargeTargetVoltage(XPOWERS_AXP2101_CHG_VOL_4V2);

  const uint16_t tableVoltage[] = {
    0, 4000, 4100, 4200, 4350, 4400, 255
  };
  val = power.getChargeTargetVoltage();
  Serial.print("Setting Charge Target Voltage : ");
  Serial.println(tableVoltage[val]);

  // Set the power level to be lower than 15% and send an interrupt to the host
  power.setLowBatWarnThreshold(10);

  // Set the power level to be lower than 5% and turn off the power supply
  power.setLowBatShutdownThreshold(5);

  Serial.println();

  delay(5000);

  adcOn();

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

    info_label = lv_label_create(lv_scr_act());
    lv_label_set_text(info_label, "Initializing...");
    lv_obj_align(info_label, LV_ALIGN_CENTER, 0, 0);

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

  power.getIrqStatus();
  if (power.isPekeyShortPressIrq()) {
    Serial.println("Power Key Short Pressed!");
    // Toggle CHG LED
    power.setChargingLedMode(power.getChargingLedMode() != XPOWERS_CHG_LED_OFF ? XPOWERS_CHG_LED_OFF : XPOWERS_CHG_LED_ON);
  }
  if (power.isPekeyLongPressIrq()) {
    Serial.println("Power Key Long Pressed! Toggle backlight.");
    exio->digitalWrite(GFX_BL, !GFX_BL);
  }
  power.clearIrqStatus();

  String info = "";

  uint8_t charge_status = power.getChargerStatus();

  info += "power Temperature: " + String(power.getTemperature()) + "*C\n";
  info += "isCharging: " + String(power.isCharging() ? "YES" : "NO") + "\n";
  info += "isDischarge: " + String(power.isDischarge() ? "YES" : "NO") + "\n";
  info += "isStandby: " + String(power.isStandby() ? "YES" : "NO") + "\n";
  info += "isVbusIn: " + String(power.isVbusIn() ? "YES" : "NO") + "\n";
  info += "isVbusGood: " + String(power.isVbusGood() ? "YES" : "NO") + "\n";

  switch (charge_status) {
    case XPOWERS_AXP2101_CHG_TRI_STATE:
      info += "Charger Status: tri_charge\n";
      break;
    case XPOWERS_AXP2101_CHG_PRE_STATE:
      info += "Charger Status: pre_charge\n";
      break;
    case XPOWERS_AXP2101_CHG_CC_STATE:
      info += "Charger Status: constant charge\n";
      break;
    case XPOWERS_AXP2101_CHG_CV_STATE:
      info += "Charger Status: constant voltage\n";
      break;
    case XPOWERS_AXP2101_CHG_DONE_STATE:
      info += "Charger Status: charge done\n";
      break;
    case XPOWERS_AXP2101_CHG_STOP_STATE:
      info += "Charger Status: not charging\n";
      break;
  }

  info += "Battery Voltage: " + String(power.getBattVoltage()) + "mV\n";
  info += "Vbus Voltage: " + String(power.getVbusVoltage()) + "mV\n";
  info += "System Voltage: " + String(power.getSystemVoltage()) + "mV\n";

  if (power.isBatteryConnect()) {
    info += "Battery Percent: " + String(power.getBatteryPercent()) + "%\n";
  }
  if (power.isCharging()) {
    info += "Charge Current: " + String(power.getChargerConstantCurr()) + "mA\n";
  } else if (power.isDischarge()) {
    info += "Discharging!\n";
  }

  lv_label_set_text(info_label, info.c_str());
  lv_obj_set_style_text_font(info_label, &lv_font_montserrat_20, LV_PART_MAIN);
}
