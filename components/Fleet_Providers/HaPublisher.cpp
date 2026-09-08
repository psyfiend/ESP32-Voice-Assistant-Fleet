#include "HaPublisher.h"
#include "DeviceIdentity.h"
#include "bsp_loader.h"

#ifndef FW_VERSION
    #define FW_VERSION "0.0.0"
#endif

// Built by hand rather than with ArduinoJson. The payload is a fixed shape we
// control end to end, the values are already sanitised, and a String we append
// to costs far less than a JsonDocument big enough to hold the whole device.
// If this ever grows nested or conditional structure, switch to ArduinoJson -
// hand-built JSON stops being worth it the moment it needs branching.

static void appendKV(String &j, const char *key, const char *val, bool quoted = true) {
    if (!val || !val[0]) return;      // omit empty keys entirely; HA prefers absent to ""
    j += "\"";
    j += key;
    j += "\":";
    if (quoted) j += "\"";
    j += val;
    if (quoted) j += "\"";
    j += ",";
}

// Trim the trailing comma an appendKV run leaves behind.
static void closeObj(String &j) {
    if (j.endsWith(",")) j.remove(j.length() - 1);
    j += "}";
}

void HaPublisher::begin(EntityRegistry *reg, MqttManager *mqtt) {
    _reg  = reg;
    _mqtt = mqtt;
}

void HaPublisher::stateTopicFor(const Entity &e, char *out, size_t outLen) const {
    // ROADMAP Q5: state = <base>/<object_id>/state
    snprintf(out, outLen, "%s/%s/state", _mqtt->getBaseTopic(), e.desc.id);
}

// Platforms Home Assistant treats as COMMANDABLE. Each requires a
// command_topic in its discovery config, and HA rejects the entity outright
// without one - it simply never appears, with no error logged anywhere.
//
// This cost us the IP Address entity on the first hardware test: it was
// declared EntityKind::TEXT because its value is a string, but HA's `text`
// platform is an input field, not a readout. A read-only string is a SENSOR
// whose valueType is TEXT_VAL.
static bool kindNeedsCommandTopic(EntityKind k) {
    switch (k) {
        case EntityKind::SWITCH:
        case EntityKind::LIGHT:
        case EntityKind::BUTTON:
        case EntityKind::NUMBER:
        case EntityKind::TEXT:
        case EntityKind::CLIMATE:
            return true;
        default:
            return false;
    }
}

bool HaPublisher::appendComponent(String &json, const Entity &e, bool first) const {
    char uniq[96];
    snprintf(uniq, sizeof(uniq), "%s_%s", DeviceIdentity::deviceId(), e.desc.id);

    char stateTopic[ENTITY_TOPIC_MAX];
    stateTopicFor(e, stateTopic, sizeof(stateTopic));

    if (!first) json += ",";
    json += "\"";
    json += e.desc.id;          // key within cmps
    json += "\":{";

    appendKV(json, "p",             entityKindHaPlatform(e.desc.kind));
    appendKV(json, "name",          e.desc.name);
    appendKV(json, "uniq_id",       uniq);
    appendKV(json, "object_id",     uniq);
    appendKV(json, "stat_t",        stateTopic);
    appendKV(json, "avty_t",        _mqtt->getAvailabilityTopic());
    appendKV(json, "pl_avail",      "online");
    appendKV(json, "pl_not_avail",  "offline");
    appendKV(json, "dev_cla",       e.desc.deviceClass);
    appendKV(json, "stat_cla",      e.desc.stateClass);
    appendKV(json, "unit_of_meas",  e.desc.unit);
    appendKV(json, "ic",            e.desc.icon);
    if (e.desc.diagnostic) appendKV(json, "ent_cat", "diagnostic");

    closeObj(json);
    return true;
}

void HaPublisher::publishDiscovery() {
    if (!_reg || !_mqtt || !_mqtt->isConnected()) return;

    String j;
    j.reserve(2048);

    j += "{\"device\":{";
    {
        char ids[80];
        snprintf(ids, sizeof(ids), "[\"%s\"]", DeviceIdentity::deviceId());
        j += "\"ids\":";
        j += ids;
        j += ",";
        // Board metadata straight from the BSP, so every environment gets its
        // own correct name in HA for free.
        appendKV(j, "name", bsp_hw.device_name);
        appendKV(j, "mf",   bsp_hw.MANUFACTURER);
        appendKV(j, "mdl",  bsp_hw.MODEL);
        appendKV(j, "sw",   FW_VERSION);
        closeObj(j);
    }

    // Origin: what software produced this. Fleet-wide, identical on every board.
    j += ",\"o\":{";
    appendKV(j, "name", "ESP32 Voice Assistant Fleet");
    appendKV(j, "sw",   FW_VERSION);
    closeObj(j);

    j += ",\"cmps\":{";
    bool first = true;
    uint8_t advertised = 0;
    for (uint8_t i = 0; i < _reg->count(); i++) {
        const Entity *e = _reg->at(i);
        if (!e || !e->desc.advertise) continue;   // not ours to announce

        // Catch the silent-rejection case before HA does. We do not publish
        // command topics yet, so any commandable platform would be dropped by
        // HA without explanation. Say so rather than letting an entity quietly
        // go missing from the device page.
        if (kindNeedsCommandTopic(e->desc.kind)) {
            Serial.printf("[HaPub] WARNING: \"%s\" is a %s, which HA requires a "
                          "command topic for. It will NOT appear. A read-only "
                          "value should be a sensor.\n",
                          e->desc.id, entityKindHaPlatform(e->desc.kind));
        }

        appendComponent(j, *e, first);
        first = false;
        advertised++;
    }
    j += "}}";

    if (advertised == 0) {
        Serial.println("[HaPub] Nothing to advertise; skipping discovery.");
        return;
    }

    char topic[160];
    snprintf(topic, sizeof(topic), "%s/device/%s/config",
             "homeassistant", DeviceIdentity::deviceId());

    Serial.printf("[HaPub] Discovery: %u entities, %u bytes -> %s\n",
                  (unsigned)advertised, (unsigned)j.length(), topic);

    if (_mqtt->publish(topic, j.c_str(), /*retain=*/true)) {
        _discoveryDone = true;
        Serial.println("[HaPub] Discovery published.");
    } else {
        // publish() already explains itself; the usual cause is the payload
        // exceeding MqttSettings::BUFFER_SIZE.
        Serial.println("[HaPub] Discovery FAILED - see the publish error above.");
    }
}

void HaPublisher::publishState(const Entity &e, uint8_t idx, uint32_t nowMs) {
    char topic[ENTITY_TOPIC_MAX];
    stateTopicFor(e, topic, sizeof(topic));

    char payload[ENTITY_TEXT_MAX + 16];
    switch (e.value.type) {
        case ValueType::BOOL:     snprintf(payload, sizeof(payload), "%s", e.value.b ? "ON" : "OFF"); break;
        case ValueType::INT:      snprintf(payload, sizeof(payload), "%ld", (long)e.value.i); break;
        case ValueType::FLOAT:    snprintf(payload, sizeof(payload), "%.2f", e.value.f); break;
        case ValueType::TEXT_VAL: snprintf(payload, sizeof(payload), "%s", e.value.text); break;
        default: return;   // nothing meaningful to say yet
    }

    if (_mqtt->publish(topic, payload, /*retain=*/true)) {
        _lastPub[idx]   = e.value;
        _everPub[idx]   = true;
        _lastPubMs[idx] = nowMs;
    }
}

void HaPublisher::loop(uint32_t nowMs) {
    if (!_reg || !_mqtt) return;

    const bool connected = _mqtt->isConnected();

    // A reconnect invalidates everything: the broker may have restarted with no
    // retained data at all, so discovery and every state are republished.
    if (connected && !_wasConnected) {
        _discoveryDone = false;
        for (uint8_t i = 0; i < ENTITY_MAX; i++) _everPub[i] = false;
    }
    _wasConnected = connected;

    if (!connected) return;

    if (!_discoveryDone) {
        publishDiscovery();
        // Deliberately return: give the discovery payload its own loop
        // iteration rather than following it immediately with a burst of state
        // publishes on a link that has just come up.
        return;
    }

    for (uint8_t i = 0; i < _reg->count(); i++) {
        const Entity *e = _reg->at(i);
        if (!e || !e->desc.advertise) continue;
        if (!e->everSet) continue;              // no reading yet; nothing honest to send

        const bool changed   = !_everPub[i] || !_lastPub[i].equals(e->value);
        const bool heartbeat = _everPub[i] && (nowMs - _lastPubMs[i]) > HEARTBEAT_MS;

        if (changed || heartbeat) publishState(*e, i, nowMs);
    }
}
