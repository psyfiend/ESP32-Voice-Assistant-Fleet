#pragma once
//
// BoardDisplay - which display library this board is built with (#67,
// docs/display/architecture.md).
//
//   -D DISPLAY_ESPLCD   Fleet_Display   raw esp_lcd          (the five P4 boards and WS_S3_4B)
//   otherwise           DisplayManager  Arduino_GFX          (CYD_S3_8048, WS_S3_5B, CYD_S3_3248)
//
// Two separate libraries rather than one with two paths woven through it: each
// can be read on its own, with no wondering whether a change reached the other.
// Both offer begin(), setBacklight(), setBrightness() and getBrightness();
// everything above them names only BoardDisplay. When the last board moves,
// DisplayManager and this switch go, and BoardDisplay is simply Fleet_Display.
//
#include "BoardDisplayFwd.h"
#ifdef DISPLAY_ESPLCD
#include "Fleet_Display.h"
#else
#include "DisplayManager.h"
#endif
