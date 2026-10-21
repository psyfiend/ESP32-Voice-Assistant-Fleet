// The arranger (2.11, K109): Layout Playground v8's answers are the expected values.
// Two sets of blocks, each run with node on the playground's own code (its v10 page, the
// owner's knobs as its defaults; scratchpad run_v8.js and run_v8_pinned.js, 2026-10-20):
//   DEVICE - the House page as the boards show it since round 6: Doors as a card, then
//            Kitchen (4 tiles, 2 in the header, 6 members), Outside and Office (3 tiles, 2 in
//            the header, 5 members);
//   HOUSE  - the playground's own House page grouped by area: Garage (2 doors, no header),
//            Outside, Kitchen and Office (2 tiles, 2 in the header, 4 members), Living's
//            Overhead and Front's Thermo as cards.
// The screens are the playground's boards, its geometry() worked out for each (pmm, the page's
// room, the gap, the card label's height), Float labels. Every grid, place, the finish's boxes,
// score and empty units match.
#include <unity.h>
#include <math.h>
#include "Layout/Arranger.h"

using namespace Layout;

void setUp() {}
void tearDown() {}

struct Board {
    const char *id;
    double pmm, aw, ah, gap, tagH;
    uint8_t recC, recR;
    uint8_t cols, rows;   // the answer's grid, units
    double score;
    uint16_t holes;
    bool capped;
};
struct P6 { uint8_t c, r, w, h, tc, tr; };
struct P4 { uint8_t c, r, w, h; };

static ArrScreen screen(const Board &b) {
    ArrScreen s;
    s.pmm = b.pmm;
    s.aw = b.aw;
    s.ah = b.ah;
    s.gap = b.gap;
    s.tagH = b.tagH;
    s.label = ArrLabel::LABEL_FLOAT;
    s.recCols = b.recC;
    s.recRows = b.recR;
    return s;
}

static const ArrBlock DEVICE[4] = {
    { false, 0, 0, 1 },   // Doors
    { true, 4, 2, 6 },    // Kitchen
    { true, 3, 2, 5 },    // Outside
    { true, 3, 2, 5 },    // Office
};
static const ArrBlock HOUSE[6] = {
    { true, 2, 0, 2 },    // Garage: the two doors
    { true, 2, 2, 4 },    // Outside
    { true, 2, 2, 4 },    // Kitchen
    { true, 2, 2, 4 },    // Office
    { false, 0, 0, 1 },   // Living's Overhead
    { false, 0, 0, 1 },   // Front's Thermo
};

static void check(const ArrScreen &s, const ArrBlock *blocks, uint8_t n, uint8_t cols, uint8_t rows,
                  double score, uint16_t holes, bool capped, const P6 *pos, const P4 *fin) {
    ArrResult out;
    TEST_ASSERT_TRUE(arrange(s, blocks, n, ArrWeights(), out));
    TEST_ASSERT_EQUAL_UINT8(cols, out.cols);
    TEST_ASSERT_EQUAL_UINT8(rows, out.rows);
    TEST_ASSERT_TRUE(fabs(score - out.score) < 1e-6);   // Unity's doubles are off here
    TEST_ASSERT_EQUAL_UINT16(holes, out.holes);
    TEST_ASSERT_EQUAL(capped, out.capped);
    for (uint8_t i = 0; i < n; i++) {
        TEST_ASSERT_EQUAL_UINT8(pos[i].c, out.pos[i].c);
        TEST_ASSERT_EQUAL_UINT8(pos[i].r, out.pos[i].r);
        TEST_ASSERT_EQUAL_UINT8(pos[i].w, out.pos[i].w);
        TEST_ASSERT_EQUAL_UINT8(pos[i].h, out.pos[i].h);
        TEST_ASSERT_EQUAL_UINT8(pos[i].tc, out.pos[i].tc);
        TEST_ASSERT_EQUAL_UINT8(pos[i].tr, out.pos[i].tr);
        TEST_ASSERT_EQUAL_UINT8(fin[i].c, out.box[i].c);
        TEST_ASSERT_EQUAL_UINT8(fin[i].r, out.box[i].r);
        TEST_ASSERT_EQUAL_UINT8(fin[i].w, out.box[i].w);
        TEST_ASSERT_EQUAL_UINT8(fin[i].h, out.box[i].h);
    }
}
static void check(const Board &b, const ArrBlock *blocks, uint8_t n, const P6 *pos, const P4 *fin) {
    check(screen(b), blocks, n, b.cols, b.rows, b.score, b.holes, b.capped, pos, fin);
}

// The owner's two references, in the playground's geometry (its refSizes()).
void test_references() {
    const ArrRef r = arrRef();
    TEST_ASSERT_TRUE(fabs(r.soloW - 13.539770330741101) < 1e-9);
    TEST_ASSERT_TRUE(fabs(r.soloH - 16.19995635071031) < 1e-9);
    TEST_ASSERT_TRUE(fabs(r.tall - 1.1964720194647203) < 1e-9);
    TEST_ASSERT_TRUE(fabs(r.tileW - 15.296916845527548) < 1e-9);
    TEST_ASSERT_TRUE(fabs(r.tileH - 6.718958234792058) < 1e-9);
}

// ---- DEVICE ----------------------------------------------------------------

// WS_P4_5: Kitchen 8 x 4 units across the top, Outside and Office 5 x 4 each below - the
// owner's "what should happen"; Doors 2 x 3 with a unit of margin above it.
void test_device_p4_5() {
    const Board b = { "p45", 11.563817165786343, 1222, 604, 21, 33, 5, 3, 10, 8, 7.085098817047365, 2, false };
    const P6 pos[4] = { {0,0,2,3,0,0}, {2,0,8,4,4,1}, {0,4,5,4,3,1}, {5,4,5,4,3,1} };
    const P4 fin[4] = { {0,1,2,3}, {2,0,8,4}, {0,4,5,4}, {5,4,5,4} };
    check(b, DEVICE, 4, pos, fin);
}
void test_device_p4_7b() {
    const Board b = { "p47b", 6.67510936483721, 990, 533, 12, 19, 6, 3, 10, 8, 12.64195896956605, 2, false };
    const P6 pos[4] = { {0,0,2,3,0,0}, {2,0,8,4,4,1}, {0,4,5,4,3,1}, {5,4,5,4,3,1} };
    const P4 fin[4] = { {0,1,2,3}, {2,0,8,4}, {0,4,5,4}, {5,4,5,4} };
    check(b, DEVICE, 4, pos, fin);
}
// WS_P4_4B and WS_S3_4B: bound by the tile limit - Kitchen across the middle, 8 units.
void test_device_p4_4b() {
    const Board b = { "p44b", 10.021985875084926, 670, 620, 18, 29, 4, 3, 8, 7, 61.21425865492643, 4, false };
    const P6 pos[4] = { {0,0,2,2,0,0}, {0,2,8,3,4,1}, {2,0,6,2,3,1}, {0,5,6,2,3,1} };
    const P4 fin[4] = { {0,0,2,2}, {0,2,8,3}, {2,0,6,2}, {0,5,6,2} };
    check(b, DEVICE, 4, pos, fin);
}
void test_device_s3_4b() {
    const Board b = { "s34b", 6.681323916723285, 446, 413, 12, 19, 4, 3, 8, 7, 61.24431594568695, 4, false };
    const P6 pos[4] = { {0,0,2,2,0,0}, {0,2,8,3,4,1}, {2,0,6,2,3,1}, {0,5,6,2,3,1} };
    const P4 fin[4] = { {0,0,2,2}, {0,2,8,3}, {2,0,6,2}, {0,5,6,2} };
    check(b, DEVICE, 4, pos, fin);
}
void test_device_4880_landscape() {
    const Board b = { "4880l", 8.54195479926065, 758, 395, 15, 25, 4, 3, 7, 6, 57.138895118570616, 2, false };
    const P6 pos[4] = { {0,0,2,2,0,0}, {0,2,3,4,2,2}, {3,0,4,3,3,1}, {3,3,4,3,3,1} };
    const P4 fin[4] = { {0,0,2,2}, {0,2,3,4}, {3,0,4,3}, {3,3,4,3} };
    check(b, DEVICE, 4, pos, fin);
}
// Portrait: the card takes the free unit rows under it as even margins.
void test_device_4880_portrait() {
    const Board b = { "4880p", 8.54195479926065, 438, 715, 15, 25, 2, 5, 6, 10, 17.631700880750657, 4, false };
    const P6 pos[4] = { {0,0,2,2,0,0}, {2,0,4,4,2,2}, {0,4,6,3,3,1}, {0,7,6,3,3,1} };
    const P4 fin[4] = { {0,1,2,2}, {2,0,4,4}, {0,4,6,3}, {0,7,6,3} };
    check(b, DEVICE, 4, pos, fin);
}
void test_device_3248() {
    const Board b = { "3248", 6.4891811481916575, 288, 416, 12, 19, 2, 4, 8, 8, 162.7118415409238, 2, false };
    const P6 pos[4] = { {0,0,3,2,0,0}, {0,2,4,6,1,4}, {4,0,4,4,1,3}, {4,4,4,4,1,3} };
    const P4 fin[4] = { {0,0,3,2}, {0,2,4,6}, {4,0,4,4}, {4,4,4,4} };
    check(b, DEVICE, 4, pos, fin);
}

// ---- HOUSE -----------------------------------------------------------------

void test_house_p4_5() {
    const Board b = { "p45", 11.563817165786343, 1222, 604, 21, 33, 5, 3, 10, 8, 15.319271606222369, 4, false };
    const P6 pos[6] = { {0,0,4,4,2,1}, {4,0,4,4,2,1}, {0,4,4,4,2,1}, {4,4,4,4,2,1}, {8,0,2,3,0,0}, {8,4,2,3,0,0} };
    const P4 fin[6] = { {0,0,4,4}, {4,0,4,4}, {0,4,4,4}, {4,4,4,4}, {8,1,2,3}, {8,5,2,3} };
    check(b, HOUSE, 6, pos, fin);
}
void test_house_p4_7b() {
    const Board b = { "p47b", 6.67510936483721, 990, 533, 12, 19, 6, 3, 15, 8, 15.927921061518724, 6, false };
    const P6 pos[6] = { {0,0,6,4,2,1}, {6,0,6,4,2,1}, {0,4,6,4,2,1}, {6,4,6,4,2,1}, {12,0,3,3,0,0}, {12,4,3,3,0,0} };
    const P4 fin[6] = { {0,0,6,4}, {6,0,6,4}, {0,4,6,4}, {6,4,6,4}, {12,1,3,3}, {12,5,3,3} };
    check(b, HOUSE, 6, pos, fin);
}
void test_house_p4_4b() {
    const Board b = { "p44b", 10.021985875084926, 670, 620, 18, 29, 4, 3, 10, 6, 53.092226799381784, 0, false };
    const P6 pos[6] = { {0,0,6,2,2,1}, {6,0,4,3,1,2}, {0,2,6,2,2,1}, {6,3,4,3,1,2}, {0,4,3,2,0,0}, {3,4,3,2,0,0} };
    const P4 fin[6] = { {0,0,6,2}, {6,0,4,3}, {0,2,6,2}, {6,3,4,3}, {0,4,3,2}, {3,4,3,2} };
    check(b, HOUSE, 6, pos, fin);
}
// The one search that hits the step budget: its answer is still the playground's.
void test_house_4880_portrait_capped() {
    const Board b = { "4880p", 8.54195479926065, 438, 715, 15, 25, 2, 5, 6, 11, 69.42708932419563, 10, true };
    const P6 pos[6] = { {0,0,3,4,1,2}, {3,0,3,4,1,2}, {0,4,4,3,2,1}, {0,7,4,3,2,1}, {4,7,2,2,0,0}, {4,9,2,2,0,0} };
    const P4 fin[6] = { {0,0,3,4}, {3,0,3,4}, {0,4,4,3}, {0,7,4,3}, {4,6,2,2}, {4,9,2,2} };
    check(b, HOUSE, 6, pos, fin);
}
void test_house_3248() {
    const Board b = { "3248", 6.4891811481916575, 288, 416, 12, 19, 2, 4, 8, 8, 110.39770261799032, 4, false };
    const P6 pos[6] = { {0,0,4,3,1,2}, {4,0,4,3,1,2}, {0,3,4,3,1,2}, {4,3,4,3,1,2}, {0,6,3,2,0,0}, {3,6,3,2,0,0} };
    const P4 fin[6] = { {0,0,4,3}, {4,0,4,3}, {0,3,4,3}, {4,3,4,3}, {0,6,3,2}, {3,6,3,2} };
    check(b, HOUSE, 6, pos, fin);
}

// ---- A PINNED GRID (the system panel's Col / Row) ----------------------------

// The P4_5's House page as the owner pinned it, 4 x 2 cells: one grid, 8 x 4 units -
// Outside and Office splitting the bottom row.
void test_pinned_p4_5_4x2() {
    ArrScreen s = screen({ "p45", 11.563817165786343, 1222, 604, 21, 33, 5, 3, 0, 0, 0, 0, false });
    s.fixCols = 4;
    s.fixRows = 2;
    const P6 pos[4] = { {0,0,2,2,0,0}, {2,0,6,2,4,1}, {0,2,4,2,3,1}, {4,2,4,2,3,1} };
    const P4 fin[4] = { {0,0,2,2}, {2,0,6,2}, {0,2,4,2}, {4,2,4,2} };
    check(s, DEVICE, 4, 8, 4, 8.957486920992693, 0, false, pos, fin);
}
void test_pinned_house_p4_5_5x3() {
    ArrScreen s = screen({ "p45", 11.563817165786343, 1222, 604, 21, 33, 5, 3, 0, 0, 0, 0, false });
    s.fixCols = 5;
    s.fixRows = 3;
    const P6 pos[6] = { {0,0,4,3,2,1}, {4,0,4,3,2,1}, {0,3,4,3,2,1}, {4,3,4,3,2,1}, {8,0,2,2,0,0}, {8,3,2,2,0,0} };
    const P4 fin[6] = { {0,0,4,3}, {4,0,4,3}, {0,3,4,3}, {4,3,4,3}, {8,1,2,2}, {8,4,2,2} };
    check(s, HOUSE, 6, 10, 6, 27.348892869338155, 4, false, pos, fin);
}
// The 4B at its own 4 x 3, pinned: three tiles at the owner's smallest need three
// quarters of its width, and no page holds the device's blocks.
void test_pinned_4b_4x3_none() {
    ArrScreen s = screen({ "p44b", 10.021985875084926, 670, 620, 18, 29, 4, 3, 0, 0, 0, 0, false });
    s.fixCols = 4;
    s.fixRows = 3;
    ArrResult out;
    TEST_ASSERT_FALSE(arrange(s, DEVICE, 4, ArrWeights(), out));
}
// One count pinned (beyond the playground, which pins both or neither): the columns are
// obeyed and the rows chosen.
void test_pinned_columns_only() {
    ArrScreen s = screen({ "p45", 11.563817165786343, 1222, 604, 21, 33, 5, 3, 0, 0, 0, 0, false });
    s.fixCols = 4;
    ArrResult out;
    TEST_ASSERT_TRUE(arrange(s, DEVICE, 4, ArrWeights(), out));
    TEST_ASSERT_EQUAL_UINT8(8, out.cols);
    TEST_ASSERT_TRUE(out.rows >= 4 && out.rows <= 8);
}

// ---- Properties ------------------------------------------------------------

// The same blocks give the same page, every time.
void test_deterministic() {
    const Board b = { "p45", 11.563817165786343, 1222, 604, 21, 33, 5, 3, 0, 0, 0, 0, false };
    ArrResult x, y;
    arrange(screen(b), HOUSE, 6, ArrWeights(), x);
    arrange(screen(b), HOUSE, 6, ArrWeights(), y);
    TEST_ASSERT_EQUAL_UINT8(x.cols, y.cols);
    TEST_ASSERT_TRUE(x.score == y.score);
    for (int i = 0; i < 6; i++) {
        TEST_ASSERT_EQUAL_UINT8(x.box[i].c, y.box[i].c);
        TEST_ASSERT_EQUAL_UINT8(x.box[i].r, y.box[i].r);
    }
}

// No two blocks overlap, and every block is on the page.
void test_no_overlap() {
    const Board b = { "p45", 11.563817165786343, 1222, 604, 21, 33, 5, 3, 0, 0, 0, 0, false };
    ArrResult out;
    TEST_ASSERT_TRUE(arrange(screen(b), HOUSE, 6, ArrWeights(), out));
    uint8_t occ[ARR_UNITS_MAX][ARR_UNITS_MAX] = {};
    for (int i = 0; i < 6; i++) {
        const ArrPlace &p = out.box[i];
        TEST_ASSERT_TRUE(p.c + p.w <= out.cols && p.r + p.h <= out.rows);
        for (int y = p.r; y < p.r + p.h; y++)
            for (int x = p.c; x < p.c + p.w; x++) { TEST_ASSERT_EQUAL_UINT8(0, occ[y][x]); occ[y][x] = 1; }
    }
}

int main(int, char **) {
    UNITY_BEGIN();
    RUN_TEST(test_references);
    RUN_TEST(test_device_p4_5);
    RUN_TEST(test_device_p4_7b);
    RUN_TEST(test_device_p4_4b);
    RUN_TEST(test_device_s3_4b);
    RUN_TEST(test_device_4880_landscape);
    RUN_TEST(test_device_4880_portrait);
    RUN_TEST(test_device_3248);
    RUN_TEST(test_house_p4_5);
    RUN_TEST(test_house_p4_7b);
    RUN_TEST(test_house_p4_4b);
    RUN_TEST(test_house_4880_portrait_capped);
    RUN_TEST(test_house_3248);
    RUN_TEST(test_pinned_p4_5_4x2);
    RUN_TEST(test_pinned_house_p4_5_5x3);
    RUN_TEST(test_pinned_4b_4x3_none);
    RUN_TEST(test_pinned_columns_only);
    RUN_TEST(test_deterministic);
    RUN_TEST(test_no_overlap);
    return UNITY_END();
}
