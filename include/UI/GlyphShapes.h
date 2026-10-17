#pragma once
// ---------------------------------------------------------------------------
// THE STATUS GLYPHS AS DATA (2.8 round 3, D-24, D-27) - each glyph's strokes on
// the 24-unit grid of the mocks they come from ("Linen Fill and Header Glyphs"
// for the Wi-Fi fan, "Fleet Status Glyphs" for the states, the round-3 canvas
// for the link, the AP mast and the bells). Pure numbers: no LVGL, so it is
// tested on the PC (host-tests/test_glyph_shapes) - every glyph inside its grid
// and centred on it, which is what the Wi-Fi fan was not.
//
// Kit::strokeGlyph() draws a shape at any size with LVGL's own round-capped
// arcs and lines; each stroke belongs to a PART, coloured on its own (a lit arc,
// an unlit one, the dot). A strike is the component's, over any glyph.
//
// Angles are LVGL's: degrees clockwise from east, the arc drawn clockwise from
// `a0` to `a1` (a1 < a0 wraps through east).
// ---------------------------------------------------------------------------
#include <stdint.h>

namespace Glyphs {

enum class Shape : uint8_t { WIFI, LINK, AP_MAST, BELL, BELL_RING, SHAPE_COUNT };
enum class StrokeKind : uint8_t { ARC, LINE, DOT };

struct Stroke {
    StrokeKind kind;
    uint8_t    part;
    float      x, y;    // ARC, DOT: the centre; LINE: one end
    float      p, q;    // ARC: radius, start angle; DOT: radius; LINE: the other end
    float      a1;      // ARC: end angle
};

// Wi-Fi's parts: the arcs, inner to outer, the dot, the stalk (no link).
constexpr uint8_t WIFI_ARC1 = 0, WIFI_ARC2 = 1, WIFI_ARC3 = 2, WIFI_DOT = 3, WIFI_STALK = 4;

constexpr float GRID   = 24.0f;
constexpr float STROKE = 2.2f;   // every stroke's width, grid units
constexpr uint8_t STROKES_MAX = 12;

// The glyph's strokes; returns how many.
uint8_t strokes(Shape s, const Stroke **out);

// The ink's bounds on the grid, stroke width included (the test's measure).
struct Box { float x0, y0, x1, y1; };
Box inkBox(Shape s);

}  // namespace Glyphs
