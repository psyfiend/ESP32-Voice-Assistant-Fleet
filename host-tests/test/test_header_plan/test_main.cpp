// The header's planner (2.8 round 2, D-19): the three places and the claims on them, each rule
// pinned down by a test. Widths are px; the numbers are round so the arithmetic can be read.
#include <unity.h>
#include "Layout/HeaderPlan.h"

using namespace Layout;

static const HeaderPlace LEFT   = HeaderPlace::PLACE_LEFT;
static const HeaderPlace MIDDLE = HeaderPlace::PLACE_MIDDLE;
static const HeaderPlace RIGHT  = HeaderPlace::PLACE_RIGHT;

// One form, held with `keep` (a resident by default).
static HeaderClaim one(HeaderPlace p, int32_t w, uint8_t keep = KEEP_ALWAYS) {
    HeaderClaim c;
    c.place = p;
    c.forms = 1;
    c.width[0] = w;
    c.keep[0] = keep;
    return c;
}

// Two forms: the full one held with k0, the plain one with k1.
static HeaderClaim two(HeaderPlace p, int32_t w0, uint8_t k0, int32_t w1, uint8_t k1) {
    HeaderClaim c;
    c.place = p;
    c.forms = 2;
    c.width[0] = w0; c.keep[0] = k0;
    c.width[1] = w1; c.keep[1] = k1;
    return c;
}

static HeaderBar bar(int32_t width, int32_t edge = 10, int32_t space = 5, int32_t clear = 10) {
    HeaderBar b;
    b.width = width; b.edge = edge; b.space = space; b.clear = clear;
    return b;
}

void setUp() {}
void tearDown() {}

// The left from the bar's start, the right back to its end, the middle on the centre.
void test_places_stand_at_the_ends_and_the_centre() {
    HeaderClaim c[4] = {one(LEFT, 100), one(LEFT, 50), one(MIDDLE, 200), one(RIGHT, 80)};
    HeaderSpot s[4];
    TEST_ASSERT_TRUE(planHeader(bar(1000), c, 4, s));
    TEST_ASSERT_EQUAL_INT32(10,  s[0].x); TEST_ASSERT_EQUAL_INT32(100, s[0].w);
    TEST_ASSERT_EQUAL_INT32(115, s[1].x);
    TEST_ASSERT_EQUAL_INT32(400, s[2].x);
    TEST_ASSERT_EQUAL_INT32(910, s[3].x);
    for (int i = 0; i < 4; i++) TEST_ASSERT_TRUE(s[i].shown);
}

// In the right place the list still reads left to right: its last claim is at the bar's end.
void test_right_place_keeps_list_order() {
    HeaderClaim c[2] = {one(RIGHT, 30), one(RIGHT, 60)};
    HeaderSpot s[2];
    TEST_ASSERT_TRUE(planHeader(bar(1000), c, 2, s));
    TEST_ASSERT_EQUAL_INT32(930, s[1].x);
    TEST_ASSERT_EQUAL_INT32(895, s[0].x);
}

// However much the sides hold, the middle never leaves the centre.
void test_middle_never_moves() {
    HeaderClaim c[3] = {one(LEFT, 20), one(MIDDLE, 200), one(RIGHT, 300)};
    HeaderSpot s[3];
    TEST_ASSERT_TRUE(planHeader(bar(1000), c, 3, s));
    TEST_ASSERT_EQUAL_INT32(400, s[1].x);
}

// The narrowest board, as round 2's mock drew it: the alarm's time goes first, then the
// temperature's icon, then the page's name - each the lowest keep left among the claims that help.
void test_narrow_bar_sheds_in_keep_order() {
    HeaderClaim c[5] = {
        one(LEFT, 60),                       // the clock
        two(LEFT, 70, 20, 40, 60),           // the temperature: icon and value, then the value
        two(MIDDLE, 90, 30, 30, KEEP_ALWAYS),// the page: name and dots, then the dots
        two(RIGHT, 100, 10, 30, 200),        // the alarm: bell and time, then the bell
        one(RIGHT, 60),                      // status
    };
    HeaderSpot s[5];
    TEST_ASSERT_TRUE(planHeader(bar(320, 8, 2, 6), c, 5, s));
    TEST_ASSERT_EQUAL_UINT8(0, s[0].form);
    TEST_ASSERT_EQUAL_UINT8(1, s[1].form);   // the value alone
    TEST_ASSERT_EQUAL_UINT8(1, s[2].form);   // the dots alone
    TEST_ASSERT_EQUAL_UINT8(1, s[3].form);   // the bell alone
    TEST_ASSERT_EQUAL_UINT8(0, s[4].form);
    TEST_ASSERT_EQUAL_INT32(145, s[2].x);    // still on the centre
    // Nothing overlaps: the left ends before the middle, the right starts after it.
    TEST_ASSERT_TRUE(s[1].x + s[1].w + 6 <= s[2].x);
    TEST_ASSERT_TRUE(s[3].x - 6 >= s[2].x + s[2].w);
}

// Only the side that runs into the middle gives way: a cheap extra on the other side stays.
void test_only_the_overflowing_side_gives_way() {
    HeaderClaim c[3] = {
        two(LEFT, 200, 50, 100, KEEP_ALWAYS),
        one(MIDDLE, 100),
        two(RIGHT, 100, 5, 50, KEEP_ALWAYS),
    };
    HeaderSpot s[3];
    TEST_ASSERT_TRUE(planHeader(bar(400, 0, 0, 0), c, 3, s));
    TEST_ASSERT_EQUAL_UINT8(1, s[0].form);
    TEST_ASSERT_EQUAL_UINT8(0, s[2].form);
}

// The middle helps both sides, so it gives way when its keep is the lowest.
void test_middle_gives_way_when_lowest() {
    HeaderClaim c[2] = {one(LEFT, 160), two(MIDDLE, 120, 10, 40, KEEP_ALWAYS)};
    HeaderSpot s[2];
    TEST_ASSERT_TRUE(planHeader(bar(400, 0, 0, 0), c, 2, s));
    TEST_ASSERT_EQUAL_UINT8(1, s[1].form);
    TEST_ASSERT_EQUAL_INT32(180, s[1].x);
}

// A claim whose condition does not hold (no alarm set) takes no room and is not shown.
void test_unwanted_claims_take_no_room() {
    HeaderClaim c[3] = {one(MIDDLE, 100), two(RIGHT, 100, 10, 30, 200), one(RIGHT, 60)};
    c[1].wanted = false;
    HeaderSpot s[3];
    TEST_ASSERT_TRUE(planHeader(bar(1000), c, 3, s));
    TEST_ASSERT_FALSE(s[1].shown);
    TEST_ASSERT_EQUAL_INT32(930, s[2].x);
}

// From its last form a claim leaves the bar; residents never do, and when nothing else may give
// way the plan says it does not fit (and keeps everything it had).
void test_last_form_leaves_and_residents_stay() {
    HeaderClaim c[3] = {one(LEFT, 150), two(LEFT, 60, 20, 30, 40), one(MIDDLE, 100)};
    HeaderSpot s[3];
    TEST_ASSERT_TRUE(planHeader(bar(400, 0, 0, 0), c, 3, s));
    TEST_ASSERT_FALSE(s[1].shown);           // its value could not stay either
    TEST_ASSERT_EQUAL_UINT8(2, s[1].form);
    TEST_ASSERT_TRUE(s[0].shown);

    HeaderClaim r[2] = {one(LEFT, 300), one(MIDDLE, 100)};
    HeaderSpot t[2];
    TEST_ASSERT_FALSE(planHeader(bar(400, 0, 0, 0), r, 2, t));
    TEST_ASSERT_TRUE(t[0].shown);
    TEST_ASSERT_TRUE(t[1].shown);
}

// Two equal keeps: the later claim gives way first.
void test_ties_give_way_from_the_later_claim() {
    HeaderClaim c[3] = {two(LEFT, 100, 20, 50, KEEP_ALWAYS), two(LEFT, 100, 20, 50, KEEP_ALWAYS),
                        one(MIDDLE, 100)};
    HeaderSpot s[3];
    TEST_ASSERT_TRUE(planHeader(bar(400, 0, 0, 0), c, 3, s));
    TEST_ASSERT_EQUAL_UINT8(0, s[0].form);
    TEST_ASSERT_EQUAL_UINT8(1, s[1].form);
}

// With nothing in the middle the two sides have the whole bar between them.
void test_no_middle_sides_share_the_bar() {
    HeaderClaim c[2] = {two(LEFT, 140, 10, 100, KEEP_ALWAYS), one(RIGHT, 140)};
    HeaderSpot s[2];
    TEST_ASSERT_TRUE(planHeader(bar(400, 0, 0, 10), c, 2, s));
    TEST_ASSERT_EQUAL_UINT8(0, s[0].form);   // 140 + 10 + 140 fits in 400
    TEST_ASSERT_TRUE(planHeader(bar(280, 0, 0, 10), c, 2, s));
    TEST_ASSERT_EQUAL_UINT8(1, s[0].form);   // 140 + 10 + 140 does not fit in 280; 100 does
}

int main(int, char **) {
    UNITY_BEGIN();
    RUN_TEST(test_places_stand_at_the_ends_and_the_centre);
    RUN_TEST(test_right_place_keeps_list_order);
    RUN_TEST(test_middle_never_moves);
    RUN_TEST(test_narrow_bar_sheds_in_keep_order);
    RUN_TEST(test_only_the_overflowing_side_gives_way);
    RUN_TEST(test_middle_gives_way_when_lowest);
    RUN_TEST(test_unwanted_claims_take_no_room);
    RUN_TEST(test_last_form_leaves_and_residents_stay);
    RUN_TEST(test_ties_give_way_from_the_later_claim);
    RUN_TEST(test_no_middle_sides_share_the_bar);
    return UNITY_END();
}
