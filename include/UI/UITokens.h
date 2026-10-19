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

    // The dim laid over the page behind a card's popup (2.10a). Dark on every
    // scheme, Linen included - a light dim reads as fog, not as "behind".
    // Drawn as a translucent BACKGROUND, never an object opa (LESSONS: layers).
    uint32_t SCRIM;
    // --- The header's status glyphs (2.8 round 3, D-24, D-27) ---
    // Their own colours per scheme: the dark schemes' the "Fleet Status Glyphs"
    // mock's softer set, Linen's darker so they read on sand (the Wi-Fi green
    // all but vanished there). Strong / good / weak / bad: Wi-Fi's strength;
    // busy: joining; off: switched off, and an unlit arc; link: the HA / MQTT
    // link while it is well.
    uint32_t SIG_STRONG, SIG_GOOD, SIG_WEAK, SIG_BAD, SIG_BUSY, SIG_OFF, SIG_LINK;
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
    // THE ACTIVE FRAMES (2.8 rounds 4-5, the owner's): what an "on" card draws
    // on its edge in place of a fill, LIT inward - drawn, never as its border,
    // so the card's layout never moves (round 4's bracket pushed the hero down).
    uint8_t ACTIVE_RING;    // 3   the corner cut's frame and its diagonal
    uint8_t ACTIVE_EDGE;    // 2   the bracket's sides
    uint8_t BRACKET_W;      // 6   the bracket's top and bottom bars
    uint8_t GLOW_DEPTH;     // 18  how far the light reaches in from a lit edge
    uint8_t GLOW_OPA;       // 115 its strength at the edge (0-255), fading to nothing (Linen 150)
    uint8_t CUT_PCT;        // 24  the corner cut's chamfer, % of the card's height
    // ...and the way a light scheme makes them loud (round 6): DARKER, where a
    // dark one makes them brighter - the frame's edge this % of the way from
    // the active colour to the scheme's text, the light inward half that.
    uint8_t ACTIVE_INK;     // 0 (dark schemes), 35 (Linen)
};

// ---------------------------------------------------------------------------
// 2b. Shape and spacing in MILLIMETRES on glass - the card window's (2.12 P2)
// ---------------------------------------------------------------------------
// The card window sizes what a finger touches and what an eye reads in real
// millimetres (its mm(), the same derivation as UI::minTouch()), so these are
// millimetres, never logical pixels: pass them to mm(), not UI::sc(). The P_
// group is in "P4_5 millimetres" (the window's pm()): the same SHARE of every
// window as on the P4_5 (docs/design/card-sheet.md section 15).
//
// One set, fleet-wide, read through UI::shape(). Every value is the one the
// window had before it had a name - 2.12 changed no pixel - so a few values
// sit close to each other: they are kept apart and listed in
// docs/design/tokens.md section 7, for the owner to merge or not.
struct UIShape {
    // --- Corner radii ---
    float R_CONTROL;    // 1.6  a dropdown and its list, a checkbox's box, a row's button; a
                        //      dialog's buttons; the control deck and its selector, squared
                        //      (#97: R_CONTROL 1.2 and R_BUTTON 1.6 merged - tokens.md 7)
    float R_PANEL;      // 1.6  a folder panel: its pane, its tab, the curve where they meet
    float R_WINDOW;     // 2.4  the window
    float R_CHIP;       // 3.0  a span chip - half its height, so a pill

    // --- Spacing, smallest first: padding, gaps, offsets ---
    float S1;           // 0.6  inside the control deck; a swatch grid's room for its ring; the
                        //      chart's strips; between an activity row's lines; the least gap
                        //      Properties closes its rows to
    float S2;           // 0.8  between the hero column's lines; a value and its unit
    float S3;           // 1.0  clear of a corner chip; between activity rows; a tab a millimetre short
    float S4;           // 1.2  the window's padding; a pill's sides; History's rows; carets from the edge
    float S5;           // 1.6  under the header row; the header row's gaps; the chart's gutters; a
                        //      label and its pill; swatches; a dropdown's sides; a panel's first
                        //      and last row; Members' rows, their padding and gaps; a fact list's
                        //      rows, and the gap between its two columns (#97: half of S8's)
    float S6;           // 2.0  the window clear of the system header and the tabs; the break before
                        //      the deck's modes; activity's two columns
    float S7;           // 2.4  a panel's sides; a row button's sides; a span chip's sides; a
                        //      dialog's rows, and its buttons' spacing where they share a narrow row
    float S8;           // 3.0  a time label's elbow room
    float S9;           // 4.0  a panel row's words and its control; a dialog's padding
    float S10;          // 5.0  activity's and a fact list's sides
    // The four that sat off the scale - S_MEMBERS 1.4, S_GUTTER 1.5, S_CHIP_SIDE 2.2,
    // S_ROW_MIN 0.55 - were merged into their nearest steps (#97, tokens.md 7).

    // --- Lines ---
    float LINE_HAIR;    // 0.2  borders: a dropdown, its list, a swatch, a row button
    float LINE_MID;     // 0.3  a checkbox's box; the finger's line on the chart; a lit span chip's
                        //      border, a dialog button's; the chart's min/max lines (LINE_FINE
                        //      0.25 merged in, #97)
    float LINE_HEAVY;   // 0.4  the dialog's border
    float LINE_PLOT;    // 0.45 the chart's line; a panel's carets
    float DASH;         // 0.6  a dotted line's dash, and its gap
    float RING_W;       // 0.4  the ring round a chosen swatch or a loaded scene
    float RING_PAD;     // 0.3  its distance from what it rings
    float TRACK_PILL;   // 0.2  letter spacing: the PAUSED pill
    float TRACK_TAB;    // 0.4  letter spacing: a panel's tab

    // --- Heights and widths ---
    float H_CONTROL;    // 6.0  a dropdown, a checkbox's box, a row's button
    float H_CHIP;       // 6.0  a span chip, and the top line's middle that holds them
    float H_ROW;        // 7.2  a panel's row; activity's "Show more" (H_MORE 7.0 merged in, #97)
    float H_BAR;        // 0.17 the auto-close bar: 2 px at 294 and 255 PPI, 1 px at 170 (#97; was 0.5)
    float H_TABS;       // 6.0  kept under the window for the panels' tabs
    float W_SIDE_MIN;   // 6.0  the least gap between the window and the screen's side
    float W_WINDOW_MAX; // 100  the widest a window gets (a hand's span)
    float W_ROWS_MAX;   // 70   activity's rows at most
    float W_DIALOG_MAX; // 90   a dialog at most
    float W_DIALOG_BTN; // 24   a dialog's buttons, at most: narrower, they share the row (#96)
    float SWATCH;       // 6.5  a colour swatch; the least height of a scene button
    float CARET_W;      // 2.4  a panel's caret
    float CARET_H;      // 1.0

    // --- How far a finger may move ---
    float SLOP_SELECTOR;// 1.0  before the control deck's selector is dragged
    float SLOP_KNOB;    // 1.2  before the toggle's knob is dragged
    float SLOP_HOLD;    // 4.0  before a long press is a drag instead

    // --- The hero and around it, in P4_5 millimetres (pm()) ---
    float P_GAP;        // 4.0  between the hero and its column
    float P_STACK_GAP;  // 3.0  between the hero and the control deck under it
    float P_GRIP_INSET; // 1.4  the slider's grip below the top of its fill
    float P_GRIP_H;     // 0.5  the grip's height
    float P_GRIP_MIN;   // 5.0  the least fill that shows a grip
    float P_MARK_H;     // 2.6  the ring marking a temperature or a colour
    float P_MARK_LINE;  // 0.35 its line
    float P_SCENE_H;    // 9.0  a scene button, at most
    float P_SCENE_GAP;  // 1.4  between scene buttons
    float P_SCENE_PAD;  // 2.4  a scene button's sides

    // --- A cluster (2.11, K89, K100-K103): a group card shown as a block of its
    // members, in real millimetres on every board (the "2.11 Clusters" mock's) ---
    float C_HDR_H;      // 5.2  its header row - one line (K100)
    float C_PAD;        // 1.1  inside its frame, round the tiles; the header's sides
    float C_TILE_GAP;   // 0.8  between mini tiles
    float C_SWATCH_W;   // 1.1  the group's swatch beside the cluster's name
    float C_SWATCH_H;   // 2.4
    float C_VALUE_GAP;  // 1.3  between the header's values, and a value's icon and words
    float C_TILE_MIN;   // 7.5  the least a mini tile is each way - "tight" (K103); a cluster
                        //      whose tiles would be smaller takes more cells

    // --- Panels and windows float over the page (D-35, the "Panel Edges" mock) ---
    float HALO;         // 5.0  the halo round a header slot's panel and every window: black at
                        //      their edge, clear this far out
    float LINE_FADE;    // 2.5  the bar's accent line fading out before an open panel's walls

    // --- The on-screen keyboard (2.11, K95): Kit::keyboard() ---
    float KEY_H;        // 7.0  a key's height - a phone's, under the touch minimum: four rows
                        //      of 9 mm took 62 % of the P4_5's height, leaving the field no room
    float KEY_GAP;      // 1.0  between keys, and round them
};

// ---------------------------------------------------------------------------
// 2c. The page header's slots, in LOGICAL pixels (2.8, #19)
// ---------------------------------------------------------------------------
// The header is sc(UIToolkit::SYSTEM_HEADER_FULL_H) tall on every board, so what
// sits in it is sized in the same logical pixels: pass these to UI::sc(). One
// set, fleet-wide, read through UI::hdr(); docs/design/tokens.md section 8.
struct UIHeader {
    uint8_t GLYPH;         // 26  a glyph's drawing box - THE SAME bare or framed (D-24: a frame never shrinks it)
    uint8_t FRAME;         // 28  a glyph's frame, the rounded square or the circle
    uint8_t FRAME_R;       //  7  the rounded square's corners
    uint8_t FRAME_MIX;     // 14  the rounded square's shade: % of TEXT over the bar
    uint8_t SLOT_GAP;      //  8  between the things in one slot
    uint8_t SLOT_PAD;      //  6  a slot's sides (its tap area and its open tab reach this far)
    // The three places (2.8 round 2, Layout/HeaderPlan.h)
    uint8_t EDGE;          //  8  from each end of the bar to its side's first slot
    uint8_t SPACE;         //  2  between two slots in one place (their pads make the rest)
    uint8_t CLEAR;         // 12  at least this between a side and the middle; closer, the lowest keep gives way
    uint8_t DOT_ON;        // 11  this page's dot
    uint8_t DOT_OFF;       //  8  another page's ring
    uint8_t DOT_RING;      //  2  the ring's line
    uint8_t DOT_GAP;       //  5  between dots
    uint8_t MARK;          // 12  the error mark beside a slot's dashes
    uint8_t LINE;          //  2  the bar's accent line - and the outline of an open slot and its panel (D-23)
    uint8_t JOIN_R;        //  6  a cove or flare: the rounded corner where that outline meets the line
    // A slot's drawer - the system panel's shape, which it was before it had a name
    uint16_t DRAWER_W;     // 260 a slot's panel at most (the status slot's is the system panel)
    uint8_t DRAWER_R;      // 15  its bottom corners
    uint8_t DRAWER_PAD;    // 10  inside it
    uint8_t DRAWER_GAP;    //  8  between its rows
    uint8_t DRAWER_LINE;   //  2  its edge
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
    const lv_font_t *HEADER;  // the page header's words: TAG's face, but on the
                              // P4_5 (26, matching the 4B's in mm; 2.8 round 4)
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

extern const UIShape UI_SHAPE;        // the card window's shape and spacing (2.12)
extern const UIHeader UI_HEADER;      // the page header's slots (2.8)

// Colour roles for UI::paint() - shared styles that repaint themselves on a
// scheme change. #64; the full story is at UI::paint() below.
enum class UIPaint : uint8_t {
    PAINT_SURFACE = 0,   // bg SURFACE, border UI::border()     - panels
    PAINT_SURFACE_ALT,   // bg SURFACE_ALT, border UI::border() - buttons
    PAINT_TEXT,          // text TEXT
    PAINT_TEXT_DIM,      // text TEXT_DIM
    PAINT_ACCENT_TEXT,   // text ACCENT                         - panel titles
    PAINT_ACCENT_BG,     // bg ACCENT
    // SHADOW ONLY - no colour of their own. The scheme's drop shadow, for the
    // chrome that should sit above the page like the cards do: toast, header,
    // drawer, deck panels, card tags. LIFT_SM is a smaller one for buttons.
    // Both are nothing at all on a scheme whose metrics have SHADOW 0, which
    // today is every scheme but Linen. The owner, 2026-09-23.
    PAINT_LIFT,
    PAINT_LIFT_SM,
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
const UIShape   &shape();   // millimetres - see section 2b
const UIHeader  &hdr();     // logical px - see section 2c

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
// The active colour ("on") in place of the scheme's, until the scheme changes;
// 0 puts the scheme's own back. For trying colours on glass (round 6,
// /popup?active=) - the caller restyles the cards.
void setActiveColour(uint32_t hex);
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

// Called whenever the shared paints are brought up to date (a scheme or accent
// change), before LVGL is told every style changed: a layer above the tokens
// that keeps shared styles of its own - the component kit (2.12) - refreshes
// them here. Up to four; a second registration of the same function is ignored.
void onRepaint(void (*cb)());

// --- Helpers ---------------------------------------------------------------
int32_t    sc(int32_t logical);          // logical px -> this board's pixels
int32_t    mm(float v);                  // millimetres on glass -> this board's pixels (section 2b)
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
// The scheme's darkest colour, near black (the ink on lit glass, warmed).
uint32_t darkest();

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

// LET A CONTAINER'S CHILDREN CAST SHADOWS OUTSIDE IT.
//
// LVGL clips children to their parent. LV_OBJ_FLAG_OVERFLOW_VISIBLE relaxes
// that - but only to the parent's coords PLUS THE PARENT'S OWN ext_draw_size
// (lv_refr.c, lv_obj_redraw), and a plain container's is zero. That is why the
// card shadow showed only inside the rounded corners until the wrapper
// declared a margin. This does both halves, with a margin sized from the
// largest shadow any scheme draws - a fixed number, so a scheme change never
// leaves a container with a margin computed for the previous one.
void unclipShadows(lv_obj_t *container);

// Silences the `lv_part_t | lv_state_t` deprecation warning that would
// otherwise be reproduced in every card type. Issue #13 asked for this.
inline lv_style_selector_t part(lv_part_t p, lv_state_t s = LV_STATE_DEFAULT) {
    return (lv_style_selector_t)p | (lv_style_selector_t)s;
}

// Scroll behaviour, applied to any container that may overflow.
//
// Four things, all of which matter more on these panels than on a phone:
//   - vertical only, so a slightly-too-wide row can never start a sideways drag
//   - no elastic rubber-banding at the ends, which looks terrible at the refresh
//     rates the S3 boards manage
//   - no scrollbar, since it is one more thing to redraw
//   - clickable, so a swipe that starts on a heading or a gap scrolls it too
//
// Call it on every scrollable container rather than remembering four flags.
void tameScroll(lv_obj_t *o);

// Minimum comfortable touch target: ~9 mm, the width of a fingertip. Derived
// from real density, so it is 60 px at 170 PPI and 104 px at 294 PPI rather
// than a number someone guessed per board.
int32_t minTouch();

} // namespace UI
