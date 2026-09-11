#include "UITokens.h"
#include <Arduino.h>

// ---------------------------------------------------------------------------
// Schemes
//
// Values come from the design bench sessions, 2026-09-10. The owner signed off
// four configurations; what they disagreed about was accent and per-board grid
// density, and what they agreed about is everything below.
// ---------------------------------------------------------------------------

// Shared across every scheme: state colours and sensor tints are content, not
// decoration, so they do not change when the scheme does. A warning is amber
// whether the dashboard is dark or light.
#define FLEET_STATE_AND_TINTS               \
    .ST_ACTIVE  = 0xF0A63C,                 \
    .ST_IDLE    = 0x64748B,                 \
    .ST_OK      = 0x4ADE80,                 \
    .ST_WARN    = 0xFBBF24,                 \
    .ST_BAD     = 0xF87171,                 \
    .TINT_TEMP  = 0xE0705F,                 \
    .TINT_HUMID = 0x4A9EDA,                 \
    .TINT_LIGHT = 0xE0A53C,                 \
    .TINT_AIR   = 0x9B7BD4,                 \
    .TINT_POWER = 0x93C04A

const UIPalette UI_PAL_SLATE = {
    .name        = "Slate",
    .GROUND      = 0x1A1F27,
    .SURFACE     = 0x212429,
    .SURFACE_ALT = 0x2B2F36,
    .TEXT        = 0xF0F2F4,
    .TEXT_DIM    = 0x98A0AA,
    .ACCENT      = 0x9B7BD4,   // violet — the owner's pick for the 3248
    .BORDER      = 0,          // derived: lighten SURFACE
    FLEET_STATE_AND_TINTS
};

const UIPalette UI_PAL_PAPER = {
    .name        = "Paper",
    .GROUND      = 0xE8E9EC,
    .SURFACE     = 0xFFFFFF,
    .SURFACE_ALT = 0xF1F2F5,
    .TEXT        = 0x16181C,
    .TEXT_DIM    = 0x666D77,
    .ACCENT      = 0xE0A53C,   // amber — the owner's pick for the large panels
    .BORDER      = 0,
    FLEET_STATE_AND_TINTS
};

// Today's shipped UI, read out of UIToolkit.cpp and Panel_Header.cpp rather
// than eyeballed. Kept so the new system can be compared against the old one on
// the same glass instead of from memory.
const UIPalette UI_PAL_FLEET = {
    .name        = "Fleet",
    .GROUND      = 0x101010,
    .SURFACE     = 0x181818,
    .SURFACE_ALT = 0x202020,
    .TEXT        = 0xDDDDDD,
    .TEXT_DIM    = 0x808080,
    .ACCENT      = 0x00A8FF,
    .BORDER      = 0x404040,   // explicit — this one is not derived
    FLEET_STATE_AND_TINTS
};

#undef FLEET_STATE_AND_TINTS

// Dark schemes take a hairline border because a dark card on a dark ground
// needs an edge; light schemes lean on the shadow instead. That difference was
// the only thing separating the owner's light and dark configs.
const UIMetrics UI_MET_DARK  = {
    .RADIUS = 10, .PAD = 5, .BORDER_W = 1, .BORDER_OPA_PCT = 40, .SHADOW = 8, .HEADER_H = 14
};
const UIMetrics UI_MET_LIGHT = {
    .RADIUS = 12, .PAD = 5, .BORDER_W = 0, .BORDER_OPA_PCT = 0,  .SHADOW = 8, .HEADER_H = 14
};

// ---------------------------------------------------------------------------
// Active state
// ---------------------------------------------------------------------------
namespace {

// Fleet, not Slate: the port onto tokens should be invisible on glass, so the
// default has to be today's colours. Switching the default to Slate or Paper is
// a one-line change once the new look has been seen on a board.
UIPalette s_pal = UI_PAL_FLEET;
UIMetrics s_met = UI_MET_DARK;

// TARGET_CARD_W is 130, not the 135 the design bench used, and the difference
// is the bench's fault rather than a change of mind.
//
// The bench modelled WS_P4_5 at a hardcoded 1.5x because that is what
// -D HIGH_DPI_DISPLAY gave it. The board is really 1.73x (294 PPI), so every
// logical value tuned in the bench renders ~15% larger here than it appeared
// there. At 135 that pushed WS_P4_5 from the intended 5x3 down to 4x2.
//
// 130 reproduces the layouts actually chosen:
//   WS_P4_5 landscape      5x3 of 230x210 px
//   CYD_S3_3248 portrait   2x3 of 141x144 px
//   CYD_S3_3248 landscape  3x2 of 144x141 px
UIGrid    s_grid = { .TARGET_CARD_W = 130, .ASPECT_PCT = 85, .GAP = 12, .INSET = 14,
                     .cols = 1, .rows = 1, .cellW = 0, .cellH = 0 };

int32_t s_vpW = 0, s_vpH = 0;
void (*s_onChange)() = nullptr;

// Blend two 0xRRGGBB values. Used only for deriving a border from a surface.
uint32_t blend(uint32_t a, uint32_t b, uint8_t pct) {
    uint32_t out = 0;
    for (int sh = 16; sh >= 0; sh -= 8) {
        uint32_t ca = (a >> sh) & 0xFF, cb = (b >> sh) & 0xFF;
        out |= (uint32_t)((ca * (100 - pct) + cb * pct) / 100) << sh;
    }
    return out;
}

void recomputeGrid() {
    if (s_vpW <= 0 || s_vpH <= 0) return;

    const int32_t inset = UI::sc(s_grid.INSET);
    const int32_t gap   = UI::sc(s_grid.GAP);
    const int32_t target = UI::sc(s_grid.TARGET_CARD_W);

    const int32_t availW = s_vpW - inset * 2;
    const int32_t availH = s_vpH - inset * 2;
    if (availW <= 0 || availH <= 0) return;

    // Columns: as many whole target-width cards as fit.
    int32_t cols = (availW + gap) / (target + gap);
    if (cols < 1) cols = 1;
    const int32_t cellW = (availW - gap * (cols - 1)) / cols;

    // Rows: aim for the requested aspect, then stretch the row height so the
    // grid fills the viewport exactly rather than leaving a dead strip.
    const int32_t wantH = (cellW * s_grid.ASPECT_PCT) / 100;
    int32_t rows = (availH + gap) / (wantH + gap);
    if (rows < 1) rows = 1;
    const int32_t cellH = (availH - gap * (rows - 1)) / rows;

    s_grid.cols  = (uint8_t)cols;
    s_grid.rows  = (uint8_t)rows;
    s_grid.cellW = (uint16_t)cellW;
    s_grid.cellH = (uint16_t)cellH;
}

} // namespace

namespace UI {

void begin(int32_t viewportW, int32_t viewportH) {
    setViewport(viewportW, viewportH);
    Serial.printf("[UI] Scheme \"%s\" | %ux%u grid of %ux%u px | scale %.2fx (%u PPI)\n",
                  s_pal.name, (unsigned)s_grid.cols, (unsigned)s_grid.rows,
                  (unsigned)s_grid.cellW, (unsigned)s_grid.cellH,
                  (double)bspUiScale(), (unsigned)bspPixelDensity());
}

void setViewport(int32_t w, int32_t h) {
    s_vpW = w; s_vpH = h;
    recomputeGrid();
}

const UIPalette &pal()  { return s_pal; }
const UIMetrics &met()  { return s_met; }
const UIGrid    &grid() { return s_grid; }

const UIType &type() {
    // Built on first use rather than at static-init time: lv_font_montserrat_*
    // are LVGL globals, and depending on a global's initialisation order from
    // another translation unit is the static-init-order fiasco waiting to
    // happen. One branch, once.
    static UIType t = {
        .VALUE = &lv_font_montserrat_40,
        .NAME  = &lv_font_montserrat_16,
        .TAG   = &lv_font_montserrat_12,
        // HERO shares VALUE's face for now. Referencing montserrat_48 pulled a
        // whole extra font into the link for something nothing draws yet -
        // measured at the cost recorded in docs/design/tokens.md. Point it at a
        // larger face when a fullscreen card actually needs one.
        .HERO  = &lv_font_montserrat_40,
        .ICON  = 26
    };
    return t;
}

void setScheme(const UIPalette &p, const UIMetrics &m) {
    s_pal = p;   // copies, so setAccent() can override without touching a const
    s_met = m;
    recomputeGrid();
    if (s_onChange) s_onChange();
}

void setAccent(uint32_t hex) {
    s_pal.ACCENT = hex;
    if (s_onChange) s_onChange();
}

void setTargetCardWidth(uint16_t logicalPx) {
    s_grid.TARGET_CARD_W = logicalPx;
    recomputeGrid();
    if (s_onChange) s_onChange();
}

void onSchemeChanged(void (*cb)()) { s_onChange = cb; }

int32_t sc(int32_t logical) { return (int32_t)(logical * bspUiScale()); }

lv_color_t c(uint32_t hex) { return lv_color_hex(hex); }

lv_color_t border() {
    if (s_pal.BORDER) return lv_color_hex(s_pal.BORDER);
    // Derived: pull the surface toward white on dark schemes, toward black on
    // light ones. This is the bug the bench found in its first version — a
    // border derived from a fixed white vanished entirely on a white card.
    const bool darkSurface = ((s_pal.SURFACE >> 16) & 0xFF) < 0x80;
    return lv_color_hex(blend(s_pal.SURFACE, darkSurface ? 0xFFFFFF : 0x000000,
                              s_met.BORDER_OPA_PCT));
}

int32_t minTouch() {
    const uint16_t ppi = bspPixelDensity();
    if (!ppi) return sc(44);          // no density declared — a sane default
    return (int32_t)((ppi * 9) / 25.4f + 0.5f);   // 9 mm
}

} // namespace UI
