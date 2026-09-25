#pragma once
//
// LVGL_Flush - how LVGL's finished pixels reach the panel. Private to
// LVGL_Startup. Milestone 2.9 (#67), docs/design/esplcd-step2.md §7 A.
//
// Exactly ONE of these is compiled into a build, each wholly inside one #if:
//
//   LVGL_Flush_Gfx.cpp     Arduino_GFX (DisplayManager)   every board without DISPLAY_ESPLCD
//   LVGL_Flush_EspLcd.cpp  raw esp_lcd (Fleet_Display)    boards with -D DISPLAY_ESPLCD
//
// Reading either file tells the whole story of that path; neither has a
// branch for the other.
//
#include <lvgl.h>
#include <esp_heap_caps.h>
#include "BoardDisplay.h"
#include "LVGL_Startup.h"

namespace LVGL_Flush {

// Creates the LVGL display at its logical (as-seen, rotated) size, allocates
// the draw buffers and registers the flush callback. Fills `info` for
// reports. Returns nullptr if anything fails.
lv_display_t *create(BoardDisplay &display, LVGL_Startup::DrawBufInfo &info);

// Draw buffers must start on an LV_DRAW_BUF_ALIGN boundary, or
// lv_display_set_buffers() fails its alignment assert - which on this fleet is
// `while(1);`, a silent freeze at boot. Plain heap_caps_malloc() only promises
// 4 (internal) or 16 (PSRAM) bytes, which was enough while LV_DRAW_BUF_ALIGN
// was 4 and stops being enough the moment it is 64 (the PPA's cache line).
// The size is rounded up too, because a cache sync over a buffer wants whole
// cache lines.
inline void *allocDrawBuf(size_t &bytes, uint32_t caps) {
    constexpr size_t A = LV_DRAW_BUF_ALIGN;
    bytes = (bytes + A - 1) / A * A;
    return heap_caps_aligned_alloc(A, bytes, caps);
}

#ifdef DISPLAY_ESPLCD
// The frame buffer the panel is showing (or about to), RGB565, physical
// orientation, `w` x `h`. For GET /screenshot?fb=1: the pixels on the glass,
// not LVGL's re-render of its objects. LVGL thread only.
const void *shownFrameBuffer(uint32_t &w, uint32_t &h);
#endif

} // namespace LVGL_Flush

namespace LVGL_Startup {
// The FlushStats GET /bench attached, or nullptr. For the flush files.
FlushStats *activeFlushStats();
}
