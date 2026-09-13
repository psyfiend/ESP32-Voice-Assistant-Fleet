#include "Cards/CardCatalog.h"

// A switch and a light do the same thing today, and both do it by asking every
// bound entity for the inverse of what the majority currently reports.
static bool wantOnFor(uint8_t active, uint8_t total) {
    return active * 2 <= total;
}

void SwitchCard::onTap() {
    const uint8_t total = primaryCount();
    if (!total) return;
    commandAll(wantOnFor(activeCount(), total));
}

void LightCard::onTap() {
    const uint8_t total = primaryCount();
    if (!total) return;
    commandAll(wantOnFor(activeCount(), total));
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
