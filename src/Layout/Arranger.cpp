// The arranger (2.11 round 2): include/Layout/Arranger.h says what and why. A
// line-for-line port of Layout Playground v5's arrangeV5() (the "Layout
// Playground" artifact, 2026-10-19) - doubles, the same order of loops and the
// same tie-breaks, so its answers are the playground's.
#include "Layout/Arranger.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

namespace Layout {

namespace {

struct Grid {
    uint8_t cols, rows;
    double  cwmm, chmm;
};

struct Shape {
    uint8_t w, h;
    bool    one;
    double  cost;
};

Grid mk(const ArrScreen &s, uint8_t c, uint8_t r) {
    return { c, r, (s.aw - (c - 1) * s.gap) / c / s.pmm, (s.ah - (r - 1) * s.gap) / r / s.pmm };
}

// The board's own grid and its neighbours (one row or column either way), each
// with cells inside the knobs' sizes - or the page's own grid when it is set.
uint8_t gridList(const ArrScreen &s, const ArrKnobs &k, Grid *out, uint8_t cap) {
    if (s.fixCols && s.fixRows) { out[0] = mk(s, s.fixCols, s.fixRows); return 1; }
    uint8_t n = 0;
    bool haveRec = false;
    for (uint8_t c = 1; c <= ARR_GRID_COLS; c++)
        for (uint8_t r = 1; r <= ARR_GRID_ROWS; r++) {
            const Grid g = mk(s, c, r);
            if (g.cwmm < k.minCell || g.chmm < k.minCell * 0.8 || g.cwmm > k.maxCell || g.chmm > k.maxCell) continue;
            const double asp = g.cwmm / g.chmm;
            if (asp < 0.7 || asp > 1.7) continue;
            if (s.recCols && (abs((int)c - s.recCols) > 1 || abs((int)r - s.recRows) > 1)) continue;
            if (n < cap) out[n++] = g;
            if (c == s.recCols && r == s.recRows) haveRec = true;
        }
    if (s.recCols && s.recRows && !haveRec && n < cap) out[n++] = mk(s, s.recCols, s.recRows);
    return n;
}

// How many of the smallest tiles a block of w x h cells holds, across and down.
void capacity(const ArrScreen &s, const ArrKnobs &k, const Grid &g, uint8_t w, uint8_t h, bool one,
              int32_t &across, int32_t &down) {
    const double pad = 1.2 * s.pmm, tg = 1.0 * s.pmm;
    // A one-cell cluster has no header and wears a card label, paid for inside
    // the cell as a card's floating tag is: up by half of it, in by a quarter.
    const double tx = one ? lround(s.tagH * 0.25) : 0.0, ty = one ? lround(s.tagH * 0.5) : 0.0;
    const double bw = w * (g.cwmm * s.pmm) + (w - 1) * s.gap - tx;
    const double bh = h * (g.chmm * s.pmm) + (h - 1) * s.gap - ty;
    across = (int32_t)floor((bw - 2 * pad + tg) / (k.tileW * s.pmm + tg));
    down   = (int32_t)floor((bh - (one ? 0.0 : k.hdr * s.pmm) - 2 * pad + tg) / (k.tileH * s.pmm + tg));
    if (across < 0) across = 0;
    if (down < 0) down = 0;
}

// A block's shapes on grid g, cheapest first (the playground's shapesV5()).
// `all` is room for every shape before the dominated ones are dropped.
uint8_t shapes(const ArrScreen &s, const ArrKnobs &k, const ArrWeights &W, const Grid &g, const ArrBlock &b,
               Shape *out, Shape *all) {
    if (!b.cluster) { out[0] = { 1, 1, false, 0.0 }; return 1; }
    uint16_t na = 0;
    const bool pinned = b.pinW && b.pinH;
    auto consider = [&](uint8_t w, uint8_t h) {
        if (w > g.cols || h > g.rows) return;
        // One cell: no header, every member a tile (K97) - only for a cluster
        // with nothing in its header (or when pinned there).
        const bool one = (w == 1 && h == 1);
        if (one && b.hdr && !pinned) return;
        const int32_t n = one ? b.members : b.tiles;
        int32_t across, down;
        capacity(s, k, g, w, h, one, across, down);
        if (across * down < n) return;
        const double wmm = w * g.cwmm + (w - 1) * s.gap / s.pmm;
        const double hmm = h * g.chmm + (h - 1) * s.gap / s.pmm;
        double inner = (wmm - 2.4) * (hmm - (one ? 1.5 : k.hdr) - 2.4);
        if (inner < 1.0) inner = 1.0;
        const double room = inner / (n * k.tileW * k.tileH);
        // Room beyond comfortable, by its square: spare cells spread over
        // clusters instead of piling up in one. Long thin clusters pay too.
        double excess = room / W.comfy - 1.0;
        if (excess < 0) excess = 0;
        double longness = fabs(log(wmm / hmm)) - log(1.8);
        if (longness < 0) longness = 0;
        all[na++] = { w, h, one, W.loose * excess * excess + W.lng * longness * longness };
    };
    if (pinned) consider(b.pinW, b.pinH);
    else for (uint8_t w = 1; w <= g.cols; w++) for (uint8_t h = 1; h <= g.rows; h++) consider(w, h);

    // Drop a shape that costs more than a smaller one inside it plus empty
    // cells for the difference.
    uint8_t n = 0;
    for (uint16_t i = 0; i < na; i++) {
        const Shape &sh = all[i];
        bool dominated = false;
        for (uint16_t j = 0; j < na && !dominated; j++) {
            if (j == i) continue;
            const Shape &t = all[j];
            if (t.w <= sh.w && t.h <= sh.h &&
                t.cost + (sh.w * sh.h - t.w * t.h) * W.hole <= sh.cost) dominated = true;
        }
        if (dominated || n >= ARR_SHAPES_MAX) continue;
        // Insert keeping the order stable: cost, then area, then height.
        uint8_t at = n;
        while (at > 0) {
            const Shape &p = out[at - 1];
            const bool after = p.cost < sh.cost ||
                               (p.cost == sh.cost && (p.w * p.h < sh.w * sh.h ||
                               (p.w * p.h == sh.w * sh.h && p.h <= sh.h)));
            if (after) break;
            out[at] = out[at - 1];
            at--;
        }
        out[at] = sh;
        n++;
    }
    return n;
}

// The search's state for one grid, and the room arrange() works in.
struct Search {
    Grid     grids[ARR_GRID_COLS * ARR_GRID_ROWS];
    Shape    all[ARR_GRID_COLS * ARR_GRID_ROWS];
    const ArrWeights *W;
    uint8_t  n, cols, rows;
    uint16_t cells;
    Shape    opts[ARR_BLOCKS_MAX][ARR_SHAPES_MAX];
    uint8_t  nopt[ARR_BLOCKS_MAX];
    int32_t  minA[ARR_BLOCKS_MAX], maxA[ARR_BLOCKS_MAX];
    double   minC[ARR_BLOCKS_MAX];
    int32_t  remMin, remMax;
    double   remC;
    int16_t  occ[ARR_GRID_COLS * ARR_GRID_ROWS];
    bool     placed[ARR_BLOCKS_MAX];
    ArrPlace pos[ARR_BLOCKS_MAX], best[ARR_BLOCKS_MAX];
    int32_t  free;
    uint32_t nodes;
    double   bestCost;
    bool     found;

    bool fits(uint8_t r, uint8_t c, const Shape &s) const {
        if (c + s.w > cols || r + s.h > rows) return false;
        for (uint8_t y = r; y < r + s.h; y++)
            for (uint8_t x = c; x < c + s.w; x++)
                if (occ[y * cols + x] != -1) return false;
        return true;
    }
    void mark(uint8_t r, uint8_t c, const Shape &s, int16_t v) {
        for (uint8_t y = r; y < r + s.h; y++)
            for (uint8_t x = c; x < c + s.w; x++) occ[y * cols + x] = v;
    }

    void rec(double cost, uint8_t done, uint16_t from) {
        if (++nodes > ARR_BUDGET) return;
        // Every block left costs at least its cheapest shape, and cells the rest
        // cannot cover even at their biggest are empty for sure.
        const int32_t sure = free - remMax;
        const double bound = cost + remC + (sure > 0 ? sure : 0) * W->hole;
        if (bound >= bestCost) return;
        if (done == n) {
            const double total = cost + free * W->hole;
            if (total < bestCost) {
                bestCost = total;
                found = true;
                memcpy(best, pos, sizeof(ArrPlace) * n);
            }
            return;
        }
        uint16_t f = from;
        while (f < cells && occ[f] != -1) f++;
        if (f >= cells || free < remMin) return;
        const uint8_t r = (uint8_t)(f / cols), c = (uint8_t)(f % cols);
        uint8_t seen = 0, skipped = 0;
        for (uint8_t j = 0; j < n && seen < ARR_WINDOW; j++) {
            if (placed[j]) continue;
            seen++;
            for (uint8_t k = 0; k < nopt[j]; k++) {
                const Shape &s = opts[j][k];
                if (!fits(r, c, s)) continue;
                const int32_t a = s.w * s.h;
                mark(r, c, s, j);
                placed[j] = true;
                free -= a; remMin -= minA[j]; remMax -= maxA[j]; remC -= minC[j];
                pos[j].c = c; pos[j].r = r; pos[j].w = s.w; pos[j].h = s.h; pos[j].one = s.one;
                rec(cost + s.cost + skipped * W->order, (uint8_t)(done + 1), (uint16_t)(f + 1));
                mark(r, c, s, -1);
                placed[j] = false;
                free += a; remMin += minA[j]; remMax += maxA[j]; remC += minC[j];
            }
            skipped++;
        }
        // Or leave this cell empty; a hole late on the page costs a hair less.
        if (free - 1 >= remMin) {
            occ[f] = -2;
            free--;
            rec(cost + W->hole + (cells - f) * 0.01, done, (uint16_t)(f + 1));
            occ[f] = -1;
            free++;
        }
    }
};

// ~17 KB, made once from the heap - never on the caller's stack, and never in
// an S3's scarce static internal RAM (a large allocation goes to PSRAM there).
Search *s_search = nullptr;

}  // namespace

bool arrange(const ArrScreen &scr, const ArrBlock *blocks, uint8_t n, const ArrWeights &W, const ArrKnobs &k,
             ArrResult &out) {
    out = ArrResult();
    if (n == 0 || n > ARR_BLOCKS_MAX || scr.pmm <= 0) return false;
    if (!s_search) s_search = new Search();
    if (!s_search) return false;
    Grid *grids = s_search->grids;
    const uint8_t ng = gridList(scr, k, grids, (uint8_t)(ARR_GRID_COLS * ARR_GRID_ROWS));
    uint8_t cards = 0;
    for (uint8_t i = 0; i < n; i++) if (!blocks[i].cluster) cards++;

    for (uint8_t gi = 0; gi < ng; gi++) {
        const Grid &g = grids[gi];
        if ((uint16_t)g.cols * g.rows > ARR_GRID_COLS * ARR_GRID_ROWS) continue;
        Search &S = *s_search;
        S.W = &W; S.n = n; S.cols = g.cols; S.rows = g.rows; S.cells = (uint16_t)(g.cols * g.rows);
        bool empty = false;
        S.remMin = S.remMax = 0;
        S.remC = 0;
        for (uint8_t i = 0; i < n && !empty; i++) {
            S.nopt[i] = shapes(scr, k, W, g, blocks[i], S.opts[i], S.all);
            if (!S.nopt[i]) { empty = true; break; }
            int32_t mn = 1 << 30, mx = 0;
            double mc = 1e300;
            for (uint8_t j = 0; j < S.nopt[i]; j++) {
                const Shape &sh = S.opts[i][j];
                const int32_t a = sh.w * sh.h;
                if (a < mn) mn = a;
                if (a > mx) mx = a;
                if (sh.cost < mc) mc = sh.cost;
            }
            S.minA[i] = mn; S.maxA[i] = mx; S.minC[i] = mc;
            S.remMin += mn; S.remMax += mx; S.remC += mc;
        }
        if (empty || S.remMin > S.cells) continue;

        // The grid's own cost: away from the board's grid, and single cards
        // drifting from today's ~20 mm card.
        const int32_t away = scr.recCols ? abs((int)g.cols - scr.recCols) + abs((int)g.rows - scr.recRows) : 0;
        const double cardC = cards * 10.0 * fabs(log(sqrt(g.cwmm * g.chmm) / 20.0));
        const double gc = W.away * away + cardC;

        for (uint16_t i = 0; i < S.cells; i++) S.occ[i] = -1;
        memset(S.placed, 0, sizeof(S.placed));
        S.free = S.cells;
        S.nodes = 0;
        S.bestCost = 1e300;
        S.found = false;
        S.rec(gc, 0, 0);
        out.nodes += S.nodes;
        if (S.nodes > ARR_BUDGET) out.capped = true;
        if (!S.found) continue;
        if (!out.ok || S.bestCost < out.score) {
            out.ok = true;
            out.cols = g.cols; out.rows = g.rows;
            out.cwmm = g.cwmm; out.chmm = g.chmm;
            out.score = S.bestCost;
            int32_t used = 0;
            for (uint8_t i = 0; i < n; i++) { out.pos[i] = S.best[i]; used += S.best[i].w * S.best[i].h; }
            out.holes = (uint16_t)(S.cells - used);
        }
    }
    return out.ok;
}

}  // namespace Layout
