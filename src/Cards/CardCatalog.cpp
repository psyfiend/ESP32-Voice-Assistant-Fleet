#include "Cards/CardCatalog.h"
#include "Cards/CardIcons.h"   // cardStateWord(): a summary's "2 open"
#include <ctype.h>
#include <stdio.h>
#include <string.h>

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

void SummaryCard::setMemberName(const char *entityId, const char *name) {
    if (!entityId || !name || !name[0] || _nNames >= NAMES_MAX) return;
    _ids[_nNames]   = entityId;
    _names[_nNames] = name;
    _nNames++;
}

// THE WORDS NAME WHAT IS ACTIVE (K98, the owner's "North", "South", "Both"):
// one member - its name; every member - "Both", or "All" for more than two;
// some - how many, in the members' own word ("2 open"); none - the card's
// name. A paused member does not count (Card::counts()). State keeps its word.
const char *SummaryCard::nameText(CardLabel lbl, const Entity &e, bool isOn) {
    if (lbl == CardLabel::LBL_STATE) return StateCard::nameText(lbl, e, isOn);
    uint8_t total = 0, on = 0;
    const Entity *one = nullptr;
    for (uint8_t i = 0; i < primaryCount(); i++) {
        const Entity *m = primary(i);
        if (!m || !counts(m)) continue;
        total++;
        if (m->value.type == ValueType::BOOL && m->value.b) { on++; one = m; }
    }
    if (!on) return shownName(lbl);
    if (on == 1) {
        for (uint8_t k = 0; k < _nNames; k++)
            if (!strcmp(_ids[k], one->desc.id)) return _names[k];
        return one->desc.name;
    }
    if (on == total) return total == 2 ? "Both" : "All";
    snprintf(_words, sizeof(_words), "%u %s", (unsigned)on, cardStateWord(one->desc, true));
    for (char *p = _words; *p; p++) *p = (char)tolower((unsigned char)*p);   // "2 open"
    return _words;
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
