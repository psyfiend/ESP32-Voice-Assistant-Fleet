// LVGL_Flush, esp_lcd path for RGB panels - the DOUBLE_DIRECT present mode
// (Fleet_BSP.h). WS_S3_4B from 2.9 step 4. See LVGL_Flush.h; the MIPI-DSI
// boards' flush is LVGL_Flush_EspLcd.cpp (TRIPLE_PARTIAL).
//
// THE SHAPE. LVGL draws straight into the panel's own frame buffers - two of
// them, in PSRAM, handed to lv_display_set_buffers() in DIRECT mode. There is
// no strip buffer and no copy step: LVGL redraws only what changed, at its
// real position, in the buffer the panel is NOT showing.
//
//   every chunk but the last   nothing to do - the pixels are already in place
//   the last chunk             hand the finished buffer to the panel (a switch,
//                              not a copy - IDF recognises its own buffer), and
//                              wait until the panel has actually moved to it,
//                              because the OTHER buffer is where LVGL draws
//                              next and it is on the glass until then
//
// Keeping the other buffer current is LVGL's own job in DIRECT mode: before it
// draws a frame it copies into it the areas the previous frame changed
// (lv_refr.c, the sync of the off-screen buffer). CPU copies - the S3 has no
// PPA or DMA2D. Only the changed areas, so on a dashboard it is small.
//
// DIRECT cannot rotate: LVGL writes pixels where they land on the glass. A
// rotated RGB board would need a partial mode and a copy (a CPU one on an S3).
//
// Cache: with bounce buffers (the S3_4B has them) the panel's pixels are read
// out of PSRAM by the CPU, into the bounce buffers, so they see what LVGL
// wrote through the cache. Without them the DMA reads PSRAM directly, and the
// buffer is written back first.
//
#if defined(DISPLAY_ESPLCD) && defined(HAS_RGB_PANEL)

#include "LVGL_Flush.h"
#include <Arduino.h>
#include <esp_timer.h>
#include "esp_cache.h"
#include "bsp_loader.h"
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

namespace {

Fleet_Display *s_d = nullptr;
void *s_fb[2] = {nullptr, nullptr};

// How long to wait for the panel to take a buffer: two frames at the S3_4B's
// ~56 Hz is ~36 ms. Past this, flush anyway and count it - a stuck panel must
// not stall the UI for good.
constexpr int64_t SWITCH_TIMEOUT_US = 100 * 1000;
uint32_t s_switchTimeouts = 0;

void disp_flush(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map) {
    LVGL_Startup::FlushStats *stats = LVGL_Startup::activeFlushStats();
    if (stats) {
        stats->chunks++;
        stats->px += (uint32_t)lv_area_get_size(area);
    }
    if (!lv_display_flush_is_last(disp)) {   // pixels already in the frame buffer
        lv_display_flush_ready(disp);
        return;
    }

    // px_map is the base of the buffer LVGL just finished.
    const uint8_t idx = (px_map == static_cast<uint8_t *>(s_fb[1])) ? 1 : 0;
    const int64_t t0 = esp_timer_get_time();
    if (s_d->bouncePixels() == 0) {
        esp_cache_msync(s_fb[idx], s_d->frameBufferBytes(), ESP_CACHE_MSYNC_FLAG_DIR_C2M);
    }
    s_d->present(idx);

    // Wait for the switch (Fleet_Display's frame-complete interrupt moves
    // scanning() to idx at the end of the frame being read out now).
    while (s_d->scanning() != idx) {
        if (esp_timer_get_time() - t0 > SWITCH_TIMEOUT_US) {
            s_switchTimeouts++;
            if (s_switchTimeouts <= 3 || s_switchTimeouts % 100 == 0) {
                Serial.printf("[LVGL] panel did not take buffer %u within 100 ms (%lu times)\n",
                              (unsigned)idx, (unsigned long)s_switchTimeouts);
            }
            break;
        }
        vTaskDelay(1);
    }
    // Hand-over plus the wait for the panel: reported as `present`.
    if (stats) stats->presentUs += esp_timer_get_time() - t0;
    lv_display_flush_ready(disp);
}

} // namespace

namespace LVGL_Flush {

lv_display_t *create(BoardDisplay &display, LVGL_Startup::DrawBufInfo &info) {
    s_d = &display;
    if (display.softwareRotation() != 0) {
        Serial.println("[LVGL] Critical: DOUBLE_DIRECT cannot rotate - this RGB board needs ROTATION 0");
        return nullptr;
    }
    if (display.numFrameBuffers() < 2) {
        Serial.println("[LVGL] Critical: DOUBLE_DIRECT needs two frame buffers");
        return nullptr;
    }
    s_fb[0] = display.frameBuffer(0);
    s_fb[1] = display.frameBuffer(1);
    const uint32_t w = display.panelWidth(), h = display.panelHeight();
    const size_t bytes = display.frameBufferBytes();

    info.bytes = bytes;
    info.lines = h;
    info.count = 2;
    info.psram = true;
    Serial.printf("[LVGL] esp_lcd path: %lux%lu, DOUBLE_DIRECT into the panel's 2 frame buffers (%u KB each)\n",
                  (unsigned long)w, (unsigned long)h, (unsigned)(bytes / 1024));

    lv_display_t *disp = lv_display_create(w, h);
    lv_display_set_flush_cb(disp, disp_flush);
    lv_display_set_buffers(disp, s_fb[0], s_fb[1], bytes, LV_DISPLAY_RENDER_MODE_DIRECT);
    return disp;
}

// The buffer on the glass. LVGL wrote it through the CPU cache, so the CPU
// already sees its pixels - no cache invalidate (which would throw away any
// line not yet written back).
const void *shownFrameBuffer(uint32_t &w, uint32_t &h) {
    if (!s_d) return nullptr;
    w = s_d->panelWidth();
    h = s_d->panelHeight();
    return s_d->frameBuffer(s_d->scanning());
}

uint32_t panelFramesScanned() { return s_d ? s_d->framesScanned() : 0; }

uint8_t softwareRotation() { return 0; }

// what=copy measures the P4's copy engines (PPA, DMA2D); the S3 has neither.
bool copyBench(CopyBench &out, int reps) {
    (void)out; (void)reps;
    return false;
}

} // namespace LVGL_Flush

#endif // DISPLAY_ESPLCD && HAS_RGB_PANEL
