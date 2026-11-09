#pragma once
//
// "Edit cards" from the page panel's CARDS (DECISIONS K148, K149). It holds the
// page's adding, hiding, removing and restoring of cards in one window.
//
// A window with the page's room holding EditCards: the page's cards (to hide or
// remove), its hidden cards (to restore) and Home Assistant's areas (to add).
// Adding goes through Add to dashboard's group step - which group the new cards
// join, Area (each its own HA area's), New group or None - and every action ends
// in a go-or-no-go question. The page is built again once the window has gone.
//
#include "UI/EditCards.h"

namespace EditCardsSheet {

struct Spec {
    const char     *page = nullptr;      // the page's title
    EditCards::List lists[2] = {};       // On the page (the target), Hidden
    uint8_t         nLists = 0;
    void           *ctx = nullptr;       // the lists' own
};

bool open(const Spec &spec);   // false: a sheet is showing
void close();
bool isOpen();

#ifdef DEBUG_POPUP
bool goDebug();                // the question's button, or the group step's Next
bool groupDebug(uint8_t i);    // a chip of the group step
#endif

}  // namespace EditCardsSheet
