// THE CLOCK'S ARITHMETIC (#100) - see include/Clock/ClockMath.h.
//
// PORTED FROM ESP32-P4-NINA-Display by chvvkumar
// (https://github.com/chvvkumar/ESP32-P4-NINA-Display), used with the author's
// permission for NON-COMMERCIAL use only:
// https://github.com/chvvkumar/ESP32-P4-NINA-Display/issues/326 (2026-10-06).
// That condition travels with this file and anything derived from it.
#include "Clock/ClockMath.h"

#include <stdio.h>

namespace Clock {

// clock_dial.h's clock_dial_delta(): the shortest way round a 24-hour day.
int dialDelta(uint8_t base, uint8_t h) {
    int d = (int)h - (int)base;
    if (d < -12) d += 24;
    if (d > 11)  d -= 24;
    return d;
}

// clock_dial.h's clock_dial_blocks(), as it is - including its stop at 12 hours
// ahead, which dialDelta() (at most 11) never reaches: the stale-slot skip is
// what drops those hours.
int dialBlocks(const uint8_t *hours, int n, int16_t *startDeg, int16_t *spanDeg, uint8_t *srcIdx) {
    if (!hours || !startDeg || !spanDeg || !srcIdx || n <= 0) return 0;
    bool allSame = true;
    for (int i = 1; i < n; i++) if (hours[i] != hours[0]) { allSame = false; break; }
    if (allSame) return 0;

    int count = 0;
    int prevOff = -1;
    for (int i = 0; i < n; i++) {
        const int off = dialDelta(hours[0], hours[i]);
        if (i > 0 && off <= prevOff) continue;   // stale or repeated slot
        if (off >= DIAL_HOURS) break;            // past the half day
        // The next hour shown after this one: where this block ends.
        int next = -1;
        for (int j = i + 1; j < n; j++) {
            const int o = dialDelta(hours[0], hours[j]);
            if (o > off) { next = o; break; }
        }
        int step = next >= 0 ? next - off : (prevOff >= 0 ? off - prevOff : 1);
        if (step < 1) step = 1;
        if (step > DIAL_HOURS - off) step = DIAL_HOURS - off;
        startDeg[count] = (int16_t)((hours[i] % DIAL_HOURS) * DEG_PER_HOUR);
        spanDeg[count]  = (int16_t)(step * DEG_PER_HOUR);
        srcIdx[count]   = (uint8_t)i;
        count++;
        prevOff = off;
    }
    return count;
}

void timeText(char *buf, size_t n, int hour, int min, bool h24) {
    if (!buf || !n) return;
    if (h24) { snprintf(buf, n, "%02d:%02d", hour, min); return; }
    int h = hour % 12;
    if (h == 0) h = 12;
    snprintf(buf, n, "%d:%02d", h, min);
}

const char *meridiem(int hour, bool h24) {
    if (h24) return "";
    return hour < 12 ? "A M" : "P M";
}

static const char *const WEEKDAYS[7] = { "SUNDAY", "MONDAY", "TUESDAY", "WEDNESDAY",
                                         "THURSDAY", "FRIDAY", "SATURDAY" };
static const char *const MONTHS[12] = { "JANUARY", "FEBRUARY", "MARCH", "APRIL", "MAY", "JUNE",
                                        "JULY", "AUGUST", "SEPTEMBER", "OCTOBER", "NOVEMBER", "DECEMBER" };

const char *weekdayName(int wday) { return (wday >= 0 && wday < 7) ? WEEKDAYS[wday] : ""; }

void dateText(char *buf, size_t n, int mon, int mday) {
    if (!buf || !n) return;
    snprintf(buf, n, "%s %d", (mon >= 0 && mon < 12) ? MONTHS[mon] : "", mday);
}

int32_t squareSide(int32_t w, int32_t h) {
    const int32_t s = w < h ? w : h;
    return s > 0 ? s : 0;
}

uint8_t pickFace(const int32_t *widths, uint8_t n, int32_t room) {
    uint8_t best = 0;
    for (uint8_t i = 0; widths && i < n; i++) if (widths[i] <= room) best = i;
    return best;
}

int32_t classicPad(int32_t side) {
    const int32_t p = side * 40 / 720;
    return p > 0 ? p : 1;
}

} // namespace Clock
