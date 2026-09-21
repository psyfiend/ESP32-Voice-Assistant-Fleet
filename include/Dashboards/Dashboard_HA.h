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
// Selected with -D USE_HA_DASHBOARD. Off by default, and deliberately so: it
// REPLACES the fleet page rather than joining it, because until 2.6's
// horizontal swipes land a board can only show one page, and 18 + 12 cards do
// not fit anywhere. Enabled on CYD_S3_3248W535 only, which is the board being
// used to test #43.
//
// When 2.6 arrives this stops being an either/or and becomes page 2.
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

inline const CardSpec HA_CARDS[] = {
    // --- Garage: the doors ------------------------------------------------
    //
    // These two report device_class garage_door and there is NO door card type
    // yet, so today they render as generic binary sensors reading on/off.
    // dashboard-target-7b.md point 3; 2.7 owes the type. Listed first anyway
    // because the priority is about what matters, not about what renders well.
    { .primaries = { "garage_door_north" },
      .label     = "North",
      .area      = "Garage",
      .place     = { .priority = HA_PRI_DOORS } },
    { .primaries = { "garage_door_south" },
      .label     = "South",
      .area      = "Garage",
      .place     = { .priority = HA_PRI_DOORS } },

    // --- Outside ----------------------------------------------------------
    //
    // outside_deck_temp is the SAME PHYSICAL SENSOR as the fleet page's
    // deck_temp, reached over the websocket instead of the broker. It arrives
    // already in Fahrenheit because HA converts to the user's display unit,
    // while the broker carries the raw Celsius - and the page's TEMP_F setting
    // converts only when the source says 'C', so both land on the same number.
    // See ExternalEntities_HA.h note 3.
    { .primaries = { "outside_deck_temp" },
      .label     = "Deck",
      .area      = "Outside",
      .place     = { .priority = HA_PRI_OUTDOOR } },
    { .primaries = { "outside_front_temp" },
      .label     = "Front",
      .area      = "Outside",
      .place     = { .priority = HA_PRI_OUTDOOR } },
    { .primaries = { "outside_front_light" },
      .label     = "Porch",
      .area      = "Outside",
      .place     = { .priority = HA_PRI_LIGHT } },
    { .primaries = { "outside_deck_lux" },
      .label     = "Deck",
      .area      = "Outside",
      .place     = { .priority = HA_PRI_EXTRA } },

    // --- Presence ---------------------------------------------------------
    //
    // HA ships the state-dependent glyph in attributes.icon - measured: these
    // report mdi:motion-sensor-off while clear. Nothing reads it yet, and the
    // glyph is not in the generated icon subset, so wiring it up without
    // regenerating the fonts would render tofu. Both are 2.7.
    { .primaries = { "kitchen_occupancy" },
      .label     = "Kitchen",
      .area      = "Kitchen",
      .place     = { .priority = HA_PRI_PRESENCE } },
    { .primaries = { "office_occupancy" },
      .label     = "Office",
      .area      = "Office",
      .place     = { .priority = HA_PRI_PRESENCE } },

    // --- Lights -----------------------------------------------------------
    //
    // office_desk is light.office: a GROUP, ['color_temp','xy']. It is the most
    // demanding card here and the one that will force LightCard to stop being
    // a SwitchCard. kitchen_table is dimmable too (['brightness']). Both render
    // as plain on/off today.
    //
    // living_overhead is a switch.* entity the owner uses as a light - the card
    // calls it a light, and #44 must call switch.turn_on for it. The domain
    // comes from the entity id, not from this file.
    { .primaries = { "office_desk" },
      .label     = "Desk",
      .area      = "Office",
      .place     = { .priority = HA_PRI_LIGHT } },
    { .primaries = { "office_overhead" },
      .label     = "Overhead",
      .area      = "Office",
      .place     = { .priority = HA_PRI_LIGHT } },
    { .primaries = { "kitchen_sink" },
      .label     = "Sink",
      .area      = "Kitchen",
      .place     = { .priority = HA_PRI_LIGHT } },
    { .primaries = { "kitchen_table" },
      .label     = "Table",
      .area      = "Kitchen",
      .place     = { .priority = HA_PRI_LIGHT } },
    { .primaries = { "living_overhead" },
      .label     = "Overhead",
      .area      = "Living",
      .place     = { .priority = HA_PRI_LIGHT } },

    // --- Indoor comfort ---------------------------------------------------
    { .primaries = { "front_thermo" },
      .label     = "Thermo",
      .area      = "Front",
      .place     = { .priority = HA_PRI_INDOOR } },
    { .primaries = { "kitchen_temp" },
      .label     = "Kitchen",
      .area      = "Kitchen",
      .place     = { .priority = HA_PRI_INDOOR } },
    { .primaries = { "office_temp" },
      .label     = "Office",
      .area      = "Office",
      .place     = { .priority = HA_PRI_INDOOR } },
    { .primaries = { "bedroom_temp" },
      .label     = "Eric",
      .area      = "Bedroom",
      .place     = { .priority = HA_PRI_INDOOR } },
    { .primaries = { "garage_temp" },
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

    .headerDefault = CardHeaderStyle::HDR_BAR,
    .showArea      = true,
    .areaColor     = true,

    .subdivision = PAGE_SUBDIVISION_DEFAULT,

    // Fahrenheit, matching the fleet page and the owner's instance. Entities
    // that arrive in Celsius are converted; those already in Fahrenheit are
    // left alone, which is what makes the two deck readings agree.
    .tempUnit = TempUnit::TEMP_F,
};

#endif // DASHBOARD_HA_H
