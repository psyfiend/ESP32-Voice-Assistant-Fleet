#pragma once
#ifndef LAYOUT_H
#define LAYOUT_H
// ---------------------------------------------------------------------------
// LAYOUT (2.12 P5) - where things go on a grid, as plain arithmetic: no LVGL,
// no Arduino, no ESP-IDF, so it runs - and is tested - anywhere (host-tests/).
// docs/design/ui-foundation.md section 8.
//
// An ENGINE takes a CONTAINER (a grid of units: columns and rows) and the
// ITEMS to place in it - each with the span it would like and the least it
// will accept, an optional pinned spot, and a priority - and decides where
// each goes, or that it does not fit. CardPage's dashboard grid is its first
// container; a cluster's sub-grid (2.11) will be another.
//
// FlowEngine is the dashboard's placement as it has always been (2.5, ROADMAP
// Q3b): pinned items first, then each in turn at the first free position with
// the widest span that fits there, and while anything does not fit the lowest
// priority is dropped and the whole layout tried again. Other engines - the
// packing ("bento") one 2.11 wants - implement the same interface.
// ---------------------------------------------------------------------------
#include <stdint.h>

namespace Layout {

constexpr uint8_t ITEMS_MAX = 48;   // CardPage's CARD_PAGE_MAX
constexpr uint8_t COLS_MAX  = 32;   // the unit grid's hard limits (CardPage)
constexpr uint8_t ROWS_MAX  = 48;

// One thing to place. Spans are in units; 0 means one unit.
struct Item {
    bool    present   = true;   // false: an empty slot, never placed
    uint8_t prefSpanX = 0, prefSpanY = 0;
    uint8_t minSpanX  = 0, minSpanY  = 0;
    int8_t  col = -1, row = -1; // a pinned spot, or -1
    uint8_t priority = 0;       // the lowest is dropped first; ties drop the later one
};

// Where one item went.
struct Place {
    uint8_t col = 0, row = 0, spanX = 0, spanY = 0;
    bool    placed  = false;
    bool    pinFail = false;    // asked for a spot it could not have. KEPT ACROSS CALLS: an
                                // engine resets everything else, so a bad pin stays reportable
};

// The grid, in units.
struct Container {
    uint8_t cols = 1, rows = 1;
};

// What an engine tells as it decides, for a log. Null members: nothing.
struct Events {
    void (*pinRejected)(void *ctx, uint8_t item, int8_t col, int8_t row) = nullptr;
    void (*dropped)(void *ctx, uint8_t item, uint8_t priority) = nullptr;
    void  *ctx = nullptr;
};

class Engine {
public:
    virtual ~Engine() = default;
    // Place items[0..n) in c, writing out[i] for each (pinFail kept, see Place).
    // Returns how many were placed.
    virtual uint8_t place(const Container &c, const Item *items, uint8_t n, Place *out,
                          const Events *ev = nullptr) = 0;
};

class FlowEngine : public Engine {
public:
    uint8_t place(const Container &c, const Item *items, uint8_t n, Place *out,
                  const Events *ev = nullptr) override;

private:
    bool placeOne(const Container &c, const Item &it, uint8_t i, Place &out, const Events *ev);
    bool blockFree(uint8_t col, uint8_t row, uint8_t spanX, uint8_t spanY) const;
    void occupy(uint8_t col, uint8_t row, uint8_t spanX, uint8_t spanY);
    void clearOcc();

    // Occupancy, one bit per unit, row-major: flow alone cannot overlap, but flow
    // MIXED with pinned items can, and a bitmap lets the two coexist. 192 bytes.
    uint8_t occ_[ROWS_MAX][(COLS_MAX + 7) / 8];
};

// The fewest CELL rows the items could need on `cellCols` cells of `sub` units
// each: a lower bound from the area they ask for, each rounded up to whole
// cells (a span of 0 asks for a whole cell here). A page starts here and adds
// rows until everything fits.
uint8_t rowsWanted(const Item *items, uint8_t n, uint8_t cellCols, uint8_t sub);

} // namespace Layout

#endif // LAYOUT_H
