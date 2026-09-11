#include "LVGL_Startup.h"
#include <Arduino.h>
#include <Arduino_GFX_Library.h>
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
    gfx->draw16bitRGBBitmap(area->x1, area->y1, (uint16_t *)px_map, w, h);

    // 2. The "Waveshare Logic" (Batching Optimization)
    // We check if this is the LAST chunk of the frame.
    // If it is, we tell the hardware to refresh. This prevents sending
    // partial frames to MIPI/RGB displays which can cause tearing or high bus overhead.
    if (lv_display_flush_is_last(disp)) {
        gfx->flush();
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

    s_draw_buf = (uint16_t *)heap_caps_malloc(byte_count, malloc_flags);

    // Fallback 1: If Internal failed, try PSRAM
    if (!s_draw_buf && (malloc_flags & MALLOC_CAP_INTERNAL)) {
        Serial.print(" (Internal Full! Retrying PSRAM)... ");
        malloc_flags &= ~MALLOC_CAP_INTERNAL;
        malloc_flags |= MALLOC_CAP_SPIRAM;
        s_draw_buf = (uint16_t *)heap_caps_malloc(byte_count, malloc_flags);
    }
    // Fallback 2: Generic Malloc
    if (!s_draw_buf) {
        Serial.print(" (Struct alloc failed! Retrying generic)... ");
        s_draw_buf = (uint16_t *)malloc(byte_count);
    }

    // Allocate Second Buffer (Double Buffering)
    s_draw_buf2 = (uint16_t *)heap_caps_malloc(byte_count, malloc_flags);
    if (!s_draw_buf2) s_draw_buf2 = (uint16_t *)malloc(byte_count);

    if (!s_draw_buf) {
        Serial.println("\n[LVGL] Critical: Failed to allocate ANY draw buffer!");
        return false;
    }
    Serial.println("Success.");

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

} // namespace LVGL_Startup
