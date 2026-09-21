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

// ROADMAP Q5: command = <base>/<object_id>/set. Issue #44.
//
// The /state and /set pair is not a house style - it is HA's own discovery
// convention (state_topic vs command_topic), it is what Zigbee2MQTT does, and
// it is what the owner's Shed Power Monitor already does with
// `.../motion_timer/state` and `.../motion_timer/set`. Three independent
// sources agreeing was worth checking before inventing a fourth.
//
// Derived here rather than stored on the descriptor, deliberately: for an
// entity WE own, the topic follows from the device identity, and ROADMAP 4.1
// is explicit that a descriptor never spells its own topics out. Only entities
// someone ELSE owns carry a commandRef, because only they have an address we
// cannot compute.
void HaPublisher::commandTopicFor(const Entity &e, char *out, size_t outLen) const {
    snprintf(out, outLen, "%s/%s/set", _mqtt->getBaseTopic(), e.desc.id);
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

    // Issue #44. Only for platforms HA requires it on, and only when the entity
    // can actually take an order - advertising a command topic for something
    // read-only would draw a control that silently does nothing.
    if (kindNeedsCommandTopic(e.desc.kind) && e.desc.writable) {
        char cmdTopic[ENTITY_TOPIC_MAX];
        commandTopicFor(e, cmdTopic, sizeof(cmdTopic));
        appendKV(json, "cmd_t", cmdTopic);
    }
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
        appendKV(j, "sw",   _swVersion);
        closeObj(j);
    }

    // Origin: what software produced this. Fleet-wide, identical on every board.
    j += ",\"o\":{";
    appendKV(j, "name", "ESP32 Voice Assistant Fleet");
    appendKV(j, "sw",   _swVersion);
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

// Tell HA this entity is deliberately not reporting. Issue #60.
//
// Published to the ENTITY'S OWN STATE TOPIC, not to an availability topic,
// because there is no per-entity availability topic: discovery points every
// entity at the DEVICE-level LWT (pl_avail / pl_not_avail above). Using that
// would mark the whole panel offline, which is the opposite of what the owner
// asked for - "set it as Unavailable for the sensor only, obviously not for the
// board itself or any other entities".
//
// Retained, so HA still sees it after a broker reconnect - a pause that
// survives our reboot but not the broker's would be a odd half-promise.
//
// NOT YET CONFIRMED ON HARDWARE: HA special-cases the literal payload
// "unavailable" on a state topic for sensor and binary_sensor. That is the
// documented behaviour and it is why this is a one-line change rather than a
// discovery-schema change (#47 warns that payload is already close to the MQTT
// buffer limit), but nobody has watched it happen on this fleet. If HA instead
// shows the string "unavailable" as a value, the fix is a per-entity
// availability_topic and it belongs with #47.
void HaPublisher::publishPaused(const Entity &e) {
    char topic[ENTITY_TOPIC_MAX];
    stateTopicFor(e, topic, sizeof(topic));
    _mqtt->publish(topic, "unavailable", /*retain=*/true);
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

        // PAUSED ENTITIES STOP TRANSMITTING. Issue #60.
        //
        // The owner's case: "if the device was sending temp data to HA but I
        // knew the temp sensor was broken, I would not want it sending bogus
        // data to HA." So we go quiet - but we say so FIRST, once, rather than
        // just stopping. Silence would leave HA showing the last good reading
        // indefinitely, which is precisely the bogus data he is trying to stop.
        //
        // Scoped to this entity. The board's own availability topic and every
        // other entity are untouched: a paused deck probe must not make the
        // whole panel look offline.
        if (e->paused) {
            if (!_pauseAnnounced[i]) {
                publishPaused(*e);
                _pauseAnnounced[i] = true;
                Serial.printf("[HaPub] %s paused - published unavailable\n",
                              e->desc.id);
            }
            continue;
        }
        if (_pauseAnnounced[i]) {
            // Resumed. Clear the flag and fall through; the value publish
            // below is itself the "it is back" signal.
            _pauseAnnounced[i] = false;
            // No explicit "available" is needed: the value publish that falls
            // through below is itself the proof it is back, exactly as it is on
            // the inbound side (EntityRegistry::setValue).
            Serial.printf("[HaPub] %s resumed\n", e->desc.id);
        }
        if (!e->everSet) continue;              // no reading yet; nothing honest to send

        const bool changed   = !_everPub[i] || !_lastPub[i].equals(e->value);
        const bool heartbeat = _everPub[i] && (nowMs - _lastPubMs[i]) > HEARTBEAT_MS;

        if (changed || heartbeat) publishState(*e, i, nowMs);
    }
}
