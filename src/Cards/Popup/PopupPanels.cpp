// The folder panels as the card window uses them (2.12 P1, split from CardPopup.cpp; the sheet's
// since P4 - KitSheetPanels.cpp builds, slides and drags them). SETTINGS on the right, whose rows
// are PopupSettingsRows.cpp; the left panel - CHART, ACTIVITY or SCENES - whose rows are
// PopupSideRows.cpp, and which this file chooses per view.
#include "PopupState.h"

namespace popup {

// By the window's names for them (the settings deck, the CHART panel).
void deckSet(uint8_t state)  { Kit::sheetRightSet(s, state); }
void deckOpen()              { Kit::sheetRightOpen(s); }
void deckSync()              { Kit::sheetRightSync(s); }
bool panelOpen()             { return Kit::sheetPanelOpen(s); }
void chartSet(uint8_t state) { Kit::sheetSideSet(s, state); }
void chartFold()             { Kit::sheetSideFold(s); }
void chartEnd()              { Kit::sheetSideEnd(s); }

// THE LEFT PANEL IS CHART, ACTIVITY OR SCENES (2.10d, DECISIONS K43; 2.10e).
// History and Scenes are never showing at once, so one panel in the left half
// serves both: CHART's rows (or ACTIVITY's) on History, SCENES' on Scenes.
// CLUSTER (2.11 round 16) is a cluster's Members': what the cluster is called,
// what its header shows, and the cards that join it.
const char *sideTitle(uint8_t side) {
    return side == SIDE_SCENES ? "SCENES" : side == SIDE_ACTIVITY ? "ACTIVITY"
         : side == SIDE_CLUSTER ? "CLUSTER" : "CHART";
}

uint8_t (*sideRows(uint8_t side))(DeckRowSpec *) {
    return side == SIDE_SCENES ? scenesSpecs : side == SIDE_ACTIVITY ? activitySpecs
         : side == SIDE_CLUSTER ? clusterSpecs : chartSpecs;
}

// The left panel belongs to its view (K43): CHART to History, SCENES to
// Scenes - up with it, gone when it goes. Another view's panel is ended and the
// right one made; the same view rebuilt (a scene list that changed) keeps its
// panel, open or not.
void sideFor(PopupView v) {
    // History's panel follows its subject (2.10e): CHART for a number, ACTIVITY
    // for the rest, none where there is no HA history. Another subject (a
    // member's history after the group's) gets its panel made afresh.
    uint8_t side = SIDE_NONE;
    if (v == PopupView::VIEW_HISTORY) {
        if (histSync() && s.left.root) chartEnd();
        if (histFromHa(s_hv.e)) side = s_hv.chart ? SIDE_CHART : SIDE_ACTIVITY;
    } else if (v == PopupView::VIEW_SCENES) {
        side = SIDE_SCENES;
    } else if (v == PopupView::VIEW_MEMBERS && s.cluster && !inMember()) {
        side = SIDE_CLUSTER;
    }
    if (s.left.root && s.left.kind != side) chartEnd();
    if (side != SIDE_NONE && !s.left.root) Kit::sheetSide(s, side, sideTitle(side), sideRows(side));
}

} // namespace popup
