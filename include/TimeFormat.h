#pragma once
#ifndef TIME_FORMAT_H
#define TIME_FORMAT_H

#include <stddef.h>
#include <stdint.h>
#include <time.h>

// ---------------------------------------------------------------------------
// TimeFormat - every time and duration the panel says, in one place (2.10e).
//
// The pattern is NINA's time_parse.c/h (chvvkumar/ESP32-P4-NINA-Display, used
// with the author's permission, non-commercial - docs/REFERENCE_PROJECTS.md):
// one pure module, no LVGL and no ESP-IDF, and an enum whose every value NAMES
// THE EXACT SHAPE of what it writes, so a caller says which wording it wants and
// two screens cannot drift apart. Add a style rather than bending one.
//
// Local time throughout (the TZ that TimeService sets); `now` is passed in, so
// a list of rows is worded against one instant and the module stays testable.
// Every function writes a NUL-terminated string and never more than `n` bytes.
// ASCII only - the panels' fonts have no "·" or "—" (CLAUDE.md).
// ---------------------------------------------------------------------------

// A clock time.
enum class TfClock : uint8_t {
    CLOCK_HM,      // "3:14 PM"
    CLOCK_HMS,     // "3:14:07 PM"
};
void tfClock(char *out, size_t n, time_t t, TfClock style = TfClock::CLOCK_HM);

// How long ago, in the words of the owner's HA template (2026-10-11).
enum class TfAgo : uint8_t {
    // "A few moments ago", "12 minutes ago", "2 hours ago" (within 15 min of
    // the hour) or "2+ hours ago", "Yesterday", "3 days ago".
    AGO_WORDS,
    // As AGO_WORDS, with the date past yesterday: "3 days ago - Oct 8" (owner,
    // round 5 - activity's rows, under the clock time).
    AGO_DATED,
};
void tfAgo(char *out, size_t n, time_t t, time_t now, TfAgo style = TfAgo::AGO_WORDS);

// A moment as a row shows it beside tfAgo(): the clock today and yesterday,
// with the date before that.
enum class TfWhen : uint8_t {
    WHEN_SHORT,    // "5:27 PM" today or yesterday; "Oct 8, 5:27 PM" before
    WHEN_FULL,     // "Yesterday 5:27 PM", "Oct 8, 5:27 PM" - one line, no tfAgo beside it
};
void tfWhen(char *out, size_t n, time_t t, time_t now, TfWhen style = TfWhen::WHEN_SHORT);

// A day, as a heading between a list's days (HA's, with a comma for its dot).
enum class TfDay : uint8_t {
    DAY_HEADING,   // "Today, October 11, 2026", "Yesterday, October 10, 2026", "Thursday, October 8, 2026"
};
void tfDay(char *out, size_t n, time_t t, time_t now, TfDay style = TfDay::DAY_HEADING);

// Calendar days from t's local date to now's: 0 today, 1 yesterday.
int tfDaysAgo(time_t t, time_t now);

// A length of time.
enum class TfDur : uint8_t {
    DUR_WORDS,     // "2 hrs 38 min", "1 hr", "45 min", "30 sec", "3 days 4 hrs"
    DUR_COMPACT,   // "2h 38m", "45m", "30s", "3d 4h"
};
void tfDuration(char *out, size_t n, int32_t seconds, TfDur style = TfDur::DUR_WORDS);

#endif // TIME_FORMAT_H
