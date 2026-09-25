// LVGL_Flush, Arduino_GFX path - every board without -D DISPLAY_ESPLCD.
// See LVGL_Flush.h. Moved out of LVGL_Startup.cpp unchanged in behaviour at
// 2.9 step 2, so that the esp_lcd path could sit beside it in its own file.
#if !defined(DISPLAY_ESPLCD)

#include "LVGL_Flush.h"
#include <Arduino.h>
#include <Arduino_GFX_Library.h>
#include <esp_memory_utils.h>   // esp_ptr_external_ram
#include <esp_timer.h>
#include "SystemReport.h"       // fmtBytes - one memory-reporting convention
#include "bsp_loader.h"

namespace {

// Borrowed, not owned.
DisplayManager *s_display   = nullptr;
uint16_t       *s_draw_buf  = nullptr;
uint16_t       *s_draw_buf2 = nullptr;

void disp_flush(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map) {
    Arduino_GFX *gfx = s_display->getGfx();
    LVGL_Startup::FlushStats *stats = LVGL_Startup::activeFlushStats();

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
    const int64_t tCopy = stats ? esp_timer_get_time() : 0;
    gfx->draw16bitRGBBitmap(area->x1, area->y1, (uint16_t *)px_map, w, h);
    if (stats) {
        stats->copyUs += esp_timer_get_time() - tCopy;
        stats->chunks++;
        stats->px += w * h;
    }

    // 2. The "Waveshare Logic" (Batching Optimization)
    // We check if this is the LAST chunk of the frame.
    // If it is, we tell the hardware to refresh. This prevents sending
    // partial frames to MIPI/RGB displays which can cause tearing or high bus overhead.
    if (lv_display_flush_is_last(disp)) {
        const int64_t tPresent = stats ? esp_timer_get_time() : 0;
        gfx->flush();
        if (stats) stats->presentUs += esp_timer_get_time() - tPresent;
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

} // namespace

namespace LVGL_Flush {

lv_display_t *create(BoardDisplay &display, LVGL_Startup::DrawBufInfo &info) {
    s_display = &display;

    // --= Buffer allocation (ping-pong strategy) =--
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
    //
    // 2.9 confirmed the owner's caveat from source: this path's flush is
    // synchronous, so the second buffer buys nothing on any board
    // (docs/design/display-stack.md §2). It stays honoured, not removed, until
    // each board moves to the esp_lcd path.
    if (bsp_lvgl.DOUBLE_BUFFERING) {
        s_draw_buf2 = (uint16_t *)allocDrawBuf(byte_count, malloc_flags);
        if (!s_draw_buf2) s_draw_buf2 = (uint16_t *)allocDrawBuf(byte_count, MALLOC_CAP_8BIT);
    } else {
        s_draw_buf2 = nullptr;
        Serial.print(" (single-buffered, per BSP) ");
    }

    if (!s_draw_buf) {
        Serial.println("\n[LVGL] Critical: Failed to allocate ANY draw buffer!");
        return nullptr;
    }
    Serial.println("Success.");
    info.bytes = byte_count;
    info.count = s_draw_buf2 ? 2 : 1;
    info.psram = esp_ptr_external_ram(s_draw_buf);

    // --= Driver registration =--
    lv_display_t *disp = lv_display_create(gfx->width(), gfx->height());
    lv_display_set_flush_cb(disp, disp_flush);

    // Note: We use PARTIAL mode. This is safer for Arduino_GFX.
    // If we used DIRECT mode, we would need to handle 'strided' memory writes manually
    // because Arduino_GFX expects contiguous bitmaps.
    lv_display_set_buffers(disp, s_draw_buf, s_draw_buf2, byte_count,
                           LV_DISPLAY_RENDER_MODE_PARTIAL);
    return disp;
}

} // namespace LVGL_Flush

#endif // !DISPLAY_ESPLCD
