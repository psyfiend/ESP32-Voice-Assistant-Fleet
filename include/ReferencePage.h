#pragma once
//
// ReferencePage — milestone 2.2's acceptance criterion: one page rendering
// every design token, on the actual glass.
//
// It is also milestone 2.3's instrument. Card cost is measured in LVGL's own
// pool (lv_mem_monitor), not the system heap: LV_USE_STDLIB_MALLOC is
// LV_STDLIB_BUILTIN with LV_MEM_ADR 0, so every widget allocation comes from a
// 128 KB static array in internal DRAM. ESP.getFreeHeap() barely moves when a
// card is created and would have been the wrong number to watch.
//
// Opened from the System panel. It loads as its own screen and restores the
// previous one on Back, so the dashboard underneath is untouched.
//
#include <lvgl.h>

namespace ReferencePage {

// Build and load the page. Safe to call repeatedly; rebuilds each time.
void show();

// Restore whatever screen was active before show(). Wired to the Back button.
void close();

} // namespace ReferencePage
