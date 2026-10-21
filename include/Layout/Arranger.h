#pragma once
// ---------------------------------------------------------------------------
// THE ARRANGER (2.11, #78; DECISIONS K105, K109) - where a page's blocks go: its
// solo cards and its clusters, each in any shape its limits allow, on a grid of
// UNITS - the device's own, a cell's halves - so a cluster can be 2.5 cells wide
// and a row 1.5 cells tall. Layout Playground version 8, ported line for line
// (the "Layout Playground" artifact, 2026-10-20) with the owner's tuned knobs as
// its defaults. Plain arithmetic like the rest of Layout - no LVGL, Arduino or
// ESP-IDF - tested on the PC (host-tests/test_arranger), where the playground's
// own answers for the same blocks are the expected values.
// docs/design/layout-engines.md, "Then version 8". Version 5 (round 2) is in
// git history and still selectable in the playground.
//
// The owner's rules (TEST_2.11 round 5), and what the search is paid for:
//   - every ENTITY THE SAME SIZE - a solo card and each cluster's tiles, the size
//     the side of the square with its own area, its label and header paid -
//     against the page's equal share first, then against the average the first
//     pass found (two passes a grid);
//   - solo cards SQUARE; tiles not too far from square, more lightly;
//   - clusters WIDER THAN TALL on a landscape page;
//   - clusters with the SAME NUMBER OF TILES the same size;
//   - empty units, by the entity-sized area they waste - cheap on purpose: "I'd
//     prefer small amounts of dead space over grossly disproportionate units per
//     tile";
//   - blocks out of reading order.
// Limits never crossed: a solo card no smaller than one card of the P4_5 at
// 7 x 3, and no taller for its width; a tile no smaller than one of a 3-wide,
// one-cell cluster on the 4B at 4 x 4. Clusters wear the device's header and
// padding (UIShape C_HDR_H, C_PAD, C_TILE_GAP). Solo cards then take spare unit
// rows above or below them as even margins (the finish, v7's).
//
// GRIDS: units across from 2 x (the board's columns - 1) to 2 x (its columns +
// 2), down 2 x (its rows -/+ 1); a count the page pins (the system panel's Col /
// Row) is the only one tried that way - both pinned, one grid, as the
// playground's fixed grid. SEARCH: v5's - fill the first empty unit
// with one of the next three unplaced blocks in each of its shapes, or leave it
// empty, branch and bound, rows as bit masks - ARR_BUDGET steps a pass.
//
// Deterministic: the same blocks on the same screen give the same page. Run when
// a page's blocks change and the answer kept (Cards/PageArranger), never on every
// page build.
// ---------------------------------------------------------------------------
#include <stdint.h>

namespace Layout {

constexpr uint8_t  ARR_BLOCKS_MAX = 24;      // a page's blocks
constexpr uint8_t  ARR_SHAPES_MAX = 10;      // a block's cheapest shapes kept, per grid (the playground's)
constexpr uint8_t  ARR_UNITS_MAX  = 24;      // units across or down, at most
constexpr uint16_t ARR_CELLS_MAX  = 192;     // units on a grid, at most: the search recurses a level a unit
constexpr uint8_t  ARR_WINDOW     = 3;       // the next unplaced blocks tried at each empty unit
constexpr uint32_t ARR_BUDGET     = 20000;   // steps per pass per grid before the search stops

// The owner's knobs (2026-10-20, tuned in the playground: its defaults now),
// adjustable from the PC (system.arranger.*).
struct ArrWeights {
    double size   = 75.0;   // entities of different sizes: x log(size / target)^2 each
    double square = 30.0;   // a solo card far from square: x 4 log(h / w)^2 (a tile 0.4 of that)
    double wide   = 20.0;   // a cluster taller than wide: x 2 log(h / w)
    double twin   = 70.0;   // clusters with as many tiles, not the same size: x |log(area ratio)|
    double gap8   = 11.0;   // an empty unit, per entity-sized area it wastes
    double order  = 4.0;    // each block placed ahead of an earlier one
};

// How a solo card wears its card label: what it takes from the card's box.
enum class ArrLabel : uint8_t {
    LABEL_FLOAT,    // across the top corner: down half the tag, in a quarter (the playground's default)
    LABEL_ATTACHED, // above the card: 0.72 of the tag
    LABEL_FOLDER,   // a whole tag's height
    LABEL_BAR,      // a band inside the card
    LABEL_NONE,
};

// The screen: the page's room in pixels and how many to a millimetre.
struct ArrScreen {
    double   pmm = 1.0;              // pixels per millimetre
    double   aw = 0, ah = 0;         // the page's width and height for blocks, px
    double   gap = 0;                // between units, px
    double   tagH = 0;               // a card label's height, px
    ArrLabel label = ArrLabel::LABEL_FLOAT;
    uint8_t  recCols = 0, recRows = 0;   // the board's own grid, in cells (0: 4 x 3)
    uint8_t  fixCols = 0, fixRows = 0;   // the page's pinned columns / rows, in cells (0: the arranger chooses)
};

// One block: a solo card, or a cluster of `members`, `hdr` of them in its header
// and `tiles` shown as tiles.
struct ArrBlock {
    bool    cluster = false;
    uint8_t tiles = 0, hdr = 0, members = 0;
};

// Where a block went, in units: its box, and a cluster's tiles across and down.
struct ArrPlace {
    uint8_t c = 0, r = 0, w = 1, h = 1;
    uint8_t tc = 0, tr = 0;   // a cluster's tiles across x down; 0 for a card
};

struct ArrResult {
    bool     ok = false;
    uint8_t  cols = 0, rows = 0;   // the grid, in units
    double   cwmm = 0, chmm = 0;   // a unit, mm
    double   score = 0;
    double   mean = 0;             // the entities' size, mm (their geometric mean)
    uint16_t holes = 0;            // empty units, as the search left them
    uint32_t nodes = 0;            // search steps, every grid and pass
    bool     capped = false;       // the kept page's search hit ARR_BUDGET
    ArrPlace pos[ARR_BLOCKS_MAX];  // as the search placed them
    ArrPlace box[ARR_BLOCKS_MAX];  // after the finish: a solo card in its even margins (a cluster as pos)
};

// The owner's two references (the playground's refSizes()): the smallest solo
// card, its tallest proportion, and the smallest tile - mm.
struct ArrRef { double soloW, soloH, tall, tileW, tileH; };
ArrRef arrRef();

// The cheapest page for the blocks, in `out`. False when none fits.
bool arrange(const ArrScreen &scr, const ArrBlock *blocks, uint8_t n, const ArrWeights &w, ArrResult &out);

}  // namespace Layout
