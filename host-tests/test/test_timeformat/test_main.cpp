// TimeFormat (2.10e): every wording the panel uses for a time or a duration, pinned down - the
// owner's own words (his HA template, rounds 4-5). Run in UTC so local time is the epoch's.
#include <unity.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "TimeFormat.h"

static char buf[64];

// 2026-10-11 (a Sunday) 15:14:07 UTC, and the moments around it.
static const time_t NOW       = 1791731647;
static const time_t MIDNIGHT5 = 1791677100;   // 00:05 the same day
static const time_t NOON      = 1791720000;
static const time_t OCT10     = 1791645247;   // yesterday, 15:14:07
static const time_t OCT8      = 1791472447;   // a Thursday, three days before

void setUp() {
#ifdef _WIN32
    _putenv_s("TZ", "UTC");
#else
    setenv("TZ", "UTC0", 1);
#endif
    tzset();
}
void tearDown() {}

void test_clock() {
    tfClock(buf, sizeof(buf), NOW);                       TEST_ASSERT_EQUAL_STRING("3:14 PM", buf);
    tfClock(buf, sizeof(buf), NOW, TfClock::CLOCK_HMS);   TEST_ASSERT_EQUAL_STRING("3:14:07 PM", buf);
    tfClock(buf, sizeof(buf), MIDNIGHT5);                 TEST_ASSERT_EQUAL_STRING("12:05 AM", buf);
    tfClock(buf, sizeof(buf), NOON);                      TEST_ASSERT_EQUAL_STRING("12:00 PM", buf);
}

// "2 hrs 38 min": the two largest units, as the owner wrote it.
void test_duration_words() {
    tfDuration(buf, sizeof(buf), 2 * 3600 + 38 * 60);       TEST_ASSERT_EQUAL_STRING("2 hrs 38 min", buf);
    tfDuration(buf, sizeof(buf), 3600);                     TEST_ASSERT_EQUAL_STRING("1 hr", buf);
    tfDuration(buf, sizeof(buf), 45 * 60);                  TEST_ASSERT_EQUAL_STRING("45 min", buf);
    tfDuration(buf, sizeof(buf), 30);                       TEST_ASSERT_EQUAL_STRING("30 sec", buf);
    tfDuration(buf, sizeof(buf), 3 * 86400 + 4 * 3600);     TEST_ASSERT_EQUAL_STRING("3 days 4 hrs", buf);
    tfDuration(buf, sizeof(buf), 86400);                    TEST_ASSERT_EQUAL_STRING("1 day", buf);
    tfDuration(buf, sizeof(buf), -5);                       TEST_ASSERT_EQUAL_STRING("0 sec", buf);   // clock skew
}

void test_duration_compact() {
    tfDuration(buf, sizeof(buf), 2 * 3600 + 38 * 60, TfDur::DUR_COMPACT);   TEST_ASSERT_EQUAL_STRING("2h 38m", buf);
    tfDuration(buf, sizeof(buf), 45 * 60, TfDur::DUR_COMPACT);              TEST_ASSERT_EQUAL_STRING("45m", buf);
    tfDuration(buf, sizeof(buf), 30, TfDur::DUR_COMPACT);                   TEST_ASSERT_EQUAL_STRING("30s", buf);
    tfDuration(buf, sizeof(buf), 3 * 86400 + 4 * 3600, TfDur::DUR_COMPACT); TEST_ASSERT_EQUAL_STRING("3d 4h", buf);
}

// The owner's HA template: within a quarter hour of the hour "2 hours", past it "2+ hours".
void test_ago() {
    tfAgo(buf, sizeof(buf), NOW - 30, NOW);               TEST_ASSERT_EQUAL_STRING("A few moments ago", buf);
    tfAgo(buf, sizeof(buf), NOW - 60, NOW);               TEST_ASSERT_EQUAL_STRING("1 minute ago", buf);
    tfAgo(buf, sizeof(buf), NOW - 12 * 60, NOW);          TEST_ASSERT_EQUAL_STRING("12 minutes ago", buf);
    tfAgo(buf, sizeof(buf), NOW - 3600, NOW);             TEST_ASSERT_EQUAL_STRING("1 hour ago", buf);
    tfAgo(buf, sizeof(buf), NOW - 130 * 60, NOW);         TEST_ASSERT_EQUAL_STRING("2 hours ago", buf);
    tfAgo(buf, sizeof(buf), NOW - 140 * 60, NOW);         TEST_ASSERT_EQUAL_STRING("2+ hours ago", buf);
    tfAgo(buf, sizeof(buf), OCT10, NOW);                  TEST_ASSERT_EQUAL_STRING("Yesterday", buf);
    tfAgo(buf, sizeof(buf), OCT8, NOW);                   TEST_ASSERT_EQUAL_STRING("3 days ago", buf);
    tfAgo(buf, sizeof(buf), OCT8, NOW, TfAgo::AGO_DATED); TEST_ASSERT_EQUAL_STRING("3 days ago - Oct 8", buf);
}

// Calendar days, immune to the hour and to a year's end.
void test_days_ago() {
    TEST_ASSERT_EQUAL_INT(0, tfDaysAgo(MIDNIGHT5, NOW));
    TEST_ASSERT_EQUAL_INT(1, tfDaysAgo(OCT10, NOW));
    TEST_ASSERT_EQUAL_INT(3, tfDaysAgo(OCT8, NOW));
    TEST_ASSERT_EQUAL_INT(1, tfDaysAgo(1767222000 /* 2025-12-31 23:00 */, 1767229200 /* 2026-01-01 01:00 */));
}

void test_day_headings() {
    tfDay(buf, sizeof(buf), NOW, NOW);     TEST_ASSERT_EQUAL_STRING("Today, October 11, 2026", buf);
    tfDay(buf, sizeof(buf), OCT10, NOW);   TEST_ASSERT_EQUAL_STRING("Yesterday, October 10, 2026", buf);
    tfDay(buf, sizeof(buf), OCT8, NOW);    TEST_ASSERT_EQUAL_STRING("Thursday, October 8, 2026", buf);
}

void test_when() {
    tfWhen(buf, sizeof(buf), NOW, NOW);                        TEST_ASSERT_EQUAL_STRING("3:14 PM", buf);
    tfWhen(buf, sizeof(buf), OCT10, NOW);                      TEST_ASSERT_EQUAL_STRING("3:14 PM", buf);
    tfWhen(buf, sizeof(buf), OCT10, NOW, TfWhen::WHEN_FULL);   TEST_ASSERT_EQUAL_STRING("Yesterday 3:14 PM", buf);
    tfWhen(buf, sizeof(buf), OCT8, NOW);                       TEST_ASSERT_EQUAL_STRING("Oct 8, 3:14 PM", buf);
}

// Never more than n bytes, always terminated.
void test_short_buffers_are_terminated() {
    char small[6];
    tfDay(small, sizeof(small), NOW, NOW);
    TEST_ASSERT_EQUAL_STRING("Today", small);
}

int main(int, char **) {
    UNITY_BEGIN();
    RUN_TEST(test_clock);
    RUN_TEST(test_duration_words);
    RUN_TEST(test_duration_compact);
    RUN_TEST(test_ago);
    RUN_TEST(test_days_ago);
    RUN_TEST(test_day_headings);
    RUN_TEST(test_when);
    RUN_TEST(test_short_buffers_are_terminated);
    return UNITY_END();
}
