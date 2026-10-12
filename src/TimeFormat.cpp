#include "TimeFormat.h"

#include <stdio.h>

// See TimeFormat.h. Pure: no LVGL, no ESP-IDF - only the C library's local time.

namespace {

const char *const MON_SHORT[12] = { "Jan", "Feb", "Mar", "Apr", "May", "Jun",
                                    "Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };
const char *const MON_LONG[12]  = { "January", "February", "March", "April", "May", "June", "July",
                                    "August", "September", "October", "November", "December" };
const char *const WEEKDAY[7]    = { "Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday",
                                    "Saturday" };

struct tm local(time_t t) {
    struct tm a;
    localtime_r(&t, &a);
    return a;
}

}  // namespace

void tfClock(char *out, size_t n, time_t t, TfClock style) {
    if (!out || !n) return;
    const struct tm a = local(t);
    int h = a.tm_hour % 12;
    if (!h) h = 12;
    const char *ap = a.tm_hour < 12 ? "AM" : "PM";
    if (style == TfClock::CLOCK_HMS) snprintf(out, n, "%d:%02d:%02d %s", h, a.tm_min, a.tm_sec, ap);
    else                             snprintf(out, n, "%d:%02d %s", h, a.tm_min, ap);
}

// Days between the two local dates, by day number from 1970 - immune to the
// hour of day and to a year's end (a tm_yday difference is not).
int tfDaysAgo(time_t t, time_t now) {
    struct tm a = local(t), b = local(now);
    auto dayNum = [](const struct tm &x) {
        // Days from 1970-01-01 to x's date (proleptic Gregorian).
        int y = x.tm_year + 1900, m = x.tm_mon + 1;
        if (m <= 2) { y--; m += 12; }
        return 365L * y + y / 4 - y / 100 + y / 400 + (153L * (m - 3) + 2) / 5 + x.tm_mday - 719469L;
    };
    return (int)(dayNum(b) - dayNum(a));
}

void tfAgo(char *out, size_t n, time_t t, time_t now, TfAgo style) {
    if (!out || !n) return;
    const int days = tfDaysAgo(t, now);
    const long mins = now > t ? (long)((now - t) / 60) : 0;
    if (days >= 2 && style == TfAgo::AGO_DATED) {
        const struct tm a = local(t);
        snprintf(out, n, "%d days ago - %s %d", days, MON_SHORT[a.tm_mon], a.tm_mday);
    }
    else if (days >= 2)  snprintf(out, n, "%d days ago", days);
    else if (days == 1)  snprintf(out, n, "Yesterday");
    else if (mins >= 60) {
        const long hrs = mins / 60;
        // The owner's template: within a quarter hour of the hour, "2 hours";
        // past it, "2+ hours".
        snprintf(out, n, "%ld%s hour%s ago", hrs, (mins - hrs * 60) <= 15 ? "" : "+", hrs == 1 ? "" : "s");
    }
    else if (mins >= 1)  snprintf(out, n, "%ld minute%s ago", mins, mins == 1 ? "" : "s");
    else                 snprintf(out, n, "A few moments ago");
}

void tfWhen(char *out, size_t n, time_t t, time_t now, TfWhen style) {
    if (!out || !n) return;
    char clock[16];
    tfClock(clock, sizeof(clock), t);
    const int days = tfDaysAgo(t, now);
    const struct tm a = local(t);
    if (days <= 0)      snprintf(out, n, "%s", clock);
    else if (days == 1) snprintf(out, n, style == TfWhen::WHEN_FULL ? "Yesterday %s" : "%s", clock);
    else                snprintf(out, n, "%s %d, %s", MON_SHORT[a.tm_mon], a.tm_mday, clock);
}

void tfDay(char *out, size_t n, time_t t, time_t now, TfDay style) {
    (void)style;
    if (!out || !n) return;
    const struct tm a = local(t);
    const int days = tfDaysAgo(t, now);
    const char *day = days == 0 ? "Today" : days == 1 ? "Yesterday" : WEEKDAY[a.tm_wday % 7];
    snprintf(out, n, "%s, %s %d, %d", day, MON_LONG[a.tm_mon], a.tm_mday, a.tm_year + 1900);
}

void tfDuration(char *out, size_t n, int32_t seconds, TfDur style) {
    if (!out || !n) return;
    if (seconds < 0) seconds = 0;   // only ever clock skew (NINA's rule)
    const int32_t d = seconds / 86400, h = seconds % 86400 / 3600, m = seconds % 3600 / 60, s = seconds % 60;
    if (style == TfDur::DUR_COMPACT) {
        if (d)      snprintf(out, n, h ? "%ldd %ldh" : "%ldd", (long)d, (long)h);
        else if (h) snprintf(out, n, m ? "%ldh %ldm" : "%ldh", (long)h, (long)m);
        else if (m) snprintf(out, n, "%ldm", (long)m);
        else        snprintf(out, n, "%lds", (long)s);
        return;
    }
    // The two largest units, as the owner wrote it: "2 hrs 38 min".
    if (d) {
        if (h) snprintf(out, n, "%ld day%s %ld hr%s", (long)d, d == 1 ? "" : "s", (long)h, h == 1 ? "" : "s");
        else   snprintf(out, n, "%ld day%s", (long)d, d == 1 ? "" : "s");
    } else if (h) {
        if (m) snprintf(out, n, "%ld hr%s %ld min", (long)h, h == 1 ? "" : "s", (long)m);
        else   snprintf(out, n, "%ld hr%s", (long)h, h == 1 ? "" : "s");
    } else if (m) {
        snprintf(out, n, "%ld min", (long)m);
    } else {
        snprintf(out, n, "%ld sec", (long)s);
    }
}
