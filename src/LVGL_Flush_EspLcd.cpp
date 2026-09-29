// LVGL_Flush, esp_lcd path - boards with -D DISPLAY_ESPLCD (WS_P4_5 from
// 2.9 step 2, WS_P4_4B from step 3). See LVGL_Flush.h, and docs/design/esplcd-step2.md §3 for the
// sequence this implements. esp_lvgl_adapter 0.6.4 (Apache-2.0,
// reference/esp-registry/) was read as the reference; no code was copied.
//
// THE SHAPE, per frame:
//
//   first chunk   pick a frame buffer the panel is neither showing nor about
//                 to show, and REPAIR it: copy in, from the newest frame, every
//                 area it is behind on (it last held a frame two presents ago)
//                 LESS what this frame redraws - on the DMA2D copier, by a
//                 worker on core 0, while LVGL draws (display-stack.md s8.10)
//   every chunk   rotate LVGL's unrotated chunk into that buffer with the PPA
//   last chunk    wait for the repair list, then hand the buffer to the panel,
//                 which switches to it at the start of its next frame - no
//                 copy, no tearing
//
// With three buffers there is always one free, so LVGL never waits for the
// panel. This file IS the TRIPLE_PARTIAL present mode (Fleet_BSP.h), the
// MIPI-DSI boards' flush; the RGB boards' is LVGL_Flush_EspLcdDirect.cpp
// (DOUBLE_DIRECT). /bench reports the rotation as `copy` and repair +
// hand-over as `present`; `wait` stays ~0. /bench?what=verify checks the
// result against LVGL's own render.
//
#if defined(DISPLAY_ESPLCD) && defined(HAS_MIPI_PANEL)

#include "LVGL_Flush.h"
#include <Arduino.h>
#include <esp_memory_utils.h>   // esp_ptr_external_ram
#include <esp_timer.h>
#include "esp_attr.h"             // IRAM_ATTR
#include "esp_cache.h"
#include "driver/ppa.h"
#include <string.h>               // memcpy, for copyBench
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include "esp_async_fbcpy_priv.h" // DMA2D frame-buffer copy (IDF-private API)
#include "SystemReport.h"       // fmtBytes
#include "bsp_loader.h"
#include "src/display/lv_display_private.h"   // inv_areas: what this refresh redraws
#include "src/misc/lv_area_private.h"         // lv_area_diff

namespace {

Fleet_Display       *s_d   = nullptr;
ppa_client_handle_t  s_ppa = nullptr;
uint8_t  s_rot = 0;                  // BSP rotation, 0-3 (Arduino_GFX's meaning, kept)
int32_t  s_pw = 0, s_ph = 0;         // physical (panel) size

// --= What each frame buffer is behind on =--
//
// Physical coordinates. A frame drawn into buffer T leaves the other two
// behind by exactly the areas that frame changed. Up to MAX_RECTS areas are
// kept per buffer. An area that overlaps or touches one already kept is
// MERGED into it (their bounding box), and past MAX_RECTS a new area merges
// into whichever kept one grows least - so a buffer is only ever wholly stale
// at boot. A merged area can cover pixels that did not change: copying them
// is wasted work, never wrong.
//
// It used to go wholly stale past 8 areas instead. During a deck-panel
// animation - 3 areas a frame, a buffer 2-3 frames behind - that meant a
// whole-frame repair every few frames, ~33 ms of PPA each
// (/bench?what=anim, 2026-09-26).
constexpr uint8_t MAX_RECTS = 16;
struct Stale {
    lv_area_t r[MAX_RECTS];
    uint8_t   n   = 0;
    bool      all = true;     // at boot, every buffer is behind on everything
};
Stale s_stale[Fleet_Display::MAX_FBS];
// What the frame being drawn redraws: LVGL's own invalidated areas for this
// refresh (not the 50-line strips it cuts them into for us - a full-screen
// redraw is one area, not fifteen), converted to physical. EXACT, never
// merged: the repair skips whatever these cover, so an enlarged area here
// would leave pixels unrepaired. LV_INV_BUF_SIZE is LVGL's own limit - past
// it, LVGL redraws the whole screen as one area.
lv_area_t s_now[LV_INV_BUF_SIZE];
uint8_t   s_nowN = 0;
bool    s_inFrame = false;
uint8_t s_target  = 0;

bool touchOrOverlap(const lv_area_t &a, const lv_area_t &b) {
    return a.x1 <= b.x2 + 1 && b.x1 <= a.x2 + 1 && a.y1 <= b.y2 + 1 && b.y1 <= a.y2 + 1;
}
lv_area_t boundingBox(const lv_area_t &a, const lv_area_t &b) {
    return {LV_MIN(a.x1, b.x1), LV_MIN(a.y1, b.y1), LV_MAX(a.x2, b.x2), LV_MAX(a.y2, b.y2)};
}

void staleClear(Stale &s) { s.n = 0; s.all = false; }
void staleAdd(Stale &s, const lv_area_t &a) {
    if (s.all) return;
    lv_area_t m = a;
    // Absorb every kept area this one overlaps or touches; the result may now
    // touch others, so start over after each absorption.
    for (uint8_t i = 0; i < s.n;) {
        if (touchOrOverlap(s.r[i], m)) {
            m = boundingBox(s.r[i], m);
            s.r[i] = s.r[--s.n];
            i = 0;
        } else {
            i++;
        }
    }
    if (s.n < MAX_RECTS) { s.r[s.n++] = m; return; }
    // Full: merge into the one kept area whose bounding box grows least.
    uint8_t  best = 0;
    uint32_t bestGrow = UINT32_MAX;
    for (uint8_t i = 0; i < s.n; i++) {
        const lv_area_t bb = boundingBox(s.r[i], m);
        const uint32_t grow = (uint32_t)lv_area_get_size(&bb) - (uint32_t)lv_area_get_size(&s.r[i]);
        if (grow < bestGrow) { bestGrow = grow; best = i; }
    }
    s.r[best] = boundingBox(s.r[best], m);
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

// esp_async_fbcpy finished (DMA2D interrupt): wake whoever waits on `sem`.
bool IRAM_ATTR onFbcpyDone(esp_async_fbcpy_handle_t mcp, esp_async_fbcpy_event_data_t *ev, void *sem) {
    (void)mcp; (void)ev;
    BaseType_t woken = pdFALSE;
    xSemaphoreGiveFromISR(static_cast<SemaphoreHandle_t>(sem), &woken);
    return woken == pdTRUE;
}

// --= Repairs on the DMA2D copier (esp_async_fbcpy) =--
//
// Measured 2026-09-26 (/bench?what=copy): the DMA2D copier moves frame-buffer
// memory at ~140-150 MB/s where the PPA manages ~45-50, on both boards - and
// it runs on its own DMA2D channels, so repairs now run ALONGSIDE the PPA's
// strip rotations. That is safe because a repair covers only what this frame
// does NOT redraw (repairArea subtracts the redraw), while every strip lies
// inside what it does redraw: the two never write the same pixel.
//
// AT MOST ONE COPY OUTSTANDING, ANYWHERE. esp_async_fbcpy() hands the DMA2D
// driver a pointer to ONE static transaction config shared by every handle
// (esp_lcd/src/esp_async_fbcpy.c at IDF 2553c5ad432), and the driver reads it
// only when a queued job STARTS. With several copies queued, each start sees
// the LAST caller's handle: some pieces copied twice, others never. Found
// 2026-09-26 as stale pixels on WS_P4_5 when this ran a pool of 16 handles
// (esp_async_fbcpy_priv.h). Espressif's own DPI driver uses one handle, one
// copy at a time, which is why it never meets it.
//
// So a frame's repair pieces go on a list, and one worker task on core 0 -
// idle while LVGL draws on core 1 (display-stack.md s8.3) - copies them one
// after another on a single handle. They still run alongside the PPA's strip
// rotations. endFrame() waits for the whole list: the panel is never handed a
// buffer that is still being written.
constexpr uint8_t REPAIR_QUEUE_MAX = 64;
lv_area_t   s_rq[REPAIR_QUEUE_MAX];
uint8_t     s_rqN      = 0;
const void *s_rqFrom   = nullptr;
void       *s_rqTo     = nullptr;
bool        s_rqActive = false;      // the worker holds this frame's list
esp_async_fbcpy_handle_t s_fbc = nullptr;
SemaphoreHandle_t s_rqStart = nullptr, s_rqDone = nullptr, s_rqPiece = nullptr;

esp_err_t ppaBlit(const void *src, uint32_t srcW, uint32_t srcH,
                  uint32_t blkX, uint32_t blkY, uint32_t blkW, uint32_t blkH,
                  void *dst, uint32_t dstX, uint32_t dstY, ppa_srm_rotation_angle_t angle,
                  bool blocking = true, lv_display_t *doneFor = nullptr);

// The worker. The list is written by the LVGL thread before it gives
// s_rqStart and not touched again until s_rqDone comes back.
void repairWorker(void *) {
    for (;;) {
        xSemaphoreTake(s_rqStart, portMAX_DELAY);
        for (uint8_t i = 0; i < s_rqN; i++) {
            const lv_area_t &r = s_rq[i];
            const uint32_t w = (uint32_t)(r.x2 - r.x1 + 1), h = (uint32_t)(r.y2 - r.y1 + 1);
            esp_async_fbcpy_trans_desc_t tr = {};
            tr.src_buffer = s_rqFrom;
            tr.dst_buffer = s_rqTo;
            tr.src_buffer_size_x = tr.dst_buffer_size_x = (size_t)s_pw;
            tr.src_buffer_size_y = tr.dst_buffer_size_y = (size_t)s_ph;
            tr.src_offset_x = tr.dst_offset_x = (size_t)r.x1;
            tr.src_offset_y = tr.dst_offset_y = (size_t)r.y1;
            tr.copy_size_x = w;
            tr.copy_size_y = h;
            tr.pixel_format_unique_id.color_type_id = COLOR_TYPE_ID(COLOR_SPACE_RGB, COLOR_PIXEL_RGB565);
            if (esp_async_fbcpy(s_fbc, &tr, onFbcpyDone, s_rqPiece) == ESP_OK) {
                if (xSemaphoreTake(s_rqPiece, pdMS_TO_TICKS(500)) == pdTRUE) continue;
                // A copy that never finished: say so; the piece may be stale.
                Serial.println("[LVGL] repair copy did not finish within 500 ms");
                continue;
            }
            // Refused: the PPA does this piece instead (blocking, from here -
            // PPA clients are thread-safe, and this area is disjoint from any
            // strip the LVGL thread is rotating).
            ppaBlit(s_rqFrom, s_pw, s_ph, r.x1, r.y1, w, h, s_rqTo, r.x1, r.y1, PPA_SRM_ROTATION_ANGLE_0);
        }
        xSemaphoreGive(s_rqDone);
    }
}

void repairFrameBegin(const void *from, void *to) {
    s_rqN = 0;
    s_rqFrom = from;
    s_rqTo = to;
}

// Put one piece on the frame's list. False when there is no worker or the
// list is full - the caller then uses the PPA.
bool repairQueue(const lv_area_t &r) {
    if (!s_fbc || s_rqN >= REPAIR_QUEUE_MAX) return false;
    s_rq[s_rqN++] = r;
    return true;
}

// Hand the list to the worker (end of beginFrame).
void repairFrameStart() {
    if (!s_rqN) return;
    s_rqActive = true;
    xSemaphoreGive(s_rqStart);
}

// Wait for the list (endFrame, before the panel sees the buffer).
void repairFrameEnd() {
    if (!s_rqActive) return;
    if (xSemaphoreTake(s_rqDone, pdMS_TO_TICKS(1000)) != pdTRUE) {
        Serial.println("[LVGL] repair list did not finish within 1 s");
    }
    s_rqActive = false;
}

// One PPA block transfer into a frame buffer.
//   blocking            returns when the pixels are there
//   non-blocking        queued; returns at once. The PPA runs one client's
//                       jobs in order, so a later job never overtakes this one.
//                       `doneFor`, if set, is released when it finishes.
// (Declared above, with its defaults, for the repair worker.)
esp_err_t ppaBlit(const void *src, uint32_t srcW, uint32_t srcH,
                  uint32_t blkX, uint32_t blkY, uint32_t blkW, uint32_t blkH,
                  void *dst, uint32_t dstX, uint32_t dstY, ppa_srm_rotation_angle_t angle,
                  bool blocking, lv_display_t *doneFor) {
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
// indices without a lock is safe. THIS IS WHERE THE FLUSH IS TRIPLE-ONLY:
// with two buffers there is often no such buffer, and the fallback would draw
// into the one on screen. DOUBLE_PARTIAL would wait here for the panel's
// frame-complete interrupt instead; Fleet_Display::begin() refuses any mode
// but TRIPLE_PARTIAL until that is built.
uint8_t freeBuffer() {
    const uint8_t n = s_d->numFrameBuffers();
    const uint8_t a = s_d->scanning(), b = s_d->submitted();
    for (uint8_t i = 0; i < n; i++) {
        if (i != a && i != b) return i;
    }
    return (uint8_t)((b + 1) % n);   // unreachable with 3
}

// LVGL's list of areas this refresh redraws. Still intact while the refresh
// is flushing: lv_refr.c clears it only after the last area is drawn. The
// same list esp_lvgl_adapter reads for the same purpose.
void captureRedraw(lv_display_t *disp) {
    s_nowN = 0;
    for (uint32_t i = 0; i < disp->inv_p && s_nowN < LV_INV_BUF_SIZE; i++) {
        if (disp->inv_area_joined[i]) continue;   // merged into another entry
        s_now[s_nowN++] = toPhysical(disp->inv_areas[i]);
    }
}

// Copy one area from the newest frame into the buffer being built. Queued,
// not waited for: the rotations queued after it cannot start until it is
// done. If the queue is full it is done blocking instead - a repair that is
// silently refused would leave stale pixels on the glass.
void repairBlit(void *from, void *to, const lv_area_t &r, LVGL_Startup::FlushStats *stats) {
    const uint32_t w = (uint32_t)(r.x2 - r.x1 + 1), h = (uint32_t)(r.y2 - r.y1 + 1);
    if (repairQueue(r)) {                 // the DMA2D copier first - 3x the PPA's speed
        if (stats) stats->repairPx += w * h;
        return;
    }
    if (ppaBlit(from, s_pw, s_ph, r.x1, r.y1, w, h, to, r.x1, r.y1,
                PPA_SRM_ROTATION_ANGLE_0, /*blocking*/ false) != ESP_OK) {
        ppaBlit(from, s_pw, s_ph, r.x1, r.y1, w, h, to, r.x1, r.y1, PPA_SRM_ROTATION_ANGLE_0);
    }
    if (stats) stats->repairPx += w * h;
}

// Repair `r` less everything this frame redraws: LVGL's own lv_area_diff
// (what it uses for the same job in direct mode, lv_refr.c) cuts each
// redrawn area out, leaving up to four pieces per cut. During a panel
// animation consecutive frames overlap almost entirely, so what is left is
// thin. If the pieces outgrow PIECES_MAX, `r` is repaired whole - more
// copying, never wrong.
constexpr uint8_t PIECES_MAX = 32;
void repairArea(void *from, void *to, const lv_area_t &r, LVGL_Startup::FlushStats *stats) {
    lv_area_t bufA[PIECES_MAX], bufB[PIECES_MAX];
    lv_area_t *cur = bufA, *nxt = bufB;
    uint8_t n = 1;
    cur[0] = r;
    for (uint8_t c = 0; c < s_nowN && n > 0; c++) {
        uint8_t m = 0;
        for (uint8_t i = 0; i < n; i++) {
            lv_area_t res[4];
            const int8_t k = lv_area_diff(res, &cur[i], &s_now[c]);
            if (k < 0) {                            // untouched by this cut
                if (m >= PIECES_MAX) { repairBlit(from, to, r, stats); return; }
                nxt[m++] = cur[i];
                continue;
            }
            if (m + k > PIECES_MAX) { repairBlit(from, to, r, stats); return; }
            for (int8_t j = 0; j < k; j++) nxt[m++] = res[j];
        }
        lv_area_t *t = cur; cur = nxt; nxt = t;
        n = m;
    }
    for (uint8_t i = 0; i < n; i++) repairBlit(from, to, cur[i], stats);
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
    repairFrameBegin(from, to);

    LVGL_Startup::FlushStats *stats = LVGL_Startup::activeFlushStats();
    if (stats) stats->areas = s_nowN;
    if (st.all) {
        const lv_area_t whole = {0, 0, s_pw - 1, s_ph - 1};
        const uint32_t before = stats ? stats->repairPx : 0;
        repairArea(from, to, whole, stats);
        if (stats && stats->repairPx - before == (uint32_t)(s_pw * s_ph)) stats->repairFull = 1;
    } else {
        for (uint8_t i = 0; i < st.n; i++) repairArea(from, to, st.r[i], stats);
    }
    repairFrameStart();   // the worker copies the list while LVGL draws
    staleClear(st);
    s_inFrame = true;
}

// Last chunk: the other two buffers are now behind by what this frame drew;
// hand this one to the panel.
void endFrame() {
    for (uint8_t i = 0; i < s_d->numFrameBuffers(); i++) {
        if (i == s_target) continue;
        for (uint8_t k = 0; k < s_nowN; k++) staleAdd(s_stale[i], s_now[k]);
    }
    repairFrameEnd();   // every repair copy finished before the panel sees the buffer
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
    s_rot = display.softwareRotation();   // 0 when the panel turns the picture itself
    s_pw  = display.panelWidth();
    s_ph  = display.panelHeight();

    ppa_client_config_t pc = {};
    pc.oper_type = PPA_OPERATION_SRM;
    // Room for a frame's repairs plus the strips in flight. A repair area can
    // be cut into several pieces (repairArea), so this is generous; one that
    // still does not fit is done blocking, never dropped (repairBlit).
    pc.max_pending_trans_num = 32;
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

    // The repair copier: one handle, one worker on core 0 (see repairWorker).
    // If any part is refused, s_fbc stays null and every repair stays on the
    // PPA, as before - slower, never wrong.
    s_rqStart = xSemaphoreCreateBinary();
    s_rqDone  = xSemaphoreCreateBinary();
    s_rqPiece = xSemaphoreCreateBinary();
    esp_async_fbcpy_handle_t fbc = nullptr;
    esp_async_fbcpy_config_t fc = {};
    if (s_rqStart && s_rqDone && s_rqPiece && esp_async_fbcpy_install(&fc, &fbc) == ESP_OK &&
        xTaskCreatePinnedToCore(repairWorker, "lcd_repair", 4096, nullptr, 6, nullptr, 0) == pdPASS) {
        s_fbc = fbc;
    }
    Serial.printf("[LVGL] repair copier: %s\n", s_fbc ? "DMA2D (esp_async_fbcpy), worker on core 0"
                                                      : "unavailable - repairs stay on the PPA");

    // LVGL's logical size: the panel's, turned by the rotation.
    const bool swap = (s_rot & 1);
    const int32_t lw = swap ? s_ph : s_pw;
    const int32_t lh = swap ? s_pw : s_ph;

    // Draw buffers: BSP height (50 lines on WS_P4_5) of the LOGICAL width, in
    // PSRAM, aligned for the PPA and the cache (LV_DRAW_BUF_ALIGN is 64 on
    // this path - lv_conf.h). A second one lets LVGL draw the next strip while
    // the PPA is still rotating this one (the strip rotations are queued, and
    // onPpaDone releases the buffer), so DOUBLE_BUFFERING matters here. 1 vs 2
    // has not been measured on this path.
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
    info.lines = lines;
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

uint32_t panelFramesScanned() { return s_d ? s_d->framesScanned() : 0; }

uint8_t softwareRotation() { return s_rot; }

bool copyBench(CopyBench &out, int reps) {
    if (!s_d || !s_ppa || reps < 1) return false;
    if (s_inFrame) return false;   // never mid-frame: the target is spoken for

    // Source: the newest complete frame. Destination: a buffer the panel is
    // neither showing nor about to show. Every test below leaves it holding a
    // copy of the source (the rotation test is followed by copies), which is
    // at worst MORE up to date than its repair record says - never less.
    const uint8_t srcIdx = s_d->submitted();
    const uint8_t dstIdx = freeBuffer();
    const void *src = s_d->frameBuffer(srcIdx);
    void *dst       = s_d->frameBuffer(dstIdx);
    const size_t bytes = s_d->frameBufferBytes();
    out.frameBytes = (uint32_t)bytes;

    // 1. PPA rotation, a square block (a whole-frame 90-degree turn would not
    //    fit a non-square frame). This board's angle; 90 on a rotation-0 board.
    const uint32_t sq = (uint32_t)LV_MIN(s_pw, s_ph);
    out.rotBytes = sq * sq * 2;
    const ppa_srm_rotation_angle_t ang = s_rot ? ppaAngle() : PPA_SRM_ROTATION_ANGLE_90;
    int64_t t = esp_timer_get_time();
    for (int i = 0; i < reps; i++) ppaBlit(src, s_pw, s_ph, 0, 0, sq, sq, dst, 0, 0, ang);
    out.ppaRotUs = (esp_timer_get_time() - t) / reps;

    // 2. PPA copy, angle 0, whole frame - what the repair uses today.
    t = esp_timer_get_time();
    for (int i = 0; i < reps; i++) ppaBlit(src, s_pw, s_ph, 0, 0, s_pw, s_ph, dst, 0, 0, PPA_SRM_ROTATION_ANGLE_0);
    out.ppaCopyUs = (esp_timer_get_time() - t) / reps;

    // 3. esp_async_fbcpy (DMA2D memory-to-memory), whole frame, waited for.
    static esp_async_fbcpy_handle_t s_fbcpy = nullptr;
    static SemaphoreHandle_t s_fbDone = nullptr;
    if (!s_fbDone) s_fbDone = xSemaphoreCreateBinary();
    if (!s_fbcpy) {
        esp_async_fbcpy_config_t cfg = {};
        out.fbcpyErr = esp_async_fbcpy_install(&cfg, &s_fbcpy);
    }
    if (s_fbcpy && s_fbDone) {
        esp_async_fbcpy_trans_desc_t tr = {};
        tr.src_buffer = src;
        tr.dst_buffer = dst;
        tr.src_buffer_size_x = tr.dst_buffer_size_x = tr.copy_size_x = (size_t)s_pw;
        tr.src_buffer_size_y = tr.dst_buffer_size_y = tr.copy_size_y = (size_t)s_ph;
        tr.pixel_format_unique_id.color_type_id = COLOR_TYPE_ID(COLOR_SPACE_RGB, COLOR_PIXEL_RGB565);
        t = esp_timer_get_time();
        for (int i = 0; i < reps && out.fbcpyErr == ESP_OK; i++) {
            out.fbcpyErr = esp_async_fbcpy(s_fbcpy, &tr, onFbcpyDone, s_fbDone);
            if (out.fbcpyErr == ESP_OK && xSemaphoreTake(s_fbDone, pdMS_TO_TICKS(1000)) != pdTRUE) {
                out.fbcpyErr = ESP_ERR_TIMEOUT;
            }
        }
        out.fbcpyUs = (esp_timer_get_time() - t) / reps;
    }

    // 3b. IS THE COPIER'S RESULT RIGHT? Clear the destination, copy, compare
    //     with the CPU - the whole frame, then one band of rows alone (the
    //     band where WS_P4_5 showed stale pixels: rows 1118-1165, clamped).
    if (s_fbcpy && s_fbDone && out.fbcpyErr == ESP_OK) {
        const uint16_t *s16 = static_cast<const uint16_t *>(src);
        uint16_t *d16 = static_cast<uint16_t *>(dst);
        auto clearDst = [&]() {
            memset(dst, 0, bytes);
            esp_cache_msync(dst, bytes, ESP_CACHE_MSYNC_FLAG_DIR_C2M | ESP_CACHE_MSYNC_FLAG_INVALIDATE);
        };
        auto runRect = [&](int32_t x, int32_t y, int32_t w, int32_t h) -> bool {
            esp_async_fbcpy_trans_desc_t tr = {};
            tr.src_buffer = src;
            tr.dst_buffer = dst;
            tr.src_buffer_size_x = tr.dst_buffer_size_x = (size_t)s_pw;
            tr.src_buffer_size_y = tr.dst_buffer_size_y = (size_t)s_ph;
            tr.src_offset_x = tr.dst_offset_x = (size_t)x;
            tr.src_offset_y = tr.dst_offset_y = (size_t)y;
            tr.copy_size_x = (size_t)w;
            tr.copy_size_y = (size_t)h;
            tr.pixel_format_unique_id.color_type_id = COLOR_TYPE_ID(COLOR_SPACE_RGB, COLOR_PIXEL_RGB565);
            return esp_async_fbcpy(s_fbcpy, &tr, onFbcpyDone, s_fbDone) == ESP_OK &&
                   xSemaphoreTake(s_fbDone, pdMS_TO_TICKS(1000)) == pdTRUE;
        };
        auto runCopy = [&](int32_t y, int32_t h) -> bool {
            esp_async_fbcpy_trans_desc_t tr = {};
            tr.src_buffer = src;
            tr.dst_buffer = dst;
            tr.src_buffer_size_x = tr.dst_buffer_size_x = tr.copy_size_x = (size_t)s_pw;
            tr.src_buffer_size_y = tr.dst_buffer_size_y = (size_t)s_ph;
            tr.src_offset_y = tr.dst_offset_y = (size_t)y;
            tr.copy_size_y = (size_t)h;
            tr.pixel_format_unique_id.color_type_id = COLOR_TYPE_ID(COLOR_SPACE_RGB, COLOR_PIXEL_RGB565);
            return esp_async_fbcpy(s_fbcpy, &tr, onFbcpyDone, s_fbDone) == ESP_OK &&
                   xSemaphoreTake(s_fbDone, pdMS_TO_TICKS(1000)) == pdTRUE;
        };
        esp_cache_msync((void *)src, bytes, ESP_CACHE_MSYNC_FLAG_DIR_M2C);

        clearDst();
        if (runCopy(0, s_ph)) {
            esp_cache_msync(dst, bytes, ESP_CACHE_MSYNC_FLAG_DIR_M2C);
            for (int32_t i = 0; i < s_pw * s_ph; i++) {
                if (d16[i] != s16[i]) {
                    if (out.fullFirstBadRow < 0) out.fullFirstBadRow = i / s_pw;
                    out.fullBad++;
                }
            }
        } else {
            out.fbcpyErr = ESP_FAIL;
        }

        out.bandH = 48;
        out.bandY = LV_MIN(1118, s_ph - out.bandH);
        clearDst();
        if (runCopy(out.bandY, out.bandH)) {
            esp_cache_msync(dst, bytes, ESP_CACHE_MSYNC_FLAG_DIR_M2C);
            for (int32_t i = 0; i < s_pw * s_ph; i++) {
                const int32_t row = i / s_pw;
                const bool inBand = row >= out.bandY && row < out.bandY + out.bandH;
                if (inBand && d16[i] != s16[i]) {
                    if (out.bandFirstBadRow < 0) out.bandFirstBadRow = row;
                    out.bandBad++;
                } else if (!inBand && d16[i] != 0) {
                    out.bandOutside++;
                }
            }
        } else {
            out.fbcpyErr = ESP_FAIL;
        }
        // Rectangles inside the band, odd and even x and width - the shapes
        // the repair's subtraction produces on a rotated panel.
        const int32_t pw = s_pw;
        const int32_t rx[CopyBench::RECTS] = {0,   0,       1,       2,  pw - 5, pw - 6};
        const int32_t rw[CopyBench::RECTS] = {pw - 53, pw - 54, pw - 54, 64, 5,      6};
        for (uint8_t k = 0; k < CopyBench::RECTS; k++) {
            CopyBench::RectCheck &rc = out.rects[k];
            rc.x = rx[k];
            rc.w = rw[k];
            clearDst();
            if (!runRect(rc.x, out.bandY, rc.w, out.bandH)) { out.fbcpyErr = ESP_FAIL; continue; }
            esp_cache_msync(dst, bytes, ESP_CACHE_MSYNC_FLAG_DIR_M2C);
            for (int32_t i = 0; i < s_pw * s_ph; i++) {
                const int32_t row = i / s_pw, col = i % s_pw;
                const bool in = row >= out.bandY && row < out.bandY + out.bandH &&
                                col >= rc.x && col < rc.x + rc.w;
                if (in && d16[i] != s16[i]) rc.bad++;
                else if (!in && d16[i] != 0) rc.outside++;
            }
        }
        // CONCURRENCY. The flush runs many copies at once; the checks above
        // ran one at a time. Band rows are clamped to the frame.
        static esp_async_fbcpy_handle_t s_fbcpy2 = nullptr;
        static SemaphoreHandle_t s_fbDone2 = nullptr;
        if (!s_fbDone2) s_fbDone2 = xSemaphoreCreateBinary();
        if (!s_fbcpy2) { esp_async_fbcpy_config_t cfg2 = {}; esp_async_fbcpy_install(&cfg2, &s_fbcpy2); }
        auto startRows = [&](esp_async_fbcpy_handle_t h, SemaphoreHandle_t sem, int32_t y, int32_t n) -> bool {
            esp_async_fbcpy_trans_desc_t tr = {};
            tr.src_buffer = src;
            tr.dst_buffer = dst;
            tr.src_buffer_size_x = tr.dst_buffer_size_x = tr.copy_size_x = (size_t)s_pw;
            tr.src_buffer_size_y = tr.dst_buffer_size_y = (size_t)s_ph;
            tr.src_offset_y = tr.dst_offset_y = (size_t)y;
            tr.copy_size_y = (size_t)n;
            tr.pixel_format_unique_id.color_type_id = COLOR_TYPE_ID(COLOR_SPACE_RGB, COLOR_PIXEL_RGB565);
            return esp_async_fbcpy(h, &tr, onFbcpyDone, sem) == ESP_OK;
        };
        // Count bad pixels in [y0, y0+n0) and [y1, y1+n1); changed pixels elsewhere.
        auto verify2 = [&](int32_t y0, int32_t n0, int32_t y1, int32_t n1,
                           uint32_t &badA, uint32_t &badB, uint32_t &outside) {
            esp_cache_msync(dst, bytes, ESP_CACHE_MSYNC_FLAG_DIR_M2C);
            for (int32_t i = 0; i < s_pw * s_ph; i++) {
                const int32_t row = i / s_pw;
                if (row >= y0 && row < y0 + n0)      { if (d16[i] != s16[i]) badA++; }
                else if (row >= y1 && row < y1 + n1) { if (d16[i] != s16[i]) badB++; }
                else if (d16[i] != 0)                outside++;
            }
        };
        const int32_t a0 = 0, an = LV_MIN(200, s_ph / 4);
        const int32_t b0 = LV_MIN(600, s_ph / 2), bn = LV_MIN(200, s_ph / 4);
        if (s_fbcpy2 && s_fbDone2) {
            clearDst();
            const bool okA = startRows(s_fbcpy, s_fbDone, a0, an);
            const bool okB = startRows(s_fbcpy2, s_fbDone2, b0, bn);
            if (okA) xSemaphoreTake(s_fbDone, pdMS_TO_TICKS(1000));
            if (okB) xSemaphoreTake(s_fbDone2, pdMS_TO_TICKS(1000));
            if (okA && okB) verify2(a0, an, b0, bn, out.dualA, out.dualB, out.dualOutside);
            else out.fbcpyErr = ESP_FAIL;
        }
        const int32_t f0 = 0, fn = LV_MIN(400, s_ph / 3);
        const int32_t p0 = LV_MIN(800, s_ph / 2), pn = LV_MIN(400, s_ph / 3);
        clearDst();
        const bool okF = startRows(s_fbcpy, s_fbDone, f0, fn);
        ppaBlit(src, s_pw, s_ph, 0, p0, s_pw, pn, dst, 0, p0, PPA_SRM_ROTATION_ANGLE_0);   // blocking
        if (okF) xSemaphoreTake(s_fbDone, pdMS_TO_TICKS(1000));
        if (okF) verify2(f0, fn, p0, pn, out.mixF, out.mixP, out.mixOutside);
        else out.fbcpyErr = ESP_FAIL;
        // Step 4 below (CPU copy) leaves the buffer holding the newest frame.
    }

    // 4. CPU: drop any cached lines of the source (the PPA wrote it behind the
    //    CPU's back), copy, then write the destination back to memory - the
    //    same end state as a DMA copy.
    esp_cache_msync((void *)src, bytes, ESP_CACHE_MSYNC_FLAG_DIR_M2C);
    t = esp_timer_get_time();
    for (int i = 0; i < reps; i++) {
        memcpy(dst, src, bytes);
        esp_cache_msync(dst, bytes, ESP_CACHE_MSYNC_FLAG_DIR_C2M | ESP_CACHE_MSYNC_FLAG_INVALIDATE);
    }
    out.cpuUs = (esp_timer_get_time() - t) / reps;
    return true;
}

} // namespace LVGL_Flush

#endif // DISPLAY_ESPLCD
