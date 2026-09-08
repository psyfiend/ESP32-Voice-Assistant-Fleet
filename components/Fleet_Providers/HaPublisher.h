#pragma once
#ifndef HA_PUBLISHER_H
#define HA_PUBLISHER_H

#include <Arduino.h>
#include "EntityRegistry.h"
#include "MqttManager.h"

// ---------------------------------------------------------------------------
// Publishes the entities WE own to Home Assistant, over MQTT.
//
// Lives in Fleet_Providers rather than Fleet_MQTT. ROADMAP Q9's table lists
// "HA discovery publishing" under Fleet_MQTT, but doing that would force
// Fleet_MQTT to include Fleet_Entities and stop being transport-only. This is
// a bridge between the registry and the broker, which is exactly what a
// provider is - so it lives with the providers and Q9's table wants a footnote.
//
// Two jobs, both driven from loop():
//
//   DISCOVERY - one retained payload describing every entity with
//               advertise = true, republished whenever the broker reconnects.
//               Device-based format (ROADMAP Q5, decided 2026-09-07):
//               homeassistant/device/<device_id>/config, with a `cmps` map
//               rather than one payload per entity.
//
//   STATE     - each entity's value on its own topic, published when the value
//               changes, plus a slow heartbeat so a restarted broker with no
//               retained data recovers without waiting for a change.
//
// Entities with advertise = false are ignored entirely: someone else owns
// them, and telling HA they are ours would duplicate an entity that already
// exists.
// ---------------------------------------------------------------------------

class HaPublisher {
public:
    void begin(EntityRegistry *reg, MqttManager *mqtt);
    void loop(uint32_t nowMs);

    // Force a discovery republish - after entities are added late, or from a
    // settings-UI "re-announce to Home Assistant" control.
    void republishDiscovery() { _discoveryDone = false; }

    bool discoveryPublished() const { return _discoveryDone; }

private:
    void publishDiscovery();
    void publishState(const Entity &e, uint8_t idx, uint32_t nowMs);
    bool appendComponent(String &json, const Entity &e, bool first) const;
    void stateTopicFor(const Entity &e, char *out, size_t outLen) const;

    EntityRegistry *_reg  = nullptr;
    MqttManager    *_mqtt = nullptr;

    bool _discoveryDone = false;
    bool _wasConnected  = false;

    // Last value published per entity, so an unchanged reading is not
    // republished every cycle. Indexed the same way the registry is.
    EntityValue _lastPub[ENTITY_MAX];
    bool        _everPub[ENTITY_MAX] = {};
    uint32_t    _lastPubMs[ENTITY_MAX] = {};

    // Republish an unchanged value this often anyway. Cheap insurance: a
    // broker that restarted without persistence has no retained state, and a
    // value that never changes would otherwise never reappear in HA.
    static constexpr uint32_t HEARTBEAT_MS = 60000;
};

#endif // HA_PUBLISHER_H
