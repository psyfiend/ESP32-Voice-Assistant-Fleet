// The sparks along the window's edge (2.12 P1, split from CardPopup.cpp).
#include "PopupState.h"

namespace popup {

IntfMode s_intfMode = IntfMode::INTF_NOW_AND_THEN;

// ---------------------------------------------------------------------------
// Interference: a spark that travels the window's edge (owner, 2026-10-04)
//
// The window is a projection, and now and then a spark of it runs along a
// short stretch of its border: a bright core in a wider glow, a fading tail
// behind, the odd fleck thrown off. It travels a section of the edge, then
// often runs again a little further on, overlapping the last run - so it reads
// as something CRAWLING along the border. Round 3 (owner, 2026-10-05): faster
// and more frenetic, more runs, each with its own dice - see intfTick().
//
// Round 1 was ten scattered pieces living one to three frames, all over the
// edge, all at once. The owner: it looked like "small porch timing issues or
// the wrong pclk frequency" - a real fault - not like electricity, and on a
// light scheme it could not be seen at all. So now: ONE spark at a time, slow
// enough to follow (a run was 250-600 ms; 120-300 ms since round 8, see
// intfNextRun()), in one place (a quarter of a side
// or less), in electric colours - whites and pale blues on a dark scheme,
// deep blues and violet on a light one. Every roll of the dice is new: which
// edge, where on it, how long the section, the spark's length, its width, its
// colours, its speed, how many runs.
//
// When: never as the window appears - the owner read an immediate one as a
// render problem - but INTF_FIRST_MIN_MS..MAX after, then every
// INTF_GAP_MIN_MS..MAX, and never within INTF_QUIET_MS of a touch. A setting,
// later; the trial switch (title long press) has "now and then", "once", off.
//
// CHEAP BECAUSE IT IS SMALL (LESSONS, "Effects that cover the screen"): five
// plain objects on the screen above the window, a few thousand pixels each,
// moved once a frame. Off is a size of 0x0, never HIDDEN.
// ---------------------------------------------------------------------------
constexpr uint32_t INTF_FIRST_MIN_MS = 1500;
constexpr uint32_t INTF_FIRST_MAX_MS = 3500;
constexpr uint32_t INTF_GAP_MIN_MS   = 5000;
constexpr uint32_t INTF_GAP_MAX_MS   = 12000;
constexpr uint32_t INTF_QUIET_MS     = 2000;

// The dice, in millimetres so a spark looks alike on every board. They are the
// effect's own, not shape tokens (2.12 P2): nothing else draws a spark.
constexpr float SPARK_LEN_MM        = 1.2f;    // the head's length: this, plus up to
constexpr float SPARK_LEN_RAND_MM   = 3.3f;    //   this much more
constexpr float SPARK_THICK_MM      = 0.3f;    // its width: this, plus up to
constexpr float SPARK_THICK_RAND_MM = 0.55f;   //   this much more
constexpr float SPARK_SECTION_MM    = 8.0f;    // the section a spark runs along, at least
constexpr float SPARK_SECTION_MIN_MM = 3.0f;   // no spark where the side leaves less than this
constexpr float SPARK_JITTER_MM     = 0.3f;    // the head's shake, each way
constexpr float SPARK_FLECK_MM      = 1.8f;    // how far a fleck is thrown off the line, at most

enum IntfPiece : uint8_t { PIECE_GLOW, PIECE_TAIL1, PIECE_TAIL2, PIECE_CORE, PIECE_FLECK, PIECE_COUNT };

struct Interference {
    lv_obj_t   *o[PIECE_COUNT] = {nullptr};
    lv_timer_t *timer   = nullptr;
    uint32_t    nextAt  = 0;        // when the next spark starts
    bool        running = false;
    uint8_t     sparks  = 0;        // sparks so far in this window

    // The spark being drawn. `along` runs the length of one edge.
    uint8_t  edge = 0;              // 0 top, 1 bottom, 2 left, 3 right
    int32_t  runFrom = 0, runTo = 0;   // this run's section, in px along the edge
    int32_t  limit = 0;             // where the straight part of the edge ends
    int32_t  secLen = 0;
    uint8_t  runsLeft = 0;
    uint32_t runStart = 0, runMs = 0;
    bool     reverse = false;       // this run travels back along the edge
    int32_t  len = 0, thick = 0;    // the spark's head: length along, width across
    uint32_t core = 0, glow = 0, tail = 0;   // its colours
};
Interference g;

// The dice for a spark's size.
int32_t rollLen()   { return mm(SPARK_LEN_MM) + (int32_t)lv_rand(0, (uint32_t)LV_MAX(1, mm(SPARK_LEN_RAND_MM))); }
int32_t rollThick() { return LV_MAX(3, mm(SPARK_THICK_MM) + (int32_t)lv_rand(0, (uint32_t)LV_MAX(1, mm(SPARK_THICK_RAND_MM)))); }

// A dark window wants light sparks; a light one, deep ones.
bool lightWindow() {
    const uint32_t w = UI::pal().SURFACE_ALT;
    return ((w >> 16) & 0xFF) + ((w >> 8) & 0xFF) + (w & 0xFF) > 3 * 128;
}

// The palette: [core, glow] pairs. Fixed colours, not tokens - these are the
// colours of electricity, the same on every scheme of a kind. Mixed into the
// window's colour for the tail, so the tail always fades into the window.
void pickColours() {
    static const uint32_t DARK[][2] = {
        {0xFFFFFF, 0x6CC8FF}, {0xF2FBFF, 0x00A8FF}, {0xE6F4FF, 0x5B8CFF},
        {0xFFFFFF, 0x9FE6FF}, {0xEAF0FF, 0x8C7BFF},
    };
    static const uint32_t LIGHT[][2] = {
        {0x1030D0, 0x3D7BFF}, {0x2A1690, 0x6A4DFF}, {0x003C9E, 0x0090D8},
        {0x10107A, 0x4A6CFF},
    };
    const bool light = lightWindow();
    const uint32_t n = light ? sizeof(LIGHT) / sizeof(LIGHT[0]) : sizeof(DARK) / sizeof(DARK[0]);
    const uint32_t k = lv_rand(0, n - 1);
    g.core = light ? LIGHT[k][0] : DARK[k][0];
    g.glow = light ? LIGHT[k][1] : DARK[k][1];
    g.tail = UI::mix(g.glow, UI::pal().SURFACE_ALT, 45);
}

// Put piece `i` `at` px along the edge, `l` long and `t` across, centred on
// the border line. l or t of 0 turns it off.
void intfPut(uint8_t i, int32_t at, int32_t l, int32_t t, uint32_t hex) {
    lv_obj_t *o = g.o[i];
    if (!o) return;
    if (l <= 0 || t <= 0) { lv_obj_set_size(o, 0, 0); return; }
    const lv_area_t &W = s.winRect;
    const int32_t bw = UI::met().BORDER_W ? UI::met().BORDER_W : 1;
    lv_obj_set_style_bg_color(o, UI::c(hex), 0);
    switch (g.edge) {
        case 0:  lv_obj_set_pos(o, W.x1 + at, W.y1 + bw / 2 - t / 2); lv_obj_set_size(o, l, t); break;
        case 1:  lv_obj_set_pos(o, W.x1 + at, W.y2 - bw / 2 - t / 2); lv_obj_set_size(o, l, t); break;
        case 2:  lv_obj_set_pos(o, W.x1 + bw / 2 - t / 2, W.y1 + at); lv_obj_set_size(o, t, l); break;
        default: lv_obj_set_pos(o, W.x2 - bw / 2 - t / 2, W.y1 + at); lv_obj_set_size(o, t, l); break;
    }
}

void intfAllOff() {
    for (uint8_t i = 0; i < PIECE_COUNT; i++) if (g.o[i]) lv_obj_set_size(g.o[i], 0, 0);
}

// Fast and frenetic (owner, round 8): a run is 120-300 ms.
void intfNextRun() {
    g.runMs    = lv_rand(120, 300);
    g.runStart = millis();
}

// Roll the dice for a new spark.
void intfStartSpark() {
    const lv_area_t &W = s.winRect;
    const int32_t r = s.winRadius;
    g.edge = (uint8_t)lv_rand(0, 3);
    const bool horiz = g.edge < 2;
    const int32_t side = horiz ? (W.x2 - W.x1 + 1) : (W.y2 - W.y1 + 1);
    g.limit = side - r;                                  // stay off the rounded corners
    // The section: an eighth to a quarter of the side, never under SPARK_SECTION_MM.
    g.secLen = LV_MAX(mm(SPARK_SECTION_MM), side / 8 + (int32_t)lv_rand(0, (uint32_t)LV_MAX(1, side / 8)));
    if (g.secLen > g.limit - r) g.secLen = g.limit - r;
    if (g.secLen < mm(SPARK_SECTION_MIN_MM)) return;
    g.runFrom = r + (int32_t)lv_rand(0, (uint32_t)LV_MAX(1, g.limit - r - g.secLen));
    g.runTo   = g.runFrom + g.secLen;
    g.runsLeft = (uint8_t)lv_rand(2, 5);
    // ONE DIRECTION PER SPARK (owner, round 9): clockwise or anticlockwise
    // round the window, every run the same way - never back and forth over
    // itself. Along the top and right edges clockwise is the way `along`
    // counts; along the bottom and left it is the other way.
    const bool cw = lv_rand(0, 1);
    g.reverse = (g.edge == 0 || g.edge == 3) ? !cw : cw;
    g.len   = rollLen();
    g.thick = rollThick();
    pickColours();
    intfNextRun();
    g.running = true;
}

// Every frame while the window is open.
void intfTick(lv_timer_t *t) {
    (void)t;
    if (s.phase != PopupPhase::PHASE_OPEN) return;
    const uint32_t now = millis();

    if (!g.running) {
        if ((int32_t)(now - g.nextAt) < 0) return;
        if (s_intfMode == IntfMode::INTF_ONCE && g.sparks) return;
        if (now - s.lastTouchMs < INTF_QUIET_MS) { g.nextAt = now + 1000; return; }
        intfStartSpark();
        if (!g.running) { g.nextAt = now + 1000; return; }
        g.sparks++;
    }

    const uint32_t el = now - g.runStart;
    if (el >= g.runMs) {
        // This run is over. EACH RUN ROLLS ITS OWN DICE (owner, round 8: the
        // same spark repeated three times read as a loop): a new speed, and
        // EITHER a new length OR a new width - never both at once, so the runs
        // still look like one spark. It keeps its direction (round 9) and
        // edges on along it: a step of 0-45% of the section, so each run
        // mostly overlaps the last.
        if (--g.runsLeft) {
            int32_t step = g.secLen * (int32_t)lv_rand(0, 45) / 100;
            if (g.reverse) step = -step;
            g.runFrom = LV_CLAMP(s.winRadius, g.runFrom + step, g.limit - g.secLen);
            g.runTo   = g.runFrom + g.secLen;
            if (lv_rand(0, 1)) g.len = rollLen(); else g.thick = rollThick();
            intfNextRun();
            intfAllOff();
            return;
        }
        intfAllOff();
        g.running = false;
        g.nextAt  = now + lv_rand(INTF_GAP_MIN_MS, INTF_GAP_MAX_MS);
        return;
    }

    // Where the head is: eased along the section, either way, with jitter.
    const int32_t q    = (int32_t)(el * 1024 / g.runMs);
    const int32_t ease = q < 512 ? 2 * q * q / 1024 : 1024 - 2 * (1024 - q) * (1024 - q) / 1024;
    const int32_t travel = (g.runTo - g.runFrom - g.len) * ease / 1024;
    int32_t head = g.reverse ? g.runTo - g.len - travel : g.runFrom + travel;
    const int32_t jit = LV_MAX(1, mm(SPARK_JITTER_MM));
    head += (int32_t)lv_rand(0, (uint32_t)(2 * jit)) - jit;

    // Crackle: the odd frame the core drops out, the glow flares or the
    // fleck jumps off the line. The tail trails behind, whichever way it runs.
    const bool coreOn  = lv_rand(0, 99) < 85;
    const int32_t flare = lv_rand(0, 99) < 20 ? g.thick / 2 : 0;
    const int32_t gap   = LV_MAX(2, g.len / 3);
    const int32_t l1 = g.len * 3 / 4, l2 = g.len / 2;
    const int32_t t1 = g.reverse ? head + g.len + gap          : head - gap - l1;
    const int32_t t2 = g.reverse ? head + g.len + 2 * gap + l1 : head - 2 * gap - l1 - l2;
    intfPut(PIECE_GLOW,  head, g.len, g.thick + flare, g.glow);
    intfPut(PIECE_TAIL1, t1, l1, LV_MAX(2, g.thick * 2 / 3), g.tail);
    intfPut(PIECE_TAIL2, t2, l2, LV_MAX(1, g.thick / 3), g.tail);
    intfPut(PIECE_CORE,  head + g.len / 6, coreOn ? g.len * 2 / 3 : 0, LV_MAX(1, g.thick / 3), g.core);
    if (lv_rand(0, 99) < 12) {
        const int32_t sz = LV_MAX(2, g.thick / 2);
        const int32_t off = (int32_t)lv_rand(0, (uint32_t)LV_MAX(1, mm(SPARK_FLECK_MM)));
        const int32_t sign = (g.edge == 0 || g.edge == 2) ? -1 : 1;
        // Thrown off the line: the same edge, shifted outward by `off`.
        intfPut(PIECE_FLECK, head + g.len / 2, sz, sz, g.core);
        lv_obj_t *f = g.o[PIECE_FLECK];
        if (g.edge < 2) lv_obj_set_y(f, lv_obj_get_y(f) + sign * off);
        else            lv_obj_set_x(f, lv_obj_get_x(f) + sign * off);
    } else {
        intfPut(PIECE_FLECK, 0, 0, 0, 0);
    }
}

// The pieces, made with the window, above it, at 0x0. Called inside
// showWindow()'s quiet build (invalidation off), so making them costs nothing.
void intfCreate() {
    for (uint8_t i = 0; i < PIECE_COUNT; i++) g.o[i] = nullptr;
    g.running = false;
    g.sparks  = 0;
    if (s_intfMode == IntfMode::INTF_OFF) return;
    lv_obj_t *scr = lv_screen_active();
    for (uint8_t i = 0; i < PIECE_COUNT; i++) {
        g.o[i] = plain(scr);
        lv_obj_set_size(g.o[i], 0, 0);
        lv_obj_set_style_bg_opa(g.o[i], LV_OPA_COVER, 0);
        lv_obj_set_style_radius(g.o[i], LV_RADIUS_CIRCLE, 0);
    }
}

void intfStart() {
    if (s_intfMode == IntfMode::INTF_OFF) return;
    g.nextAt = millis() + lv_rand(INTF_FIRST_MIN_MS, INTF_FIRST_MAX_MS);
    g.timer  = lv_timer_create(intfTick, LV_DEF_REFR_PERIOD, nullptr);
}

void intfEnd() {
    if (g.timer) { lv_timer_delete(g.timer); g.timer = nullptr; }
    for (uint8_t i = 0; i < PIECE_COUNT; i++) {
        if (g.o[i]) lv_obj_delete(g.o[i]);
        g.o[i] = nullptr;
    }
    g.running = false;
}

} // namespace popup
