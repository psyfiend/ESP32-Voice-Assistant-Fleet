#pragma once
#ifndef HA_DEFAULTS_H
#define HA_DEFAULTS_H

#include <stdint.h>

// ---------------------------------------------------------------------------
// Compile-time defaults for the Home Assistant websocket session.
//
// Same shape as MqttDefaults.h: one struct of named constants with the
// reasoning next to each value, rather than numbers scattered through the
// implementation. Phase 4's runtime config overrides these; until then this is
// the single place to change them.
//
// Every number that came from a measurement says so. The measurements are in
// docs/design/ha-websocket.md, taken against the owner's live instance
// (HA 2026.8.1, 1,662 entities, 152 devices) on 2026-09-15/16.
// ---------------------------------------------------------------------------

struct HaDefaultsT {
    // --- Where ------------------------------------------------------------
    // Plain ws:// on the LAN, no TLS. The owner's instance, and the one every
    // number in ha-websocket.md was measured against.
    const char *HOST;
    uint16_t    PORT;
    const char *PATH;

    // --- Reassembly -------------------------------------------------------
    //
    // THE BUFFER IS SIZED BY THE BIGGEST SINGLE MESSAGE WE CHOOSE TO ACCEPT,
    // NOT BY THE BIGGEST HA CAN SEND.
    //
    // HA can send 787 KB for get_states and 1.36 MB for the full entity
    // registry. Neither is reachable on this hardware and neither is in the
    // design: ha-websocket.md section 7 fetches per-entity forms (842 B) and
    // the area registry whole (3.6 KB) instead, precisely so this buffer can
    // stay small. The device registry is the one large read (120 KB) and it is
    // streamed, never buffered.
    //
    // 8 KB covers the area registry whole, any per-entity reply, and a trigger
    // event (1,365 B) with room to spare. A message larger than this is a
    // design violation somewhere else, so it is reported as STOP_OVERSIZE
    // rather than silently truncated - a truncated JSON parse fails in ways
    // that look like a server bug.
    uint32_t RX_BUFFER_BYTES;

    // esp_websocket_client's own per-read buffer. Independent of the above:
    // this is how much it hands us per WEBSOCKET_EVENT_DATA, and anything
    // larger arrives as several fragments that we reassemble.
    uint32_t WS_RX_CHUNK_BYTES;

    // --- The websocket task ----------------------------------------------
    //
    // esp_websocket_client runs its own FreeRTOS task and calls our handler on
    // it. That handler parses JSON and writes to EntityRegistry, so the stack
    // has to cover ArduinoJson's working set, not just the client's.
    //
    // THE TASK IS UNPINNED, AND THERE IS NO KNOB HERE TO CHANGE THAT.
    // esp_websocket_client.c line 1403 calls xTaskCreate, not
    // xTaskCreatePinnedToCore, and the config struct exposes only task_prio,
    // task_stack and task_name. Checked in the vendored source rather than
    // assumed, because the obvious thing to write here was "core 0, beside the
    // TCP/IP task", and that would have been a comment asserting something the
    // code does not do.
    //
    // What this means in practice: FreeRTOS may schedule it on core 1, beside
    // loopTask and LVGL, and at priority 5 against loopTask's 1 it will preempt
    // the renderer while it parses. That is acceptable and is still strictly
    // better than the MQTT path, where a synchronous connect blocks the loop
    // task outright. It is NOT a correctness problem: the handler never touches
    // LVGL, and EntityRegistry carries its own mutex.
    //
    // Pinning it would mean patching the vendored component, which costs the
    // "unmodified upstream" property that makes it updatable. Not worth it
    // unless the UI is measurably stuttering - and if that day comes, measure
    // first, because #49 spent a week being blamed on core affinity that turned
    // out to be innocent.
    uint32_t TASK_STACK_BYTES;
    int      TASK_PRIORITY;

    // --- Timing -----------------------------------------------------------
    //
    // The handshake is three messages and measured in milliseconds, so a
    // handshake still unfinished after this long is not slow, it is wedged.
    uint32_t HANDSHAKE_TIMEOUT_MS;

    // Transport-level network timeout handed to esp_websocket_client.
    uint32_t NETWORK_TIMEOUT_MS;

    // Reconnect backoff. Deliberately NOT esp_websocket_client's own
    // auto-reconnect: this client has to stand down entirely while
    // Fleet_Connectivity reports the link dead, and come back when it
    // recovers. A library retrying underneath that would fight it. See
    // disable_auto_reconnect in HaClient.cpp.
    uint32_t BACKOFF_MIN_MS;
    uint32_t BACKOFF_MAX_MS;

    // Keepalive. HA does not require application-level pings, but a silent
    // socket on a wall panel is indistinguishable from a dead one, and #49 is
    // the whole reason this project no longer trusts "no news is good news".
    uint32_t PING_INTERVAL_MS;
    uint32_t PONG_TIMEOUT_MS;
};

inline const HaDefaultsT &haDefaults() {
    static const HaDefaultsT d = {
        .HOST                 = "192.168.0.70",
        .PORT                 = 8123,
        .PATH                 = "/api/websocket",

        .RX_BUFFER_BYTES      = 8192,
        .WS_RX_CHUNK_BYTES    = 2048,

        .TASK_STACK_BYTES     = 6144,
        .TASK_PRIORITY        = 5,

        .HANDSHAKE_TIMEOUT_MS = 8000,
        .NETWORK_TIMEOUT_MS   = 6000,

        .BACKOFF_MIN_MS       = 2000,
        .BACKOFF_MAX_MS       = 60000,

        .PING_INTERVAL_MS     = 20000,
        .PONG_TIMEOUT_MS      = 10000,
    };
    return d;
}

#endif // HA_DEFAULTS_H
