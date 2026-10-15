// Layout's flow engine (2.12 P5): the dashboard's placement rules, each pinned down by a test.
// Units: at the default subdivision of 2, one cell is 2x2 units.
#include <unity.h>
#include "Layout/Layout.h"

using namespace Layout;

static Item cell(uint8_t priority = 128) {
    Item it;
    it.prefSpanX = it.prefSpanY = it.minSpanX = it.minSpanY = 2;
    it.priority = priority;
    return it;
}

void setUp() {}
void tearDown() {}

// Left to right, then down: the reading order the dashboard has always had.
void test_flow_fills_rows_left_to_right() {
    FlowEngine e;
    const Container c{4, 4};   // 2 x 2 cells
    Item it[4] = {cell(), cell(), cell(), cell()};
    Place p[4];
    TEST_ASSERT_EQUAL_UINT8(4, e.place(c, it, 4, p));
    TEST_ASSERT_EQUAL_UINT8(0, p[0].col); TEST_ASSERT_EQUAL_UINT8(0, p[0].row);
    TEST_ASSERT_EQUAL_UINT8(2, p[1].col); TEST_ASSERT_EQUAL_UINT8(0, p[1].row);
    TEST_ASSERT_EQUAL_UINT8(0, p[2].col); TEST_ASSERT_EQUAL_UINT8(2, p[2].row);
    TEST_ASSERT_EQUAL_UINT8(2, p[3].col); TEST_ASSERT_EQUAL_UINT8(2, p[3].row);
}

// The first free position wins, and there the widest span that fits: a card that
// fits beside its neighbour at less than it wanted beats one alone on a new row.
void test_widest_span_that_fits_at_the_first_free_position() {
    FlowEngine e;
    const Container c{6, 4};
    Item b = cell();
    b.prefSpanX = 6;   // wants the whole row; takes 4 beside the first
    Item it[2] = {cell(), b};
    Place p[2];
    TEST_ASSERT_EQUAL_UINT8(2, e.place(c, it, 2, p));
    TEST_ASSERT_EQUAL_UINT8(2, p[1].col);
    TEST_ASSERT_EQUAL_UINT8(0, p[1].row);
    TEST_ASSERT_EQUAL_UINT8(4, p[1].spanX);
}

// A pin is a stronger statement than flow: pinned items are placed first,
// whatever their order, so flow cannot take their spot.
void test_pinned_items_go_first() {
    FlowEngine e;
    const Container c{4, 2};
    Item b = cell();
    b.col = 0; b.row = 0;
    Item it[2] = {cell(), b};
    Place p[2];
    TEST_ASSERT_EQUAL_UINT8(2, e.place(c, it, 2, p));
    TEST_ASSERT_EQUAL_UINT8(0, p[1].col);
    TEST_ASSERT_EQUAL_UINT8(2, p[0].col);
    TEST_ASSERT_FALSE(p[1].pinFail);
}

static int s_pinEvents = 0;
static void onPin(void *, uint8_t, int8_t, int8_t) { s_pinEvents++; }

// A bad pin is reported and flowed, never dropped - and the report stays across
// later calls, which reset everything else.
void test_bad_pin_is_flowed_and_reported() {
    FlowEngine e;
    const Container c{4, 2};
    Item a = cell();
    a.col = 3; a.row = 0;   // a 2-wide item at column 3 of 4: out of bounds
    Item it[1] = {a};
    Place p[1];
    Events ev;
    ev.pinRejected = onPin;
    s_pinEvents = 0;
    TEST_ASSERT_EQUAL_UINT8(1, e.place(c, it, 1, p, &ev));
    TEST_ASSERT_TRUE(p[0].placed);
    TEST_ASSERT_TRUE(p[0].pinFail);
    TEST_ASSERT_EQUAL_UINT8(0, p[0].col);
    TEST_ASSERT_EQUAL_INT(1, s_pinEvents);

    // Moved somewhere it fits, the pin holds - and the earlier report is kept.
    it[0].col = 2;
    TEST_ASSERT_EQUAL_UINT8(1, e.place(c, it, 1, p));
    TEST_ASSERT_EQUAL_UINT8(2, p[0].col);
    TEST_ASSERT_TRUE(p[0].pinFail);
}

static int s_dropped = -1;
static void onDrop(void *, uint8_t item, uint8_t) { s_dropped = item; }

// Out of room, the LOWEST priority goes - not the item that happened to fail -
// and among equals the later one, so an author's reading order survives.
void test_lowest_priority_is_dropped_ties_drop_the_later() {
    FlowEngine e;
    const Container c{4, 2};   // room for two cells
    Item it[3] = {cell(200), cell(50), cell(50)};
    Place p[3];
    Events ev;
    ev.dropped = onDrop;
    s_dropped = -1;
    TEST_ASSERT_EQUAL_UINT8(2, e.place(c, it, 3, p, &ev));
    TEST_ASSERT_TRUE(p[0].placed);
    TEST_ASSERT_TRUE(p[1].placed);
    TEST_ASSERT_FALSE(p[2].placed);
    TEST_ASSERT_EQUAL_INT(2, s_dropped);
}

// The lowest priority goes even when it was declared FIRST and a later item is
// the one that did not fit.
void test_the_dropped_item_is_the_lowest_not_the_one_that_failed() {
    FlowEngine e;
    const Container c{4, 2};
    Item it[3] = {cell(10), cell(200), cell(200)};
    Place p[3];
    TEST_ASSERT_EQUAL_UINT8(2, e.place(c, it, 3, p));
    TEST_ASSERT_FALSE(p[0].placed);
    TEST_ASSERT_EQUAL_UINT8(0, p[1].col);
    TEST_ASSERT_EQUAL_UINT8(2, p[2].col);
}

// A card made for a bigger board takes the widest span the container has.
void test_spans_are_clamped_to_the_container() {
    FlowEngine e;
    const Container c{4, 4};
    Item a = cell();
    a.prefSpanX = 8;
    a.minSpanX  = 6;
    Item it[1] = {a};
    Place p[1];
    TEST_ASSERT_EQUAL_UINT8(1, e.place(c, it, 1, p));
    TEST_ASSERT_EQUAL_UINT8(4, p[0].spanX);
}

// An empty slot is never placed and never counted.
void test_absent_items_are_skipped() {
    FlowEngine e;
    const Container c{4, 2};
    Item gap;
    gap.present = false;
    Item it[2] = {gap, cell()};
    Place p[2];
    TEST_ASSERT_EQUAL_UINT8(1, e.place(c, it, 2, p));
    TEST_ASSERT_FALSE(p[0].placed);
    TEST_ASSERT_EQUAL_UINT8(0, p[1].col);
}

// The page asks for as few rows as the area allows: whole cells each, rounded up.
void test_rows_wanted_is_a_floor_in_whole_cells() {
    Item it[5] = {cell(), cell(), cell(), cell(), cell()};
    TEST_ASSERT_EQUAL_UINT8(3, rowsWanted(it, 5, 2, 2));   // 5 cells on 2 columns
    Item half = cell();
    half.prefSpanX = 1; half.prefSpanY = 1;                  // half a cell still takes one
    Item h[2] = {half, half};
    TEST_ASSERT_EQUAL_UINT8(1, rowsWanted(h, 2, 2, 2));
    TEST_ASSERT_EQUAL_UINT8(1, rowsWanted(h, 0, 2, 2));
}

// ...and never fewer than the tallest item's cells (#100): a lone 2 x 2-cell card on six
// columns has the area of one row, but needs two.
void test_rows_wanted_fits_the_tallest_item() {
    Item big = cell();
    big.prefSpanX = 4; big.prefSpanY = 4; big.minSpanX = 4; big.minSpanY = 4;
    Item one[1] = {big};
    TEST_ASSERT_EQUAL_UINT8(2, rowsWanted(one, 1, 6, 2));
    Item mix[3] = {big, cell(), cell()};
    TEST_ASSERT_EQUAL_UINT8(2, rowsWanted(mix, 3, 6, 2));   // 6 cells on 6 columns, but 2 tall
    // ...and placed whole at the top left on that many rows.
    FlowEngine e;
    Container c{12, 4};
    Place p[1];
    TEST_ASSERT_EQUAL_UINT8(1, e.place(c, one, 1, p));
    TEST_ASSERT_TRUE(p[0].placed);
    TEST_ASSERT_EQUAL_UINT8(0, p[0].col);
    TEST_ASSERT_EQUAL_UINT8(0, p[0].row);
    TEST_ASSERT_EQUAL_UINT8(4, p[0].spanY);
}

int main(int, char **) {
    UNITY_BEGIN();
    RUN_TEST(test_flow_fills_rows_left_to_right);
    RUN_TEST(test_widest_span_that_fits_at_the_first_free_position);
    RUN_TEST(test_pinned_items_go_first);
    RUN_TEST(test_bad_pin_is_flowed_and_reported);
    RUN_TEST(test_lowest_priority_is_dropped_ties_drop_the_later);
    RUN_TEST(test_the_dropped_item_is_the_lowest_not_the_one_that_failed);
    RUN_TEST(test_spans_are_clamped_to_the_container);
    RUN_TEST(test_absent_items_are_skipped);
    RUN_TEST(test_rows_wanted_is_a_floor_in_whole_cells);
    RUN_TEST(test_rows_wanted_fits_the_tallest_item);
    return UNITY_END();
}
