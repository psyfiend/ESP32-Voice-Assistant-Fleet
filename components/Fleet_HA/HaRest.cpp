#include "HaRest.h"

#include <Arduino.h>
#include <ArduinoJson.h>
#include <string.h>
#include <stdio.h>

#include "esp_http_client.h"

#include "EntityRegistry.h"
#include "HaDefaults.h"
#include "HaValue.h"

#if __has_include("ConnectivityLocalSecrets.h")
#include "ConnectivityLocalSecrets.h"
#endif
#ifndef LOCAL_HA_ACCESS_TOKEN
#define LOCAL_HA_ACCESS_TOKEN ""
#endif

#ifdef DEBUG_HA
  #define DBG_REST(...) Serial.printf("[HaRest] " __VA_ARGS__)
#else
  #define DBG_REST(...) do {} while (0)
#endif

// One entity's reply is 399-843 bytes measured, so 1536 is comfortable
// headroom without being another buffer worth worrying about.
static constexpr int   REST_BODY_MAX   = 1536;
static constexpr int   REST_TIMEOUT_MS = 2000;
static constexpr uint32_t REST_GAP_MS  = 20;

void HaRest::begin(EntityRegistry *reg) {
    _reg  = reg;
    _done = true;
}

void HaRest::restart() {
    _cursor   = 0;
    _done     = false;
    _nextAtMs = 0;
    _fetched  = 0;
    _failed   = 0;
    DBG_REST("initial value pass starting\n");
}

void HaRest::loop(uint32_t nowMs) {
    if (_done || !_reg) return;
    if (nowMs < _nextAtMs) return;

    // Walk forward to the next HA-sourced entity. Non-HA entities are skipped
    // without costing a loop pass, since skipping is free and only the HTTP
    // request is expensive.
    while (_cursor < _reg->count()) {
        const Entity *e = _reg->at(_cursor);
        if (e && e->desc.source == EntitySource::HA && e->desc.externalRef[0]) break;
        _cursor++;
    }

    if (_cursor >= _reg->count()) {
        _done = true;
        Serial.printf("[HaRest] initial values: %u fetched, %u failed\n",
                      (unsigned)_fetched, (unsigned)_failed);
        return;
    }

    const Entity *e = _reg->at(_cursor);
    _cursor++;

    // ONE request, then return to the caller. See the note in HaRest.h: this
    // blocks the loop task, so exactly one blocking call happens per pass.
    if (fetchOne(e->desc.externalRef, e->desc.id)) _fetched++;
    else                                           _failed++;

    _nextAtMs = nowMs + REST_GAP_MS;
}

bool HaRest::fetchOne(const char *entityRef, const char *ourId) {
    const HaDefaultsT &d = haDefaults();

    char url[256];
    snprintf(url, sizeof(url), "http://%s:%u/api/states/%s",
             d.HOST, (unsigned)d.PORT, entityRef);

    char auth[256];
    snprintf(auth, sizeof(auth), "Bearer %s", LOCAL_HA_ACCESS_TOKEN);

    esp_http_client_config_t cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.url             = url;
    cfg.method          = HTTP_METHOD_GET;
    cfg.timeout_ms      = REST_TIMEOUT_MS;
    cfg.disable_auto_redirect = true;

    esp_http_client_handle_t c = esp_http_client_init(&cfg);
    if (!c) return false;

    esp_http_client_set_header(c, "Authorization", auth);
    esp_http_client_set_header(c, "Content-Type", "application/json");

    bool ok = false;
    char body[REST_BODY_MAX];

    esp_err_t err = esp_http_client_open(c, 0);
    if (err == ESP_OK) {
        // fetch_headers must precede read, or content_length is unknown.
        esp_http_client_fetch_headers(c);
        int status = esp_http_client_get_status_code(c);

        int n = esp_http_client_read(c, body, sizeof(body) - 1);
        if (n < 0) n = 0;
        body[n] = '\0';

        if (status == 200 && n > 0) {
            // Filtered, for the same reason HaProvider filters: a state reply
            // carries the full attribute set and we want two fields from it.
            static JsonDocument filter;
            if (filter.isNull() || filter.size() == 0) {
                filter["state"] = true;
                filter["attributes"]["unit_of_measurement"] = true;
            }

            JsonDocument doc;
            if (!deserializeJson(doc, body, (size_t)n,
                                 DeserializationOption::Filter(filter))) {
                const char *state = doc["state"] | "";
                const Entity *e = _reg->find(ourId);
                EntityValue v;
                if (e && haCoerceState(e->desc, state, v)) {
                    _reg->setValue(ourId, v, millis());
                    ok = true;
                    DBG_REST("%s = %s\n", ourId, state);
                } else {
                    // Issue #56. "unavailable" is a STATEMENT, not a gap, and
                    // it gets recorded. A board that boots while a sensor is
                    // down must not show a blank card that reads as "waiting" -
                    // it is not coming.
                    //
                    // Anything else ("unknown", unparseable) means the entity
                    // is there with nothing useful to say, so no claim is made
                    // either way and the last good value stands.
                    if (strcmp(state, "unavailable") == 0) {
                        _reg->setAvailable(ourId, false, millis());
                        DBG_REST("%s = unavailable\n", ourId);
                    } else {
                        DBG_REST("%s -> %s (not a value)\n", ourId, state);
                    }
                    ok = true;
                }
            } else {
                DBG_REST("%s: unparseable reply (%d B)\n", ourId, n);
            }
        } else {
            // 401 here means the token is wrong, and it will be wrong for
            // every entity - worth saying once at full volume rather than
            // eighteen times quietly.
            if (status == 401) {
                Serial.println("[HaRest] 401 Unauthorized - the access token is "
                               "rejected by REST. The websocket uses the same "
                               "token, so check LOCAL_HA_ACCESS_TOKEN.");
            } else {
                DBG_REST("%s: HTTP %d (%d B)\n", ourId, status, n);
            }
        }
    } else {
        DBG_REST("%s: open failed (%d)\n", ourId, (int)err);
    }

    esp_http_client_close(c);
    esp_http_client_cleanup(c);
    return ok;
}
