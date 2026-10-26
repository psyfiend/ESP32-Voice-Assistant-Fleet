// The arranger (2.11, K109, K120): Layout Playground v9's answers are the expected values.
// GENERATED from the playground's own code (its page v11, 2026-10-26) by scratchpad gen_tests.py
// with run_v9.js - do not edit the cases by hand; run it again when the playground changes.
// Four sets of blocks: DEVICE (the House page as the boards showed it in round 6: Doors, Kitchen
// 4 tiles, Outside and Office 3), PLAYGROUND (its own House page by area), P1060 (the 1060's page in
// round 9: Doors, three 3-tile clusters, Thermo) and FLEET (sixteen solo cards, Free Heap and Uptime
// preferring 1.5 cells across). The screens are the playground's boards, its geometry() worked out
// for each, Float labels. Every grid, place, trimmed box, finished rectangle, score and empty unit
// is checked.
#include <unity.h>
#include <math.h>
#include "Layout/Arranger.h"

using namespace Layout;

void setUp() {}
void tearDown() {}

static const ArrBlock DEVICE[4] = {
    { false, 0, 0, 1, 1.0 },
    { true, 4, 2, 6, 1.0 },
    { true, 3, 2, 5, 1.0 },
    { true, 3, 2, 5, 1.0 },
};
static const ArrBlock PLAYGROUND[6] = {
    { true, 2, 0, 2, 1.0 },
    { true, 2, 2, 4, 1.0 },
    { true, 2, 2, 4, 1.0 },
    { true, 2, 2, 4, 1.0 },
    { false, 0, 0, 1, 1.0 },
    { false, 0, 0, 1, 1.0 },
};
static const ArrBlock P1060[5] = {
    { false, 0, 0, 1, 1.0 },
    { true, 3, 2, 5, 1.0 },
    { true, 3, 2, 5, 1.0 },
    { true, 3, 2, 5, 1.0 },
    { false, 0, 0, 1, 1.0 },
};
static const ArrBlock FLEET[16] = {
    { false, 0, 0, 1, 1.0 },
    { false, 0, 0, 1, 1.0 },
    { false, 0, 0, 1, 1.0 },
    { false, 0, 0, 1, 1.0 },
    { false, 0, 0, 1, 1.0 },
    { false, 0, 0, 1, 1.0 },
    { false, 0, 0, 1, 1.0 },
    { false, 0, 0, 1, 1.0 },
    { false, 0, 0, 1, 1.0 },
    { false, 0, 0, 1, 1.0 },
    { false, 0, 0, 1, 1.0 },
    { false, 0, 0, 1, 1.0 },
    { false, 0, 0, 1, 1.0 },
    { false, 0, 0, 1, 1.0 },
    { false, 0, 0, 1, 1.5 },
    { false, 0, 0, 1, 1.5 },
};

struct Want { uint8_t c, r, w, h, tc, tr; uint8_t bc, br, bw, bh; int32_t x, y, rw, rh; };

static ArrScreen screen(double pmm, double aw, double ah, double gap, double tagH, uint8_t rc, uint8_t rr) {
    ArrScreen s;
    s.pmm = pmm; s.aw = aw; s.ah = ah; s.gap = gap; s.tagH = tagH;
    s.label = ArrLabel::LABEL_FLOAT;
    s.recCols = rc; s.recRows = rr;
    return s;
}

static void check(const ArrScreen &s, const ArrBlock *blocks, uint8_t n, uint8_t cols, uint8_t rows,
                  double score, uint16_t holes, bool capped, const Want *want) {
    ArrResult out;
    TEST_ASSERT_TRUE(arrange(s, blocks, n, ArrWeights(), out));
    TEST_ASSERT_EQUAL_UINT8(cols, out.cols);
    TEST_ASSERT_EQUAL_UINT8(rows, out.rows);
    TEST_ASSERT_TRUE(fabs(score - out.score) < 1e-6);   // Unity's doubles are off here
    TEST_ASSERT_EQUAL_UINT16(holes, out.holes);
    TEST_ASSERT_EQUAL(capped, out.capped);
    for (uint8_t i = 0; i < n; i++) {
        const Want &q = want[i];
        TEST_ASSERT_EQUAL_UINT8(q.c, out.pos[i].c);  TEST_ASSERT_EQUAL_UINT8(q.r, out.pos[i].r);
        TEST_ASSERT_EQUAL_UINT8(q.w, out.pos[i].w);  TEST_ASSERT_EQUAL_UINT8(q.h, out.pos[i].h);
        TEST_ASSERT_EQUAL_UINT8(q.tc, out.pos[i].tc); TEST_ASSERT_EQUAL_UINT8(q.tr, out.pos[i].tr);
        TEST_ASSERT_EQUAL_UINT8(q.bc, out.box[i].c); TEST_ASSERT_EQUAL_UINT8(q.br, out.box[i].r);
        TEST_ASSERT_EQUAL_UINT8(q.bw, out.box[i].w); TEST_ASSERT_EQUAL_UINT8(q.bh, out.box[i].h);
        TEST_ASSERT_EQUAL_INT32(q.x, out.rect[i].x); TEST_ASSERT_EQUAL_INT32(q.y, out.rect[i].y);
        TEST_ASSERT_EQUAL_INT32(q.rw, out.rect[i].w); TEST_ASSERT_EQUAL_INT32(q.rh, out.rect[i].h);
    }
}

void test_device_p45() {
    ArrScreen s = screen(11.563817165786343, 1222.0, 604.0, 21.0, 33.0, 5, 3);
    const Want want[4] = {
        {0,0,2,3,0,0, 0,0,2,3, 0,39,228,213}, {2,0,8,4,4,1, 2,0,8,4, 249,0,973,292},
        {0,4,5,4,3,1, 0,4,5,4, 0,313,601,292}, {5,4,5,4,3,1, 5,4,5,4, 622,313,601,292},
    };
    check(s, DEVICE, 4, 10, 8, 7.10036473075221, 2, false, want);
}

void test_device_p47b() {
    ArrScreen s = screen(6.67510936483721, 990.0, 533.0, 12.0, 19.0, 6, 3);
    const Want want[4] = {
        {0,0,2,3,0,0, 0,0,2,3, 0,34,188,192}, {2,0,8,4,4,1, 2,0,8,4, 200,0,790,261},
        {0,4,5,4,3,1, 0,4,5,4, 0,273,489,261}, {5,4,5,4,3,1, 5,4,5,4, 501,273,489,261},
    };
    check(s, DEVICE, 4, 10, 8, 12.64195896956605, 2, false, want);
}

void test_device_1060() {
    ArrScreen s = screen(6.67510936483721, 990.0, 533.0, 12.0, 19.0, 6, 3);
    const Want want[4] = {
        {0,0,2,3,0,0, 0,0,2,3, 0,34,188,192}, {2,0,8,4,4,1, 2,0,8,4, 200,0,790,261},
        {0,4,5,4,3,1, 0,4,5,4, 0,273,489,261}, {5,4,5,4,3,1, 5,4,5,4, 501,273,489,261},
    };
    check(s, DEVICE, 4, 10, 8, 12.64195896956605, 2, false, want);
}

void test_device_p44b() {
    ArrScreen s = screen(10.021985875084926, 670.0, 620.0, 18.0, 29.0, 4, 3);
    const Want want[4] = {
        {0,0,2,2,0,0, 0,0,2,2, 0,0,154,164}, {0,2,8,3,4,1, 0,2,8,3, 0,182,670,255},
        {2,0,6,2,3,1, 2,0,6,2, 172,0,498,164}, {0,5,6,2,3,1, 0,5,6,2, 86,456,498,164},
    };
    check(s, DEVICE, 4, 8, 7, 61.599421034390375, 4, false, want);
}

void test_device_s34b() {
    ArrScreen s = screen(6.681323916723285, 446.0, 413.0, 12.0, 19.0, 4, 3);
    const Want want[4] = {
        {0,0,2,2,0,0, 0,0,2,2, 0,0,103,109}, {0,2,8,3,4,1, 0,2,8,3, 0,121,446,170},
        {2,0,6,2,3,1, 2,0,6,2, 115,0,332,109}, {0,5,6,2,3,1, 0,5,6,2, 57,304,332,109},
    };
    check(s, DEVICE, 4, 8, 7, 61.61636477564413, 4, false, want);
}

void test_device_4880l() {
    ArrScreen s = screen(8.54195479926065, 758.0, 395.0, 15.0, 25.0, 4, 3);
    const Want want[4] = {
        {0,0,2,2,0,0, 0,0,2,2, 0,0,206,122}, {0,2,3,4,2,2, 0,2,3,4, 0,137,316,258},
        {3,0,4,3,3,1, 3,0,4,3, 331,0,427,190}, {3,3,4,3,3,1, 3,3,4,3, 331,205,427,190},
    };
    check(s, DEVICE, 4, 7, 6, 58.60419982017362, 2, false, want);
}

void test_device_4880p() {
    ArrScreen s = screen(8.54195479926065, 438.0, 715.0, 15.0, 25.0, 2, 5);
    const Want want[4] = {
        {0,0,2,2,0,0, 0,0,2,2, 0,73,136,131}, {2,0,4,4,2,2, 2,0,4,4, 151,0,287,277},
        {0,4,6,3,3,1, 0,4,6,3, 0,292,438,204}, {0,7,6,3,3,1, 0,7,6,3, 0,511,438,204},
    };
    check(s, DEVICE, 4, 6, 10, 17.631700880750657, 4, false, want);
}

void test_device_3248() {
    ArrScreen s = screen(6.4891811481916575, 288.0, 416.0, 12.0, 19.0, 2, 4);
    const Want want[4] = {
        {0,0,3,2,0,0, 0,0,3,2, 0,0,101,95}, {0,2,4,6,1,4, 0,2,4,6, 0,107,138,309},
        {4,0,4,4,1,3, 4,0,4,4, 150,0,138,202}, {4,4,4,4,1,3, 4,4,4,4, 150,214,138,202},
    };
    check(s, DEVICE, 4, 8, 8, 163.29042153858282, 2, false, want);
}

void test_playground_p45() {
    ArrScreen s = screen(11.563817165786343, 1222.0, 604.0, 21.0, 33.0, 5, 3);
    const Want want[6] = {
        {0,0,4,4,2,1, 0,0,4,4, 0,0,476,292}, {4,0,4,4,2,1, 4,0,4,4, 497,0,476,292},
        {0,4,4,4,2,1, 0,4,4,4, 0,313,476,292}, {4,4,4,4,2,1, 4,4,4,4, 497,313,476,292},
        {8,0,2,3,0,0, 8,0,2,3, 994,39,228,213}, {8,4,2,3,0,0, 8,4,2,3, 994,352,228,213},
    };
    check(s, PLAYGROUND, 6, 10, 8, 15.319271606222369, 4, false, want);
}

void test_playground_p47b() {
    ArrScreen s = screen(6.67510936483721, 990.0, 533.0, 12.0, 19.0, 6, 3);
    const Want want[6] = {
        {0,0,6,4,2,1, 0,0,6,4, 0,0,389,261}, {6,0,6,4,2,1, 6,0,6,4, 401,0,389,261},
        {0,4,6,4,2,1, 0,4,6,4, 0,273,389,261}, {6,4,6,4,2,1, 6,4,6,4, 401,273,389,261},
        {12,0,3,3,0,0, 12,0,3,3, 802,34,188,192}, {12,4,3,3,0,0, 12,4,3,3, 802,307,188,192},
    };
    check(s, PLAYGROUND, 6, 15, 8, 15.927921061518724, 6, false, want);
}

void test_playground_1060() {
    ArrScreen s = screen(6.67510936483721, 990.0, 533.0, 12.0, 19.0, 6, 3);
    const Want want[6] = {
        {0,0,6,4,2,1, 0,0,6,4, 0,0,389,261}, {6,0,6,4,2,1, 6,0,6,4, 401,0,389,261},
        {0,4,6,4,2,1, 0,4,6,4, 0,273,389,261}, {6,4,6,4,2,1, 6,4,6,4, 401,273,389,261},
        {12,0,3,3,0,0, 12,0,3,3, 802,34,188,192}, {12,4,3,3,0,0, 12,4,3,3, 802,307,188,192},
    };
    check(s, PLAYGROUND, 6, 15, 8, 15.927921061518724, 6, false, want);
}

void test_playground_p44b() {
    ArrScreen s = screen(10.021985875084926, 670.0, 620.0, 18.0, 29.0, 4, 3);
    const Want want[6] = {
        {0,0,6,2,2,1, 0,0,6,2, 0,0,395,195}, {6,0,4,3,1,2, 6,0,4,3, 413,0,257,301},
        {0,2,6,2,2,1, 0,2,6,2, 0,213,395,195}, {6,3,4,3,1,2, 6,3,4,3, 413,319,257,301},
        {0,4,3,2,0,0, 0,4,3,2, 0,425,188,195}, {3,4,3,2,0,0, 3,4,3,2, 206,425,188,195},
    };
    check(s, PLAYGROUND, 6, 10, 6, 55.000857075066776, 0, false, want);
}

void test_playground_s34b() {
    ArrScreen s = screen(6.681323916723285, 446.0, 413.0, 12.0, 19.0, 4, 3);
    const Want want[6] = {
        {0,0,6,2,2,1, 0,0,6,2, 0,0,263,130}, {6,0,4,3,1,2, 6,0,4,3, 275,0,171,201},
        {0,2,6,2,2,1, 0,2,6,2, 0,142,263,130}, {6,3,4,3,1,2, 6,3,4,3, 275,213,171,201},
        {0,4,3,2,0,0, 0,4,3,2, 0,283,125,130}, {3,4,3,2,0,0, 3,4,3,2, 137,283,125,130},
    };
    check(s, PLAYGROUND, 6, 10, 6, 54.94680820425001, 0, false, want);
}

void test_playground_4880l() {
    ArrScreen s = screen(8.54195479926065, 758.0, 395.0, 15.0, 25.0, 4, 3);
    const Want want[6] = {
        {0,0,4,4,2,1, 0,0,4,4, 0,0,294,190}, {4,0,4,4,2,1, 4,0,4,4, 309,0,294,190},
        {0,4,4,4,2,1, 0,4,4,4, 0,205,294,190}, {4,4,4,4,2,1, 4,4,4,4, 309,205,294,190},
        {8,0,2,3,0,0, 8,0,2,3, 618,26,140,139}, {8,4,2,3,0,0, 8,4,2,3, 618,231,140,139},
    };
    check(s, PLAYGROUND, 6, 10, 8, 14.183002143201573, 4, false, want);
}

void test_playground_4880p() {
    ArrScreen s = screen(8.54195479926065, 438.0, 715.0, 15.0, 25.0, 2, 5);
    const Want want[6] = {
        {0,0,3,4,1,2, 0,0,3,4, 0,0,212,250}, {3,0,3,4,1,2, 3,0,3,4, 227,0,212,250},
        {0,4,4,3,2,1, 0,4,4,3, 76,265,287,184}, {0,7,4,3,2,1, 0,7,4,3, 0,498,287,184},
        {4,7,2,2,0,0, 4,7,2,2, 302,465,136,118}, {4,9,2,2,0,0, 4,9,2,2, 302,597,136,118},
    };
    check(s, PLAYGROUND, 6, 6, 11, 69.42708932419563, 10, true, want);
}

void test_playground_3248() {
    ArrScreen s = screen(6.4891811481916575, 288.0, 416.0, 12.0, 19.0, 2, 4);
    const Want want[6] = {
        {0,0,4,3,1,2, 0,0,4,3, 0,0,138,149}, {4,0,4,3,1,2, 4,0,4,3, 150,0,138,149},
        {0,3,4,3,1,2, 0,3,4,3, 0,161,138,149}, {4,3,4,3,1,2, 4,3,4,3, 150,161,138,149},
        {0,6,3,2,0,0, 0,6,3,2, 19,321,101,95}, {3,6,3,2,0,0, 3,6,3,2, 169,321,101,95},
    };
    check(s, PLAYGROUND, 6, 8, 8, 112.36331111883878, 4, false, want);
}

void test_p1060_1060() {
    ArrScreen s = screen(6.67510936483721, 990.0, 533.0, 12.0, 19.0, 6, 3);
    const Want want[5] = {
        {0,0,3,3,0,0, 0,0,3,3, 12,34,203,192}, {3,0,7,4,3,1, 3,0,7,4, 251,0,489,261},
        {0,4,7,4,3,1, 0,4,7,4, 0,273,489,261}, {7,4,7,4,3,1, 7,4,7,4, 501,273,489,261},
        {10,0,3,3,0,0, 10,0,3,3, 775,34,203,192},
    };
    check(s, P1060, 5, 14, 8, 30.739821508604095, 10, true, want);
}

void test_fleet_p45() {
    ArrScreen s = screen(11.563817165786343, 1222.0, 604.0, 21.0, 33.0, 5, 3);
    const Want want[16] = {
        {0,0,2,2,0,0, 0,0,2,2, 0,0,186,187}, {2,0,2,2,0,0, 2,0,2,2, 207,0,186,187},
        {4,0,2,2,0,0, 4,0,2,2, 414,0,186,187}, {6,0,2,2,0,0, 6,0,2,2, 622,0,186,187},
        {8,0,2,2,0,0, 8,0,2,2, 829,0,186,187}, {10,0,2,2,0,0, 10,0,2,2, 1036,0,186,187},
        {0,2,2,2,0,0, 0,2,2,2, 0,208,186,187}, {2,2,2,2,0,0, 2,2,2,2, 207,208,186,187},
        {4,2,2,2,0,0, 4,2,2,2, 414,208,186,187}, {6,2,2,2,0,0, 6,2,2,2, 622,208,186,187},
        {8,2,2,2,0,0, 8,2,2,2, 829,208,186,187}, {10,2,2,2,0,0, 10,2,2,2, 1036,208,186,187},
        {0,4,2,2,0,0, 0,4,2,2, 26,417,186,187}, {2,4,2,2,0,0, 2,4,2,2, 285,417,186,187},
        {4,4,3,2,0,0, 4,4,3,2, 544,417,290,187}, {7,4,3,2,0,0, 7,4,3,2, 906,417,290,187},
    };
    check(s, FLEET, 16, 12, 6, 10.339312592990291, 4, false, want);
}

void test_fleet_p47b() {
    ArrScreen s = screen(6.67510936483721, 990.0, 533.0, 12.0, 19.0, 6, 3);
    const Want want[16] = {
        {0,0,2,2,0,0, 0,0,2,2, 0,0,155,170}, {2,0,2,2,0,0, 2,0,2,2, 167,0,155,170},
        {4,0,2,2,0,0, 4,0,2,2, 334,0,155,170}, {6,0,2,2,0,0, 6,0,2,2, 501,0,155,170},
        {8,0,2,2,0,0, 8,0,2,2, 668,0,155,170}, {10,0,2,2,0,0, 10,0,2,2, 835,0,155,170},
        {0,2,2,2,0,0, 0,2,2,2, 0,182,155,170}, {2,2,2,2,0,0, 2,2,2,2, 167,182,155,170},
        {4,2,2,2,0,0, 4,2,2,2, 334,182,155,170}, {6,2,2,2,0,0, 6,2,2,2, 501,182,155,170},
        {8,2,2,2,0,0, 8,2,2,2, 668,182,155,170}, {10,2,2,2,0,0, 10,2,2,2, 835,182,155,170},
        {0,4,2,2,0,0, 0,4,2,2, 21,363,155,170}, {2,4,2,2,0,0, 2,4,2,2, 230,363,155,170},
        {4,4,3,2,0,0, 4,4,3,2, 438,363,239,170}, {7,4,3,2,0,0, 7,4,3,2, 731,363,239,170},
    };
    check(s, FLEET, 16, 12, 6, 25.128440489225042, 4, false, want);
}

// the P4_5's House page as the owner pinned it
void test_device_p45_pinned_4x2() {
    ArrScreen s = screen(11.563817165786343, 1222.0, 604.0, 21.0, 33.0, 5, 3);
    s.fixCols = 4; s.fixRows = 2;
    const Want want[4] = {
        {0,0,2,2,0,0, 0,0,2,2, 0,0,290,292}, {2,0,6,2,4,1, 2,0,6,2, 311,0,911,292},
        {0,2,4,2,3,1, 0,2,4,2, 0,313,601,292}, {4,2,4,2,3,1, 4,2,4,2, 622,313,601,292},
    };
    check(s, DEVICE, 4, 8, 4, 14.150077859490546, 0, false, want);
}

void test_playground_p45_pinned_5x3() {
    ArrScreen s = screen(11.563817165786343, 1222.0, 604.0, 21.0, 33.0, 5, 3);
    s.fixCols = 5; s.fixRows = 3;
    const Want want[6] = {
        {0,0,4,3,2,1, 0,0,4,3, 0,0,476,292}, {4,0,4,3,2,1, 4,0,4,3, 497,0,476,292},
        {0,3,4,3,2,1, 0,3,4,3, 0,313,476,292}, {4,3,4,3,2,1, 4,3,4,3, 497,313,476,292},
        {8,0,2,2,0,0, 8,0,2,2, 994,52,228,187}, {8,3,2,2,0,0, 8,3,2,2, 994,365,228,187},
    };
    check(s, PLAYGROUND, 6, 10, 6, 27.348892869338155, 4, false, want);
}

// the 4B pinned at five rows: bands spread down the page
void test_device_p44b_pinned_4x5() {
    ArrScreen s = screen(10.021985875084926, 670.0, 620.0, 18.0, 29.0, 4, 3);
    s.fixCols = 4; s.fixRows = 5;
    const Want want[4] = {
        {0,0,2,3,0,0, 0,0,2,3, 0,0,154,173}, {0,3,8,4,4,1, 0,3,8,4, 0,191,670,237},
        {2,0,6,3,3,1, 2,0,6,3, 172,0,498,173}, {0,7,6,3,3,1, 0,7,6,3, 86,447,498,173},
    };
    check(s, DEVICE, 4, 8, 10, 43.488537944050535, 6, false, want);
}

// the 4B pinned at its own 4 x 3: no page holds the blocks
void test_device_p44b_pinned_4x3() {
    ArrScreen s = screen(10.021985875084926, 670.0, 620.0, 18.0, 29.0, 4, 3);
    s.fixCols = 4; s.fixRows = 3;
    ArrResult out;
    TEST_ASSERT_FALSE(arrange(s, DEVICE, 4, ArrWeights(), out));
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

// One count pinned (beyond the playground, which pins both or neither): the columns are
// obeyed and the rows chosen.
void test_pinned_columns_only() {
    ArrScreen s = screen(11.563817165786343, 1222.0, 604.0, 21.0, 33.0, 5, 3);
    s.fixCols = 4;
    ArrResult out;
    TEST_ASSERT_TRUE(arrange(s, DEVICE, 4, ArrWeights(), out));
    TEST_ASSERT_EQUAL_UINT8(8, out.cols);
    TEST_ASSERT_TRUE(out.rows >= 4 && out.rows <= 8);
}

// A kept answer finished again gives the same boxes and rectangles (PageArranger keeps pos).
void test_finish_again() {
    const ArrScreen s = screen(11.563817165786343, 1222.0, 604.0, 21.0, 33.0, 5, 3);
    ArrResult out;
    TEST_ASSERT_TRUE(arrange(s, PLAYGROUND, 6, ArrWeights(), out));
    ArrPlace box[ARR_BLOCKS_MAX];
    ArrRect rect[ARR_BLOCKS_MAX];
    finishPage(s, PLAYGROUND, 6, out.cols, out.rows, out.pos, box, rect);
    for (int i = 0; i < 6; i++) {
        TEST_ASSERT_EQUAL_UINT8(out.box[i].r, box[i].r);
        TEST_ASSERT_EQUAL_UINT8(out.box[i].h, box[i].h);
        TEST_ASSERT_EQUAL_INT32(out.rect[i].x, rect[i].x);
        TEST_ASSERT_EQUAL_INT32(out.rect[i].y, rect[i].y);
    }
}

// No two finished rectangles overlap, and all lie in the page's room.
void test_no_overlap() {
    const ArrScreen s = screen(11.563817165786343, 1222.0, 604.0, 21.0, 33.0, 5, 3);
    ArrResult out;
    TEST_ASSERT_TRUE(arrange(s, FLEET, 16, ArrWeights(), out));
    for (int i = 0; i < 16; i++) {
        const ArrRect &a = out.rect[i];
        TEST_ASSERT_TRUE(a.x >= 0 && a.y >= 0 && a.x + a.w <= 1222 && a.y + a.h <= 604);
        for (int j = i + 1; j < 16; j++) {
            const ArrRect &b = out.rect[j];
            TEST_ASSERT_TRUE(a.x + a.w <= b.x || b.x + b.w <= a.x || a.y + a.h <= b.y || b.y + b.h <= a.y);
        }
    }
}

int main(int, char **) {
    UNITY_BEGIN();
    RUN_TEST(test_device_p45);
    RUN_TEST(test_device_p47b);
    RUN_TEST(test_device_1060);
    RUN_TEST(test_device_p44b);
    RUN_TEST(test_device_s34b);
    RUN_TEST(test_device_4880l);
    RUN_TEST(test_device_4880p);
    RUN_TEST(test_device_3248);
    RUN_TEST(test_playground_p45);
    RUN_TEST(test_playground_p47b);
    RUN_TEST(test_playground_1060);
    RUN_TEST(test_playground_p44b);
    RUN_TEST(test_playground_s34b);
    RUN_TEST(test_playground_4880l);
    RUN_TEST(test_playground_4880p);
    RUN_TEST(test_playground_3248);
    RUN_TEST(test_p1060_1060);
    RUN_TEST(test_fleet_p45);
    RUN_TEST(test_fleet_p47b);
    RUN_TEST(test_device_p45_pinned_4x2);
    RUN_TEST(test_playground_p45_pinned_5x3);
    RUN_TEST(test_device_p44b_pinned_4x5);
    RUN_TEST(test_device_p44b_pinned_4x3);
    RUN_TEST(test_references);
    RUN_TEST(test_pinned_columns_only);
    RUN_TEST(test_finish_again);
    RUN_TEST(test_no_overlap);
    return UNITY_END();
}
