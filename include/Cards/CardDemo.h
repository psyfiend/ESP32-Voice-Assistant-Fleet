#pragma once
#ifndef CARD_DEMO_H
#define CARD_DEMO_H

#include "EntityRegistry.h"
#include "Cards/CardBinder.h"
#include <functional>

// ---------------------------------------------------------------------------
// CardDemo - the card bench: hand-built cards on the actual glass.
//
// It is the same kind of object ExternalEntities.h is: a hand-written test
// fixture, written in code rather than taken from a page spec.
//
// It makes four things checkable rather than arguable:
//
//   1. BOTH header treatments, side by side and switchable live. cards.md
//      section 2 asks for exactly this and refuses to pick one on paper.
//   2. Both halves of the optimistic write - one switch that confirms and one
//      that refuses.
//   3. The aggregate light: one card, several entities, one tap, and a mixed
//      indicator rather than a card that picks a side and lies.
//   4. That a scheme change repaints live cards, which is the rule about
//      never caching a colour being enforced by something other than good
//      intentions.
//
// Opened from the System panel, loads as its own screen, restores the previous
// one on Back - deliberately the same shape as ReferencePage, so it can be
// judged without disturbing the dashboard.
// ---------------------------------------------------------------------------

namespace CardDemo {

void show(EntityRegistry &reg, CardBinder &binder);

// What the "Dump" button runs. Registered by GUIManager, for the same reason
// Panel_System's is: the full System Doctor report needs SystemCore, and this
// page has no business knowing SystemCore exists.
void setDumpHandler(std::function<void()> cb);

// Run when the bench closes. The bench borrows UI::grid() - it derives the
// grid from ITS host, which is the screen minus a button bar, and that is
// global state the dashboard also reads. Handing back control rather than
// guessing at the previous viewport is the only way the dashboard comes back
// in its own shape; GUIManager registers a rebuild here.
void setCloseHandler(std::function<void()> cb);
void close();

} // namespace CardDemo

#endif // CARD_DEMO_H
