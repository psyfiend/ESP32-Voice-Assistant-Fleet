#include "HaClient.h"

#include <Arduino.h>
#include <ArduinoJson.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "esp_websocket_client.h"

#include "ConnectivityManager.h"
#include "EntityRegistry.h"

// The long-lived access token lives with the other local secrets, gitignored.
// Guarded so a fresh clone still BUILDS - it will report a missing token at
// runtime instead of failing to compile, which is the behaviour a new
// contributor can actually diagnose.
#if __has_include("ConnectivityLocalSecrets.h")
#include "ConnectivityLocalSecrets.h"
#endif
#ifndef LOCAL_HA_ACCESS_TOKEN
#define LOCAL_HA_ACCESS_TOKEN ""
#endif

// ---------------------------------------------------------------------------
// Names
// ---------------------------------------------------------------------------

const char *haLinkName(HaLink s) {
    switch (s) {
        case HaLink::SESSION_OFF:  return "off";
        case HaLink::LINK_IDLE:    return "idle";
        case HaLink::LINK_OPENING: return "opening";
        case HaLink::LINK_AUTHING: return "authing";
        case HaLink::LINK_READY:   return "ready";
        case HaLink::LINK_BACKOFF: return "backoff";
        case HaLink::LINK_REFUSED: return "refused";
    }
    return "?";
}

const char *haStopName(HaStop s) {
    switch (s) {
        case HaStop::STOP_NONE:         return "none";
        case HaStop::STOP_NET_DOWN:     return "net down";
        case HaStop::STOP_TRANSPORT:    return "transport";
        case HaStop::STOP_AUTH_INVALID: return "auth invalid";
        case HaStop::STOP_TIMEOUT:      return "timeout";
        case HaStop::STOP_OVERSIZE:     return "oversize";
    }
    return "?";
}

// ---------------------------------------------------------------------------
// Lifecycle
// ---------------------------------------------------------------------------

bool HaClient::begin(ConnectivityManager *conn, EntityRegistry *reg) {
    _conn = conn;
    _reg  = reg;

    const HaDefaultsT &d = haDefaults();

    // Allocated ONCE, here, so the cost lands in the boot report next to every
    // other allocation rather than appearing later as unexplained
    // fragmentation. 8 KB is a real amount of internal RAM on the S3 boards and
    // it should be visible at the moment it is taken, not discovered.
    _rxCap = d.RX_BUFFER_BYTES;
    _rx    = (char *)malloc(_rxCap);
    if (!_rx) {
        Serial.printf("[HA] FATAL: could not allocate the %lu byte receive buffer.\n",
                      (unsigned long)_rxCap);
        _rxCap = 0;
        setState(HaLink::SESSION_OFF);
        return false;
    }
    _rxLen = 0;

    if (LOCAL_HA_ACCESS_TOKEN[0] == '\0') {
        // Not fatal, and deliberately not silent. A board with no token should
        // say so once at boot rather than looking like a network fault forever.
        Serial.println("[HA] No access token. Define LOCAL_HA_ACCESS_TOKEN in "
                       "ConnectivityLocalSecrets.h. The HA session stays off.");
        setState(HaLink::SESSION_OFF);
        return false;
    }

    Serial.printf("[HA] ws://%s:%u%s, rx buffer %lu B, task stack %lu B prio %d (unpinned)\n",
                  d.HOST, (unsigned)d.PORT, d.PATH,
                  (unsigned long)_rxCap, (unsigned long)d.TASK_STACK_BYTES, d.TASK_PRIORITY);

    _backoffMs = d.BACKOFF_MIN_MS;
    _attempt   = 0;
    setState(HaLink::LINK_IDLE);
    return true;
}

void HaClient::stop(HaStop why) {
    closeSocket();
    _stop.store((uint8_t)why);
    setState(HaLink::SESSION_OFF);
    Serial.printf("[HA] session stopped: %s\n", haStopName(why));
}

void HaClient::setState(HaLink s) {
    HaLink was = (HaLink)_state.load();
    if (was == s) return;
    _state.store((uint8_t)s);
    _stateSinceMs = millis();
}

uint32_t HaClient::readyForMs(uint32_t nowMs) const {
    if ((HaLink)_state.load() != HaLink::LINK_READY) return 0;
    return nowMs - _stateSinceMs;
}

// ---------------------------------------------------------------------------
// Socket
// ---------------------------------------------------------------------------

bool HaClient::openSocket(uint32_t nowMs) {
    const HaDefaultsT &d = haDefaults();

    if (_ws) closeSocket();

    esp_websocket_client_config_t cfg;
    memset(&cfg, 0, sizeof(cfg));

    // Assigned field by field rather than with designated initializers. C++
    // requires those to appear in declaration order, and this struct's order is
    // upstream's to change - see CLAUDE.md on the same trap in BSP headers.
    cfg.host                   = d.HOST;
    cfg.port                   = d.PORT;
    cfg.path                   = d.PATH;
    cfg.transport              = WEBSOCKET_TRANSPORT_OVER_TCP;   // plain ws:// on the LAN
    cfg.buffer_size            = (int)d.WS_RX_CHUNK_BYTES;
    cfg.task_stack             = (int)d.TASK_STACK_BYTES;
    cfg.task_prio              = d.TASK_PRIORITY;
    cfg.task_name              = "ha_ws";
    cfg.network_timeout_ms     = (int)d.NETWORK_TIMEOUT_MS;
    cfg.ping_interval_sec      = d.PING_INTERVAL_MS / 1000;
    cfg.pingpong_timeout_sec   = d.PONG_TIMEOUT_MS / 1000;

    // WE own the retry schedule, not the library.
    //
    // esp_websocket_client will happily reconnect on its own, but it has no
    // idea that Fleet_Connectivity may be reporting the radio dead - and #49 is
    // precisely the bug where a component kept retrying into a link that could
    // not carry anything. Two independent retry ladders on one socket is not a
    // belt and braces, it is two components disagreeing about what is true.
    cfg.disable_auto_reconnect = true;

    _ws = esp_websocket_client_init(&cfg);
    if (!_ws) {
        Serial.println("[HA] esp_websocket_client_init failed.");
        enterBackoff(HaStop::STOP_TRANSPORT, nowMs);
        return false;
    }

    esp_websocket_register_events(_ws, WEBSOCKET_EVENT_ANY, wsEventTrampoline, this);

    esp_err_t err = esp_websocket_client_start(_ws);
    if (err != ESP_OK) {
        Serial.printf("[HA] start failed: %d\n", (int)err);
        closeSocket();
        enterBackoff(HaStop::STOP_TRANSPORT, nowMs);
        return false;
    }

    _attempt++;
    _rxLen         = 0;
    _rxOverflowed  = false;
    _authWanted.store(false);
    setState(HaLink::LINK_OPENING);
    return true;
}

void HaClient::closeSocket() {
    if (!_ws) return;
    esp_websocket_client_stop(_ws);
    esp_websocket_client_destroy(_ws);
    _ws = nullptr;
}

void HaClient::enterBackoff(HaStop why, uint32_t nowMs) {
    const HaDefaultsT &d = haDefaults();
    _stop.store((uint8_t)why);
    closeSocket();

    _retryAtMs = nowMs + _backoffMs;
    Serial.printf("[HA] %s; retry in %lu ms (attempt %lu)\n",
                  haStopName(why), (unsigned long)_backoffMs, (unsigned long)_attempt);

    _backoffMs *= 2;
    if (_backoffMs > d.BACKOFF_MAX_MS) _backoffMs = d.BACKOFF_MAX_MS;

    setState(HaLink::LINK_BACKOFF);
}

// ---------------------------------------------------------------------------
// The websocket task's entry point
// ---------------------------------------------------------------------------

void HaClient::wsEventTrampoline(void *handlerArg, const char *base,
                                 int32_t eventId, void *eventData) {
    (void)base;
    HaClient *self = (HaClient *)handlerArg;
    if (self) self->onWsEvent(eventId, eventData);
}

void HaClient::onWsEvent(int32_t eventId, void *eventData) {
    esp_websocket_event_data_t *e = (esp_websocket_event_data_t *)eventData;

    switch (eventId) {
        case WEBSOCKET_EVENT_CONNECTED:
            // The SOCKET is up. The SESSION is not - HA has yet to send
            // auth_required. Staying in LINK_OPENING until the handshake
            // completes is what stops anything upstream from sending a request
            // into a socket that will refuse it.
            Serial.println("[HA] socket connected; waiting for auth_required");
            break;

        case WEBSOCKET_EVENT_DATA: {
            if (!e) break;
            // op_code 0x1 is text and 0x0 is a continuation frame. Everything
            // else here is protocol housekeeping the library already handled:
            // 0x8 close, 0x9 ping, 0xA pong.
            if (e->op_code != 0x01 && e->op_code != 0x00) break;
            if (accumulate(e->data_ptr, e->data_len, e->payload_offset, e->payload_len)) {
                _rx[_rxLen] = '\0';
                _msgsRx.fetch_add(1);
                _bytesRx.fetch_add(_rxLen);
                dispatch(_rx, _rxLen);
                _rxLen = 0;
            }
            break;
        }

        case WEBSOCKET_EVENT_ERROR:
            _transportErrs.fetch_add(1);
            _stop.store((uint8_t)HaStop::STOP_TRANSPORT);
            break;

        case WEBSOCKET_EVENT_DISCONNECTED:
        case WEBSOCKET_EVENT_CLOSED:
            // Flagged here, ACTED ON in loop(). Tearing the client down from
            // inside its own event handler means destroying the task that is
            // currently running this code.
            if ((HaLink)_state.load() != HaLink::LINK_REFUSED) {
                _stop.store((uint8_t)HaStop::STOP_TRANSPORT);
            }
            break;

        default:
            break;
    }
}

// Reassemble a message that esp_websocket_client split across several events.
//
// payload_len is the total; payload_offset is where THIS chunk belongs. An
// oversized message is dropped whole rather than truncated: a truncated JSON
// document fails to parse in ways that look like a server bug, and this project
// has spent enough time chasing diagnostics that lied.
bool HaClient::accumulate(const char *data, int len, int payloadOffset, int payloadLen) {
    if (payloadOffset == 0) {
        _rxLen        = 0;
        _rxOverflowed = false;
    }

    if (payloadLen > 0 && (uint32_t)payloadLen >= _rxCap) {
        if (!_rxOverflowed) {
            _rxOverflowed = true;
            _oversized.fetch_add(1);
            Serial.printf("[HA] message of %d B exceeds the %lu B buffer; dropped. "
                          "Something upstream asked for more than the design allows.\n",
                          payloadLen, (unsigned long)_rxCap);
        }
        return false;
    }
    if (_rxOverflowed) return false;
    if (len <= 0 || !data) return false;

    if (_rxLen + (uint32_t)len >= _rxCap) {
        _rxOverflowed = true;
        _oversized.fetch_add(1);
        return false;
    }

    memcpy(_rx + _rxLen, data, (size_t)len);
    _rxLen += (uint32_t)len;

    // Complete when we hold every byte the server said it was sending.
    return (payloadLen > 0) && (_rxLen >= (uint32_t)payloadLen);
}

// ---------------------------------------------------------------------------
// Message classification
// ---------------------------------------------------------------------------

void HaClient::dispatch(const char *json, size_t len) {
    // The handshake is three messages and they are tiny, so a filtered parse
    // buys nothing here. The LIVE feed is the one that needs a streaming filter
    // - ha-websocket.md section 3 measured ~95% of every trigger event as
    // discardable - and that belongs to whoever registers as the message
    // handler, not here.
    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, json, len);
    if (err) {
        Serial.printf("[HA] unparseable message (%u B): %s\n",
                      (unsigned)len, err.c_str());
        return;
    }

    const char *type = doc["type"] | "";

    if (strcmp(type, "auth_required") == 0) {
        const char *ver = doc["ha_version"] | "?";
        Serial.printf("[HA] auth_required from HA %s\n", ver);
        _authWanted.store(true);      // loop() does the sending. See HaClient.h.
        return;
    }

    if (strcmp(type, "auth_ok") == 0) {
        const HaDefaultsT &d = haDefaults();
        _sessions.fetch_add(1);
        _stop.store((uint8_t)HaStop::STOP_NONE);
        _backoffMs = d.BACKOFF_MIN_MS;   // a real session resets the ladder
        _attempt   = 0;
        setState(HaLink::LINK_READY);
        Serial.println("[HA] auth_ok - session ready");
        return;
    }

    if (strcmp(type, "auth_invalid") == 0) {
        const char *msg = doc["message"] | "";
        Serial.printf("[HA] auth_invalid: %s\n", msg);
        // TERMINAL, AND THAT IS DELIBERATE. A rejected token will be rejected
        // identically on every retry, so retrying only guarantees a board that
        // hammers HA forever while displaying nothing. It needs a human.
        _stop.store((uint8_t)HaStop::STOP_AUTH_INVALID);
        setState(HaLink::LINK_REFUSED);
        return;
    }

    // Everything else is the session's business, not the socket's.
    if (_msgFn) _msgFn(json, len, _msgCtx);
}

// ---------------------------------------------------------------------------
// The loop task
// ---------------------------------------------------------------------------

void HaClient::loop(uint32_t nowMs) {
    const HaDefaultsT &d = haDefaults();
    HaLink s = (HaLink)_state.load();

    // A refused token is terminal until someone changes it and reboots.
    if (s == HaLink::SESSION_OFF || s == HaLink::LINK_REFUSED) return;

    // THE LINK IS THE PRECONDITION FOR EVERYTHING BELOW.
    //
    // isOnline() now means what it says - it returns false for a LINK_DEAD
    // board even while the state machine still claims STA_CONNECTED. That is
    // the #49 fix, and this is the first new component to get it for free.
    const bool online = _conn && _conn->isOnline();

    if (!online && s != HaLink::LINK_IDLE && s != HaLink::LINK_BACKOFF) {
        enterBackoff(HaStop::STOP_NET_DOWN, nowMs);
        return;
    }

    switch (s) {
        case HaLink::LINK_IDLE:
            if (online) openSocket(nowMs);
            break;

        case HaLink::LINK_OPENING:
            // The handler saw auth_required and deferred the send to us.
            if (_authWanted.exchange(false)) {
                char frame[512];
                int n = snprintf(frame, sizeof(frame),
                                 "{\"type\":\"auth\",\"access_token\":\"%s\"}",
                                 LOCAL_HA_ACCESS_TOKEN);
                if (n <= 0 || n >= (int)sizeof(frame)) {
                    Serial.println("[HA] auth frame did not fit; token too long?");
                    enterBackoff(HaStop::STOP_TRANSPORT, nowMs);
                    break;
                }
                int sent = esp_websocket_client_send_text(_ws, frame, n, portMAX_DELAY);
                if (sent < 0) {
                    enterBackoff(HaStop::STOP_TRANSPORT, nowMs);
                    break;
                }
                setState(HaLink::LINK_AUTHING);
                break;
            }
            if (nowMs - _stateSinceMs > d.HANDSHAKE_TIMEOUT_MS) {
                enterBackoff(HaStop::STOP_TIMEOUT, nowMs);
            }
            break;

        case HaLink::LINK_AUTHING:
            if (nowMs - _stateSinceMs > d.HANDSHAKE_TIMEOUT_MS) {
                enterBackoff(HaStop::STOP_TIMEOUT, nowMs);
            }
            break;

        case HaLink::LINK_READY:
            // The handler flags a drop and we act on it here, because it cannot
            // destroy the task it is running on.
            if ((HaStop)_stop.load() == HaStop::STOP_TRANSPORT) {
                enterBackoff(HaStop::STOP_TRANSPORT, nowMs);
            }
            break;

        case HaLink::LINK_BACKOFF:
            if ((int32_t)(nowMs - _retryAtMs) >= 0) setState(HaLink::LINK_IDLE);
            break;

        default:
            break;
    }
}
