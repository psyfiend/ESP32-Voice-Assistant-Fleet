#pragma once
// ---------------------------------------------------------------------------
// THE BOARD'S ALARMS (K84) - not built yet. What exists is what the header asks
// (2.8, D-20, D-21):
//   - is an alarm set, and when does it go off next (the clock's panel);
//   - is it within a week - THE BELL beside the clock (D-21: "set at all, or
//     will go off within 7 days or so");
//   - is it within the owner's warning time - THE NOTICE among the status
//     icons, 12 hours unless the owner chose otherwise on the clock's panel
//     (1, 3, 6, 12 hours, or off; Settings system.alarm.warn) - and has its
//     notice been dismissed (until the next alarm comes due).
// An unset alarm is not missing data, so with none set nothing shows.
//
// Until K84 builds alarms, only a PC sets one, to see all this:
// /popup?alarm=HHMM (DEBUG_POPUP builds; 24-hour, the next such time from now),
// alarm=-1 clears it. It is held in RAM, never saved, and nothing rings.
// No LVGL: a lower layer (docs/design/ui-architecture.md section 1).
// ---------------------------------------------------------------------------
#include <stdint.h>
#include <time.h>

namespace Alarms {

// The next alarm, if one is set: true and its time in `when`.
bool next(time_t &when);

// THE BELL: the next alarm is within a week of `now`.
constexpr uint32_t BELL_SECS = 7UL * 24UL * 3600UL;
bool bell(time_t now);

// THE NOTICE: the next alarm is within the warning time of `now` and its notice
// has not been dismissed. `when` its time.
bool warning(time_t now, time_t &when);
void dismissWarning();               // this alarm's notice, until the next one
// The warning time in hours, 0 for off (Settings system.alarm.warn; 12 unless set).
uint8_t warnHours();
void    setWarnHours(uint8_t hours); // saved; 12 is stored as nothing

// For a PC until K84: the next hh:mm from `now` (today's if it is still to
// come, else tomorrow's). Negative hours clear it. False when the clock does
// not know the time yet.
bool setPretend(int hour, int minute, time_t now);

}  // namespace Alarms
