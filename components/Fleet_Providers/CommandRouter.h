#pragma once
#ifndef COMMAND_ROUTER_H
#define COMMAND_ROUTER_H

#include <Arduino.h>
#include "EntityRegistry.h"

class MqttManager;
class HaClient;

// ---------------------------------------------------------------------------
// The outbound leg. Issue #44.
//
// `EntityRegistry::commandValue()` has existed since #10: it applies a value
// optimistically, remembers what to fall back to, and `tick()` reverts it if no
// echo arrives. All of that was written, compiled, and **never transmitted
// anything**, because nothing turned a command into a publish. This is that.
//
//
// ONE PLACE DECIDES WHICH TRANSPORT, AND IT IS NOT THE REGISTRY.
//
// The registry stays transport-agnostic - ROADMAP 4.1's whole point is that a
// card binds to an entity and never learns how the value travels. So the
// registry hands a commanded entity to a sink, and this class is the sink. It
// is the only code in the project that knows a command can go two ways.
//
//   EntitySource::HA    -> websocket `call_service`
//   EntitySource::MQTT  -> publish to the device's command topic
//   LOCAL / SYSTEM      -> nothing to send; the value IS the truth
//   VIRTUAL             -> VirtualProvider answers it itself
//
//
// THE SERVICE DOMAIN COMES FROM THE ENTITY ID, NOT FROM `kind`.
//
// The owner uses `switch.tv_room_switch_1` as a living-room light, and the card
// is declared LIGHT because that is what it is in the room. Calling
// `light.turn_on` on it would fail. Home Assistant guarantees the part of an
// entity id before the dot names the service domain, so that is what gets read
// - and only that part. `dashboard-target-7b.md` warns against parsing meaning
// out of an entity id and it is right, but the warning is about the part AFTER
// the dot, where `switch.office_plug_3d_printer` is named "Living Room Plug".
//
//
// ECHO, NOT ACKNOWLEDGEMENT.
//
// Nothing here waits for a reply. `call_service` returns a result and MQTT
// returns nothing at all, and neither says the light actually changed - only
// the state coming back does. That arrives through the ordinary inbound path
// and clears `pending` exactly as an unsolicited change would. Measured against
// the owner's instance on 2026-09-20: a `light.turn_on` echoes back through
// `subscribe_trigger` in **~91 ms**, so the reconcile window has orders of
// magnitude of slack.
// ---------------------------------------------------------------------------

class CommandRouter {
public:
    // Either transport may be null; a command for a missing one is refused
    // loudly rather than dropped.
    void begin(EntityRegistry *reg, MqttManager *mqtt, HaClient *ha);

    uint16_t sent()     const { return _sent; }
    uint16_t refused()  const { return _refused; }

private:
    // The registry's CommandSink. Called with the registry lock RELEASED.
    static void onCommand(const Entity &e, const EntityValue &v, void *ctx);
    void route(const Entity &e, const EntityValue &v);

    bool sendHa(const Entity &e, const EntityValue &v);
    bool sendMqtt(const Entity &e, const EntityValue &v);

    EntityRegistry *_reg  = nullptr;
    MqttManager    *_mqtt = nullptr;
    HaClient       *_ha   = nullptr;

    uint16_t _sent    = 0;
    uint16_t _refused = 0;
};

#endif // COMMAND_ROUTER_H
