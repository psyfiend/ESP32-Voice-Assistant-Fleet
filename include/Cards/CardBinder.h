#pragma once
#ifndef CARD_BINDER_H
#define CARD_BINDER_H

#include <lvgl.h>
#include "EntityRegistry.h"
#include "Cards/Card.h"

// ---------------------------------------------------------------------------
// CardBinder - the one place the registry meets the UI.
//
// This is the missing half of a pipe that has been sitting unconnected since
// milestone 1.7: EntityRegistry::drainDirty() has existed, fully documented,
// since the registry was written, and until this file NOTHING IN THE TREE
// CALLED IT. Every entity on the device has been updating a value that no
// widget was watching.
//
// The contract it satisfies is EntityRegistry.h's, verbatim:
//
//   Providers run on their own tasks, call setValue(), and never touch LVGL.
//   The LVGL task calls drainDirty() from an lv_timer, roughly every 100 ms.
//   That is the ONLY place bound widgets are updated.
//
// So there is exactly one of these, it owns exactly one lv_timer, and it is
// the only caller of drainDirty() in the application. Pages come and go; the
// binder does not, which is why it is separate from CardPage rather than part
// of it - milestone 2.6's tileview will have several pages sharing this one
// pump, and building them apart now costs nothing.
//
// Why 100 ms and not every loop(): EntityValue::equals() already suppresses
// unchanged values, so a topic firing 50 times a second produces no redraws at
// all. What the timer adds is a ceiling on the OTHER direction - a value that
// genuinely changes every few milliseconds redraws at most 10 times a second.
// That is ROADMAP 4.2's rate limiting, and between the two mechanisms it costs
// no bookkeeping whatsoever.
// ---------------------------------------------------------------------------

static constexpr uint8_t  CARD_BINDER_MAX    = 64;
static constexpr uint32_t CARD_BINDER_TICK_MS = 100;

class CardBinder {
public:
    // Takes the registry every card commands through and every value arrives
    // from. Starts the timer. Safe to call once.
    void begin(EntityRegistry *reg);

    // Cards register themselves here, not with a page. A card that is on no
    // page still gets values, which is what a future off-screen tileview page
    // needs - and is why add() takes a Card and not a CardPage.
    bool add(Card *c);
    void remove(Card *c);

    // Restyle everything. Registered with UI::onSchemeChanged() by whoever
    // owns the dashboard, so switching scheme repaints live cards.
    void restyleAll();

    // Testing only: pin every registered card to one state, or release them
    // all. See Card::debugForceState() for why this exists.
    void debugForceAll(CardState s, bool force = true);

    uint8_t count() const { return _n; }

private:
    static void timerCb(lv_timer_t *t);
    static void dirtyCb(const Entity &snapshot, void *ctx);
    void tick();

    EntityRegistry *_reg = nullptr;
    lv_timer_t     *_timer = nullptr;
    Card           *_cards[CARD_BINDER_MAX] = {nullptr};
    uint8_t         _n = 0;
};

#endif // CARD_BINDER_H
