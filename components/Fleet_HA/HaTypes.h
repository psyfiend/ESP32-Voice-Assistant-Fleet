#pragma once
#ifndef HA_TYPES_H
#define HA_TYPES_H

#include <stdint.h>

// ---------------------------------------------------------------------------
// Home Assistant websocket session state.
//
// EVERY ENUMERATOR IS A COMPOUND NAME ON PURPOSE. esp32-hal-gpio.h defines
// bare ALL-CAPS macros - DISABLED, CHANGE, HIGH, LOW, OPEN_DRAIN and friends -
// and a macro applies inside a scoped enum class too, where a same-named
// enumerator ought to be perfectly safe. `MqttState::DISABLED` was silently
// rewritten to `MqttState::0x00` and the resulting error cascade pointed at
// esp32-hal-gpio.h and at the USES of the enum, never at the declaration.
// See CLAUDE.md, "Arduino's global macro namespace will eat your enum".
//
// So: no single-word enumerator here, ever. The prefix costs nothing.
// ---------------------------------------------------------------------------
enum class HaLink : uint8_t {
    SESSION_OFF = 0,  // never started, or deliberately stopped
    LINK_IDLE,        // started, waiting for the network to come up
    LINK_OPENING,     // websocket handshake in flight
    LINK_AUTHING,     // server sent auth_required; our token is on the wire
    LINK_READY,       // auth_ok - the session is usable
    LINK_BACKOFF,     // dropped or refused; waiting before the next attempt
    LINK_REFUSED,     // auth_invalid. A bad token is not worth retrying.
};

const char *haLinkName(HaLink s);

// Why the last session ended. Kept separate from HaLink because the state
// says where we ARE and this says how we got here - the same split that made
// ConnectivityManager's "last reason" readable in a dump.
enum class HaStop : uint8_t {
    STOP_NONE = 0,
    STOP_NET_DOWN,      // Fleet_Connectivity says we are offline
    STOP_TRANSPORT,     // socket error or unexpected close
    STOP_AUTH_INVALID,  // HA rejected the token
    STOP_TIMEOUT,       // handshake started and never completed
    STOP_OVERSIZE,      // a message exceeded the reassembly buffer
};

const char *haStopName(HaStop s);

#endif // HA_TYPES_H
