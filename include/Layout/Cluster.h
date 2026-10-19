#pragma once
// ---------------------------------------------------------------------------
// CLUSTER ARITHMETIC (2.11 round 1, DECISIONS K89, K92, K103) - a cluster is a
// group card shown as a block of its members on the page: one border, a header
// row, and the members as MINI TILES inside (no border of their own). Plain
// arithmetic like the rest of Layout - no LVGL, Arduino or ESP-IDF - so it is
// tested on the PC (host-tests/test_cluster). docs/design/group-cards.md §6-7,
// layout-engines.md ("A cluster is a container inside an item").
//
// Two questions, both in pixels:
//   - how the tiles share the room inside a cluster (tileGrid), and
//   - how many cells a cluster asks the page for (clusterSpan) - until the
//     arranger (#78, K105) decides shapes, the page's flow engine places the
//     cluster at the span this gives.
// ---------------------------------------------------------------------------
#include <stdint.h>

namespace Layout {

// Columns x rows for n tiles in a w x h box, `gap` between them, and the size
// each tile gets. The rule is the "2.11 Clusters" mock's: the biggest touch
// target first (a tile's shorter side), then the squarest tile, then the fewest
// empty places. A short last row is not left ragged: its tiles share the row's
// whole width (lastW).
struct TileGrid {
    uint8_t cols = 1, rows = 1;
    int32_t tileW = 0, tileH = 0;   // every tile but a short last row's
    int32_t lastW = 0;              // a tile in the last row (== tileW when it is full)
};
TileGrid tileGrid(uint8_t n, int32_t w, int32_t h, int32_t gap);

// Where tile i (0-based, reading order) stands in a grid laid out by tileGrid,
// from the box's top-left corner.
void tileAt(const TileGrid &g, uint8_t n, uint8_t i, int32_t gap, int32_t &x, int32_t &y, int32_t &w);

// The page's cells and a cluster's own frame, in pixels.
struct ClusterGeom {
    int32_t cellW = 0, cellH = 0;   // one cell
    int32_t gap = 0;                // between cells - a block of two cells is 2 cells + 1 gap
    int32_t chromeW = 0;            // what the frame takes from a block's width: border and padding
    int32_t chromeH = 0;            // ... and its height: border, header row and padding
    int32_t tileGap = 0;
    int32_t minTile = 0;            // the least a tile may be each way ("tight", K103)
    bool    oneCell = false;        // may it be a single cell? Sensors only, no header (K103)
};

// How many cells a cluster of n tiles asks for: the fewest cells in which every
// tile is at least minTile each way; among equal counts the wider block (a
// cluster reads across, and the page's rows are short), then the one whose
// tiles are bigger. Never more than maxCols x maxRows; fits is false when no
// block that size holds the tiles - the cluster is then shown as its one-cell
// group card (K92 rule 6).
struct ClusterSpan {
    uint8_t cols = 0, rows = 0;
    bool    fits = false;
};
ClusterSpan clusterSpan(uint8_t n, const ClusterGeom &g, uint8_t maxCols, uint8_t maxRows);

}  // namespace Layout
