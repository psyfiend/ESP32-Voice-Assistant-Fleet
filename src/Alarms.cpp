// The board's alarms - only what the header asks today. See include/Alarms.h.
#include "Alarms.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "Settings.h"

namespace Alarms {

namespace {
// Written and read on the UI thread only (/popup hands over).
time_t s_next = 0;        // 0: none set
time_t s_dismissed = 0;   // the alarm whose notice was dismissed
constexpr uint8_t WARN_DEFAULT = 12;
}  // namespace

bool next(time_t &when) {
    if (!s_next) return false;
    // One that has gone off is not "next" any more; nothing rings yet (K84).
    if (time(nullptr) >= s_next) { s_next = 0; return false; }
    when = s_next;
    return true;
}

bool bell(time_t now) {
    time_t when;
    return next(when) && (uint32_t)(when - now) <= BELL_SECS;
}

bool warning(time_t now, time_t &when) {
    const uint8_t h = warnHours();
    if (!h || !next(when) || when == s_dismissed) return false;
    return (uint32_t)(when - now) <= (uint32_t)h * 3600UL;
}

void dismissWarning() {
    time_t when;
    if (next(when)) s_dismissed = when;
}

uint8_t warnHours() {
    char v[8];
    if (!Settings::system("alarm", "warn", v, sizeof(v))) return WARN_DEFAULT;
    if (!strcmp(v, "off")) return 0;
    const int h = atoi(v);
    return (h > 0 && h <= 48) ? (uint8_t)h : WARN_DEFAULT;
}

void setWarnHours(uint8_t hours) {
    char v[8];
    snprintf(v, sizeof(v), "%u", (unsigned)hours);
    Settings::setSystem("alarm", "warn", hours == WARN_DEFAULT ? nullptr : hours ? v : "off");
    Settings::save();
}

bool setPretend(int hour, int minute, time_t now) {
    if (hour < 0) { s_next = 0; return true; }
    if (hour > 23 || minute < 0 || minute > 59 || now < 1000000000) return false;
    struct tm t;
    localtime_r(&now, &t);
    t.tm_hour = hour;
    t.tm_min  = minute;
    t.tm_sec  = 0;
    t.tm_isdst = -1;      // mktime decides, so a change of clocks overnight is counted
    time_t at = mktime(&t);
    if (at <= now) {
        localtime_r(&now, &t);
        t.tm_mday += 1;   // mktime normalises the month's end
        t.tm_hour = hour;
        t.tm_min  = minute;
        t.tm_sec  = 0;
        t.tm_isdst = -1;
        at = mktime(&t);
    }
    s_next = at;
    return true;
}

}  // namespace Alarms
