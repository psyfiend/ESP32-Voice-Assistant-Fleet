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

bool CommandRouter::sendHa(const Entity &e, const EntityValue &v) {
    if (!_ha || !_ha->isReady()) {
        Serial.printf("[Cmd] %s: no HA session; command dropped\n", e.desc.id);
        return false;
    }
    if (!e.desc.externalRef[0]) {
        Serial.printf("[Cmd] %s: HA entity with no externalRef\n", e.desc.id);
        return false;
    }

    // The domain is the part of the entity id before the dot. See the header:
    // this is HA's documented format, not an inference about naming.
    char domain[32];
    const char *dot = strchr(e.desc.externalRef, '.');
    if (!dot || (size_t)(dot - e.desc.externalRef) >= sizeof(domain)) {
        Serial.printf("[Cmd] %s: malformed entity id '%s'\n",
                      e.desc.id, e.desc.externalRef);
        return false;
    }
    const size_t dlen = (size_t)(dot - e.desc.externalRef);
    memcpy(domain, e.desc.externalRef, dlen);
    domain[dlen] = '\0';

    // Only on/off is expressible today. Brightness and colour are LightCard's
    // job (2.7) and will add fields to `service_data` here rather than a second
    // code path - `light.turn_on` carries them on the same call.
    const char *service = nullptr;
    if (v.type == ValueType::BOOL) {
        service = v.b ? "turn_on" : "turn_off";
    } else {
        Serial.printf("[Cmd] %s: no HA service for value type %d yet\n",
                      e.desc.id, (int)v.type);
        return false;
    }

    char frame[320];
    int n = snprintf(frame, sizeof(frame),
                     "{\"id\":%lu,\"type\":\"call_service\","
                     "\"domain\":\"%s\",\"service\":\"%s\","
                     "\"target\":{\"entity_id\":\"%s\"}}",
                     (unsigned long)_ha->nextId(), domain, service,
                     e.desc.externalRef);
    if (n <= 0 || n >= (int)sizeof(frame)) {
        Serial.printf("[Cmd] %s: call_service frame did not fit\n", e.desc.id);
        return false;
    }

    if (!_ha->sendText(frame, n)) {
        Serial.printf("[Cmd] %s: call_service send failed\n", e.desc.id);
        return false;
    }

    Serial.printf("[Cmd] %s -> %s.%s\n", e.desc.id, domain, service);
    return true;
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
