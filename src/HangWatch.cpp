// HangWatch - see HangWatch.h.
#ifdef DEBUG_HANG

#include "HangWatch.h"

#include <Arduino.h>   // Serial, millis
#include <stdio.h>
#include <atomic>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "HttpServer.h"

namespace {

constexpr uint32_t STALL_MS   = 3000;
constexpr uint8_t  SAMPLES    = 3;
constexpr uint8_t  STACK_MAX  = 48;      // code addresses kept from the stack

TaskHandle_t          s_loop = nullptr;
std::atomic<uint32_t> s_beat{0};
std::atomic<bool>     s_test{false};
char                  s_report[4096] = "no hang seen since boot\n";
int                 (*s_extra)(char *, size_t) = nullptr;

// Flash-mapped code (and read-only data, which shares the range on the P4:
// addr2line answers "??" for those). Return addresses are 2-byte aligned.
bool codeAddr(uint32_t w) { return w >= 0x40000000u && w < 0x44000000u && !(w & 1u); }

const char *stateName(eTaskState s) {
    switch (s) {
        case eRunning:   return "running";
        case eReady:     return "ready (spinning)";
        case eBlocked:   return "blocked (waiting on something)";
        case eSuspended: return "suspended";
        default:         return "deleted?";
    }
}

// loopTask is suspended: its registers were saved on its own stack, and the
// first word of the TCB points at them. The IDF RISC-V frame starts mepc, ra,
// sp (riscv/rvruntime-frames.h).
void capture(char *out, size_t cap, int &n, bool withStack) {
    const uint32_t *frame = *(const uint32_t * const *)s_loop;
    const uint32_t pc = frame[0], ra = frame[1], sp = frame[2];
    n += snprintf(out + n, cap - n, "  pc 0x%08lx  ra 0x%08lx  sp 0x%08lx\n",
                  (unsigned long)pc, (unsigned long)ra, (unsigned long)sp);
    if (!withStack) return;
    const uint32_t lo  = (uint32_t)pxTaskGetStackStart(s_loop);
    const uint32_t top = lo + 16 * 1024;   // SET_LOOP_TASK_STACK_SIZE in main.cpp
    if (sp < lo || sp >= top) { n += snprintf(out + n, cap - n, "  sp outside the stack\n"); return; }
    n += snprintf(out + n, cap - n, "  stack, innermost first:");
    uint8_t k = 0;
    for (const uint32_t *p = (const uint32_t *)sp; (uint32_t)p < top && k < STACK_MAX; p++) {
        if (!codeAddr(*p)) continue;
        n += snprintf(out + n, cap - n, "%s0x%08lx", (k % 6) ? " " : "\n   ", (unsigned long)*p);
        k++;
    }
    n += snprintf(out + n, cap - n, "\n");
}

void watchTask(void *) {
    bool reported = false;
    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(500));
        const uint32_t b = s_beat.load();
        if (!b) continue;                                   // loop() not started yet
        // Signed: loop() may beat between the two reads, a beat "in the future".
        const int32_t idle = (int32_t)(millis() - b);
        if (idle < (int32_t)STALL_MS) { reported = false; continue; }
        if (reported) continue;
        reported = true;

        static char out[sizeof(s_report)];
        int n = snprintf(out, sizeof(out), "HANG at %lu ms: loop() last ran %lu ms ago, loopTask %s\n",
                         (unsigned long)millis(), (unsigned long)idle, stateName(eTaskGetState(s_loop)));
        for (uint8_t i = 0; i < SAMPLES && n < (int)sizeof(out) - 400; i++) {
            vTaskSuspend(s_loop);
            n += snprintf(out + n, sizeof(out) - n, "sample %u:\n", (unsigned)(i + 1));
            capture(out, sizeof(out), n, i == 0);
            vTaskResume(s_loop);
            vTaskDelay(pdMS_TO_TICKS(150));
        }
        n += snprintf(out + n, sizeof(out) - n, "decode: addr2line -pfiaC -e firmware.elf <addresses>\n");
        if (s_extra && n < (int)sizeof(out) - 1) n += s_extra(out + n, sizeof(out) - n);
        memcpy(s_report, out, sizeof(s_report));
        Serial.print(s_report);
    }
}

esp_err_t handleHang(httpd_req_t *req) {
    char q[16], v[4];
    if (httpd_req_get_url_query_str(req, q, sizeof(q)) == ESP_OK &&
        httpd_query_key_value(q, "test", v, sizeof(v)) == ESP_OK && atoi(v)) {
        s_test = true;
        httpd_resp_set_type(req, "text/plain");
        return httpd_resp_sendstr(req, "loop() will spin for 4 s in HangWatch::beat(): read /hang after\n");
    }
    httpd_resp_set_type(req, "text/plain");
    return httpd_resp_send(req, s_report, HTTPD_RESP_USE_STRLEN);
}

} // namespace

namespace HangWatch {
void begin(HttpServer &http) {
    s_loop = xTaskGetCurrentTaskHandle();
    xTaskCreatePinnedToCore(watchTask, "hangwatch", 4096, nullptr, configMAX_PRIORITIES - 2, nullptr, 0);
    http.addRoute("/hang", HTTP_GET, handleHang);
    Serial.println("[Hang] watching loop() - a stall of 3 s is reported at /hang");
}

void setExtra(int (*fn)(char *, size_t)) { s_extra = fn; }

void beat() {
    s_beat.store(millis() | 1u);
    // /hang?test=1: a stall made on purpose, to see the report name this line.
    if (s_test.exchange(false)) { const uint32_t t = millis(); while (millis() - t < 4000) { } }
}
}

#endif // DEBUG_HANG
