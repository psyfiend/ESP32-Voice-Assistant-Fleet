#pragma once
#ifndef HA_CLIENT_H
#define HA_CLIENT_H

#include <stdint.h>
#include <stddef.h>
#include <atomic>

#include "HaTypes.h"
#include "HaDefaults.h"

class ConnectivityManager;
class EntityRegistry;

// Opaque - so this header does not drag esp_websocket_client.h (and through it
// the whole esp_transport chain) into every translation unit that merely wants
// to ask whether HA is connected.
struct esp_websocket_client;
typedef struct esp_websocket_client *esp_websocket_client_handle_t;

// ---------------------------------------------------------------------------
// The Home Assistant websocket session.
//
// Owns the SOCKET and the SESSION, and nothing above that: connect, the three
// message auth handshake, request ids, and handing complete JSON messages to
// whoever registered for them. It does not know what an area is, does not know
// which entities we care about, and never touches a card. That split is the
// same one Fleet_MQTT draws - see ROADMAP Q9.
//
//
// THIS IS THE PROJECT'S FIRST GENUINELY ASYNCHRONOUS PROVIDER. READ THIS.
//
// Every data path before this one ran on the loop task. PubSubClient is
// synchronous and is pumped from loop(); the local sensor providers are polled
// from loop(). esp_websocket_client is not: it runs its own FreeRTOS task and
// calls our handler on THAT task, at whatever moment a frame arrives.
//
// Two rules follow, and neither is optional:
//
//   1. THE HANDLER MUST NOT TOUCH LVGL. Not a label, not a style, not a
//      lv_timer. LVGL here is single threaded (LV_USE_OS == LV_OS_NONE) and
//      its thread is the loop task. This is the same rule CLAUDE.md already
//      states for providers, except that until now no provider could actually
//      break it. Now one can.
//
//   2. WRITE THROUGH EntityRegistry, WHICH HAS ITS OWN std::mutex. setValue()
//      takes a short lock, writes, and marks dirty; the loop task picks the
//      change up via drainDirty() on its own schedule. That is the whole
//      handoff, and it is why no display mutex is needed.
//
// What the handler MAY do: parse, write entities, update counters, set the
// small std::atomic members below. Anything that needs the loop task is
// deferred by setting a flag here and acting on it in loop().
//
// Sending is deliberately done from loop(), never from the event handler, even
// though esp_websocket_client permits it. Re-entering the client from inside
// its own callback is a class of deadlock this project does not need to find
// out about, and the cost is at most one loop iteration of latency.
// ---------------------------------------------------------------------------
class HaClient {
public:
    // A complete, reassembled text message from HA. Called ON THE WEBSOCKET
    // TASK - see the rules above. `json` is NUL-terminated and valid only for
    // the duration of the call.
    typedef void (*MessageFn)(const char *json, size_t len, void *ctx);

    bool begin(ConnectivityManager *conn, EntityRegistry *reg);

    // Pumped from the loop task. Drives connect/backoff, sends anything the
    // handler deferred, and enforces the handshake timeout.
    void loop(uint32_t nowMs);

    // Stop the session and stay stopped until begin() is called again.
    void stop(HaStop why);

    void setMessageHandler(MessageFn fn, void *ctx) { _msgFn = fn; _msgCtx = ctx; }

    // --- State, safe to call from either task ----------------------------
    HaLink state()   const { return (HaLink)_state.load(); }
    HaStop lastStop() const { return (HaStop)_stop.load(); }
    bool   isReady() const { return (HaLink)_state.load() == HaLink::LINK_READY; }

    // The next request id. HA replies carry the id back, which is how a reply
    // is matched to its request.
    //
    // IT MUST STRICTLY INCREASE WITHIN A CONNECTION, AND THAT IS A SERVER RULE,
    // NOT A CONVENTION. Measured against HA 2026.9.2 on 2026-09-20: sending id
    // 5 then id 3 returns
    //
    //     {"success":false,"error":{"code":"id_reuse",
    //      "message":"Identifier values have to increase."}}
    //
    // and id 6 afterwards succeeds. So a scheme that allocates ids from a pool,
    // or recycles them when a request completes, silently breaks - and breaks as
    // a per-request error rather than a dropped connection, which is the kind of
    // failure that gets blamed on the network.
    //
    // A counter that never resets is therefore correct by construction. Resetting
    // it on reconnect would ALSO be legal (the id space is per-connection -
    // measured: a fresh socket accepted id 2 again after the previous one had
    // used it), but there is no reason to, and not resetting is one less rule to
    // remember.
    uint32_t nextId() { return ++_reqId; }

    // --- Diagnostics, for SystemReport -----------------------------------
    uint32_t messagesRx()  const { return _msgsRx.load(); }
    uint32_t bytesRx()     const { return _bytesRx.load(); }
    uint32_t sessions()    const { return _sessions.load(); }
    uint32_t transportErrors() const { return _transportErrs.load(); }
    uint32_t oversized()   const { return _oversized.load(); }
    // How long the current session has been up, or 0 when not LINK_READY.
    uint32_t readyForMs(uint32_t nowMs) const;

private:
    // esp_websocket_client's C callback. Trampolines to onWsEvent().
    static void wsEventTrampoline(void *handlerArg, const char *base,
                                  int32_t eventId, void *eventData);
    void onWsEvent(int32_t eventId, void *eventData);

    // Fragment reassembly. Returns true when a complete message is in _rx.
    bool accumulate(const char *data, int len, int payloadOffset, int payloadLen);

    // Classify a complete message. Auth handshake is handled here; everything
    // else is forwarded to _msgFn.
    void dispatch(const char *json, size_t len);

    void setState(HaLink s);
    void enterBackoff(HaStop why, uint32_t nowMs);
    bool openSocket(uint32_t nowMs);
    void closeSocket();

    ConnectivityManager *_conn = nullptr;
    EntityRegistry      *_reg  = nullptr;

    esp_websocket_client_handle_t _ws = nullptr;

    MessageFn _msgFn  = nullptr;
    void     *_msgCtx = nullptr;

    // Reassembly buffer, heap-allocated once in begin() so the cost is paid
    // at startup and shows up in the boot report rather than appearing as a
    // mysterious fragmentation later.
    char    *_rx     = nullptr;
    uint32_t _rxCap  = 0;
    uint32_t _rxLen  = 0;
    bool     _rxOverflowed = false;   // drop the rest of an oversized message

    // Written on the websocket task, read on the loop task.
    std::atomic<uint8_t>  _state{(uint8_t)HaLink::SESSION_OFF};
    std::atomic<uint8_t>  _stop{(uint8_t)HaStop::STOP_NONE};
    std::atomic<bool>     _authWanted{false};   // handler saw auth_required
    std::atomic<uint32_t> _msgsRx{0};
    std::atomic<uint32_t> _bytesRx{0};
    std::atomic<uint32_t> _sessions{0};
    std::atomic<uint32_t> _transportErrs{0};
    std::atomic<uint32_t> _oversized{0};

    uint32_t _reqId        = 0;
    uint32_t _stateSinceMs = 0;
    uint32_t _retryAtMs    = 0;
    uint32_t _backoffMs    = 0;
    uint32_t _attempt      = 0;
};

#endif // HA_CLIENT_H
