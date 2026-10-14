// THE CLOCK FACES' OWN COLOURS (#100) - see include/Clock/ClockPalettes.h. The one
// file of the clock allowed colour literals (scripts/structure_check.py, COLOUR_OK).
//
// FROM ESP32-P4-NINA-Display by chvvkumar
// (https://github.com/chvvkumar/ESP32-P4-NINA-Display), main/ui/nina_clock_internal.h,
// used with the author's permission for NON-COMMERCIAL use only:
// https://github.com/chvvkumar/ESP32-P4-NINA-Display/issues/326 (2026-10-06).
// That condition travels with this file and anything derived from it.
#include "Clock/ClockPalettes.h"

namespace Clock {

// NINA's CLK_* colours for the Classic face.
const Palette CLASSIC = {
    .bg        = 0x121110,
    .primary   = 0xE8E2D4,
    .secondary = 0xC8C2B4,
    .tertiary  = 0x908A7E,
    .dim       = 0x6A6458,
    .condition = 0x908A7E,
    .rule      = 0x2A2622,
    .barLabel  = 0x5A5448,
    .accent    = 0xB86A3A,
};

} // namespace Clock
