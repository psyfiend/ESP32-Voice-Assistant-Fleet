#include "Cards/CardCatalog.h"

// A switch and a light do the same thing, and since 2.10b it is what an HA
// group does (owner, 2026-10-05): a card that is ON - by its GroupOn rule,
// any member or all of them - turns every member off; one that is off turns
// them all on. It was "the inverse of the majority", which no HA group does.
// One entity is the same rule with one member.
void SwitchCard::onTap() {
    if (!primaryCount()) return;
    commandAll(!groupIsOn());
}

void LightCard::onTap() {
    if (!primaryCount()) return;
    commandAll(!groupIsOn());
}

void ButtonCard::onTap() {
    // No toggle: a button has no state to invert. It fires, and nothing is
    // expected to echo back.
    commandAll(true);
}

Card *cardForKind(EntityKind kind) {
    switch (kind) {
        case EntityKind::SENSOR:        return new SensorCard();
        case EntityKind::BINARY_SENSOR: return new BinarySensorCard();
        case EntityKind::SWITCH:        return new SwitchCard();
        case EntityKind::LIGHT:         return new LightCard();
        case EntityKind::BUTTON:        return new ButtonCard();

        // Real kinds with no card yet. Listed rather than swept into a default
        // so that adding an EntityKind cannot silently inherit someone else's
        // card - the compiler names the one you forgot, which is the same
        // discipline EntityTypes.h uses for HA platform strings.
        case EntityKind::NUMBER:
        case EntityKind::TEXT:
        case EntityKind::CLIMATE:
        case EntityKind::WEATHER:
            return nullptr;
    }
    return nullptr;
}

Card *cardForEntity(const Entity *e) {
    if (!e) return nullptr;
    Card *c = cardForKind(e->desc.kind);
    if (c) c->bindPrimary(e);
    return c;
}
