// The clock's arithmetic (#100): NINA's dial cases kept exactly as its own host test has them
// (test/host/test_clock_dial.c in ESP32-P4-NINA-Display by chvvkumar, non-commercial use with the
// author's permission - https://github.com/chvvkumar/ESP32-P4-NINA-Display/issues/326), then the
// Classic face's words and where it goes.
#include <unity.h>
#include <string.h>
#include "Clock/ClockMath.h"

using namespace Clock;

static int16_t start[10], span[10];
static uint8_t src[10];
static char buf[32];

void setUp() {}
void tearDown() {}

// 1-hour steps from 09:00: every entry drawn, each one hour mark.
void test_dial_one_hour_steps() {
    const uint8_t h[10] = { 9, 10, 11, 12, 13, 14, 15, 16, 17, 18 };
    TEST_ASSERT_EQUAL_INT(10, dialBlocks(h, 10, start, span, src));
    TEST_ASSERT_EQUAL_INT(270, start[0]);
    TEST_ASSERT_EQUAL_INT(30, span[0]);
    TEST_ASSERT_EQUAL_INT(0, start[3]);     // noon, back at twelve
    TEST_ASSERT_EQUAL_INT(180, start[9]);
    TEST_ASSERT_EQUAL_INT(30, span[9]);
    TEST_ASSERT_EQUAL_INT(9, src[9]);       // nothing skipped
}

// 3-hour steps with OpenWeatherMap's short first gap; the last block clipped to the half day.
void test_dial_three_hour_steps() {
    const uint8_t h[10] = { 10, 12, 15, 18, 21, 0, 3, 6, 9, 12 };
    TEST_ASSERT_EQUAL_INT(5, dialBlocks(h, 10, start, span, src));
    TEST_ASSERT_EQUAL_INT(300, start[0]);
    TEST_ASSERT_EQUAL_INT(60, span[0]);
    TEST_ASSERT_EQUAL_INT(0, start[1]);
    TEST_ASSERT_EQUAL_INT(90, span[1]);
    TEST_ASSERT_EQUAL_INT(270, start[4]);
    TEST_ASSERT_EQUAL_INT(30, span[4]);
}

// The grid's first slot behind now: skipped, and block 1 comes from entry 2.
void test_dial_grid_behind_now() {
    const uint8_t h[10] = { 13, 12, 15, 18, 21, 0, 3, 6, 9, 12 };
    TEST_ASSERT_EQUAL_INT(5, dialBlocks(h, 10, start, span, src));
    TEST_ASSERT_EQUAL_INT(30, start[0]);
    TEST_ASSERT_EQUAL_INT(60, span[0]);
    TEST_ASSERT_EQUAL_INT(2, src[1]);
    TEST_ASSERT_EQUAL_INT(0, start[4]);
    TEST_ASSERT_EQUAL_INT(30, span[4]);
}

void test_dial_grid_level_with_now() {
    const uint8_t h[10] = { 13, 13, 15, 18, 21, 0, 3, 6, 9, 12 };
    TEST_ASSERT_EQUAL_INT(5, dialBlocks(h, 10, start, span, src));
    TEST_ASSERT_EQUAL_INT(30, start[0]);
    TEST_ASSERT_EQUAL_INT(60, span[0]);
    TEST_ASSERT_EQUAL_INT(2, src[1]);
}

void test_dial_wraps_midnight() {
    const uint8_t h[10] = { 22, 23, 0, 1, 2, 3, 4, 5, 6, 7 };
    TEST_ASSERT_EQUAL_INT(10, dialBlocks(h, 10, start, span, src));
    TEST_ASSERT_EQUAL_INT(300, start[0]);
    TEST_ASSERT_EQUAL_INT(0, start[2]);
}

// A forecast that failed left every hour at zero: nothing is drawn.
void test_dial_all_zero_draws_nothing() {
    const uint8_t h[10] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
    TEST_ASSERT_EQUAL_INT(0, dialBlocks(h, 10, start, span, src));
    TEST_ASSERT_EQUAL_INT(0, dialBlocks(nullptr, 10, start, span, src));
    TEST_ASSERT_EQUAL_INT(0, dialBlocks(h, 0, start, span, src));
}

void test_dial_delta() {
    TEST_ASSERT_EQUAL_INT(-1, dialDelta(13, 12));
    TEST_ASSERT_EQUAL_INT(2, dialDelta(23, 1));
    TEST_ASSERT_EQUAL_INT(11, dialDelta(0, 11));
    TEST_ASSERT_EQUAL_INT(-12, dialDelta(0, 12));
}

void test_time_words() {
    timeText(buf, sizeof(buf), 9, 25, false);   TEST_ASSERT_EQUAL_STRING("9:25", buf);
    timeText(buf, sizeof(buf), 0, 5, false);    TEST_ASSERT_EQUAL_STRING("12:05", buf);   // midnight
    timeText(buf, sizeof(buf), 12, 0, false);   TEST_ASSERT_EQUAL_STRING("12:00", buf);   // noon
    timeText(buf, sizeof(buf), 23, 59, false);  TEST_ASSERT_EQUAL_STRING("11:59", buf);
    timeText(buf, sizeof(buf), 9, 25, true);    TEST_ASSERT_EQUAL_STRING("09:25", buf);
    timeText(buf, sizeof(buf), 0, 0, true);     TEST_ASSERT_EQUAL_STRING("00:00", buf);
    TEST_ASSERT_EQUAL_STRING("A M", meridiem(0, false));
    TEST_ASSERT_EQUAL_STRING("A M", meridiem(11, false));
    TEST_ASSERT_EQUAL_STRING("P M", meridiem(12, false));
    TEST_ASSERT_EQUAL_STRING("", meridiem(15, true));
}

void test_date_words() {
    TEST_ASSERT_EQUAL_STRING("SUNDAY", weekdayName(0));
    TEST_ASSERT_EQUAL_STRING("TUESDAY", weekdayName(2));
    TEST_ASSERT_EQUAL_STRING("", weekdayName(7));
    dateText(buf, sizeof(buf), 9, 14);   TEST_ASSERT_EQUAL_STRING("OCTOBER 14", buf);
    dateText(buf, sizeof(buf), 0, 1);    TEST_ASSERT_EQUAL_STRING("JANUARY 1", buf);
    char small[5];
    dateText(small, sizeof(small), 8, 30);   TEST_ASSERT_EQUAL_STRING("SEPT", small);   // cut, terminated
}

void test_where_it_goes() {
    TEST_ASSERT_EQUAL_INT32(340, squareSide(340, 540));
    TEST_ASSERT_EQUAL_INT32(440, squareSide(500, 440));
    TEST_ASSERT_EQUAL_INT32(0, squareSide(-4, 100));
    const int32_t w[3] = { 200, 270, 360 };
    TEST_ASSERT_EQUAL_UINT8(2, pickFace(w, 3, 400));
    TEST_ASSERT_EQUAL_UINT8(1, pickFace(w, 3, 300));
    TEST_ASSERT_EQUAL_UINT8(0, pickFace(w, 3, 100));   // none fits: the smallest
    TEST_ASSERT_EQUAL_INT32(40, classicPad(720));
    TEST_ASSERT_EQUAL_INT32(24, classicPad(440));
    TEST_ASSERT_EQUAL_INT32(1, classicPad(10));
}

int main(int, char **) {
    UNITY_BEGIN();
    RUN_TEST(test_dial_one_hour_steps);
    RUN_TEST(test_dial_three_hour_steps);
    RUN_TEST(test_dial_grid_behind_now);
    RUN_TEST(test_dial_grid_level_with_now);
    RUN_TEST(test_dial_wraps_midnight);
    RUN_TEST(test_dial_all_zero_draws_nothing);
    RUN_TEST(test_dial_delta);
    RUN_TEST(test_time_words);
    RUN_TEST(test_date_words);
    RUN_TEST(test_where_it_goes);
    return UNITY_END();
}
