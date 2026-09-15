#pragma once
#ifndef CARD_PAGE_H
#define CARD_PAGE_H

#include <lvgl.h>
#include "Cards/Card.h"
#include "Cards/CardBinder.h"

// ---------------------------------------------------------------------------
// CardPage - a grid of cards.
//
// SCOPE, stated plainly so the next session does not mistake this for 2.5.
// Milestone 2.4's job is the Card base class; this page exists so cards have
// somewhere to be looked at, and so the span fields issue #15 insists on are
// actually exercised rather than merely stored. What it does:
//
//   - derives its columns and rows from UI::grid(), which derives them from
//     the viewport. No per-board layout code, on any board.
//   - places cards in declaration order, honouring prefSpan, shrinking to
//     minSpan when the rest of a row is too narrow, and wrapping when even
//     that does not fit.
//
// What it deliberately does NOT do, and what milestone 2.5 owes:
//
//   - anything with CardPlacement::priority. It is carried and reported and
//     nothing reads it. Real responsive degradation means ORDERING by priority
//     and dropping the losers, and that is a page-engine decision that wants
//     the whole config-struct model around it.
//   - a config struct. Pages are built in code here; 2.5 builds them from
//     data and 3.1 builds that data from the build sheet.
//
// The page OWNS its cards and deletes them. It does not own the binder.
// ---------------------------------------------------------------------------

static constexpr uint8_t CARD_PAGE_MAX = 24;

class CardPage {
public:
    ~CardPage();

    // parent is any container; the page fills it. binder may be null, in which
    // case the cards render once and never update - useful for a static
    // reference layout, wrong for a dashboard.
    //
    // tagOverhang is how far an external tag sticks up above its card, in real
    // pixels, or 0 when no card on this page wears one. It is a PAGE concern
    // rather than a card one because the clearance a tag needs comes out of
    // the space BETWEEN cards: the owner's own framing - "enabling the tag sets
    // a requirement for a minimum card gap so a tag doesn't touch or overlap
    // with a card above it". The page widens its row gap and top inset to
    // exactly that, and shortens the rows so the grid still fits.
    void begin(lv_obj_t *parent, CardBinder *binder, int32_t tagOverhang = 0);

    // Hand the page a card. The page takes ownership, builds it, places it and
    // registers it with the binder. Returns the same pointer for chaining, or
    // null if the page is full - in which case the card is deleted rather than
    // leaked.
    Card *add(Card *c);

    // Re-run placement. Call after a viewport or target-card-width change.
    void relayout();

    lv_obj_t *root() const { return _root; }
    uint8_t   count() const { return _n; }

    // One line per card for the System Doctor: type, span, state, bindings.
    void report() const;

private:
    void placeCard(Card *c);

    // Grow the row descriptor so `need` rows exist. Rows past the viewport are
    // real rows that scroll into view, not overflow.
    void ensureRows(uint8_t need);

    // Dry-run of placeCard()'s cursor arithmetic: would this card land on a
    // row the page actually has? A page does not scroll, so one that would not
    // is never built.
    bool fits(const Card *c) const;

    lv_obj_t   *_root   = nullptr;
    CardBinder *_binder = nullptr;
    Card       *_cards[CARD_PAGE_MAX] = {nullptr};
    uint8_t     _n = 0;

    // LVGL's grid descriptors are held BY POINTER for the lifetime of the
    // container, not copied, so they are members rather than locals. +1 for
    // LV_GRID_TEMPLATE_LAST, which is how LVGL finds the end of each array.
    //
    // THE ROW ARRAY IS SIZED FOR EVERY CARD, not for the visible rows, and
    // that is not defensive padding - it is the fix for a hard freeze. The row
    // descriptor used to hold exactly UIGrid::rows entries, because the page
    // scrolls and rows are conceptually unbounded. But placement kept flowing
    // into row 3, 4, 5... and LVGL then indexed a three-entry array with a 4,
    // read past the end, and hit LV_ASSERT - which lv_conf.h defines as
    // `while(1);`. On WS_P4_5 the nine demo cards happened to fill exactly
    // 5x2 with nothing left over, so it never showed; on CYD_S3_3248 at two
    // columns the same nine cards need five rows and it locked the board
    // solid. A page can never place more rows than it has cards.
    lv_coord_t _colDsc[16 + 1];
    lv_coord_t _rowDsc[CARD_PAGE_MAX + 1];

    uint8_t    _rowsDefined = 0;   // entries currently valid in _rowDsc
    uint16_t   _cellH       = 0;   // the token height every row gets
    uint8_t    _maxRows     = 1;   // rows that FIT. Nothing is placed past them

    // The cursor placement walks. Rows are unbounded - a page taller than the
    // viewport scrolls, which is why every container here goes through
    // UI::tameScroll().
    uint8_t _curCol = 0;
    uint8_t _curRow = 0;
};

#endif // CARD_PAGE_H
