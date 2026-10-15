#pragma once
#ifndef DASHBOARD_CLOCK_H
#define DASHBOARD_CLOCK_H

#include "Cards/PageSpec.h"

// ---------------------------------------------------------------------------
// THE CLOCK PAGE (#100, a prototype): NINA's Classic clock face as a card two
// cells by two (four units each way), pinned at the top left of a page of its
// own, after House and Fleet - so nothing on those pages moves. Where it
// belongs is 2.11's hero region (docs/design/layout-engines.md), which will
// place a square card properly; this page is the flow engine doing what it
// can today.
//
// The card has no entity: the spec asks for it by name (CardSpec::type).
// ---------------------------------------------------------------------------
inline const CardSpec CLOCK_CARDS[] = {
    {
        // No label and no area: HDR_NONE would write them in the card's top
        // strip, over the face - the face is the whole card.
        .id     = "clock_classic_261014_1230",
        .header = (uint8_t)CardHeaderStyle::HDR_NONE,
        .place  = { .prefSpanX = U_2, .prefSpanY = U_2, .minSpanX = U_2, .minSpanY = U_2,
                    .col = 0, .row = 0 },
        .type   = CardSpecType::SPEC_CLOCK,
    },
};

inline const PageSpec CLOCK_PAGE = {
    .id    = 3,          // append-only (PageSpec's comment): the next free one
    .slug  = "clock",
    .title = "Clock",
    .cards = CLOCK_CARDS,
    .count = (uint8_t)(sizeof(CLOCK_CARDS) / sizeof(CLOCK_CARDS[0])),
    .headerDefault = CardHeaderStyle::HDR_NONE,
    .showArea      = false,
    .areaColor     = true,
    .subdivision   = PAGE_SUBDIVISION_DEFAULT,
};

#endif // DASHBOARD_CLOCK_H
