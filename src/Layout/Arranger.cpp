// The arranger (2.11): include/Layout/Arranger.h says what and why. A
// line-for-line port of Layout Playground v9's arrangeV9() with its shapesV9()
// (v8's for clusters), searchV8(), scoreV9(), refSizes() and the finish,
// finishV9() (the "Layout Playground" artifact, its page v11, 2026-10-26) -
// doubles, the same order of loops, sums and tie-breaks, so its answers are the
// playground's.
#include "Layout/Arranger.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

namespace Layout {

namespace {

// The device's cluster tokens, as the playground has them (UIShape C_HDR_H,
// C_PAD, C_TILE_GAP - mm); Layout includes no UI header.
constexpr double HDR = 5.2, PAD = 1.1, TG = 0.8;
constexpr double SQ_HI = 1.12, SQ_LO = 0.85;   // the finish: near square, and no flatter

double jsRound(double x) { return floor(x + 0.5); }   // JavaScript's Math.round
double sq(double x) { return x * x; }

struct Grid {
    uint8_t cols, rows;   // units
    double  cwmm, chmm;   // a unit, mm
};

// A shape for one block on one grid. `other` is what scoreV8() adds as "shape":
// a card's squareness, a cluster's tile shape, empty tile slots and width.
struct Shape {
    uint8_t w, h, tc, tr, n;   // n: a cluster's tiles; 0 for a card
    double  cost, size, other;
};

Grid mk(const ArrScreen &s, uint8_t C, uint8_t R) {
    return { C, R, (s.aw - (C - 1) * s.gap) / C / s.pmm, (s.ah - (R - 1) * s.gap) / R / s.pmm };
}

double spanW(const ArrScreen &s, const Grid &g, int w) { return w * g.cwmm + (w - 1) * s.gap / s.pmm; }
double spanH(const ArrScreen &s, const Grid &g, int h) { return h * g.chmm + (h - 1) * s.gap / s.pmm; }

// What a solo card's label takes from its box (the playground's insetForTag()), px.
void insetForTag(const ArrScreen &s, double &w, double &h) {
    const double t = s.tagH;
    switch (s.label) {
        case ArrLabel::LABEL_FLOAT:    w -= jsRound(t * 0.25); h -= jsRound(t * 0.5); break;
        case ArrLabel::LABEL_ATTACHED: h -= jsRound(t * 0.72); break;
        case ArrLabel::LABEL_FOLDER:   h -= t; break;
        default: break;
    }
}

// A solo card's body, its label paid (mm).
void soloBody(const ArrScreen &s, double wmm, double hmm, double &bw, double &bh) {
    double w = wmm * s.pmm, h = hmm * s.pmm;
    insetForTag(s, w, h);
    const double band = s.label == ArrLabel::LABEL_BAR ? jsRound(s.tagH * 1.15) : 0.0;
    bw = w / s.pmm;
    bh = (h - band) / s.pmm;
}

// The board's grid and its neighbours, in units (the playground's gridListV8()).
// A pinned count (the page's own Col / Row) is the one count tried that way; the
// playground pins both or neither, and with both this is its one grid. A grid of
// more than ARR_CELLS_MAX units is skipped: the search's depth is its units.
uint8_t gridList(const ArrScreen &s, Grid *out, uint8_t cap) {
    if (s.fixCols && s.fixRows) {
        const int C = 2 * s.fixCols, R = 2 * s.fixRows;
        if (C > ARR_UNITS_MAX || R > ARR_UNITS_MAX || C * R > ARR_CELLS_MAX) return 0;
        out[0] = mk(s, (uint8_t)C, (uint8_t)R);
        return 1;
    }
    const int rc0 = s.recCols ? s.recCols : 4, rc1 = s.recRows ? s.recRows : 3;
    const int c0 = s.fixCols ? 2 * s.fixCols : 2 * (rc0 - 1 > 1 ? rc0 - 1 : 1);
    const int c1 = s.fixCols ? 2 * s.fixCols : 2 * (rc0 + 2);
    const int r0 = s.fixRows ? 2 * s.fixRows : 2 * (rc1 - 1 > 1 ? rc1 - 1 : 1);
    const int r1 = s.fixRows ? 2 * s.fixRows : 2 * (rc1 + 1);
    uint8_t n = 0;
    for (int C = c0; C <= c1; C++)
        for (int R = r0; R <= r1; R++) {
            if (C > ARR_UNITS_MAX || R > ARR_UNITS_MAX || C * R > ARR_CELLS_MAX) continue;
            const Grid g = mk(s, (uint8_t)C, (uint8_t)R);
            if (g.cwmm < 3 || g.chmm < 3) continue;
            if (n < cap) out[n++] = g;
        }
    return n;
}

// Is a before b in the playground's sort: cost, then area (a stable sort).
bool before(const Shape &a, const Shape &b) {
    return a.cost < b.cost || (a.cost == b.cost && a.w * a.h < b.w * b.h);
}

// A block's shapes on grid g, cheapest first, at most ARR_SHAPES_MAX (the
// playground's shapesV8()). `all` is room for every shape before the dominated
// ones go.
uint8_t shapes(const ArrScreen &s, const ArrWeights &W, const ArrRef &ref, const Grid &g, const ArrBlock &b,
               double L, Shape *out, Shape *all) {
    uint16_t na = 0;
    if (!b.cluster) {
        // v9 (shapesV9()): its preferred shape A stretches its limits and is what
        // square means; its size is taken at that shape; bigger than the target
        // pays BIG on top.
        // A LOOSE BOX (round 12): taller than the card may be, up to half as much
        // again - the card is drawn at its tallest allowed shape, centred
        // (finishPage()), and the height it leaves pays as a hole. A pinned page
        // with few rows (the 4B at 4 x 3) otherwise had no page at all.
        const double A = b.aspect > 0 ? b.aspect : 1.0;
        for (int w = 1; w <= g.cols; w++)
            for (int h = 1; h <= g.rows; h++) {
                const double wmm = spanW(s, g, w), hmm = spanH(s, g, h);
                const double hd = fmin(hmm, wmm * ref.tall / A);
                if (wmm * hd < ref.soloW * ref.soloH * A * 0.98 || hmm / wmm > ref.tall / A * 1.5 || wmm / hmm > 1.7 * A)
                    continue;
                double bw, bh;
                soloBody(s, wmm, hd, bw, bh);
                const double size = sqrt(bw * bh / A);
                const double lr = log(size / L);
                const double waste = hmm - hd > 0.01 ? W.gap8 * wmm * (hmm - hd) / (L * L) : 0.0;
                const double sizeC = W.size * sq(lr) + W.big * sq(lr > 0 ? lr : 0.0);
                const double sqC = W.square * sq(log(hd / wmm * A)) * 4 + waste;
                all[na++] = { (uint8_t)w, (uint8_t)h, 0, 0, 0, sizeC + sqC, size, sqC };
            }
    } else {
        const int n = b.tiles;
        if (!n) return 0;
        for (int w = 1; w <= g.cols; w++)
            for (int h = 1; h <= g.rows; h++) {
                const double wmm = spanW(s, g, w), hmm = spanH(s, g, h);
                bool have = false;
                Shape best = {};
                for (int tc = 1; tc <= n; tc++) {
                    const int tr = (n + tc - 1) / tc;
                    if (tc * tr - n >= tc) continue;
                    const double tw = (wmm - 2 * PAD - (tc - 1) * TG) / tc;
                    const double th = (hmm - HDR - 2 * PAD - (tr - 1) * TG) / tr;
                    if (tw < ref.tileW * 0.98 || th < ref.tileH * 0.98) continue;
                    const double size = sqrt(tw * th);
                    const double sizeC = n * W.size * sq(log(size / L)), tshC = n * W.square * 0.4 * sq(log(th / tw));
                    const double slotC = (tc * tr - n) * 8;
                    // Rule 2: a step taller than wide is charged, more the taller (landscape pages);
                    // and (round 12, the owner: "I would always prefer horizontal to vertical
                    // groups") tiles stacked more rows than columns pay WIDE again, by how many more.
                    const double lw = log(hmm / wmm), lt = log((double)tr / tc);
                    const double wideC = W.wide * (lw > 0 ? lw : 0.0) * 2 + W.wide * (lt > 0 ? lt : 0.0) * 2;
                    const double cost = sizeC + tshC + slotC + wideC;
                    if (!have || cost < best.cost) {
                        have = true;
                        best = { (uint8_t)w, (uint8_t)h, (uint8_t)tc, (uint8_t)tr, (uint8_t)n, cost, size,
                                 tshC + slotC + wideC };
                    }
                }
                if (have) all[na++] = best;
            }
    }
    // As v5: a shape that costs more than a smaller one inside it plus the empty
    // units between is dropped. Then the cheapest first, ties by area, kept in
    // the order made (the playground's sort is stable).
    // THE TIGHTEST SHAPE ALWAYS STAYS (round 12), after the ten: with only the
    // cheapest, a pinned page whose blocks needed their smallest shapes had no page
    // (the 4B at 4 x 3 to 4 x 5 - its clusters kept only shapes four units tall).
    // Ties: the cheapest, then the first made.
    const double hole1 = W.gap8 * (g.cwmm * g.chmm) / (L * L);
    uint8_t n = 0;
    bool haveTight = false;
    Shape tight = {};
    for (uint16_t i = 0; i < na; i++) {
        const Shape &sh = all[i];
        bool dominated = false;
        for (uint16_t j = 0; j < na && !dominated; j++) {
            if (j == i) continue;
            const Shape &t = all[j];
            if (t.w <= sh.w && t.h <= sh.h && t.cost + (sh.w * sh.h - t.w * t.h) * hole1 <= sh.cost) dominated = true;
        }
        if (dominated) continue;
        const int area = sh.w * sh.h;
        if (!haveTight || area < tight.w * tight.h || (area == tight.w * tight.h && sh.cost < tight.cost)) {
            haveTight = true;
            tight = sh;
        }
        // Into the sorted list, after every shape not after it; only the first ten stay.
        uint8_t at = n;
        while (at > 0 && before(sh, out[at - 1])) at--;
        if (at >= ARR_SHAPES_CHEAP) continue;
        const uint8_t last = n < ARR_SHAPES_CHEAP ? n : (uint8_t)(ARR_SHAPES_CHEAP - 1);
        for (uint8_t k = last; k > at; k--) out[k] = out[k - 1];
        out[at] = sh;
        if (n < ARR_SHAPES_CHEAP) n++;
    }
    if (haveTight) {
        bool in = false;
        for (uint8_t k = 0; k < n && !in; k++) in = out[k].w == tight.w && out[k].h == tight.h;
        if (!in) out[n++] = tight;
    }
    return n;
}

// The search's state for one grid and pass, and the room arrange() works in.
// RECURSIVE, a level a unit at most - placed or left empty - so its stack is
// ~100 bytes x the grid's units: PageArranger's task has room for that.
constexpr uint8_t GRIDS_MAX = 64;   // the playground's list is 7 x 5 at most
struct Search {
    Grid     grids[GRIDS_MAX];
    Shape    all[ARR_UNITS_MAX * ARR_UNITS_MAX];
    const ArrWeights *W;
    const ArrBlock   *blocks;
    uint8_t  n, cols, rows;
    uint16_t cells;
    double   hole1;
    Shape    opts[ARR_BLOCKS_MAX][ARR_SHAPES_MAX];
    uint8_t  nopt[ARR_BLOCKS_MAX];
    int32_t  minA[ARR_BLOCKS_MAX], maxA[ARR_BLOCKS_MAX];
    double   minC[ARR_BLOCKS_MAX];
    int32_t  remMin, remMax;
    double   remC;
    uint32_t occ[ARR_UNITS_MAX];   // a row of units, a bit each
    bool     placed[ARR_BLOCKS_MAX];
    Shape    pos[ARR_BLOCKS_MAX], best[ARR_BLOCKS_MAX];
    uint8_t  pc[ARR_BLOCKS_MAX], pr[ARR_BLOCKS_MAX], bc[ARR_BLOCKS_MAX], br[ARR_BLOCKS_MAX];
    int32_t  free;
    uint32_t nodes;
    double   bestCost;
    bool     found;

    // W.twin x |log(area / area)|, kept: the search asks for the same few pairs
    // millions of times, and the P4's doubles are done in software. The value is
    // the playground's expression exactly, so nothing changes but the time.
    struct Twin { uint32_t key; double v; };
    Twin     twin[512];

    double twinOf(int a, int b) {
        const uint32_t key = ((uint32_t)a << 12 | (uint32_t)b) + 1;
        Twin &t = twin[(a * 31 + b) & 511];
        if (t.key != key) { t.key = key; t.v = W->twin * fabs(log((double)a / (double)b)); }
        return t.v;
    }

    bool fits(int r, int c, const Shape &s) const {
        if (c + s.w > cols || r + s.h > rows) return false;
        const uint32_t m = ((1u << s.w) - 1) << c;
        for (int y = r; y < r + s.h; y++) if (occ[y] & m) return false;
        return true;
    }
    void mark(int r, int c, const Shape &s, bool on) {
        const uint32_t m = ((1u << s.w) - 1) << c;
        for (int y = r; y < r + s.h; y++) occ[y] = on ? (occ[y] | m) : (occ[y] & ~m);
    }
    // Rule 3, as each cluster goes down: against those placed with as many tiles.
    double twinC(int j, const Shape &s) {
        if (!blocks[j].cluster) return 0;
        double c = 0;
        for (int k = 0; k < n; k++)
            if (placed[k] && blocks[k].cluster && pos[k].n == s.n) c += twinOf(s.w * s.h, pos[k].w * pos[k].h);
        return c;
    }

    void rec(double cost, uint8_t done, uint16_t from) {
        if (++nodes > W->budget) return;
        const int32_t sure = free - remMax;
        if (cost + remC + (sure > 0 ? sure : 0) * hole1 >= bestCost) return;
        if (done == n) {
            const double total = cost + free * hole1;
            if (total < bestCost) {
                bestCost = total;
                found = true;
                memcpy(best, pos, sizeof(Shape) * n);
                memcpy(bc, pc, n);
                memcpy(br, pr, n);
            }
            return;
        }
        uint16_t f = from;
        while (f < cells && ((occ[f / cols] >> (f % cols)) & 1u)) f++;
        if (f >= cells || free < remMin) return;
        const int r = f / cols, c = f % cols;
        uint8_t seen = 0, skipped = 0;
        for (int j = 0; j < n && seen < ARR_WINDOW; j++) {
            if (placed[j]) continue;
            seen++;
            for (uint8_t k = 0; k < nopt[j]; k++) {
                const Shape &s = opts[j][k];
                if (!fits(r, c, s)) continue;
                const int32_t a = s.w * s.h;
                const double tw = twinC(j, s);
                mark(r, c, s, true);
                placed[j] = true;
                free -= a; remMin -= minA[j]; remMax -= maxA[j]; remC -= minC[j];
                pos[j] = s; pc[j] = (uint8_t)c; pr[j] = (uint8_t)r;
                rec(cost + s.cost + tw + skipped * W->order, (uint8_t)(done + 1), (uint16_t)(f + 1));
                mark(r, c, s, false);
                placed[j] = false;
                free += a; remMin += minA[j]; remMax += maxA[j]; remC += minC[j];
            }
            skipped++;
        }
        // Or leave this unit empty; one late on the page costs a hair less.
        if (free - 1 >= remMin) {
            occ[r] |= 1u << c;
            free--;
            rec(cost + hole1 + (cells - f) * 0.001, done, (uint16_t)(f + 1));
            occ[r] &= ~(1u << c);
            free++;
        }
    }
};

// ~40 KB, made once from the heap - never on the caller's stack, and never in an
// S3's scarce static internal RAM (a large allocation goes to PSRAM there).
Search *s_search = nullptr;

// A search's answer, scored (the playground's scoreV8()).
struct Score { double mean, total; uint16_t holes; };

Score score(const ArrWeights &W, const Grid &g, const Shape *pos, const uint8_t *pc, const uint8_t *pr,
            const ArrBlock *blocks, uint8_t n) {
    // The entities' sizes, in the order the playground lists them.
    double lsum = 0;
    int    cnt = 0;
    for (uint8_t i = 0; i < n; i++) {
        const int k = pos[i].n ? pos[i].n : 1;
        for (int e = 0; e < k; e++) { lsum += log(pos[i].size); cnt++; }
    }
    const double mean = exp(lsum / cnt);
    double sizeC = 0, other = 0, twin = 0;
    for (uint8_t i = 0; i < n; i++) {
        const int k = pos[i].n ? pos[i].n : 1;
        for (int e = 0; e < k; e++) sizeC += W.size * sq(log(pos[i].size / mean));
    }
    for (uint8_t i = 0; i < n; i++) other += pos[i].other;
    for (uint8_t i = 0; i < n; i++)
        for (uint8_t j = 0; j < n; j++)
            if (i < j && blocks[i].cluster && blocks[j].cluster && pos[i].n == pos[j].n)
                twin += W.twin * fabs(log((double)(pos[i].w * pos[i].h) / (double)(pos[j].w * pos[j].h)));
    int used = 0;
    for (uint8_t i = 0; i < n; i++) used += pos[i].w * pos[i].h;
    const int holes = g.cols * g.rows - used;
    const double holeC = W.gap8 * holes * (g.cwmm * g.chmm) / (mean * mean);
    int inv = 0;
    for (uint8_t i = 0; i < n; i++)
        for (uint8_t j = 0; j < n; j++)
            if (i < j && pr[i] * g.cols + pc[i] > pr[j] * g.cols + pc[j]) inv++;
    const double orderC = W.order * inv;
    double total = sizeC + other + twin + holeC + orderC;
    // v9 (scoreV9()): each solo card bigger than the entities' size, on top.
    double big = 0;
    for (uint8_t i = 0; i < n; i++) {
        if (blocks[i].cluster) continue;
        const double lr = log(pos[i].size / mean);
        big += W.big * sq(lr > 0 ? lr : 0.0);
    }
    total += big;
    return { mean, total, (uint16_t)holes };
}

// Groups of blocks for the finish: each group's blocks, by index.
struct Groups {
    uint8_t m[ARR_BLOCKS_MAX][ARR_BLOCKS_MAX];
    uint8_t len[ARR_BLOCKS_MAX];
    uint8_t n;
};

// The blocks in `idx` grouped where their [lo, hi) overlap, transitively, in
// order of lo, ties by index (the playground's overlapGroups()).
void overlapGroups(const uint8_t *idx, uint8_t k, const double *lo, const double *hi, Groups &g) {
    uint8_t ix[ARR_BLOCKS_MAX];
    for (uint8_t i = 0; i < k; i++) {
        const uint8_t v = idx[i];
        uint8_t at = i;
        while (at > 0 && (lo[v] < lo[ix[at - 1]] || (lo[v] == lo[ix[at - 1]] && v < ix[at - 1]))) {
            ix[at] = ix[at - 1];
            at--;
        }
        ix[at] = v;
    }
    g.n = 0;
    double end = -INFINITY;
    for (uint8_t j = 0; j < k; j++) {
        const uint8_t i = ix[j];
        if (g.n && lo[i] < end) {
            g.m[g.n - 1][g.len[g.n - 1]++] = i;
            if (hi[i] > end) end = hi[i];
        } else {
            g.m[g.n][0] = i;
            g.len[g.n] = 1;
            g.n++;
            end = hi[i];
        }
    }
}

// The groups spread along one axis in `room` (the playground's spreadAround()):
// the spare in n parts, half of one at each edge, a whole one between, each group
// moved as one. `p` is the blocks' start on that axis, `len` their length.
void spreadAround(const Groups &g, double *p, const double *len, double room, double gap) {
    const uint8_t n = g.n;
    if (!n) return;
    double e0[ARR_BLOCKS_MAX], e1[ARR_BLOCKS_MAX];
    for (uint8_t k = 0; k < n; k++) {
        e0[k] = INFINITY;
        e1[k] = -INFINITY;
        for (uint8_t j = 0; j < g.len[k]; j++) {
            const uint8_t i = g.m[k][j];
            if (p[i] < e0[k]) e0[k] = p[i];
            if (p[i] + len[i] > e1[k]) e1[k] = p[i] + len[i];
        }
    }
    double used = (n - 1) * gap;
    for (uint8_t k = 0; k < n; k++) used += e1[k] - e0[k];
    // Round 12: a page filled to the pixel (clusters grown into the spare height)
    // is still packed - only one that overflows stays where the units put it.
    const double spare = room - used;
    if (spare < -0.5) return;
    const double s = (spare > 0 ? spare : 0.0) / n;
    double at = s / 2;
    for (uint8_t k = 0; k < n; k++) {
        const double d = at - e0[k];
        for (uint8_t j = 0; j < g.len[k]; j++) p[g.m[k][j]] += d;
        at += e1[k] - e0[k] + gap + s;
    }
}

}  // namespace

// THE FINISH (the playground's finishV9()). 1. v7's trim, in units: each solo
// card, in the blocks' order, gives up units at its top while that brings it
// nearer its own shape's square (never flatter than SQ_LO), units in pairs across
// while far too flat. 2. In pixels, the spare room shared out: blocks whose rows
// overlap make a BAND, and bands spread down the page; in each band, blocks whose
// columns overlap make a COLUMN, columns spread across it, and each column is
// centred in its band's height. Before the spreading (round 12): a loose card is
// drawn at its tallest allowed shape, and clusters grow into the spare height.
void finishPage(const ArrScreen &s, const ArrBlock *blocks, uint8_t n, uint8_t cols, uint8_t rows,
                const ArrPlace *pos, ArrPlace *box, ArrRect *rect) {
    if (!n || n > ARR_BLOCKS_MAX || !cols || !rows) return;
    const Grid g = mk(s, cols, rows);
    const double sx = g.cwmm * s.pmm + s.gap, sy = g.chmm * s.pmm + s.gap;
    auto tall = [&](const ArrPlace &b, double A) {
        double w = b.w * sx - s.gap, h = b.h * sy - s.gap;
        insetForTag(s, w, h);
        return h / w * A;
    };
    for (uint8_t i = 0; i < n; i++) box[i] = pos[i];
    for (uint8_t i = 0; i < n; i++) {
        if (blocks[i].cluster) continue;
        const double A = blocks[i].aspect > 0 ? blocks[i].aspect : 1.0;
        ArrPlace &b = box[i];
        while (b.h > 1 && tall(b, A) > SQ_HI) {
            ArrPlace t = b;
            t.r++; t.h--;
            if (tall(t, A) < SQ_LO || fabs(log(tall(t, A))) >= fabs(log(tall(b, A)))) break;
            b = t;
        }
        while (b.w > 2 && tall(b, A) < SQ_LO) {
            ArrPlace t = b;
            t.c++; t.w -= 2;
            if (tall(t, A) > SQ_HI || fabs(log(tall(t, A))) >= fabs(log(tall(b, A)))) break;
            b = t;
        }
    }
    // ~2.4 KB of room, from the heap: the arranger's task and the LVGL thread
    // (a kept answer finished at a build) both come here.
    struct Work {
        double  x[ARR_BLOCKS_MAX], y[ARR_BLOCKS_MAX], w[ARR_BLOCKS_MAX], h[ARR_BLOCKS_MAX];
        double  hiY[ARR_BLOCKS_MAX], hiX[ARR_BLOCKS_MAX];
        uint8_t all[ARR_BLOCKS_MAX];
        Groups  bands, colsG;
    };
    Work *wk = (Work *)calloc(1, sizeof(Work));
    if (!wk) {
        for (uint8_t i = 0; i < n; i++) rect[i] = { 0, 0, 0, 0 };
        return;
    }
    double *x = wk->x, *y = wk->y, *w = wk->w, *h = wk->h, *hiY = wk->hiY, *hiX = wk->hiX;
    uint8_t *all = wk->all;
    Groups &bands = wk->bands, &colsG = wk->colsG;
    for (uint8_t i = 0; i < n; i++) {
        x[i] = box[i].c * sx;
        y[i] = box[i].r * sy;
        w[i] = box[i].w * sx - s.gap;
        h[i] = box[i].h * sy - s.gap;
        all[i] = i;
    }
    // A loose card's box (shapes()) is drawn at its tallest allowed shape, centred.
    const ArrRef ref = arrRef();
    for (uint8_t i = 0; i < n; i++) {
        if (blocks[i].cluster) continue;
        const double hd = w[i] * ref.tall / (blocks[i].aspect > 0 ? blocks[i].aspect : 1.0);
        if (h[i] > hd + 0.5) {
            y[i] += (h[i] - hd) / 2;
            h[i] = hd;
        }
    }
    for (uint8_t i = 0; i < n; i++) hiY[i] = y[i] + h[i];
    overlapGroups(all, n, y, hiY, bands);
    // CLUSTERS GROW INTO THE SPARE HEIGHT FIRST (round 12, the owner: "the clusters
    // should expand vertically to fill the space and leave the solo cards the size
    // they currently are"): each band holding a cluster takes an equal share of the
    // page's spare height, each cluster in it growing to its band's height plus that
    // share - never past square tiles. What is left is spread: "around the blocks,
    // but only when that would not make the blocks too small".
    double usedH = (bands.n - 1) * s.gap;
    uint8_t grows = 0;
    double bb0[ARR_BLOCKS_MAX], bb1[ARR_BLOCKS_MAX];
    for (uint8_t k = 0; k < bands.n; k++) {
        bb0[k] = INFINITY;
        bb1[k] = -INFINITY;
        bool cl = false;
        for (uint8_t j = 0; j < bands.len[k]; j++) {
            const uint8_t i = bands.m[k][j];
            if (y[i] < bb0[k]) bb0[k] = y[i];
            if (y[i] + h[i] > bb1[k]) bb1[k] = y[i] + h[i];
            cl = cl || blocks[i].cluster;
        }
        usedH += bb1[k] - bb0[k];
        if (cl) grows++;
    }
    const double spareH = s.ah - usedH;
    if (spareH > 0 && grows) {
        const double share = spareH / grows;
        for (uint8_t k = 0; k < bands.n; k++)
            for (uint8_t j = 0; j < bands.len[k]; j++) {
                const uint8_t i = bands.m[k][j];
                if (!blocks[i].cluster) continue;
                // How much taller it may grow before its tiles are square, px.
                // A place without its tiles down (an older caller) has them from across.
                const int tc = pos[i].tc ? pos[i].tc : 1;
                const int tr = pos[i].tr ? pos[i].tr : (blocks[i].tiles + tc - 1) / tc;
                const double wmm = w[i] / s.pmm, hmm = h[i] / s.pmm;
                const double tw = (wmm - 2 * PAD - (tc - 1) * TG) / tc;
                const double th = (hmm - HDR - 2 * PAD - (tr - 1) * TG) / (tr ? tr : 1);
                const double room = fmax(0.0, (tw - th) * tr * s.pmm);
                h[i] += fmax(0.0, fmin(bb1[k] - bb0[k] + share - h[i], room));
            }
    }
    spreadAround(bands, y, h, s.ah, s.gap);
    for (uint8_t k = 0; k < bands.n; k++) {
        const uint8_t *bd = bands.m[k];
        const uint8_t bn = bands.len[k];
        for (uint8_t j = 0; j < bn; j++) hiX[bd[j]] = x[bd[j]] + w[bd[j]];
        overlapGroups(bd, bn, x, hiX, colsG);
        spreadAround(colsG, x, w, s.aw, s.gap);
        double b0 = INFINITY, b1 = -INFINITY;
        for (uint8_t j = 0; j < bn; j++) {
            if (y[bd[j]] < b0) b0 = y[bd[j]];
            if (y[bd[j]] + h[bd[j]] > b1) b1 = y[bd[j]] + h[bd[j]];
        }
        for (uint8_t c = 0; c < colsG.n; c++) {
            double c0 = INFINITY, c1 = -INFINITY;
            for (uint8_t j = 0; j < colsG.len[c]; j++) {
                const uint8_t i = colsG.m[c][j];
                if (y[i] < c0) c0 = y[i];
                if (y[i] + h[i] > c1) c1 = y[i] + h[i];
            }
            const double d = (b0 + b1) / 2 - (c0 + c1) / 2;
            for (uint8_t j = 0; j < colsG.len[c]; j++) y[colsG.m[c][j]] += d;
        }
    }
    for (uint8_t i = 0; i < n; i++)
        rect[i] = { (int32_t)jsRound(x[i]), (int32_t)jsRound(y[i]), (int32_t)jsRound(w[i]), (int32_t)jsRound(h[i]) };
    free(wk);
}

ArrRef arrRef() {
    // The owner's two references, in the playground's own geometry: one card of
    // the P4_5 at 7 x 3, and a tile of a 3-wide one-cell cluster on the 4B at 4 x 4.
    struct Geo { double pmm, aw, ah, gap; };
    auto geo = [](double w, double h, double diag) {
        const double pmm = hypot(w, h) / diag / 25.4;
        const double hdrH = jsRound(5 * pmm), inset = jsRound(2.5 * pmm), gap = jsRound(1.8 * pmm);
        return Geo{ pmm, w - 2 * inset, h - hdrH - 2 * inset, gap };
    };
    const Geo g5 = geo(1280, 720, 5.0), g4 = geo(720, 720, 4.0);
    const double cw = (g5.aw - 6 * g5.gap) / 7 / g5.pmm, ch = (g5.ah - 2 * g5.gap) / 3 / g5.pmm;
    const double c4w = (g4.aw - 3 * g4.gap) / 4 / g4.pmm, c4h = (g4.ah - 3 * g4.gap) / 4 / g4.pmm;
    const double clw = 3 * c4w + 2 * g4.gap / g4.pmm;
    return { cw, ch, ch / cw, (clw - 2 * PAD - 2 * TG) / 3, c4h - HDR - 2 * PAD };
}

bool arrange(const ArrScreen &scr, const ArrBlock *blocks, uint8_t n, const ArrWeights &W, ArrResult &out) {
    out = ArrResult();
    if (n == 0 || n > ARR_BLOCKS_MAX || scr.pmm <= 0) return false;
    if (!s_search) s_search = new Search();
    if (!s_search) return false;
    Search &S = *s_search;
    const ArrRef ref = arrRef();
    const uint8_t ng = gridList(scr, S.grids, GRIDS_MAX);
    int nEnt = 0;
    for (uint8_t i = 0; i < n; i++) nEnt += blocks[i].cluster ? blocks[i].tiles : 1;
    if (!nEnt) return false;
    // An equal share of the page, its headers paid roughly.
    const double L0 = sqrt((scr.aw / scr.pmm) * (scr.ah / scr.pmm) / (nEnt * 1.35));

    for (uint8_t gi = 0; gi < ng; gi++) {
        const Grid g = S.grids[gi];
        // Two passes: against the page's equal share, then against the average the first found.
        double L = L0;
        bool have = false;
        double haveScore = 0;
        Score haveSc = {};
        bool haveCapped = false;
        Shape hp[ARR_BLOCKS_MAX];
        uint8_t hc[ARR_BLOCKS_MAX], hr[ARR_BLOCKS_MAX];
        for (int pass = 0; pass < 2; pass++) {
            S.W = &W; S.blocks = blocks; S.n = n; S.cols = g.cols; S.rows = g.rows;
            S.cells = (uint16_t)(g.cols * g.rows);
            bool empty = false;
            for (uint8_t i = 0; i < n && !empty; i++) {
                S.nopt[i] = shapes(scr, W, ref, g, blocks[i], L, S.opts[i], S.all);
                if (!S.nopt[i]) empty = true;
            }
            if (empty) break;
            S.remMin = S.remMax = 0;
            S.remC = 0;
            for (uint8_t i = 0; i < n; i++) {
                int32_t mn = 1 << 30, mx = 0;
                double mc = 1e300;
                for (uint8_t j = 0; j < S.nopt[i]; j++) {
                    const int32_t a = S.opts[i][j].w * S.opts[i][j].h;
                    if (a < mn) mn = a;
                    if (a > mx) mx = a;
                    if (S.opts[i][j].cost < mc) mc = S.opts[i][j].cost;
                }
                S.minA[i] = mn; S.maxA[i] = mx; S.minC[i] = mc;
            }
            // Summed as the playground's reduce() does: in order, from 0.
            for (uint8_t i = 0; i < n; i++) S.remMin += S.minA[i];
            for (uint8_t i = 0; i < n; i++) S.remMax += S.maxA[i];
            for (uint8_t i = 0; i < n; i++) S.remC += S.minC[i];
            if (S.remMin > S.cells) break;
            S.hole1 = W.gap8 * (g.cwmm * g.chmm) / (L * L);
            memset(S.occ, 0, sizeof(S.occ));
            memset(S.placed, 0, sizeof(S.placed));
            memset(S.twin, 0, sizeof(S.twin));
            S.free = S.cells;
            S.nodes = 0;
            S.bestCost = INFINITY;
            S.found = false;
            S.rec(0, 0, 0);
            out.nodes += S.nodes;
            if (!S.found) break;
            const Score sc = score(W, g, S.best, S.bc, S.br, blocks, n);
            if (!have || sc.total < haveScore) {
                have = true;
                haveScore = sc.total;
                haveSc = sc;
                haveCapped = S.nodes > W.budget;
                memcpy(hp, S.best, sizeof(Shape) * n);
                memcpy(hc, S.bc, n);
                memcpy(hr, S.br, n);
            }
            if (fabs(log(sc.mean / L)) < 0.03) break;
            L = sc.mean;
        }
        if (!have) continue;
        if (!out.ok || haveScore < out.score) {
            out.ok = true;
            out.cols = g.cols; out.rows = g.rows;
            out.cwmm = g.cwmm; out.chmm = g.chmm;
            out.score = haveScore;
            out.mean = haveSc.mean;
            out.holes = haveSc.holes;
            out.capped = haveCapped;
            for (uint8_t i = 0; i < n; i++)
                out.pos[i] = { hc[i], hr[i], hp[i].w, hp[i].h, hp[i].tc, hp[i].tr };
        }
    }
    if (!out.ok) return false;
    finishPage(scr, blocks, n, out.cols, out.rows, out.pos, out.box, out.rect);
    return true;
}

}  // namespace Layout
