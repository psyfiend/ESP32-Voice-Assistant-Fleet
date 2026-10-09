#pragma once
//
// CardGroups - which GROUP a card is in, and so what its CARD LABEL says and
// what colour it is (2.10d, DECISIONS K42, K44, K46; design: docs/design/
// card-sheet.md section 17).
//
// ONE KIND OF GROUP (K46): the dashboard's own version of an area. A name, a
// colour, and perhaps a link to an HA area. Groups belong to the device, not to
// a page, so a group spans pages as an HA area does. A card is in one group or
// none (per card, K44 G1). COLOUR IS THE GROUP'S IDENTITY: every card in a
// group wears its colour; the words are the group's name unless the card has
// its own (and keeps the colour). Being together on a page means nothing.
//
// Where groups come from:
//   ha_<area_id>    an HA area, linked: made for every area an entity on the
//                   board is in. Its name is HA's, or a local rename (saved),
//                   or - when one of the dashboard's areas is that area under a
//                   shorter name - the dashboard's. A dashboard area IS an HA
//                   area when every HA card in it sits in that one HA area.
//   sheet_<name>    a dashboard area that is not one HA area: a local group
//   <name>_<stamp>  made by the owner (saved: groups.<id>.name), kept until
//                   deleted; or made by "Own group" for one card (kind "solo"),
//                   gone when its last card leaves
//
// A card's group: its saved "group" (a group id, or "none"), else its
// dashboard area's. A group's colour: its saved one, else the colour of its
// name - so an untouched dashboard looks exactly as before 2.10d. A new group
// made here takes a colour no other group is using, while one is left.
//
// UI code; LVGL thread. Not thread-safe.
//
#include <stddef.h>
#include <stdint.h>

class Card;
class EntityRegistry;
struct PageSpec;

namespace CardGroups {

constexpr const char *SHEET_PREFIX = "sheet_";
constexpr const char *HA_PREFIX    = "ha_";

// The dashboard's pages and the registry (for the HA areas). Once, from
// GUIManager.
void setSources(const PageSpec *const *pages, uint8_t n, const EntityRegistry *reg);

struct Group {
    char     id[48];
    char     name[40];
    uint32_t color;
    char     haName[40];   // linked to an HA area: HA's name for it; else ""
    bool     solo;         // made by "Own group" for one card
};

// A group by id; false if no such group is known.
bool byId(const char *gid, Group &g);
// The card's group now; false = none.
bool ofCard(const Card &c, Group &g);

// Apply the card's group to its card label: words (its own if it has them,
// else the group's name), colour (if `areaColor`, the page's setting).
void apply(Card &c, bool areaColor);

// SETTINGS' Card label row: every group, "Own group", "None". The lines into
// `opts`, what each is into `out`, the one chosen now into `sel`.
enum ChoiceKind : uint8_t { CH_GROUP, CH_OWN, CH_NONE };
struct Choice { ChoiceKind kind; char gid[48]; };
uint8_t rowChoices(const Card &c, Choice *out, uint8_t cap, char *opts, size_t optsCap, uint16_t &sel);

// The owner chose: kept, and applied at once. Leaving a "solo" group that
// nothing else is in deletes it.
void choose(Card &c, const Choice &ch, bool areaColor);

// Every known group, for the System Doctor and /settings?stats=1.
void forEach(void (*fn)(const Group &g, void *ctx), void *ctx);

}  // namespace CardGroups
