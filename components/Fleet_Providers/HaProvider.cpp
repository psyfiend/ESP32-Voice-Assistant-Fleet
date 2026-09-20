#include "HaProvider.h"

#include <ArduinoJson.h>
#include <string.h>
#include <stdlib.h>

#ifdef DEBUG_HA
  #define DBG_HAP(...) Serial.printf("[HaProv] " __VA_ARGS__)
#else
  #define DBG_HAP(...) do {} while (0)
#endif

void HaProvider::begin(EntityRegistry *reg, HaClient *ha) {
    _reg = reg;
    _ha  = ha;
    if (_ha) _ha->setMessageHandler(&HaProvider::onMessage, this);
}

// ---------------------------------------------------------------------------
// Subscription - loop task
// ---------------------------------------------------------------------------

bool HaProvider::sendSubscribe() {
    if (!_reg || !_ha) return false;

    // Built from the REGISTRY, not from a list, so an entity added anywhere
    // gets subscribed without touching this file.
    //
    // The frame is assembled by hand rather than with ArduinoJson. It is a
    // fixed shape with one repeated field, and serialising a document to build
    // it would mean holding both the document and the output at once on the
    // loop task for no benefit.
    static char frame[1536];

    uint32_t id = _ha->nextId();
    int n = snprintf(frame, sizeof(frame),
                     "{\"id\":%lu,\"type\":\"subscribe_trigger\","
                     "\"trigger\":{\"platform\":\"state\",\"entity_id\":[",
                     (unsigned long)id);

    uint8_t named = 0;
    for (uint8_t i = 0; i < _reg->count(); i++) {
        const Entity *e = _reg->at(i);
        if (!e) continue;
        if (e->desc.source != EntitySource::HA) continue;
        if (e->desc.externalRef[0] == '\0') continue;

        int add = snprintf(frame + n, sizeof(frame) - n, "%s\"%s\"",
                           named ? "," : "", e->desc.externalRef);
        if (add < 0 || n + add >= (int)sizeof(frame) - 8) {
            // Refuse rather than send a truncated subscription. A half list
            // would silently drop entities and look like HA not reporting
            // them - the exact class of lying diagnostic this project keeps
            // paying for.
            Serial.printf("[HaProv] subscription frame full at %u entities; "
                          "raise the buffer or split the request.\n",
                          (unsigned)named);
            return false;
        }
        n += add;
        named++;
    }

    if (named == 0) {
        DBG_HAP("no HA-sourced entities registered; nothing to subscribe to\n");
        return false;
    }

    int tail = snprintf(frame + n, sizeof(frame) - n, "]}}");
    if (tail < 0 || n + tail >= (int)sizeof(frame)) return false;
    n += tail;

    if (!_ha->sendText(frame, n)) return false;

    Serial.printf("[HaProv] subscribe_trigger id %lu for %u entities (%d B)\n",
                  (unsigned long)id, (unsigned)named, n);
    return true;
}

void HaProvider::loop(uint32_t nowMs) {
    (void)nowMs;
    if (!_ha) return;

    const uint32_t session = _ha->sessions();

    // Not ready, or no session yet: nothing to do. Note we do NOT clear
    // _subscribedForSession here - it holds the session number it was valid
    // for, so a reconnect is detected by the number CHANGING rather than by
    // having seen a disconnected state in between. A drop and recovery that
    // both happen between two loop() calls would be invisible to a boolean.
    if (!_ha->isReady() || session == 0) return;

    if (_subscribedForSession == session) return;

    if (sendSubscribe()) _subscribedForSession = session;
}

// ---------------------------------------------------------------------------
// Receive - WEBSOCKET TASK
// ---------------------------------------------------------------------------

void HaProvider::onMessage(const char *json, size_t len, void *ctx) {
    HaProvider *self = (HaProvider *)ctx;
    if (self) self->handle(json, len);
}

void HaProvider::handle(const char *json, size_t len) {
    if (!_reg) return;

    // A FILTERED PARSE, because ~95% of every trigger event is discardable.
    //
    // ha-websocket.md section 3 measured one trigger event at 1,365 bytes of
    // which we keep 71: the entity id, the state and the icon. old_state alone
    // is 488 bytes we never read. ArduinoJson's filter skips the unwanted
    // subtrees during parsing rather than after, so the document never holds
    // them and the websocket task's stack never has to.
    static JsonDocument filter;
    if (filter.isNull() || filter.size() == 0) {
        JsonObject ts = filter["event"]["variables"]["trigger"]
                              .to<JsonObject>()["to_state"].to<JsonObject>();
        ts["entity_id"]               = true;
        ts["state"]                   = true;
        ts["attributes"]["icon"]      = true;
        filter["type"]                = true;
    }

    JsonDocument doc;
    DeserializationError err = deserializeJson(
        doc, json, len, DeserializationOption::Filter(filter));
    if (err) {
        _parseFails++;
        DBG_HAP("parse failed (%u B): %s\n", (unsigned)len, err.c_str());
        return;
    }

    // Replies to our own subscribe_trigger arrive as type "result" and carry
    // nothing we filtered for. Only "event" matters here.
    const char *type = doc["type"] | "";
    if (strcmp(type, "event") != 0) return;

    JsonVariantConst to = doc["event"]["variables"]["trigger"]["to_state"];
    if (to.isNull()) return;

    const char *ref   = to["entity_id"] | "";
    const char *state = to["state"]     | "";
    if (!ref[0] || !state[0]) return;

    // Map HA's entity id back to ours. A linear scan over the registry is
    // right at this size - 18 entities, a handful of events a minute - and it
    // keeps the mapping in exactly one place: the descriptor's externalRef.
    const Entity *match = nullptr;
    for (uint8_t i = 0; i < _reg->count(); i++) {
        const Entity *e = _reg->at(i);
        if (!e || e->desc.source != EntitySource::HA) continue;
        if (strcmp(e->desc.externalRef, ref) == 0) { match = e; break; }
    }
    if (!match) {
        _unmatched++;
        DBG_HAP("event for unregistered entity %s\n", ref);
        return;
    }

    EntityValue v;
    if (!coerce(*match, state, v)) {
        // unavailable / unknown / unparseable. Deliberately NOT written: an
        // unavailable sensor is not a reading of zero, and writing one would
        // make a dead thermostat display 0 degrees with full confidence.
        DBG_HAP("%s -> %s (not a value; left alone)\n", match->desc.id, state);
        return;
    }

    _reg->setValue(match->desc.id, v, millis());
    _handled++;
}

bool HaProvider::coerce(const Entity &e, const char *state, EntityValue &out) const {
    if (strcmp(state, "unavailable") == 0) return false;
    if (strcmp(state, "unknown")     == 0) return false;

    switch (e.desc.valueType) {
        case ValueType::BOOL: {
            // HA's on/off covers switches, lights and binary sensors. A
            // binary_sensor with device_class garage_door still reports
            // on/off, not open/closed - the device class only changes how HA's
            // own frontend words it. open/closed are accepted anyway so that a
            // future entity that does report them is not silently dropped.
            const bool on  = (strcmp(state, "on")   == 0) ||
                             (strcmp(state, "open") == 0) ||
                             (strcmp(state, "true") == 0);
            const bool off = (strcmp(state, "off")    == 0) ||
                             (strcmp(state, "closed") == 0) ||
                             (strcmp(state, "false")  == 0);
            if (!on && !off) return false;
            out.type = ValueType::BOOL;
            out.b    = on;
            return true;
        }
        case ValueType::FLOAT: {
            char *end = nullptr;
            float f = strtof(state, &end);
            if (end == state) return false;
            out.type = ValueType::FLOAT;
            out.f    = f;
            return true;
        }
        case ValueType::INT: {
            char *end = nullptr;
            long l = strtol(state, &end, 10);
            if (end == state) return false;
            out.type = ValueType::INT;
            out.i    = (int32_t)l;
            return true;
        }
        case ValueType::TEXT_VAL: {
            out.type = ValueType::TEXT_VAL;
            snprintf(out.text, sizeof(out.text), "%s", state);
            return true;
        }
        default:
            return false;
    }
}
