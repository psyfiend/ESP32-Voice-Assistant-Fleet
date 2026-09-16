#pragma once
#ifndef CARD_PAGE_H
#define CARD_PAGE_H

#include <lvgl.h>
#include "Cards/Card.h"
#include "Cards/CardBinder.h"
#include "Cards/PageSpec.h"
#include "EntityRegistry.h"

// ---------------------------------------------------------------------------
// CardPage - a grid of cards, rendered from a PageSpec.
//
// MILESTONE 2.5. 2.4 left this class deliberately thin: it derived columns and
// honoured spans so that issue #15's span fields were exercised rather than
// merely stored, and it said in this comment what it did not do. This is that
// list, done:
//
//   - a page is built from DATA (PageSpec), not from imperative calls. The
//     imperative path still exists for CardDemo's comparison bench, and both
//     now finish through the same commit(), because a second placement
//     implementation is exactly the kind of third thing HANDOFF.md warns about.
//   - SUB-GRID UNITS. ROADMAP Q3b: a page authored as N x M cells allocates
//     N*sub x M*sub units and cards span in units, so half- and quarter-cell
//     cards are expressible. Default subdivision 2.
//   - EXPLICIT PLACEMENT plus a validator. A card may pin itself to a unit
//     coordinate; overlaps and out-of-bounds are detected and REPORTED rather
//     than silently resolved, which is Q3b's "no solver, fully predictable".
//   - PRIORITY DEGRADATION. When the page runs out of room the lowest-priority
//     cards are dropped until the rest fit, instead of whatever happened to be
//     declared last.
//
// THE SUBDIVISION IS DIMENSIONALLY TRANSPARENT. Splitting each cell into `sub`
// tracks with a gap between them leaves a `sub`-unit span exactly the width a
// whole cell used to be - the arithmetic cancels, which is why no card changes
// size when the subdivision does. Row heights lose up to (sub-1) px to integer
// division, and commit() hands each card its REAL pixel height so the
// compact/full decision is made against what the card actually got.
//
// The page OWNS its cards and deletes them. It does not own the binder.
// ---------------------------------------------------------------------------

// Raised from 24 at 2.5. A 7-column board at 3 rows is 21 cells before any
// half-cell card exists, and the owner's own entity list is around 18 cards -
// 24 was close enough to bite.
static constexpr uint8_t CARD_PAGE_MAX = 48;

// The unit grid's hard limits. 16 cells across at subdivision 2 is already far
// past the fleet's densest board; the rows bound is what _rowDsc is sized for.
static constexpr uint8_t PAGE_MAX_UNIT_COLS = 32;
static constexpr uint8_t PAGE_MAX_UNIT_ROWS = 48;

class CardPage {
public:
    ~CardPage();

    // parent is any container; the page fills it. binder may be null, in which
    // case the cards render once and never update - useful for a static
    // reference layout, wrong for a dashboard.
    void begin(lv_obj_t *parent, CardBinder *binder,
               uint8_t subdivision = PAGE_SUBDIVISION_DEFAULT);

    // Build a whole page from data. Creates a card per spec whose primary
    // entity exists, configures it, plans the layout and commits.
    //
    // Specs whose primary is missing from the registry are SKIPPED and
    // reported, never fatal. That is what makes one fleet-wide dashboard safe:
    // a board that cannot see an entity simply does not draw its card.
    void applySpec(const PageSpec &spec, EntityRegistry &reg);

    // Hand the page a card. The page takes ownership. NOTHING IS BUILT OR
    // PLACED until commit() - placement is a decision about all the cards at
    // once now that priority can drop some of them, so it cannot be made one
    // card at a time. Returns the same pointer for chaining, or null if the
    // page is full, in which case the card is deleted rather than leaked.
    Card *add(Card *c);

    // Plan, build and place everything added since begin(). Idempotent.
    void commit();

    // Re-plan and re-place without rebuilding widgets. Correct for a span or
    // priority change; NOT enough after the grid itself moves, because a card
    // resolves compact-vs-full from its cell height at build time - rebuild
    // the page for that.
    void relayout();

    lv_obj_t *root() const { return _root; }
    uint8_t   count() const { return _n; }
    uint8_t   placed() const { return _placed; }
    uint8_t   dropped() const { return (uint8_t)(_n - _placed); }

    // One line per card for the System Doctor: type, span, priority, where it
    // landed, and why it is not on screen if it is not.
    void report() const;

private:
    // Where one card ended up. Held beside the card rather than on it because
    // it is the PAGE's decision and a card that moves to another page must not
    // carry a stale one.
    struct Slot {
        uint8_t col = 0, row = 0, spanX = 0, spanY = 0;
        bool    placed  = false;
        bool    pinFail = false;   // asked for an explicit spot it could not have
    };

    // The whole layout decision. Runs on plain arithmetic with no LVGL, so it
    // can be reasoned about (and later tested) without a display.
    void plan();
    bool placeOne(uint8_t i);
    bool blockFree(uint8_t col, uint8_t row, uint8_t spanX, uint8_t spanY) const;
    void occupy(uint8_t col, uint8_t row, uint8_t spanX, uint8_t spanY);
    void clearOcc();

    // The real pixel height of a card spanning `spanY` units, gaps included.
    int32_t heightOf(uint8_t spanY) const;

    lv_obj_t   *_root   = nullptr;
    CardBinder *_binder = nullptr;
    Card       *_cards[CARD_PAGE_MAX] = {nullptr};
    Slot        _slot [CARD_PAGE_MAX];
    uint8_t     _n      = 0;
    uint8_t     _placed = 0;
    bool        _committed = false;

    // LVGL holds these BY POINTER for the lifetime of the container, not by
    // copy, so they are members rather than locals.
    //
    // The row array is sized for the unit grid's hard limit rather than for
    // the visible rows, and that is not defensive padding - a short row
    // descriptor is what froze CYD_S3_3248 solid at 2.4. LVGL indexed past the
    // end and hit LV_ASSERT, which lv_conf.h defines as `while(1);`.
    lv_coord_t _colDsc[PAGE_MAX_UNIT_COLS + 1];
    lv_coord_t _rowDsc[PAGE_MAX_UNIT_ROWS + 1];

    uint8_t  _sub   = PAGE_SUBDIVISION_DEFAULT;
    uint8_t  _uCols = 1;      // unit columns
    uint8_t  _uRows = 1;      // unit rows that FIT. Nothing is placed past them
    int32_t  _unitH = 0;      // one unit row, in real pixels
    int32_t  _gap   = 0;

    // Occupancy of the unit grid, one bit per unit, row-major. 192 bytes.
    // Flow placement alone cannot overlap, but flow MIXED with pinned cards
    // can, and a bitmap is the honest way to let the two coexist.
    uint8_t _occ[PAGE_MAX_UNIT_ROWS][(PAGE_MAX_UNIT_COLS + 7) / 8];
};

#endif // CARD_PAGE_H
