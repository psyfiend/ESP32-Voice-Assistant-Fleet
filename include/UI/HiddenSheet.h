#pragma once
// "Hidden cards" (2.11 round 6): a page's hidden cards (K66, K96) in a sheet of
// their own, each with Restore - the page slot's panel opens it with "View
// hidden cards", so the panel itself stays short (the owner). Laid out as Card
// UI is: the kit's chip table, a card's name against its one chip. A Sheet
// client (docs/design/components.md, "Sheet"); GUIManager says what is hidden
// and what Restore does.
#include <stdint.h>

namespace HiddenSheet {

static constexpr uint8_t MAX = 12;   // a page's hidden cards listed, at most

// A card hidden from the page, or a member hidden from its cluster (round 7):
// each row says which - "Restore to page", "Restore to cluster" - and a member's
// name says which cluster.
struct Spec {
    const char *page = nullptr;              // the page's title, for the sheet's
    const char *names[MAX] = {};             // each hidden card, by the name it shows
    const char *chip[MAX] = {};              // ...and its row's button
    uint8_t     n = 0;
    const char *note = nullptr;              // a sentence more under the rows (cards crowded off the page)
    bool (*restore)(void *user, uint8_t i) = nullptr;   // names[i] back on the page: true when it went
    void *user = nullptr;
};

bool open(const Spec &spec);   // false when a sheet is already showing
void close();
bool isOpen();

#ifdef DEBUG_POPUP
// GET /popup?restore=N: as a tap on the Nth card's Restore. False when the sheet
// is not open or has no Nth card.
bool restoreDebug(uint8_t i);
#endif

}  // namespace HiddenSheet
