#include <Wire.h>
#include <Arduino.h>
#include <lvgl.h>

#include "pin_config.h"
#include "Arduino_GFX_Library.h"
#include "GFX_Init/display_init.h"
#include "lv_conf.h"
#include "XPowersLib.h"

#include "ui_power_monitor.h"



//-----------------------------------------------------------------
// Create Display objects
//-----------------------------------------------------------------

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

// Create pin expander object
Arduino_XCA9554SWSPI *exio;

// Create XPowersPMU object
XPowersPMU power;

// Power Monitoring Variables
bool isVbusIn = false;
float vbusVoltage = 0;
float vsysVoltage = 0;
float battVoltage = 0;
float battPercentage = 0;
bool isCharging = false;
bool isDischarging = false;
const char* eventMessage = NULL;
const char* chargerStatus = NULL;

// Enable internal ADC detection
void adcOn() {
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


// LVGL Globals
uint32_t screenWidth;
uint32_t screenHeight;
uint32_t bufSize;
lv_display_t *disp;
lv_color_t *disp_draw_buf;
lv_obj_t *info_label;

// -------------------------------------------------------------------------
// LVGL Configuration
// -------------------------------------------------------------------------
#define SCREEN_WIDTH 480
#define SCREEN_HEIGHT 480

// Define a buffer size (1/10th of screen is a good balance of RAM vs Perf)
// 480 * 480 / 10 = 23,040 pixels * 2 bytes = ~46KB
#define LV_BUF_SIZE (SCREEN_WIDTH * SCREEN_HEIGHT)

// Buffer pointer
static uint16_t *lv_draw_buf;

// -------------------------------------------------------------------------
// LVGL
// -------------------------------------------------------------------------
void my_disp_flush(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map) {
    uint32_t w = (area->x2 - area->x1 + 1);
    uint32_t h = (area->y2 - area->y1 + 1);
    
    gfx->draw16bitRGBBitmap(area->x1, area->y1, (uint16_t *)px_map, w, h);

    lv_display_flush_ready(disp);
}

static uint32_t my_tick_get_cb(void) { 
    return millis(); 
}

void setup() {

//-----------------------------------------------------------------
// Serial and I2C Init
//-----------------------------------------------------------------

    Serial.begin(115200);
    // Serial.setDebugOutput(true);
    // while(!Serial);
    Serial.println("Arduino_GFX LVGL_Arduino_v9 example ");
    String LVGL_Arduino = String('V') + lv_version_major() + "." + lv_version_minor() + "." + lv_version_patch();
    Serial.println(LVGL_Arduino);

    Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL);
    Wire.beginTransmission(TCA9554_I2C);
    if (Wire.endTransmission() != 0) {
        Serial.println();
        Serial.println("[HW] CRITICAL: TCA9554 Not Found!");
        // return false;
    }

    // Enable Backlight
    pinMode(4, OUTPUT);
    digitalWrite(4, LOW);

    // Create Expander Object
    exio = (Arduino_XCA9554SWSPI*)bus;
    // Start the expander explicitly (good practice)
    bus->begin();

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

//-----------------------------------------------------------------
// AXP2101 Init
//-----------------------------------------------------------------

    bool result = power.begin(Wire, AXP2101_SLAVE_ADDRESS, PIN_I2C_SDA, PIN_I2C_SCL);

        if (result == false) {
            Serial.println();
            Serial.println("Could not start AXP2101!"); while (1)delay(50);
        }

    Serial.printf("getID:0x%x\n", power.getChipID());

    
//-------------------------------------------------------------
// Charge monitor configuration
//-------------------------------------------------------------  

    // Set the minimum common working voltage of the PMU VBUS input,
    // below this value will turn off the PMU
    power.setVbusVoltageLimit(XPOWERS_AXP2101_VBUS_VOL_LIM_4V36);

    // Set the maximum current of the PMU VBUS input,
    // higher than this value will turn off the PMU
    power.setVbusCurrentLimit(XPOWERS_AXP2101_VBUS_CUR_LIM_1500MA);

    // Get the VSYS shutdown voltage
    uint16_t vol = power.getSysPowerDownVoltage();
    Serial.printf("->  getSysPowerDownVoltage:%u\n", vol);

    // Set VSY off voltage as 2600mV , Adjustment range 2600mV ~ 3300mV
    power.setSysPowerDownVoltage(2600);

    // It is necessary to disable the detection function of the TS pin on the board
    // without the battery temperature detection function, otherwise it will cause abnormal charging
    power.disableTSPinMeasure();

    // power.enableTemperatureMeasure();

    /*
      The default setting is CHGLED is automatically controlled by the PMU.
    - XPOWERS_CHG_LED_OFF,
    - XPOWERS_CHG_LED_BLINK_1HZ,
    - XPOWERS_CHG_LED_BLINK_4HZ,
    - XPOWERS_CHG_LED_ON,
    - XPOWERS_CHG_LED_CTRL_CHG,
    * */

    power.setChargingLedMode(XPOWERS_CHG_LED_CTRL_CHG);

    // Disable all interrupts
    power.disableIRQ(XPOWERS_AXP2101_ALL_IRQ);
    // Clear all interrupt flags
    power.clearIrqStatus();
    // Enable the required interrupt function
    power.enableIRQ(
        XPOWERS_AXP2101_BAT_INSERT_IRQ    | XPOWERS_AXP2101_BAT_REMOVE_IRQ      |   //BATTERY
        XPOWERS_AXP2101_VBUS_INSERT_IRQ   | XPOWERS_AXP2101_VBUS_REMOVE_IRQ     |   //VBUS
        XPOWERS_AXP2101_PKEY_SHORT_IRQ    | XPOWERS_AXP2101_PKEY_LONG_IRQ       |   //POWER KEY
        XPOWERS_AXP2101_BAT_CHG_DONE_IRQ  | XPOWERS_AXP2101_BAT_CHG_START_IRQ   |    //CHARGE
        XPOWERS_AXP2101_WARNING_LEVEL1_IRQ | XPOWERS_AXP2101_WARNING_LEVEL2_IRQ     //Low battery warning
        // XPOWERS_AXP2101_PKEY_NEGATIVE_IRQ | XPOWERS_AXP2101_PKEY_POSITIVE_IRQ   |   //POWER KEY
    );

    // Set the precharge charging current
    power.setPrechargeCurr(XPOWERS_AXP2101_PRECHARGE_200MA);

    // Set stop charging termination current
    power.setChargerTerminationCurr(XPOWERS_AXP2101_CHG_ITERM_25MA);

    // Set constant current charge current limit
    if (!power.setChargerConstantCurr(XPOWERS_AXP2101_CHG_CUR_1000MA)) {
        Serial.println();
        Serial.println("Setting Charger Constant Current Failed!");
    }

    const uint16_t currTable[] = {
        0, 0, 0, 0, 100, 125, 150, 175, 200, 300, 400, 500, 600, 700, 800, 900, 1000
    };
    uint8_t val = power.getChargerConstantCurr();
    Serial.print("Setting Charge Target Current : ");
    Serial.println(currTable[val]);

    // Set charge cut-off voltage
    power.setChargeTargetVoltage(XPOWERS_AXP2101_CHG_VOL_4V2);

    const uint16_t tableVoltage[] = {
        0, 4000, 4100, 4200, 4350, 4400, 255
    };
    val = power.getChargeTargetVoltage();
    Serial.print("Setting Charge Target Voltage : ");
    Serial.println(tableVoltage[val]);

    // Set the power level to be lower than 15% and send an interrupt to the host`
    power.setLowBatWarnThreshold(10);

    // Set the power level to be lower than 5% and turn off the power supply
    power.setLowBatShutdownThreshold(5);
    Serial.println();

    // Activate measurement/control functions
    adcOn();
    Serial.println("ADC Enabled.");


//-----------------------------------------------------------------
// LVGL Init
//-----------------------------------------------------------------

    /* register print function for debugging */
    #if LV_USE_LOG != 0
        lv_log_register_print_cb(my_print);
    #endif

    // LVGL Setup
    lv_init();
    // Set a tick source so that LVGL will know how much time elapsed.
    lv_tick_set_cb(my_tick_get_cb);

    lv_display_t *disp = lv_display_create(SCREEN_WIDTH, SCREEN_HEIGHT);
    lv_display_set_flush_cb(disp, my_disp_flush);

    // Allocate display buffer(s)
    size_t buffer_size = (gfx->width() * gfx->height() / 10);
    lv_draw_buf = (uint16_t*)heap_caps_malloc(buffer_size * 2, MALLOC_CAP_SPIRAM);
    lv_display_set_buffers(disp, lv_draw_buf, NULL, buffer_size * 2, LV_DISPLAY_RENDER_MODE_PARTIAL);

    // Create the UI
    ui_init_power_monitor();

    Serial.println();
    Serial.println("Should be seeing the empty display now.");

}

uint32_t printTime = 0;

void loop()
{
    lv_timer_handler(); /* let the GUI do its work */
    delay(5);

    PowerStats stats = {0};

    stats.eventMessage = NULL;
    stats.chargerStatus = NULL;

    stats.isCharging = power.isCharging();
    stats.isDischarging = power.isDischarge();

    stats.isVbusIn = power.isVbusIn();
    stats.vsysVoltage = power.getSystemVoltage();
    stats.battVoltage = power.getBattVoltage();
    stats.vbusVoltage = power.getVbusVoltage();
    stats.battPercentage = power.getBatteryPercent();

    ui_update_power_stats(stats);

    
    if (millis() > printTime) {
        printTime = millis() + 2000;

        // Get Charger Status
        uint8_t charge_status = power.getChargerStatus();
        switch(charge_status) {
            case XPOWERS_AXP2101_CHG_TRI_STATE: stats.chargerStatus = "Tri-Charge"; break;
            case XPOWERS_AXP2101_CHG_PRE_STATE: stats.chargerStatus = "Pre-Charge"; break;
            case XPOWERS_AXP2101_CHG_CC_STATE:  stats.chargerStatus = "Constant Charge(CC)"; break;
            case XPOWERS_AXP2101_CHG_CV_STATE:  stats.chargerStatus = "Constant Voltage(CV)"; break;
            case XPOWERS_AXP2101_CHG_DONE_STATE:stats.chargerStatus = "Finished Charging"; break;
            case XPOWERS_AXP2101_CHG_STOP_STATE:stats.chargerStatus = "Not Charging"; break;
            default: stats.chargerStatus = "Idle"; break;
        }

        if (uint32_t status = power.getIrqStatus()) {
            Serial.print("STATUS => HEX:");
            Serial.print(status, HEX);
            Serial.print(" BIN:");
            Serial.println(status, BIN);

            // Check for PMU Interrupt Events
            if (power.isDropWarningLevel2Irq()) {stats.eventMessage = "Warning Level 2 Reached!";}
            if (power.isDropWarningLevel1Irq()) {stats.eventMessage = ">>>> Warning Level 1 Reached! <<<<";
                stats.eventMessage = "   *<<[[> SHUTTING DOWN <]]>*";
                power.shutdown();}
            if (power.isStateOfChargeLowIrq()) {stats.eventMessage = "Low charge state!";}
            if (power.isVbusInsertIrq()) {stats.eventMessage = "Vbus Inserted!";}
            if (power.isVbusRemoveIrq()) {stats.eventMessage = "Vbus Removed!";}
            if (power.isBatInsertIrq()) {stats.eventMessage = "Battery Inserted!";}
            if (power.isBatRemoveIrq()) {stats.eventMessage = "Battery Removed!";}
            if (power.isPekeyShortPressIrq()) {stats.eventMessage = "Power Key Short Pressed!";}
            if (power.isPekeyLongPressIrq()) {stats.eventMessage = "Power Key Long Pressed!";}
            if (power.isBatChargeDoneIrq()) {stats.eventMessage = "Battery Charge Done!";}
            if (power.isBatChargeStartIrq()) {stats.eventMessage = "Battery Charge Started!";}
            if (power.isBatOverVoltageIrq()) {stats.eventMessage = "Battery Over Voltage!";}

            // Clear PMU Interrupt Status Register
            power.clearIrqStatus();
            Serial.println("IRQ Status Cleared.");
        }

    }
    
}