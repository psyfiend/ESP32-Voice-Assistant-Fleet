// LVGL_Flush, esp_lcd path - boards with -D DISPLAY_ESPLCD (WS_P4_5 from
// 2.9 step 2). See LVGL_Flush.h, and docs/design/esplcd-step2.md §3 for the
// sequence this implements. esp_lvgl_adapter 0.6.4 (Apache-2.0,
// reference/esp-registry/) was read as the reference; no code was copied.
//
// THE SHAPE, per frame:
//
//   first chunk   pick a frame buffer the panel is neither showing nor about
//                 to show, and REPAIR it: copy in, from the newest frame, every
//                 area it is behind on (it last held a frame two presents ago)
//   every chunk   rotate LVGL's unrotated chunk into that buffer with the PPA
//   last chunk    hand the buffer to the panel, which switches to it at the
//                 start of its next frame - no copy, no tearing
//
// With three buffers there is always one free, so LVGL never waits for the
// panel. /bench reports the rotation as `copy` and repair + hand-over as
// `present`; `wait` stays ~0.
//
#if defined(DISPLAY_ESPLCD)

#include "LVGL_Flush.h"
#include <Arduino.h>
#include <esp_memory_utils.h>   // esp_ptr_external_ram
#include <esp_timer.h>
#include "esp_attr.h"             // IRAM_ATTR
#include "esp_cache.h"
#include "driver/ppa.h"
#include "SystemReport.h"       // fmtBytes
#include "bsp_loader.h"
#include "src/display/lv_display_private.h"   // inv_areas: what this refresh redraws

namespace {

Fleet_Display       *s_d   = nullptr;
ppa_client_handle_t  s_ppa = nullptr;
uint8_t  s_rot = 0;                  // BSP rotation, 0-3 (Arduino_GFX's meaning, kept)
int32_t  s_pw = 0, s_ph = 0;         // physical (panel) size

// --= What each frame buffer is behind on =--
//
// Physical coordinates. A frame drawn into buffer T leaves the other two
// behind by exactly the areas that frame changed. Up to MAX_RECTS areas are
// kept per buffer; past that, the buffer is simply marked wholly stale and
// repaired with one full-frame copy - slower, never wrong.
constexpr uint8_t MAX_RECTS = 8;
struct Stale {
    lv_area_t r[MAX_RECTS];
    uint8_t   n   = 0;
    bool      all = true;     // at boot, every buffer is behind on everything
};
Stale s_stale[Fleet_Display::NUM_FBS];
// What the frame being drawn redraws: LVGL's own invalidated areas for this
// refresh (not the 50-line strips it cuts them into for us - a full-screen
// redraw is one area, not fifteen), converted to physical.
Stale s_now;
bool    s_inFrame = false;
uint8_t s_target  = 0;

void staleClear(Stale &s) { s.n = 0; s.all = false; }
void staleAdd(Stale &s, const lv_area_t &a) {
    if (s.all) return;
    if (s.n < MAX_RECTS) s.r[s.n++] = a;
    else s.all = true;
}
void staleMerge(Stale &into, const Stale &from) {
    if (from.all) { into.all = true; return; }
    for (uint8_t i = 0; i < from.n; i++) staleAdd(into, from.r[i]);
}

// Logical (as LVGL sees it) -> physical (as the panel is wired). Mirrors
// Arduino_GFX's own mapping for the same ROTATION value
// (Arduino_DSI_Display.cpp:83-86: rotation 1 puts logical (x,y) at physical
// column pw-1-y, row x), so a board looks the same on either path. Equal to
// esp_lvgl_adapter's ROTATE_90/180/270 (lvgl_bridge_v9.c:2832-2851).
lv_area_t toPhysical(const lv_area_t &a) {
    lv_area_t p;
    switch (s_rot) {
    case 1: p.x1 = s_pw - 1 - a.y2; p.x2 = s_pw - 1 - a.y1; p.y1 = a.x1; p.y2 = a.x2; break;
    case 2: p.x1 = s_pw - 1 - a.x2; p.x2 = s_pw - 1 - a.x1; p.y1 = s_ph - 1 - a.y2; p.y2 = s_ph - 1 - a.y1; break;
    case 3: p.x1 = a.y1; p.x2 = a.y2; p.y1 = s_ph - 1 - a.x2; p.y2 = s_ph - 1 - a.x1; break;
    default: p = a; break;
    }
    return p;
}

// The PPA counts angles counter-clockwise; our rotations are clockwise.
ppa_srm_rotation_angle_t ppaAngle() {
    switch (s_rot) {
    case 1:  return PPA_SRM_ROTATION_ANGLE_270;
    case 2:  return PPA_SRM_ROTATION_ANGLE_180;
    case 3:  return PPA_SRM_ROTATION_ANGLE_90;
    default: return PPA_SRM_ROTATION_ANGLE_0;
    }
}

// A queued rotation has finished: release LVGL's draw buffer, so LVGL can
// flush the strip it has been drawing meanwhile. lv_display_flush_ready() is
// this single store (lv_display.c:656-659), written here directly so the
// interrupt calls nothing in flash.
bool IRAM_ATTR onPpaDone(ppa_client_handle_t client, ppa_event_data_t *ev, void *user) {
    (void)client; (void)ev;
    if (user) static_cast<lv_display_t *>(user)->flushing = 0;
    return false;
}

// One PPA block transfer into a frame buffer.
//   blocking            returns when the pixels are there
//   non-blocking        queued; returns at once. The PPA runs one client's
//                       jobs in order, so a later job never overtakes this one.
//                       `doneFor`, if set, is released when it finishes.
esp_err_t ppaBlit(const void *src, uint32_t srcW, uint32_t srcH,
                  uint32_t blkX, uint32_t blkY, uint32_t blkW, uint32_t blkH,
                  void *dst, uint32_t dstX, uint32_t dstY, ppa_srm_rotation_angle_t angle,
                  bool blocking = true, lv_display_t *doneFor = nullptr) {
    ppa_srm_oper_config_t op = {};
    op.in.buffer         = src;
    op.in.pic_w          = srcW;
    op.in.pic_h          = srcH;
    op.in.block_w        = blkW;
    op.in.block_h        = blkH;
    op.in.block_offset_x = blkX;
    op.in.block_offset_y = blkY;
    op.in.srm_cm         = PPA_SRM_COLOR_MODE_RGB565;
    op.out.buffer         = dst;
    op.out.buffer_size    = s_d->frameBufferBytes();
    op.out.pic_w          = s_pw;
    op.out.pic_h          = s_ph;
    op.out.block_offset_x = dstX;
    op.out.block_offset_y = dstY;
    op.out.srm_cm         = PPA_SRM_COLOR_MODE_RGB565;
    op.rotation_angle = angle;
    op.scale_x = 1.0f;
    op.scale_y = 1.0f;
    op.mode      = blocking ? PPA_TRANS_MODE_BLOCKING : PPA_TRANS_MODE_NON_BLOCKING;
    op.user_data = doneFor;
    return ppa_do_scale_rotate_mirror(s_ppa, &op);
}

// A buffer the panel is neither scanning nor about to scan. With three there
// is always one; see Fleet_Display::present() for why reading the two
// indices without a lock is safe.
uint8_t freeBuffer() {
    const uint8_t a = s_d->scanning(), b = s_d->submitted();
    for (uint8_t i = 0; i < Fleet_Display::NUM_FBS; i++) {
        if (i != a && i != b) return i;
    }
    return (uint8_t)((b + 1) % Fleet_Display::NUM_FBS);   // unreachable with 3
}

// LVGL's list of areas this refresh redraws. Still intact while the refresh
// is flushing: lv_refr.c clears it only after the last area is drawn. The
// same list esp_lvgl_adapter reads for the same purpose.
void captureRedraw(lv_display_t *disp) {
    staleClear(s_now);
    for (uint32_t i = 0; i < disp->inv_p; i++) {
        if (disp->inv_area_joined[i]) continue;   // merged into another entry
        staleAdd(s_now, toPhysical(disp->inv_areas[i]));
    }
}

bool contains(const lv_area_t &outer, const lv_area_t &inner) {
    return inner.x1 >= outer.x1 && inner.y1 >= outer.y1 &&
           inner.x2 <= outer.x2 && inner.y2 <= outer.y2;
}

// True when this frame will redraw all of `r` anyway, so repairing it first
// would be wasted work. Conservative: only a single area that wholly covers
// `r` counts; anything less, and `r` is repaired.
bool redrawnAnyway(const lv_area_t &r) {
    if (s_now.all) return false;
    for (uint8_t i = 0; i < s_now.n; i++) {
        if (contains(s_now.r[i], r)) return true;
    }
    return false;
}

// First chunk of a frame: choose the buffer, and bring it up to date from the
// newest complete frame (the one last handed to the panel) - except where
// this frame is about to redraw anyway.
void beginFrame(lv_display_t *disp) {
    captureRedraw(disp);
    s_target = freeBuffer();
    Stale &st = s_stale[s_target];
    const uint8_t src = s_d->submitted();
    void *from = s_d->frameBuffer(src);
    void *to   = s_d->frameBuffer(s_target);

    if (st.all) {
        const lv_area_t whole = {0, 0, s_pw - 1, s_ph - 1};
        if (!redrawnAnyway(whole)) {
            ppaBlit(from, s_pw, s_ph, 0, 0, s_pw, s_ph, to, 0, 0, PPA_SRM_ROTATION_ANGLE_0,
                    /*blocking*/ false);
        }
    } else {
        for (uint8_t i = 0; i < st.n; i++) {
            const lv_area_t &r = st.r[i];
            if (redrawnAnyway(r)) continue;
            // Queued, not waited for: the rotations queued after it cannot
            // start until it is done.
            ppaBlit(from, s_pw, s_ph, r.x1, r.y1, r.x2 - r.x1 + 1, r.y2 - r.y1 + 1,
                    to, r.x1, r.y1, PPA_SRM_ROTATION_ANGLE_0, /*blocking*/ false);
        }
    }
    staleClear(st);
    s_inFrame = true;
}

// Last chunk: the other two buffers are now behind by what this frame drew;
// hand this one to the panel.
void endFrame() {
    for (uint8_t i = 0; i < Fleet_Display::NUM_FBS; i++) {
        if (i != s_target) staleMerge(s_stale[i], s_now);
    }
    s_d->present(s_target);
    s_inFrame = false;
}

void disp_flush(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map) {
    LVGL_Startup::FlushStats *stats = LVGL_Startup::activeFlushStats();
    const bool last = lv_display_flush_is_last(disp);

    if (!s_inFrame) {
        const int64_t t = stats ? esp_timer_get_time() : 0;
        beginFrame(disp);
        if (stats) stats->presentUs += esp_timer_get_time() - t;
    }

    // Rotate this chunk into the target buffer. The PPA reads memory, not
    // the CPU cache, so LVGL's freshly drawn pixels are written back first.
    const int64_t tCopy = stats ? esp_timer_get_time() : 0;
    const uint32_t w = (uint32_t)(area->x2 - area->x1 + 1);
    const uint32_t h = (uint32_t)(area->y2 - area->y1 + 1);
    const uint32_t stridePx = lv_draw_buf_width_to_stride(w, LV_COLOR_FORMAT_RGB565) / 2;
    esp_cache_msync(px_map, (size_t)stridePx * 2 * h,
                    ESP_CACHE_MSYNC_FLAG_DIR_C2M | ESP_CACHE_MSYNC_FLAG_UNALIGNED);
    const lv_area_t p = toPhysical(*area);
    void *fb = s_d->frameBuffer(s_target);

    // OVERLAP. Every strip but the last is only QUEUED: LVGL draws the next
    // strip into its other draw buffer while the PPA rotates this one, and
    // onPpaDone releases this buffer when the rotation is done. The last strip
    // is WAITED for - and since the PPA runs a client's jobs in order, so is
    // everything queued before it - because the panel must never be handed a
    // buffer that is still being written. A job the queue refuses is done
    // blocking instead, so LVGL is never left waiting on one that never ran.
    bool queued = false;
    if (!last) {
        queued = ppaBlit(px_map, stridePx, h, 0, 0, w, h, fb, p.x1, p.y1, ppaAngle(),
                         /*blocking*/ false, disp) == ESP_OK;
    }
    if (!queued) ppaBlit(px_map, stridePx, h, 0, 0, w, h, fb, p.x1, p.y1, ppaAngle());
    if (stats) {
        stats->copyUs += esp_timer_get_time() - tCopy;   // queued strips: the time to queue
        stats->chunks++;
        stats->px += w * h;
    }

    if (last) {
        const int64_t t = stats ? esp_timer_get_time() : 0;
        endFrame();
        if (stats) stats->presentUs += esp_timer_get_time() - t;
    }

    if (!queued) lv_display_flush_ready(disp);
}

} // namespace

namespace LVGL_Flush {

lv_display_t *create(BoardDisplay &display, LVGL_Startup::DrawBufInfo &info) {
    s_d   = &display;
    s_rot = (uint8_t)(bsp_display.ROTATION & 3);
    s_pw  = display.panelWidth();
    s_ph  = display.panelHeight();

    ppa_client_config_t pc = {};
    pc.oper_type = PPA_OPERATION_SRM;
    // Room for a frame's repairs (up to MAX_RECTS) plus the strips in flight.
    pc.max_pending_trans_num = MAX_RECTS + 4;
    // The longest DMA burst, as LVGL's own PPA code uses (lv_draw_ppa.c:51-52,
    // LV_PPA_BURST_LENGTH 128). The default measured barely faster than the CPU.
    pc.data_burst_length = PPA_DATA_BURST_LENGTH_128;
    if (ppa_register_client(&pc, &s_ppa) != ESP_OK) {
        Serial.println("[LVGL] PPA client refused");
        return nullptr;
    }
    ppa_event_callbacks_t cbs = {};
    cbs.on_trans_done = onPpaDone;
    if (ppa_client_register_event_callbacks(s_ppa, &cbs) != ESP_OK) {
        Serial.println("[LVGL] PPA callback refused");
        return nullptr;
    }

    // LVGL's logical size: the panel's, turned by the rotation.
    const bool swap = (s_rot & 1);
    const int32_t lw = swap ? s_ph : s_pw;
    const int32_t lh = swap ? s_pw : s_ph;

    // Draw buffers: BSP height (50 lines on WS_P4_5) of the LOGICAL width, in
    // PSRAM, aligned for the PPA and the cache (LV_DRAW_BUF_ALIGN is 64 on
    // this path - lv_conf.h). A second one buys nothing while the flush is
    // synchronous; honoured anyway, as on the Arduino_GFX path.
    const uint32_t lines = bsp_lvgl.DRAW_BUF_HEIGHT > 0 ? bsp_lvgl.DRAW_BUF_HEIGHT : 50;
    size_t bytes = (size_t)lw * lines * 2;
    char bbuf[48];
    Serial.printf("[LVGL] esp_lcd path: %ldx%ld logical, rotation %u, draw buffers %s x %u\n",
                  (long)lw, (long)lh, (unsigned)s_rot,
                  SystemReport::fmtBytes(bytes, bbuf, sizeof(bbuf)),
                  bsp_lvgl.DOUBLE_BUFFERING ? 2u : 1u);
    // DMA | SPIRAM, 64-aligned: what the PPA's DMA needs, per the rules
    // recorded from the Allsky/NINA author's working PPA code and LVGL's own
    // (docs/research/display-stack-migration.md, "Buffer-requirements summary").
    const uint32_t caps = MALLOC_CAP_DMA | MALLOC_CAP_SPIRAM;
    void *b1 = allocDrawBuf(bytes, caps);
    void *b2 = bsp_lvgl.DOUBLE_BUFFERING ? allocDrawBuf(bytes, caps) : nullptr;
    if (!b1) {
        Serial.println("[LVGL] Critical: no PSRAM for a draw buffer");
        return nullptr;
    }
    info.bytes = bytes;
    info.count = b2 ? 2 : 1;
    info.psram = esp_ptr_external_ram(b1);

    lv_display_t *disp = lv_display_create(lw, lh);
    lv_display_set_flush_cb(disp, disp_flush);
    lv_display_set_buffers(disp, b1, b2, bytes, LV_DISPLAY_RENDER_MODE_PARTIAL);
    return disp;
}

const void *shownFrameBuffer(uint32_t &w, uint32_t &h) {
    if (!s_d) return nullptr;
    w = (uint32_t)s_pw;
    h = (uint32_t)s_ph;
    void *fb = s_d->frameBuffer(s_d->submitted());
    // The PPA wrote it behind the CPU's back: drop any cached lines so the
    // CPU reads memory. The CPU never writes these buffers, so nothing dirty
    // is lost.
    esp_cache_msync(fb, s_d->frameBufferBytes(), ESP_CACHE_MSYNC_FLAG_DIR_M2C);
    return fb;
}

} // namespace LVGL_Flush

#endif // DISPLAY_ESPLCD
