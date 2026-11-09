#pragma once
// ---------------------------------------------------------------------------
// THE PAGE HEADER'S SLOTS - docs/design/header-slots.md, DECISIONS D-11 to D-21.
//
// The header has three PLACES (D-16): the left holds the clock (with its bell)
// and the weather; the middle the page's name and dots, on the bar's centre; the
// right the status glyphs, with the alarm's notice and the AP mast. A SLOT is one thing in a place, and
// it is DATA: its kind, its place and, where it has one, its entity. Each slot
// makes a CLAIM on its place's room (D-19): what it shows in each of its forms
// and how hard each form holds its room - its KEEP. When a side would run into
// the middle, the lowest keep gives way first (Layout/HeaderPlan.h, tested on
// the PC); a resident never does. A claim whose condition does not hold - the
// alarm while none is set (D-20) - takes no room at all.
//
// A tap on a slot slides down that slot's own panel, joined to it (D-13, D-23);
// the status slot's panel is the system panel.
//
// A new kind is a SlotKind, a row in HeaderSlots.cpp's table (its name and its
// keeps), its build and update in Panel_Header.cpp, and a panel in
// SlotPanels.cpp if it has one. Never layout code in the header itself. A
// notice (#76, not built) will be another claim.
//
// Nothing critical lives only here (D-11): a page may hide its header.
// ---------------------------------------------------------------------------
#include <stdint.h>
#include "Layout/HeaderPlan.h"

// Prefixed, never a bare word: Arduino's macros eat single ALL-CAPS words
// (CLAUDE.md, "Arduino's global macro namespace will eat your enum").
// SLOT_CLOCK holds the bell while an alarm is within a week (D-21); SLOT_ALARM
// is the alarm's NOTICE among the status icons, within the owner's warning time
// (12 hours unless set); SLOT_AP the access point's mast while one is up (D-24).
enum class SlotKind : uint8_t { SLOT_CLOCK, SLOT_PAGE, SLOT_STATUS, SLOT_SENSOR, SLOT_ALARM, SLOT_AP, SLOT_KIND_COUNT };

using Layout::HeaderPlace;

struct SlotSpec {
    SlotKind    kind   = SlotKind::SLOT_CLOCK;
    HeaderPlace place  = HeaderPlace::PLACE_LEFT;
    const char *entity = nullptr;   // SLOT_SENSOR: the entity it shows (EntityDescriptor::id)
};

// A kind's claim, fleet-wide: its forms, fullest first, each held with a keep
// (Layout::HeaderClaim::keep). A slot's EXTRAS are what its forms shed, in
// order - form 1 has lost extras[0], form 2 extras[1] as well - and its last
// keep is for staying on the bar at all. KEEP_ALWAYS never gives way.
struct SlotKindInfo {
    SlotKind    kind;
    const char *name;    // "clock", ... - /popup?slot=, the System Doctor
    uint8_t     forms;   // 1 + the extras it can shed
    uint8_t     keep[Layout::HEADER_FORMS_MAX];
};
const SlotKindInfo &slotKindInfo(SlotKind k);
const char *slotKindName(SlotKind k);
bool        slotKindFromName(const char *name, SlotKind &out);

// The header every page has unless its PageSpec names its own
// (PageSpec::headerSlots): the clock (and its bell) and the deck's outdoor
// temperature (from HA - on the House page's cards as "Deck") on the left, the
// page in the middle, the alarm's notice, the AP mast and the status glyphs on
// the right.
extern const SlotSpec HEADER_DEFAULT[];
extern const uint8_t  HEADER_DEFAULT_COUNT;
static constexpr uint8_t HEADER_SLOTS_MAX = Layout::HEADER_CLAIMS_MAX;

// HOW THE BAR'S GLYPHS ARE FRAMED. Every glyph in the bar takes the style (the
// status glyphs, the alarm's bell, the sensor's icon); words and numbers never
// do.
//   GLYPH_BARE    the glyph alone
//   GLYPH_SQUARE  a rounded square of a shade behind it - the default
//   GLYPH_CIRCLE  a circle in the glyph's own colour, the glyph in the bar's
//                 colour
// Saved by NAME (Settings: system.header.glyphs; the default as nothing); UI
// Tweaks has the switcher (D-25).
enum class GlyphStyle : uint8_t { GLYPH_BARE, GLYPH_SQUARE, GLYPH_CIRCLE, GLYPH_STYLE_COUNT };
static constexpr GlyphStyle GLYPH_DEFAULT = GlyphStyle::GLYPH_SQUARE;
const char *glyphStyleName(GlyphStyle s);    // "bare", "square", "circle"
const char *glyphStyleLabel(GlyphStyle s);   // "Bare", "Square", "Circle" - a button's words
bool        glyphStyleFromName(const char *name, GlyphStyle &out);
