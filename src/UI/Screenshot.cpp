// Screenshot - issue #58. The design is in Screenshot.h; the reasons are here.
//
// Modelled on agillis/esphome-lvgl-screenshot (MIT), which the owner found in
// reference/esphome-modular-lvgl-buttons/components/lvgl_screenshot/. What was
// kept: snapshot -> repack -> stb_image_write PNG -> esp_http_server, and a
// request/done handshake so the capture happens on the LVGL thread. What had to
// change, each for a reason found in the source rather than on the glass:
//
//   - lv_snapshot_take() is NOT used. It allocates through lv_draw_buf_create()
//     -> lv_malloc, which on this fleet is LVGL's own 128 KB pool
//     (LV_STDLIB_BUILTIN). A full screen is 0.3-2.7 MB. It could never work.
//     We allocate in PSRAM and use lv_snapshot_take_to_draw_buf() instead.
//   - The top and system layers are composited on, because the owner asked for
//     the screen exactly as it is. A snapshot of the screen object alone
//     leaves out every toast and the FPS monitor.
//   - The screen is rendered in the DISPLAY's colour format (RGB565), not
//     RGB888, so the PNG carries the colours the panel shows rather than a
//     smoother 24-bit re-render the panel never displayed.
//   - The PNG encoder runs on the HTTP server task, not the LVGL thread, so
//     the UI pauses only for the render. See HttpServer.cpp for how that task
//     is placed so the encode cannot starve the UI or trip the watchdog.
//   - stb_image_write allocates a filtered copy of the whole image plus its
//     zlib state through plain malloc. Every one of those is steered to PSRAM
//     below; plain malloc under 4 KB would land in internal RAM.
//
#ifdef ENABLE_SCREENSHOT

#include "UI/Screenshot.h"
#include "HttpServer.h"
#include "DeviceIdentity.h"
#ifdef DISPLAY_ESPLCD
#include "../LVGL_Flush.h"   // shownFrameBuffer(), for ?fb=1
#endif

#include <Arduino.h>          // Serial only
#include <lvgl.h>
#include <atomic>
#include <stdlib.h>
#include <string.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <esp_heap_caps.h>
#include <esp_timer.h>

#define STBIW_MALLOC(sz)        heap_caps_malloc((sz), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT)
#define STBIW_REALLOC(p, newsz) heap_caps_realloc((p), (newsz), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT)
#define STBIW_FREE(p)           heap_caps_free(p)
#define STBI_WRITE_NO_STDIO
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "third_party/stb_image_write.h"

namespace {

// How long the handler waits for loop() to pick a request up. Only a stalled
// loop() takes longer; a render already under way is always waited out.
constexpr uint32_t PICKUP_WAIT_MS = 5000;

constexpr uint32_t PSRAM_CAPS = MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT;

// PNG encoder defaults, MEASURED on WS_P4_5 at 1280x720, 2026-09-24:
//
//   stb default (z8, all filters)   encode 3097 ms   89 KB
//   z5, filter 1 (Sub)              encode 1192 ms   85 KB
//   z5, filter 0 (None)             encode  870 ms   75 KB   <- chosen
//   z9, filter 0                    encode 1140 ms   74 KB
//
// Filter None wins on BOTH speed and size. A dashboard is large runs of flat
// colour, which deflate already compresses; the predictive filters only add
// work, and trying all five per row (stb's default) triples the encode. z below
// 5 is identical to 5 because stb raises it (stb_image_write.h, "quality < 5").
constexpr int DEFAULT_Z = 5;
constexpr int DEFAULT_F = 0;

// One capture at a time. The handler moves IDLE -> REQUESTED; loop() moves
// REQUESTED -> CAPTURING -> DONE; the handler moves DONE -> IDLE when it has
// sent the picture. Each move is a compare-exchange, so a handler that gives up
// and loop() starting the render cannot both win.
enum : int { ST_IDLE, ST_REQUESTED, ST_CAPTURING, ST_DONE,
              ST_REQUESTED_FB };   // ?fb=1: the panel's frame buffer (esp_lcd path)

std::atomic<int>  s_state{ST_IDLE};
SemaphoreHandle_t s_done = nullptr;

// Written by loop() before DONE, read by the handler after it. The semaphore
// is the hand-off.
struct Capture {
    uint8_t    *rgb      = nullptr;   // w*h*3, R,G,B, in PSRAM
    uint32_t    w        = 0;
    uint32_t    h        = 0;
    uint32_t    renderMs = 0;
    uint8_t     overlays = 0;         // top/sys layers composited on
    size_t      psramBefore = 0;
    const char *error    = nullptr;
};
Capture s_cap;

size_t psramFree() { return heap_caps_get_free_size(MALLOC_CAP_SPIRAM); }

uint8_t *psramAlloc(size_t n) {
    return static_cast<uint8_t *>(heap_caps_aligned_alloc(64, n, PSRAM_CAPS));
}

// --= Rendering (LVGL thread only) =--

// Renders `obj` into `mem`, which we own, as a W x H image of format `cf`.
// Every object passed here is a screen or a display layer: display-sized, at
// the origin. Anything else would not line up with the screen underneath it,
// so it is refused rather than composited wrongly.
//
// An extended draw area (a shadow on the screen object itself) is refused too,
// without our checking for it: the snapshot sizes itself to the object PLUS
// that area, finds the buffer we gave it too small, and returns an error. The
// size getter is private in LVGL 9.5, so that refusal is the check.
bool render(lv_obj_t *obj, lv_color_format_t cf, uint8_t *mem, uint32_t stride,
            uint32_t W, uint32_t H) {
    lv_obj_update_layout(obj);
    lv_area_t a;
    lv_obj_get_coords(obj, &a);
    if (a.x1 != 0 || a.y1 != 0 ||
        lv_area_get_width(&a) != (int32_t)W || lv_area_get_height(&a) != (int32_t)H) {
        Serial.printf("[Shot] object is not display-sized at the origin (%ld,%ld %ldx%ld); skipped\n",
                      (long)a.x1, (long)a.y1, (long)lv_area_get_width(&a),
                      (long)lv_area_get_height(&a));
        return false;
    }

    lv_draw_buf_t db;
    if (lv_draw_buf_init(&db, W, H, cf, stride, mem, stride * H) != LV_RESULT_OK) return false;
    lv_draw_buf_clear(&db, nullptr);
    if (lv_snapshot_take_to_draw_buf(obj, cf, &db) != LV_RESULT_OK) {
        Serial.println("[Shot] lv_snapshot_take_to_draw_buf refused the object");
        return false;
    }
    return true;
}

// Display RGB565 -> R,G,B. Bit replication, so white stays 255 and not 248.
void expand565(const uint8_t *src, uint32_t stride, uint8_t *rgb, uint32_t W, uint32_t H) {
    for (uint32_t y = 0; y < H; y++) {
        const uint16_t *s = reinterpret_cast<const uint16_t *>(src + (size_t)y * stride);
        uint8_t *d = rgb + (size_t)y * W * 3;
        for (uint32_t x = 0; x < W; x++) {
            const uint16_t v = s[x];
            const uint8_t r = v >> 11, g = (v >> 5) & 0x3F, b = v & 0x1F;
            d[0] = (r << 3) | (r >> 2);
            d[1] = (g << 2) | (g >> 4);
            d[2] = (b << 3) | (b >> 2);
            d += 3;
        }
    }
}

// LVGL's RGB888 is B,G,R in memory; PNG wants R,G,B. The reference's comment on
// this is worth repeating: miss it and every screenshot has red and blue
// swapped, which looks like a display bug and gets chased in the wrong file.
void repack888(const uint8_t *src, uint32_t stride, uint8_t *rgb, uint32_t W, uint32_t H) {
    for (uint32_t y = 0; y < H; y++) {
        const uint8_t *s = src + (size_t)y * stride;
        uint8_t *d = rgb + (size_t)y * W * 3;
        for (uint32_t x = 0; x < W; x++, s += 3, d += 3) {
            d[0] = s[2]; d[1] = s[1]; d[2] = s[0];
        }
    }
}

// Straight-alpha ARGB8888 (LVGL memory order B,G,R,A) over R,G,B.
void blendOver(const uint8_t *src, uint32_t stride, uint8_t *rgb, uint32_t W, uint32_t H) {
    for (uint32_t y = 0; y < H; y++) {
        const uint8_t *s = src + (size_t)y * stride;
        uint8_t *d = rgb + (size_t)y * W * 3;
        for (uint32_t x = 0; x < W; x++, s += 4, d += 3) {
            const uint32_t a = s[3];
            if (a == 0) continue;
            if (a == 255) { d[0] = s[2]; d[1] = s[1]; d[2] = s[0]; continue; }
            const uint32_t ia = 255 - a;
            d[0] = (s[2] * a + d[0] * ia + 127) / 255;
            d[1] = (s[1] * a + d[1] * ia + 127) / 255;
            d[2] = (s[0] * a + d[2] * ia + 127) / 255;
        }
    }
}

// Worth rendering only if something on it could show. Hidden children and a
// transparent layer cost a 4-bytes-per-pixel buffer and a render for nothing.
bool layerHasContent(lv_obj_t *layer) {
    if (!layer) return false;
    if (lv_obj_get_style_bg_opa(layer, LV_PART_MAIN) > LV_OPA_TRANSP) return true;
    const uint32_t n = lv_obj_get_child_count(layer);
    for (uint32_t i = 0; i < n; i++) {
        if (!lv_obj_has_flag(lv_obj_get_child(layer, i), LV_OBJ_FLAG_HIDDEN)) return true;
    }
    return false;
}

#ifdef DISPLAY_ESPLCD
// ?fb=1 on the esp_lcd path (2.9 step 2): the frame buffer the panel is
// actually showing, copied as-is - physical orientation (WS_P4_5 comes out
// portrait), no overlays re-rendered, no LVGL involved. The one check of
// "what is on the glass" that does not trust LVGL, the rotation or the
// repair bookkeeping: it reads their result.
void captureFramebuffer() {
    const int64_t t0 = esp_timer_get_time();
    uint32_t w = 0, h = 0;
    const void *fb = LVGL_Flush::shownFrameBuffer(w, h);
    if (!fb) { s_cap.error = "no frame buffer"; return; }
    uint8_t *rgb = psramAlloc((size_t)w * h * 3);
    if (!rgb) { s_cap.error = "not enough PSRAM for the image"; return; }
    expand565(static_cast<const uint8_t *>(fb), w * 2, rgb, w, h);
    s_cap.rgb      = rgb;
    s_cap.w        = w;
    s_cap.h        = h;
    s_cap.renderMs = (uint32_t)((esp_timer_get_time() - t0) / 1000);
}
#endif

void capture(bool fromFrameBuffer) {
    s_cap = Capture{};
    s_cap.psramBefore = psramFree();
    #ifdef DISPLAY_ESPLCD
    if (fromFrameBuffer) { captureFramebuffer(); return; }
    #else
    (void)fromFrameBuffer;
    #endif
    const int64_t t0 = esp_timer_get_time();

    lv_display_t *disp = lv_display_get_default();
    lv_obj_t *scr = disp ? lv_display_get_screen_active(disp) : nullptr;
    if (!scr) { s_cap.error = "no active LVGL screen"; return; }

    // LOGICAL resolution - what the UI is laid out in, upright on every board
    // whatever the panel's physical orientation.
    const uint32_t W = lv_display_get_horizontal_resolution(disp);
    const uint32_t H = lv_display_get_vertical_resolution(disp);

    uint8_t *rgb = psramAlloc((size_t)W * H * 3);
    if (!rgb) { s_cap.error = "not enough PSRAM for the image"; return; }

    // 1. The screen, in the display's own format.
    const lv_color_format_t cf =
        lv_display_get_color_format(disp) == LV_COLOR_FORMAT_RGB565 ? LV_COLOR_FORMAT_RGB565
                                                                     : LV_COLOR_FORMAT_RGB888;
    const uint32_t sStride = lv_draw_buf_width_to_stride(W, cf);
    uint8_t *base = psramAlloc((size_t)sStride * H);
    if (!base) { heap_caps_free(rgb); s_cap.error = "not enough PSRAM for the screen render"; return; }
    if (!render(scr, cf, base, sStride, W, H)) {
        heap_caps_free(base); heap_caps_free(rgb);
        s_cap.error = "rendering the screen failed";
        return;
    }
    if (cf == LV_COLOR_FORMAT_RGB565) expand565(base, sStride, rgb, W, H);
    else                              repack888(base, sStride, rgb, W, H);
    heap_caps_free(base);

    // 2. The layers above it, in the order the display draws them. Rendered
    //    with alpha onto a cleared buffer, then blended over the screen.
    lv_obj_t *overlays[] = { lv_display_get_layer_top(disp), lv_display_get_layer_sys(disp) };
    const uint32_t oStride = lv_draw_buf_width_to_stride(W, LV_COLOR_FORMAT_ARGB8888);
    uint8_t *ov = nullptr;
    for (lv_obj_t *layer : overlays) {
        if (!layerHasContent(layer)) continue;
        if (!ov && !(ov = psramAlloc((size_t)oStride * H))) {
            Serial.println("[Shot] not enough PSRAM for an overlay; sending the screen without it");
            break;
        }
        if (render(layer, LV_COLOR_FORMAT_ARGB8888, ov, oStride, W, H)) {
            blendOver(ov, oStride, rgb, W, H);
            s_cap.overlays++;
        }
    }
    if (ov) heap_caps_free(ov);

    s_cap.rgb      = rgb;
    s_cap.w        = W;
    s_cap.h        = H;
    s_cap.renderMs = (uint32_t)((esp_timer_get_time() - t0) / 1000);
}

// --= Encoding and sending (HTTP server task only) =--

struct PngCtx {
    httpd_req_t *req;
    esp_err_t    err;
    size_t       bytes;
    int64_t      encodedAt;
    size_t       psramLow;   // free PSRAM while stb holds its buffers - the peak
};

// stb calls this once, with the whole finished PNG, while its own buffers are
// still allocated - so this is where the memory high-water mark is.
void pngWrite(void *ctx, void *data, int size) {
    auto *c = static_cast<PngCtx *>(ctx);
    if (c->bytes == 0) {
        c->encodedAt = esp_timer_get_time();
        c->psramLow  = psramFree();
    }
    if (c->err != ESP_OK || size <= 0) return;
    c->err = httpd_resp_send_chunk(c->req, static_cast<const char *>(data), size);
    if (c->err == ESP_OK) c->bytes += (size_t)size;
}

// Optional query knobs, for tuning speed against size without a rebuild:
//   ?z=1..9   zlib effort; 1-5 are all the same to stb
//   ?f=-1..4  PNG row filter; -1 tries all five per row (stb's own default)
// Out-of-range values are ignored. Both are stb globals, which is safe only
// because one capture runs at a time.
void applyQueryKnobs(httpd_req_t *req, int &z, int &f) {
    z = DEFAULT_Z;
    f = DEFAULT_F;
    char q[32], v[8];
    if (httpd_req_get_url_query_str(req, q, sizeof(q)) != ESP_OK) return;
    if (httpd_query_key_value(q, "z", v, sizeof(v)) == ESP_OK) {
        const int n = atoi(v);
        if (n >= 1 && n <= 9) z = n;
    }
    if (httpd_query_key_value(q, "f", v, sizeof(v)) == ESP_OK) {
        const int n = atoi(v);
        if (n >= -1 && n <= 4) f = n;
    }
}

esp_err_t sendPng(httpd_req_t *req) {
    int z, f;
    applyQueryKnobs(req, z, f);
    stbi_write_png_compression_level = z;
    stbi_write_force_png_filter      = f;

    char disposition[80];
    snprintf(disposition, sizeof(disposition), "inline; filename=\"%s.png\"",
             DeviceIdentity::hostname());
    char renderMs[12];
    snprintf(renderMs, sizeof(renderMs), "%lu", (unsigned long)s_cap.renderMs);

    // Header values must outlive the first send, which happens inside
    // stbi_write_png_to_func() below - these locals do.
    httpd_resp_set_type(req, "image/png");
    httpd_resp_set_hdr(req, "Content-Disposition", disposition);
    httpd_resp_set_hdr(req, "Cache-Control", "no-store");
    httpd_resp_set_hdr(req, "X-Render-Ms", renderMs);

    PngCtx ctx = { req, ESP_OK, 0, 0, 0 };
    const int64_t t0 = esp_timer_get_time();
    const int ok = stbi_write_png_to_func(pngWrite, &ctx, (int)s_cap.w, (int)s_cap.h, 3,
                                          s_cap.rgb, (int)(s_cap.w * 3));
    const int64_t t1 = esp_timer_get_time();

    if (!ok) {
        // Nothing was sent yet, so a proper error response is still possible.
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR,
                            "PNG encode failed (out of PSRAM?)");
        Serial.println("[Shot] PNG encode failed");
        return ESP_FAIL;
    }
    if (ctx.err == ESP_OK) ctx.err = httpd_resp_send_chunk(req, nullptr, 0);

    const uint32_t encodeMs = (uint32_t)((ctx.encodedAt - t0) / 1000);
    const uint32_t sendMs   = (uint32_t)((t1 - ctx.encodedAt) / 1000);
    Serial.printf("[Shot] %lux%lu, %u overlay(s), z%d f%d | render %lu ms, encode %lu ms, send %lu ms "
                  "| PNG %u KB | PSRAM free %u KB, low %u KB | %s\n",
                  (unsigned long)s_cap.w, (unsigned long)s_cap.h, s_cap.overlays, z, f,
                  (unsigned long)s_cap.renderMs, (unsigned long)encodeMs, (unsigned long)sendMs,
                  (unsigned)(ctx.bytes / 1024), (unsigned)(s_cap.psramBefore / 1024),
                  (unsigned)(ctx.psramLow / 1024),
                  ctx.err == ESP_OK ? "sent" : esp_err_to_name(ctx.err));
    return ctx.err;
}

esp_err_t handleScreenshot(httpd_req_t *req) {
    // ?fb=1 asks for the panel's frame buffer instead of LVGL's re-render; it
    // is carried in the request STATE, so loop() can never pick up a request
    // with the other kind's setting.
    int want = ST_REQUESTED;
    {
        char q[32], v[4];
        if (httpd_req_get_url_query_str(req, q, sizeof(q)) == ESP_OK &&
            httpd_query_key_value(q, "fb", v, sizeof(v)) == ESP_OK && !strcmp(v, "1")) {
            want = ST_REQUESTED_FB;
        }
    }

    int expected = ST_IDLE;
    if (!s_state.compare_exchange_strong(expected, want)) {
        httpd_resp_set_status(req, "503 Service Unavailable");
        httpd_resp_set_hdr(req, "Retry-After", "2");
        return httpd_resp_sendstr(req, "A screenshot is already in progress; try again.\n");
    }

    if (xSemaphoreTake(s_done, pdMS_TO_TICKS(PICKUP_WAIT_MS)) != pdTRUE) {
        expected = want;
        if (s_state.compare_exchange_strong(expected, ST_IDLE)) {
            // loop() never picked it up - the UI thread is stalled.
            Serial.println("[Shot] loop() did not pick the request up; gave up");
            httpd_resp_set_status(req, "503 Service Unavailable");
            return httpd_resp_sendstr(req, "The UI thread did not respond within 5 s.\n");
        }
        // It did - a render is under way, and a render always finishes.
        xSemaphoreTake(s_done, portMAX_DELAY);
    }

    // DONE: s_cap belongs to this task until the state goes back to IDLE.
    esp_err_t res;
    if (s_cap.rgb) {
        res = sendPng(req);
    } else {
        Serial.printf("[Shot] capture failed: %s\n", s_cap.error ? s_cap.error : "unknown");
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR,
                            s_cap.error ? s_cap.error : "capture failed");
        res = ESP_FAIL;
    }
    heap_caps_free(s_cap.rgb);
    s_cap.rgb = nullptr;
    s_state.store(ST_IDLE);
    return res;
}

} // namespace

namespace Screenshot {

void begin(HttpServer &http) {
    if (!s_done) s_done = xSemaphoreCreateBinary();
    if (!s_done) {
        Serial.println("[Shot] could not create semaphore; /screenshot disabled");
        return;
    }
    http.addRoute("/screenshot", HTTP_GET, handleScreenshot);
}

void service() {
    int expected = s_state.load();
    if (expected != ST_REQUESTED && expected != ST_REQUESTED_FB) return;
    const bool fromFrameBuffer = (expected == ST_REQUESTED_FB);
    if (!s_state.compare_exchange_strong(expected, ST_CAPTURING)) return;
    capture(fromFrameBuffer);
    s_state.store(ST_DONE);
    xSemaphoreGive(s_done);
}

} // namespace Screenshot

#endif // ENABLE_SCREENSHOT
