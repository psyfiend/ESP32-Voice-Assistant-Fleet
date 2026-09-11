#pragma once
//
// UITokens — the fleet's design system.
//
// This file answers "what does it LOOK like" and nothing else. What a card IS
// comes from two other places, and keeping the three apart is deliberate:
//
//   design tokens   (here)             what colour is "active", how round, what size
//   card type rules (Card subclasses)  a light uses state colour, a sensor a tint
//   card instances  (build sheet, #20) THIS card shows deck_temp, spans 2, row 1
//
// If tokens carried entity bindings, changing a colour would mean editing card
// definitions — inverting ROADMAP §4's layering, where the build sheet is the
// top layer and tokens sit near the bottom. See docs/design/tokens.md.
//
// Struct style follows Fleet_BSP on purpose: flat groups, `const` instances,
// designated initialisers. One idiom for the project, and its traps are already
// documented in CLAUDE.md (declaration order, macro collisions).
//
#include <lvgl.h>
#include <stdint.h>
#include "bsp_loader.h"

// ---------------------------------------------------------------------------
// 1. Colour
// ---------------------------------------------------------------------------
struct UIPalette {
    const char *name;

    uint32_t GROUND;        // the screen behind the cards
    uint32_t SURFACE;       // card body
    uint32_t SURFACE_ALT;   // wells, slider tracks, icon discs
    uint32_t TEXT;
    uint32_t TEXT_DIM;
    uint32_t ACCENT;
    uint32_t BORDER;        // 0 = derive from SURFACE, see UI::borderColor()

    // --- Semantic state palette ---
    // Separate from ACCENT on purpose. Accent is decoration; these carry
    // meaning, and docs/design/cards.md §0 records why that distinction turned
    // out to be the most important thing the design bench produced.
    uint32_t ST_ACTIVE;     // light on, door open, motion present
    uint32_t ST_IDLE;       // off, closed, clear
    uint32_t ST_OK;
    uint32_t ST_WARN;       // stale
    uint32_t ST_BAD;        // command refused — see cards.md §3

    // --- Sensor icon tints, by quantity ---
    // A measure card's icon is tinted by what it measures, so a wall of sensor
    // cards is scannable by colour before you read a single number.
    uint32_t TINT_TEMP;
    uint32_t TINT_HUMID;
    uint32_t TINT_LIGHT;
    uint32_t TINT_AIR;
    uint32_t TINT_POWER;
};

// ---------------------------------------------------------------------------
// 2. Shape — per scheme
// ---------------------------------------------------------------------------
// All values are LOGICAL pixels. Run them through UI::sc() at the point of use;
// never store a scaled value, because the scale is per board.
struct UIMetrics {
    uint8_t RADIUS;
    uint8_t PAD;            // card inner padding
    uint8_t BORDER_W;       // 0 on light schemes, 1 on dark
    uint8_t BORDER_OPA_PCT; // strength of the derived border colour
    uint8_t SHADOW;
    uint8_t HEADER_H;       // card header bar — covers the border, edge to edge
};

// ---------------------------------------------------------------------------
// 3. Grid — derived from the viewport, not declared
// ---------------------------------------------------------------------------
// Columns and rows are always DERIVED. You pick a target card width and the
// page fits as many whole cards as the screen allows; the row height is then
// stretched so the grid fills the height exactly, leaving no dead strip.
//
// This is what makes one token set produce 5 columns on WS_P4_5 and 2 on
// CYD_S3_3248 portrait with no per-board layout code.
struct UIGrid {
    uint16_t TARGET_CARD_W; // logical px — the knob you actually turn
    uint8_t  ASPECT_PCT;    // 100 = square. Only a hint; rows stretch to fit
    uint8_t  GAP;
    uint8_t  INSET;         // cluster inset from the screen edge

    // Filled in by UI::setViewport()
    uint8_t  cols;
    uint8_t  rows;
    uint16_t cellW;         // physical px, already scaled
    uint16_t cellH;
};

// ---------------------------------------------------------------------------
// 4. Type — fleet-wide
// ---------------------------------------------------------------------------
// LVGL compiles fixed bitmap faces, so this is a shortlist, not a scale.
// NOTE: lv_conf.h currently enables every Montserrat size from 8 to 48 — 21
// faces, most of them unused. Trimming that to the set named here is real flash
// back, and it competes directly with the MDI icon subset for the same budget.
// Measured under #14 before the subset is generated.
struct UIType {
    const lv_font_t *VALUE;   // the number on a measure card
    const lv_font_t *NAME;    // card name
    const lv_font_t *TAG;     // header bar, status row, units
    const lv_font_t *HERO;    // oversized, for a fullscreen card
    uint8_t ICON;             // logical px
};

// ---------------------------------------------------------------------------
// Built-in schemes
// ---------------------------------------------------------------------------
extern const UIPalette UI_PAL_SLATE;  // dark, the owner's primary
extern const UIPalette UI_PAL_PAPER;  // light
extern const UIPalette UI_PAL_FLEET;  // today's shipped UI, for comparison

extern const UIMetrics UI_MET_DARK;   // 1px lighten @40% border, no shadow needed
extern const UIMetrics UI_MET_LIGHT;  // no border, leans on the shadow

namespace UI {

// Call once, after LVGL is up and the display's rotated size is known.
void begin(int32_t viewportW, int32_t viewportH);

// Re-derive the grid. Call on rotation or when TARGET_CARD_W changes.
void setViewport(int32_t w, int32_t h);

const UIPalette &pal();
const UIMetrics &met();
const UIGrid    &grid();
const UIType    &type();

// --- Runtime customisation -------------------------------------------------
// The active palette and metrics are COPIES of a scheme, not pointers to one,
// so individual values can be overridden live without editing a const. Costs
// ~80 bytes and buys on-glass comparison, which is the whole point.
//
// Cards must therefore read UI::pal() when they build or restyle and must never
// cache a colour — cheap to honour now, invasive to retrofit later.
void setScheme(const UIPalette &p, const UIMetrics &m);
void setAccent(uint32_t hex);
void setTargetCardWidth(uint16_t logicalPx);

// Registered by GUIManager so a scheme change can restyle what is on screen.
void onSchemeChanged(void (*cb)());

// --- Helpers ---------------------------------------------------------------
int32_t    sc(int32_t logical);          // logical px -> this board's pixels
lv_color_t c(uint32_t hex);              // the one place that calls lv_color_hex
lv_color_t border();                     // BORDER, or derived from SURFACE

// Silences the `lv_part_t | lv_state_t` deprecation warning that would
// otherwise be reproduced in every card type. Issue #13 asked for this.
inline lv_style_selector_t part(lv_part_t p, lv_state_t s = LV_STATE_DEFAULT) {
    return (lv_style_selector_t)p | (lv_style_selector_t)s;
}

// Minimum comfortable touch target: ~9 mm, the width of a fingertip. Derived
// from real density, so it is 60 px at 170 PPI and 104 px at 294 PPI rather
// than a number someone guessed per board.
int32_t minTouch();

} // namespace UI
