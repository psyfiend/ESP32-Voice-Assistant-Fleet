// The page header's slots: the default header, each kind's claim, the name
// tables. See include/UI/HeaderSlots.h; the slots are built by
// Panel_Header.cpp, placed by Layout/HeaderPlan, their panels by SlotPanels.cpp.
#include "UI/HeaderSlots.h"

#include <string.h>

using Layout::KEEP_ALWAYS;

// The three places (D-16, D-18, D-21): "status icons on the right, page info
// in the middle", the clock (its bell inside it) and the weather on the left;
// the alarm's notice and the AP mast among the status icons. In each place,
// left to right.
const SlotSpec HEADER_DEFAULT[] = {
    { SlotKind::SLOT_CLOCK,  HeaderPlace::PLACE_LEFT,   nullptr },
    { SlotKind::SLOT_SENSOR, HeaderPlace::PLACE_LEFT,   "outdoor_temp" },
    { SlotKind::SLOT_PAGE,   HeaderPlace::PLACE_MIDDLE, nullptr },
    { SlotKind::SLOT_ALARM,  HeaderPlace::PLACE_RIGHT,  nullptr },
    { SlotKind::SLOT_AP,     HeaderPlace::PLACE_RIGHT,  nullptr },
    { SlotKind::SLOT_STATUS, HeaderPlace::PLACE_RIGHT,  nullptr },
};
const uint8_t HEADER_DEFAULT_COUNT = sizeof(HEADER_DEFAULT) / sizeof(HEADER_DEFAULT[0]);

// THE KEEPS - what gives way first when a side would run into the middle: the
// clock's bell (15 - the notice and the clock's panel still say when), the
// temperature's icon (20), the page's name (30); then the temperature itself
// (60). The alarm's notice (210) and the AP mast (220) are warnings, held
// against nearly everything; the clock, the page's dots and the status glyphs
// are residents.
namespace {
const SlotKindInfo KINDS[] = {
    //  kind                  name       forms  keep per form
    { SlotKind::SLOT_CLOCK,  "clock",  2, { 15, KEEP_ALWAYS } },          // time and bell; the time
    { SlotKind::SLOT_PAGE,   "page",   2, { 30, KEEP_ALWAYS } },          // name and dots; the dots
    { SlotKind::SLOT_STATUS, "status", 1, { KEEP_ALWAYS } },
    { SlotKind::SLOT_SENSOR, "sensor", 2, { 20, 60 } },                   // icon and value; the value
    { SlotKind::SLOT_ALARM,  "alarm",  1, { 210 } },                      // the ringing bell
    { SlotKind::SLOT_AP,     "ap",     1, { 220 } },                      // the mast
};
}  // namespace

const SlotKindInfo &slotKindInfo(SlotKind k) {
    for (const SlotKindInfo &i : KINDS) if (i.kind == k) return i;
    return KINDS[0];
}

const char *slotKindName(SlotKind k) {
    for (const SlotKindInfo &i : KINDS) if (i.kind == k) return i.name;
    return "?";
}

bool slotKindFromName(const char *name, SlotKind &out) {
    if (!name) return false;
    for (const SlotKindInfo &i : KINDS)
        if (!strcmp(i.name, name)) { out = i.kind; return true; }
    return false;
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
    return glyphStyleName(GLYPH_DEFAULT);
}

const char *glyphStyleLabel(GlyphStyle s) {
    for (const GlyphName &g : GLYPH_NAMES) if (g.style == s) return g.label;
    return glyphStyleLabel(GLYPH_DEFAULT);
}

bool glyphStyleFromName(const char *name, GlyphStyle &out) {
    if (!name) return false;
    for (const GlyphName &g : GLYPH_NAMES)
        if (!strcmp(g.name, name)) { out = g.style; return true; }
    return false;
}
