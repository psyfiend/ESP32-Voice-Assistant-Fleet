#pragma once
#ifndef CLUSTER_CARD_H
#define CLUSTER_CARD_H

#include "Cards/Card.h"

class CardBinder;

// ---------------------------------------------------------------------------
// ClusterCard - a GROUP CARD shown as a CLUSTER (2.11, DECISIONS K88-K92,
// K100-K104; docs/design/group-cards.md §6): one border on the page, a header
// row, and the members inside it as MINI TILES with no border of their own.
//
// The seam Card.h kept for "a card that contains other cards". The cluster OWNS
// its members - each a whole card, built from its own spec with its own id and
// settings, registered with the binder like any card - and draws the tiles
// where Layout::tileGrid() puts them.
//
//   header   one line (K100): the group's swatch and the cluster's name on the
//            left, the header's members' values on the right - icons with words
//            where there is room, words alone where not (K101), placed by the
//            page header's planner of claims (Layout/HeaderPlan.h), so what does
//            not fit gives way lowest first (K104: no fixed number of slots).
//            Swatch or Bar (K102). Occupancy has its own icon for each state.
//   tiles    the visible members (K104's Visible): a light's lit disc, a
//            sensor's number. A tap is the member's own; a long press opens the
//            member's own window (K104).
//   itself   a long press on the header opens the cluster's window on its
//            Members; a tap there does nothing (the owner, 2026-10-19: so a brush
//            cannot switch a room off).
//
// It has no state of its own: its members show theirs. Its header follows the
// members it shows (owns() / onSnapshot()).
// ---------------------------------------------------------------------------
class ClusterCard : public Card {
public:
    ClusterCard();
    ~ClusterCard() override;

    const char *typeName() const override { return "cluster"; }

    // Before build(). Members in order; the cluster deletes them. `tile`: shown as
    // a tile (K104's Visible); `slot`: its place in the header's priority list, or
    // -1. A member that is neither is still a member - in its window's Members.
    bool addMember(Card *m, bool tile, int8_t slot);
    void setBinder(CardBinder *b) { _binder = b; }
    void setStyle(ClusterStyle s) { _style = s; }
    void setIcons(bool on) { _icons = on; }

    uint8_t memberCount() const { return _n; }
    uint8_t tileCount() const;
    uint8_t headerCount() const;   // the members in its header
    Card   *memberAt(uint8_t i) const { return i < _n ? _m[i] : nullptr; }
    bool    tileAt(uint8_t i) const { return i < _n && _tile[i]; }     // shown as a tile, as built
    int8_t  slotAt(uint8_t i) const { return i < _n ? _slotOf[i] : -1; }   // its place in the header, or -1

    bool owns(const char *entityId) const override;
    void onSnapshot(const Entity &snap) override;
    void pollState(uint32_t nowMs) override { (void)nowMs; }

    // The frame's share of a block: what the tiles cannot have (border, header
    // row, padding), in pixels. The page asks it before it knows the cluster's
    // span (Layout::clusterSpan()).
    static int32_t chromeW();
    static int32_t chromeH();

protected:
    void buildBody(lv_obj_t *body) override;
    void render() override;
    void onLongPress() override;

private:
    void layoutHeader();

    Card       *_m[GROUP_MEMBERS_MAX] = {nullptr};
    bool        _tile[GROUP_MEMBERS_MAX] = {false};
    int8_t      _slotOf[GROUP_MEMBERS_MAX] = {0};
    uint8_t     _n = 0;
    CardBinder *_binder = nullptr;
    ClusterStyle _style = ClusterStyle::CLSTYLE_SWATCH;
    bool        _icons = true;

    lv_obj_t *_room = nullptr;   // the card's body: the header and the tiles stand in it
    // The header: the row, its bar's skirt, the swatch, the name, and a value per slot.
    lv_obj_t *_hdr = nullptr, *_skirt = nullptr, *_swatch = nullptr, *_title = nullptr;
    struct Slot { int8_t member = -1; lv_obj_t *box = nullptr, *icon = nullptr, *text = nullptr; };
    Slot      _slot[CLUSTER_SLOTS_MAX];
    int32_t   _innerW = 0;
};

#endif // CLUSTER_CARD_H
