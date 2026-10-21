// SettingsHttp - the PC's addresses for the saved settings: GET /settings (the
// file, ?stats=1, and the settings without a screen yet) and POST /settings
// (a restore, A17). Split from Settings.cpp on 2026-10-15 when that file
// passed 1,000 lines; the store's state stays private to Settings.cpp and is
// reached through SettingsInternal.h. See Settings.h and HTTP_REFERENCE.md.
//
// Handlers run on the HTTP server's task, whose stack is in PSRAM: nothing
// here touches the flash (a flash access from that task reboots the board,
// LESSONS) - the settings task writes, and the file listing is taken by it.
#include "Settings.h"
#include "SettingsInternal.h"

#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "esp_heap_caps.h"
#include "esp_system.h"    // esp_restart(), after a restore with ?reboot=1
#include "esp_timer.h"
#include "HttpServer.h"

namespace {

constexpr size_t CARD_ID_ARG = 48;   // CARD_ID_MAX (PageSpec.h) + 1

// %XX and '+' in a query value, in place. No escapes are written anywhere.
void urlDecode(char *s) {
    char *o = s;
    for (const char *p = s; *p; p++) {
        if (*p == '+') { *o++ = ' '; continue; }
        if (*p == '%' && isxdigit((unsigned char)p[1]) && isxdigit((unsigned char)p[2])) {
            const char hex[3] = { p[1], p[2], 0 };
            *o++ = (char)strtol(hex, nullptr, 16);
            p += 2;
            continue;
        }
        *o++ = *p;
    }
    *o = '\0';
}

// The panels draw ASCII and the degree sign only (CLAUDE.md): anything else
// would show as boxes, so a name is refused rather than stored broken.
bool printableAscii(const char *s) {
    for (; *s; s++) if ((unsigned char)*s < 0x20 || (unsigned char)*s > 0x7E) return false;
    return true;
}

esp_err_t handleSettings(httpd_req_t *req) {
    char q[160], v[4];
    bool stats = false;
    const bool haveQ = httpd_req_get_url_query_str(req, q, sizeof(q)) == ESP_OK;
    if (haveQ && httpd_query_key_value(q, "stats", v, sizeof(v)) == ESP_OK)
        stats = atoi(v) != 0;

    // A card's custom name, from a PC (K37, until the web UI): name= sets it,
    // an empty name= removes it (the Reset). Kept and written like any other
    // change; the card shows it the next time its page is built.
    // Groups (K44), from a PC until the web UI: group=new&name=Temperature
    // makes a custom group and answers its id; group=<id>&color=RRGGBB sets a
    // group's colour (empty: back to its colour-by-name). Cards already built
    // show it when their page is next built.
    {
        char gid[48], val[48];
        if (haveQ && httpd_query_key_value(q, "card", gid, sizeof(gid)) != ESP_OK &&
            httpd_query_key_value(q, "group", gid, sizeof(gid)) == ESP_OK) {
            char out[160];
            int len;
            if (!strcmp(gid, "new") && httpd_query_key_value(q, "name", val, sizeof(val)) == ESP_OK) {
                urlDecode(val);
                char id[48];
                if (strlen(val) >= 40 || !printableAscii(val) || !Settings::newGroup(val, id, sizeof(id)))
                    len = snprintf(out, sizeof(out), "refused: a group name is 1-39 characters of plain ASCII\n");
                else {
                    Settings::save();
                    len = snprintf(out, sizeof(out), "new group \"%s\": %s\n", val, id);
                }
            } else if (httpd_query_key_value(q, "color", val, sizeof(val)) == ESP_OK) {
                bool hex = strlen(val) == 6;
                for (const char *p = val; *p && hex; p++) hex = isxdigit((unsigned char)*p);
                if (val[0] && !hex) len = snprintf(out, sizeof(out), "refused: color is RRGGBB, or empty\n");
                else {
                    Settings::setGroup(gid, "color", val[0] ? val : nullptr);
                    Settings::save();
                    len = snprintf(out, sizeof(out), "group %s: color %s\n", gid, val[0] ? val : "by name");
                }
            } else if (httpd_query_key_value(q, "name", val, sizeof(val)) == ESP_OK) {
                // Rename a group - every card in it (K46, rename c); for an HA
                // area, a local rename that keeps the link. Empty: its own name.
                urlDecode(val);
                if (strlen(val) >= 40 || !printableAscii(val))
                    len = snprintf(out, sizeof(out), "refused: a group name is up to 39 characters of plain ASCII\n");
                else {
                    Settings::setGroup(gid, "name", val[0] ? val : nullptr);
                    Settings::save();
                    len = snprintf(out, sizeof(out), "group %s: name %s\n", gid, val[0] ? val : "its own again");
                }
            } else if (httpd_query_key_value(q, "delete", val, sizeof(val)) == ESP_OK && atoi(val)) {
                // A card still pointing at it shows its dashboard area again
                // (CardGroups::apply()) until the owner chooses.
                const bool gone = Settings::setGroup(gid, "name", nullptr) | Settings::setGroup(gid, "color", nullptr) |
                                  Settings::setGroup(gid, "kind", nullptr);
                Settings::save();
                len = snprintf(out, sizeof(out), "group %s: %s\n", gid, gone ? "deleted" : "not found");
            } else {
                len = snprintf(out, sizeof(out), "group=new&name=..., group=<id>&color=RRGGBB, group=<id>&delete=1\n");
            }
            httpd_resp_set_type(req, "text/plain");
            return httpd_resp_send(req, out, len);
        }
    }

    // Clear what nobody claims (K35): prune=cards or prune=entities.
    char what[12];
    if (haveQ && httpd_query_key_value(q, "prune", what, sizeof(what)) == ESP_OK) {
        const uint8_t n = Settings::prune(what);
        char out[96];
        const int len = snprintf(out, sizeof(out), "pruned %u unclaimed %s\n", (unsigned)n,
                                 strcmp(what, "entities") ? "card(s)" : "entit(ies)");
        httpd_resp_set_type(req, "text/plain");
        return httpd_resp_send(req, out, len);
    }

    // THE WINDOWS' COUNTDOWN (owner, T6, 2026-10-14), from a PC until the system
    // settings page: window_close=60|30|10|off (60, D6's, is the default and
    // is stored as nothing) and window_bar=on|off, alone or together. In
    // effect from the next window.
    {
        static const char *const CLOSE[] = { "60", "30", "10", "off" };
        char cl[8] = "", bar[8] = "";
        const bool hasClose = haveQ && httpd_query_key_value(q, "window_close", cl, sizeof(cl)) == ESP_OK;
        const bool hasBar   = haveQ && httpd_query_key_value(q, "window_bar", bar, sizeof(bar)) == ESP_OK;
        if (hasClose || hasBar) {
            bool okClose = !hasClose;
            for (const char *c : CLOSE) okClose = okClose || !strcmp(cl, c);
            const bool okBar = !hasBar || !strcmp(bar, "on") || !strcmp(bar, "off");
            char out[160];
            int len;
            if (!okClose || !okBar) {
                len = snprintf(out, sizeof(out), "refused: window_close=60|30|10|off, window_bar=on|off\n");
            } else {
                if (hasClose) Settings::setSystem("window", "close", strcmp(cl, "60") ? cl : nullptr);
                if (hasBar)   Settings::setSystem("window", "bar", strcmp(bar, "on") ? bar : nullptr);
                Settings::save();
                char c[8], b[8];
                if (!Settings::system("window", "close", c, sizeof(c))) snprintf(c, sizeof(c), "60");
                const bool barOff = Settings::system("window", "bar", b, sizeof(b)) && !strcmp(b, "off");
                if (!strcmp(c, "off"))
                    len = snprintf(out, sizeof(out), "windows: never close by themselves (no countdown bar) - from the next window\n");
                else
                    len = snprintf(out, sizeof(out), "windows: close after %s s untouched, countdown bar %s - from the next window\n",
                                   c, barOff ? "off" : "on");
            }
            httpd_resp_set_type(req, "text/plain");
            return httpd_resp_send(req, out, len);
        }
    }

    char id[CARD_ID_ARG], name[200];   // a cluster's slots: up to four card ids
    // THE ARRANGER'S KNOBS (2.11, K105, K109 - Layout Playground v8's), tuned
    // from a PC: arranger=<size|square|wide|twin|gap8|order|off>&v=<number>
    // (empty: the owner's tuned default). A page is arranged again when it is
    // next built.
    if (haveQ && httpd_query_key_value(q, "arranger", id, sizeof(id)) == ESP_OK &&
        httpd_query_key_value(q, "v", name, sizeof(name)) == ESP_OK) {
        static const char *const W[] = { "size", "square", "wide", "twin", "gap8", "order", "off" };
        bool known = false;
        for (const char *w : W) known = known || !strcmp(w, id);
        char out[160];
        int n;
        if (!known) {
            n = snprintf(out, sizeof(out), "refused: arranger=size|square|wide|twin|gap8|order|off\n");
        } else {
            Settings::setSystem("arranger", id, name[0] ? name : nullptr);
            Settings::save();
            n = snprintf(out, sizeof(out), "arranger %s: %s - pages are arranged again when next built\n",
                         id, name[0] ? name : "the default");
        }
        httpd_resp_set_type(req, "text/plain");
        return httpd_resp_send(req, out, n);
    }
    // A PAGE'S CLUSTERS' HEADER STYLE (2.11, K102), until the glass sets it:
    // page=<slug>&cl_style=swatch|bar (empty: the dashboard's).
    if (haveQ && httpd_query_key_value(q, "page", id, sizeof(id)) == ESP_OK &&
        httpd_query_key_value(q, "cl_style", name, sizeof(name)) == ESP_OK) {
        const bool reset = !name[0] || !strcmp(name, "default");
        Settings::setPage(id, "cl_style", reset ? nullptr : name);
        Settings::save();
        char out[160];
        const int n = snprintf(out, sizeof(out), "page %s: cl_style %s - in effect when it is next built\n",
                               id, reset ? "removed" : name);
        httpd_resp_set_type(req, "text/plain");
        return httpd_resp_send(req, out, n);
    }
    // The other card settings too, as SETTINGS' rows would set them - for
    // tests from a PC: label=, tap=, tap_scene= (the scene's HA id). The
    // value is not checked against the choices here; a name the firmware
    // does not know reads as "not set". An empty value removes the key.
    // group= (ha_area / own / none / a group id) and label_text= (the card
    // label's own text, with group=own) are the card label's (K44).
    // A group card's (2.11, until round 3 edits them on the glass): present=
    // card|cluster, slots= (member card ids, comma-separated, highest priority
    // first), cl_style=swatch|bar, cl_icons=off; a member's hidden=yes|no.
    static const char *const KEYS[] = { "label", "tap", "tap_scene", "group", "label_text",
                                        "present", "slots", "cl_style", "cl_icons", "hidden" };
    for (const char *key : KEYS) {
        if (!haveQ || httpd_query_key_value(q, "card", id, sizeof(id)) != ESP_OK ||
            httpd_query_key_value(q, key, name, sizeof(name)) != ESP_OK) continue;
        urlDecode(name);
        // label=inherit is kept since K49 (a named card's default is Custom).
        const bool reset = !name[0] || !strcmp(name, "default") ||
                           (strcmp(key, "label") && !strcmp(name, "inherit"));
        Settings::setCard(id, key, reset ? nullptr : name);
        Settings::save();
        char out[160];
        const int n = snprintf(out, sizeof(out), "%s: %s %s - in effect when its page is next built\n",
                               id, key, reset ? "removed" : name);
        httpd_resp_set_type(req, "text/plain");
        return httpd_resp_send(req, out, n);
    }
    if (haveQ && httpd_query_key_value(q, "card", id, sizeof(id)) == ESP_OK &&
        httpd_query_key_value(q, "name", name, sizeof(name)) == ESP_OK) {
        urlDecode(name);
        char out[200];
        int n;
        if (strlen(name) >= 40 || !printableAscii(name)) {
            n = snprintf(out, sizeof(out), "refused: a name is at most 39 characters of plain ASCII\n");
        } else {
            const bool changed = Settings::setCard(id, "name", name[0] ? name : nullptr);
            Settings::save();
            n = snprintf(out, sizeof(out), "%s: name %s%s%s%s - shown when its page is next built\n", id,
                         name[0] ? "\"" : "removed", name, name[0] ? "\"" : "", changed ? "" : " (no change)");
        }
        httpd_resp_set_type(req, "text/plain");
        return httpd_resp_send(req, out, n);
    }
    if (stats) {
        static char out[2048];   // the server has one task: one request at a time
        const int n = Settings::detail::statusText(out, sizeof(out));
        struct Acc { char *out; int n; int cap; } acc{ out, n, (int)sizeof(out) };
        Settings::forEachUnclaimed([](const char *sec, const char *id, void *c) {
            Acc &a = *(Acc *)c;
            if (a.n < a.cap - 64) a.n += snprintf(a.out + a.n, a.cap - a.n, "unclaimed %s: %s\n", sec, id);
        }, &acc);
        httpd_resp_set_type(req, "text/plain");
        return httpd_resp_send(req, out, acc.n);
    }
    size_t len = 0;
    char *buf = Settings::detail::serialiseCopy(len);
    if (!buf) return httpd_resp_send_500(req);
    httpd_resp_set_type(req, "application/json");
    const esp_err_t r = httpd_resp_send(req, buf, len);
    heap_caps_free(buf);
    return r;
}

esp_err_t replyText(httpd_req_t *req, const char *status, const char *fmt, ...) {
    char out[240];
    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(out, sizeof(out), fmt, ap);
    va_end(ap);
    if (n < 0) n = 0;
    if (n >= (int)sizeof(out)) n = (int)sizeof(out) - 1;
    httpd_resp_set_status(req, status);
    httpd_resp_set_type(req, "text/plain");
    return httpd_resp_send(req, out, n);
}

void restartNow(void *) { esp_restart(); }

// PUT A SAVED FILE BACK (A17): POST /settings with a settings file as the body
// (scripts/release/restore_settings.ps1). The body is received here and handed
// to Settings::detail::restore(), which checks it - JSON, an object, a schema
// this firmware knows - replaces the document and waits for the settings task
// to write it: never this handler, whose stack is in PSRAM (a flash write from
// it reboots the board, LESSONS). ?reboot=1 restarts the board after a good
// write, so every page and setting is read from the file again.
esp_err_t handleRestore(httpd_req_t *req) {
    const size_t len = req->content_len;
    if (len == 0 || len > Settings::detail::FILE_MAX)
        return replyText(req, "400 Bad Request", "refused: the body is %u bytes; a settings file is 1 to %u\n",
                         (unsigned)len, (unsigned)Settings::detail::FILE_MAX);
    {
        char why[160];
        if (!Settings::detail::writable(why, sizeof(why)))
            return replyText(req, "409 Conflict", "refused: this boot does not write the settings file (%s) - "
                             "nothing changed\n", why);
    }
    char *buf = (char *)heap_caps_malloc(len + 1, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!buf) return replyText(req, "500 Internal Server Error", "refused: no memory for %u bytes\n", (unsigned)len);
    size_t got = 0;
    uint8_t timeouts = 0;
    while (got < len) {
        const int r = httpd_req_recv(req, buf + got, len - got);
        if (r == HTTPD_SOCK_ERR_TIMEOUT && ++timeouts < 3) continue;
        if (r <= 0) break;
        got += (size_t)r;
    }
    if (got != len) {
        heap_caps_free(buf);
        return replyText(req, "400 Bad Request", "refused: the body stopped at %u of %u bytes - nothing changed\n",
                         (unsigned)got, (unsigned)len);
    }
    char msg[200];
    const int status = Settings::detail::restore(buf, len, msg, sizeof(msg));
    heap_caps_free(buf);
    if (status != 200) {
        const char *line = status == 400 ? "400 Bad Request" : status == 409 ? "409 Conflict"
                                                                             : "500 Internal Server Error";
        return replyText(req, line, "%s\n", msg);
    }
    char q[48], v[4];
    const bool reboot = httpd_req_get_url_query_str(req, q, sizeof(q)) == ESP_OK &&
                        httpd_query_key_value(q, "reboot", v, sizeof(v)) == ESP_OK && atoi(v) != 0;
    const esp_err_t r = replyText(req, "200 OK", "%s - %s\n", msg,
                                  reboot ? "restarting" : "pages show it when next built, everything at the next boot");
    if (reboot) {
        esp_timer_create_args_t a = {};
        a.callback = restartNow;
        a.name = "settings_restart";
        esp_timer_handle_t t = nullptr;
        if (esp_timer_create(&a, &t) == ESP_OK) esp_timer_start_once(t, 500 * 1000);
    }
    return r;
}

}  // namespace

void Settings::detail::registerRoutes(HttpServer &http) {
    http.addRoute("/settings", HTTP_GET, handleSettings);
    http.addRoute("/settings", HTTP_POST, handleRestore);
}

