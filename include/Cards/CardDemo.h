#pragma once
#ifndef CARD_DEMO_H
#define CARD_DEMO_H

#include "EntityRegistry.h"
#include "Cards/CardBinder.h"

// ---------------------------------------------------------------------------
// CardDemo - milestone 2.4's acceptance criterion, on the actual glass.
//
// It is the same kind of object ExternalEntities.h is: a hand-written stand-in
// for the build sheet (#20), which is where "this card shows deck_temp, spans
// 2, row 1" actually belongs. It is written in code because 2.5 has not built
// the page config struct yet and 3.1 has not built the sheet that fills it.
//
// It exists to make four things checkable rather than arguable:
//
//   1. BOTH header treatments, side by side and switchable live. cards.md
//      section 2 asks for exactly this and refuses to pick one on paper.
//   2. Both halves of the optimistic write - one switch that confirms and one
//      that refuses - which have never run on hardware in this project.
//   3. The aggregate light: one card, several entities, one tap, and a mixed
//      indicator rather than a card that picks a side and lies.
//   4. That a scheme change repaints live cards, which is the rule about
//      never caching a colour being enforced by something other than good
//      intentions.
//
// Opened from the System panel, loads as its own screen, restores the previous
// one on Back - deliberately the same shape as ReferencePage, which is the
// pattern that let 2.2 be judged without disturbing the dashboard.
// ---------------------------------------------------------------------------

namespace CardDemo {

void show(EntityRegistry &reg, CardBinder &binder);
void close();

} // namespace CardDemo

#endif // CARD_DEMO_H
