#pragma once
// ---------------------------------------------------------------------------
// THE PAGE HEADER'S SLOTS (2.8, #19) - docs/design/header-slots.md, DECISIONS
// D-11 to D-15.
//
// The header is a row of slots, each an equal share of the bar with no true
// middle - four by default, left to right: the clock, the page (its name and
// dots), status (the HA / MQTT link and Wi-Fi) and a sensor (the outdoor
// temperature). A slot is DATA: its kind and, where it has one, its entity. A
// tap on a slot slides down that slot's own panel from its place; the status
// slot's panel is the system panel.
//
// A new kind is a new SlotKind and an entry in Panel_Header.cpp's table - its
// build and its update - and a panel in SlotPanels.cpp if it has one. Never
// layout code in the header itself.
//
// Nothing critical lives only here (D-11): a page may hide its header.
// ---------------------------------------------------------------------------
#include <stdint.h>

// Prefixed, never a bare word: Arduino's macros eat single ALL-CAPS words
// (CLAUDE.md, "Arduino's global macro namespace will eat your enum").
enum class SlotKind : uint8_t { SLOT_CLOCK, SLOT_PAGE, SLOT_STATUS, SLOT_SENSOR, SLOT_KIND_COUNT };

struct SlotSpec {
    SlotKind    kind   = SlotKind::SLOT_CLOCK;
    const char *entity = nullptr;   // SLOT_SENSOR: the entity it shows (EntityDescriptor::id)
};

// The header every page has unless its PageSpec names its own
// (PageSpec::headerSlots). The sensor is the deck's outdoor temperature, from
// HA - on the House page's cards as "Deck".
extern const SlotSpec HEADER_DEFAULT[];
extern const uint8_t  HEADER_DEFAULT_COUNT;
static constexpr uint8_t HEADER_SLOTS_MAX = 6;

const char *slotKindName(SlotKind k);   // "clock", "page", "status", "sensor"

// HOW THE BAR'S GLYPHS ARE FRAMED - #19's open question, built as a setting so
// all three can be compared on glass. Every glyph in the bar takes it (the
// status glyphs, the sensor's icon); words and numbers never do.
//   GLYPH_BARE    the glyph alone, as the header has always drawn it
//   GLYPH_SQUARE  a rounded square of a shade behind it (#19's "2+")
//   GLYPH_CIRCLE  a circle in the glyph's own colour, the glyph in the bar's
//                 colour (#19's treatment 4)
// Saved by NAME (Settings: system.header.glyphs); set from the status panel's
// Glyphs button, or a PC: /settings?header_glyphs=bare|square|circle.
enum class GlyphStyle : uint8_t { GLYPH_BARE, GLYPH_SQUARE, GLYPH_CIRCLE, GLYPH_STYLE_COUNT };
const char *glyphStyleName(GlyphStyle s);    // "bare", "square", "circle"
const char *glyphStyleLabel(GlyphStyle s);   // "Bare", "Square", "Circle" - a button's words
bool        glyphStyleFromName(const char *name, GlyphStyle &out);
