// The arranger (2.11): include/Layout/Arranger.h says what and why. A
// line-for-line port of Layout Playground v8's arrangeV8() with its shapesV8(),
// searchV8(), scoreV8(), refSizes() and the finish, finishWith() at a unit a
// step (the "Layout Playground" artifact, 2026-10-20) - doubles, the same order
// of loops, sums and tie-breaks, so its answers are the playground's.
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
        for (int w = 1; w <= g.cols; w++)
            for (int h = 1; h <= g.rows; h++) {
                const double wmm = spanW(s, g, w), hmm = spanH(s, g, h);
                if (wmm * hmm < ref.soloW * ref.soloH * 0.98 || hmm / wmm > ref.tall * 1.001 || wmm / hmm > 1.7) continue;
                double bw, bh;
                soloBody(s, wmm, hmm, bw, bh);
                const double size = sqrt(bw * bh);
                const double sizeC = W.size * sq(log(size / L)), sqC = W.square * sq(log(hmm / wmm)) * 4;
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
                    // Rule 2: a step taller than wide is charged, more the taller (landscape pages).
                    const double lw = log(hmm / wmm);
                    const double wideC = W.wide * (lw > 0 ? lw : 0.0) * 2;
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
    const double hole1 = W.gap8 * (g.cwmm * g.chmm) / (L * L);
    uint8_t n = 0;
    for (uint16_t i = 0; i < na; i++) {
        const Shape &sh = all[i];
        bool dominated = false;
        for (uint16_t j = 0; j < na && !dominated; j++) {
            if (j == i) continue;
            const Shape &t = all[j];
            if (t.w <= sh.w && t.h <= sh.h && t.cost + (sh.w * sh.h - t.w * t.h) * hole1 <= sh.cost) dominated = true;
        }
        if (dominated) continue;
        // Into the sorted list, after every shape not after it; only the first ten stay.
        uint8_t at = n;
        while (at > 0 && before(sh, out[at - 1])) at--;
        if (at >= ARR_SHAPES_MAX) continue;
        const uint8_t last = n < ARR_SHAPES_MAX ? n : (uint8_t)(ARR_SHAPES_MAX - 1);
        for (uint8_t k = last; k > at; k--) out[k] = out[k - 1];
        out[at] = sh;
        if (n < ARR_SHAPES_MAX) n++;
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
        if (++nodes > ARR_BUDGET) return;
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
    return { mean, sizeC + other + twin + holeC + orderC, (uint16_t)holes };
}

// THE FINISH (v7's, a unit a step): each solo card, in the blocks' order, gives
// up units at its top while that brings it nearer square (never flatter than
// SQ_LO), units in pairs across while far too wide, and then takes whole free
// unit rows above and below it as even margins - the odd unit below. Clusters
// stay as placed.
void finish(const ArrScreen &s, const Grid &g, const ArrBlock *blocks, uint8_t n, ArrPlace *box) {
    const int R = g.rows;
    const double sx = g.cwmm * s.pmm + s.gap, sy = g.chmm * s.pmm + s.gap;
    auto tall = [&](const ArrPlace &b) {
        double w = b.w * sx - s.gap, h = b.h * sy - s.gap;
        insetForTag(s, w, h);
        return h / w;
    };
    uint8_t occ[ARR_UNITS_MAX][ARR_UNITS_MAX] = {};
    auto mark = [&](const ArrPlace &b, uint8_t v) {
        for (int y = b.r; y < b.r + b.h; y++) for (int x = b.c; x < b.c + b.w; x++) occ[y][x] = v;
    };
    for (uint8_t i = 0; i < n; i++) mark(box[i], 1);
    for (uint8_t i = 0; i < n; i++) {
        if (blocks[i].cluster) continue;
        ArrPlace &b = box[i];
        const int r0 = b.r, h0 = b.h;
        mark(b, 0);
        while (b.h > 1 && tall(b) > SQ_HI) {
            ArrPlace t = b;
            t.r++; t.h--;
            if (tall(t) < SQ_LO || fabs(log(tall(t))) >= fabs(log(tall(b)))) break;
            b = t;
        }
        while (b.w > 2 && tall(b) < SQ_LO) {
            ArrPlace t = b;
            t.c++; t.w -= 2;
            if (tall(t) > SQ_HI || fabs(log(tall(t))) >= fabs(log(tall(b)))) break;
            b = t;
        }
        // Its own units are its room, and any whole unit rows free above or below
        // them. Up and down only: centred across a free row, a card wandered away
        // from its neighbours (the 4B).
        auto rowFree = [&](int y) {
            if (y < 0 || y >= R) return false;
            for (int x = b.c; x < b.c + b.w; x++) if (occ[y][x]) return false;
            return true;
        };
        int top = r0, bot = r0 + h0 - 1;
        while (rowFree(top - 1)) top--;
        while (rowFree(bot + 1)) bot++;
        b.r = (uint8_t)(top + (bot - top + 1 - b.h + 1) / 2);
        mark(b, 1);
    }
}

}  // namespace

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
                haveCapped = S.nodes > ARR_BUDGET;
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
    Grid g = { out.cols, out.rows, out.cwmm, out.chmm };
    memcpy(out.box, out.pos, sizeof(ArrPlace) * n);
    finish(scr, g, blocks, n, out.box);
    return true;
}

}  // namespace Layout
