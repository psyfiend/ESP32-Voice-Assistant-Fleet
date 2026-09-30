#pragma once
//
// BoardDisplay, forward-declared - for headers that only pass it by reference.
// The full picture, and why there are two, is in BoardDisplay.h.
//
#ifdef DISPLAY_ESPLCD
class Fleet_Display;
using BoardDisplay = Fleet_Display;
#else
class DisplayManager;
using BoardDisplay = DisplayManager;
#endif
