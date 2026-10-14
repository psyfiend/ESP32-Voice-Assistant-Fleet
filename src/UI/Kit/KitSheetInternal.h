#pragma once
// What the sheet's files share and no one else sees (2.12 P4): KitSheet.cpp (the
// frame), KitSheetPanels.cpp (the two folder panels), KitSparks.cpp (the sparks).
#include "UI/Sheet.h"

namespace Kit {

extern Sheet *s_open;                 // the sheet showing, or null

void rightCreate(Sheet &S);           // SETTINGS, built with the sheet, folded below the screen
void rightEnd(Sheet &S);              // and gone with it

void sparksCreate();                  // the sparks' pieces, inside the quiet build
void sparksStart();                   // their clock, once the sheet shows
void sparksEnd();

} // namespace Kit
