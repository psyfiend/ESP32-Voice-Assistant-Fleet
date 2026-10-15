// The page header's slots: the default header and the name tables. See
// include/UI/HeaderSlots.h; the slots are built by Panel_Header.cpp, their
// panels by SlotPanels.cpp.
#include "UI/HeaderSlots.h"

#include <string.h>

// The owner's four, left to right (D-12): "1:55am | House oO | status icons |
// climate/secondary".
const SlotSpec HEADER_DEFAULT[] = {
    { SlotKind::SLOT_CLOCK,  nullptr },
    { SlotKind::SLOT_PAGE,   nullptr },
    { SlotKind::SLOT_STATUS, nullptr },
    { SlotKind::SLOT_SENSOR, "outside_deck_temp" },
};
const uint8_t HEADER_DEFAULT_COUNT = sizeof(HEADER_DEFAULT) / sizeof(HEADER_DEFAULT[0]);

const char *slotKindName(SlotKind k) {
    switch (k) {
        case SlotKind::SLOT_CLOCK:  return "clock";
        case SlotKind::SLOT_PAGE:   return "page";
        case SlotKind::SLOT_STATUS: return "status";
        case SlotKind::SLOT_SENSOR: return "sensor";
        default:                    return "?";
    }
}

namespace {
struct GlyphName { GlyphStyle style; const char *name; const char *label; };
const GlyphName GLYPH_NAMES[] = {
    { GlyphStyle::GLYPH_BARE,   "bare",   "Bare"   },
    { GlyphStyle::GLYPH_SQUARE, "square", "Square" },
    { GlyphStyle::GLYPH_CIRCLE, "circle", "Circle" },
};
}  // namespace

const char *glyphStyleName(GlyphStyle s) {
    for (const GlyphName &g : GLYPH_NAMES) if (g.style == s) return g.name;
    return "bare";
}

const char *glyphStyleLabel(GlyphStyle s) {
    for (const GlyphName &g : GLYPH_NAMES) if (g.style == s) return g.label;
    return "Bare";
}

bool glyphStyleFromName(const char *name, GlyphStyle &out) {
    if (!name) return false;
    for (const GlyphName &g : GLYPH_NAMES)
        if (!strcmp(g.name, name)) { out = g.style; return true; }
    return false;
}
