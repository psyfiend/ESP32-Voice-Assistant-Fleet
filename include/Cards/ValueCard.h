#pragma once
#ifndef VALUE_CARD_H
#define VALUE_CARD_H

#include "Cards/Card.h"

// ---------------------------------------------------------------------------
// ValueCard - the first of cards.md section 4's two layout families.
//
// Temperature, lux, power, RSSI, free heap. Anything whose point is a NUMBER.
//
//   small tinted icon top-left, name beside it
//   value centred and dominant
//   status row along the bottom, only if there is something to put in it
//
// The single most important rule here is about the NAME, and it is the one a
// reimplementation would get wrong: the label is the LOCATION, not the
// measurement. "Deck", not "Temperature". The tinted icon already says what
// kind of quantity this is, so spending a line on saying it again costs a line
// on every sensor card in the fleet. Card::setLabel() is how a page supplies
// it; falling back to the entity's name is a convenience, not the intent.
//
// THIS IS A LAYOUT, NOT A CARD TYPE, and the distinction is the whole point of
// the rename that produced this file.
//
// docs/design/cards.md - the spec - lists card types by HOME ASSISTANT DOMAIN:
// sensor, binary_sensor, light, action/scene, weather, group. ROADMAP milestone
// 2.7 names the first four the same way. The "two layout families" framing
// exists in exactly one place, HANDOFF.md, as a paraphrase written to brief the
// next session - and that paraphrase got promoted to the class names, which
// skipped the domain layer underneath it entirely.
//
// The result was a user having to know that a temperature reading wants a
// "measure" card. They should pick `sensor`, give it a topic, and never learn
// where the number goes. What the thing IS and how it is ARRANGED are two
// axes, and only the first one belongs to whoever is configuring a dashboard.
//
// So this class deliberately does NOT implement typeName(): it stays abstract,
// and the compiler refuses to let anyone instantiate a layout. Concrete types
// live in CardCatalog.h and are named for their domain.
//
// ---------------------------------------------------------------------------

class ValueCard : public Card {
protected:
    void buildBody(lv_obj_t *body) override;
    void render() override;

private:
    // Two flex rows rather than free-floating labels. Both replaced hand
    // positioning that needed a layout pass which had not run yet - the name
    // wrapped mid-word and the unit landed beside it instead of beside the
    // number. See the .cpp for the specific trap in each case.
    lv_obj_t *_titleRow = nullptr;   // icon + name, one line
    lv_obj_t *_icon     = nullptr;
    lv_obj_t *_name     = nullptr;

    lv_obj_t *_valueRow = nullptr;   // value + unit, bottom-aligned
    lv_obj_t *_value    = nullptr;
    lv_obj_t *_unit     = nullptr;

    // The two bottom corners, independently present or absent.
    lv_obj_t *_battery  = nullptr;   // bottom-left, with its own glyph
    lv_obj_t *_seen     = nullptr;   // bottom-right
};

#endif // VALUE_CARD_H
