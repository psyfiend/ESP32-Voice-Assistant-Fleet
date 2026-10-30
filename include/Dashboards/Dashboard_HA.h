#pragma once
#ifndef DASHBOARD_HA_H
#define DASHBOARD_HA_H

#include "Cards/PageSpec.h"
#include "ExternalEntities_HA.h"

// ---------------------------------------------------------------------------
// THE REAL DASHBOARD - the owner's 18 Home Assistant entities.
//
// This is docs/design/dashboard-target-7b.md rendered. That document is the
// spec; ExternalEntities_HA.h is the entity half of the transcription and this
// is the layout half. If any of the three disagree, the DOC wins.
//
// PAGE 1 OF THE SWIPE ORDER on every board since 2.6 - House first, then
// Fleet (GUIManager::initPages()). It used to REPLACE the fleet page under
// -D USE_HA_DASHBOARD, because a board could only show one page; that flag is
// gone. Its id stays 2: ids are frozen identities, order is a separate list.
//
//
// PRIORITY IS THE COLUMN THAT MATTERS, AND THE RULE IS THE SAME ONE
// Dashboard_Fleet.h USES: what you cannot see from where you are standing
// outranks what you can.
//
// That puts the garage doors at the top - a door left open is the only thing
// on this page that is expensive to miss - then the outdoor readings, then
// lights, then indoor comfort. The 3248 in portrait shows roughly six cells,
// so this ordering is not academic: it decides what that board actually is.
//
// The 7B shows 18 to 21 cells and fits the whole list with nothing spare,
// which makes this page a genuine test of degradation rather than a
// theoretical one.
// ---------------------------------------------------------------------------

static constexpr uint8_t HA_PRI_DOORS    = 230;   // cannot see them, costly to miss
static constexpr uint8_t HA_PRI_OUTDOOR  = 200;
static constexpr uint8_t HA_PRI_PRESENCE = 170;
static constexpr uint8_t HA_PRI_LIGHT    = 140;
static constexpr uint8_t HA_PRI_INDOOR   = 100;
static constexpr uint8_t HA_PRI_EXTRA    = 60;    // first to go

// --- 2.11: CLUSTERS (DECISIONS K88-K92, K100-K104) ---------------------------
//
// Office, Kitchen and Outside are group cards made on the device, shown as
// clusters: one border, the members inside as mini tiles, a header filled from
// the members (the "2.11 Clusters" mock's House page). The members are the cards
// these areas had before - SAME IDS, so every setting the owner saved on them
// (Desk's tap, the history spans) is still theirs. They wear their cluster's
// group (K92 rule 2), so their own area below only says where they came from.
//
// The header's members (`slots`) are not tiles unless made visible (K104).
// A cluster's priority is its most important member's, so it stands where that
// card stood in the page's order.
inline const CardSpec HA_OUTSIDE[] = {
    { .id        = "outside_porch_261008_0310",
      .primaries = { "outside_front_light" },
      .label     = "Porch",
      .area      = "Outside",
      .place     = { .priority = HA_PRI_LIGHT } },
    // outside_deck_temp is the SAME PHYSICAL SENSOR as the fleet page's
    // deck_temp, reached over the websocket instead of the broker. It arrives
    // already in Fahrenheit because HA converts to the user's display unit,
    // while the broker carries the raw Celsius - and the page's TEMP_F setting
    // converts only when the source says 'C', so both land on the same number.
    // See ExternalEntities_HA.h note 3.
    { .id        = "outside_front_261008_0310",
      .primaries = { "outside_front_temp" },
      .label     = "Front",
      .area      = "Outside",
      .place     = { .priority = HA_PRI_OUTDOOR } },
    { .id        = "outside_deck_261008_0310",
      .primaries = { "outside_deck_temp" },
      .label     = "Deck",
      .area      = "Outside",
      .place     = { .priority = HA_PRI_OUTDOOR } },
    { .id        = "outside_deck_261008_0310b",
      .primaries = { "outside_deck_lux" },
      .label     = "Deck",
      .area      = "Outside",
      .place     = { .priority = HA_PRI_EXTRA } },
};

// HA ships the state-dependent glyph in attributes.icon - measured: the
// occupancy sensors report mdi:motion-sensor-off while clear. Each state has its
// own icon in the header (K101), from our device_class table.
inline const CardSpec HA_KITCHEN[] = {
    { .id        = "kitchen_sink_261008_0310",
      .primaries = { "kitchen_sink" },
      .label     = "Sink",
      .area      = "Kitchen",
      .place     = { .priority = HA_PRI_LIGHT } },
    // kitchen_table is dimmable (['brightness']).
    { .id        = "kitchen_table_261008_0310",
      .primaries = { "kitchen_table" },
      .label     = "Table",
      .area      = "Kitchen",
      .place     = { .priority = HA_PRI_LIGHT } },
    { .id        = "kitchen_261008_0310",
      .primaries = { "kitchen_occupancy" },
      .label     = "Kitchen",
      .area      = "Kitchen",
      .place     = { .priority = HA_PRI_PRESENCE } },
    { .id        = "kitchen_261008_0310b",
      .primaries = { "kitchen_temp" },
      .label     = "Kitchen",
      .area      = "Kitchen",
      .place     = { .priority = HA_PRI_INDOOR } },
};

inline const CardSpec HA_OFFICE[] = {
    // office_desk is light.office: a GROUP defined in Hue, ['color_temp','xy'] -
    // the most demanding card here. As a mini tile its tap toggles all three
    // lamps and its long press opens Desk's own window (K92 rule 3).
    { .id        = "office_desk_261008_0310",
      .primaries = { "office_desk" },
      .label     = "Desk",
      .area      = "Office",
      .place     = { .priority = HA_PRI_LIGHT } },
    { .id        = "office_overhead_261008_0310",
      .primaries = { "office_overhead" },
      .label     = "Overhead",
      .area      = "Office",
      .place     = { .priority = HA_PRI_LIGHT } },
    { .id        = "office_261008_0310",
      .primaries = { "office_occupancy" },
      .label     = "Office",
      .area      = "Office",
      .place     = { .priority = HA_PRI_PRESENCE } },
    { .id        = "office_261008_0310b",
      .primaries = { "office_temp" },
      .label     = "Office",
      .area      = "Office",
      .place     = { .priority = HA_PRI_INDOOR } },
};

// --- Garage: the doors ------------------------------------------------------
//
// These two report device_class garage_door. Since 2.11 round 4 they are the
// members of a SUMMARY (K98): one card, open while either door is, its words
// naming the open one ("North", "South", "Both"). Same ids as the two cards
// they were, so what was saved on them is still theirs.
inline const CardSpec HA_GARAGE_DOORS[] = {
    { .id        = "garage_north_261008_0310",
      .primaries = { "garage_door_north" },
      .label     = "North",
      .area      = "Garage",
      .place     = { .priority = HA_PRI_DOORS } },
    { .id        = "garage_south_261008_0310",
      .primaries = { "garage_door_south" },
      .label     = "South",
      .area      = "Garage",
      .place     = { .priority = HA_PRI_DOORS } },
};

inline const CardSpec HA_CARDS[] = {
    // --- Garage: the doors, first - the priority is about what matters -------
    // Id made 2026-10-19 (K33).
    { .id          = "garage_doors_261019_1050",
      .label       = "Doors",
      .area        = "Garage",
      .place       = { .priority = HA_PRI_DOORS },
      .members     = HA_GARAGE_DOORS,
      .memberCount = (uint8_t)(sizeof(HA_GARAGE_DOORS) / sizeof(HA_GARAGE_DOORS[0])),
      .present     = GroupPresent::PRESENT_SUMMARY },

    // --- The clusters (2.11) - Outside, then the two rooms with presence ----
    // Ids made 2026-10-19 (K33); the members are above, with the ids they had.
    { .id          = "outside_261019_0500",
      .label       = "Outside",
      .area        = "Outside",
      .place       = { .priority = HA_PRI_OUTDOOR },
      .members     = HA_OUTSIDE,
      .memberCount = (uint8_t)(sizeof(HA_OUTSIDE) / sizeof(HA_OUTSIDE[0])),
      .present     = GroupPresent::PRESENT_CLUSTER,
      .slots       = { "outside_deck_261008_0310", "outside_deck_261008_0310b" } },
    { .id          = "kitchen_261019_0500",
      .label       = "Kitchen",
      .area        = "Kitchen",
      .place       = { .priority = HA_PRI_PRESENCE },
      .members     = HA_KITCHEN,
      .memberCount = (uint8_t)(sizeof(HA_KITCHEN) / sizeof(HA_KITCHEN[0])),
      .present     = GroupPresent::PRESENT_CLUSTER,
      .slots       = { "kitchen_261008_0310b", "kitchen_261008_0310" } },
    { .id          = "office_261019_0500",
      .label       = "Office",
      .area        = "Office",
      .place       = { .priority = HA_PRI_PRESENCE },
      .members     = HA_OFFICE,
      .memberCount = (uint8_t)(sizeof(HA_OFFICE) / sizeof(HA_OFFICE[0])),
      .present     = GroupPresent::PRESENT_CLUSTER,
      .slots       = { "office_261008_0310b", "office_261008_0310" } },

    // --- Lights -----------------------------------------------------------
    //
    // living_overhead is a switch.* entity the owner uses as a light - the card
    // calls it a light, and #44 must call switch.turn_on for it. The domain
    // comes from the entity id, not from this file.
    { .id        = "living_overhead_261008_0310",
      .primaries = { "living_overhead" },
      .label     = "Overhead",
      .area      = "Living",
      .place     = { .priority = HA_PRI_LIGHT } },

    // --- Indoor comfort ---------------------------------------------------
    { .id        = "front_thermo_261008_0310",
      .primaries = { "front_thermo" },
      .label     = "Thermo",
      .area      = "Front",
      .place     = { .priority = HA_PRI_INDOOR } },
    { .id        = "bedroom_eric_261008_0310",
      .primaries = { "bedroom_temp" },
      .label     = "Eric",
      .area      = "Bedroom",
      .place     = { .priority = HA_PRI_INDOOR } },
    { .id        = "garage_261008_0310",
      .primaries = { "garage_temp" },
      .label     = "Garage",
      .area      = "Garage",
      .place     = { .priority = HA_PRI_EXTRA } },
};

inline const PageSpec HA_PAGE = {
    // Page 2. Ids are append-only and 1 is home forever - see PageSpec. When
    // 2.6 lands and both pages can coexist, this id is already correct.
    .id    = 2,
    .slug  = "house",
    .title = "House",
    .cards = HA_CARDS,
    .count = (uint8_t)(sizeof(HA_CARDS) / sizeof(HA_CARDS[0])),

    .headerDefault = CardHeaderStyle::HDR_TAG_FLOAT,   // owner 2026-10-01
    .showArea      = true,
    .areaColor     = true,

    .subdivision = PAGE_SUBDIVISION_DEFAULT,

    // Fahrenheit, matching the fleet page and the owner's instance. Entities
    // that arrive in Celsius are converted; those already in Fahrenheit are
    // left alone, which is what makes the two deck readings agree.
    .tempUnit = TempUnit::TEMP_F,
};

#endif // DASHBOARD_HA_H
