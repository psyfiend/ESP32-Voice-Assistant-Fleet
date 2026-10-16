#pragma once
// ---------------------------------------------------------------------------
// THE BOARD'S ALARMS (K84) - not built yet. What exists is the one question the
// header's alarm slot asks (2.8 round 2, D-20): is an alarm set, and when does
// it go off next. An unset alarm is not missing data, so with none set the
// slot leaves the bar.
//
// Until K84 builds alarms, only a PC sets one, to see the slot and its panel:
// /popup?alarm=HHMM (DEBUG_POPUP builds; 24-hour, the next such time from now),
// alarm=-1 clears it. It is held in RAM, never saved, and nothing rings.
// No LVGL: a lower layer (docs/design/ui-architecture.md section 1).
// ---------------------------------------------------------------------------
#include <stdint.h>
#include <time.h>

namespace Alarms {

// The next alarm, if one is set: true and its time in `when`.
bool next(time_t &when);

// For a PC until K84: the next hh:mm from `now` (today's if it is still to
// come, else tomorrow's). Negative hours clear it. False when the clock does
// not know the time yet.
bool setPretend(int hour, int minute, time_t now);

}  // namespace Alarms
