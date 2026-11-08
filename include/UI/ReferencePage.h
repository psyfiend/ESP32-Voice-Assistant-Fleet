#pragma once
//
// ReferencePage — one page rendering every design token, on the actual glass.
//
// It is also the instrument for card cost. Card cost is measured in LVGL's own
// pool (lv_mem_monitor), not the system heap: every widget allocation comes from
// LVGL's pool (in PSRAM, A11), and ESP.getFreeHeap() barely moves when a card is
// created.
//
// Opened from the System panel. It loads as its own screen and restores the
// previous one on Back, so the dashboard underneath is untouched.
//
#include <lvgl.h>

namespace ReferencePage {

// Build and load the page. Safe to call repeatedly; rebuilds each time.
void show();

// Run when the page closes. Registered by GUIManager so the dashboard - which
// stands down before this page opens, to keep two full widget trees from
// existing at once - is rebuilt on the way back.
void setCloseHandler(void (*cb)());

// Restore whatever screen was active before show(). Wired to the Back button.
void close();

} // namespace ReferencePage
