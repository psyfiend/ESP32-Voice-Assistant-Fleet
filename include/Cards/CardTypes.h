#pragma once
#ifndef CARD_TYPES_H
#define CARD_TYPES_H

#include <stdint.h>

// ---------------------------------------------------------------------------
// Card vocabulary. Shaped like EntityTypes.h on purpose: the small enums and
// plain structs the card layer is built out of, with no LVGL and no logic, so
// a build sheet (#20) can name these values without pulling the UI in.
//
// Everything here is a DECISION already recorded in docs/design/cards.md. If a
// value below disagrees with that document, that document wins.
// ---------------------------------------------------------------------------

// How many entities one card may bind.
//
// SIX primaries, not one. docs/design/cards.md section 1 caps a *normal* card
// at one primary plus two secondaries, and that cap is real - but the owner's
// aggregate light (every light in a room, one tap toggles all, and the card
// still looks like an ordinary light card) is N entities behind a 1x1 tile
// that renders ONE derived state. cards.md section 4 already allows it under
// "Light: groupable by room", so it is not a group card and must not need one.
//
// The cost of getting this wrong is asymmetric. Binding one entity in the base
// class and discovering the aggregate case later means touching every subclass
// that already exists; carrying a six-pointer array costs 24 bytes against a
// measured ~715 bytes per card. Same reasoning as issue #15's insistence that
// spans exist from the first version.
static constexpr uint8_t CARD_PRIMARY_MAX   = 6;

// TWO secondaries, and this cap IS the hard one from cards.md section 1: a
// secondary is another entity of the same physical device, which in practice
// means battery and last-seen. Anything wanting a third is asking to be a
// group card.
static constexpr uint8_t CARD_SECONDARY_MAX = 2;

// ---------------------------------------------------------------------------
// What a card is currently saying about itself.
//
// Names are prefixed rather than bare single words. CLAUDE.md records why:
// esp32-hal-gpio.h defines bare ALL-CAPS macros (DISABLED, CHANGE, LOW...) and
// a macro rewrites an enumerator even inside a scoped enum, producing errors
// that point at the framework header instead of at the declaration. A prefix
// costs nothing and is immune.
// ---------------------------------------------------------------------------
enum class CardState : uint8_t {
    ST_LIVE = 0,     // fresh. cards.md section 3: renders nothing extra
    ST_STALE,        // past desc.staleAfterMs - a tag, never a dim
    ST_LONG_STALE,   // past the second threshold - unmistakable
    ST_REFUSED,      // we commanded it and it did not happen. NOT staleness
    ST_PAUSED,       // the user's own choice, so quiet is correct. May dim
};

// Dimming is rejected for staleness and permitted for pause, and cards.md
// section 3 is explicit that the two must look different on purpose. This
// helper is the one place that distinction is encoded.
inline bool cardStateMayDim(CardState s) { return s == CardState::ST_PAUSED; }

// ---------------------------------------------------------------------------
// The card header bar. cards.md section 2 asks for BOTH treatments to be built
// and compared on glass rather than one being chosen on paper, so this is a
// prototype switch, not a permanent setting - once the owner picks, the loser
// comes out.
//
// Either way the layout is fixed: area on the left, STALE on the right.
// ---------------------------------------------------------------------------
enum class CardHeaderStyle : uint8_t {
    HDR_NONE = 0,    // no header. The status row and body carry everything
    HDR_INTERNAL,    // a band inside the card's top edge, edge to edge
    HDR_EXTERNAL,    // a small tag bolted to the top edge, outside the border
};

// ---------------------------------------------------------------------------
// Where a card wants to sit, in grid CELLS.
//
// Issue #15: "MUST carry preferred_span / min_span / priority from the very
// first version. Retrofitting responsive sizing after 8 card types exist means
// rewriting all 8." So these exist now even though milestone 2.4's placement
// only honours the spans - priority is carried and reported, and 2.5's page
// engine is what will actually order and degrade by it.
//
// Deliberately NOT on EntityDescriptor. Entity.h says why: keeping size out of
// the entity is what lets the same temperature reading be a 1x1 tile on one
// page and a 4x2 chart on another.
// ---------------------------------------------------------------------------
struct CardPlacement {
    uint8_t prefSpanX = 1;
    uint8_t prefSpanY = 1;
    uint8_t minSpanX  = 1;   // shrink to this before wrapping to a new row
    uint8_t minSpanY  = 1;
    uint8_t priority  = 128; // higher keeps its preferred size when space runs
                             // short. 128 is the neutral middle of the range,
                             // so a card can be pushed either way without
                             // renumbering everything else
};

const char *cardStateName(CardState s);

#endif // CARD_TYPES_H
