#pragma once
#ifndef BATTERY_PROVIDER_H
#define BATTERY_PROVIDER_H

#include <Arduino.h>
#include "EntityRegistry.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"

// ---------------------------------------------------------------------------
// BatteryProvider - a single-cell Li-ion read through a resistor divider on
// one ADC pin, as battery percentage and millivolts.
//
// Quick prototype (2026-09-29). ESP-IDF's oneshot ADC driver plus its eFuse
// calibration, not Arduino's analogRead - the fleet's IDF-first rule. The pin
// and the divider come from the BSP (BoardHardware.BAT_ADC, BAT_DIV_X1000);
// a board that leaves BAT_DIV_X1000 at 0 has no battery ADC and this stays off.
//
// What it cannot know: whether a battery is fitted, and whether it is
// charging. A charger holds its BAT pin near 4.2 V with no cell attached, and
// a charging cell reads high. Both show as a full battery. The mV entity is
// there so a meter can settle what the number means on each board.
//
// Boards with a PMU instead of a divider (WS_S3_4B's AXP2101, the CYD S3s'
// IP5306) need a different source behind the same two entities. Not built.
// ---------------------------------------------------------------------------

class BatteryProvider {
public:
    // Registers the entities and opens the ADC. Returns false, having
    // registered nothing, on a board without a battery ADC or if the ADC
    // cannot be opened - a card bound to the battery is then simply skipped.
    bool begin(EntityRegistry *reg);

    void loop(uint32_t nowMs);

    // Li-ion open-circuit curve, piecewise linear. Public so /bench or a test
    // can check it; nothing else should need it.
    static int percentFromMv(int mv);

private:
    EntityRegistry *_reg = nullptr;
    adc_oneshot_unit_handle_t _unit = nullptr;
    adc_cali_handle_t         _cali = nullptr;
    adc_channel_t             _chan = ADC_CHANNEL_0;

    uint32_t _intervalMs = 5000;
    uint32_t _lastPollMs = 0;
    uint32_t _lastLogMs  = 0;
    int32_t  _filtMv     = -1;   // smoothed battery mV, -1 = no reading yet
    bool     _begun      = false;
};

#endif // BATTERY_PROVIDER_H
