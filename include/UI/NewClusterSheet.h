#pragma once
//
// NewClusterSheet - "New group" from the page panel's CARDS (DECISIONS K168).
//
// Full size, as Edit cards. First "Model this group after an existing HA area?" -
// HA's areas, then Create new. Then Edit cards' flow: the new group's own list
// first ("Office (new)", or "New group"), HA's areas after it; a tap adds a card,
// a second pins it, a sensor's third puts it in the header (K168), the next takes
// it off; "Create group N". Then its name - the area's, or "New group" when a group
// has that name already - with Change name (the keyboard, "New group name"); Next
// asks "Create new group?". A group modelled after an area is linked to it
// (K141); its group card goes on the page after the page's own.
//
// A Sheet client (UI/Sheet.h). LVGL thread.
//
namespace NewClusterSheet {

bool open(const char *pageTitle);
void close();
bool isOpen();
#ifdef DEBUG_POPUP
bool areaDebug(const char *areaId);   // /popup?ncarea=<id>: an area chosen ("" Create new)
bool createDebug();                   // /popup?nccreate=1: the next step's button
#endif

}  // namespace NewClusterSheet
