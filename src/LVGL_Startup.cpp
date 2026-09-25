#include "LVGL_Startup.h"
#include <Arduino.h>
#include <Arduino_GFX_Library.h>
#include <esp_heap_caps.h>
#include <esp_memory_utils.h>   // esp_ptr_external_ram
#include <esp_timer.h>
#include "DisplayManager.h"
#include "TouchManager.h"
#include "SystemReport.h"   // fmtBytes - one memory-reporting convention
#include "bsp_loader.h"

namespace {

// Borrowed, not owned. Set by begin(); the callbacks below reach hardware
// through these rather than through lv_display_get_user_data(), which used to
// hand back a GUIManager* and put the screen-content class on the render path.
DisplayManager *s_display = nullptr;
TouchManager   *s_touch   = nullptr;

lv_display_t *s_disp      = nullptr;
lv_indev_t   *s_indev     = nullptr;
uint16_t     *s_draw_buf  = nullptr;
uint16_t     *s_draw_buf2 = nullptr;
LVGL_Startup::DrawBufInfo s_bufInfo;

// Attached only while GET /bench is measuring. See LVGL_Startup.h.
LVGL_Startup::FlushStats *s_flushStats = nullptr;

// Draw buffers must start on an LV_DRAW_BUF_ALIGN boundary, or
// lv_display_set_buffers() fails its alignment assert - which on this fleet is
// `while(1);`, a silent freeze at boot. Plain heap_caps_malloc() only promises
// 4 (internal) or 16 (PSRAM) bytes, which was enough while LV_DRAW_BUF_ALIGN
// was 4 and stops being enough the moment it is 64 (the PPA experiment, 2.9).
// The size is rounded up too, because a cache sync over a buffer wants whole
// cache lines.
void *allocDrawBuf(size_t &bytes, uint32_t caps) {
    constexpr size_t A = LV_DRAW_BUF_ALIGN;
    bytes = (bytes + A - 1) / A * A;
    return heap_caps_aligned_alloc(A, bytes, caps);
}

void disp_flush(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map) {
    Arduino_GFX *gfx = s_display->getGfx();

    #ifdef DEBUG_DISPLAY
    // Unconditional, every call - separate from the is_last-gated counter
    // below, so we can tell "flush never called at all" apart from
    // "called repeatedly but is_last never true" instead of just seeing
    // silence either way.
    static uint32_t callCount = 0;
    callCount++;
    if (callCount <= 10 || callCount % 200 == 0) {
        Serial.printf("[LVGL] disp_flush called #%lu: area (%d,%d)-(%d,%d) is_last=%d\n",
            (unsigned long)callCount, area->x1, area->y1, area->x2, area->y2,
            lv_display_flush_is_last(disp));
    }
    #endif

    // 1. Draw the chunk (Partial or Full)
    // Even in "Direct Mode" style usage with full buffers, we use this to copy
    // the pixels from our LVGL buffer into the Arduino_GFX internal driver.
    uint32_t w = (area->x2 - area->x1 + 1);
    uint32_t h = (area->y2 - area->y1 + 1);
    const int64_t tCopy = s_flushStats ? esp_timer_get_time() : 0;
    gfx->draw16bitRGBBitmap(area->x1, area->y1, (uint16_t *)px_map, w, h);
    if (s_flushStats) {
        s_flushStats->copyUs += esp_timer_get_time() - tCopy;
        s_flushStats->chunks++;
        s_flushStats->px += w * h;
    }

    // 2. The "Waveshare Logic" (Batching Optimization)
    // We check if this is the LAST chunk of the frame.
    // If it is, we tell the hardware to refresh. This prevents sending
    // partial frames to MIPI/RGB displays which can cause tearing or high bus overhead.
    if (lv_display_flush_is_last(disp)) {
        const int64_t tPresent = s_flushStats ? esp_timer_get_time() : 0;
        gfx->flush();
        if (s_flushStats) s_flushStats->presentUs += esp_timer_get_time() - tPresent;
        #ifdef DEBUG_DISPLAY
        static uint32_t frameCount = 0;
        frameCount++;
        if (frameCount <= 5 || frameCount % 100 == 0) {
            Serial.printf("[LVGL] flush: frame #%lu complete (last area %d,%d %dx%d)\n",
                (unsigned long)frameCount, area->x1, area->y1, w, h);
        }
        #endif
    }

    // 3. Notify LVGL we are done
    lv_display_flush_ready(disp);
}

void touch_read(lv_indev_t *indev, lv_indev_data_t *data) {
    TouchPoint points[5];
    uint8_t count = s_touch->read(points, 5);

    if (count > 0) {
        data->state = LV_INDEV_STATE_PRESSED;
        data->point.x = points[0].x;
        data->point.y = points[0].y;
    } else {
        data->state = LV_INDEV_STATE_RELEASED;
    }
}

uint32_t tick_get_cb(void) { return millis(); }

#if LV_USE_LOG
void lv_log_print(lv_log_level_t level, const char *buf) {
    Serial.print("[LVGL] ");
    Serial.println(buf);

    // FLUSH, because the most important message LVGL ever prints is the last
    // one before it stops.
    //
    // LV_ASSERT_HANDLER is `while(1);` (see docs/LESSONS.md), and both
    // LV_USE_ASSERT_NULL and LV_USE_ASSERT_MALLOC are on - so an allocation
    // failure logs and then hangs the board forever. Six of eight boards are
    // ARDUINO_USB_CDC_ON_BOOT=1, where Serial is a buffered USB endpoint:
    // without this, that final message is still sitting in the buffer when the
    // CPU stops, and the board looks like it froze in silence for no reason.
    //
    // That silence is exactly what made the 2026-09-18 CYD_S3_3248 Dump Config
    // freeze look causeless. Guarded on `if (Serial)` for the same reason as
    // the one in SystemReport::line(): on a CDC board, flushing with no host
    // attached waits for a host that will never drain it.
    if (Serial) Serial.flush();
}
#endif

} // namespace

namespace LVGL_Startup {

bool begin(DisplayManager &display, TouchManager &touch) {
    s_display = &display;
    s_touch   = &touch;

    // --= 1. Core init =--
    lv_init();
    #if LV_USE_LOG
    lv_log_register_print_cb(lv_log_print);
    #endif
    lv_tick_set_cb(tick_get_cb);

    // --= 2. Buffer allocation (ping-pong strategy) =--
    Arduino_GFX *gfx = display.getGfx();

    size_t   pixel_count  = 0;
    uint32_t malloc_flags = MALLOC_CAP_DMA; // Always need DMA capability

    // Case A: High Bandwidth Interfaces (MIPI / RGB)
    #if defined(HAS_MIPI_PANEL) || defined(HAS_RGB_PANEL)
        Serial.println("[LVGL] Config: High Bandwidth (MIPI/RGB)");

        // Use Full Frame Buffers in PSRAM.
        // Even though we use PARTIAL mode below, having the buffer be full size
        // allows LVGL to render huge chunks at once, and our 'is_last' check
        // ensures we sync with the display refresh rate.
        pixel_count = gfx->width() * gfx->height();
        malloc_flags |= MALLOC_CAP_SPIRAM;

        Serial.println("[LVGL] Strategy: Full Frame Buffers (PSRAM) + Frame Sync");

    // Case B: Bus Constrained Interfaces (SPI / QSPI)
    #else
        Serial.println("[LVGL] Config: Bus Constrained (SPI/QSPI)");

        // 1/10th screen size in Internal SRAM.
        pixel_count = (gfx->width() * gfx->height()) / 10;
        malloc_flags |= MALLOC_CAP_INTERNAL;

        Serial.println("[LVGL] Strategy: 1/10th Partial Buffers (Internal SRAM)");
    #endif

    // Override: If the BSP specified a draw buffer height, respect it (0 = no override)
    if (bsp_lvgl.DRAW_BUF_HEIGHT > 0) {
        pixel_count = gfx->width() * bsp_lvgl.DRAW_BUF_HEIGHT;
        Serial.println("[LVGL] Override: Using Custom Draw Buffer Height");
    }

    size_t byte_count = pixel_count * sizeof(uint16_t);
    char bbuf[48];
    Serial.printf("[LVGL] Allocating %s per buffer... ",
                  SystemReport::fmtBytes(byte_count, bbuf, sizeof(bbuf)));

    s_draw_buf = (uint16_t *)allocDrawBuf(byte_count, malloc_flags);

    // Fallback 1: If Internal failed, try PSRAM
    if (!s_draw_buf && (malloc_flags & MALLOC_CAP_INTERNAL)) {
        Serial.print(" (Internal Full! Retrying PSRAM)... ");
        malloc_flags &= ~MALLOC_CAP_INTERNAL;
        malloc_flags |= MALLOC_CAP_SPIRAM;
        s_draw_buf = (uint16_t *)allocDrawBuf(byte_count, malloc_flags);
    }
    // Fallback 2: anything 8-bit capable, still aligned
    if (!s_draw_buf) {
        Serial.print(" (Struct alloc failed! Retrying generic)... ");
        malloc_flags = MALLOC_CAP_8BIT;
        s_draw_buf = (uint16_t *)allocDrawBuf(byte_count, malloc_flags);
    }

    // Allocate Second Buffer - ONLY IF THE BOARD ASKED FOR ONE.
    //
    // This used to allocate unconditionally, which quietly ignored
    // bsp_lvgl.DOUBLE_BUFFERING. CYD_S3_3248 sets it FALSE and got two buffers
    // anyway: 2 x 30,720 bytes of INTERNAL SRAM, on the one board in the fleet
    // that cannot spare it - the only QSPI panel, so the only one whose draw
    // buffers must be internal rather than PSRAM.
    //
    // The cost was the whole evening. It booted with 10 KB of internal heap
    // free, the WiFi driver and LWIP had nothing to allocate sockets from, and
    // MQTT died at the keepalive every single time with "state=-3 ... heap
    // 5588". It looked like a leak and was not: the heap never trended down,
    // it simply sat at 2-8 KB, which is below what a TCP connection needs.
    // AND THE OWNER'S CAVEAT, WHICH GOES FURTHER THAN THIS FIX (2026-09-18):
    // "despite our configuration of double buffering in LVGL_Startup,
    // GFX_Library is essentially neutered and only uses 1 of the buffers, thus
    // eating available memory."
    //
    // If that holds, the second buffer is dead weight on ALL SEVEN of the
    // other boards too, not just the one that could not afford it - the P4s
    // are simply rich enough in PSRAM not to notice. Honouring the BSP flag is
    // as far as this change goes, because "GFX ignores buffer two" is a claim
    // about the library that should be confirmed against its source before
    // seven boards are changed on the strength of it. That confirmation is
    // milestone 2.9's job (Arduino_GFX -> esp_lcd), and it is now one of the
    // concrete things 2.9 buys rather than a general tidy-up.
    if (bsp_lvgl.DOUBLE_BUFFERING) {
        s_draw_buf2 = (uint16_t *)allocDrawBuf(byte_count, malloc_flags);
        if (!s_draw_buf2) s_draw_buf2 = (uint16_t *)allocDrawBuf(byte_count, MALLOC_CAP_8BIT);
    } else {
        s_draw_buf2 = nullptr;
        Serial.print(" (single-buffered, per BSP) ");
    }

    if (!s_draw_buf) {
        Serial.println("\n[LVGL] Critical: Failed to allocate ANY draw buffer!");
        return false;
    }
    Serial.println("Success.");
    s_bufInfo.bytes = byte_count;
    s_bufInfo.count = s_draw_buf2 ? 2 : 1;
    s_bufInfo.psram = esp_ptr_external_ram(s_draw_buf);

    // --= 3. Driver registration =--
    s_disp = lv_display_create(gfx->width(), gfx->height());
    lv_display_set_flush_cb(s_disp, disp_flush);

    // Note: We use PARTIAL mode. This is safer for Arduino_GFX.
    // If we used DIRECT mode, we would need to handle 'strided' memory writes manually
    // because Arduino_GFX expects contiguous bitmaps.
    lv_display_set_buffers(s_disp, s_draw_buf, s_draw_buf2, byte_count,
                           LV_DISPLAY_RENDER_MODE_PARTIAL);

    // --= DPI Awareness =--
    // LVGL uses this for lv_dpx() and its own default sizing, so it wants the
    // panel's REAL density, not a blanket figure. This used to be a flat 150
    // on the three -D HIGH_DPI_DISPLAY boards and LVGL's own default (130)
    // everywhere else - wrong by up to 96 PPI on WS_P4_5.
    {
        const uint16_t ppi = bspPixelDensity();
        if (ppi) {
            lv_display_set_dpi(s_disp, ppi);
            Serial.printf("[LVGL] Panel density: %u PPI (UI scale %.2fx)\n",
                          (unsigned)ppi, (double)bspUiScale());
        } else {
            Serial.println("[LVGL] Panel density unknown (no DIAGONAL_IN) - UI scale 1.00x");
        }
    }

    s_indev = lv_indev_create();
    lv_indev_set_type(s_indev, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(s_indev, touch_read);

    Serial.println("[LVGL] Engine started.");
    return true;
}

void tick() { lv_timer_handler(); }

// See the header for why these are no-ops and why they exist anyway.
bool lock(int timeout_ms) { (void)timeout_ms; return true; }
void unlock() {}

lv_display_t *display() { return s_disp; }
lv_indev_t   *indev()   { return s_indev; }

void attachFlushStats(FlushStats *stats) { s_flushStats = stats; }
DrawBufInfo drawBufInfo() { return s_bufInfo; }

} // namespace LVGL_Startup
