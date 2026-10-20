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
// id into `idOut`. False when full, or the parent has no id. `fromId`: the
// member card it came out of (a cluster's tile, the sprint after round 7), kept
// as "from" - so its Visibility knows a copy from a move.
bool add(const Card &parent, const Entity &member, char *idOut, size_t cap, const char *fromId = nullptr);
// To the page's pool. False if it is not a card added here.
bool remove(const char *cardId);

// --- A TILE TAKEN OUT TO THE PAGE, TRACKED (the sprint after round 7, the owner:
// Outside's Front added to the dashboard stayed in the cluster, and its new card
// could only be hidden - a copy in the hidden cards that should not exist) ------
// Add card to dashboard from a cluster's tile asks Copy or Move, as Add to a
// cluster does the other way (K111). The page card records "from", the member.
//   Copy  the member stays; the page card can go again for good (drop()).
//   Move  the member is "hidden": "moved" - no tile, not in the hidden cards,
//         Members says On the page; its page card moves it back (moveBack()),
//         and is never dropped: it is the card itself.
bool moveToPage(const char *memberId);       // the member's half of a Move
bool isMovedToPage(const char *memberId);    // a member whose card is on the page now
bool movedHere(const char *pageCardId);      // a page card that IS a member moved out
bool moveBack(const char *pageCardId);       // ...back into its cluster; the page card deleted
bool drop(const char *pageCardId);           // a card added here deleted, entry and all (not a moved one)

// --- MEMBERS ADDED TO A CLUSTER (2.11 round 3, K91, K100) -----------------------
// A card copied into a cluster from the glass: a card entry with "in" (the
// cluster's id), "entity" and "area" - a copy with an id of its own (K92: a
// grouping is of copies), wearing the cluster's group (K92 rule 2). Built after
// the cluster's compiled members (CardPage::makeGroupCard()).
//
//   "cards": { "<id>": { "in": "office_261019_0500", "entity": "...", "area": "Office",
//                        "group": "ha_office", "from": "<the card it came from>" } }
//
// COPY OR MOVE, TRACKED (round 7, the owner: a card moved into a cluster turned
// up in the page's hidden cards, and could be restored AND added again from the
// cluster - two of one light). "from" names the card a member came from. Moved,
// that card is "removed": "moved" - off its page, and NOT in its pool: the
// hidden cards are only what was hidden (Hide from this page, Hide from the
// cluster). Removing the member puts the moved card back on its page.
bool    addTo(const Card &cluster, const Entity &e, char *idOut, size_t cap, const char *fromId = nullptr);
uint8_t membersIn(const char *clusterId, const CardSpec **out, uint8_t cap);
bool    inCluster(const char *clusterId, const char *entityId);   // a copy of it is in there already
bool    isMember(const char *cardId);       // a copy added to a cluster here
const char *fromOf(const char *cardId);     // the card a member, or a card added to a page, came from; or null
// Removed from its cluster: a copy made on the glass is deleted, entry and all -
// the card it was copied from is untouched; one it was MOVED from goes back.
bool    removeMember(const char *cardId);
bool    moveOut(const char *cardId);        // a card moved into a cluster: off its page, not in the pool
bool    isMoved(const char *cardId);

// --- THE PAGE'S POOL, FOR EVERY CARD (2.11 round 3, K66, K96) -----------------
// "Hide from this page" sends any card on a page there - a compiled one too:
// "removed": "yes" on its entry, as on an added card's, its settings kept.
// merge() leaves a hidden card out; the cards added after it still follow its
// place. The page's list of hidden cards brings it back (restore()).
bool isHidden(const char *cardId);
bool hide(const char *cardId);      // false: no id, or hidden already
bool restore(const char *cardId);   // false: not hidden (or moved)

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
