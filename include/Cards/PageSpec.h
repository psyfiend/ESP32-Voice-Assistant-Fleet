#pragma once
#ifndef PAGE_SPEC_H
#define PAGE_SPEC_H

#include "Cards/CardTypes.h"

// ---------------------------------------------------------------------------
// What a page IS, as data.
//
// This is the shape milestone 2.5 owes and the shape the build sheet (#20)
// will serialise into. Everything on a dashboard today is constructed by hand
// in CardDemo.cpp; a CardSpec is that same construction written down instead
// of performed, and the difference is the whole point: 3.3's JSON loader fills
// in these structs, so it becomes a parser rather than a second design.
//
// It carries exactly what milestone 2.4 established a sheet entry has to
// express, and HANDOFF.md lists:
//
//   a domain          NOT here - see below
//   a header mode     bar / tag / none, or inherit the page's
//   area on, colour on   two independent settings, on the page
//   spans and priority   CardPlacement, in UNITS - see CardTypes.h
//   a variant override   for when VAR_AUTO guesses wrong
//   staleness overrides  longStaleMs, per card
//
// THERE IS NO DOMAIN FIELD, and that is deliberate rather than an omission.
// A card's type comes from its primary entity's EntityKind by way of
// cardForEntity(), which is already how every card in the project is built.
// Writing the domain here as well would let a sheet disagree with the registry
// about what a thing is, and there is no good answer to that disagreement.
// When the build sheet learns to DECLARE entities as well as place them, the
// domain belongs on the declaration - next to the topic - and not here.
// ---------------------------------------------------------------------------

// The page's header mode, rather than this card's own. Not a value of
// CardHeaderStyle because HDR_NONE is 0 and is a real choice, so zero-fill
// cannot mean "inherit" - it has to be a sentinel outside the enum's range.
static constexpr uint8_t CARD_HDR_INHERIT = 0xFF;

// Sub-grid units. ROADMAP Q3b, decided 2026-09-03 and unchanged:
//
//   "Author the page as 3x3 CELLS, but allocate 6x6 UNITS underneath (each
//    cell = 2x2 units). Cards span in units. A quarter-page card is 3x3 units
//    - exactly expressible. A normal card is 2x2 units."
//
// So a page declares how finely its cells divide and every span, every
// coordinate and every minimum below is in UNITS. Two is the default because
// halves are the case that actually comes up; a page needing thirds sets 3.
static constexpr uint8_t PAGE_SUBDIVISION_DEFAULT = 2;

// Sentinel for "no explicit coordinate - let the page flow me".
// Flow is the normal case and explicit placement is the exception, which is
// why this is the default rather than something a card has to ask for.
static constexpr int8_t PAGE_FLOW = -1;

// ---------------------------------------------------------------------------
// One card on a page.
//
// Entities are named by ID and must already be in the registry. The page skips
// - and reports - any spec whose primary is missing, which is what makes one
// fleet-wide dashboard safe: a board that does not have a given entity simply
// does not draw that card rather than failing to build a page.
// ---------------------------------------------------------------------------
struct CardSpec {
    // 1..6 primaries. More than one is the aggregate case - every light in a
    // room behind a single tile, which CardTypes.h explains at length. The
    // FIRST primary decides the card's type.
    const char *primaries[CARD_PRIMARY_MAX] = {nullptr};

    // Battery, last-seen: another entity of the SAME PHYSICAL DEVICE, capped
    // at two by cards.md section 1. Anything wanting a third is a group card.
    const char *secondaries[CARD_SECONDARY_MAX] = {nullptr};

    // The LOCATION, not the measurement - cards.md section 4. The tinted icon
    // already says what is being measured, so "Deck", never "Temperature".
    const char *label = nullptr;

    // nullptr leaves the card with no area, whatever the page's showArea says.
    const char *area = nullptr;

    uint8_t       header  = CARD_HDR_INHERIT;
    CardVariant   variant = CardVariant::VAR_AUTO;
    CardPlacement place   = {};
    TempUnit      tempUnit = TempUnit::TEMP_INHERIT;

    // 0 = use cardLongStaleMs() for this entity's kind.
    uint32_t longStaleMs = 0;

    bool paused = false;
};

// ---------------------------------------------------------------------------
// A page of cards.
// ---------------------------------------------------------------------------
struct PageSpec {
    const char *title = nullptr;

    const CardSpec *cards = nullptr;
    uint8_t         count = 0;

    // The default every card inherits unless it overrides.
    CardHeaderStyle headerDefault = CardHeaderStyle::HDR_TAG;

    // Two independent settings, not one. The owner was explicit: the colour
    // should be available whether or not the area's NAME is displayed.
    bool showArea  = true;
    bool areaColor = true;

    uint8_t  subdivision = PAGE_SUBDIVISION_DEFAULT;
    TempUnit tempUnit    = TempUnit::TEMP_INHERIT;
};

// Author-facing span constants, for the default subdivision of 2.
//
// Spans are in units and a normal card is 2x2, which reads oddly enough in a
// table of literals to be worth naming. A page that changes `subdivision` must
// not use these - it is saying its cells divide differently.
static constexpr uint8_t U_CELL = PAGE_SUBDIVISION_DEFAULT;      // one whole cell
static constexpr uint8_t U_HALF = PAGE_SUBDIVISION_DEFAULT / 2;  // half a cell
static constexpr uint8_t U_2    = U_CELL * 2;
static constexpr uint8_t U_3    = U_CELL * 3;

#endif // PAGE_SPEC_H
