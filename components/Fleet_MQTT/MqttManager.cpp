#include "MqttManager.h"
#include "DeviceIdentity.h"

#include <string.h>

// Diagnostics worth keeping, gated per the repo's debug-flag convention.
// Enable with -D DEBUG_MQTT in a specific environment's build_flags.
#ifdef DEBUG_MQTT
    #define DBG_MQTT(...) Serial.printf("[Mqtt:debug] " __VA_ARGS__)
#else
    #define DBG_MQTT(...) do {} while (0)
#endif

MqttManager *MqttManager::s_self = nullptr;

// ---------------------------------------------------------------------------
// Types
// ---------------------------------------------------------------------------

const char *mqttStateName(MqttState s) {
    switch (s) {
        case MqttState::SESSION_OFF:  return "disabled";
        case MqttState::NO_LINK:      return "no-link";
        case MqttState::CONNECTING:   return "connecting";
        case MqttState::CONNECTED:    return "connected";
        case MqttState::BACKOFF:      return "backoff";
        case MqttState::AUTH_STOPPED: return "auth-stopped";
    }
    return "?";
}

const char *mqttFailureName(MqttFailure f) {
    switch (f) {
        case MqttFailure::NONE:          return "none";
        case MqttFailure::ENVIRONMENTAL: return "broker unreachable";
        case MqttFailure::CREDENTIALS:   return "rejected credentials";
        case MqttFailure::PROTOCOL:      return "protocol/client-id rejected";
    }
    return "?";
}

MqttFailure mqttClassify(int st) {
    switch (st) {
        case -4: // MQTT_CONNECTION_TIMEOUT
        case -3: // MQTT_CONNECTION_LOST
        case -2: // MQTT_CONNECT_FAILED
        case -1: // MQTT_DISCONNECTED
        case  3: // MQTT_CONNECT_UNAVAILABLE - broker up but not accepting yet
            return MqttFailure::ENVIRONMENTAL;
        case  4: // MQTT_CONNECT_BAD_CREDENTIALS
        case  5: // MQTT_CONNECT_UNAUTHORIZED
            return MqttFailure::CREDENTIALS;
        case  1: // MQTT_CONNECT_BAD_PROTOCOL
        case  2: // MQTT_CONNECT_BAD_CLIENT_ID
            return MqttFailure::PROTOCOL;
        default:
            return MqttFailure::NONE;
    }
}

// ---------------------------------------------------------------------------
// Lifecycle
// ---------------------------------------------------------------------------

void MqttManager::begin(ConnectivityManager *link, const MqttSettings &settings) {
    _link = link;
    _cfg  = settings;
    s_self = this;

    if (!_cfg.ENABLED || !_cfg.BROKER_HOST || _cfg.BROKER_HOST[0] == '\0') {
        setState(MqttState::SESSION_OFF);
        Serial.println("[Mqtt] Disabled (no broker configured).");
        _begun = true;
        return;
    }

    // Topics are derived from the same device identity the hostname comes
    // from, so an MQTT topic and a DHCP lease can never disagree about which
    // board is which. Per ROADMAP Q5.
    snprintf(_baseTopic, sizeof(_baseTopic), "%s/%s",
             _cfg.TOPIC_PREFIX, DeviceIdentity::deviceId());
    snprintf(_availTopic, sizeof(_availTopic), "%s/status", _baseTopic);

    _client.setServer(_cfg.BROKER_HOST, _cfg.BROKER_PORT);
    _client.setCallback(&MqttManager::staticCallback);
    _client.setKeepAlive(_cfg.KEEPALIVE_S);
    _client.setSocketTimeout(_cfg.SOCKET_TIMEOUT_S);

    // Not optional - see MqttDefaults.h. A false return means the allocation
    // failed, which would leave every publish silently dropped, so it is
    // reported loudly rather than ignored.
    if (!_client.setBufferSize(_cfg.BUFFER_SIZE)) {
        Serial.printf("[Mqtt] WARNING: could not allocate a %u-byte buffer. "
                      "Large payloads (HA discovery) will be dropped silently.\n",
                      (unsigned)_cfg.BUFFER_SIZE);
    }

    _backoffMs     = 0;
    _lastAttemptMs = 0;
    _authFailCount = 0;
    _begun         = true;

    setState(MqttState::NO_LINK);
    Serial.printf("[Mqtt] Configured: %s:%u  base=\"%s\"\n",
                  _cfg.BROKER_HOST, (unsigned)_cfg.BROKER_PORT, _baseTopic);
}

void MqttManager::setState(MqttState s) {
    if (_state == s) return;
    Serial.printf("[Mqtt] %s -> %s\n", mqttStateName(_state), mqttStateName(s));
    _state = s;
}

// ---------------------------------------------------------------------------
// Main loop
// ---------------------------------------------------------------------------

void MqttManager::loop() {
    if (!_begun || _state == MqttState::SESSION_OFF) return;

    const uint32_t now = millis();

    // A broker session is meaningless without a link. Drop straight back to
    // NO_LINK rather than burning connect attempts against a dead interface -
    // and note this is deliberately the ONLY thing MqttManager asks about the
    // link. It never learns whether it is WiFi, AP or Ethernet.
    if (!_link || !_link->isOnline()) {
        if (_client.connected()) _client.disconnect();
        if (_state != MqttState::AUTH_STOPPED) setState(MqttState::NO_LINK);
        return;
    }

    if (_client.connected()) {
        _client.loop();
        return;
    }

    // Was connected, is not any more.
    if (_state == MqttState::CONNECTED) {
        Serial.println("[Mqtt] Disconnected from broker.");
        escalate(now, MqttFailure::ENVIRONMENTAL);
        return;
    }

    // Credentials the broker has actively rejected will not start working on
    // a timer. Same reasoning as ConnectivityManager's unproven auth stop:
    // only a configuration change or an explicit retry gets out of here.
    if (_state == MqttState::AUTH_STOPPED) return;

    if (_state == MqttState::NO_LINK) {
        // Link just came up.
        _backoffMs     = 0;
        _lastAttemptMs = 0;
    }

    if (_backoffMs == 0 || (now - _lastAttemptMs) >= _backoffMs) {
        attemptConnect(now);
    }
}

void MqttManager::attemptConnect(uint32_t now) {
    _lastAttemptMs = now;
    setState(MqttState::CONNECTING);

    // The client id must be stable and unique across the fleet, or two boards
    // will silently evict each other from the broker in a reconnect loop that
    // looks like a flaky network.
    const char *clientId = DeviceIdentity::deviceId();

    const char *user = (_cfg.USERNAME && _cfg.USERNAME[0]) ? _cfg.USERNAME : nullptr;
    const char *pass = (_cfg.PASSWORD && _cfg.PASSWORD[0]) ? _cfg.PASSWORD : nullptr;

    DBG_MQTT("connect id=\"%s\" user=%s\n", clientId, user ? user : "(anon)");

    // Last Will armed in the CONNECT packet itself: retained, QoS 1, so an
    // ungraceful drop marks every entity unavailable in Home Assistant.
    const bool ok = _client.connect(clientId, user, pass,
                                    _availTopic, 1, true, "offline");

    if (ok) {
        onConnected();
        return;
    }

    const int  raw = _client.state();
    const MqttFailure why = mqttClassify(raw);
    DBG_MQTT("connect failed raw=%d -> %s\n", raw, mqttFailureName(why));
    escalate(now, why);
}

void MqttManager::onConnected() {
    _backoffMs     = 0;
    _authFailCount = 0;
    _lastFailure   = MqttFailure::NONE;
    setState(MqttState::CONNECTED);

    // Birth message, retained, matching the Will's topic and QoS.
    _client.publish(_availTopic, "online", true);

    resubscribeAll();

    Serial.printf("[Mqtt] Online: %s:%u as \"%s\"\n",
                  _cfg.BROKER_HOST, (unsigned)_cfg.BROKER_PORT,
                  DeviceIdentity::deviceId());
}

void MqttManager::escalate(uint32_t now, MqttFailure why) {
    _lastFailure   = why;
    _lastAttemptMs = now;

    if (why == MqttFailure::CREDENTIALS) {
        // Tolerate a couple, because a broker restarting can reject before its
        // auth backend is ready - the direct analogue of WiFi reason 2
        // (AUTH_EXPIRE) being transient rather than terminal.
        if (++_authFailCount >= _cfg.AUTH_RETRY_COUNT) {
            setState(MqttState::AUTH_STOPPED);
            Serial.println("[Mqtt] Broker rejected these credentials. Not retrying "
                           "automatically - correct them and reboot, or use Retry now.");
            return;
        }
    } else {
        _authFailCount = 0;
    }

    // Environmental ladder: double to a ceiling, then hold there forever. A
    // broker that is down is usually a broker that is restarting.
    if (_backoffMs == 0) {
        _backoffMs = _cfg.BACKOFF_INITIAL_MS;
    } else {
        _backoffMs = (_backoffMs * 2 > _cfg.BACKOFF_MAX_MS)
                     ? _cfg.BACKOFF_MAX_MS : _backoffMs * 2;
    }

    setState(MqttState::BACKOFF);
    Serial.printf("[Mqtt] Next broker retry in %lu s (%s)\n",
                  (unsigned long)(_backoffMs / 1000), mqttFailureName(why));
}

void MqttManager::stop() {
    if (!_begun) return;

    // The Will only fires on an UNGRACEFUL disconnect, so a clean shutdown has
    // to say "offline" itself or Home Assistant keeps showing every entity as
    // available until the keepalive eventually lapses.
    if (_client.connected()) {
        _client.publish(_availTopic, "offline", true);
        _client.disconnect();
    }
    _backoffMs     = 0;
    _authFailCount = 0;
    setState(_cfg.ENABLED ? MqttState::NO_LINK : MqttState::SESSION_OFF);
}

void MqttManager::retryNow() {
    if (!_begun || _state == MqttState::SESSION_OFF) return;
    _authFailCount = 0;
    _backoffMs     = 0;
    _lastAttemptMs = 0;
    _lastFailure   = MqttFailure::NONE;
    setState(MqttState::NO_LINK);   // loop() picks it up on the next iteration
}

uint32_t MqttManager::secondsUntilRetry() const {
    if (_state != MqttState::BACKOFF || _backoffMs == 0) return 0;
    const uint32_t elapsed = millis() - _lastAttemptMs;
    if (elapsed >= _backoffMs) return 0;
    return (_backoffMs - elapsed) / 1000;
}

// ---------------------------------------------------------------------------
// Publish / subscribe
// ---------------------------------------------------------------------------

bool MqttManager::publish(const char *topic, const char *payload, bool retain) {
    if (!_client.connected() || !topic || !payload) return false;

    const bool ok = _client.publish(topic, payload, retain);
    if (!ok) {
        // Almost always the buffer: PubSubClient refuses anything larger than
        // its buffer and reports it only through this return value.
        Serial.printf("[Mqtt] publish to \"%s\" FAILED (%u bytes; buffer %u)\n",
                      topic, (unsigned)strlen(payload), (unsigned)_cfg.BUFFER_SIZE);
    }
    return ok;
}

bool MqttManager::addSub(const char *topic, bool isCommand) {
    if (!topic || !topic[0]) return false;
    if (_subCount >= MAX_SUBS) {
        Serial.printf("[Mqtt] subscription table full, dropping \"%s\"\n", topic);
        return false;
    }
    snprintf(_subs[_subCount].topic, sizeof(_subs[_subCount].topic), "%s", topic);
    _subs[_subCount].isCommand = isCommand;
    _subCount++;

    if (isCommand && _cmdTopicCount < MAX_CMD_TOPICS) {
        snprintf(_cmdTopics[_cmdTopicCount], sizeof(_cmdTopics[0]), "%s", topic);
        _cmdTopicCount++;
    }

    // Subscribe immediately when already connected; otherwise resubscribeAll()
    // will replay it on the next successful connect. Either way the retained
    // window restarts, because this SUBSCRIBE can itself trigger a retained
    // delivery.
    if (_client.connected()) {
        _client.subscribe(topic, 1);
        _subscribedAtMs = millis();
    }
    return true;
}

bool MqttManager::subscribeState(const char *topic)   { return addSub(topic, false); }
bool MqttManager::subscribeCommand(const char *topic) { return addSub(topic, true);  }

void MqttManager::resubscribeAll() {
    // A broker restart drops every subscription and PubSubClient does not
    // remember them, so they are replayed on each connect rather than only at
    // startup. Without this, a device survives a broker restart but silently
    // stops responding to commands.
    for (uint8_t i = 0; i < _subCount; i++) {
        _client.subscribe(_subs[i].topic, 1);
        DBG_MQTT("resubscribed \"%s\"%s\n", _subs[i].topic,
                 _subs[i].isCommand ? " (command)" : "");
    }
    // Starts the window during which a command-topic delivery is assumed to be
    // a retained replay rather than a live instruction.
    _subscribedAtMs = millis();
    if (_subCount) Serial.printf("[Mqtt] Subscribed to %u topic(s).\n", (unsigned)_subCount);
}

bool MqttManager::isCommandTopic(const char *topic) const {
    for (uint8_t i = 0; i < _cmdTopicCount; i++) {
        if (strcmp(_cmdTopics[i], topic) == 0) return true;
    }
    return false;
}

// ---------------------------------------------------------------------------
// Inbound
// ---------------------------------------------------------------------------

void MqttManager::staticCallback(char *topic, uint8_t *payload, unsigned int length) {
    MqttManager *self = s_self;
    if (!self) return;

    // --- The retained-command guard, and why it is shaped like this ---------
    //
    // Retained payloads on a COMMAND topic must be dropped: a broker
    // redelivers them on every reconnect, so acting on one replays a stale
    // instruction forever. On a "reboot" command that is an outright boot loop
    // (receive, reboot, reconnect, receive again...) - a failure mode the
    // reference project in reference/ documents having hit. See issue #9.
    //
    // PubSubClient does NOT surface the MQTT retain flag through its callback,
    // so the flag cannot simply be read. Rather than ship a guard that looks
    // like protection and is not, timing is used instead:
    //
    // A broker delivers retained messages immediately in response to a
    // SUBSCRIBE. So anything landing on a command topic within a short window
    // of our subscribe is retained with high probability; anything later is
    // live.
    //
    // The trade-off is explicit: a genuine command issued in the first couple
    // of seconds after this device connects is also dropped. That is the right
    // way round - a command arriving in that window is far more likely to be
    // the broker replaying history than a human acting on a device that has
    // only just appeared in Home Assistant.
    //
    // If we move to a library that exposes the retain flag (espMqttClient
    // does, as does ESP-IDF's esp-mqtt), delete this heuristic and read it.
    bool retained = false;
    if (self->isCommandTopic(topic)) {
        const uint32_t sinceSubscribe = millis() - self->_subscribedAtMs;
        if (sinceSubscribe < RETAINED_CMD_WINDOW_MS) {
            Serial.printf("[Mqtt] Ignoring probable retained payload on command "
                          "topic %s (%lu ms after subscribe)\n",
                          topic, (unsigned long)sinceSubscribe);
            return;
        }
    }

    if (self->_handler) {
        self->_handler(topic, payload, length, retained);
    }
}
