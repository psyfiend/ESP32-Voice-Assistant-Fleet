// Layout (2.12 P5): the dashboard's flow placement, moved from CardPage.cpp as it was - see
// include/Layout/Layout.h. Tested off the board in host-tests/test/test_layout.
#include "Layout/Layout.h"

namespace Layout {

void FlowEngine::clearOcc() {
    for (uint8_t r = 0; r < ROWS_MAX; r++)
        for (uint8_t b = 0; b < sizeof(occ_[0]); b++) occ_[r][b] = 0;
}

bool FlowEngine::blockFree(uint8_t col, uint8_t row, uint8_t spanX, uint8_t spanY) const {
    for (uint8_t r = row; r < row + spanY; r++) {
        if (r >= ROWS_MAX) return false;
        for (uint8_t c = col; c < col + spanX; c++) {
            if (c >= COLS_MAX) return false;
            if (occ_[r][c >> 3] & (uint8_t)(1u << (c & 7))) return false;
        }
    }
    return true;
}

void FlowEngine::occupy(uint8_t col, uint8_t row, uint8_t spanX, uint8_t spanY) {
    for (uint8_t r = row; r < row + spanY && r < ROWS_MAX; r++)
        for (uint8_t c = col; c < col + spanX && c < COLS_MAX; c++)
            occ_[r][c >> 3] |= (uint8_t)(1u << (c & 7));
}

bool FlowEngine::placeOne(const Container &cn, const Item &p, uint8_t i, Place &out, const Events *ev) {
    // Clamp to the container before anything else. On CYD_S3_3248 portrait the
    // page is two cells wide, so an item asking for three is not a
    // misconfiguration to reject - it is a card designed for a bigger board, and
    // the correct answer is the widest thing that fits.
    uint8_t pX = p.prefSpanX ? p.prefSpanX : 1;
    uint8_t mX = p.minSpanX  ? p.minSpanX  : 1;
    uint8_t pY = p.prefSpanY ? p.prefSpanY : 1;
    uint8_t mY = p.minSpanY  ? p.minSpanY  : 1;
    if (pX > cn.cols) pX = cn.cols;
    if (pY > cn.rows) pY = cn.rows;
    if (mX > pX)      mX = pX;
    if (mY > pY)      mY = pY;

    // --- Explicit placement, and the validator ROADMAP Q3b asked for -------
    //
    // An invalid pin is REPORTED and then flowed, rather than dropped. The item
    // is still the one the author asked for; only its coordinate was wrong, and
    // silently losing it would be a worse answer than putting it somewhere
    // visible and saying so.
    if (p.col >= 0 && p.row >= 0) {
        const uint8_t cc = (uint8_t)p.col, rr = (uint8_t)p.row;
        if (cc + pX <= cn.cols && rr + pY <= cn.rows && blockFree(cc, rr, pX, pY)) {
            occupy(cc, rr, pX, pY);
            out.col = cc; out.row = rr;
            out.spanX = pX; out.spanY = pY;
            out.placed = true;
            return true;
        }
        out.pinFail = true;
        if (ev && ev->pinRejected) ev->pinRejected(ev->ctx, i, p.col, p.row);
    }

    // --- Flow -------------------------------------------------------------
    //
    // First free position wins, and at that position the WIDEST span that fits
    // wins. That ordering is deliberate: an item that fits at its minimum beside
    // its neighbour beats one sitting alone on a new row at full width, which is
    // the rule 2.4's cursor arithmetic encoded and this keeps.
    for (uint8_t r = 0; r + mY <= cn.rows; r++) {
        for (uint8_t cc = 0; cc + mX <= cn.cols; cc++) {
            for (uint8_t sx = pX; sx >= mX; sx--) {
                if (cc + sx > cn.cols) continue;
                for (uint8_t sy = pY; sy >= mY; sy--) {
                    if (r + sy > cn.rows) continue;
                    if (!blockFree(cc, r, sx, sy)) continue;
                    occupy(cc, r, sx, sy);
                    out.col = cc; out.row = r;
                    out.spanX = sx; out.spanY = sy;
                    out.placed = true;
                    return true;
                }
            }
        }
    }
    return false;
}

uint8_t FlowEngine::place(const Container &cn, const Item *items, uint8_t n, Place *out, const Events *ev) {
    if (n > ITEMS_MAX) n = ITEMS_MAX;
    bool alive[ITEMS_MAX];
    for (uint8_t i = 0; i < n; i++) alive[i] = items[i].present;

    for (;;) {
        clearOcc();
        for (uint8_t i = 0; i < n; i++) {
            const bool pf = out[i].pinFail;
            out[i] = Place();
            out[i].pinFail = pf;   // a bad pin stays reportable across replans
        }

        // PINNED ITEMS ARE PLACED FIRST, in two passes.
        //
        // The serial from WS_P4_5 showed "pin 2,2 rejected (out of bounds or
        // occupied) - flowing": the coordinate was perfectly valid and had
        // already been taken by a card that flowed into it first. A pin is a
        // STRONGER statement than flow - the author named that spot - so flow
        // must not be allowed to consume it. One pass for the pinned, then one
        // for everything else.
        bool ok = true;
        for (uint8_t pass = 0; pass < 2 && ok; pass++) {
            for (uint8_t i = 0; i < n; i++) {
                if (!alive[i] || !items[i].present) continue;
                const bool pinned = (items[i].col >= 0 && items[i].row >= 0);
                if (pinned != (pass == 0)) continue;
                if (!placeOne(cn, items[i], i, out[i], ev)) { ok = false; break; }
            }
        }
        if (ok) break;

        // PRIORITY DEGRADATION. Drop the lowest-priority item still alive and try
        // the whole layout again - not the item that happened to fail. Those are
        // different items whenever a low-priority one was declared early, and
        // picking the failure would make the outcome depend on declaration
        // order, which is precisely what priority exists to stop.
        //
        // Ties go to the LATER declaration, so an author listing equals keeps
        // the reading order they wrote.
        int16_t drop = -1;
        uint8_t lowest = 255;
        for (uint8_t i = 0; i < n; i++) {
            if (!alive[i] || !items[i].present) continue;
            const uint8_t pr = items[i].priority;
            if (drop < 0 || pr <= lowest) { lowest = pr; drop = (int16_t)i; }
        }
        if (drop < 0) break;

        alive[(uint8_t)drop] = false;
        if (ev && ev->dropped) ev->dropped(ev->ctx, (uint8_t)drop, lowest);
    }

    uint8_t placed = 0;
    for (uint8_t i = 0; i < n; i++) if (out[i].placed) placed++;
    return placed;
}

uint8_t rowsWanted(const Item *items, uint8_t n, uint8_t cellCols, uint8_t sub) {
    if (!n) return 1;
    if (!sub) sub = 1;
    if (!cellCols) cellCols = 1;
    uint16_t cells = 0;
    for (uint8_t i = 0; i < n; i++) {
        if (!items[i].present) continue;
        const uint8_t sx = items[i].prefSpanX ? items[i].prefSpanX : sub;
        const uint8_t sy = items[i].prefSpanY ? items[i].prefSpanY : sub;
        // Round each item up to whole cells; a half-cell card still occupies a
        // row, and this is a floor, not a packing.
        const uint8_t cx = (uint8_t)((sx + sub - 1) / sub);
        const uint8_t cy = (uint8_t)((sy + sub - 1) / sub);
        cells = (uint16_t)(cells + cx * cy);
    }
    uint16_t rows = (uint16_t)((cells + cellCols - 1) / cellCols);
    if (rows < 1) rows = 1;
    return (uint8_t)(rows > 255 ? 255 : rows);
}

} // namespace Layout
