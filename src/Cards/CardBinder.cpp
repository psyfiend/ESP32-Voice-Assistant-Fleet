#include "Cards/CardBinder.h"
#include "Cards/CardGroups.h"
#include <Arduino.h>
#include <string.h>

void CardBinder::begin(EntityRegistry *reg) {
    _reg = reg;

    // Commands go back the same way values came. Set here rather than per card
    // so a card can be constructed before anyone decides which registry it
    // belongs to - see Card::useRegistry().
    Card::useRegistry(reg);

    if (!_timer) {
        _timer = lv_timer_create(timerCb, CARD_BINDER_TICK_MS, this);
    }
    Serial.printf("[Cards] Binder running at %u ms\n", (unsigned)CARD_BINDER_TICK_MS);
}

bool CardBinder::add(Card *c) {
    if (!c || _n >= CARD_BINDER_MAX) return false;
    for (uint8_t i = 0; i < _n; i++) if (_cards[i] == c) return true;
    _cards[_n++] = c;
    return true;
}

void CardBinder::remove(Card *c) {
    for (uint8_t i = 0; i < _n; i++) {
        if (_cards[i] != c) continue;
        _cards[i] = _cards[--_n];   // order is not meaningful here; placement
        _cards[_n] = nullptr;       // order lives on the page, not the binder
        return;
    }
}

void CardBinder::restyleAll() {
    for (uint8_t i = 0; i < _n; i++) if (_cards[i]) _cards[i]->restyle();
}

void CardBinder::debugForceAll(CardState s, bool force) {
    for (uint8_t i = 0; i < _n; i++) if (_cards[i]) _cards[i]->debugForceState(s, force);
    Serial.printf("[Cards] forced state: %s\n", force ? cardStateName(s) : "released");
}

void CardBinder::timerCb(lv_timer_t *t) {
    CardBinder *self = (CardBinder *)lv_timer_get_user_data(t);
    if (self) self->tick();
}

// Routing is a linear scan of cards per dirty entity, and an id strcmp per
// card. With eight entities and a page of cards that is a few hundred byte
// comparisons every 100 ms - far below anything worth indexing for, and an
// index would have to be rebuilt whenever a page is built or torn down.
//
// It scans by ID rather than by registry index because a snapshot is a COPY
// and carries no index back. That is not a shortcoming of drainDirty(); it is
// what makes the callback safe to run outside the lock.
void CardBinder::dirtyCb(const Entity &snapshot, void *ctx) {
    CardBinder *self = (CardBinder *)ctx;
    if (!self) return;
    for (uint8_t i = 0; i < self->_n; i++) {
        Card *c = self->_cards[i];
        if (c && c->owns(snapshot.desc.id)) c->onSnapshot(snapshot);
    }
}

void CardBinder::tick() {
    if (!_reg) return;

    // Values first, then state. Order matters: a snapshot that arrives in this
    // same tick is what clears a card's staleness, and doing it the other way
    // round would flash a STALE tag for one frame on every value that lands
    // late.
    _reg->drainDirty(dirtyCb, this);

    const uint32_t now = millis();
    for (uint8_t i = 0; i < _n; i++) {
        if (_cards[i]) _cards[i]->pollState(now);
    }

    // EVERY CARD'S GROUP AGAIN WHEN HA'S AREAS CHANGE (2.10d). A group's colour
    // depends on every group's (K47: who has which colour first), so the area
    // of an entity on one card can recolour cards on others. A card re-applied
    // only its own on its own entity's area - and Kitchen's four cards kept
    // Garage's green on the P4_5 (2026-10-11) when Garage's area arrived after
    // theirs. The areas arrive together in one answer: applied once they have
    // been still for 300 ms, with the colours worked out afresh.
    const uint32_t ep = _reg->areaEpoch();
    if (ep != _areaEpoch) { _areaEpoch = ep; _areaMs = now; _areaPending = true; }
    else if (_areaPending && now - _areaMs >= 300) {
        _areaPending = false;
        CardGroups::forgetColours();
        for (uint8_t i = 0; i < _n; i++) if (_cards[i]) _cards[i]->reapplyGroup();
    }
}
