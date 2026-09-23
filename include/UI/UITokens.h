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
#include "UI/UITypeScale.h"   // generated: this board's text faces
#include "UI/UIIcons.h"       // generated: this board's MDI subset

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

    // --- Corner icon tints, by kind of thing. 2.7 round two. ---
    // The owner: "if the temp and lux are going to be colored then they all
    // should be". Bulbs yellow-white "like one would picture a light bulb";
    // doors blue. TINT_LIGHTING is per scheme rather than shared, because a
    // yellow-white glyph vanishes on a light scheme's white card.
    uint32_t TINT_OPENING;  // doors, windows, garage doors, locks
    uint32_t TINT_PRESENCE; // occupancy, motion, presence
    uint32_t TINT_LIGHTING; // lights, and switches used as lights
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
    uint8_t SHADOW;         // blur width. 0 = none
    uint8_t HEADER_H;       // card header bar — covers the border, edge to edge
    // How far the shadow drops below the card. 0 is a halo; a few pixels is
    // a DROP shadow, which is what makes a card read as lifted off the page -
    // the owner's "it looks too flat" on Linen, 2026-09-23.
    uint8_t SHADOW_Y;
    uint8_t SHADOW_OPA;     // 0-255
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
// The hard limits on what a grid may be asked for. Not taste - a card narrower
// than a finger is not a card, and the unit grid underneath has a fixed-size
// descriptor. The owner: "we can set a limit within reason".
static constexpr uint8_t UI_MAX_COLS = 8;
static constexpr uint8_t UI_MAX_ROWS = 6;

struct UIGrid {
    uint16_t TARGET_CARD_W; // logical px - decides how many COLUMNS fit

    // An EXPLICIT column count, or 0 to derive one from TARGET_CARD_W.
    //
    // The derivation is the right default - it is what makes one page spec
    // render sensibly on eight panels. But "I want three columns on this
    // board" is a legitimate thing to say, and saying it by tuning a target
    // width until the arithmetic lands on three is not a user interface. When
    // this is set the count is obeyed and the cells take whatever width falls
    // out of it.
    uint8_t  COLS_OVERRIDE;
    // The TALLEST a card may be, as a percentage of its own width.
    //
    // CHANGED AT 2.5. It used to be a target shape that decided the ROW COUNT,
    // and that was the bug behind "hiding the deck made the cards smaller":
    // more height bought another empty row instead of taller cards. The row
    // count now comes from how many cards there are (CardPage::rowsWanted()),
    // and this only stops a page with three cards on it from making each one
    // as tall as the screen. Above the cap the grid leaves slack at the bottom.
    uint8_t  ASPECT_PCT;
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
// LVGL compiles fixed bitmap faces, so this is a shortlist, not a scale - and
// the shortlist is a FLASH BUDGET. Enabling a size in lv_conf.h is free,
// because the linker drops unreferenced font objects; REFERENCING one is what
// costs.
//
// HOW MUCH, corrected at 2.7. "~96 KB per face" was measured on ONE large,
// full-ASCII Montserrat and was then applied to every face for months. From the
// P4 builds' object files: a generated digits-only VALUE face is 6-10 KB, an
// MDI icon face 10-73 KB by size, Montserrat 14-24 is 14-29 KB. Still a budget;
// a much smaller one. See docs/design/cards.md section 13.
struct UIType {
    const lv_font_t *VALUE;   // the number on a measure card. DIGITS ONLY on
                              // dense boards - it is a generated subset, and
                              // it has no letters and no LV_SYMBOL range
    const lv_font_t *VALUE_SM;// the number on a CRAMPED card, ~3/4 of VALUE.
                              // Always the digits-only subset. #62, 2.7
    const lv_font_t *UNIT;    // the unit beside a value, deliberately smaller
    const lv_font_t *NAME;    // card name
    const lv_font_t *TAG;     // header bar, status row
    // The two icon faces. Material Design Icons, generated as an 84-glyph
    // subset per board - NOT Montserrat, and not interchangeable with it:
    // MDI codepoints sit in the private use area, so these faces can draw
    // nothing but icons and every other face can draw none of them.
    const lv_font_t *ICON;    // the disc glyph on a state card
    const lv_font_t *ICON_MD; // between the two: the corner on a LARGE card,
                              // the hero on a CRAMPED one. 2.7
    const lv_font_t *ICON_SM; // the corner icon on an ordinary card
    const lv_font_t *HERO;    // oversized, for a fullscreen card
};

// ---------------------------------------------------------------------------
// Built-in schemes
// ---------------------------------------------------------------------------
// Three, pruned on glass 2026-09-23. See UITokens.cpp.
extern const UIPalette UI_PAL_FLEET;    // the original shipped UI
extern const UIPalette UI_PAL_MIDNIGHT; // dark, the default
extern const UIPalette UI_PAL_LINEN;    // light

extern const UIMetrics UI_MET_DARK;   // 1px lighten @40% border, no shadow needed
extern const UIMetrics UI_MET_LIGHT;  // no border, leans on the shadow

// Colour roles for UI::paint() - shared styles that repaint themselves on a
// scheme change. #64; the full story is at UI::paint() below.
enum class UIPaint : uint8_t {
    PAINT_SURFACE = 0,   // bg SURFACE, border UI::border()     - panels
    PAINT_SURFACE_ALT,   // bg SURFACE_ALT, border UI::border() - buttons
    PAINT_TEXT,          // text TEXT
    PAINT_TEXT_DIM,      // text TEXT_DIM
    PAINT_ACCENT_TEXT,   // text ACCENT                         - panel titles
    PAINT_ACCENT_BG,     // bg ACCENT
    PAINT_COUNT
};

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
// The next scheme in the knob's order - Fleet, Midnight, Linen - with its
// metrics. Every scheme button calls this; the order lives in UITokens.cpp.
void cycleScheme();
// The active scheme as a position in that order, and back. Pages remember
// their own scheme since 2.6 (the owner: colour is per page), and an index is
// what a page can hold without knowing what a scheme is.
uint8_t schemeIndex();
void    setSchemeIndex(uint8_t i);
void setAccent(uint32_t hex);
void setTargetCardWidth(uint16_t logicalPx);

// The OTHER grid knob, and the one that was missing.
//
// TARGET_CARD_W decides how many COLUMNS fit; ASPECT_PCT decides how many
// ROWS, by suggesting a height for the column width that came out. Neither
// sizes a card - both counts are then stretched to fill the viewport exactly,
// so the ratio a card ends up with is an output.
//
// It matters because the two are easy to confuse: on WS_P4_7B at 6 columns the
// row count misses three by four hundredths, and the fix is here rather than
// in the width. See docs/design/tokens.md.
void setAspectPct(uint8_t pct);

// Demand an exact column count, or 0 to go back to deriving it.
void setColumnsOverride(uint8_t cols);

// Registered by GUIManager so a scheme change can restyle what is on screen.
void onSchemeChanged(void (*cb)());

// --- Helpers ---------------------------------------------------------------
int32_t    sc(int32_t logical);          // logical px -> this board's pixels
lv_color_t c(uint32_t hex);              // the one place that calls lv_color_hex
lv_color_t border();                     // BORDER, or derived from SURFACE

// Blend two 0xRRGGBB values, pct% of b into a.
//
// Exists so a card can look QUIET without using opacity. Setting an opa below
// LV_OPA_COVER on a container makes LVGL render that whole subtree to an
// intermediate layer buffer - which on a constrained board fails outright
// ("lv_draw_layer_alloc_buf: Allocating layer buffer failed"). Mixing toward
// the ground colour costs nothing.
uint32_t mix(uint32_t a, uint32_t b, uint8_t pct);

// Whichever of `a` and `b` stands further from `bg` in perceived brightness.
//
// For colour the palette does not own - a light's own rgb_color, drawn in its
// card's disc - where no scheme token can be picked in advance. Passing two
// scheme tokens (GROUND and TEXT) keeps the answer inside the palette, so the
// "no colour literals in UI code" rule holds.
uint32_t contrastOf(uint32_t bg, uint32_t a, uint32_t b);

// --- Shared paints: chrome that repaints itself on a scheme change. #64 -----
//
// Cards re-read UI::pal() in their own restyle(), and CardBinder calls it on
// every card when the scheme changes. The CHROME around them - deck panels,
// the system drawer, their buttons - set a local colour once at build time and
// had no restyle at all, so they kept the old scheme until something rebuilt
// them. The owner saw it switching Fleet to Slate.
//
// Rather than give every panel a restyle() that has to remember each widget
// it coloured, these are SHARED lv_style_t objects, one per role. A widget
// adds the style instead of setting a colour; a scheme change updates the
// style in place and tells LVGL, and every widget using it repaints. One
// place decides what a scheme change repaints - the same consolidation that
// fixed the drawer offset in 2.6.
//
// A LOCAL style property beats a shared style in LVGL, so a widget painted
// this way must not also call lv_obj_set_style_*_color for the same property.
// The roles are UIPaint, declared above the namespace with the other types.
lv_style_t *paint(UIPaint p);

// Silences the `lv_part_t | lv_state_t` deprecation warning that would
// otherwise be reproduced in every card type. Issue #13 asked for this.
inline lv_style_selector_t part(lv_part_t p, lv_state_t s = LV_STATE_DEFAULT) {
    return (lv_style_selector_t)p | (lv_style_selector_t)s;
}

// Scroll behaviour, applied to any container that may overflow.
//
// Three things, all of which matter more on these panels than on a phone:
//   - vertical only, so a slightly-too-wide row can never start a sideways drag
//   - no elastic rubber-banding at the ends, which looks terrible at the refresh
//     rates the S3 boards manage
//   - no scrollbar, since it is one more thing to redraw
//
// Call it on every scrollable container rather than remembering three flags.
void tameScroll(lv_obj_t *o);

// Minimum comfortable touch target: ~9 mm, the width of a fingertip. Derived
// from real density, so it is 60 px at 170 PPI and 104 px at 294 PPI rather
// than a number someone guessed per board.
int32_t minTouch();

} // namespace UI
