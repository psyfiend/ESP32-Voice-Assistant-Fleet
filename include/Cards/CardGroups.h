#pragma once
//
// CardGroups - what a card's CARD LABEL says and what colour it is (2.10d,
// DECISIONS K42, K44; design: docs/design/card-sheet.md section 17).
//
// A card label shows the card's GROUP, and the colour belongs to the group, so
// every card in it shares one. Where a card's group comes from, by its saved
// "group" setting (Settings.h, per card - K44 G1):
//
//   (nothing saved)   the dashboard's area for it (CardSpec::area) - today's
//                     look; its group id is "sheet_<name>"
//   "ha_area"         its primary entity's area in Home Assistant, learnt once
//                     per session; followed when it arrives or changes
//   "<group id>"      one of the dashboard's areas ("sheet_...") or a custom
//                     group made by the owner (Settings::newGroup())
//   "own"             its own words ("label_text", may be empty) - it has left
//                     any group (D-7's Detach), so a group's colour change
//                     does not reach it; shown even with no words
//   "none"            no card label
//
// A group's colour is its saved one (groups.<id>.color) or else today's
// colour-by-name (cardAreaColor()), so a dashboard area and an HA area of the
// same name match, and nothing changes until the owner changes a colour.
// Whether colour is used at all is still the page's (PageSpec::areaColor).
//
// UI code: no LVGL here beyond what Card does. Not thread-safe; LVGL thread.
//
#include <stddef.h>
#include <stdint.h>

class Card;
struct PageSpec;

namespace CardGroups {

constexpr const char *SHEET_PREFIX = "sheet_";
constexpr const char *HA_PREFIX    = "ha_";

// The dashboard's pages, for the areas they name. Once, from GUIManager.
void setPages(const PageSpec *const *pages, uint8_t n);

// "<prefix><name as lowercase letters, digits and _>".
void idFor(const char *prefix, const char *name, char *out, size_t cap);

// A group's colour: its saved one, else the colour of its name.
uint32_t colorOfGroup(const char *groupId, const char *name);
uint32_t colorFor(const char *prefix, const char *name);   // idFor() + colorOfGroup()

// Apply the card's saved card label over what its spec set: words, colour (if
// `areaColor`, the page's setting), following the HA area, shown without words.
void apply(Card &c, bool areaColor);

// SETTINGS' Card label row for a card: the choices as dropdown lines into
// `opts`, what each line is into `out`, and the line now chosen into `sel`.
enum ChoiceKind : uint8_t { CH_HA_AREA, CH_GROUP, CH_OWN, CH_NONE };
struct Choice { ChoiceKind kind; char gid[48]; };
uint8_t rowChoices(const Card &c, Choice *out, uint8_t cap, char *opts, size_t optsCap, uint16_t &sel);

// The owner chose `ch` for the card: kept, and applied to it at once.
void choose(Card &c, const Choice &ch, bool areaColor);

}  // namespace CardGroups
