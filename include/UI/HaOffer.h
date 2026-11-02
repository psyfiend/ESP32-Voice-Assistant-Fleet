#pragma once
//
// HaOffer - what Home Assistant offers this board, as the pickers show it (2.13,
// DECISIONS K140-K142; design/ha-cards.md 2.4).
//
// HaCatalog (Fleet_Providers) holds HA's answer; this is the UI's side of it:
// the rows a picker lists, which of them are new, the notice when a linked area
// gains one, and making the entity for a row the owner adds. LVGL thread.
//
// NEW: an entity id HA labels "fleet" that the board has not shown the owner
// yet. The ids it has shown are kept in the settings file (system.ha.known);
// the first catalogue a board ever reads knows them all, so nothing is new on
// the day this arrives. An area's new ones become known when a picker lists
// that area (seen()).
//
#include <stddef.h>
#include <stdint.h>

class HaCatalog;
class HaProvider;
class EntityRegistry;
struct Entity;

namespace HaOffer {

void begin(HaCatalog *cat, HaProvider *prov, EntityRegistry *reg);
void tick();       // from GUIManager's loop: a new catalogue, its notices
void refresh();    // ask HA again (a picker opening)
bool ready();      // a catalogue has arrived this boot
uint32_t epoch();  // counts catalogues; a picker rebuilds when it moves

struct AreaRow {
    char    id[32];          // "" = No area
    char    name[40];
    char    floorName[32];   // "" on no floor
    uint8_t items;
    uint8_t fresh;           // how many are new
};
// Every area with a fleet entity, floors in HA's order, then the areas on no
// floor, then No area (only if some entity has none).
uint8_t areas(AreaRow *out, uint8_t cap);
const char *areaName(const char *areaId);   // "" when unknown; "No area" is the caller's word

struct Row {
    char ref[72];
    char name[48];
    char areaId[32];
    char dc[20];
    char unit[12];
    char state[24];
    bool isNew;
    const Entity *have;      // the board's entity for it, if it has one
};
// One area's entities ("" = those with no area), in HA's order.
uint8_t rows(const char *areaId, Row *out, uint8_t cap);
uint8_t newIn(const char *areaId);   // how many of an area's are new
void    seen(const char *areaId);    // the owner has been shown them: known now

// The board's entity for a row: the one it has, else made now (EntityRegistry::
// adopt) from HA's domain, device class and unit. Null for a kind no card can
// show yet, or a full table. `made` says it was made here - the card must then
// carry "ha", "ha_dc" and "ha_unit" (CardAdded) so the next boot makes it again.
const Entity *entityFor(const Row &r, bool *made = nullptr);

// A GROUP'S HA AREA (K141): its saved link (groups.<id>.ha_area: an area or a
// floor id, or "none"), else the area an "ha_" group is made from, else an HA
// area or floor with the group's name ("Outside", a floor). False: none. A
// floor stands for every area on it.
bool groupArea(const char *gid, char *out, size_t cap);
bool setGroupArea(const char *gid, const char *areaOrFloorId);   // "none" unlinks
// The name of an area or floor id ("Office", "Outside"); "" when unknown.
const char *placeName(const char *areaOrFloorId);
// Is `areaId` that place, or on it (a floor)?
bool inPlace(const char *areaId, const char *place);

// For /popup?catalog=1.
void dump(void (*out)(const char *line, void *ctx), void *ctx);

}  // namespace HaOffer
