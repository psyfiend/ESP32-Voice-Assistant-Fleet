// The arranger (2.11 round 2, K105): Layout Playground v5's answers are the expected values.
// The blocks are the playground's House page grouped by area, its header kinds promoted:
// Garage (2 doors, no header), Outside, Kitchen and Office (2 tiles, 2 in the header, 4
// members each), and two cards (Living, Front). The screens are the playground's boards,
// their geometry worked out as its geometry() does; its weights and knobs are the defaults.
// Run 2026-10-19 with node on the playground's own code (scratchpad run_v5.js).
#include <unity.h>
#include <math.h>
#include "Layout/Arranger.h"

using namespace Layout;

void setUp() {}
void tearDown() {}

static ArrScreen board(int w, int h, double diag, uint8_t recC, uint8_t recR) {
    const double ppi = hypot((double)w, (double)h) / diag, pmm = ppi / 25.4;
    const double hdrH = round(5 * pmm), inset = round(2.5 * pmm), gap = round(1.8 * pmm);
    ArrScreen s;
    s.pmm = pmm;
    s.aw = w - 2 * inset;
    s.ah = h - hdrH - 2 * inset;
    s.gap = gap;
    s.tagH = round(round(2.0 * pmm) * 1.45);
    s.recCols = recC;
    s.recRows = recR;
    return s;
}

static const ArrBlock HOUSE[6] = {
    { true, 2, 0, 2, 0, 0 },   // Garage: the two doors
    { true, 2, 2, 4, 0, 0 },   // Outside
    { true, 2, 2, 4, 0, 0 },   // Kitchen
    { true, 2, 2, 4, 0, 0 },   // Office
    { false, 0, 0, 1, 0, 0 },  // Living's Overhead
    { false, 0, 0, 1, 0, 0 },  // Front's Thermo
};

struct Want { uint8_t c, r, w, h; bool one; };

static void check(const ArrScreen &s, uint8_t cols, uint8_t rows, double score, uint16_t holes, const Want *want) {
    ArrResult out;
    TEST_ASSERT_TRUE(arrange(s, HOUSE, 6, ArrWeights(), ArrKnobs(), out));
    TEST_ASSERT_EQUAL_UINT8(cols, out.cols);
    TEST_ASSERT_EQUAL_UINT8(rows, out.rows);
    TEST_ASSERT_TRUE(fabs(score - out.score) < 1e-5);   // Unity's doubles are off here
    TEST_ASSERT_EQUAL_UINT16(holes, out.holes);
    for (int i = 0; i < 6; i++) {
        TEST_ASSERT_EQUAL_UINT8(want[i].c, out.pos[i].c);
        TEST_ASSERT_EQUAL_UINT8(want[i].r, out.pos[i].r);
        TEST_ASSERT_EQUAL_UINT8(want[i].w, out.pos[i].w);
        TEST_ASSERT_EQUAL_UINT8(want[i].h, out.pos[i].h);
        TEST_ASSERT_EQUAL(want[i].one, out.pos[i].one);
    }
}

// WS_P4_5: a 5 x 2 page with no empty cell, four even 2 x 1 clusters (group-cards.md §6.5).
void test_p4_5() {
    const Want w[6] = { {0,0,2,1,false}, {2,0,2,1,false}, {0,1,2,1,false}, {2,1,2,1,false},
                        {4,0,1,1,false}, {4,1,1,1,false} };
    check(board(1280, 720, 5.0, 5, 3), 5, 2, 115.804208, 0, w);
}

// WS_P4_7B: too few blocks for its cells - 5 x 3 with six left empty, Garage in one cell.
void test_p4_7b() {
    const Want w[6] = { {0,0,1,1,true}, {1,0,2,1,false}, {3,0,2,1,false}, {0,1,2,1,false},
                        {2,1,1,1,false}, {3,1,1,1,false} };
    check(board(1024, 600, 7.0, 6, 3), 5, 3, 662.122620, 6, w);
}

// WS_P4_4B and WS_S3_4B: 4 x 3, two clusters three cells wide.
void test_p4_4b() {
    const Want w[6] = { {0,0,2,1,false}, {2,0,2,1,false}, {0,1,3,1,false}, {0,2,3,1,false},
                        {3,1,1,1,false}, {3,2,1,1,false} };
    check(board(720, 720, 4.0, 4, 3), 4, 3, 37.751485, 0, w);
}
void test_s3_4b() {
    const Want w[6] = { {0,0,2,1,false}, {2,0,2,1,false}, {0,1,3,1,false}, {0,2,3,1,false},
                        {3,1,1,1,false}, {3,2,1,1,false} };
    check(board(480, 480, 4.0, 4, 3), 4, 3, 37.411450, 0, w);
}

// CYD_P4_4880, landscape and portrait.
void test_4880_landscape() {
    const Want w[6] = { {0,0,2,1,false}, {2,0,2,1,false}, {0,1,2,1,false}, {2,1,2,1,false},
                        {4,0,1,1,false}, {4,1,1,1,false} };
    check(board(800, 480, 4.3, 4, 3), 5, 2, 35.106002, 0, w);
}
void test_4880_portrait() {
    const Want w[6] = { {0,0,1,2,false}, {1,0,1,2,false}, {0,2,1,2,false}, {1,2,1,2,false},
                        {0,4,1,1,false}, {1,4,1,1,false} };
    check(board(480, 800, 4.3, 2, 5), 2, 5, 38.874692, 0, w);
}

// CYD_S3_3248 in portrait: the playground finds no page for these blocks.
void test_3248_none() {
    ArrResult out;
    TEST_ASSERT_FALSE(arrange(board(320, 480, 3.5, 2, 4), HOUSE, 6, ArrWeights(), ArrKnobs(), out));
}

// The same blocks give the same page, every time.
void test_deterministic() {
    ArrResult a, b;
    arrange(board(1280, 720, 5.0, 5, 3), HOUSE, 6, ArrWeights(), ArrKnobs(), a);
    arrange(board(1280, 720, 5.0, 5, 3), HOUSE, 6, ArrWeights(), ArrKnobs(), b);
    TEST_ASSERT_EQUAL_UINT8(a.cols, b.cols);
    TEST_ASSERT_TRUE(a.score == b.score);
    for (int i = 0; i < 6; i++) TEST_ASSERT_EQUAL_UINT8(a.pos[i].c, b.pos[i].c);
}

// A page's own grid is obeyed (UI Tweaks' Col / Row).
void test_fixed_grid() {
    ArrScreen s = board(1280, 720, 5.0, 5, 3);
    s.fixCols = 5;
    s.fixRows = 3;
    ArrResult out;
    TEST_ASSERT_TRUE(arrange(s, HOUSE, 6, ArrWeights(), ArrKnobs(), out));
    TEST_ASSERT_EQUAL_UINT8(5, out.cols);
    TEST_ASSERT_EQUAL_UINT8(3, out.rows);
}

int main(int, char **) {
    UNITY_BEGIN();
    RUN_TEST(test_p4_5);
    RUN_TEST(test_p4_7b);
    RUN_TEST(test_p4_4b);
    RUN_TEST(test_s3_4b);
    RUN_TEST(test_4880_landscape);
    RUN_TEST(test_4880_portrait);
    RUN_TEST(test_3248_none);
    RUN_TEST(test_deterministic);
    RUN_TEST(test_fixed_grid);
    return UNITY_END();
}
