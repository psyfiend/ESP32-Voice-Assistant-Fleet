#include "CommandRouter.h"

#include <string.h>
#include <stdio.h>

#include "MqttManager.h"
#include "HaClient.h"

void CommandRouter::begin(EntityRegistry *reg, MqttManager *mqtt, HaClient *ha) {
    _reg  = reg;
    _mqtt = mqtt;
    _ha   = ha;
    if (_reg) _reg->setCommandSink(&CommandRouter::onCommand, this);
    if (_reg) _reg->setLightSink(&CommandRouter::onLight, this);
}

void CommandRouter::onLight(const Entity &e, const LightCommand &c, void *ctx) {
    CommandRouter *self = (CommandRouter *)ctx;
    if (!self) return;
    bool ok = false;
    switch (e.desc.source) {
        case EntitySource::VIRTUAL: ok = true; break;
        case EntitySource::HA:      ok = self->sendHaLight(e, c); break;
        default:
            // MQTT lights (Zigbee2MQTT's JSON schema) would go here; none
            // exists yet. Loud, and the registry's window marks it FAILED.
            Serial.printf("[Cmd] %s: no light levels over %s yet; refused\n",
                          e.desc.id, entitySourceName(e.desc.source));
            break;
    }
    if (ok) self->_sent++;
    else    self->_refused++;
}

void CommandRouter::onHaResult(uint32_t id, bool ok, const char *code, const char *msg, void *ctx) {
    CommandRouter *self = (CommandRouter *)ctx;
    if (!self) return;
    char entity[ENTITY_ID_MAX] = {0};
    bool superseded = false;
    {
        std::lock_guard<std::mutex> lk(self->_callMx);
        for (HaCall &c : self->_calls) {
            if (c.id != id) continue;
            memcpy(entity, c.entity, sizeof(entity));
            c.id = 0;
            break;
        }
        // A LATER CALL FOR THE SAME ENTITY OUTRANKS THIS ONE (G8). While a
        // slider is dragged a call goes every 300 ms; a refusal of an earlier
        // one says nothing about the latest, which the registry is waiting on.
        if (entity[0])
            for (const HaCall &c : self->_calls)
                if (c.id > id && strcmp(c.entity, entity) == 0) { superseded = true; break; }
    }
    if (!entity[0] || ok) return;   // not ours, or accepted: the report decides

    self->_haRefused++;
    Serial.printf("[Cmd] %s: HA REFUSED call %lu%s: %s - %s\n", entity, (unsigned long)id,
                  superseded ? " (a later call stands)" : "", code, msg);
    if (superseded || !self->_reg) return;
    char why[80];
    snprintf(why, sizeof(why), "HA refused: %s - %s", code, msg);
    self->_reg->failCommand(entity, why);
}

void CommandRouter::onCommand(const Entity &e, const EntityValue &v, void *ctx) {
    CommandRouter *self = (CommandRouter *)ctx;
    if (self) self->route(e, v);
}

void CommandRouter::route(const Entity &e, const EntityValue &v) {
    bool ok = false;

    switch (e.desc.source) {
        case EntitySource::HA:
            ok = sendHa(e, v);
            break;

        case EntitySource::MQTT:
            ok = sendMqtt(e, v);
            break;

        case EntitySource::LOCAL:
        case EntitySource::SYSTEM:
            // Nothing to send. We own the hardware, so the optimistic value
            // already IS the truth, and HaPublisher will publish it outward on
            // its next pass like any other reading of ours.
            ok = true;
            break;

        case EntitySource::VIRTUAL:
            // VirtualProvider answers these itself - that is what they exist
            // for, and it is how the reconcile path got exercised before any
            // real writable entity existed.
            ok = true;
            break;
    }

    if (ok) _sent++;
    else    _refused++;
}

// ---------------------------------------------------------------------------
// Home Assistant
// ---------------------------------------------------------------------------

// The domain is the part of the entity id before the dot. See the header:
// this is HA's documented format, not an inference about naming.
static bool haDomain(const Entity &e, char *out, size_t cap) {
    const char *dot = strchr(e.desc.externalRef, '.');
    if (!dot || (size_t)(dot - e.desc.externalRef) >= cap) {
        Serial.printf("[Cmd] %s: malformed entity id '%s'\n",
                      e.desc.id, e.desc.externalRef);
        return false;
    }
    const size_t dlen = (size_t)(dot - e.desc.externalRef);
    memcpy(out, e.desc.externalRef, dlen);
    out[dlen] = '\0';
    return true;
}

bool CommandRouter::callService(const Entity &e, const char *domain, const char *service,
                                const char *data) {
    if (!_ha || !_ha->isReady()) {
        Serial.printf("[Cmd] %s: no HA session; command dropped\n", e.desc.id);
        return false;
    }
    if (!e.desc.externalRef[0]) {
        Serial.printf("[Cmd] %s: HA entity with no externalRef\n", e.desc.id);
        return false;
    }

    const uint32_t id = _ha->nextId();
    char frame[384];
    int n = snprintf(frame, sizeof(frame),
                     "{\"id\":%lu,\"type\":\"call_service\","
                     "\"domain\":\"%s\",\"service\":\"%s\","
                     "\"target\":{\"entity_id\":\"%s\"}%s%s%s}",
                     (unsigned long)id, domain, service, e.desc.externalRef,
                     data[0] ? ",\"service_data\":{" : "", data, data[0] ? "}" : "");
    if (n <= 0 || n >= (int)sizeof(frame)) {
        Serial.printf("[Cmd] %s: call_service frame did not fit\n", e.desc.id);
        return false;
    }

    // Remembered BEFORE sending: the reply can arrive on the websocket task
    // before sendText() has even returned here.
    {
        std::lock_guard<std::mutex> lk(_callMx);
        HaCall &c = _calls[_callNext];
        _callNext = (uint8_t)((_callNext + 1) % HA_CALLS);
        c.id = id;
        snprintf(c.entity, sizeof(c.entity), "%s", e.desc.id);
    }

    if (!_ha->sendText(frame, n)) {
        Serial.printf("[Cmd] %s: call_service send failed\n", e.desc.id);
        return false;
    }

    Serial.printf("[Cmd] %s -> %s.%s%s%s\n", e.desc.id, domain, service,
                  data[0] ? " " : "", data);
    return true;
}

bool CommandRouter::sendHa(const Entity &e, const EntityValue &v) {
    char domain[32];
    if (!haDomain(e, domain, sizeof(domain))) return false;

    // On/off. A light's levels come through sendHaLight() - the same
    // light.turn_on, with service_data.
    if (v.type != ValueType::BOOL) {
        Serial.printf("[Cmd] %s: no HA service for value type %d yet\n",
                      e.desc.id, (int)v.type);
        return false;
    }
    return callService(e, domain, v.b ? "turn_on" : "turn_off", "");
}

// light.turn_on with brightness (0-255, the registry's own scale, so nothing is
// rounded twice), color_temp_kelvin or hs_color - DECISIONS C5. A level means
// ON, as in HA; temperature and colour are exclusive and colour wins, as in
// LightCommand. Measured on the owner's Hue bulbs: the first report matches
// within the registry's tolerances (ha-websocket.md section 9).
bool CommandRouter::sendHaLight(const Entity &e, const LightCommand &c) {
    char domain[32];
    if (!haDomain(e, domain, sizeof(domain))) return false;
    if (strcmp(domain, "light") != 0) {
        // A switch.* drawn as a light never reports levels, so the popup never
        // offers them; this is the guard if one ever arrives anyway.
        Serial.printf("[Cmd] %s: levels for a %s entity; refused\n", e.desc.id, domain);
        return false;
    }

    if (!c.wantsOn()) return callService(e, domain, "turn_off", "");

    // Three fields at most, each under 30 characters: 96 cannot overflow.
    char bri[24] = "", col[40] = "";
    if (c.brightness > 0)
        snprintf(bri, sizeof(bri), "\"brightness\":%d", c.brightness > 255 ? 255 : c.brightness);
    if (c.hue >= 0)
        snprintf(col, sizeof(col), "\"hs_color\":[%d,%d]", c.hue, c.sat >= 0 ? c.sat : 100);
    else if (c.colorTempK > 0)
        snprintf(col, sizeof(col), "\"color_temp_kelvin\":%d", c.colorTempK);

    char data[96];
    snprintf(data, sizeof(data), "%s%s%s", bri, (bri[0] && col[0]) ? "," : "", col);
    return callService(e, domain, "turn_on", data);
}

// ---------------------------------------------------------------------------
// MQTT
// ---------------------------------------------------------------------------

bool CommandRouter::sendMqtt(const Entity &e, const EntityValue &v) {
    if (!_mqtt || !_mqtt->isConnected()) {
        Serial.printf("[Cmd] %s: broker not connected; command dropped\n", e.desc.id);
        return false;
    }
    if (!e.desc.commandRef[0]) {
        // Deliberately loud. An MQTT entity marked writable with nowhere to
        // write to is a build-sheet mistake, and silently swallowing the tap
        // would present as "the button does nothing".
        Serial.printf("[Cmd] %s: writable MQTT entity has no commandRef\n", e.desc.id);
        return false;
    }

    char payload[ENTITY_TEXT_MAX + 16];
    switch (v.type) {
        case ValueType::BOOL:     snprintf(payload, sizeof(payload), "%s", v.b ? "ON" : "OFF"); break;
        case ValueType::INT:      snprintf(payload, sizeof(payload), "%ld", (long)v.i); break;
        case ValueType::FLOAT:    snprintf(payload, sizeof(payload), "%.2f", v.f); break;
        case ValueType::TEXT_VAL: snprintf(payload, sizeof(payload), "%s", v.text); break;
        default:
            Serial.printf("[Cmd] %s: no payload for value type %d\n",
                          e.desc.id, (int)v.type);
            return false;
    }

    // RETAIN IS FALSE AND MUST STAY FALSE.
    //
    // A retained command is an instruction the broker replays to every
    // subscriber on every reconnect, forever. The device would be re-ordered to
    // its last commanded state each time it came back, overriding whatever the
    // user or an automation did in between. #44's own note calls this out, and
    // LESSONS.md records the matching inbound heuristic that exists because
    // PubSubClient will not expose the retain flag on receive.
    if (!_mqtt->publish(e.desc.commandRef, payload, /*retain=*/false)) {
        Serial.printf("[Cmd] %s: publish to %s failed\n", e.desc.id, e.desc.commandRef);
        return false;
    }

    Serial.printf("[Cmd] %s -> %s = %s\n", e.desc.id, e.desc.commandRef, payload);
    return true;
}
