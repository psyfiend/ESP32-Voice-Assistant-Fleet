#pragma once
#ifndef MEASURE_CARD_H
#define MEASURE_CARD_H

#include "Cards/Card.h"

// ---------------------------------------------------------------------------
// MeasureCard - the first of cards.md section 4's two layout families.
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
// ---------------------------------------------------------------------------

class MeasureCard : public Card {
public:
    const char *typeName() const override { return "measure"; }

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

#endif // MEASURE_CARD_H
