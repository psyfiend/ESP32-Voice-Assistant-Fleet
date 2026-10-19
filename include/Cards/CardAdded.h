#pragma once
//
// CardAdded - cards made on the device (K17, DECISIONS K65-K66; design:
// docs/design/group-cards.md section 5).
//
// The first cards that are not in the compiled dashboard: a member of a group
// card ("Desk" > Members > the lamp) made into a card of its own from SETTINGS'
// "Add as a card". A COPY - the member stays in its group card.
//
// Kept in the settings file as an ordinary card entry with a few keys more, so
// its other choices (Tap action, Entity label...) are saved exactly as any
// card's, and 3.1's build sheet takes the lot over unchanged (K34):
//
//   "cards": { "<id>": { "page": "house", "after": "<the card it came from>",
//                        "entity": "ha_light_office_lamp", "area": "Office",
//                        "pri": "140", "group": "ha_office",
//                        "removed": "yes" } }        <- only once removed
//
// A page is built from its compiled cards with each one followed by the cards
// added after it (merge()), so a new card stands RIGHT AFTER the card it came
// from (owner), with that card's priority - on a full page the lowest priority
// still drops first. Removed cards stay in the file with their settings, the
// page's pool (K35, K66), and are not built.
//
// Records are only ever appended, never moved: a Card keeps a POINTER to its id
// and the CardSpecs merge() hands out point into them.
//
// UI code; LVGL thread. Not thread-safe.
//
#include <stddef.h>
#include <stdint.h>
#include "Entity.h"            // ENTITY_ID_MAX, ENTITY_SHORT_MAX: a pool item's strings
#include "Cards/PageSpec.h"    // CARD_ID_MAX

class Card;

namespace CardAdded {

constexpr uint8_t MAX = 24;   // cards added on one device and on its pages (removed ones are in the file only)

// Read the added cards from the settings file. `compiledId` says whether an id
// is a compiled card's (so a new id never repeats one). Once, at boot, before
// the first page is built.
void begin(bool (*compiledId)(const char *id));

// The page being built, by slug: where "Add as a card" puts its card.
void setPage(const char *slug);

// The page's cards: each compiled one followed by the cards added after it (and
// after those), then any whose card is gone. Into `out`; how many.
uint8_t merge(const PageSpec &page, CardSpec *out, uint8_t cap);

// Was `cardId` added here (removed or not)? For the settings file's claims.
bool known(const char *cardId);
// Added here and still on its page.
bool isAdded(const char *cardId);
// The card already made from `entityId` out of `parentId`, still on its page,
// or null.
const char *addedFor(const char *parentId, const char *entityId);

// Make a card for `member`, right after `parent`, on the page being built. Its
// id into `idOut`. False when full, or the parent has no id.
bool add(const Card &parent, const Entity &member, char *idOut, size_t cap);
// To the page's pool. False if it is not a card added here.
bool remove(const char *cardId);

// --- MEMBERS ADDED TO A CLUSTER (2.11 round 3, K91, K100) -----------------------
// A card copied into a cluster from the glass: a card entry with "in" (the
// cluster's id), "entity" and "area" - a copy with an id of its own (K92: a
// grouping is of copies), wearing the cluster's group (K92 rule 2). Built after
// the cluster's compiled members (CardPage::makeGroupCard()).
//
//   "cards": { "<id>": { "in": "office_261019_0500", "entity": "...", "area": "Office",
//                        "group": "ha_office" } }
bool    addTo(const Card &cluster, const Entity &e, char *idOut, size_t cap);
uint8_t membersIn(const char *clusterId, const CardSpec **out, uint8_t cap);
bool    inCluster(const char *clusterId, const char *entityId);   // a copy of it is in there already

// --- THE PAGE'S POOL, FOR EVERY CARD (2.11 round 3, K66, K96) -----------------
// "Hide from this page" sends any card on a page there - a compiled one too:
// "removed": "yes" on its entry, as on an added card's, its settings kept.
// merge() leaves a hidden card out; the cards added after it still follow its
// place. The page's list of hidden cards brings it back (restore()).
bool isHidden(const char *cardId);
bool hide(const char *cardId);      // false: no id, or hidden already
bool restore(const char *cardId);   // false: not hidden

// A hidden card on a page, for its list: its id, the entity it shows (a group
// card's first member's), its dashboard name (null: none - HA's), its area.
struct PoolItem {
    char        id[CARD_ID_MAX + 1];
    char        entity[ENTITY_ID_MAX];
    const char *label;
    char        area[ENTITY_SHORT_MAX];
};
uint8_t pool(const PageSpec &page, PoolItem *out, uint8_t cap);

// True once after add(), remove(), hide() or restore(): the page must be rebuilt
// to show it (GUIManager, once no card window is open).
bool takeChanged();

}  // namespace CardAdded
