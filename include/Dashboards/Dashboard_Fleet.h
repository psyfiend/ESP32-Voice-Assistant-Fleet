#pragma once
#ifndef DASHBOARD_FLEET_H
#define DASHBOARD_FLEET_H

#include "Cards/PageSpec.h"
#include "SystemEntities.h"
#include "ExternalEntities.h"
#include "VirtualEntities.h"

// ---------------------------------------------------------------------------
// THE DASHBOARD. One page, fleet-wide, rendered on every board.
//
// This is the file that turns the card library into a device you can use. It
// is deliberately DATA rather than code: CardDemo.cpp builds its page by
// calling methods, and everything it does that way would have to be redesigned
// when the build sheet lands. Everything here is a struct the build sheet's
// JSON loader (#20, milestone 3.3) will fill in instead, so the loader becomes
// a parser rather than a second design.
//
// ONE DEFINITION FOR THE WHOLE FLEET, and that is the owner's call (2026-09-15:
// "Fleet-wide is fine for now but per-board is definitely important later").
// It works because nothing here is board-specific:
//
//   - the GRID is derived from the panel, not declared. WS_P4_7B gets 7 cells
//     across and CYD_S3_3248 portrait gets 2, from the same table.
//   - cards that do not fit are dropped BY PRIORITY, so the small boards keep
//     the things worth keeping instead of whatever was declared first.
//   - a spec whose entity is not in this board's registry is skipped and
//     reported, never fatal.
//
// So the 3248 renders a true subset of the 7B rather than a different page,
// and flashing it is a free check on the whole degradation scheme.
//
// WHEN PER-BOARD ARRIVES it wants to be another PageSpec selected by the board
// identity macro every BSP header already defines - the same mechanism
// ConnectivityDefaults.h uses for per-board overrides, and no new machinery.
//
// ---------------------------------------------------------------------------
// WHAT IS ON IT TODAY, AND WHY IT IS NOT YOUR HOUSE
//
// Eleven entities exist on this device: four Zigbee2MQTT values off one deck
// sensor, four pieces of this board's own telemetry, and the virtual switches
// that exist so the command path has something to command. That is the whole
// supply until Home Assistant arrives over the websocket (#43), which is the
// next milestone and the one that makes this list real.
//
// The owner's actual dashboard - office and kitchen lights, temperature and
// occupancy, the living room lights, the indoor temperatures, and the front
// door, deck and garage - is roughly 18 cards and every one of them needs #43.
// Until then this page is the same structure with the entities we have, which
// is what makes it worth booting into rather than a placeholder.
// ---------------------------------------------------------------------------

// PRIORITY IS THE INTERESTING COLUMN HERE.
//
// It is what a small board reads when it runs out of room, and until 2.5
// nothing read it at all. The ordering below says: the things outside that you
// cannot see from indoors matter most, the lights matter next, and the panel's
// own diagnostics are the first things to go - they are on the System page
// anyway, and a 2-column portrait board showing its own free heap instead of
// the temperature outside would be exactly backwards.
static constexpr uint8_t PRI_CRITICAL = 220;   // survives on the smallest board
static constexpr uint8_t PRI_NORMAL   = 160;
static constexpr uint8_t PRI_NICE     = 100;
static constexpr uint8_t PRI_DEBUG    = 40;    // first to be dropped

inline const CardSpec FLEET_CARDS[] = {
    // --- Outdoors ---------------------------------------------------------
    //
    // The deck sensor publishes temperature, illuminance, occupancy and its
    // own battery on ONE Zigbee2MQTT topic, so these four cards are four
    // entities of one physical device - which is exactly the case
    // CardSpec::secondaries exists for. The battery rides along on the
    // temperature card rather than taking a cell of its own.
    {
        .primaries   = { "deck_temp" },
        .secondaries = { "deck_battery" },
        .label       = "Deck",          // the LOCATION. The tinted icon
        .area        = "Outdoor",       // already says it is a temperature
        .place       = { .priority = PRI_CRITICAL },
    },
    {
        .primaries = { "deck_motion" },
        .label     = "Deck",
        .area      = "Outdoor",
        .place     = { .priority = PRI_NORMAL },
    },
    {
        .primaries = { "deck_lux" },
        .label     = "Deck",
        .area      = "Outdoor",
        .place     = { .priority = PRI_NICE },
    },

    // --- Lights -----------------------------------------------------------
    //
    // The aggregate: ONE card, four entities, one tap for all of them. Not a
    // group card - cards.md section 4's "groupable by room" - and it is the
    // case CARD_PRIMARY_MAX exists for. It earns a double-width cell because
    // it stands for four things.
    {
        .primaries = { VIRT_ENT_L1, VIRT_ENT_L2, VIRT_ENT_L3, VIRT_ENT_L4 },
        .label     = "All Lamps",
        .area      = "Kitchen",
        // One whole cell, 2026-09-19. It spanned U_2 (two cells) as the group
        // card's showcase; the owner would rather have the cell back. Dropping
        // it takes the page from 15 cells to 14, which is the difference
        // between a card surviving and not on the 3-column boards.
        .place     = { .priority = PRI_NORMAL },
    },
    // Four plain lamps, one cell each.
    //
    // Lamp 1 and Lamp 2 carried 3-unit spans (one and a half cells) through
    // 2.5 as the only rendering of Q3b's sub-grid that had ever existed.
    // Removed 2026-09-18 at the owner's request: the point was proved on three
    // boards and a shipped page should not keep wearing its test fixture.
    //
    // The mechanism is untouched. A unit is still half a cell, `prefSpanX` and
    // `minSpanX` are still ordinary fields, and "All Lamps" above still spans
    // U_2 - so the sub-grid remains exercised by the group card on every boot.
    { .primaries = { VIRT_ENT_L1 }, .label = "Lamp 1", .area = "Kitchen",
      .place = { .priority = PRI_NICE } },
    { .primaries = { VIRT_ENT_L2 }, .label = "Lamp 2", .area = "Kitchen",
      .place = { .priority = PRI_NICE } },
    { .primaries = { VIRT_ENT_L3 }, .label = "Lamp 3", .area = "Lounge",
      .place = { .priority = PRI_NICE } },
    // Lamp 4's CARD is gone (owner, 2026-09-19) - 13 cards to 12, and with
    // All Lamps narrowed, 15 cells to 13.
    //
    // VIRT_ENT_L4 itself deliberately stays. "All Lamps" above still
    // aggregates all four, so the group card keeps showing a child that has no
    // card of its own - which is the normal case for a real group and is worth
    // having on screen rather than hiding.

    // --- The two test switches -------------------------------------------
    //
    // TEMPORARY, and they leave with #44. One echoes its command and one is
    // deliberately ignored, which is the only way the optimistic-write revert
    // and the FAILED treatment can be seen on a real board - nothing else in
    // the fleet is writable yet.
    // The #16 acceptance instrumentation is GONE, 2026-09-18, at the owner's
    // request now that the milestone is signed off.
    //
    // These two used to be pinned - one to a valid unit coordinate and one
    // deliberately out of bounds - to prove that explicit placement worked and
    // that an invalid pin was reported and then flowed rather than dropped.
    // Both were observed on hardware; the labels were literally "Obeys" and
    // "Ignores". Lamp 1 and Lamp 2 carried 3-unit spans for the same reason:
    // to make the sub-grid visible at all.
    //
    // The demonstration is done, and a shipped page should look like a
    // dashboard rather than like a test fixture. The code paths they exercised
    // are unchanged and still reachable - `col`/`row` and `prefSpanX` remain
    // ordinary fields any page may set - so nothing was removed but the
    // evidence. The two switches stay because they are the only writable
    // entities on the device, and they leave with #44.
    { .primaries = { VIRT_ENT_SWITCH }, .label = "Switch", .area = "Office",
      .place = { .priority = PRI_NICE } },
    { .primaries = { VIRT_ENT_STUCK },  .label = "Stuck",  .area = "Office",
      .place = { .priority = PRI_NICE } },

    // --- This panel -------------------------------------------------------
    //
    // Lowest priority on the page, on purpose. These are the cards a small
    // board should lose first, and dropping them is how the degradation gets
    // exercised on hardware rather than argued about.
    {
        .primaries = { SYS_ENT_RSSI },
        .label     = "Signal",
        .area      = "Panel",
        .place     = { .priority = PRI_DEBUG },
    },
    // Free Heap and Uptime are THREE UNITS each - one and a half cells.
    //
    // Two reasons, and the second is the one that will outlive the first.
    //
    // Packing: they used to be U_2 (two cells) and U_CELL (one), three cells
    // between them. Three units each is still three cells, so the page total
    // does not move - the width is redistributed rather than spent.
    //
    // Width: Uptime is about to format HH:MM:SS (#51), and six digits plus two
    // colons do not fit the VALUE face in a single cell on any board in the
    // fleet - least of all the 4B pair, which are the most cramped and are
    // exactly where Uptime becomes visible for the first time.
    //
    // minSpanX lets both fall back to a whole cell where three units will not
    // fit. That matters on CYD_S3_3248, whose rows are only four units wide:
    // a 3-unit card there would strand a unit beside it every time.
    {
        .primaries = { SYS_ENT_HEAP },
        .label     = "Free Heap",
        .area      = "Panel",
        .place     = { .prefSpanX = 3, .minSpanX = U_CELL,
                       .priority  = PRI_DEBUG },
    },
    {
        .primaries = { SYS_ENT_UPTIME },
        .label     = "Uptime",
        .area      = "Panel",
        // ABOVE Free Heap, so Free Heap is the panel card that goes first.
        //
        // All three panel cards sat at PRI_DEBUG, and ties break toward the
        // LATER declaration - so Uptime, declared last, was always the first
        // card dropped on the whole page. It has never once been visible on
        // either 4B board. This is the smallest change that fixes that, and it
        // is the mechanism working as designed rather than a workaround:
        // priority is exactly how you say "this one matters more".
        .place     = { .prefSpanX = 3, .minSpanX = U_CELL,
                       .priority  = PRI_DEBUG + 10 },
    },
};

inline const PageSpec FLEET_PAGE = {
    // Page 1 is home, and 1 stays home forever. See PageSpec's comment: ids
    // are append-only and slugs are the external key.
    .id    = 1,
    .slug  = "home",
    .title = "Home",
    .cards = FLEET_CARDS,
    .count = (uint8_t)(sizeof(FLEET_CARDS) / sizeof(FLEET_CARDS[0])),

    // Two of the three header modes are still open questions the owner has to
    // answer on glass (HANDOFF: "which header mode is the default" is "what a
    // card looks like when nobody chose", not an elimination). HDR_TAG is what
    // the 2.4 bench defaulted to and the only mode that costs the card body
    // nothing, so it is the one to be talked out of rather than into.
    .headerDefault = CardHeaderStyle::HDR_BAR,   // chosen on glass 2026-09-17
    .showArea      = true,
    .areaColor     = true,

    .subdivision = PAGE_SUBDIVISION_DEFAULT,

    // Fahrenheit, fleet-wide, converting anything that arrives in Celsius.
    // The owner's standing preference, and the deck sensor reports Celsius -
    // so this page is its own test of the conversion.
    .tempUnit = TempUnit::TEMP_F,
};

#endif // DASHBOARD_FLEET_H
