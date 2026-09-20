#pragma once
#ifndef HA_REST_H
#define HA_REST_H

#include <stdint.h>
#include <stddef.h>

class EntityRegistry;

// ---------------------------------------------------------------------------
// Initial values for HA-sourced entities, over REST.
//
// WHY REST AND NOT THE WEBSOCKET WE ALREADY HAVE.
//
// Because HA's websocket API has no per-entity state call. `get_states` is
// all-or-nothing and on the owner's instance that is 1,662 entities and
// 787,728 bytes - not reachable on this hardware and not worth reaching for.
// REST's `GET /api/states/<entity_id>` returns ONE entity in 399-843 bytes in
// 4-25 ms, measured. That gap is the entire reason both transports exist here;
// it is not a philosophical split. See docs/design/ha-websocket.md section 7.
//
// WHY THIS IS NEEDED AT ALL. subscribe_trigger fires on CHANGE. Without a
// one-shot fetch, a light that nobody touches and a thermostat holding steady
// render blank forever - the dashboard would come up empty and slowly populate
// over hours as things happened to move.
//
//
// IT BLOCKS, AND IT IS SPREAD OUT BECAUSE OF THAT.
//
// esp_http_client is synchronous. Eighteen sequential GETs on the loop task is
// roughly 450 ms of frozen UI, which is exactly the shape of bug that made
// PubSubClient's connect() freeze the panel (#49's neighbour). So this fetches
// AT MOST ONE ENTITY PER loop() PASS: the stall is one request, ~25 ms on the
// measured numbers, and it happens once per session rather than continuously.
//
// The timeout is deliberately short for the same reason. An HA that is
// unreachable must cost one short pause and then stop being asked, not stall
// the renderer while TCP works through its own retries.
//
// Runs entirely on the LOOP TASK, unlike HaClient's receive path. Nothing here
// is called from the websocket task.
// ---------------------------------------------------------------------------

class HaRest {
public:
    void begin(EntityRegistry *reg);

    // Start a fresh pass over every HA-sourced entity. Called when a websocket
    // session comes up - including after a reconnect, because a reconnect is a
    // cold start and the values may have moved while we were away.
    void restart();

    // Fetches at most one entity, then returns. Call every loop().
    void loop(uint32_t nowMs);

    bool     isComplete()  const { return _done; }
    uint16_t fetched()     const { return _fetched; }
    uint16_t failed()      const { return _failed; }

private:
    // Returns true if the value was parsed and written.
    bool fetchOne(const char *entityRef, const char *ourId);

    EntityRegistry *_reg = nullptr;

    uint8_t  _cursor  = 0;      // index into the registry
    bool     _done    = true;   // nothing to do until restart()
    uint32_t _nextAtMs = 0;     // small gap between requests

    uint16_t _fetched = 0;
    uint16_t _failed  = 0;
};

#endif // HA_REST_H
