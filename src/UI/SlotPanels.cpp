// The slots' panels (2.8). See include/UI/SlotPanels.h.
#include "UI/SlotPanels.h"

#include <Arduino.h>   // millis()
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include "UI/Kit.h"
#include "Cards/CardIcons.h"   // a reading and its age, as its card says them
#include "Alarms.h"
#include "EntityRegistry.h"
#include "TimeFormat.h"
#include "TimeService.h"

namespace SlotPanels {

namespace {

Context s_ctx;   // the open panel's - one opens at a time

void pageCb(lv_event_t *e) {
    const uint8_t i = (uint8_t)(uintptr_t)lv_event_get_user_data(e);
    if (s_ctx.goToPage) s_ctx.goToPage(s_ctx.user, i);
}

void headerCb(lv_event_t *e) {
    // The chips' order: 0 Shown, 1 Hidden.
    const uint8_t i = (uint8_t)(uintptr_t)lv_event_get_user_data(e);
    if (s_ctx.setHeaderShown) s_ctx.setHeaderShown(s_ctx.user, i == 0);
}

void title(lv_obj_t *face, const char *words) {
    lv_label_set_text(Kit::label(face, Kit::KitStyle::KS_PANEL_TITLE), words);
}

lv_obj_t *dim(lv_obj_t *parent, const char *words) {
    lv_obj_t *l = Kit::label(parent, Kit::KitStyle::KS_LABEL_NAME_DIM);
    lv_label_set_text(l, words);
    return l;
}

// The clock: the time large and the date under it. The next alarm is the
// alarm slot's (D-18), among the status glyphs.
void buildClock(lv_obj_t *face) {
    char t[24] = "--:--", d[48] = "The time is not known yet";
    if (TimeService::isSet()) {
        const time_t now = time(nullptr);
        struct tm tm;
        localtime_r(&now, &tm);
        const int h12 = tm.tm_hour % 12 ? tm.tm_hour % 12 : 12;
        snprintf(t, sizeof(t), "%d:%02d %s", h12, tm.tm_min, tm.tm_hour < 12 ? "am" : "pm");
        char wd[16], mo[16];
        strftime(wd, sizeof(wd), "%A", &tm);
        strftime(mo, sizeof(mo), "%B", &tm);
        snprintf(d, sizeof(d), "%s, %s %d", wd, mo, tm.tm_mday);
    }
    Kit::drawerBig(face, t);
    dim(face, d);
    Kit::drawerNote(face, "Later: timers (K84, with the Clock page); 24 hours or seconds, as settings.");
}

// The alarm: when the next one goes off, and how long until then. It is on
// the bar only while one is set (D-20), so its panel always has one to show.
void buildAlarm(lv_obj_t *face) {
    time_t when = 0;
    char t[24] = "--:--", d[48] = "No alarm set";
    if (Alarms::next(when)) {
        struct tm tm;
        localtime_r(&when, &tm);
        const int h12 = tm.tm_hour % 12 ? tm.tm_hour % 12 : 12;
        snprintf(t, sizeof(t), "%d:%02d %s", h12, tm.tm_min, tm.tm_hour < 12 ? "am" : "pm");
        // Whole minutes, rounded up: "in 1 min" until it goes off.
        const int32_t secs = (int32_t)(((when - time(nullptr) + 59) / 60) * 60);
        char dur[32];
        tfDuration(dur, sizeof(dur), secs, TfDur::DUR_WORDS);
        snprintf(d, sizeof(d), "in %s", dur);
    }
    title(face, "NEXT ALARM");
    Kit::drawerBig(face, t);
    dim(face, d);
    Kit::drawerNote(face, "Set from a PC for now. Later: setting, snoozing and your phone's alarms (K84).");
}

// The page: every page, to go to one (this one marked), and whether this page
// shows its header (D-14). Hidden, a swipe down from the top edge brings the
// bar back for a moment, as Hide Bar's does.
void buildPage(lv_obj_t *face, const Context &c) {
    title(face, "PAGES");
    for (uint8_t i = 0; i < c.pageCount && i < PAGES_MAX; i++)
        Kit::drawerItem(face, c.pageTitles[i], i == c.pageIndex, pageCb, (void *)(uintptr_t)i);
    static const char *const SHOWN[] = { "Shown", "Hidden" };
    Kit::chipRow(Kit::row(face, "Header", true), SHOWN, 2, c.headerShown ? 0 : 1, 0, headerCb);
    Kit::drawerNote(face, "Hidden: swipe down from the top edge to see it for a moment. Later: this "
                          "page's cards, arrangement and name (2.11).");
}

// The sensor: its name, its reading - or dashes and what went wrong - and when
// it last reported. Never an old value (D-15).
void buildSensor(lv_obj_t *face, const Context &c) {
    const Entity *e = (c.reg && c.entity) ? c.reg->find(c.entity) : nullptr;
    // The name the dashboard gave it, else its source's; wrapped, never cut.
    const char *name = !e ? (c.entity ? c.entity : "-")
                     : e->desc.name[0] ? e->desc.name : e->sourceName;
    lv_obj_t *n = dim(face, name);
    lv_obj_set_width(n, lv_pct(100));
    lv_label_set_long_mode(n, LV_LABEL_LONG_WRAP);
    const uint32_t now = millis();
    char val[32] = "--", line[64];
    if (!e) {
        snprintf(line, sizeof(line), "This board does not know it");
    } else if (!e->everSet) {
        snprintf(line, sizeof(line), "No reading yet - waiting for its first report");
    } else if (!e->available) {
        snprintf(line, sizeof(line), "Unavailable at its source");
    } else if (c.reg->isStale(*e, now)) {
        char age[16];
        cardFormatAge(now - e->lastUpdateMs, age, sizeof(age));
        snprintf(line, sizeof(line), "No report for %s", age);
    } else {
        cardFormatValue(*e, val, sizeof(val), true);
        char age[16];
        cardFormatAge(now - e->lastUpdateMs, age, sizeof(age));
        if (!strcmp(age, "now")) snprintf(line, sizeof(line), "reported just now");
        else                     snprintf(line, sizeof(line), "reported %s ago", age);
    }
    Kit::drawerBig(face, val);
    dim(face, line);
    Kit::drawerNote(face, "Later: the forecast, more sensors, and choosing which one this slot shows.");
}

}  // namespace

bool build(SlotKind kind, lv_obj_t *face, const Context &c) {
    s_ctx = c;
    switch (kind) {
        case SlotKind::SLOT_CLOCK:  buildClock(face);     return true;
        case SlotKind::SLOT_PAGE:   buildPage(face, c);   return true;
        case SlotKind::SLOT_SENSOR: buildSensor(face, c); return true;
        case SlotKind::SLOT_ALARM:  buildAlarm(face);     return true;
        default:                    return false;   // status: the system panel
    }
}

}  // namespace SlotPanels
