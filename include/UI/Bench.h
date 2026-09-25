#pragma once
//
// Bench - GET /bench times where a frame goes: LVGL drawing it, or the flush
// getting it onto the glass. Milestone 2.9 (#67), step 1 of
// docs/design/display-stack.md: every later step of the esp_lcd migration is
// judged against these numbers.
//
//   GET /bench?n=20&what=full&page=0&deck=1
//
//   n      frames to time, 1-100 (default 20)
//   what   full - redraw the whole screen (a page change, the worst case)
//          card - redraw one card's area (a value changing, the common case)
//   page   swipe-order index to measure on (default: the page showing)
//   deck   0 or 1: deck hidden or shown (default: as it is)
//   keep   1: stay on that page/deck afterwards (scripts/bench.py uses it to
//          screenshot what was measured, then puts the board back itself)
//
// If page or deck differ from what is showing, the bench switches, lets the
// board settle for SETTLE_MS of ordinary loop() - the rebuild, the page toast,
// the HA values arriving - measures, and switches back unless told to keep.
// Returns JSON; the field meanings are in Bench.cpp above buildJson().
//
// Same threading as Screenshot, and for the same reason: the handler runs on
// the HTTP server's task and never touches LVGL. service(), called from
// loop(), does everything that does.
//
// Compiled in only with -D ENABLE_BENCH. No authentication, like /screenshot.
// The UI freezes for the length of the measurement: n x the frame time.
//
class HttpServer;
class GUIManager;

namespace Bench {

#ifdef ENABLE_BENCH
void begin(HttpServer &http, GUIManager &gui);
void service();   // every loop(), LVGL thread
#else
inline void begin(HttpServer &, GUIManager &) {}
inline void service() {}
#endif

} // namespace Bench
