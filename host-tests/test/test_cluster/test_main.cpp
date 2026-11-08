// Cluster arithmetic: how mini tiles share a cluster, and how many
// cells a cluster asks for. Sizes are the P4_5's and the 4B's in real pixels.
#include <unity.h>
#include "Layout/Cluster.h"

using namespace Layout;

void setUp() {}
void tearDown() {}

// Two tiles in a wide, short box stand side by side - the House page's clusters.
void test_two_tiles_side_by_side_in_a_wide_box() {
    const TileGrid g = tileGrid(2, 470, 105, 9);
    TEST_ASSERT_EQUAL_UINT8(2, g.cols);
    TEST_ASSERT_EQUAL_UINT8(1, g.rows);
    TEST_ASSERT_EQUAL_INT32(230, g.tileW);
    TEST_ASSERT_EQUAL_INT32(105, g.tileH);
    TEST_ASSERT_EQUAL_INT32(230, g.lastW);
}

// Two tiles in a tall box stack.
void test_two_tiles_stack_in_a_tall_box() {
    const TileGrid g = tileGrid(2, 100, 300, 10);
    TEST_ASSERT_EQUAL_UINT8(1, g.cols);
    TEST_ASSERT_EQUAL_UINT8(2, g.rows);
}

// Four tiles in a squarish box make two rows of two, not a ribbon.
void test_four_tiles_in_a_square_box_are_two_by_two() {
    const TileGrid g = tileGrid(4, 300, 300, 10);
    TEST_ASSERT_EQUAL_UINT8(2, g.cols);
    TEST_ASSERT_EQUAL_UINT8(2, g.rows);
}

// The arranger's column count (v8's tc) is taken as given: four tiles in a row in a box
// that would otherwise make two rows of two.
void test_the_arrangers_columns_are_obeyed() {
    const TileGrid g = tileGrid(4, 300, 300, 10, 4);
    TEST_ASSERT_EQUAL_UINT8(4, g.cols);
    TEST_ASSERT_EQUAL_UINT8(1, g.rows);
    TEST_ASSERT_EQUAL_INT32(67, g.tileW);
    TEST_ASSERT_EQUAL_INT32(300, g.tileH);
    // More columns than tiles: the rule chooses, as with none given.
    const TileGrid h = tileGrid(4, 300, 300, 10, 6);
    TEST_ASSERT_EQUAL_UINT8(2, h.cols);
}

// A short last row is not ragged: its tiles share the row.
void test_short_last_row_shares_the_width() {
    const TileGrid g = tileGrid(3, 300, 300, 10);
    TEST_ASSERT_EQUAL_UINT8(2, g.cols);
    TEST_ASSERT_EQUAL_UINT8(2, g.rows);
    TEST_ASSERT_EQUAL_INT32(145, g.tileW);
    TEST_ASSERT_EQUAL_INT32(300, g.lastW);
    int32_t x, y, w;
    tileAt(g, 3, 2, 10, x, y, w);
    TEST_ASSERT_EQUAL_INT32(0, x);
    TEST_ASSERT_EQUAL_INT32(155, y);
    TEST_ASSERT_EQUAL_INT32(300, w);
    tileAt(g, 3, 1, 10, x, y, w);
    TEST_ASSERT_EQUAL_INT32(155, x);
    TEST_ASSERT_EQUAL_INT32(0, y);
    TEST_ASSERT_EQUAL_INT32(145, w);
}

void test_nothing_to_lay_out() {
    const TileGrid g = tileGrid(0, 300, 300, 10);
    TEST_ASSERT_EQUAL_INT32(0, g.tileW);
}

// The P4_5 at 5 x 3: a cell 250 x 190 px, a 60 px header and 13 px padding.
static ClusterGeom p45() {
    ClusterGeom g;
    g.cellW = 250; g.cellH = 190; g.gap = 21;
    g.chromeW = 2 * 13 + 2; g.chromeH = 60 + 2 * 13 + 2;
    g.tileGap = 9; g.minTile = 87;   // 7.5 mm at 293.7 PPI
    return g;
}

// Two tiles ask for two cells side by side (the House page's Office, Kitchen, Outside).
void test_two_tiles_take_two_cells_across() {
    const ClusterSpan s = clusterSpan(2, p45(), 5, 3);
    TEST_ASSERT_TRUE(s.fits);
    TEST_ASSERT_EQUAL_UINT8(2, s.cols);
    TEST_ASSERT_EQUAL_UINT8(1, s.rows);
}

// One cell only where it is allowed (K103: sensors, no header).
void test_one_cell_only_when_allowed() {
    ClusterGeom g = p45();
    g.chromeH = 2 * 13 + 2;   // no header
    g.oneCell = true;
    const ClusterSpan s = clusterSpan(1, g, 5, 3);
    TEST_ASSERT_TRUE(s.fits);
    TEST_ASSERT_EQUAL_UINT8(1, s.cols);
    TEST_ASSERT_EQUAL_UINT8(1, s.rows);
    g.oneCell = false;
    const ClusterSpan t = clusterSpan(1, g, 5, 3);
    TEST_ASSERT_EQUAL_UINT8(2, t.cols);
}

// Six tiles do not fit two cells across at the minimum (74 px each); the
// fewest cells that hold them are two cells stacked, the tiles 2 x 3 (106 x 98).
void test_many_tiles_take_more_cells() {
    const ClusterSpan s = clusterSpan(6, p45(), 5, 3);
    TEST_ASSERT_TRUE(s.fits);
    TEST_ASSERT_EQUAL_UINT8(1, s.cols);
    TEST_ASSERT_EQUAL_UINT8(2, s.rows);
}

// A board too narrow for the block: it does not fit, and is shown as its card.
void test_too_narrow_does_not_fit() {
    const ClusterSpan s = clusterSpan(2, p45(), 1, 1);
    TEST_ASSERT_FALSE(s.fits);
}

// THE GROUP VIEW (K159). The 1060's window, four tiles, the biggest a tile may be
// 25 x 20 mm (167 x 133 px at 169.5 PPI): two by two at the most, not a ribbon.
void test_capped_four_tiles_two_by_two_at_the_cap() {
    TileCap cap;
    cap.maxW = 167; cap.maxH = 133; cap.tall = 1.1f; cap.wide = 2.0f;
    const TileGrid g = cappedGrid(4, 560, 330, 5, cap);
    TEST_ASSERT_EQUAL_UINT8(2, g.cols);
    TEST_ASSERT_EQUAL_UINT8(2, g.rows);
    TEST_ASSERT_EQUAL_INT32(167, g.tileW);
    TEST_ASSERT_EQUAL_INT32(133, g.tileH);
    TEST_ASSERT_EQUAL_INT32(167, g.lastW);
}

// A short last row's tile is the size of the rest, centred under them; the
// spare room spread evenly between the tiles and at the edges (K162).
void test_capped_last_row_centred_not_stretched() {
    TileCap cap;
    cap.maxW = 100; cap.maxH = 100;
    const TileGrid g = cappedGrid(3, 300, 300, 10, cap);
    TEST_ASSERT_EQUAL_UINT8(2, g.cols);
    TEST_ASSERT_EQUAL_UINT8(2, g.rows);
    int32_t x, y;
    cappedTileAt(g, 3, 0, 10, 300, 300, x, y);
    TEST_ASSERT_EQUAL_INT32(33, x);
    TEST_ASSERT_EQUAL_INT32(33, y);
    cappedTileAt(g, 3, 1, 10, 300, 300, x, y);
    TEST_ASSERT_EQUAL_INT32(166, x);
    cappedTileAt(g, 3, 2, 10, 300, 300, x, y);
    TEST_ASSERT_EQUAL_INT32(100, x);
    TEST_ASSERT_EQUAL_INT32(166, y);
}

// No spare room: the tiles' own gap, nothing at the edges.
void test_capped_full_box_keeps_the_gap() {
    TileCap cap;
    const TileGrid g = cappedGrid(2, 210, 100, 10, cap);
    int32_t x, y;
    cappedTileAt(g, 2, 1, 10, 210, 100, x, y);
    TEST_ASSERT_EQUAL_INT32(110, x);
    TEST_ASSERT_EQUAL_INT32(0, y);
}

// No wider than twice its height.
void test_capped_never_twice_as_wide_as_tall() {
    TileCap cap;
    cap.wide = 2.0f;
    const TileGrid g = cappedGrid(1, 400, 100, 10, cap);
    TEST_ASSERT_EQUAL_INT32(200, g.tileW);
    TEST_ASSERT_EQUAL_INT32(100, g.tileH);
}

int main(int, char **) {
    UNITY_BEGIN();
    RUN_TEST(test_two_tiles_side_by_side_in_a_wide_box);
    RUN_TEST(test_two_tiles_stack_in_a_tall_box);
    RUN_TEST(test_four_tiles_in_a_square_box_are_two_by_two);
    RUN_TEST(test_the_arrangers_columns_are_obeyed);
    RUN_TEST(test_short_last_row_shares_the_width);
    RUN_TEST(test_nothing_to_lay_out);
    RUN_TEST(test_two_tiles_take_two_cells_across);
    RUN_TEST(test_one_cell_only_when_allowed);
    RUN_TEST(test_many_tiles_take_more_cells);
    RUN_TEST(test_too_narrow_does_not_fit);
    RUN_TEST(test_capped_four_tiles_two_by_two_at_the_cap);
    RUN_TEST(test_capped_last_row_centred_not_stretched);
    RUN_TEST(test_capped_never_twice_as_wide_as_tall);
    RUN_TEST(test_capped_full_box_keeps_the_gap);
    return UNITY_END();
}
