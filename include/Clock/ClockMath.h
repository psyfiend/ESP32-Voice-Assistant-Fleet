#pragma once
// ---------------------------------------------------------------------------
// THE CLOCK'S ARITHMETIC (#100) - pure: no LVGL, no Arduino, no ESP-IDF, so it
// is tested on the PC (host-tests/test/test_clock).
//
// PORTED FROM ESP32-P4-NINA-Display by chvvkumar
// (https://github.com/chvvkumar/ESP32-P4-NINA-Display), used with the author's
// permission for NON-COMMERCIAL use only:
// https://github.com/chvvkumar/ESP32-P4-NINA-Display/issues/326 (2026-10-06).
// That condition travels with this file and anything derived from it
// (docs/REFERENCE_PROJECTS.md). The dial maths is main/ui/clock_dial.h; the
// Classic face's words and proportions are main/ui/nina_clock.c.
// ---------------------------------------------------------------------------
#include <stddef.h>
#include <stdint.h>

namespace Clock {

// --- The dial (clock_dial.h) ------------------------------------------------
// A 12-hour dial in degrees: 0 at twelve o'clock, clockwise, 30 per hour. For
// the round faces' forecast arcs: each forecast hour becomes a block of the
// dial from its hour to the next one shown.
constexpr int DIAL_HOURS  = 12;
constexpr int DEG_PER_HOUR = 30;

// The signed hours from `base` to `h` (both 0-23), in -12..11.
int dialDelta(uint8_t base, uint8_t h);

// The blocks for up to `n` forecast hours (local, 0-23): where each starts on
// the dial, how far it runs, and which hour it came from. Hours that are not
// ahead of the one before (a stale or repeated slot) are skipped; the blocks
// stop before a full turn. Returns how many - 0 when there is nothing to show
// (no hours, or every hour the same: a forecast that failed left zeros).
int dialBlocks(const uint8_t *hours, int n, int16_t *startDeg, int16_t *spanDeg, uint8_t *srcIdx);

// --- The Classic face's words ---------------------------------------------------
// "9:25" on a 12-hour clock, "09:25" on a 24-hour one.
void timeText(char *buf, size_t n, int hour, int min, bool h24);
// "A M" / "P M", spaced as NINA's Classic face spaces them; "" on a 24-hour clock.
const char *meridiem(int hour, bool h24);
// "TUESDAY" (wday 0 = Sunday, as struct tm) and "OCTOBER 14" (mon 0 = January).
const char *weekdayName(int wday);
void dateText(char *buf, size_t n, int mon, int mday);

// --- Where it goes ------------------------------------------------------------------
// The largest square inside a w x h box.
int32_t squareSide(int32_t w, int32_t h);

// Of `n` faces, the largest one whose "00:00" width (`widths[i]`, smallest
// first) fits `room`; 0 (the smallest) when none does.
uint8_t pickFace(const int32_t *widths, uint8_t n, int32_t room);

// The Classic face in a square of `side` px, from NINA's 720 px design: its
// padding (40 at 720) and the gap under each rule. At least 1 px each.
int32_t classicPad(int32_t side);

} // namespace Clock
