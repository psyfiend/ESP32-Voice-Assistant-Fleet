#pragma once
// "Hidden cards": a page's hidden cards (K66, K96) in a sheet of their own,
// each with Restore. The page panel has no button for it now (Edit cards lists
// and restores hidden cards); /popup?hidden=1 opens it in debug builds. Laid
// out as Card UI is: the kit's chip table, a card's name against its one chip. A Sheet client
// (docs/design/components.md, "Sheet"); GUIManager says what is hidden and what
// Restore does.
#include <stdint.h>

namespace HiddenSheet {

static constexpr uint8_t MAX = 12;   // a page's hidden cards listed, at most

// A page's hidden cards. A member hidden from its cluster is listed and restored
// in the cluster's Members view instead (K125). Each row's button words are the
// caller's, "Restore" when it gives none.
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
