#pragma once
#ifndef MQTT_MANAGER_H
#define MQTT_MANAGER_H

#include <Arduino.h>
#include <WiFiClient.h>
#include <PubSubClient.h>

#include "MqttTypes.h"
#include "MqttDefaults.h"
#include "ConnectivityManager.h"

// ---------------------------------------------------------------------------
// Fleet MQTT - broker session only.
//
// Per ROADMAP Q9 this library owns the broker session and nothing else: it
// never learns what an entity is, and it never learns whether the link under
// it is WiFi or Ethernet. Entities live in Fleet_Entities; the code that
// bridges the two is a provider, not this class.
//
// Non-blocking throughout. loop() is called from the main loop beside
// ConnectivityManager::loop() and never spins, sleeps or waits on the network.
// ---------------------------------------------------------------------------

class MqttManager {
public:
    // Called for every message on a subscribed topic.
    //
    // IMPORTANT: this runs from loop() (PubSubClient is polled, not
    // interrupt-driven), so it is already on the main task. It must still be
    // treated as a parse-and-enqueue point rather than a place to do work:
    // once the Entity Registry exists it will be the thing draining the
    // queue, and a handler that blocks here stalls the keepalive.
    using MessageHandler = void (*)(const char *topic,
                                    const uint8_t *payload,
                                    unsigned int length,
                                    bool retained);

    // link must outlive this object. MqttManager only ever reads its state -
    // it never drives the radio.
    void begin(ConnectivityManager *link,
               const MqttSettings &settings = MQTT_DEFAULT_SETTINGS);

    // Drives the session: connects when the link comes up, runs the backoff
    // ladder, and pumps PubSubClient. Cheap to call every iteration.
    void loop();

    // Graceful shutdown. Publishes "offline" to the availability topic
    // EXPLICITLY before disconnecting - the Last Will only fires on an
    // ungraceful drop, so a clean stop would otherwise leave every entity
    // showing as available in Home Assistant indefinitely.
    void stop();

    // --- Publishing --------------------------------------------------------

    bool publish(const char *topic, const char *payload, bool retain = false);

    // --- Subscribing -------------------------------------------------------
    //
    // Two deliberately separate calls, because the retained-message rule is
    // opposite for the two kinds of topic and getting it wrong is a genuine
    // footgun rather than a style question.

    // A STATE topic: retained payloads are wanted. The broker's retained value
    // is the last known state, which is exactly what should be restored.
    bool subscribeState(const char *topic);

    // A COMMAND topic: retained payloads are DROPPED, with a warning.
    //
    // A broker redelivers retained messages on every reconnect. On a command
    // topic that is not a command, it is a stale instruction replayed forever
    // - and it bites hardest on the commands that matter most. A retained
    // "reboot" payload reboots the device, which reconnects, which receives it
    // again. The reference project this pattern came from documents exactly
    // that boot loop. See GitHub issue #9.
    bool subscribeCommand(const char *topic);

    void setMessageHandler(MessageHandler h) { _handler = h; }

    // --- State -------------------------------------------------------------

    MqttState   getState() const     { return _state; }
    MqttFailure getLastFailure() const { return _lastFailure; }
    bool        isConnected() const  { return _state == MqttState::CONNECTED; }

    // "<TOPIC_PREFIX>/<deviceId>", e.g. "fleet/fleet_ws_p4_5_e0d24b".
    // Per ROADMAP Q5. Valid after begin().
    const char *getBaseTopic() const { return _baseTopic; }

    // "<baseTopic>/status" - the LWT / birth topic every discovery payload
    // points at via avty_t.
    const char *getAvailabilityTopic() const { return _availTopic; }

    // Seconds until the next connect attempt, 0 when not in BACKOFF.
    uint32_t secondsUntilRetry() const;

    // Clears a latched AUTH_STOPPED and retries immediately. For a settings
    // UI "retry now" control after credentials are corrected.
    void retryNow();

private:
    void   attemptConnect(uint32_t now);
    void   onConnected();
    void   escalate(uint32_t now, MqttFailure why);
    void   setState(MqttState s);
    static void staticCallback(char *topic, uint8_t *payload, unsigned int length);

    ConnectivityManager *_link = nullptr;
    MqttSettings         _cfg{};
    WiFiClient           _net;
    PubSubClient         _client{_net};
    MessageHandler       _handler = nullptr;

    MqttState   _state       = MqttState::SESSION_OFF;
    MqttFailure _lastFailure = MqttFailure::NONE;

    uint32_t _backoffMs      = 0;
    uint32_t _lastAttemptMs  = 0;
    uint8_t  _authFailCount  = 0;
    bool     _begun          = false;

    char _baseTopic[96]  = {0};
    char _availTopic[112] = {0};

    // Command topics are remembered so retained payloads arriving on them can
    // be dropped. A small fixed table: this is a panel, not a broker bridge,
    // and a fixed array avoids heap churn in the message path.
    //
    // PubSubClient does not expose the MQTT retain flag to its callback, so
    // "is this retained?" is inferred from timing instead: a broker delivers
    // retained messages immediately in response to SUBSCRIBE, so anything on a
    // command topic within this window of our subscribe is treated as retained
    // and dropped. Short deliberately - it also swallows genuine commands, and
    // that is the correct trade (see MqttManager.cpp for the full reasoning).
    static constexpr uint32_t RETAINED_CMD_WINDOW_MS = 2000;
    uint32_t _subscribedAtMs = 0;

    static constexpr uint8_t MAX_CMD_TOPICS = 12;
    char    _cmdTopics[MAX_CMD_TOPICS][96] = {};
    uint8_t _cmdTopicCount = 0;
    bool    isCommandTopic(const char *topic) const;

    // Subscriptions are replayed after every reconnect - a broker restart
    // drops them, and PubSubClient does not remember them for us.
    struct Sub { char topic[96]; bool isCommand; };
    static constexpr uint8_t MAX_SUBS = 16;
    Sub     _subs[MAX_SUBS] = {};
    uint8_t _subCount = 0;
    bool    addSub(const char *topic, bool isCommand);
    void    resubscribeAll();

    static MqttManager *s_self;   // PubSubClient's callback is a bare function
};

#endif // MQTT_MANAGER_H
