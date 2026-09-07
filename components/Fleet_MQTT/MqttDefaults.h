#pragma once
#ifndef MQTT_DEFAULTS_H
#define MQTT_DEFAULTS_H

#include <Arduino.h>
#include "MqttTypes.h"

// Optional local-only override for broker credentials during dev/testing, so
// a real broker password never needs to be committed. Exactly the same shape
// as Fleet_Connectivity's ConnectivityLocalSecrets.h, and gitignored the same
// way. Not required - if absent, MQTT stays disabled rather than attempting
// to connect to nothing. To use, create
// components/Fleet_MQTT/MqttLocalSecrets.h with:
//     #define LOCAL_MQTT_BROKER   "192.168.0.x"
//     #define LOCAL_MQTT_PORT     1883
//     #define LOCAL_MQTT_USER     "your-broker-user"
//     #define LOCAL_MQTT_PASSWORD "your-broker-password"
#if __has_include("MqttLocalSecrets.h")
    #include "MqttLocalSecrets.h"
#endif
#ifndef LOCAL_MQTT_BROKER
    #define LOCAL_MQTT_BROKER ""
#endif
#ifndef LOCAL_MQTT_PORT
    #define LOCAL_MQTT_PORT 1883
#endif
#ifndef LOCAL_MQTT_USER
    #define LOCAL_MQTT_USER ""
#endif
#ifndef LOCAL_MQTT_PASSWORD
    #define LOCAL_MQTT_PASSWORD ""
#endif

// ---------------------------------------------------------------------------
// Compile-time fallback values only, not the runtime source of truth - the
// same layering Fleet_Connectivity uses. NVS wins once the settings UI can
// write it; these apply on a fresh flash or after a factory reset.
//
// Deliberately NOT part of Fleet_BSP.h: which broker a panel talks to is a
// deployment setting, not a hardware-wiring fact.
// ---------------------------------------------------------------------------

struct MqttSettings {
    // --- Session ---
    bool        ENABLED;         // false = never connect, state stays DISABLED
    const char *BROKER_HOST;     // hostname or dotted-quad, no scheme
    uint16_t    BROKER_PORT;
    const char *USERNAME;        // empty = anonymous
    const char *PASSWORD;

    // --- Topics ---
    // Base topic is built as "<TOPIC_PREFIX>/<deviceId>", e.g.
    // "fleet/fleet_ws_p4_5_e0d24b". Per ROADMAP Q5.
    const char *TOPIC_PREFIX;
    // Where Home Assistant listens for discovery. Configurable because a
    // non-default value is a real (if uncommon) HA setting, and hardcoding it
    // is the kind of thing that wastes an afternoon.
    const char *DISCOVERY_PREFIX;

    // --- Transport tuning ---
    // PubSubClient's built-in buffer is 256 bytes and it fails SILENTLY:
    // publish() simply returns false and nothing reaches the broker. A
    // device-based HA discovery payload is far larger than that, so this is
    // not optional. See GitHub issue #11.
    uint16_t    BUFFER_SIZE;
    uint16_t    KEEPALIVE_S;
    uint32_t    SOCKET_TIMEOUT_S;

    // --- Reconnect ladder ---
    // Same shape as ConnectivityManager's environmental backoff: double until
    // a ceiling, then hold there indefinitely. A broker that is down is
    // usually a broker that is restarting.
    uint32_t    BACKOFF_INITIAL_MS;
    uint32_t    BACKOFF_MAX_MS;

    // How many credential rejections to tolerate before latching AUTH_STOPPED.
    // More than one because a broker restarting can briefly answer with a
    // rejection before its auth backend is ready - the direct analogue of
    // WiFi's reason 2 (AUTH_EXPIRE) being transient rather than terminal.
    uint8_t     AUTH_RETRY_COUNT;
};

inline constexpr MqttSettings MQTT_DEFAULT_SETTINGS = {
    // Enabled only when a broker was actually supplied. A board with no
    // MqttLocalSecrets.h stays cleanly DISABLED instead of retrying against
    // an empty hostname forever.
    .ENABLED          = (sizeof(LOCAL_MQTT_BROKER) > 1),
    .BROKER_HOST      = LOCAL_MQTT_BROKER,
    .BROKER_PORT      = LOCAL_MQTT_PORT,
    .USERNAME         = LOCAL_MQTT_USER,
    .PASSWORD         = LOCAL_MQTT_PASSWORD,

    .TOPIC_PREFIX     = "fleet",
    .DISCOVERY_PREFIX = "homeassistant",

    .BUFFER_SIZE      = 4096,
    .KEEPALIVE_S      = 30,
    .SOCKET_TIMEOUT_S = 10,

    .BACKOFF_INITIAL_MS = 5000,     // 5 s
    .BACKOFF_MAX_MS     = 300000,   // 5 min ceiling
    .AUTH_RETRY_COUNT   = 2,
};

#endif // MQTT_DEFAULTS_H
