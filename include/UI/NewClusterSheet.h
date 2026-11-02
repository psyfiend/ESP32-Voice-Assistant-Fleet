#pragma once
//
// NewClusterSheet - "New cluster" from the page panel (2.13, DECISIONS K140 and
// K91's "making a cluster"; artifact "2.13 Add Cards Mocks", screen 3).
//
// First the area, as chips; then its entities labelled "fleet", the K92 tap set
// (lights and switches) and the header's sensors (temperature, occupancy, light
// level - K115's order, four at most) ticked to start with; Create. The cluster
// is named for the area and is in its group (K141: so linked to it); the ticked
// sensors fill its header, the rest are its tiles. It goes on the page showing,
// after the page's own cards; Rename cluster renames it.
//
// A Sheet client (UI/Sheet.h). LVGL thread.
//
namespace NewClusterSheet {

bool open(const char *pageTitle);
void close();
bool isOpen();
#ifdef DEBUG_POPUP
bool areaDebug(const char *areaId);   // /popup?ncarea=<id>: an area chosen
bool createDebug();                   // /popup?nccreate=1: its Create
#endif

}  // namespace NewClusterSheet
