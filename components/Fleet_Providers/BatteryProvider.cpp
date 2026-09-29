#include "BatteryProvider.h"
#include "BatteryEntities.h"
#include "bsp_loader.h"
#include "esp_adc/adc_cali_scheme.h"

// ---------------------------------------------------------------------------
// The provider half. Same split as SystemProvider: WHAT the entities are is
// BatteryEntities.h; this file only reads the pin.
// ---------------------------------------------------------------------------

// Samples averaged per poll. Guition's adc_test takes 500 in a tight loop; a
// oneshot read is a few microseconds, so 64 costs well under a millisecond on
// the loop task and is plenty against the ADC's own noise.
static constexpr int BATT_SAMPLES = 64;

// Below this the pin is not looking at a cell (none fitted, or a divider with
// nothing behind it). Nothing is written: the cards grey out rather than
// claiming an empty battery - the SystemProvider rule about confident lies.
static constexpr int BATT_MIN_PLAUSIBLE_MV = 2500;

bool BatteryProvider::begin(EntityRegistry *reg) {
    if (_begun || !reg) return _begun;
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

    _reg        = reg;
    _begun      = true;
    _lastPollMs = 0;
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

    // Log once a minute: the calibration evidence, pin and battery side by side.
    if (_lastLogMs == 0 || (nowMs - _lastLogMs) >= 60000) {
        _lastLogMs = nowMs;
        Serial.printf("[Battery] raw %d  pin %d mV  batt %d mV  %d%%\n",
                      raw, pinMv, battMv, percentFromMv(_filtMv < 0 ? battMv : (int)_filtMv));
    }

    if (battMv < BATT_MIN_PLAUSIBLE_MV) { _filtMv = -1; return; }

    // Light smoothing (1/4 new) so the percentage does not flicker a step
    // either side of a boundary. The first reading seeds it.
    _filtMv = (_filtMv < 0) ? battMv : (_filtMv * 3 + battMv) / 4;

    _reg->setValue(BATT_ENT_MV,  EntityValue::makeInt(_filtMv), nowMs);
    _reg->setValue(BATT_ENT_PCT, EntityValue::makeInt(percentFromMv((int)_filtMv)), nowMs);
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
