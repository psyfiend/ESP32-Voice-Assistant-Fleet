#pragma once
#ifndef ACTOR_CARD_H
#define ACTOR_CARD_H

#include "Cards/Card.h"

// ---------------------------------------------------------------------------
// ActorCard - the second of cards.md section 4's two layout families.
//
// Lights, switches, doors, motion. Anything whose point is a STATE.
//
//   big icon in a disc, centred
//   name below it
//   the whole surface takes the state colour when active
//
// Three rules from cards.md that are easy to lose and expensive to re-derive:
//
//   1. NO STATE WORDS. No "On", no "Off", no "Open", no "Closed", anywhere.
//      The icon and its colour are the state. This is not a style preference;
//      it is what makes a wall of these readable at a glance from across a
//      room, which is the whole job.
//   2. The colour is across the WHOLE SURFACE, not in a corner. The rejected
//      accent rail survives only where it IS the state indicator.
//   3. Mixed state gets its OWN indicator. A group of lights that disagrees
//      does not pick a side and lie about it.
//
// The aggregate case is why Card binds up to six primaries. An ActorCard bound
// to one switch and an ActorCard bound to every light in a room are the same
// class with the same layout - the second just derives its one state from all
// of them, and one tap commands all of them. That is cards.md section 4's
// "groupable by room", and it is deliberately NOT the group card.
// ---------------------------------------------------------------------------

// How an active state is shown. cards.md section 4 specifies FILL - "the card
// reflects state across its whole surface, not in a corner" - but the owner
// recalled an earlier option worth comparing on glass rather than on paper,
// where only the icon and name light up and the card keeps its surface. Both
// are built so the choice is made by looking.
enum class ActorStateStyle : uint8_t {
    FILL_SURFACE = 0,   // cards.md section 4 as written
    LIGHT_ICON,         // icon and name take the state colour; surface does not
};

class ActorCard : public Card {
public:
    const char *typeName() const override { return "actor"; }

    void setStateStyle(ActorStateStyle s) { _style = s; if (root()) render(); }

protected:
    void buildBody(lv_obj_t *body) override;
    void render() override;
    void onTap() override;

private:
    // How many bound primaries are currently on. Returns the count rather than
    // a bool so the caller can tell all / none / some apart in one read, which
    // is what rule 3 needs.
    uint8_t activeCount() const;

    ActorStateStyle _style = ActorStateStyle::FILL_SURFACE;

    lv_obj_t *_disc  = nullptr;
    lv_obj_t *_icon  = nullptr;
    lv_obj_t *_name  = nullptr;
    lv_obj_t *_mixed = nullptr;   // the not-uniform badge. Hidden when uniform
};

#endif // ACTOR_CARD_H
