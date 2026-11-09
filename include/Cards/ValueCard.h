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
// THIS IS A LAYOUT, NOT A CARD TYPE.
//
// docs/design/cards.md lists card types by HOME ASSISTANT DOMAIN: sensor,
// binary_sensor, light, action/scene, weather, group. A user picks `sensor`,
// gives it a topic, and never learns where the number goes. What the thing IS
// and how it is ARRANGED are two axes, and only the first belongs to whoever
// is configuring a dashboard.
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
    // Words for the status row's right-hand corner in place of "Seen: ...";
    // false (the default) keeps the age.
    virtual bool statusWords(const Entity &e, char *out, size_t cap) const { (void)e; (void)out; (void)cap; return false; }

private:
    // As a cluster's MINI TILE (K89): the value and its unit, its entity
    // label under them. Cards/MiniTile.cpp.
    void buildMiniBody(lv_obj_t *body);
    void renderMini();

    // A vertical stack. Nothing here is positioned against anything else.
    // See the .cpp.
    lv_obj_t *_icon      = nullptr;   // top-left corner, OUT of the flow

    lv_obj_t *_mid       = nullptr;   // grows; centres value+name as a group
    lv_obj_t *_valueRow  = nullptr;   // value + unit, bottom-aligned
    lv_obj_t *_value     = nullptr;
    lv_obj_t *_unit      = nullptr;
    lv_obj_t *_name      = nullptr;   // the LOCATION, under the value

    lv_obj_t *_statusRow = nullptr;   // battery left, seen right. Absent in COMPACT
    lv_obj_t *_battGroup = nullptr;
    lv_obj_t *_battIcon  = nullptr;   // in the ICON face
    lv_obj_t *_battery   = nullptr;   // its percentage, in the TEXT face
    lv_obj_t *_seen      = nullptr;
};

#endif // VALUE_CARD_H
