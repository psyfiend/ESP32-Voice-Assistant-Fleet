#pragma once
#ifndef MQTT_TYPES_H
#define MQTT_TYPES_H

#include <stdint.h>

// ---------------------------------------------------------------------------
// Fleet MQTT - shared vocabulary.
//
// Deliberately mirrors Fleet_Connectivity's shape: a small state enum, a
// failure classifier, and name helpers for logging. The parallel is not
// cosmetic - the hard-won lesson from the WiFi state machine applies verbatim
// to a broker session: "could not connect" is not one condition. Bad
// credentials must stop; an unreachable broker must retry forever. Collapsing
// the two produces either a device that hammers a broker it will never be
// allowed into, or one that gives up on a broker that was merely rebooting.
// ---------------------------------------------------------------------------

enum class MqttState : uint8_t {
    // NOT named DISABLED: Arduino's esp32-hal-gpio.h has `#define DISABLED 0x00`
    // (an interrupt mode), so `MqttState::DISABLED` is rewritten by the
    // preprocessor into `MqttState::0x00` and the enum fails to compile in a
    // spectacularly misleading way. Same trap CLAUDE.md records for board
    // macros. Avoid bare single-word ALL-CAPS enumerators that collide with
    // Arduino's global macro namespace (DISABLED, RISING, FALLING, CHANGE,
    // HIGH, LOW, INPUT, OUTPUT, ANALOG...).
    SESSION_OFF = 0,  // not configured, or switched off. Never attempts anything.
    NO_LINK,       // ConnectivityManager is not online. Not a fault.
    CONNECTING,    // TCP + CONNECT in flight
    CONNECTED,     // session up, LWT armed
    BACKOFF,       // connect failed, waiting out the ladder
    AUTH_STOPPED,  // broker rejected our credentials. Permanent until reconfigured.
};

// Why the last connect attempt failed. Mapped from PubSubClient::state(),
// whose raw codes are terse integers, into the same three-way distinction
// ConnectivityManager draws for WiFi.
enum class MqttFailure : uint8_t {
    NONE = 0,
    ENVIRONMENTAL, // unreachable, timed out, connection lost, broker unavailable.
                   // Routinely self-resolving - retry indefinitely with backoff.
    CREDENTIALS,   // bad username/password, or not authorised. No timer fixes
                   // this; only a configuration change will.
    PROTOCOL,      // bad protocol version or rejected client id. A bug or a
                   // misconfiguration on our side, not a transient.
};

const char *mqttStateName(MqttState s);
const char *mqttFailureName(MqttFailure f);

// Classify a PubSubClient::state() return code. Kept as a free function with
// no dependencies so it can be unit-tested on a PC, the same way ROADMAP Q9
// wants Fleet_Entities to be testable.
//
// PubSubClient's codes:
//   -4 timeout   -3 connection lost   -2 connect failed   -1 disconnected
//    0 connected  1 bad protocol       2 bad client id     3 unavailable
//    4 bad credentials                 5 unauthorised
MqttFailure mqttClassify(int pubSubClientState);

#endif // MQTT_TYPES_H
