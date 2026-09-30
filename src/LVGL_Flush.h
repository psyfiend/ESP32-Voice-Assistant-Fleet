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
#include <esp_err.h>
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

// Frames the panel has finished scanning since boot, counted in its
// frame-complete interrupt. Two readings over a known time give the refresh
// rate the panel is REALLY running at, not the one its timing implies.
uint32_t panelFramesScanned();

// The rotation the flush applies between LVGL and the frame buffer (0-3,
// Arduino_GFX's meaning). Usually the BSP's ROTATION; 0 when the panel turns
// the picture itself (Fleet_Display::softwareRotation()). For anything that
// maps LVGL coordinates onto the frame buffer, e.g. /bench?what=verify.
uint8_t softwareRotation();

// GET /bench?what=copy: how fast each engine copies frame-buffer memory on
// this board, PSRAM to PSRAM - the question behind the PPA's unexplained
// ~110 MB/s (display-stack.md s8.5) and whether esp_async_fbcpy would make
// a better repair engine. Averages of `reps` runs, microseconds. LVGL thread
// only; uses the free frame buffer and leaves it holding the newest frame.
struct CopyBench {
    uint32_t  frameBytes = 0;     // one whole frame
    uint32_t  rotBytes   = 0;     // the square block the rotation test moves
    int64_t   ppaCopyUs  = 0;     // PPA, angle 0, whole frame
    int64_t   ppaRotUs   = 0;     // PPA, this board's rotation angle (90 if 0), square block
    int64_t   fbcpyUs    = 0;     // esp_async_fbcpy (DMA2D), whole frame
    int64_t   cpuUs      = 0;     // memcpy + cache write-back, whole frame
    esp_err_t fbcpyErr   = ESP_OK;
    // Correctness of esp_async_fbcpy, checked by the CPU against the source
    // after the destination was cleared: the whole frame, then one band of
    // rows (bandY, bandH) alone. -1 = no bad pixel. bandOutside counts pixels
    // OUTSIDE the band that the band copy changed (it must change none).
    uint32_t  fullBad = 0;   int32_t fullFirstBadRow = -1;
    uint32_t  bandBad = 0;   int32_t bandFirstBadRow = -1;
    uint32_t  bandOutside = 0;
    int32_t   bandY = 0, bandH = 0;
    // The same check for rectangles inside that band with odd and even
    // x/width: {x, w, bad pixels inside, changed pixels outside}.
    static constexpr uint8_t RECTS = 6;
    struct RectCheck { int32_t x, w; uint32_t bad, outside; };
    RectCheck rects[RECTS] = {};
    // Concurrency, checked the same way: two esp_async_fbcpy copies on two
    // handles at once (rows 0-199 and 600-799), and one alongside a PPA copy
    // (fbcpy rows 0-399, PPA rows 800-1199, clamped to the frame). Bad pixels
    // inside each range; changed pixels outside both. NOTE the dual test
    // passes only because both copies START at once: a copy that has to QUEUE
    // behind another is corrupted (esp_async_fbcpy_priv.h) - this test cannot
    // show that, the flush's verify (what=verify) did.
    uint32_t  dualA = 0, dualB = 0, dualOutside = 0;
    uint32_t  mixF = 0, mixP = 0, mixOutside = 0;
};
bool copyBench(CopyBench &out, int reps);
#endif

} // namespace LVGL_Flush

namespace LVGL_Startup {
// The FlushStats GET /bench attached, or nullptr. For the flush files.
FlushStats *activeFlushStats();
}
