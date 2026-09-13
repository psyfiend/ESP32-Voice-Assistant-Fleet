#pragma once
#ifndef CARD_CATALOG_H
#define CARD_CATALOG_H

#include "Cards/ValueCard.h"
#include "Cards/StateCard.h"

// ---------------------------------------------------------------------------
// The card types a dashboard can actually ask for, named for what they ARE.
//
// This file is the layer that was missing. `ValueCard` and `StateCard` are two
// arrangements; these are the things a person picks. The split matters because
// only one of the two is anybody's business but ours:
//
//   a user says            "this is a sensor, here is its topic"
//   the framework decides  "a sensor's hero is a number, so: value layout"
//
// docs/design/cards.md section 4 lists card types by Home Assistant domain and
// ROADMAP milestone 2.7 names the first four the same way. The layout names
// were never the spec's - they came from a paraphrase in HANDOFF.md - and
// having them as the public surface meant a build sheet would have had to
// record a layout decision that is not the user's to make.
//
// ALL FOUR ARE THIN ON PURPOSE. A domain type answers three questions and
// nothing else: which layout, what a tap does, and what it is called. If one
// of these ever grows real logic, that is a sign the logic belongs in the
// layout it derives from, where every domain sharing that layout gets it.
//
// Deliberately ONE header and ONE .cpp rather than a file pair per type. Four
// twenty-line classes do not need eight files, and the file count of the card
// layer is already something to keep an eye on.
// ---------------------------------------------------------------------------

// A read-only number. Temperature, illuminance, power, RSSI, free heap.
// The hero is the reading; the name beside it is the LOCATION, not the
// quantity, because the tinted icon already says what is being measured.
class SensorCard : public ValueCard {
public:
    const char *typeName() const override { return "sensor"; }
};

// A read-only bit. Motion, occupancy, a door, a window.
//
// It uses the state layout rather than the value one because one bit is not a
// number - cards.md section 4: placed as its own card, a binary sensor
// "behaves as a state card... exactly like a non-dimmable light". It looks
// identical to a switch and does NOTHING when tapped, which is the entire
// difference between the two and the reason tap handling never belonged on the
// shared layout.
class BinarySensorCard : public StateCard {
public:
    const char *typeName() const override { return "binary_sensor"; }
};

// A writable on/off. One tap flips it.
//
// Bound to several entities it becomes the aggregate - every light in a room,
// one tap for all of them - which is why the target state is the inverse of the
// MAJORITY rather than of each entity individually. A room with two on and one
// off should go fully off on the first tap, not swap which one is lit.
class SwitchCard : public StateCard {
public:
    const char *typeName() const override { return "switch"; }
protected:
    void onTap() override;
};

// A light. Identical to a switch today.
//
// It is a separate type anyway rather than an alias, because brightness and
// colour are its own (cards.md section 4: "where the light reports colour or
// colour temperature, the card mirrors it"), and the day that lands it should
// be a change to this class rather than a new branch inside SwitchCard.
class LightCard : public StateCard {
public:
    const char *typeName() const override { return "light"; }
protected:
    void onTap() override;
};

// Stateless and momentary: the press is the whole payload.
//
// It has no state to reflect, so a tap commands `true` and nothing is expected
// to come back. That also means it can never show FAILED - there is no echo to
// wait for and no previous value to revert to.
class ButtonCard : public StateCard {
public:
    const char *typeName() const override { return "button"; }
protected:
    void onTap() override;
};

// ---------------------------------------------------------------------------
// The factory.
//
// One place that maps a Home Assistant domain onto a class, so that a build
// sheet (#20) writes `type: sensor` and never names a C++ type or a layout.
// Every caller that creates a card should come through here.
//
// Returns nullptr for a kind with no card yet - NUMBER, TEXT, CLIMATE and
// WEATHER are all real EntityKinds with no implementation, and a null is a
// more honest answer than quietly rendering one of them as something else.
// ---------------------------------------------------------------------------
Card *cardForKind(EntityKind kind);

// Convenience: build the right card for an entity and bind it as the primary.
// The overwhelmingly common case, and it keeps the kind lookup out of page code.
Card *cardForEntity(const Entity *e);

#endif // CARD_CATALOG_H
