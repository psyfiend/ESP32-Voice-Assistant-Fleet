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

// A section's title: centred between its rules (round 14, the owner), as the
// system panel's are.
void title(lv_obj_t *face, const char *words) { Kit::drawerTitle(face, words); }

lv_obj_t *dim(lv_obj_t *parent, const char *words) {
    lv_obj_t *l = Kit::label(parent, Kit::KitStyle::KS_LABEL_NAME_DIM);
    lv_label_set_text(l, words);
    return l;
}

// A time as the clock writes it, "6:30 am", and how long until it, "in 4 hrs
// 10 min" - whole minutes, rounded up, so "in 1 min" until it goes off.
void alarmWords(time_t when, char *t, size_t tn, char *d, size_t dn) {
    struct tm tm;
    localtime_r(&when, &tm);
    const int h12 = tm.tm_hour % 12 ? tm.tm_hour % 12 : 12;
    snprintf(t, tn, "%d:%02d %s", h12, tm.tm_min, tm.tm_hour < 12 ? "am" : "pm");
    const int32_t secs = (int32_t)(((when - time(nullptr) + 59) / 60) * 60);
    char dur[32];
    tfDuration(dur, sizeof(dur), secs, TfDur::DUR_WORDS);
    snprintf(d, dn, "in %s", dur);
}

// A panel's selector: compact, as wide as the panel's face allows.
int32_t faceRoom() { return Kit::drawerWidth() - 2 * UI::sc(UI::hdr().DRAWER_PAD); }

// "Warn me before" (D-21): the notice's hours, or off - saved.
const uint8_t WARN_HOURS[] = { 1, 3, 6, 12, 0 };
const char *const WARN_WORDS[] = { "1 h", "3 h", "6 h", "12 h", "Off" };
Kit::Selector s_warnSel;
void warnChosen(uint8_t i) {
    if (i >= sizeof(WARN_HOURS)) return;
    Alarms::setWarnHours(WARN_HOURS[i]);
    if (s_ctx.refreshHeader) s_ctx.refreshHeader(s_ctx.user);
}

// The clock: the time large and the date under it; then the next alarm and
// how long until it, and how long before it its notice joins the status icons
// (D-21: "the within 12 hours warning time can be customized ... from the
// clock panel"). Setting alarms comes with K84.
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
    title(face, "NEXT ALARM");
    time_t when = 0;
    if (Alarms::next(when)) {
        char at[24], in[48];
        alarmWords(when, at, sizeof(at), in, sizeof(in));
        lv_obj_t *r = Kit::row(face, at, true);
        dim(r, in);
    } else {
        dim(face, "None set");
    }
    dim(face, "Warn me before");
    Kit::SelectorSpec sp = {};
    for (uint8_t i = 0; i < 5; i++) sp.glyph[i] = WARN_WORDS[i];
    sp.n = 5;
    const uint8_t h = Alarms::warnHours();
    sp.chosen = 4;
    for (uint8_t i = 0; i < 4; i++) if (WARN_HOURS[i] == h) sp.chosen = i;
    sp.compact  = true;
    sp.room     = faceRoom();
    sp.onChosen = warnChosen;
    Kit::selector(s_warnSel, face, sp);
    Kit::drawerNote(face, "Setting alarms comes with K84 (a PC sets a pretend one for now); "
                          "timers and 24 hours later.");
}

// The alarm's notice (D-21): when, how long until it; Dismiss - until the next
// alarm comes due - or Settings, which is the clock's panel.
void dismissCb(lv_event_t *) { if (s_ctx.dismissWarning) s_ctx.dismissWarning(s_ctx.user); }
void settingsCb(lv_event_t *) { if (s_ctx.openClock) s_ctx.openClock(s_ctx.user); }

void buildAlarm(lv_obj_t *face) {
    time_t when = 0;
    char t[24] = "--:--", d[48] = "No alarm set";
    if (Alarms::next(when)) alarmWords(when, t, sizeof(t), d, sizeof(d));
    title(face, "ALARM SOON");
    Kit::drawerBig(face, t);
    dim(face, d);
    lv_obj_t *r = Kit::drawerRow(face);
    const int32_t bw = (faceRoom() - UI::sc(UI::hdr().DRAWER_GAP)) / 2;
    Kit::button(r, Kit::ButtonRole::BTN_CANCEL, "Dismiss", bw, UI::minTouch(), dismissCb);
    Kit::button(r, Kit::ButtonRole::BTN_CANCEL, "Settings", bw, UI::minTouch(), settingsCb);
    Kit::drawerNote(face, "Dismiss hides it until the next alarm comes due. Settings is the clock's "
                          "panel: how long before, or off.");
}

// The page: every page, to go to one (this one marked), and whether this page
// shows its header (D-14) - a selector (D-22), Show / Hide (round 4). The bar
// stays while the panel is open; hidden, a swipe down from the top edge brings
// it back for a moment.
Kit::Selector s_hdrSel;
lv_obj_t *s_hdrNote = nullptr;   // HEADER's words: shown the moment Hide is chosen (round 15)
void hdrChosen(uint8_t i) {
    if (s_ctx.setHeaderShown) s_ctx.setHeaderShown(s_ctx.user, i == 0);
    Kit::drawerReveal(s_hdrNote, i != 0);
}

// THIS PAGE'S HIDDEN CARDS (2.11, K66, K96 - the mock's "this page's list of
// hidden cards"): a button to their own sheet (round 6, the owner: the list made
// the panel long), there only when the page has some.
void viewHiddenCb(lv_event_t *) {
    if (s_ctx.viewHidden) s_ctx.viewHidden(s_ctx.user);
}

void hiddenCards(lv_obj_t *face, const Context &c) {
    if (!c.hiddenCount || !c.viewHidden) return;
    char words[32];
    snprintf(words, sizeof(words), "View hidden cards (%u)", (unsigned)c.hiddenCount);
    Kit::button(Kit::buttonRow(face), Kit::ButtonRole::BTN_CANCEL, words, faceRoom(), UI::minTouch(), viewHiddenCb);
}

// THREE SECTIONS (round 14, the owner's): PAGES - every page, close together;
// HEADER - Show / Hide, centred; HIDDEN - View hidden cards when there are some.
// A section's words only while they are needed (round 14b: with the button the
// panel overran the P4_5): HEADER's while it is hidden - from the tap on Hide,
// round 15 - HIDDEN's with no button.
void buildPage(lv_obj_t *face, const Context &c) {
    title(face, "PAGES");
    lv_obj_t *pages = Kit::plain(face);
    lv_obj_set_size(pages, lv_pct(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(pages, LV_FLEX_FLOW_COLUMN);
    for (uint8_t i = 0; i < c.pageCount && i < PAGES_MAX; i++)
        Kit::drawerItem(pages, c.pageTitles[i], i == c.pageIndex, pageCb, (void *)(uintptr_t)i);
    title(face, "HEADER");
    Kit::SelectorSpec sp = {};
    sp.glyph[0] = "Show";
    sp.glyph[1] = "Hide";
    sp.n = 2;
    sp.chosen = c.headerShown ? 0 : 1;
    sp.compact  = true;
    sp.room     = faceRoom();
    sp.onChosen = hdrChosen;
    Kit::selector(s_hdrSel, Kit::drawerCentred(face), sp);
    s_hdrNote = Kit::drawerNote(face, "When hidden, swipe down from the top edge of the screen to reveal");
    if (c.headerShown) lv_obj_add_flag(s_hdrNote, LV_OBJ_FLAG_HIDDEN);
    title(face, "HIDDEN");
    hiddenCards(face, c);
    if (!c.hiddenCount || !c.viewHidden)
        Kit::drawerNote(face, "Cards hidden from the dashboard can be viewed/restored from here");
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

#ifdef DEBUG_POPUP
bool viewHiddenDebug() {
    if (!s_ctx.viewHidden || !s_ctx.hiddenCount) return false;
    s_ctx.viewHidden(s_ctx.user);
    s_ctx.viewHidden = nullptr;   // once: the panel it was built for has gone
    return true;
}
#endif

}  // namespace SlotPanels
