#include "LVGL_Startup.h"
#include "LVGL_Flush.h"     // how pixels reach the panel - one file per display library
#include <Arduino.h>
#include "TouchManager.h"
#include "bsp_loader.h"

namespace {

// Borrowed, not owned. Set by begin(); the touch callback reaches hardware
// through this rather than through lv_display_get_user_data(), which used to
// hand back a GUIManager* and put the screen-content class on the render path.
TouchManager   *s_touch   = nullptr;

lv_display_t *s_disp      = nullptr;
lv_indev_t   *s_indev     = nullptr;
LVGL_Startup::DrawBufInfo s_bufInfo;

// Attached only while GET /bench is measuring. See LVGL_Startup.h.
LVGL_Startup::FlushStats *s_flushStats = nullptr;

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

bool begin(BoardDisplay &display, TouchManager &touch) {
    s_touch   = &touch;

    // --= 1. Core init =--
    lv_init();
    #if LV_USE_LOG
    lv_log_register_print_cb(lv_log_print);
    #endif
    lv_tick_set_cb(tick_get_cb);

    // --= 2. The display: size, draw buffers, flush =--
    // LVGL_Flush_Gfx.cpp or LVGL_Flush_EspLcd.cpp, whichever this board builds.
    s_disp = LVGL_Flush::create(display, s_bufInfo);
    if (!s_disp) {
        Serial.println("[LVGL] Critical: display creation failed");
        return false;
    }

    // THE OVERLAY LAYERS MUST NOT SCROLL. #68.
    //
    // LVGL creates the top and system layers scrollable - it only removes
    // CLICKABLE (lv_display.c:159-172). Drag anything on the top layer (the
    // Touch Points panel is draggable) and LVGL also tries to SCROLL: the
    // panel cannot, so the scroll passes up to its parent, the layer, which
    // can. The layer then scrolls, carrying every child with it. Measured on
    // both dev boards, 2026-09-26: top layer scrolled (0,-144) on WS_P4_5 and
    // (-33,-236) on CYD_S3_3248, the toast's own alignment exactly right and
    // the toast visibly "attached" to the panel. It only happened when the
    // layer had room to scroll in the drag's direction, which is why it looked
    // random. An overlay is a sheet of glass over the screen; it has nothing
    // to scroll to.
    lv_obj_remove_flag(lv_display_get_layer_top(s_disp), LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_remove_flag(lv_display_get_layer_sys(s_disp), LV_OBJ_FLAG_SCROLLABLE);

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
FlushStats *activeFlushStats() { return s_flushStats; }
DrawBufInfo drawBufInfo() { return s_bufInfo; }

} // namespace LVGL_Startup
