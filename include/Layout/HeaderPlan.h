#pragma once
// ---------------------------------------------------------------------------
// HEADER PLAN (2.8 round 2, DECISIONS D-19) - where the page header's slots
// stand, as plain arithmetic: no LVGL, no Arduino, no ESP-IDF, so it is tested
// on the PC (host-tests/test_header_plan). docs/design/header-slots.md §10.
//
// The header has three PLACES: the left and the right, pinned to the bar's
// ends, and the middle, which stands on the bar's centre and never moves. Each
// slot in a place makes a CLAIM on its room: what it would show in each of its
// FORMS (form 0 the fullest - the alarm's bell and time; form 1 the bell), and
// how hard each form holds its room - its KEEP, a priority.
//
// When a side would run into the middle, the lowest keep among that side's
// claims and the middle's gives way, one step at a time: a claim drops to its
// next form, and from its last form it leaves the bar. A resident
// (KEEP_ALWAYS) never gives way. So a narrow board sheds what matters least,
// measured on the board rather than set per board - and a notice of high
// priority (#76) will hold its room against everything below it.
// ---------------------------------------------------------------------------
#include <stdint.h>

namespace Layout {

enum class HeaderPlace : uint8_t { PLACE_LEFT, PLACE_MIDDLE, PLACE_RIGHT };

constexpr uint8_t HEADER_CLAIMS_MAX = 8;
constexpr uint8_t HEADER_FORMS_MAX  = 3;
constexpr uint8_t KEEP_ALWAYS       = 255;   // a resident: never gives way

struct HeaderClaim {
    HeaderPlace place  = HeaderPlace::PLACE_LEFT;
    bool        wanted = true;    // its condition holds (an alarm is set); false: not on the bar
    uint8_t     forms  = 1;       // 1..HEADER_FORMS_MAX
    int32_t     width[HEADER_FORMS_MAX] = {};   // each form's width, px
    // keep[k]: how hard form k holds its room against stepping down to form
    // k + 1 - the priority of the extra it would lose. keep[forms - 1]: the
    // priority of being on the bar at all.
    uint8_t     keep[HEADER_FORMS_MAX] = {};
};

// The bar, in px.
struct HeaderBar {
    int32_t width = 0;
    int32_t edge  = 0;   // from each end of the bar to its side's first slot
    int32_t space = 0;   // between two slots in one place
    int32_t clear = 0;   // at least this between a side and the middle (or the other side)
};

// Where one claim went.
struct HeaderSpot {
    bool    shown = false;
    uint8_t form  = 0;
    int32_t x = 0, w = 0;   // in the bar
};

// Plan the bar: every claim's spot, in `out` (n of them). In each place the
// claims stand in list order, left to right. True when everything shown fits;
// false when a side still runs into the middle with nothing left that may give
// way - drawn anyway, and the caller says so.
bool planHeader(const HeaderBar &bar, const HeaderClaim *claims, uint8_t n, HeaderSpot *out);

}  // namespace Layout
