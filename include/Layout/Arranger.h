#pragma once
// ---------------------------------------------------------------------------
// THE ARRANGER (2.11 round 2, #78, DECISIONS K105) - where a page's blocks go:
// its cards (one cell each) and its clusters (as many cells as their tiles need),
// and on what grid. Layout Playground version 5, ported as prototyped: its
// default weights, an empty cell at 60 (the owner's). Plain arithmetic like the
// rest of Layout - no LVGL, Arduino or ESP-IDF - tested on the PC
// (host-tests/test_arranger), where the playground's own answers for the same
// blocks are the expected values. docs/design/layout-engines.md, "As prototyped".
//
// How it works, in the playground's words:
//   - GRIDS: the board's own (the owner's pick on glass) and its neighbours, one
//     row or column either way, each with cells between the smallest and biggest
//     allowed size; or exactly one grid when the page's columns and rows are set.
//   - SHAPES: every rectangle that holds a cluster's tiles at the smallest tile
//     size; one cell only for a cluster with nothing in its header. A shape is
//     dropped when a smaller shape inside it plus empty cells for the difference
//     costs no more.
//   - SEARCH: fill the FIRST EMPTY CELL in reading order with one of the next
//     three unplaced blocks in each of its shapes, or leave it empty - every
//     tiling, blocks kept near their list order. Branch and bound: a branch stops
//     once it costs more than the best page so far.
//   - COST (each a knob): an empty cell; a cluster's room beyond comfortable,
//     by its square; a long cluster; a block placed ahead of earlier ones; each
//     row or column away from the board's grid; single cards drifting from
//     today's ~20 mm card.
//
// Deterministic: the same blocks on the same screen give the same page. It is
// run when a page's blocks change and the answer kept (CardPage), never on every
// page build - its cost is the open question (K105: timed on the boards first).
// ---------------------------------------------------------------------------
#include <stdint.h>

namespace Layout {

constexpr uint8_t ARR_BLOCKS_MAX = 24;    // a page's blocks
constexpr uint8_t ARR_SHAPES_MAX = 24;    // a cluster's shapes kept, per grid
constexpr uint8_t ARR_GRID_COLS  = 12;    // the grids tried (the playground's ranges)
constexpr uint8_t ARR_GRID_ROWS  = 10;
constexpr uint8_t ARR_WINDOW     = 3;     // the next unplaced blocks tried at each empty cell
constexpr uint32_t ARR_BUDGET    = 150000;   // steps per grid before the search stops

// The weights (the playground's "Arranger" panel), adjustable from the PC.
struct ArrWeights {
    double hole  = 60.0;   // an empty cell (K105; 40 when first published)
    double comfy = 1.6;    // the room a tile needs to feel comfortable, x its least area
    double loose = 25.0;   // a cluster with too much room: x the excess, squared
    double lng   = 40.0;   // a long, thin cluster (beyond 1.8 : 1): x the log excess, squared
    double order = 4.0;    // each block placed ahead of an earlier one
    double away  = 6.0;    // each row or column away from the board's own grid
};

// The sizes, in millimetres (the playground's "Knobs").
struct ArrKnobs {
    double maxCell = 34.0, minCell = 14.0;   // a grid cell's size, each way
    double tileW = 12.0, tileH = 9.0;        // the smallest member tile
    double hdr = 5.0;                        // a cluster's header row
};

// The screen: the page's room in pixels and how many to a millimetre.
struct ArrScreen {
    double  pmm = 1.0;            // pixels per millimetre
    double  aw = 0, ah = 0;       // the page's width and height for cells, px
    double  gap = 0;              // between cells, px
    double  tagH = 0;             // a card label's height, px: a one-cell cluster wears one
    uint8_t recCols = 0, recRows = 0;   // the board's own grid (0: none)
    uint8_t fixCols = 0, fixRows = 0;   // the page's grid when set (0: the arranger chooses)
};

// One block: a card, or a cluster of `members`, `hdr` of them in its header
// and `tiles` shown as tiles. A pinned cluster keeps pinW x pinH cells.
struct ArrBlock {
    bool    cluster = false;
    uint8_t tiles = 0, hdr = 0, members = 0;
    uint8_t pinW = 0, pinH = 0;
};

struct ArrPlace {
    uint8_t c = 0, r = 0, w = 1, h = 1;
    bool    one = false;   // a cluster in one cell: no header, every member a tile
};

struct ArrResult {
    bool     ok = false;
    uint8_t  cols = 0, rows = 0;
    double   cwmm = 0, chmm = 0;   // the cell, mm
    double   score = 0;
    uint16_t holes = 0;
    uint32_t nodes = 0;            // search steps, every grid
    bool     capped = false;       // a grid's search hit ARR_BUDGET
    ArrPlace pos[ARR_BLOCKS_MAX];
};

// The cheapest page for the blocks, in `out`. False when none fits.
bool arrange(const ArrScreen &scr, const ArrBlock *blocks, uint8_t n, const ArrWeights &w,
             const ArrKnobs &k, ArrResult &out);

}  // namespace Layout
