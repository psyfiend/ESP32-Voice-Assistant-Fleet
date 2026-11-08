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
//      A user may ASK for the word in place of the name (CardLabel, cards.md
//      section 13); the choice defaults to off. The word is the device_class
//      table's "Open" or "Detected", never a raw "on" off the wire.
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

// How an active state is shown (the card state, D-30 to D-33). cards.md
// section 4 specifies FILL - "the card reflects state across its whole
// surface, not in a corner"; LIGHT_ICON lights only the icon and name and the
// card keeps its surface; the frames draw the state on the card's edge.
enum class StateCardFill : uint8_t {
    FILL_SURFACE = 0,   // cards.md section 4 as written
    LIGHT_ICON,         // icon and name take the state colour; surface does not
    // THE ACTIVE FRAMES: LIGHT_ICON's icon and name, and the state on the
    // card's EDGE, lit inward, in place of a fill (Card::activeFrame).
    FRAME_BRACKET,      // heavy top and bottom bars, light falling from them
    FRAME_CORNER,       // the top-right and bottom-left corners cut away at 45 degrees (not on Linen)
};

class StateCard : public Card {
public:
    // PAGE-WIDE, deliberately static (the device's card state, a UI Tweaks
    // choice): making it static means page code never has to downcast a Card
    // to reach it.
    static void setFill(StateCardFill f) { s_fill = f; }
    static StateCardFill fill() { return s_fill; }

protected:
    void buildBody(lv_obj_t *body) override;
    void render() override;

    // How many bound primaries are on: Card::activeCount() (shared
    // with the popup and the GroupOn rule). A count rather than a bool,
    // so all / none / some are told apart in one read - the mixed indicator.

    // Command every bound primary to `on`. The shared half of what a tap does;
    // WHETHER a tap does anything is the domain type's business, which is why
    // this layout deliberately does not override onTap(). A binary_sensor and a
    // switch look identical and behave completely differently.
    void commandAll(bool on);

    // The words under the hero for a resolved label: the state's word, or the
    // name. A summary (K98) names its active members instead (CardCatalog).
    virtual const char *nameText(CardLabel lbl, const Entity &e, bool isOn);

private:
    // As a cluster's MINI TILE (K89, K100): the icon on its disc - lit, a
    // solid disc in the active colour, while on - its entity label under it.
    // Cards/MiniTile.cpp.
    void buildMiniBody(lv_obj_t *body);
    void renderMini();

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
