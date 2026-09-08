#pragma once
#ifndef MQTT_PROVIDER_H
#define MQTT_PROVIDER_H

#include <Arduino.h>
#include "EntityRegistry.h"
#include "MqttManager.h"

// ---------------------------------------------------------------------------
// Reads values published by OTHER devices and writes them into the registry.
//
// The mirror image of HaPublisher:
//
//   HaPublisher   - entities with advertise = true.  We own them, we announce
//                   them, we publish their values outward.
//   MqttProvider  - entities with source = MQTT and an externalRef. Someone
//                   else owns them; we subscribe and read.
//
// Both write to the same registry, and a card cannot tell which one filled a
// value in. That is ROADMAP 4.1 working as intended.
//
// Subscriptions are derived from the entity table, not hardcoded: register an
// entity with an externalRef and this subscribes to it. Several entities may
// share one topic - a Zigbee2MQTT sensor publishes temperature, humidity and
// battery in one message, and each entity picks its own key via valueKey.
// ---------------------------------------------------------------------------

class MqttProvider {
public:
    void begin(EntityRegistry *reg, MqttManager *mqtt);

    // Re-subscribes after a reconnect. Cheap to call every iteration.
    void loop(uint32_t nowMs);

    uint16_t messagesHandled() const { return _handled; }
    uint16_t messagesUnmatched() const { return _unmatched; }

private:
    void subscribeAll();

    // MqttManager hands out a bare function pointer, so the instance is
    // reached through a file-static - the same shape MqttManager itself uses
    // for PubSubClient's callback.
    static void onMessage(const char *topic, const uint8_t *payload,
                          unsigned int length, bool retained);
    void handle(const char *topic, const uint8_t *payload, unsigned int length);

    // Writes a value and announces the FIRST one each entity ever receives.
    // A working parse is otherwise completely silent, which makes "is my
    // sensor arriving?" unanswerable without opening the System panel.
    // Self-limiting: one line per entity for the life of the boot.
    void applyValue(const char *id, const EntityValue &v, uint32_t now,
                    bool wasEverSet, const char *unit);

    EntityRegistry *_reg  = nullptr;
    MqttManager    *_mqtt = nullptr;

    bool     _subscribed = false;
    bool     _wasConnected = false;
    uint16_t _handled   = 0;
    uint16_t _unmatched = 0;

    static MqttProvider *s_self;
};

#endif // MQTT_PROVIDER_H
