#pragma once
// ---------------------------------------------------------------------------
// THE CLOCK FACES' OWN COLOURS (#100), as data in one file.
//
// NINA's faces are drawn in fixed palettes of their own, chosen with each face's
// type - a clock face is a picture, not chrome - so they do not follow the
// scheme. The values live in src/Clock/ClockPalettes.cpp, the one file of the
// clock allowed colour literals (scripts/structure_check.py, COLOUR_OK); the
// card reads them through UI::c(), the one place that calls lv_color_hex.
//
// FROM ESP32-P4-NINA-Display by chvvkumar
// (https://github.com/chvvkumar/ESP32-P4-NINA-Display), main/ui/nina_clock_internal.h,
// used with the author's permission for NON-COMMERCIAL use only:
// https://github.com/chvvkumar/ESP32-P4-NINA-Display/issues/326 (2026-10-06).
// That condition travels with this file and anything derived from it.
// ---------------------------------------------------------------------------
#include <stdint.h>

namespace Clock {

// NINA's nine slots (clk_palette_t). The slots the time-and-date prototype does
// not draw yet (condition, bar_label: the weather) are kept for the faces to come.
struct Palette {
    uint32_t bg, primary, secondary, tertiary, dim, condition, rule, barLabel, accent;
};

extern const Palette CLASSIC;   // face 0: a warm near-black, bone white, clay

} // namespace Clock
