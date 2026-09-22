#pragma once
#ifndef STATE_CARD_H
#define STATE_CARD_H

#include "Cards/Card.h"

// ---------------------------------------------------------------------------
// StateCard - the second of cards.md section 4's two layout families.
//
// Lights, switches, doors, motion. Anything whose point is a STATE.
//
//   big icon in a disc, centred
//   name below it
//   the whole surface takes the state colour when active
//
// Three rules from cards.md that are easy to lose and expensive to re-derive:
//
//   1. NO STATE WORDS BY DEFAULT. The icon and its colour are the state; that
//      is what makes a wall of these readable at a glance from across a room.
//      Since 2.7 a user may ASK for the word in place of the name (CardLabel,
//      cards.md section 13) - it is a choice that defaults to off, not a rule
//      that was dropped. The word is the device_class table's "Open" or
//      "Detected", never a raw "on" off the wire.
//   2. The colour is across the WHOLE SURFACE, not in a corner. The rejected
//      accent rail survives only where it IS the state indicator.
//   3. Mixed state gets its OWN indicator. A group of lights that disagrees
//      does not pick a side and lie about it.
//
// The aggregate case is why Card binds up to six primaries. An StateCard bound
// to one switch and an StateCard bound to every light in a room are the same
// class with the same layout - the second just derives its one state from all
// of them, and one tap commands all of them. That is cards.md section 4's
// "groupable by room", and it is deliberately NOT the group card.
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

// How an active state is shown. cards.md section 4 specifies FILL - "the card
// reflects state across its whole surface, not in a corner" - but the owner
// recalled an earlier option worth comparing on glass rather than on paper,
// where only the icon and name light up and the card keeps its surface. Both
// are built so the choice is made by looking.
enum class StateCardFill : uint8_t {
    FILL_SURFACE = 0,   // cards.md section 4 as written
    LIGHT_ICON,         // icon and name take the state colour; surface does not
};

class StateCard : public Card {
public:
    // PAGE-WIDE, deliberately static. It is a comparison knob while both
    // treatments are being judged on glass, not a property of an individual
    // card - and making it static means page code never has to downcast a Card
    // to reach it. When the owner picks a winner this whole enum goes away and
    // the loser's branch in render() goes with it.
    static void setFill(StateCardFill f) { s_fill = f; }
    static StateCardFill fill() { return s_fill; }

protected:
    void buildBody(lv_obj_t *body) override;
    void render() override;

    // How many bound primaries are currently on. Returns the count rather than
    // a bool so a caller can tell all / none / some apart in one read, which is
    // what the mixed-state indicator needs.
    //
    // Protected rather than private because the domain types built on this
    // layout need it to decide what a tap should ask for.
    uint8_t activeCount() const;

    // Command every bound primary to `on`. The shared half of what a tap does;
    // WHETHER a tap does anything is the domain type's business, which is why
    // this layout deliberately does not override onTap(). A binary_sensor and a
    // switch look identical and behave completely differently.
    void commandAll(bool on);

private:

    static StateCardFill s_fill;

    // Is a point `fromBottom` px above the surface's bottom edge covered by a
    // brightness fill of `pct` percent? Decides text colour element by
    // element, because on a half-filled card the name sits on the fill and
    // the corner icon does not. See render().
    bool onFill(int32_t fromBottom, int pct) const;

    lv_obj_t *_mid       = nullptr;   // grows; centres the disc
    lv_obj_t *_statusRow = nullptr;   // reserved, matches ValueCard's status line
    lv_obj_t *_corner = nullptr;  // the DOMAIN icon; see Card::renderCornerIcon()
    lv_obj_t *_disc  = nullptr;
    lv_obj_t *_icon  = nullptr;   // the HERO: this thing, in its current state
    lv_obj_t *_name  = nullptr;
    lv_obj_t *_mixed = nullptr;   // the not-uniform badge. Hidden when uniform
};

#endif // STATE_CARD_H
