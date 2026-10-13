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

class Card;
struct Entity;
struct CardSpec;
struct PageSpec;

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
// Added here and still on its page: SETTINGS offers "Remove card".
bool isAdded(const char *cardId);
// The card already made from `entityId` out of `parentId`, still on its page,
// or null.
const char *addedFor(const char *parentId, const char *entityId);

// Make a card for `member`, right after `parent`, on the page being built. Its
// id into `idOut`. False when full, or the parent has no id.
bool add(const Card &parent, const Entity &member, char *idOut, size_t cap);
// To the page's pool. False if it is not a card added here.
bool remove(const char *cardId);

// True once after add() or remove(): the page must be rebuilt to show it
// (GUIManager, once no card window is open).
bool takeChanged();

}  // namespace CardAdded
