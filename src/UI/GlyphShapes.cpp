// The status glyphs as data (2.8 round 3). See include/UI/GlyphShapes.h.
#include "UI/GlyphShapes.h"

#include <math.h>

namespace Glyphs {

namespace {

using K = StrokeKind;

// WI-FI: the Linen mock's fan ("M2 9a15 15 0 0 1 20 0", ...) as arcs about
// their own centres, moved so the ink sits on the grid's centre (the fan's
// origin used to sit four fifths down its box, so its ink sat low).
const Stroke WIFI[] = {
    { K::ARC,  WIFI_ARC1,  12.0f, 18.92f,  5.0f, 225.6f, 314.4f },
    { K::ARC,  WIFI_ARC2,  12.0f, 18.99f, 10.0f, 225.6f, 314.4f },
    { K::ARC,  WIFI_ARC3,  12.0f, 19.53f, 15.0f, 228.2f, 311.8f },
    { K::DOT,  WIFI_DOT,   12.0f, 18.85f,  1.7f,   0.0f,   0.0f },
    { K::LINE, WIFI_STALK, 12.0f, 18.85f, 12.0f,  13.35f,  0.0f },
};

// THE LINK: two opposing arrows, publish and subscribe - the mock's, larger,
// with no square of their own (a frame is the square).
const Stroke LINK[] = {
    { K::LINE, 0,  4.0f,  8.5f, 20.0f,  8.5f, 0.0f },
    { K::LINE, 0, 20.0f,  8.5f, 16.0f,  4.5f, 0.0f },
    { K::LINE, 0, 20.0f, 15.5f,  4.0f, 15.5f, 0.0f },
    { K::LINE, 0,  4.0f, 15.5f,  8.0f, 19.5f, 0.0f },
};

// THE AP MAST (D-27, "B"): a mast, its tip, and two waves to either side.
const Stroke AP_MAST[] = {
    { K::LINE, 0, 12.0f, 12.3f, 12.0f, 20.5f,   0.0f },
    { K::DOT,  0, 12.0f, 10.0f,  1.8f,  0.0f,   0.0f },
    { K::ARC,  0, 12.0f, 10.0f,  5.0f, 135.0f, 225.0f },
    { K::ARC,  0, 12.0f, 10.0f,  5.0f, 315.0f,  45.0f },
    { K::ARC,  0, 12.0f, 10.0f,  9.0f, 135.0f, 225.0f },
    { K::ARC,  0, 12.0f, 10.0f,  9.0f, 315.0f,  45.0f },
};

// THE BELL: a dome, its sides, the flared rim, the clapper.
#define BELL_STROKES \
    { K::ARC,  0, 12.0f,  9.9f,  5.8f, 180.0f, 360.0f }, \
    { K::LINE, 0,  6.2f, 15.1f,  6.2f,  9.9f,   0.0f }, \
    { K::LINE, 0, 17.8f,  9.9f, 17.8f, 15.1f,   0.0f }, \
    { K::LINE, 0, 17.8f, 15.1f, 19.4f, 17.1f,   0.0f }, \
    { K::LINE, 0, 19.4f, 17.1f,  4.6f, 17.1f,   0.0f }, \
    { K::LINE, 0,  4.6f, 17.1f,  6.2f, 15.1f,   0.0f }, \
    { K::LINE, 0, 10.0f, 19.9f, 14.0f, 19.9f,   0.0f }

const Stroke BELL[] = { BELL_STROKES };

// RINGING: the bell and a stroke either side - the 12-hour notice (D-21).
const Stroke BELL_RING[] = {
    BELL_STROKES,
    { K::LINE, 0,  3.0f,  7.4f,  5.4f,  3.4f, 0.0f },
    { K::LINE, 0, 21.0f,  7.4f, 18.6f,  3.4f, 0.0f },
};
#undef BELL_STROKES

struct Table { const Stroke *s; uint8_t n; };
#define N(a) (uint8_t)(sizeof(a) / sizeof(a[0]))
const Table TABLES[] = {
    { WIFI, N(WIFI) }, { LINK, N(LINK) }, { AP_MAST, N(AP_MAST) }, { BELL, N(BELL) }, { BELL_RING, N(BELL_RING) },
};
#undef N

bool inSpan(float deg, float a0, float a1) {
    return a0 <= a1 ? (deg >= a0 && deg <= a1) : (deg >= a0 || deg <= a1);
}

void grow(Box &b, float x, float y) {
    if (x < b.x0) b.x0 = x;
    if (x > b.x1) b.x1 = x;
    if (y < b.y0) b.y0 = y;
    if (y > b.y1) b.y1 = y;
}

}  // namespace

uint8_t strokes(Shape s, const Stroke **out) {
    const uint8_t i = (uint8_t)s;
    if (i >= (uint8_t)Shape::SHAPE_COUNT) { *out = nullptr; return 0; }
    *out = TABLES[i].s;
    return TABLES[i].n;
}

Box inkBox(Shape s) {
    const Stroke *st;
    const uint8_t n = strokes(s, &st);
    Box b = { 1e9f, 1e9f, -1e9f, -1e9f };
    const float h = STROKE / 2.0f;
    const float rad = 3.14159265f / 180.0f;
    for (uint8_t i = 0; i < n; i++) {
        const Stroke &k = st[i];
        if (k.kind == StrokeKind::DOT) {
            grow(b, k.x - k.p, k.y - k.p);
            grow(b, k.x + k.p, k.y + k.p);
        } else if (k.kind == StrokeKind::LINE) {
            grow(b, k.x - h, k.y - h); grow(b, k.x + h, k.y + h);
            grow(b, k.p - h, k.q - h); grow(b, k.p + h, k.q + h);
        } else {
            // Its ends, and wherever it crosses a compass point.
            const float ang[6] = { k.q, k.a1, 0.0f, 90.0f, 180.0f, 270.0f };
            for (uint8_t j = 0; j < 6; j++) {
                float a = ang[j];
                if (j >= 2 && !inSpan(a, k.q, k.a1) && !(a == 0.0f && inSpan(360.0f, k.q, k.a1))) continue;
                const float px = k.x + k.p * cosf(a * rad), py = k.y + k.p * sinf(a * rad);
                grow(b, px - h, py - h);
                grow(b, px + h, py + h);
            }
        }
    }
    return b;
}

}  // namespace Glyphs
