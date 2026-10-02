#include "TimeService.h"

#include <Arduino.h>
#include <esp_netif_sntp.h>
#include <esp_sntp.h>
#include <esp_timer.h>
#include <sys/time.h>
#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include "sdkconfig.h"

namespace {

bool              s_started    = false;
esp_err_t         s_startErr   = ESP_OK;

// Written by the sync callback, which runs on lwIP's task; read on loop().
// Two aligned words, each written once per sync - no lock needed for a
// report that only ever wants "roughly when, and how many".
volatile uint32_t s_syncs      = 0;
volatile int64_t  s_lastSyncUs = 0;

uint32_t          s_reported   = 0;   // loop() only

// ON LWIP'S TASK. Records, and nothing else: no Serial, no LVGL.
void onSync(struct timeval *) {
    s_lastSyncUs = esp_timer_get_time();
    s_syncs = s_syncs + 1;
}

// The first configured server lwIP currently marks reachable, so the report
// can say which one set the clock. "" when none is (yet).
const char *reachableServer() {
    for (uint8_t i = 0; i < CONFIG_LWIP_SNTP_MAX_SERVERS; i++) {
        if (esp_sntp_getreachability(i) == 0) continue;
        const char *n = esp_sntp_getservername(i);
        if (n && n[0]) return n;
    }
    return "";
}

void start() {
    s_started = true;

    // As many of the three as this framework build allows. An IDF build with
    // fewer server slots refuses a longer list outright, so ask for what fits.
#if CONFIG_LWIP_SNTP_MAX_SERVERS >= 3
    esp_sntp_config_t cfg = ESP_NETIF_SNTP_DEFAULT_CONFIG_MULTIPLE(3,
        ESP_SNTP_SERVER_LIST(FLEET_NTP_1, FLEET_NTP_2, FLEET_NTP_3));
#elif CONFIG_LWIP_SNTP_MAX_SERVERS == 2
    esp_sntp_config_t cfg = ESP_NETIF_SNTP_DEFAULT_CONFIG_MULTIPLE(2,
        ESP_SNTP_SERVER_LIST(FLEET_NTP_1, FLEET_NTP_2));
#else
    esp_sntp_config_t cfg = ESP_NETIF_SNTP_DEFAULT_CONFIG(FLEET_NTP_1);
#endif
    cfg.sync_cb           = onSync;
    cfg.smooth_sync       = false;                // a step is fine for a dashboard
    cfg.ip_event_to_renew = IP_EVENT_STA_GOT_IP;  // re-sync after every reconnect
    cfg.start             = true;

    s_startErr = esp_netif_sntp_init(&cfg);
    if (s_startErr == ESP_OK) {
        Serial.printf("[Time] SNTP started: %s, %s, %s (%d slot%s)\n",
                      FLEET_NTP_1, FLEET_NTP_2, FLEET_NTP_3,
                      CONFIG_LWIP_SNTP_MAX_SERVERS,
                      CONFIG_LWIP_SNTP_MAX_SERVERS == 1 ? "" : "s");
    } else {
        Serial.printf("[Time] SNTP failed to start: %s\n", esp_err_to_name(s_startErr));
    }
}

}  // namespace

namespace TimeService {

void begin() {
    // Before anything formats a time, so every later localtime_r() is local.
    setenv("TZ", FLEET_TZ, 1);
    tzset();
    Serial.printf("[Time] Zone %s\n", FLEET_TZ);
}

void loop(bool online) {
    if (!s_started && online) start();

    // Each new sync, said once, from the loop task.
    const uint32_t n = s_syncs;
    if (n != s_reported) {
        s_reported = n;
        char d[112];
        describe(d, sizeof(d));
        Serial.printf("[Time] %s\n", d);
    }
}

bool isSet() {
    return time(nullptr) > 1700000000;   // any real date is past late 2023
}

void describe(char *buf, size_t len) {
    if (!buf || !len) return;

    if (isSet()) {
        const time_t now = time(nullptr);
        struct tm tmv;
        localtime_r(&now, &tmv);
        char t[40];
        strftime(t, sizeof(t), "%Y-%m-%d %H:%M:%S %Z", &tmv);

        if (s_syncs) {
            const unsigned long mins =
                (unsigned long)((esp_timer_get_time() - s_lastSyncUs) / 60000000LL);
            const char *srv = reachableServer();
            snprintf(buf, len, "%s  (SNTP%s%s, synced %lu min ago)",
                     t, srv[0] ? " " : "", srv, mins);
        } else {
            snprintf(buf, len, "%s  (set, but not by SNTP)", t);
        }
        return;
    }

    if (!s_started)              snprintf(buf, len, "not set  (SNTP waiting for the network)");
    else if (s_startErr != ESP_OK) snprintf(buf, len, "not set  (SNTP failed: %s)", esp_err_to_name(s_startErr));
    else                         snprintf(buf, len, "not set  (SNTP waiting for a server)");
}

}  // namespace TimeService
