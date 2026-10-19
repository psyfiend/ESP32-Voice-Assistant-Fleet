// Cluster arithmetic (2.11 round 1): include/Layout/Cluster.h says what and why.
#include "Layout/Cluster.h"
#include <math.h>

namespace Layout {

TileGrid tileGrid(uint8_t n, int32_t w, int32_t h, int32_t gap) {
    TileGrid best;
    if (n == 0 || w <= 0 || h <= 0) return best;
    float bestScore = 0.f;
    bool  found = false;
    for (uint8_t c = 1; c <= n; c++) {
        const uint8_t r  = (uint8_t)((n + c - 1) / c);
        const int32_t tw = (w - (c - 1) * gap) / c;
        const int32_t th = (h - (r - 1) * gap) / r;
        if (tw <= 0 || th <= 0) continue;
        const float m = (float)(tw < th ? tw : th);
        // The mock's score: a bigger touch target first, a squarer tile next,
        // and an empty place costs a little of the box's width.
        const float score = -m + fabsf(logf((float)tw / (float)th)) * m * 0.15f
                          + (float)(c * r - n) * 0.02f * (float)w;
        if (!found || score < bestScore) {
            found = true;
            bestScore = score;
            best.cols = c;
            best.rows = r;
            best.tileW = tw;
            best.tileH = th;
        }
    }
    if (!found) return best;
    const uint8_t inLast = (uint8_t)(n - (best.rows - 1) * best.cols);
    best.lastW = (w - (inLast - 1) * gap) / inLast;
    return best;
}

void tileAt(const TileGrid &g, uint8_t n, uint8_t i, int32_t gap, int32_t &x, int32_t &y, int32_t &w) {
    const uint8_t cols = g.cols ? g.cols : 1;
    const uint8_t row  = (uint8_t)(i / cols);
    const uint8_t col  = (uint8_t)(i % cols);
    const bool    last = (row + 1 == g.rows) && (n % cols != 0);
    w = last ? g.lastW : g.tileW;
    x = col * (w + gap);
    y = row * (g.tileH + gap);
}

ClusterSpan clusterSpan(uint8_t n, const ClusterGeom &g, uint8_t maxCols, uint8_t maxRows) {
    ClusterSpan out;
    if (n == 0 || maxCols == 0 || maxRows == 0) return out;
    const uint16_t most = (uint16_t)maxCols * maxRows;
    for (uint16_t a = 1; a <= most; a++) {
        // The wider block first: a cluster reads across, and the rows are short.
        for (uint8_t c = maxCols; c >= 1; c--) {
            if (a % c) continue;
            const uint16_t r = a / c;
            if (r > maxRows) continue;
            if (a == 1 && !g.oneCell) continue;
            const int32_t bw = c * g.cellW + (c - 1) * g.gap - g.chromeW;
            const int32_t bh = (int32_t)r * g.cellH + ((int32_t)r - 1) * g.gap - g.chromeH;
            const TileGrid t = tileGrid(n, bw, bh, g.tileGap);
            if (!t.tileW || !t.tileH) continue;
            int32_t m = t.tileW < t.tileH ? t.tileW : t.tileH;
            if (t.lastW && t.lastW < m) m = t.lastW;
            if (m < g.minTile) continue;
            out.cols = c;
            out.rows = (uint8_t)r;
            out.fits = true;
            return out;
        }
    }
    return out;
}

}  // namespace Layout
