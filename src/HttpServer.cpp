#include "HttpServer.h"

#include <Arduino.h>          // Serial only
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_heap_caps.h>

bool HttpServer::addRoute(const char *uri, httpd_method_t method, Handler handler) {
    if (_count >= MAX_ROUTES) {
        Serial.printf("[HTTP] route table full (%u); %s not added\n", MAX_ROUTES, uri);
        return false;
    }
    _routes[_count] = { uri, method, handler };
    const uint8_t i = _count++;
    return _server ? registerRoute(i) : true;
}

void HttpServer::loop(bool online) {
    if (_server || _failed || _count == 0 || !online) return;
    if (!start()) _failed = true;
}

bool HttpServer::start() {
    httpd_config_t cfg = HTTPD_DEFAULT_CONFIG();
    cfg.server_port      = PORT;
    cfg.max_uri_handlers = MAX_ROUTES;

    // --= The server task: where it runs, and why =--
    //
    // PINNED TO THE LVGL CORE, AT THE LVGL TASK'S PRIORITY (1). A screenshot
    // spends seconds in the PNG encoder on this task. FreeRTOS time-slices
    // equal priorities on one core, so the UI keeps drawing - slower - while it
    // runs, instead of freezing for the whole encode.
    //
    // The alternatives both fail in a way worth writing down. IDF's default is
    // priority 5, unpinned: that outranks loopTask and would freeze the screen
    // for the encode. On core 0 at any priority above idle, a multi-second
    // encode starves IDLE0, and this framework builds with the task watchdog
    // watching IDLE0 and set to PANIC after 5 s - a reboot, not a warning.
    // Core 1's idle task is not watched (CONFIG_ESP_TASK_WDT_CHECK_IDLE_TASK_CPU1
    // is unset in both esp32s3 and esp32p4_es sdkconfigs, checked 2026-09-24).
    //
    // begin-from-loop() means xPortGetCoreID() is loopTask's core, which keeps
    // this correct without naming an Arduino macro.
    cfg.task_priority = tskIDLE_PRIORITY + 1;
    cfg.core_id       = xPortGetCoreID();

    // STACK IN PSRAM. Internal RAM is the scarcest thing on this fleet
    // (LESSONS.md), and CYD_S3_3248 in particular cannot spare 8 KB. Both
    // framework variants set CONFIG_SPIRAM_ALLOW_STACK_EXTERNAL_MEMORY. The
    // known limit - a PSRAM-stacked task must not run while the flash cache is
    // disabled - does not apply: nothing here writes flash.
    cfg.stack_size = 8192;
    cfg.task_caps  = MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT;

    // A handful of clients at most. Every socket is a slice of LWIP's pool of
    // 16, which MQTT, the HA websocket and HA REST also draw on. Purging the
    // least recently used keeps a browser's idle keep-alive from locking out
    // the next request.
    cfg.max_open_sockets  = 3;
    cfg.lru_purge_enable  = true;
    cfg.send_wait_timeout = 10;

    esp_err_t err = httpd_start(&_server, &cfg);
    if (err != ESP_OK) {
        Serial.printf("[HTTP] httpd_start failed: %s\n", esp_err_to_name(err));
        _server = nullptr;
        return false;
    }

    for (uint8_t i = 0; i < _count; i++) registerRoute(i);

    Serial.printf("[HTTP] server up on port %u, %u route(s):", PORT, _count);
    for (uint8_t i = 0; i < _count; i++) Serial.printf(" %s", _routes[i].uri);
    Serial.println();
    return true;
}

bool HttpServer::registerRoute(uint8_t i) {
    httpd_uri_t u = {};
    u.uri      = _routes[i].uri;
    u.method   = _routes[i].method;
    u.handler  = _routes[i].handler;
    u.user_ctx = nullptr;
    esp_err_t err = httpd_register_uri_handler(_server, &u);
    if (err != ESP_OK) {
        Serial.printf("[HTTP] could not register %s: %s\n", u.uri, esp_err_to_name(err));
        return false;
    }
    return true;
}
