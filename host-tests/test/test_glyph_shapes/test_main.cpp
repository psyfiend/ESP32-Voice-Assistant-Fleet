// The status glyphs as data (2.8 round 3, D-24): every glyph inside its 24-unit grid and centred
// on it - the owner's T9: the Wi-Fi fan sat low and its dot off to the left.
#include <unity.h>
#include "UI/GlyphShapes.h"

using namespace Glyphs;

void setUp() {}
void tearDown() {}

static void inside_and_centred(Shape s) {
    const Box b = inkBox(s);
    TEST_ASSERT_TRUE(b.x0 >= 0.0f && b.y0 >= 0.0f);
    TEST_ASSERT_TRUE(b.x1 <= GRID && b.y1 <= GRID);
    TEST_ASSERT_FLOAT_WITHIN(0.5f, 12.0f, (b.x0 + b.x1) / 2.0f);
    TEST_ASSERT_FLOAT_WITHIN(0.5f, 12.0f, (b.y0 + b.y1) / 2.0f);
}

void test_wifi_is_centred() { inside_and_centred(Shape::WIFI); }
void test_link_is_centred() { inside_and_centred(Shape::LINK); }
void test_ap_mast_is_centred() { inside_and_centred(Shape::AP_MAST); }
void test_bell_is_centred() { inside_and_centred(Shape::BELL); }
void test_bell_ring_is_centred() { inside_and_centred(Shape::BELL_RING); }

// The fan fits a square frame: the mock's narrower fan, at most 23 units wide and 18 tall.
void test_wifi_fits_a_square() {
    const Box b = inkBox(Shape::WIFI);
    TEST_ASSERT_TRUE(b.x1 - b.x0 <= 23.0f);
    TEST_ASSERT_TRUE(b.y1 - b.y0 <= 18.0f);
}

// The dot sits on the fan's axis, under the arcs, each of which is symmetric about it.
void test_wifi_dot_on_the_axis() {
    const Stroke *st;
    const uint8_t n = strokes(Shape::WIFI, &st);
    for (uint8_t i = 0; i < n; i++) {
        TEST_ASSERT_FLOAT_WITHIN(0.01f, 12.0f, st[i].x);
        if (st[i].kind == StrokeKind::ARC)
            TEST_ASSERT_FLOAT_WITHIN(0.01f, 540.0f, st[i].q + st[i].a1);   // start and end mirror about 270
    }
}

int main(int, char **) {
    UNITY_BEGIN();
    RUN_TEST(test_wifi_is_centred);
    RUN_TEST(test_link_is_centred);
    RUN_TEST(test_ap_mast_is_centred);
    RUN_TEST(test_bell_is_centred);
    RUN_TEST(test_bell_ring_is_centred);
    RUN_TEST(test_wifi_fits_a_square);
    RUN_TEST(test_wifi_dot_on_the_axis);
    return UNITY_END();
}
