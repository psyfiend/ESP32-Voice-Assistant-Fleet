#include "BatteryProvider.h"
#include "BatteryEntities.h"
#include "bsp_loader.h"
#include "esp_adc/adc_cali_scheme.h"
#include <esp_system.h>   // esp_reset_reason
#include <math.h>
#if SOC_USB_SERIAL_JTAG_SUPPORTED && __has_include("driver/usb_serial_jtag.h")
  #include "driver/usb_serial_jtag.h"
  #define BATT_HAS_USJ 1
#endif

// ---------------------------------------------------------------------------
// The provider half. Same split as SystemProvider: WHAT the entities are is
// BatteryEntities.h; this file only reads the pin.
// ---------------------------------------------------------------------------

// Samples averaged per poll. Guition's adc_test takes 500 in a tight loop; a
// oneshot read is a few microseconds, so 64 costs well under a millisecond on
// the loop task and is plenty against the ADC's own noise.
static constexpr int BATT_SAMPLES = 64;

// Below this there is no usable cell, so nothing is written and the cards grey
// out rather than claiming an empty battery - the SystemProvider rule about
// confident lies. 3.2 V, not lower: with no cell fitted a charger's BAT pin
// floats at 2.9-3.1 V (measured on CYD_P4_4880, IP5306), and a board running
// on its battery shuts down near 3.0 V anyway - so a running board that reads
// under 3.2 V is on USB with no cell, or a dead one.
static constexpr int BATT_MIN_PLAUSIBLE_MV = 3200;

// A change of this much between two consecutive 5-s readings is a change of
// power source, not drift. The 7B's charger arriving measured ~+100 mV; a
// backlight or radio burst moves the reading by tens at most.
static constexpr int BATT_STEP_MV = 60;

// Trend thresholds, mV per minute. A cell charging near the top of its curve
// still rises a few mV a minute; one discharging under a display load falls.
static constexpr float BATT_TREND_MV_MIN = 1.5f;
// "Full": external power, at or above this, and flatter than FLAT_MV_MIN.
static constexpr int   BATT_FULL_MV      = 4150;
static constexpr float BATT_FLAT_MV_MIN  = 1.0f;

bool BatteryProvider::begin(EntityRegistry *reg) {
    if (_begun || !reg) return _begun;
    // Two conditions, deliberately separate: the BOARD can take a battery (the
    // BSP's divider), and this UNIT has one fitted (-D HAS_BATTERY in its
    // environment - owner, 2026-09-30: not every board will carry a cell).
#ifndef HAS_BATTERY
    return false;
#endif
    if (bsp_hw.BAT_DIV_X1000 == 0) return false;   // this board has no battery ADC

    adc_unit_t unit;
    if (adc_oneshot_io_to_channel(bsp_hw.BAT_ADC, &unit, &_chan) != ESP_OK) {
        Serial.printf("[Battery] GPIO%u is not an ADC pin - battery off.\n",
                      (unsigned)bsp_hw.BAT_ADC);
        return false;
    }

    adc_oneshot_unit_init_cfg_t ucfg = {};
    ucfg.unit_id  = unit;
    ucfg.ulp_mode = ADC_ULP_MODE_DISABLE;
    if (adc_oneshot_new_unit(&ucfg, &_unit) != ESP_OK) {
        // Most likely: the unit is already claimed by someone else's driver.
        Serial.printf("[Battery] ADC%d unavailable - battery off.\n", (int)unit + 1);
        return false;
    }

    adc_oneshot_chan_cfg_t ccfg = {};
    ccfg.atten    = ADC_ATTEN_DB_12;         // full ~0-3.1 V range at the pin
    ccfg.bitwidth = ADC_BITWIDTH_DEFAULT;
    if (adc_oneshot_config_channel(_unit, _chan, &ccfg) != ESP_OK) {
        adc_oneshot_del_unit(_unit);
        _unit = nullptr;
        Serial.println("[Battery] channel config failed - battery off.");
        return false;
    }

    // eFuse calibration. Without it the raw count is converted by the nominal
    // line, which is tens of mV out - tolerable for a percentage, and said.
#if ADC_CALI_SCHEME_CURVE_FITTING_SUPPORTED
    adc_cali_curve_fitting_config_t cal = {};
    cal.unit_id  = unit;
    cal.chan     = _chan;
    cal.atten    = ADC_ATTEN_DB_12;
    cal.bitwidth = ADC_BITWIDTH_DEFAULT;
    if (adc_cali_create_scheme_curve_fitting(&cal, &_cali) != ESP_OK) _cali = nullptr;
#elif ADC_CALI_SCHEME_LINE_FITTING_SUPPORTED
    adc_cali_line_fitting_config_t cal = {};
    cal.unit_id  = unit;
    cal.atten    = ADC_ATTEN_DB_12;
    cal.bitwidth = ADC_BITWIDTH_DEFAULT;
    if (adc_cali_create_scheme_line_fitting(&cal, &_cali) != ESP_OK) _cali = nullptr;
#endif

    for (uint8_t i = 0; i < BATTERY_ENTITY_COUNT; i++) {
        if (!reg->add(BATTERY_ENTITIES[i])) {
            Serial.printf("[Battery] FAILED to register \"%s\"\n", BATTERY_ENTITIES[i].id);
        }
    }

    _reg          = reg;
    _begun        = true;
    _lastPollMs   = 0;
    _unitId       = (int)unit;
    _state        = PowerState::PWR_UNKNOWN;
    _bootBrownout = (esp_reset_reason() == ESP_RST_BROWNOUT);
    Serial.printf("[Battery] GPIO%u = ADC%d ch%d, divider x%u.%03u, %s\n",
                  (unsigned)bsp_hw.BAT_ADC, (int)unit + 1, (int)_chan,
                  (unsigned)(bsp_hw.BAT_DIV_X1000 / 1000),
                  (unsigned)(bsp_hw.BAT_DIV_X1000 % 1000),
                  _cali ? "calibrated" : "UNCALIBRATED (nominal conversion)");
    return true;
}

void BatteryProvider::loop(uint32_t nowMs) {
    if (!_begun) return;
    if (_lastPollMs != 0 && (nowMs - _lastPollMs) < _intervalMs) return;
    _lastPollMs = nowMs;

    int32_t sum = 0;
    int     n   = 0;
    for (int i = 0; i < BATT_SAMPLES; i++) {
        int raw;
        if (adc_oneshot_read(_unit, _chan, &raw) == ESP_OK) { sum += raw; n++; }
    }
    if (n == 0) return;
    const int raw = (int)(sum / n);

    int pinMv;
    if (!_cali || adc_cali_raw_to_voltage(_cali, raw, &pinMv) != ESP_OK) {
        pinMv = raw * 3100 / 4095;   // nominal: 12 dB attenuation, 12 bits
    }
    const int battMv = (int)((int32_t)pinMv * bsp_hw.BAT_DIV_X1000 / 1000);
    _lastRaw = raw;
    _lastPinMv = pinMv;

    inferState(nowMs, battMv);   // uses the previous reading, so before _lastMv moves
    _lastMv = battMv;

    // Log once a minute, and at once on a change of power source: the
    // calibration evidence, pin and battery side by side, and the state.
    if (_lastLogMs == 0 || (nowMs - _lastLogMs) >= 60000 || _stepAtMs == nowMs) {
        _lastLogMs = nowMs;
        Serial.printf("[Battery] raw %d  pin %d mV  batt %d mV  %d%%  %s (%s)\n",
                      raw, pinMv, battMv, percentFromMv(_filtMv < 0 ? battMv : (int)_filtMv),
                      stateName(), _why);
    }

    if (battMv < BATT_MIN_PLAUSIBLE_MV) { _filtMv = -1; return; }

    // Light smoothing (1/4 new) so the percentage does not flicker a step
    // either side of a boundary. The first reading seeds it.
    _filtMv = (_filtMv < 0) ? battMv : (_filtMv * 3 + battMv) / 4;

    _reg->setValue(BATT_ENT_MV,  EntityValue::makeInt(_filtMv), nowMs);
    _reg->setValue(BATT_ENT_PCT, EntityValue::makeInt(percentFromMv((int)_filtMv)), nowMs);
}

const char *BatteryProvider::stateName() const {
    switch (_state) {
        case PowerState::PWR_NO_ADC:     return "no battery input";
        case PowerState::PWR_NO_BATTERY: return "no battery";
        case PowerState::PWR_UNKNOWN:    return "battery, source unknown";
        case PowerState::PWR_ON_BATTERY: return "running on battery";
        case PowerState::PWR_CHARGING:   return "charging";
        case PowerState::PWR_FULL:       return "battery fully charged";
    }
    return "?";
}

void BatteryProvider::inferState(uint32_t nowMs, int battMv) {
    // No usable cell: nothing else matters.
    if (battMv < BATT_MIN_PLAUSIBLE_MV) {
        _state = PowerState::PWR_NO_BATTERY;
        _why   = "reading under 3.2 V";
        _histCount = _histHead = 0;
        _trendValid = false;
        return;
    }

    // 1. A step from the previous reading is a change of power source. It
    //    also invalidates the trend, which would otherwise straddle the step.
    if (_lastMv >= BATT_MIN_PLAUSIBLE_MV) {
        const int d = battMv - _lastMv;
        if (d >= BATT_STEP_MV || d <= -BATT_STEP_MV) {
            _stepMv   = d;
            _stepAtMs = nowMs;
            _extPower = d > 0 ? 1 : 0;
            _histCount = _histHead = 0;
            _trendValid = false;
        }
    }

    // 4. The trend: mean of the oldest six readings in the window against the
    //    newest six, over the time between them.
    _hist[_histHead] = (int16_t)battMv;
    _histHead = (_histHead + 1) % TREND_N;
    if (_histCount < TREND_N) _histCount++;
    if (_histCount >= 36) {   // 3 minutes
        const int oldest = (_histHead + TREND_N - _histCount) % TREND_N;
        int32_t a = 0, b = 0;
        for (int i = 0; i < 6; i++) {
            a += _hist[(oldest + i) % TREND_N];
            b += _hist[(_histHead + TREND_N - 1 - i) % TREND_N];
        }
        const float minutes = (float)(_histCount - 6) * (_intervalMs / 1000.0f) / 60.0f;
        _trend = ((b - a) / 6.0f) / minutes;
        _trendValid = true;
    }

    // 2. A PC on the chip's own USB port proves external power.
#ifdef BATT_HAS_USJ
    _usbHost = usb_serial_jtag_is_connected();
#endif

    // Decide, strongest evidence first.
    int ext = _extPower;
    const char *why = _extPower == 1 ? "a +step: a charger arrived"
                    : _extPower == 0 ? "a -step: the charger left" : "";
    if (ext != 1 && _usbHost) {
        ext = 1; why = "a PC is on the USB port";
    }
    if (ext < 0 && _bootBrownout) {
        ext = 0; why = "restarted by brownout: power was lost";
    }
    if (ext < 0 && _trendValid) {
        if (_trend >=  BATT_TREND_MV_MIN) { ext = 1; why = "voltage rising"; }
        if (_trend <= -BATT_TREND_MV_MIN) { ext = 0; why = "voltage falling"; }
    }

    if (ext == 0) {
        _state = PowerState::PWR_ON_BATTERY;
    } else if (ext == 1) {
        const bool flat = _trendValid && fabsf(_trend) < BATT_FLAT_MV_MIN;
        if (battMv >= BATT_FULL_MV && flat) {
            _state = PowerState::PWR_FULL;
            // A charger with no cell holds its BAT pin here too (the ETA6098
            // at ~4.17 V): without a step ever seen, the two look the same.
            why = _stepAtMs ? "at the charge voltage and flat"
                            : "at the charge voltage and flat - or no cell: some chargers hold this with none";
        } else {
            _state = PowerState::PWR_CHARGING;
        }
    } else if (battMv >= BATT_FULL_MV && _trendValid && fabsf(_trend) < BATT_FLAT_MV_MIN) {
        // No evidence of the source, but pinned at the charge voltage: only
        // external power holds a cell there.
        _state = PowerState::PWR_FULL;
        why = "at the charge voltage and flat - or no cell: some chargers hold this with none";
    } else {
        _state = PowerState::PWR_UNKNOWN;
        why = _trendValid ? "voltage steady, no step seen" : "watching - needs 3 minutes of readings";
    }
    _why = why;
}

int BatteryProvider::percentFromMv(int mv) {
    // A typical 1S Li-ion/LiPo open-circuit curve. Under load the voltage sags
    // and under charge it rises, so this is a gauge, not a fuel meter.
    static const struct { int16_t mv; int8_t pct; } curve[] = {
        {4200, 100}, {4100, 90}, {4000, 80}, {3920, 70}, {3850, 60},
        {3800,  50}, {3750, 40}, {3700, 30}, {3650, 20}, {3550, 10},
        {3400,   5}, {3300,  0},
    };
    const int N = sizeof(curve) / sizeof(curve[0]);
    if (mv >= curve[0].mv)     return 100;
    if (mv <= curve[N - 1].mv) return 0;
    for (int i = 1; i < N; i++) {
        if (mv >= curve[i].mv) {
            const int span = curve[i - 1].mv - curve[i].mv;
            return curve[i].pct + (mv - curve[i].mv) * (curve[i - 1].pct - curve[i].pct) / span;
        }
    }
    return 0;
}
