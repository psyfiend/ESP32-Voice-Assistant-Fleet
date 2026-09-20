#pragma once
#ifndef HA_PROVIDER_H
#define HA_PROVIDER_H

#include <Arduino.h>
#include "EntityRegistry.h"
#include "HaClient.h"

// ---------------------------------------------------------------------------
// Reads Home Assistant's entities over the websocket and writes them into the
// registry.
//
// The websocket twin of MqttProvider, and deliberately the same shape: it
// derives its subscription from the entity table rather than from a hardcoded
// list, so registering an entity with source = HA and an externalRef is all it
// takes to start receiving it.
//
//
// ONE subscribe_trigger, NOT subscribe_events. This is the single most
// important decision in the file and it is measured, not preferred.
// ha-websocket.md section 3: subscribing to all state_changed on this instance
// is 731 events and 913 KB per minute - roughly 15 KB/s of JSON parsed
// continuously - to find the handful we care about. The same window filtered
// server-side to our entities was ZERO bytes. Three solar sensors alone
// produced 140 of those 731 events.
//
//
// A RECONNECT IS A COLD START. Measured 2026-09-20: HA discards a subscription
// with its connection, and a fresh socket accepts the same request id again. So
// there is nothing to clean up after a drop and everything to rebuild after
// one. This class watches HaClient::sessions() rather than its state, because
// the session COUNTER is what distinguishes "still connected" from
// "disconnected and reconnected between two loop() calls".
//
//
// THE PARSE RUNS ON THE WEBSOCKET TASK. onMessage() is called by HaClient from
// esp_websocket_client's task, so it must never touch LVGL and writes through
// EntityRegistry's mutex. See the long note in HaClient.h. Sending - which is
// only ever the subscription - happens on the loop task.
// ---------------------------------------------------------------------------

class HaProvider {
public:
    void begin(EntityRegistry *reg, HaClient *ha);

    // Loop task. Issues the subscription when a new session appears.
    void loop(uint32_t nowMs);

    bool     isSubscribed()    const { return _subscribedForSession != 0; }
    uint16_t eventsHandled()   const { return _handled; }
    uint16_t eventsUnmatched() const { return _unmatched; }
    uint16_t parseFailures()   const { return _parseFails; }

private:
    // HaClient hands out a bare function pointer, so the instance is reached
    // through the ctx argument - same shape MqttProvider uses for PubSubClient.
    static void onMessage(const char *json, size_t len, void *ctx);
    void handle(const char *json, size_t len);

    // Turn HA's string state into an EntityValue of the descriptor's declared
    // type. Returns false for "unavailable" / "unknown", which must NOT be
    // written - an unavailable entity is not a value of zero.
    bool coerce(const Entity &e, const char *state, EntityValue &out) const;

    bool sendSubscribe();

    EntityRegistry *_reg = nullptr;
    HaClient       *_ha  = nullptr;

    // The session counter this provider last subscribed for. Zero means "not
    // subscribed"; HaClient::sessions() starts at 1 on the first auth_ok.
    uint32_t _subscribedForSession = 0;

    uint16_t _handled    = 0;
    uint16_t _unmatched  = 0;
    uint16_t _parseFails = 0;
};

#endif // HA_PROVIDER_H
