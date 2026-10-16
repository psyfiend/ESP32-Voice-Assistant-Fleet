// The board's alarms - only what the header asks today. See include/Alarms.h.
#include "Alarms.h"

namespace Alarms {

namespace {
time_t s_next = 0;   // 0: none set. Written and read on the UI thread only (/popup hands over)
}  // namespace

bool next(time_t &when) {
    if (!s_next) return false;
    // One that has gone off is not "next" any more; nothing rings yet (K84).
    if (time(nullptr) >= s_next) { s_next = 0; return false; }
    when = s_next;
    return true;
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
