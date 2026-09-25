// Bench - milestone 2.9 (#67), step 1. The interface is in Bench.h; the
// reasons are here.
//
// WHAT IS TIMED, AND HOW. Each frame is: invalidate the target, then
// lv_refr_now(), with esp_timer (microseconds) around it. LVGL's own FPS
// overlay already splits "render | flush", but from lv_tick - whole
// milliseconds - averaged over whatever frames happened to be drawn. Here the
// frames are forced, identical, and the flush is split in two by
// LVGL_Startup's disp_flush() (see FlushStats there):
//
//   total    lv_refr_now(): layout, drawing and flushing, the lot
//   copy     inside draw16bitRGBBitmap(), summed over the frame's chunks
//   present  inside gfx->flush() on the frame's last chunk
//   wait     LVGL waiting for a flush to finish (LV_EVENT_FLUSH_WAIT_*).
//            ~0 today because the flush is synchronous; it is here so the
//            asynchronous esp_lcd flush of step 2 is measured the same way
//   render   total - copy - present - wait: what LVGL itself costs
//
// `copy + present` is the most any flush change can win back. `render` is
// untouched by one.
//
// Between frames the task yields for one tick, outside the timed part, so the
// WiFi and websocket tasks are serviced and nothing times out during a run.
//
#ifdef ENABLE_BENCH

#include "UI/Bench.h"
#include "HttpServer.h"
#include "GUIManager.h"
#include "LVGL_Startup.h"
#include "UI/UITokens.h"
#include "DeviceIdentity.h"
#include "bsp_loader.h"
#include "fleet_fw_version.h"

#include <Arduino.h>          // Serial, millis
#include <lvgl.h>
#include <atomic>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/task.h>
#include <esp_heap_caps.h>
#include <esp_timer.h>
#include <esp_system.h>       // esp_reset_reason

#ifndef FW_VERSION
    #define FW_VERSION "unknown"
#endif

namespace {

constexpr uint32_t PICKUP_WAIT_MS = 5000;   // as Screenshot: only a stalled loop() takes longer
constexpr uint32_t SETTLE_MS      = 2500;   // a rebuild, the 1.5 s page toast, HA values landing
constexpr int      N_DEFAULT      = 20;
constexpr int      N_MAX          = 100;

// One run at a time. The handler moves IDLE -> CLAIMED, fills in the request,
// then -> REQUESTED; service() moves REQUESTED -> RUNNING -> DONE across as
// many loop() calls as settling takes; the handler moves DONE -> IDLE once it
// has replied. CLAIMED exists so service() can never see REQUESTED before the
// request it names has been written.
enum : int { ST_IDLE, ST_CLAIMED, ST_REQUESTED, ST_RUNNING, ST_DONE };
std::atomic<int>  s_state{ST_IDLE};
SemaphoreHandle_t s_done = nullptr;
GUIManager       *s_gui  = nullptr;

enum class What : uint8_t { WHAT_FULL, WHAT_CARD };

struct Request {
    int  n    = N_DEFAULT;
    What what = What::WHAT_FULL;
    int  page = -1;   // -1 = leave as it is
    int  deck = -1;
    bool keep = false; // stay on the measured page/deck, so a screenshot can follow
};

struct Agg {
    int64_t sum = 0, min = INT64_MAX, max = 0;
    void add(int64_t v) { sum += v; if (v < min) min = v; if (v > max) max = v; }
    int64_t avg(int n) const { return n ? sum / n : 0; }
};

// Written by service() before DONE, read by the handler after it.
struct Result {
    Request     req;
    const char *error = nullptr;
    uint8_t     page = 0;
    const char *pageSlug = "";
    const char *scheme = "";       // per page, and Linen's shadows cost a lot
    bool        deck = false;
    uint8_t     wasPage = 0;       // what was showing before the run
    bool        wasDeck = false;
    uint32_t    settledMs = 0;
    Agg         total, render, copy, present, wait;
    uint64_t    chunks = 0, px = 0;
    size_t      heapBefore = 0, heapAfter = 0;
    size_t      lvFreeBefore = 0, lvFreeAfter = 0;
};
Result s_res;

// Run phases, owned by service().
enum class Phase : uint8_t { PH_START, PH_SETTLE };
Phase    s_phase     = Phase::PH_START;
uint32_t s_settleEnd = 0;
uint32_t s_settleAt  = 0;
uint8_t  s_origPage  = 0;
bool     s_origDeck  = false;

// LV_EVENT_FLUSH_WAIT_START/FINISH, while measuring.
int64_t s_waitStart = 0;
int64_t s_waitUs    = 0;

void waitCb(lv_event_t *e) {
    const lv_event_code_t c = lv_event_get_code(e);
    if (c == LV_EVENT_FLUSH_WAIT_START)       s_waitStart = esp_timer_get_time();
    else if (c == LV_EVENT_FLUSH_WAIT_FINISH) s_waitUs   += esp_timer_get_time() - s_waitStart;
}

size_t lvMemFree() {
    lv_mem_monitor_t m;
    lv_mem_monitor(&m);
    return m.free_size;
}

// --= LVGL thread =--

void measure() {
    Result &r = s_res;
    lv_display_t *disp = lv_display_get_default();
    if (!disp) { r.error = "no LVGL display"; return; }

    r.page     = s_gui->currentPage();
    r.pageSlug = s_gui->currentPageSlug();
    r.scheme   = UI::pal().name;   // a string literal in the scheme table; safe to keep
    r.deck     = s_gui->deckShown();

    lv_obj_t *target = r.req.what == What::WHAT_CARD ? s_gui->firstCard()
                                                     : lv_display_get_screen_active(disp);
    if (!target) {
        r.error = r.req.what == What::WHAT_CARD ? "no card on this page" : "no active screen";
        return;
    }

    // Anything already invalid goes out now, so frame 1 times only our area.
    lv_refr_now(disp);

    r.heapBefore   = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
    r.lvFreeBefore = lvMemFree();
    lv_display_add_event_cb(disp, waitCb, LV_EVENT_ALL, nullptr);

    for (int i = 0; i < r.req.n; i++) {
        LVGL_Startup::FlushStats fs;
        s_waitUs = 0;
        lv_obj_invalidate(target);

        LVGL_Startup::attachFlushStats(&fs);
        const int64_t t0 = esp_timer_get_time();
        lv_refr_now(disp);
        const int64_t total = esp_timer_get_time() - t0;
        LVGL_Startup::attachFlushStats(nullptr);

        r.total.add(total);
        r.copy.add(fs.copyUs);
        r.present.add(fs.presentUs);
        r.wait.add(s_waitUs);
        r.render.add(total - fs.copyUs - fs.presentUs - s_waitUs);
        r.chunks += fs.chunks;
        r.px     += fs.px;

        vTaskDelay(1);   // outside the timed part: let WiFi and the websocket breathe
    }

    lv_display_remove_event_cb_with_user_data(disp, waitCb, nullptr);
    r.heapAfter   = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
    r.lvFreeAfter = lvMemFree();
}

// Page first, then deck: the deck is one setting across pages, and a page
// change rebuilds the dashboard around whatever it currently is.
bool applyTarget(int page, int deck) {
    bool changed = false;
    if (page >= 0 && page != s_gui->currentPage()) { s_gui->goToPage((int16_t)page); changed = true; }
    if (deck >= 0 && (deck != 0) != s_gui->deckShown()) { s_gui->toggleDeck(); changed = true; }
    return changed;
}

// --= HTTP server task =--

// Everything below is formatted from s_res after DONE, off the LVGL thread,
// into a PSRAM buffer taken per request. NOT a static: .bss is internal RAM,
// and CYD_S3_3248 boots with ~22 KB of that free - 2 KB of it idle between
// benches is 10% of what WiFi and MQTT have left (LESSONS.md).
constexpr size_t JSON_CAP = 2048;
char *s_json = nullptr;

struct Out {
    size_t len = 0;
    void add(const char *fmt, ...) {
        if (len >= JSON_CAP) return;
        va_list ap;
        va_start(ap, fmt);
        const int w = vsnprintf(s_json + len, JSON_CAP - len, fmt, ap);
        va_end(ap);
        if (w > 0) len += (size_t)w;
    }
    void agg(const char *name, const Agg &a, int n, bool comma = true) {
        add("\"%s\":{\"avg\":%lld,\"min\":%lld,\"max\":%lld}%s", name,
            (long long)a.avg(n), (long long)(n ? a.min : 0), (long long)a.max, comma ? "," : "");
    }
};

const char *busName() {
#if defined(HAS_MIPI_PANEL)
    return "DSI";
#elif defined(HAS_RGB_PANEL)
    return "RGB";
#elif defined(HAS_QSPI_PANEL)
    return "QSPI";
#else
    return "SPI";
#endif
}

// FIELD MEANINGS. All times are microseconds per frame; see the top of this
// file for what each one covers. `fps_ceiling` is 1e6 / total.avg: the rate
// this scenario could never beat, not a rate anyone will see.
size_t buildJson() {
    const Result &r = s_res;
    const int n = r.req.n;
    const LVGL_Startup::DrawBufInfo b = LVGL_Startup::drawBufInfo();
    lv_display_t *disp = lv_display_get_default();

    Out o;
    o.add("{\"host\":\"%s\",\"fw\":\"%s\",\"board\":\"%s\",\"panel\":\"%s\",\"bus\":\"%s\",",
          DeviceIdentity::hostname(), FW_VERSION, bsp_hw.MODEL, bsp_display.PANEL_MODEL, busName());
    o.add("\"flush_path\":\"arduino_gfx\",\"res\":[%ld,%ld],\"rotation\":%u,",
          (long)lv_display_get_horizontal_resolution(disp),
          (long)lv_display_get_vertical_resolution(disp), (unsigned)bsp_display.ROTATION);
    o.add("\"buf\":{\"bytes\":%u,\"lines\":%u,\"count\":%u,\"where\":\"%s\"},",
          (unsigned)b.bytes, (unsigned)bsp_lvgl.DRAW_BUF_HEIGHT, (unsigned)b.count,
          b.psram ? "psram" : "internal");
    o.add("\"lv_use_ppa\":%d,\"lv_draw_buf_align\":%d,", (int)LV_USE_PPA, (int)LV_DRAW_BUF_ALIGN);
    o.add("\"scenario\":{\"what\":\"%s\",\"page\":%u,\"page_slug\":\"%s\",\"scheme\":\"%s\",\"deck\":%s,\"n\":%d,\"settled_ms\":%lu},",
          r.req.what == What::WHAT_CARD ? "card" : "full", (unsigned)r.page, r.pageSlug,
          r.scheme ? r.scheme : "", r.deck ? "true" : "false", n, (unsigned long)r.settledMs);
    o.add("\"was\":{\"page\":%u,\"deck\":%s,\"restored\":%s},\"page_count\":%u,",
          (unsigned)r.wasPage, r.wasDeck ? "true" : "false", r.req.keep ? "false" : "true",
          (unsigned)s_gui->pageCount());
    o.add("\"us\":{");
    o.agg("total", r.total, n);
    o.agg("render", r.render, n);
    o.agg("copy", r.copy, n);
    o.agg("present", r.present, n);
    o.agg("wait", r.wait, n, false);
    o.add("},");
    const int64_t avgTotal = r.total.avg(n);
    o.add("\"chunks\":%.1f,\"px\":%llu,\"fps_ceiling\":%.1f,",
          n ? (double)r.chunks / n : 0.0, (unsigned long long)(n ? r.px / n : 0),
          avgTotal > 0 ? 1e6 / (double)avgTotal : 0.0);
    o.add("\"heap\":{\"internal_before\":%u,\"internal_after\":%u,\"lv_mem_free_before\":%u,\"lv_mem_free_after\":%u},",
          (unsigned)r.heapBefore, (unsigned)r.heapAfter,
          (unsigned)r.lvFreeBefore, (unsigned)r.lvFreeAfter);
    // Whether the board rebooted between two runs, and why it last did. A
    // dropped connection mid-matrix is otherwise indistinguishable from a
    // panic (TEST_2.9.md T7).
    o.add("\"uptime_s\":%lu,\"reset_reason\":%d}\n",
          (unsigned long)(esp_timer_get_time() / 1000000), (int)esp_reset_reason());
    return o.len < JSON_CAP ? o.len : JSON_CAP - 1;
}

// Returns an error message for a bad query, nullptr when it is fine.
const char *parseQuery(httpd_req_t *req, Request &q) {
    char s[96], v[16];
    if (httpd_req_get_url_query_str(req, s, sizeof(s)) != ESP_OK) return nullptr;
    if (httpd_query_key_value(s, "n", v, sizeof(v)) == ESP_OK) {
        q.n = atoi(v);
        if (q.n < 1 || q.n > N_MAX) return "n must be 1-100";
    }
    if (httpd_query_key_value(s, "what", v, sizeof(v)) == ESP_OK) {
        if      (!strcmp(v, "full")) q.what = What::WHAT_FULL;
        else if (!strcmp(v, "card")) q.what = What::WHAT_CARD;
        else return "what must be full or card";
    }
    if (httpd_query_key_value(s, "page", v, sizeof(v)) == ESP_OK) {
        q.page = atoi(v);
        // pageCount() is a plain byte set once at boot; reading it here is safe.
        if (q.page < 0 || q.page >= s_gui->pageCount()) return "page out of range";
    }
    if (httpd_query_key_value(s, "deck", v, sizeof(v)) == ESP_OK) {
        if      (!strcmp(v, "0")) q.deck = 0;
        else if (!strcmp(v, "1")) q.deck = 1;
        else return "deck must be 0 or 1";
    }
    if (httpd_query_key_value(s, "keep", v, sizeof(v)) == ESP_OK) q.keep = !strcmp(v, "1");
    return nullptr;
}

esp_err_t handleBench(httpd_req_t *req) {
    Request q;
    if (const char *bad = parseQuery(req, q)) {
        return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, bad);
    }

    int expected = ST_IDLE;
    if (!s_state.compare_exchange_strong(expected, ST_CLAIMED)) {
        httpd_resp_set_status(req, "503 Service Unavailable");
        httpd_resp_set_hdr(req, "Retry-After", "5");
        return httpd_resp_sendstr(req, "A bench run is already in progress; try again.\n");
    }
    // CLAIMED: s_res is ours until the store below hands it to service().
    s_res = Result{};
    s_res.req = q;
    s_state.store(ST_REQUESTED);

    if (xSemaphoreTake(s_done, pdMS_TO_TICKS(PICKUP_WAIT_MS)) != pdTRUE) {
        expected = ST_REQUESTED;
        if (s_state.compare_exchange_strong(expected, ST_IDLE)) {
            Serial.println("[Bench] loop() did not pick the request up; gave up");
            httpd_resp_set_status(req, "503 Service Unavailable");
            return httpd_resp_sendstr(req, "The UI thread did not respond within 5 s.\n");
        }
        xSemaphoreTake(s_done, portMAX_DELAY);   // it did; a run always finishes
    }

    esp_err_t res;
    if (s_res.error) {
        Serial.printf("[Bench] failed: %s\n", s_res.error);
        res = httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, s_res.error);
    } else if (!(s_json = static_cast<char *>(heap_caps_malloc(JSON_CAP, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT)))) {
        res = httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "no PSRAM for the reply");
    } else {
        const size_t len = buildJson();
        const int n = s_res.req.n;
        Serial.printf("[Bench] %s p%u%s n=%d | total %lld us: render %lld, copy %lld, present %lld, wait %lld | %.1f chunks\n",
                      s_res.req.what == What::WHAT_CARD ? "card" : "full", (unsigned)s_res.page,
                      s_res.deck ? "+deck" : "", n,
                      (long long)s_res.total.avg(n), (long long)s_res.render.avg(n),
                      (long long)s_res.copy.avg(n), (long long)s_res.present.avg(n),
                      (long long)s_res.wait.avg(n), n ? (double)s_res.chunks / n : 0.0);
        httpd_resp_set_type(req, "application/json");
        httpd_resp_set_hdr(req, "Cache-Control", "no-store");
        res = httpd_resp_send(req, s_json, (ssize_t)len);
        heap_caps_free(s_json);
        s_json = nullptr;
    }
    s_state.store(ST_IDLE);
    return res;
}

} // namespace

namespace Bench {

void begin(HttpServer &http, GUIManager &gui) {
    s_gui = &gui;
    if (!s_done) s_done = xSemaphoreCreateBinary();
    if (!s_done) {
        Serial.println("[Bench] could not create semaphore; /bench disabled");
        return;
    }
    http.addRoute("/bench", HTTP_GET, handleBench);
}

void service() {
    int st = s_state.load();
    if (st == ST_REQUESTED) {
        if (!s_state.compare_exchange_strong(st, ST_RUNNING)) return;
        s_origPage = s_res.wasPage = s_gui->currentPage();
        s_origDeck = s_res.wasDeck = s_gui->deckShown();
        s_settleAt = millis();
        if (applyTarget(s_res.req.page, s_res.req.deck)) {
            s_settleEnd = s_settleAt + SETTLE_MS;
            s_phase     = Phase::PH_SETTLE;
            return;   // ordinary loop()s until settled
        }
        s_phase = Phase::PH_START;
    } else if (st == ST_RUNNING) {
        if (s_phase == Phase::PH_SETTLE && (int32_t)(millis() - s_settleEnd) < 0) return;
    } else {
        return;
    }

    s_res.settledMs = millis() - s_settleAt;
    measure();
    if (!s_res.req.keep) applyTarget(s_origPage, s_origDeck ? 1 : 0);   // put the board back as found

    s_state.store(ST_DONE);
    xSemaphoreGive(s_done);
}

} // namespace Bench

#endif // ENABLE_BENCH
