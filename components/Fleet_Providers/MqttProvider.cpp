#include "MqttProvider.h"
#include <ArduinoJson.h>

MqttProvider *MqttProvider::s_self = nullptr;

void MqttProvider::begin(EntityRegistry *reg, MqttManager *mqtt) {
    _reg   = reg;
    _mqtt  = mqtt;
    s_self = this;
    if (_mqtt) _mqtt->setMessageHandler(&MqttProvider::onMessage);
}

void MqttProvider::subscribeAll() {
    if (!_reg || !_mqtt) return;

    uint8_t n = 0;
    for (uint8_t i = 0; i < _reg->count(); i++) {
        const Entity *e = _reg->at(i);
        if (!e) continue;
        if (e->desc.source != EntitySource::MQTT) continue;
        if (!e->desc.externalRef[0]) continue;

        // A state topic: retained payloads are WANTED here. The broker's
        // retained value is the sensor's last known reading, which is exactly
        // what should populate a card on boot rather than an empty tile.
        // (Command topics take the opposite rule - see MqttManager.)
        _mqtt->subscribeState(e->desc.externalRef);
        n++;
    }

    _subscribed = true;
    if (n) Serial.printf("[MqttProv] Subscribed for %u external entities.\n", (unsigned)n);
}

void MqttProvider::loop(uint32_t nowMs) {
    (void)nowMs;
    if (!_mqtt) return;

    const bool connected = _mqtt->isConnected();
    // A reconnect drops every subscription broker-side, so they are re-issued.
    if (connected && !_wasConnected) _subscribed = false;
    _wasConnected = connected;

    if (connected && !_subscribed) subscribeAll();
}

void MqttProvider::onMessage(const char *topic, const uint8_t *payload,
                             unsigned int length, bool retained) {
    (void)retained;   // retained is expected on a state topic; nothing to guard
    if (s_self) s_self->handle(topic, payload, length);
}

void MqttProvider::handle(const char *topic, const uint8_t *payload, unsigned int length) {
    if (!_reg || !topic) return;

    // Copy to a NUL-terminated buffer. Deliberately NOT payload[length] = 0 -
    // that writes one byte past the buffer the MQTT library owns, which is a
    // real out-of-bounds write even though it usually appears to work.
    char buf[257];
    const unsigned int n = (length < sizeof(buf) - 1) ? length : sizeof(buf) - 1;
    memcpy(buf, payload, n);
    buf[n] = '\0';

    const uint32_t now = millis();
    bool matchedAny = false;

    // Several entities may share one topic, each reading its own key, so every
    // entity is checked rather than stopping at the first match.
    for (uint8_t i = 0; i < _reg->count(); i++) {
        const Entity *e = _reg->at(i);
        if (!e) continue;
        if (e->desc.source != EntitySource::MQTT) continue;
        if (strcmp(e->desc.externalRef, topic) != 0) continue;

        matchedAny = true;

        // Snapshot what we need before releasing our read of the entity.
        const ValueType vt = e->desc.valueType;
        char id[ENTITY_ID_MAX];
        char key[ENTITY_SHORT_MAX];
        snprintf(id,  sizeof(id),  "%s", e->desc.id);
        snprintf(key, sizeof(key), "%s", e->desc.valueKey);

        const char *raw = buf;
        JsonDocument doc;             // only populated when a key is named

        if (key[0]) {
            if (deserializeJson(doc, buf, n) != DeserializationError::Ok) {
                Serial.printf("[MqttProv] %s: payload on \"%s\" is not JSON\n", id, topic);
                continue;
            }
            JsonVariant v = doc[key];
            if (v.isNull()) {
                Serial.printf("[MqttProv] %s: key \"%s\" not in payload\n", id, key);
                continue;
            }
            // Numbers are converted below from the variant directly; text
            // routes through a string view of it.
            switch (vt) {
                case ValueType::FLOAT: _reg->setValue(id, EntityValue::makeFloat(v.as<float>()), now); continue;
                case ValueType::INT:   _reg->setValue(id, EntityValue::makeInt(v.as<int32_t>()), now); continue;
                case ValueType::BOOL:  _reg->setValue(id, EntityValue::makeBool(v.as<bool>()), now);  continue;
                default: {
                    const char *s = v.as<const char *>();
                    _reg->setValue(id, EntityValue::makeText(s ? s : ""), now);
                    continue;
                }
            }
        }

        // No key: the whole payload is the value.
        switch (vt) {
            case ValueType::FLOAT: _reg->setValue(id, EntityValue::makeFloat(atof(raw)), now); break;
            case ValueType::INT:   _reg->setValue(id, EntityValue::makeInt((int32_t)atol(raw)), now); break;
            case ValueType::BOOL: {
                // Accept the spellings devices actually use, rather than
                // insisting on one and silently reading everything else as off.
                const bool on = (strcasecmp(raw, "ON") == 0) || (strcasecmp(raw, "true") == 0) ||
                                (strcmp(raw, "1") == 0) || (strcasecmp(raw, "open") == 0);
                _reg->setValue(id, EntityValue::makeBool(on), now);
                break;
            }
            default:
                _reg->setValue(id, EntityValue::makeText(raw), now);
                break;
        }
    }

    if (matchedAny) {
        _handled++;
    } else {
        // Subscribed but nothing wants it. Almost always a topic typo in an
        // entity declaration, so it is worth saying rather than dropping.
        _unmatched++;
        Serial.printf("[MqttProv] No entity for topic \"%s\"\n", topic);
    }
}
