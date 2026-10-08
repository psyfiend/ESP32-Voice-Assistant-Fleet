// PanelDebug - see PanelDebug.h.
#if defined(DISPLAY_ESPLCD) && defined(HAS_MIPI_PANEL)

#include "PanelDebug.h"

#include <Arduino.h>   // Serial
#include <stdio.h>
#include <string.h>
#include "esp_timer.h"
#include "HttpServer.h"
#include "Fleet_Display.h"
#include "bsp_loader.h"

namespace {

Fleet_Display *s_disp = nullptr;

// One line per reading: the four registers. What a healthy panel reads differs
// by chip (the ST7703 sets 0Ah's booster bit, the HX8394 and JD9165 do not), so
// each reading is compared with this panel's own at boot.
int line(char *buf, size_t cap, const char *when, const uint8_t r[4]) {
    return snprintf(buf, cap, "%-7s power %02X, MADCTL %02X, pixel format %02X, image mode %02X\n",
                    when, r[0], r[1], r[2], r[3]);
}

int status(char *buf, size_t cap, const char *when) {
    uint8_t r[4], b[4];
    if (!s_disp->readPanelStatus(r)) return snprintf(buf, cap, "%-7s the panel did not answer\n", when);
    int n = line(buf, cap, when, r);
    if (s_disp->bootPanelStatus(b))
        n += snprintf(buf + n, cap - n, "        %s\n",
                      memcmp(r, b, 4) ? "DIFFERENT FROM BOOT" : "the same as at boot");
    return n;
}

esp_err_t handlePanel(httpd_req_t *req) {
    char q[32], v[8];
    bool resend = false;
    if (httpd_req_get_url_query_str(req, q, sizeof(q)) == ESP_OK &&
        httpd_query_key_value(q, "resend", v, sizeof(v)) == ESP_OK)
        resend = atoi(v) != 0;

    char out[640];
    int n = snprintf(out, sizeof(out), "%s, driver %s\n", bsp_display.PANEL_MODEL, s_disp->driverName());
    uint8_t b[4];
    if (s_disp->bootPanelStatus(b)) n += line(out + n, sizeof(out) - n, "boot:", b);
    else                            n += snprintf(out + n, sizeof(out) - n, "boot:   not read\n");
    n += status(out + n, sizeof(out) - n, "now:");
    if (resend) {
        const int64_t t0 = esp_timer_get_time();
        const int sent = s_disp->resendPanelInit();
        const long ms = (long)((esp_timer_get_time() - t0) / 1000);
        if (sent < 0) n += snprintf(out + n, sizeof(out) - n, "resend FAILED (or no init sequence in the BSP)\n");
        else          n += snprintf(out + n, sizeof(out) - n, "resent %d init commands in %ld ms\n", sent, ms);
        n += status(out + n, sizeof(out) - n, "after:");
        Serial.printf("[Panel] init sequence resent from /panel: %d commands\n", sent);
    }
    httpd_resp_set_type(req, "text/plain");
    return httpd_resp_send(req, out, n);
}

} // namespace

namespace PanelDebug {
void begin(HttpServer &http, Fleet_Display &display) {
    s_disp = &display;
    http.addRoute("/panel", HTTP_GET, handlePanel);
}
}

#endif
