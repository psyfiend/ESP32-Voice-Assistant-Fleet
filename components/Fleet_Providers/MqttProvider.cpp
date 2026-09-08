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

    // First pass: does anything want this topic, and does anything need the
    // payload parsed as JSON?
    bool matchedAny = false;
    bool needsJson  = false;
    for (uint8_t i = 0; i < _reg->count(); i++) {
        const Entity *e = _reg->at(i);
        if (!e) continue;
        if (e->desc.source != EntitySource::MQTT) continue;
        if (strcmp(e->desc.externalRef, topic) != 0) continue;
        matchedAny = true;
        if (e->desc.valueKey[0]) needsJson = true;
    }

    if (!matchedAny) {
        // Subscribed but nothing wants it. Almost always a topic typo in an
        // entity declaration, so it is worth saying rather than dropping.
        _unmatched++;
        Serial.printf("[MqttProv] No entity for topic \"%s\"\n", topic);
        return;
    }

    // Parse ONCE for the whole message, straight from the library's buffer.
    //
    // Deliberately no intermediate copy. An earlier version copied into a
    // fixed 257-byte buffer first, which silently TRUNCATED any larger payload
    // and then handed the fragment to the parser - every Zigbee2MQTT message
    // is bigger than that, so every one failed as "not JSON". The lesson
    // generalises: a fixed buffer sized by guess in the middle of a data path
    // fails as corruption, not as an error.
    //
    // ArduinoJson reads (pointer, length) directly and never needs the payload
    // NUL-terminated, which also avoids the payload[length] = 0 idiom - that
    // writes one byte past a buffer the MQTT library owns.
    JsonDocument doc;
    if (needsJson) {
        DeserializationError err = deserializeJson(doc, (const char *)payload, length);
        if (err) {
            // Reported once per message rather than once per entity.
            Serial.printf("[MqttProv] \"%s\": %u-byte payload did not parse (%s)\n",
                          topic, length, err.c_str());
            return;
        }
    }

    const uint32_t now = millis();

    for (uint8_t i = 0; i < _reg->count(); i++) {
        const Entity *e = _reg->at(i);
        if (!e) continue;
        if (e->desc.source != EntitySource::MQTT) continue;
        if (strcmp(e->desc.externalRef, topic) != 0) continue;

        // Snapshot what is needed before writing back into the registry.
        const ValueType vt = e->desc.valueType;
        char id[ENTITY_ID_MAX];
        char key[ENTITY_SHORT_MAX];
        snprintf(id,  sizeof(id),  "%s", e->desc.id);
        snprintf(key, sizeof(key), "%s", e->desc.valueKey);

        if (key[0]) {
            JsonVariant v = doc[key];
            if (v.isNull()) {
                Serial.printf("[MqttProv] %s: key \"%s\" not in payload\n", id, key);
                continue;
            }
            switch (vt) {
                case ValueType::FLOAT: _reg->setValue(id, EntityValue::makeFloat(v.as<float>()), now); break;
                case ValueType::INT:   _reg->setValue(id, EntityValue::makeInt(v.as<int32_t>()), now); break;
                case ValueType::BOOL:  _reg->setValue(id, EntityValue::makeBool(v.as<bool>()), now);   break;
                default: {
                    const char *s = v.as<const char *>();
                    _reg->setValue(id, EntityValue::makeText(s ? s : ""), now);
                    break;
                }
            }
            continue;
        }

        // No key: the whole payload is the value. These are short by nature
        // ("ON", "21.4"), so a small stack buffer is safe here - and unlike the
        // JSON path, a truncation would be visible rather than silent.
        char raw[64];
        const unsigned int n = (length < sizeof(raw) - 1) ? length : sizeof(raw) - 1;
        memcpy(raw, payload, n);
        raw[n] = '\0';

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

    _handled++;
}
