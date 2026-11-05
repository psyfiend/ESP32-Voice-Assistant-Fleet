#pragma once
//
// NewClusterSheet - "New group" from the page panel's CARDS (2.13 round 5, the
// owner's design, DECISIONS K168; it replaced round 1's area chips and picker, K140).
//
// Full size, as Edit cards. First "Model this group after an existing HA area?" -
// HA's areas, then Create new. Then Edit cards' flow: the new group's own list
// first ("Office (new)", or "New group"), HA's areas after it; a tap adds a card,
// a second pins it, a sensor's third puts it in the header (K168), the next takes
// it off; "Create group N". Then its name - the area's, or "New group" when a group
// has that name already - with Change name (the keyboard, "New group name"); Next
// asks "Create new group?" in the owner's words. The group is linked to the area
// it was modelled after (K141); its cluster goes on the page after the page's own.
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
