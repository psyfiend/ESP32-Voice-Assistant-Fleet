#pragma once
//
// AddCardSheet - "Add card" from the page panel (2.13, DECISIONS K140; artifact
// "2.13 Add Cards Mocks", screen 2): HA's areas in a column, HA's floors as its
// sections, beside the chosen area's entities labelled "fleet" (UI/HaPicker).
// Each chosen one becomes a card of its own on the page showing, after the
// page's own cards, in its HA area's group; the page is built again.
//
// A Sheet client (UI/Sheet.h). LVGL thread.
//
namespace AddCardSheet {

bool open(const char *pageTitle);   // false while another sheet shows
void close();
bool isOpen();
#ifdef DEBUG_POPUP
bool addDebug();                    // its Add, for /popup?pageadd=1
#endif

}  // namespace AddCardSheet
