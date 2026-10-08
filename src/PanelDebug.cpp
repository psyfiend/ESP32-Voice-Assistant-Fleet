// PanelDebug - see PanelDebug.h.
#if defined(DISPLAY_ESPLCD) && defined(HAS_MIPI_PANEL)

#include "PanelDebug.h"

#include <Arduino.h>   // Serial
#include <stdio.h>
#include "esp_timer.h"
#include "HttpServer.h"
#include "Fleet_Display.h"
#include "bsp_loader.h"

namespace {

Fleet_Display *s_disp = nullptr;

// NO READS (2.10c round 9). The first version read the panel's status
// registers over DSI; with that read also done at boot, all three P4s came up
// with the backlight on and a black screen, the boards otherwise running. A
// DSI read while the panel streams video stops the picture.
esp_err_t handlePanel(httpd_req_t *req) {
    char q[32], v[8];
    bool resend = false;
    if (httpd_req_get_url_query_str(req, q, sizeof(q)) == ESP_OK &&
        httpd_query_key_value(q, "resend", v, sizeof(v)) == ESP_OK)
        resend = atoi(v) != 0;

    char out[320];
    int n = snprintf(out, sizeof(out), "%s, driver %s\n", bsp_display.PANEL_MODEL, s_disp->driverName());
    if (resend) {
        const int64_t t0 = esp_timer_get_time();
        const int sent = s_disp->resendPanelInit();
        const long ms = (long)((esp_timer_get_time() - t0) / 1000);
        if (sent < 0) n += snprintf(out + n, sizeof(out) - n, "resend FAILED (or no init sequence in the BSP)\n");
        else          n += snprintf(out + n, sizeof(out) - n, "resent %d init commands in %ld ms\n", sent, ms);
        Serial.printf("[Panel] init sequence resent from /panel: %d commands\n", sent);
    } else {
        n += snprintf(out + n, sizeof(out) - n, "/panel?resend=1 sends the panel its init sequence again\n");
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
