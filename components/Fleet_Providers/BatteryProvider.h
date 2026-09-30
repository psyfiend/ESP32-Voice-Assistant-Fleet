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
// What it cannot know directly: whether a battery is fitted, and whether it is
// charging. With no cell the IP5306's BAT pin floats at 2.9-3.1 V (measured,
// CYD_P4_4880), under the 3.2 V plausibility floor, so the cards grey out; the
// ETA6098 boards are not measured yet. A charging cell reads at the charge
// voltage (~4.16-4.2 V), so it shows as full. The mV entity is there so a
// meter can settle what the number means on each board.
//
// Boards with a PMU instead of a divider (WS_S3_4B's AXP2101, the CYD S3s'
// IP5306) need a different source behind the same two entities. Not built.
//
// POWER STATE, INFERRED (#72). None of the divider boards routes a charge
// status to a GPIO, so the state is worked out from evidence, strongest first:
//   1. a STEP between two readings: a charger arriving lifts the cell ~100 mV
//      at once (measured on the 7B), one leaving drops it
//   2. a PC on the chip's own USB port (USB-Serial-JTAG) - external power
//   3. a restart by BROWNOUT - on both P4 families losing USB power resets
//      the board, which then runs on the cell
//   4. the TREND over the last few minutes - rising = charging
// "Full" is external power with the reading pinned near the charge voltage
// and flat. The state says which piece of evidence it rests on, because every
// one of them is an inference; the System Doctor prints it.
// ---------------------------------------------------------------------------

// Compound names on purpose: Arduino defines bare ALL-CAPS macros (CLAUDE.md).
enum class PowerState : uint8_t {
    PWR_NO_ADC,       // this board has no battery input
    PWR_NO_BATTERY,   // reading under the plausibility floor
    PWR_UNKNOWN,      // a cell, but no evidence yet of where power comes from
    PWR_ON_BATTERY,
    PWR_CHARGING,
    PWR_FULL,
};

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

    // --- For the System Doctor. Plain reads; call from the loop task. -------
    bool        active() const        { return _begun; }
    int         unitNumber() const    { return _unitId + 1; }   // ADC1 / ADC2
    int         channel() const       { return (int)_chan; }
    bool        calibrated() const    { return _cali != nullptr; }
    int         lastRaw() const       { return _lastRaw; }
    int         lastPinMv() const     { return _lastPinMv; }
    int         lastMv() const        { return _lastMv; }        // unsmoothed, -1 = none
    int         smoothedMv() const    { return (int)_filtMv; }  // -1 = none/implausible
    PowerState  state() const         { return _state; }
    const char *stateName() const;
    const char *evidence() const      { return _why; }          // what the state rests on
    float       trendMvPerMin() const { return _trend; }
    bool        trendValid() const    { return _trendValid; }
    int         lastStepMv() const    { return _stepMv; }        // 0 = none seen
    uint32_t    lastStepAgeS(uint32_t nowMs) const {
        return _stepAtMs ? (nowMs - _stepAtMs) / 1000 : 0;
    }
    bool        bootBrownout() const  { return _bootBrownout; }
    bool        usbHostSeen() const   { return _usbHost; }

private:
    void inferState(uint32_t nowMs, int battMv);

    EntityRegistry *_reg = nullptr;
    adc_oneshot_unit_handle_t _unit = nullptr;
    adc_cali_handle_t         _cali = nullptr;
    adc_channel_t             _chan = ADC_CHANNEL_0;
    int                       _unitId = 0;

    uint32_t _intervalMs = 5000;
    uint32_t _lastPollMs = 0;
    uint32_t _lastLogMs  = 0;
    int32_t  _filtMv     = -1;   // smoothed battery mV, -1 = no reading yet
    bool     _begun      = false;

    int _lastRaw = -1, _lastPinMv = -1, _lastMv = -1;

    // Evidence for the state
    static constexpr int TREND_N = 60;   // 5 minutes of 5-s readings
    int16_t    _hist[TREND_N] = {};
    uint8_t    _histCount = 0, _histHead = 0;
    float      _trend = 0.0f;
    bool       _trendValid = false;
    int        _stepMv = 0;
    uint32_t   _stepAtMs = 0;
    int8_t     _extPower = -1;           // -1 unknown, 0 none, 1 present (from a step)
    bool       _bootBrownout = false;
    bool       _usbHost = false;
    PowerState _state = PowerState::PWR_NO_ADC;
    const char *_why = "";
};

#endif // BATTERY_PROVIDER_H
