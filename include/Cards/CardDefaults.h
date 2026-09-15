#pragma once
#ifndef CARD_DEFAULTS_H
#define CARD_DEFAULTS_H

#include "Entity.h"

// ---------------------------------------------------------------------------
// The second staleness threshold, per data type.
//
// cards.md section 9, question 1, answered: it is its OWN field, not a
// multiple of the first. The reasoning is that the two thresholds answer
// different questions - "is this still trustworthy" and "is this obviously
// dead" - and a switch and a thermometer disagree about both independently.
//
// The first threshold is already in the registry as desc.staleAfterMs, set by
// whoever declared the entity. This file supplies only the second, and only as
// a DEFAULT: cards.md says any default is overridable from the build sheet
// (#20), which is exactly the overlay model issue #20 describes. When the build
// sheet lands it sets Card::setLongStaleMs() and this table stops being
// consulted for that card.
//
// A header of plain inline functions rather than a table, so adding an
// EntityKind cannot silently inherit someone else's timing - the switch below
// has no default case and the compiler will name the kind you forgot.
// ---------------------------------------------------------------------------

// Commandable things: we told it to be in a state, so silence is suspicious
// fast. Thirty seconds is already a long time to wait for a light to admit
// what it is doing.
static constexpr uint32_t CARD_LONG_STALE_COMMANDABLE_MS = 30000;

// Measurements: a Zigbee sensor reports on its own schedule and can be quiet
// for a long time without anything being wrong. An hour of total silence from
// a device that normally speaks every few minutes is not ambiguous.
static constexpr uint32_t CARD_LONG_STALE_MEASURED_MS = 3600000;

inline uint32_t cardLongStaleMs(const EntityDescriptor &d) {
    uint32_t lng = CARD_LONG_STALE_MEASURED_MS;

    switch (d.kind) {
        case EntityKind::SWITCH:
        case EntityKind::LIGHT:
        case EntityKind::BUTTON:
        case EntityKind::NUMBER:
        case EntityKind::TEXT:
            lng = CARD_LONG_STALE_COMMANDABLE_MS;
            break;
        case EntityKind::SENSOR:
        case EntityKind::BINARY_SENSOR:
        case EntityKind::CLIMATE:
        case EntityKind::WEATHER:
            lng = CARD_LONG_STALE_MEASURED_MS;
            break;
    }

    // The two thresholds are independent fields, which means nothing stops an
    // entity declaring a first threshold LONGER than this default - the deck
    // motion sensor's 30 minutes against a commandable 30 seconds would do it
    // if someone changed its kind. Without this guard such a card would skip
    // the quiet STALE tag entirely and go straight to the loud treatment,
    // which is the one failure mode that would teach the owner to distrust the
    // loud treatment.
    if (d.staleAfterMs && lng <= d.staleAfterMs) lng = d.staleAfterMs * 2;

    return lng;
}

#endif // CARD_DEFAULTS_H
