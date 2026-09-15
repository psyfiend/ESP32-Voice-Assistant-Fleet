#pragma once
#ifndef CARD_TYPES_H
#define CARD_TYPES_H

#include <stdint.h>
#include <Arduino.h>

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
    ST_PARTIAL,      // SOME of what we commanded did not happen - see below
    ST_PAUSED,       // the user's own choice, so quiet is correct. May dim
};

// ST_PARTIAL exists because of the owner's rule for cards that command several
// entities at once, and it is a genuinely different thing from ST_REFUSED:
//
//   "unless every single switch has failed then the card should continue to
//    reflect the state of its children (on or off) but show a simple warning
//    tag in the header. If every single child has failed, then the parent card
//    should say failed."
//
// So a partial failure must NOT take over the card's body - the body is still
// telling the truth about what the lights are doing, and replacing it with a
// failure treatment would throw away correct information to report a partial
// one. Only the tag changes. That is what stateColor() returning 0 for this
// state encodes, and why the tag colour is asked for separately.
inline bool cardStateOwnsBody(CardState s) {
    return s == CardState::ST_REFUSED ||
           s == CardState::ST_STALE   ||
           s == CardState::ST_LONG_STALE;
}

// Dimming is rejected for staleness and permitted for pause, and cards.md
// section 3 is explicit that the two must look different on purpose. This
// helper is the one place that distinction is encoded.
inline bool cardStateMayDim(CardState s) { return s == CardState::ST_PAUSED; }

// ---------------------------------------------------------------------------
// How a card presents its top section.
//
// THESE ARE THREE PERMANENT CHOICES, not a comparison to be narrowed later.
// The owner's call: all three stay, and a build sheet picks one with a single
// setting. They were briefly treated as a prototype switch with a winner to be
// declared - that was wrong, and it would have deleted two of them.
//
// Every card has a TOP SECTION regardless. It carries two things and always in
// the same places: the area (or a custom grouping, later) on the left, and the
// STALE marker on the right. What changes between these three is only how that
// section is PRESENTED:
//
//   HDR_BAR    a filled band inside the card, edge to edge
//   HDR_TAG    a pill hanging OUTSIDE, above the card's top-left corner, with
//              a second pill on the right when the card needs a STALE marker
//   HDR_NONE   plain text in the top strip inside the card, no fill
//
// HDR_TAG is the only one that costs the card nothing: the body gets the whole
// surface, because the tag is not on it. The other two reserve a strip. That
// difference is real and is the reason to pick one.
//
// The room a tag needs comes out of the grid's ROW GAP, not out of the card -
// see CardPage::begin(). Cards are the same size and shape in all three modes,
// which the owner was explicit about: "in order for this to look good the
// cards must not differ in shape or size because of the tag."
// ---------------------------------------------------------------------------
enum class CardHeaderStyle : uint8_t {
    HDR_NONE = 0,   // plain text in the card's top strip
    HDR_BAR,        // a filled band inside the card, edge to edge
    HDR_TAG,        // pills hanging outside, above the card
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

// ---------------------------------------------------------------------------
// How much of itself a card draws.
//
// DERIVED from the cell it was given, not declared - the same rule the grid and
// the type scale already follow. A card asked to live in a small cell shows
// less rather than overflowing or shrinking its text below the readable size
// the type scale worked out.
//
// What each layout drops is the thing cards.md already calls optional:
//
//   ValueCard   the status corners. Section 1: "if the entity has neither
//               battery nor last-seen, the row is simply absent - not an empty
//               reserved strip." Compact treats a cell too small to hold it the
//               same way it treats an entity with nothing to put in it.
//   StateCard   the name. Section 4 is explicit that on an actor card "state is
//               the icon and its colour" - so the icon is the part that cannot
//               go, and a very small one is a disc and nothing else.
//
// VAR_AUTO is the default and resolves at build time by measuring. A build
// sheet can pin either variant when it disagrees.
// ---------------------------------------------------------------------------
enum class CardVariant : uint8_t {
    VAR_AUTO = 0,
    VAR_FULL,
    VAR_COMPACT,
};

// Debug output, gated and tagged the way every other area in this project does
// it - see DBG_WIFI in ConnectivityManager and DBG_MQTT in MqttManager. Enabled
// per build from the shared [S3-options] / [P4-options] sections of
// platformio.ini rather than per board.
#ifdef DEBUG_CARDS
    #define DBG_CARDS(...) Serial.printf("[Cards:debug] " __VA_ARGS__)
#else
    #define DBG_CARDS(...)
#endif

const char *cardStateName(CardState s);
const char *cardVariantName(CardVariant v);

#endif // CARD_TYPES_H
