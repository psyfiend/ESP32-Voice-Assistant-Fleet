// The arranger for the dashboard's pages (2.11): include/Cards/PageArranger.h
// says what and why.
#include "Cards/PageArranger.h"
#include "Settings.h"
#include <Arduino.h>
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <atomic>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

namespace PageArranger {

namespace {

// "v3 fp CxR" and "c,r,w,h,tc" a block, in units.
constexpr size_t VALUE_MAX = 24 + 16 * Layout::ARR_BLOCKS_MAX;

// The search recurses a level a unit (Layout::ARR_CELLS_MAX at most), each
// level ~100 bytes; the high-water mark is logged after every run.
constexpr uint32_t TASK_STACK = 24576;

// The running job: its own copy of everything it reads.
Input              s_in;
char               s_slug[32];
Layout::ArrWeights s_w;
Layout::ArrResult  s_res;
uint32_t           s_fp = 0;
uint32_t           s_failFp = 0;   // blocks no page holds: not tried again on every build
int64_t            s_us = 0;
uint32_t           s_stackFree = 0;   // bytes the task never touched
// 0 idle, 1 running, 2 an answer waiting to be kept (on the LVGL thread).
std::atomic<uint8_t> s_state{0};
char               s_last[200] = "not run since boot";   // the last run
char               s_use[96]   = "";                     // how the last page built was placed
char               s_text[320];

double num(const char *key, double dflt) {
    char v[16];
    if (!Settings::system("arranger", key, v, sizeof(v)) || !v[0]) return dflt;
    return atof(v);
}

// The owner's knobs (K109): system.arranger.*, else the playground's defaults.
void tuning(Layout::ArrWeights &w) {
    const Layout::ArrWeights d;
    w.size   = num("size", d.size);
    w.square = num("square", d.square);
    w.wide   = num("wide", d.wide);
    w.twin   = num("twin", d.twin);
    w.gap8   = num("gap8", d.gap8);
    w.order  = num("order", d.order);
    w.big    = num("big", d.big);
}

// FNV-1a over everything an answer depends on.
struct Fp {
    uint32_t h = 2166136261u;
    void byte(uint8_t b) { h ^= b; h *= 16777619u; }
    void i32(int32_t v) { for (int i = 0; i < 4; i++) byte((uint8_t)(v >> (8 * i))); }
    void str(const char *s) { for (; s && *s; s++) byte((uint8_t)*s); byte(0); }
    void dbl(double v) { i32((int32_t)lround(v * 1000.0)); }
};

uint32_t fingerprint(const Input &in, const Layout::ArrWeights &w) {
    Fp f;
    f.byte(9);   // the arranger's version: v9
    f.str(in.slug);
    f.i32(in.n);
    for (uint8_t i = 0; i < in.n; i++) {
        const Layout::ArrBlock &b = in.blocks[i];
        f.byte(b.cluster); f.byte(b.tiles); f.byte(b.hdr); f.byte(b.members); f.dbl(b.aspect);
        f.str(in.ids[i]);
    }
    const Layout::ArrScreen &s = in.scr;
    f.dbl(s.pmm); f.dbl(s.aw); f.dbl(s.ah); f.dbl(s.gap); f.dbl(s.tagH); f.byte((uint8_t)s.label);
    f.byte(s.recCols); f.byte(s.recRows); f.byte(s.fixCols); f.byte(s.fixRows);
    f.dbl(w.size); f.dbl(w.square); f.dbl(w.wide); f.dbl(w.twin); f.dbl(w.gap8); f.dbl(w.order); f.dbl(w.big);
    return f.h;
}

void task(void *) {
    const int64_t t0 = esp_timer_get_time();
    Layout::arrange(s_in.scr, s_in.blocks, s_in.n, s_w, s_res);
    s_us = esp_timer_get_time() - t0;
    s_stackFree = (uint32_t)uxTaskGetStackHighWaterMark(nullptr);   // bytes, in ESP-IDF
    s_state.store(2);
    vTaskDelete(nullptr);
}

}  // namespace

bool kept(const Input &in, Layout::ArrResult &out) {
    out = Layout::ArrResult();
    if (!in.slug || !in.n) return false;
    Layout::ArrWeights w;
    tuning(w);
    char v[VALUE_MAX];
    if (!Settings::page(in.slug, "layout", v, sizeof(v))) return false;
    // "v3 <fingerprint> <cols>x<rows> c,r,w,h,tc ..." - units; each block as the
    // search placed it, a cluster's tiles across; one per block, in order. The
    // finish (Layout::finishPage()) is run again here: cheap, and the same every
    // time. Older answers (v1 cells, v2 round 9's finished boxes) are not read.
    char *p = v;
    if (strncmp(p, "v3 ", 3)) return false;
    p += 3;
    const uint32_t fp = (uint32_t)strtoul(p, &p, 16);
    if (fp != fingerprint(in, w)) return false;
    const long cols = strtol(p, &p, 10);
    if (*p != 'x') return false;
    const long rows = strtol(p + 1, &p, 10);
    if (cols < 1 || rows < 1 || cols > Layout::ARR_UNITS_MAX || rows > Layout::ARR_UNITS_MAX) return false;
    for (uint8_t i = 0; i < in.n; i++) {
        long q[5];
        for (int j = 0; j < 5; j++) {
            while (*p == ' ' || *p == ',') p++;
            char *e;
            q[j] = strtol(p, &e, 10);
            if (e == p) return false;
            p = e;
        }
        if (q[0] < 0 || q[1] < 0 || q[2] < 1 || q[3] < 1 || q[0] + q[2] > cols || q[1] + q[3] > rows) return false;
        Layout::ArrPlace &b = out.pos[i];
        b.c = (uint8_t)q[0]; b.r = (uint8_t)q[1]; b.w = (uint8_t)q[2]; b.h = (uint8_t)q[3];
        b.tc = (uint8_t)q[4];
    }
    out.ok = true;
    out.cols = (uint8_t)cols;
    out.rows = (uint8_t)rows;
    Layout::finishPage(in.scr, in.blocks, in.n, out.cols, out.rows, out.pos, out.box, out.rect);
    snprintf(s_use, sizeof(s_use), "%s built from its kept answer, %u blocks on %ldx%ld units",
             in.slug, (unsigned)in.n, cols, rows);
    return true;
}

void start(const Input &in) {
    if (s_state.load() != 0 || !in.slug || !in.n) return;
    s_in = in;
    snprintf(s_slug, sizeof(s_slug), "%s", in.slug);
    s_in.slug = s_slug;
    // The ids are only fingerprinted, and that is done here, with the page alive.
    tuning(s_w);
    s_fp = fingerprint(in, s_w);
    if (s_fp == s_failFp) return;
    for (uint8_t i = 0; i < Layout::ARR_BLOCKS_MAX; i++) s_in.ids[i] = nullptr;
    s_state.store(1);
    // Low priority, so the screen and the network go first; its stack in
    // internal RAM. The search's own room is made once, from the heap.
    if (xTaskCreatePinnedToCore(task, "arrange", TASK_STACK, nullptr, 1, nullptr, 0) != pdPASS) {
        s_state.store(0);
        snprintf(s_last, sizeof(s_last), "%s: could not start the arranger", s_slug);
    }
    snprintf(s_use, sizeof(s_use), "%s flowed while it is arranged", s_slug);
}

bool takeDone() {
    if (s_state.load() != 2) return false;
    bool keptNew = false;
    if (s_res.ok) {
        char v[VALUE_MAX];
        int len = snprintf(v, sizeof(v), "v3 %08lx %ux%u", (unsigned long)s_fp, (unsigned)s_res.cols,
                           (unsigned)s_res.rows);
        for (uint8_t i = 0; i < s_in.n && len < (int)sizeof(v); i++) {
            const Layout::ArrPlace &q = s_res.pos[i];
            len += snprintf(v + len, sizeof(v) - len, " %u,%u,%u,%u,%u", (unsigned)q.c, (unsigned)q.r,
                            (unsigned)q.w, (unsigned)q.h, (unsigned)q.tc);
        }
        Settings::setPage(s_slug, "layout", v);
        Settings::save();
        keptNew = true;
        snprintf(s_last, sizeof(s_last),
                 "%s: %u blocks on %ux%u units, %u empty, score %.2f, entities %.1f mm; %lu steps%s in %lld ms, "
                 "stack %lu of %lu free",
                 s_slug, (unsigned)s_in.n, (unsigned)s_res.cols, (unsigned)s_res.rows, (unsigned)s_res.holes,
                 s_res.score, s_res.mean, (unsigned long)s_res.nodes, s_res.capped ? " (its grid hit the budget)" : "",
                 (long long)(s_us / 1000), (unsigned long)s_stackFree, (unsigned long)TASK_STACK);
    } else {
        s_failFp = s_fp;
        snprintf(s_last, sizeof(s_last), "%s: no page holds these %u blocks (%lu steps in %lld ms, stack %lu free) - flowed",
                 s_slug, (unsigned)s_in.n, (unsigned long)s_res.nodes, (long long)(s_us / 1000),
                 (unsigned long)s_stackFree);
    }
    Serial.printf("[Arranger] %s\n", s_last);
    s_state.store(0);
    return keptNew;
}

void flowed(const char *slug, const char *why) {
    snprintf(s_use, sizeof(s_use), "%s flowed (%s)", slug ? slug : "?", why ? why : "");
}

const char *lastText() {
    snprintf(s_text, sizeof(s_text), "last run: %s%s; now: %s", s_last,
             s_state.load() == 1 ? " (one running)" : "", s_use[0] ? s_use : "-");
    return s_text;
}

}  // namespace PageArranger
