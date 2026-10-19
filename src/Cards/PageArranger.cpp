// The arranger for the dashboard's pages (2.11 round 2): include/Cards/PageArranger.h
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

constexpr size_t VALUE_MAX = 16 + 12 * Layout::ARR_BLOCKS_MAX;   // "v1 fp CxR" and "c,r,w,h,o" a block

// The running job: its own copy of everything it reads.
Input              s_in;
char               s_slug[32];
Layout::ArrWeights s_w;
Layout::ArrKnobs   s_k;
Layout::ArrResult  s_res;
uint32_t           s_fp = 0;
uint32_t           s_failFp = 0;   // blocks no page holds: not tried again on every build
int64_t            s_us = 0;
// 0 idle, 1 running, 2 an answer waiting to be kept (on the LVGL thread).
std::atomic<uint8_t> s_state{0};
char               s_last[160] = "not run since boot";   // the last run
char               s_use[96]   = "";                     // how the last page built was placed
char               s_text[264];

double num(const char *key, double dflt) {
    char v[16];
    if (!Settings::system("arranger", key, v, sizeof(v)) || !v[0]) return dflt;
    return atof(v);
}

void tuning(Layout::ArrWeights &w, Layout::ArrKnobs &k) {
    const Layout::ArrWeights dw;
    const Layout::ArrKnobs dk;
    w.hole  = num("hole", dw.hole);
    w.comfy = num("comfy", dw.comfy);
    w.loose = num("loose", dw.loose);
    w.lng   = num("long", dw.lng);
    w.order = num("order", dw.order);
    w.away  = num("away", dw.away);
    k.maxCell = num("maxcell", dk.maxCell);
    k.minCell = num("mincell", dk.minCell);
    k.tileW   = num("tilew", dk.tileW);
    k.tileH   = num("tileh", dk.tileH);
    k.hdr     = num("hdr", dk.hdr);
}

// FNV-1a over everything an answer depends on.
struct Fp {
    uint32_t h = 2166136261u;
    void byte(uint8_t b) { h ^= b; h *= 16777619u; }
    void i32(int32_t v) { for (int i = 0; i < 4; i++) byte((uint8_t)(v >> (8 * i))); }
    void str(const char *s) { for (; s && *s; s++) byte((uint8_t)*s); byte(0); }
    void dbl(double v) { i32((int32_t)lround(v * 1000.0)); }
};

uint32_t fingerprint(const Input &in, const Layout::ArrWeights &w, const Layout::ArrKnobs &k) {
    Fp f;
    f.str(in.slug);
    f.i32(in.n);
    for (uint8_t i = 0; i < in.n; i++) {
        const Layout::ArrBlock &b = in.blocks[i];
        f.byte(b.cluster); f.byte(b.tiles); f.byte(b.hdr); f.byte(b.members); f.byte(b.pinW); f.byte(b.pinH);
        f.str(in.ids[i]);
    }
    const Layout::ArrScreen &s = in.scr;
    f.dbl(s.pmm); f.dbl(s.aw); f.dbl(s.ah); f.dbl(s.gap); f.dbl(s.tagH);
    f.byte(s.recCols); f.byte(s.recRows); f.byte(s.fixCols); f.byte(s.fixRows);
    f.dbl(w.hole); f.dbl(w.comfy); f.dbl(w.loose); f.dbl(w.lng); f.dbl(w.order); f.dbl(w.away);
    f.dbl(k.maxCell); f.dbl(k.minCell); f.dbl(k.tileW); f.dbl(k.tileH); f.dbl(k.hdr);
    return f.h;
}

void task(void *) {
    const int64_t t0 = esp_timer_get_time();
    Layout::arrange(s_in.scr, s_in.blocks, s_in.n, s_w, s_k, s_res);
    s_us = esp_timer_get_time() - t0;
    s_state.store(2);
    vTaskDelete(nullptr);
}

}  // namespace

bool kept(const Input &in, Layout::ArrResult &out) {
    out = Layout::ArrResult();
    if (!in.slug || !in.n) return false;
    Layout::ArrWeights w;
    Layout::ArrKnobs k;
    tuning(w, k);
    char v[VALUE_MAX];
    if (!Settings::page(in.slug, "layout", v, sizeof(v))) return false;
    // "v1 <fingerprint> <cols>x<rows> c,r,w,h,one ..." - one place per block, in order.
    char *p = v;
    if (strncmp(p, "v1 ", 3)) return false;
    p += 3;
    const uint32_t fp = (uint32_t)strtoul(p, &p, 16);
    if (fp != fingerprint(in, w, k)) return false;
    const long cols = strtol(p, &p, 10);
    if (*p != 'x') return false;
    const long rows = strtol(p + 1, &p, 10);
    if (cols < 1 || rows < 1) return false;
    for (uint8_t i = 0; i < in.n; i++) {
        long q[5];
        for (int j = 0; j < 5; j++) {
            while (*p == ' ' || *p == ',') p++;
            char *e;
            q[j] = strtol(p, &e, 10);
            if (e == p) return false;
            p = e;
        }
        out.pos[i] = { (uint8_t)q[0], (uint8_t)q[1], (uint8_t)q[2], (uint8_t)q[3], q[4] != 0 };
    }
    out.ok = true;
    out.cols = (uint8_t)cols;
    out.rows = (uint8_t)rows;
    snprintf(s_use, sizeof(s_use), "%s built from its kept answer, %u blocks on %ldx%ld",
             in.slug, (unsigned)in.n, cols, rows);
    return true;
}

void start(const Input &in) {
    if (s_state.load() != 0 || !in.slug || !in.n) return;
    s_in = in;
    snprintf(s_slug, sizeof(s_slug), "%s", in.slug);
    s_in.slug = s_slug;
    // The ids are only fingerprinted, and that is done here, with the page alive.
    tuning(s_w, s_k);
    s_fp = fingerprint(in, s_w, s_k);
    if (s_fp == s_failFp) return;
    for (uint8_t i = 0; i < Layout::ARR_BLOCKS_MAX; i++) s_in.ids[i] = nullptr;
    s_state.store(1);
    // Low priority, so the screen and the network go first; its stack in
    // internal RAM. The search's own room is made once, from the heap.
    if (xTaskCreatePinnedToCore(task, "arrange", 8192, nullptr, 1, nullptr, 0) != pdPASS) {
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
        int len = snprintf(v, sizeof(v), "v1 %08lx %ux%u", (unsigned long)s_fp, (unsigned)s_res.cols,
                           (unsigned)s_res.rows);
        for (uint8_t i = 0; i < s_in.n && len < (int)sizeof(v); i++) {
            const Layout::ArrPlace &q = s_res.pos[i];
            len += snprintf(v + len, sizeof(v) - len, " %u,%u,%u,%u,%u", (unsigned)q.c, (unsigned)q.r,
                            (unsigned)q.w, (unsigned)q.h, q.one ? 1u : 0u);
        }
        Settings::setPage(s_slug, "layout", v);
        Settings::save();
        keptNew = true;
        snprintf(s_last, sizeof(s_last), "%s: %u blocks on %ux%u, %u empty, score %.1f; %lu steps%s in %lld ms",
                 s_slug, (unsigned)s_in.n, (unsigned)s_res.cols, (unsigned)s_res.rows, (unsigned)s_res.holes,
                 s_res.score, (unsigned long)s_res.nodes, s_res.capped ? " (a grid hit the cap)" : "",
                 (long long)(s_us / 1000));
    } else {
        s_failFp = s_fp;
        snprintf(s_last, sizeof(s_last), "%s: no page holds these %u blocks (%lu steps in %lld ms) - flowed",
                 s_slug, (unsigned)s_in.n, (unsigned long)s_res.nodes, (long long)(s_us / 1000));
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
