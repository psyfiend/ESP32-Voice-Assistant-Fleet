// PanelDebug - see PanelDebug.h.
#if defined(DISPLAY_ESPLCD) && defined(HAS_MIPI_PANEL)

#include "PanelDebug.h"

#include <Arduino.h>   // Serial
#include <stdio.h>
#include "esp_timer.h"
#include "nvs.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "soc/mipi_dsi_bridge_struct.h"
#include "HttpServer.h"
#include "Fleet_Display.h"
#include "bsp_loader.h"

namespace {

Fleet_Display *s_disp = nullptr;

// --- Underruns -------------------------------------------------------------
//
// The bridge's raw status bit is set by the hardware and stays set until
// written clear. ESP-IDF's own bridge interrupt (esp_lcd_panel_dpi.c,
// mipi_dsi_bridge_isr_handler) is enabled by esp_lcd_new_panel_dpi() - read on
// WS_P4_5 2026-10-08: int_ena 0x1 - and clears the bit the moment it is set,
// printing "can't fetch data from external memory fast enough, underrun
// happens" from the interrupt. A poll would then never see it, so begin()
// turns that interrupt off and the latch is ours: counted here, logged by
// poll(), and readable from a PC in /panel's reply. One count is "a poll found
// the latch set" - several starved frames between two polls count once.
portMUX_TYPE s_mux = portMUX_INITIALIZER_UNLOCKED;
uint32_t s_underruns = 0;
constexpr uint8_t RECENT = 8;
uint32_t s_recentMs[RECENT] = {};   // millis() of the last few, newest at s_recentHead-1
uint8_t  s_recentHead = 0;
uint32_t s_loggedCount = 0;          // the count at the last log line
uint32_t s_loggedMs = 0;

// Returns how many were found by this call (0 or 1: the bit is a latch).
uint32_t sample() {
    uint32_t found = 0;
    portENTER_CRITICAL(&s_mux);
    if (MIPI_DSI_BRIDGE.int_raw.underrun_int_raw) {
        MIPI_DSI_BRIDGE.int_clr.underrun_int_clr = 1;
        s_underruns++;
        s_recentMs[s_recentHead] = millis();
        s_recentHead = (uint8_t)((s_recentHead + 1) % RECENT);
        found = 1;
    }
    portEXIT_CRITICAL(&s_mux);
    return found;
}

uint32_t underrunCount() {
    portENTER_CRITICAL(&s_mux);
    const uint32_t n = s_underruns;
    portEXIT_CRITICAL(&s_mux);
    return n;
}

// --- Flash-write test ------------------------------------------------------
//
// NVS WRITES ONLY (#98, the owner's choice of fix, 2026-10-14). There was an
// erase mode too: erase-and-write cycles at the start of the "spiffs" data
// partition, written when that partition was unused. Since 2.10d it is the
// LittleFS partition that holds /cfg/settings.json, and erasing its first
// blocks destroys the file system's superblock - the next boot formats it and
// every saved setting is gone. The erase test had already answered its
// question (DECISIONS A14), so it was removed rather than fenced; a request
// for it is refused with the reason.

constexpr int      FLASH_TEST_MAX = 50;
constexpr uint32_t FLASH_TEST_GAP_MS = 150;

// One NVS write of 512 bytes, committed, in the scratch namespace. Returns
// microseconds, or -1 on failure.
int64_t flashCycle(int i, nvs_handle_t h) {
    static uint8_t buf[512];
    memset(buf, (uint8_t)(0x30 + (i & 0x3F)), sizeof(buf));
    const int64_t t0 = esp_timer_get_time();
    esp_err_t err = nvs_set_blob(h, "t", buf, sizeof(buf));
    if (err == ESP_OK) err = nvs_commit(h);
    const int64_t us = esp_timer_get_time() - t0;
    return err == ESP_OK ? us : -1;
}

// THE WRITES RUN ON A TASK OF THEIR OWN, WITH ITS STACK IN INTERNAL RAM.
// A flash write turns the cache off, and ESP-IDF asserts that the calling
// task's stack is not in PSRAM (cache_utils.c, esp_task_stack_is_sane_cache_
// disabled). The HTTP server's task stack IS in PSRAM (HttpServer.cpp,
// task_caps), so the first version of this test rebooted the board from the
// handler. xTaskCreate() puts a stack in internal RAM.
//
// WHAT THE RECORDER CANNOT SEE (read 2026-10-08). The bridge masks underruns in
// a frame's first DPI_MISC_CONFIG.fifo_underrun_discard_vcnt lines (413 by
// default), where the FIFO is empty by design while the frame's transfer
// starts. A late START - the flash-write case, where the interrupt that starts
// each frame's transfer waits for the cache - lands there. So a zero from this
// test says the feed never ran dry later in a frame; whether a write is
// visible on the glass takes eyes. Two controls were tried and removed: PSRAM
// copies on both cores for 2 s latched nothing, and turning the cache off by
// hand (spi_flash_disable_interrupts_caches_and_other_cpu) panicked with a
// cache error, which no real flash write did.
struct FlashJob {
    int  n = 0;
    bool ok = false;
    int64_t  us[FLASH_TEST_MAX] = {};
    uint32_t got[FLASH_TEST_MAX] = {};
    uint32_t hits = 0;
    SemaphoreHandle_t done = nullptr;
};
FlashJob s_job;

void flashTask(void *) {
    FlashJob &j = s_job;
    nvs_handle_t h = 0;
    j.ok = nvs_open("fleet_test", NVS_READWRITE, &h) == ESP_OK;
    j.hits = 0;
    for (int i = 0; j.ok && i < j.n; i++) {
        // Counts, not sample()'s return: loop() samples the same latch
        // concurrently and may be the one to find it.
        sample();                                   // anything before is not ours
        const uint32_t before = underrunCount();
        j.us[i] = flashCycle(i, h);
        vTaskDelay(pdMS_TO_TICKS(40));              // let a starved frame finish and latch
        sample();
        j.got[i] = underrunCount() - before;
        j.hits += j.got[i];
        vTaskDelay(pdMS_TO_TICKS(FLASH_TEST_GAP_MS));
    }
    if (j.ok) { nvs_erase_all(h); nvs_commit(h); nvs_close(h); }
    xSemaphoreGive(j.done);
    vTaskDelete(nullptr);
}

// NO READS (2.10c round 9). The first version read the panel's status
// registers over DSI; with that read also done at boot, all three P4s came up
// with the backlight on and a black screen, the boards otherwise running. A
// DSI read while the panel streams video stops the picture.
esp_err_t handlePanel(httpd_req_t *req) {
    char q[48], v[8];
    bool resend = false, nvs = false;
    int flash = 0;
    if (httpd_req_get_url_query_str(req, q, sizeof(q)) == ESP_OK) {
        if (httpd_query_key_value(q, "resend", v, sizeof(v)) == ESP_OK) resend = atoi(v) != 0;
        if (httpd_query_key_value(q, "flash", v, sizeof(v)) == ESP_OK)  flash = atoi(v);
        if (httpd_query_key_value(q, "nvs", v, sizeof(v)) == ESP_OK)    nvs = atoi(v) != 0;
    }
    if (flash > FLASH_TEST_MAX) flash = FLASH_TEST_MAX;

    static char out[2600];
    int n = snprintf(out, sizeof(out), "%s, driver %s\n", bsp_display.PANEL_MODEL, s_disp->driverName());

    sample();
    {
        portENTER_CRITICAL(&s_mux);
        const uint32_t cnt = s_underruns;
        uint32_t recent[RECENT];
        for (uint8_t k = 0; k < RECENT; k++) recent[k] = s_recentMs[(s_recentHead + RECENT - 1 - k) % RECENT];
        const uint32_t ena = MIPI_DSI_BRIDGE.int_ena.val;
        portEXIT_CRITICAL(&s_mux);
        n += snprintf(out + n, sizeof(out) - n, "underruns since boot: %lu (bridge int_ena 0x%lx; now %lu ms)\n",
                      (unsigned long)cnt, (unsigned long)ena, (unsigned long)millis());
        for (uint8_t k = 0; k < RECENT && k < cnt; k++)
            n += snprintf(out + n, sizeof(out) - n, "  at %lu ms\n", (unsigned long)recent[k]);
    }

    if (flash > 0 && !nvs) {
        // The erase mode is gone (#98): it erased the settings file system.
        n += snprintf(out + n, sizeof(out) - n,
                      "flash test refused: only the NVS mode is kept - /panel?flash=N&nvs=1. The erase mode\n"
                      "erased the start of the partition that holds the settings file (#98), so it was removed.\n");
    } else if (flash > 0) {
        if (!s_job.done) s_job.done = xSemaphoreCreateBinary();
        s_job.n = flash;
        Serial.printf("[Panel] flash test: %d NVS cycles\n", flash);
        const bool started = xTaskCreate(flashTask, "flashtest", 4096, nullptr, 5, nullptr) == pdPASS;
        const TickType_t wait = pdMS_TO_TICKS((uint32_t)flash * (FLASH_TEST_GAP_MS + 500) + 2000);
        if (!started || xSemaphoreTake(s_job.done, wait) != pdTRUE) {
            n += snprintf(out + n, sizeof(out) - n, "flash test: %s\n", started ? "timed out" : "task not started");
        } else if (!s_job.ok) {
            n += snprintf(out + n, sizeof(out) - n, "flash test: no NVS namespace\n");
        } else {
            n += snprintf(out + n, sizeof(out) - n, "flash test: %d NVS writes of 512 B, %lu ms apart\n", flash,
                          (unsigned long)FLASH_TEST_GAP_MS);
            for (int i = 0; i < flash && n < (int)sizeof(out) - 64; i++)
                n += snprintf(out + n, sizeof(out) - n, "  %2d: %6.1f ms%s\n", i,
                              s_job.us[i] < 0 ? -1.0 : s_job.us[i] / 1000.0, s_job.got[i] ? "  UNDERRUN" : "");
            n += snprintf(out + n, sizeof(out) - n, "cycles followed by an underrun: %lu of %d\n",
                          (unsigned long)s_job.hits, flash);
            Serial.printf("[Panel] flash test done: %lu of %d cycles followed by an underrun\n",
                          (unsigned long)s_job.hits, flash);
        }
    }

    if (resend) {
        const int64_t t0 = esp_timer_get_time();
        const int sent = s_disp->resendPanelInit();
        const long ms = (long)((esp_timer_get_time() - t0) / 1000);
        if (sent < 0) n += snprintf(out + n, sizeof(out) - n, "resend FAILED (or no init sequence in the BSP)\n");
        else          n += snprintf(out + n, sizeof(out) - n, "resent %d init commands in %ld ms\n", sent, ms);
        Serial.printf("[Panel] init sequence resent from /panel: %d commands\n", sent);
    } else if (!flash) {
        n += snprintf(out + n, sizeof(out) - n,
                      "/panel?resend=1 sends the panel its init sequence again\n"
                      "/panel?flash=N&nvs=1 tests N NVS writes (a scratch namespace, erased after)\n");
    }
    httpd_resp_set_type(req, "text/plain");
    return httpd_resp_send(req, out, n);
}

} // namespace

namespace PanelDebug {
void begin(HttpServer &http, Fleet_Display &display) {
    s_disp = &display;
    // See "Underruns" above: the latch becomes ours.
    portENTER_CRITICAL(&s_mux);
    MIPI_DSI_BRIDGE.int_ena.underrun_int_ena = 0;
    MIPI_DSI_BRIDGE.int_clr.underrun_int_clr = 1;
    portEXIT_CRITICAL(&s_mux);
    http.addRoute("/panel", HTTP_GET, handlePanel);
}

// Every loop(). Logs at most once a second, with how many since the last line,
// so a burst cannot flood the UART (a blocked UART stalls the loop - LESSONS).
void poll() {
    if (!s_disp) return;
    sample();
    const uint32_t cnt = underrunCount();
    if (cnt == s_loggedCount) return;
    const uint32_t now = millis();
    if (s_loggedMs && now - s_loggedMs < 1000) return;
    Serial.printf("[Panel] UNDERRUN: %lu since boot (+%lu) at %lu ms\n",
                  (unsigned long)cnt, (unsigned long)(cnt - s_loggedCount), (unsigned long)now);
    s_loggedCount = cnt;
    s_loggedMs = now;
}
}

#endif
