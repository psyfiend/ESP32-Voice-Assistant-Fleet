#pragma once
//
// BoardDisplay - which display library this board is built with. Milestone
// 2.9 (#67), docs/design/esplcd-step2.md §7 A.
//
//   -D DISPLAY_ESPLCD   Fleet_Display   raw esp_lcd          (WS_P4_5 from step 2)
//   otherwise           DisplayManager  Arduino_GFX          (every other board)
//
// Two separate libraries rather than one with two paths woven through it, at
// the owner's request: each can be read on its own, with no wondering whether
// a change reached the other. Both offer begin(), setBacklight(),
// setBrightness() and getBrightness(); everything above them names only
// BoardDisplay. When the last board moves (step 6), DisplayManager and this
// switch go, and BoardDisplay is simply Fleet_Display.
//
#include "BoardDisplayFwd.h"
#ifdef DISPLAY_ESPLCD
#include "Fleet_Display.h"
#else
#include "DisplayManager.h"
#endif
