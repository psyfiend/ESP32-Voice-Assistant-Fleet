#pragma once
#ifndef CARD_TYPES_H
#define CARD_TYPES_H

#include <stdint.h>
#include <Arduino.h>

// ---------------------------------------------------------------------------
// Card vocabulary. Shaped like EntityTypes.h on purpose: the small enums and
// plain structs the card layer is built out of, with no LVGL and no logic, so
// page specs and the settings file can name these values without pulling the
// UI in.
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
// Carrying a six-pointer array costs 24 bytes against a measured ~715 bytes
// per card; binding one entity in the base class would mean touching every
// subclass to add the aggregate case.
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

    // THE SOURCE SAID SO.
    //
    // Distinct from ST_STALE: stale means "we have not heard from this in a
    // while and are guessing", while this means "the thing that owns it TOLD us
    // it is not there". One is an inference from silence, the other is a fact,
    // and a panel that renders them identically throws away the better signal.
    //
    // It matters most on the HA path, where silence carries no information at
    // all: subscribe_trigger fires on change, so a quiet sensor and a dead one
    // look the same by age. HA sends the word "unavailable" as a state, and
    // that word is the only reliable evidence available. Without a state for it
    // the card would keep displaying its last good reading for ever.
    ST_UNAVAILABLE,
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
           s == CardState::ST_LONG_STALE ||
           // UNAVAILABLE MUST TAKE THE BODY, not merely wear a tag.
           //
           // stateColor() returns early for anything this predicate rejects,
           // so a state with a colour but no entry here would keep displaying
           // its last good reading under a small "N/A" badge - the lie the
           // state exists to remove, with a label on it.
           //
           // The body is the thing the user reads at a glance from across the
           // room. If the source says the sensor is gone, the number has to go
           // with it.
           s == CardState::ST_UNAVAILABLE;
}

// Dimming is rejected for staleness and permitted for pause, and cards.md
// section 3 is explicit that the two must look different on purpose. This
// helper is the one place that distinction is encoded.
inline bool cardStateMayDim(CardState s) { return s == CardState::ST_PAUSED; }

// ---------------------------------------------------------------------------
// How a card presents its top section.
//
// THERE ARE FOUR PERMANENT CHOICES (below), each picked with a single setting;
// none is a prototype with a winner to be declared.
//
// Every card has a TOP SECTION regardless. It carries two things and always in
// the same places: the card label (its group) on the left, and the STALE
// marker on the right. What changes between these three is only how that
// section is PRESENTED:
//
//   HDR_BAND   a filled band inside the card, edge to edge (card-sheet 11.3;
//              "band" because "bar" reads like the system header bar)
//   HDR_TAG    a pill hanging OUTSIDE, above the card's top-left corner, with
//              a second pill on the right when the card needs a STALE marker
//   HDR_NONE   plain text in the top strip inside the card, no fill
//
// HDR_TAG is the only one that costs the card nothing: the body gets the whole
// surface, because the tag is not on it. The other two reserve a strip. That
// difference is real and is the reason to pick one.
//
// The room a tag needs comes out of the grid's ROW GAP, not out of the card -
// see CardPage::begin(). Cards are the same size and shape in all the modes:
// they must not differ in shape or size because of the tag.
// ---------------------------------------------------------------------------
//
// HDR_TAG_FLOAT: the same pills, but lying ACROSS the card's top corners -
// half above the card's top edge and sticking out the same amount past its
// side - so a tag reads as resting on the card and, on Linen, casts its shadow
// onto it. It is the default. It costs the card exactly what HDR_TAG does (one
// headerHeight()), split between the row above the card and an inset at the
// top of the body that keeps the corner icon out from under the pill.
enum class CardHeaderStyle : uint8_t {
    HDR_NONE = 0,   // plain text in the card's top strip
    HDR_BAND,       // a filled band inside the card, edge to edge
    HDR_TAG,        // pills hanging outside, above the card
    HDR_TAG_FLOAT,  // pills lying across the card's top corners
};

// Either tag mode. Most questions - does this card have pills, does the cell
// pay for a tag - have the same answer for both.
inline bool cardHeaderIsTag(CardHeaderStyle s) {
    return s == CardHeaderStyle::HDR_TAG || s == CardHeaderStyle::HDR_TAG_FLOAT;
}

// For logs and the System Doctor.
inline const char *cardHeaderName(CardHeaderStyle s) {
    switch (s) {
        case CardHeaderStyle::HDR_TAG_FLOAT: return "floating tag";
        case CardHeaderStyle::HDR_TAG:       return "tag";
        case CardHeaderStyle::HDR_BAND:      return "band";
        case CardHeaderStyle::HDR_NONE:      return "none";
    }
    return "?";
}

// Which unit a temperature is DISPLAYED in, whatever the source reports.
//
// The owner prefers Fahrenheit, with both supported and customizable when the
// source is not in the unit wanted. His Home Assistant has sensors in both, so
// a fleet with one preference and no conversion would render the odd one wrong,
// silently, and look exactly like a working card.
//
// Conversion is a DISPLAY concern and happens at format time. The registry
// keeps what the source actually said, because that value is what an echo is
// compared against, what an optimistic write reverts to, and what our own HA
// discovery would publish. Converting on the way in would corrupt all three.
enum class TempUnit : uint8_t {
    TEMP_INHERIT = 0,   // use the page's setting (on a card) or the fleet's
    TEMP_SOURCE,        // show whatever the entity reports, unconverted
    TEMP_C,
    TEMP_F,
};

// ---------------------------------------------------------------------------
// How a DURATION is rendered.
//
// Deliberately the same shape as TempUnit above, resolved the same way -
// fleet default, then page, then card - because it is the same problem:
// a value whose stored form and displayed form differ. Uptime is stored as a
// plain count of SECONDS (SystemProvider writes nowMs / 1000) and nobody wants
// to read 598 and work out that it means ten minutes.
//
// One grammar for "display differs from storage" rather than two. The registry
// still keeps exactly what the source said, for the same three reasons the
// temperature note gives above.
//
// Compound names per the naming hazard in CLAUDE.md - DUR_ rather than bare
// AUTO/FIXED/RAW, any of which is a plausible future Arduino macro.
enum class DurationFormat : uint8_t {
    DUR_INHERIT = 0,  // use the page's setting (on a card) or the fleet's
    // Switches by magnitude: "09:12" under an hour, "04:15:33" under a day,
    // "3d 04:15" beyond. The default - it is what a person says out loud, and
    // it keeps the card narrow while a board is young without lying about a
    // board that has been up for weeks.
    DUR_AUTO,
    // Always H:MM:SS, hours growing past two digits rather than wrapping.
    // Honest, and wide - three weeks reads "504:00:00".
    DUR_CLOCK,
    // The raw count with its source unit, exactly as it arrives.
    DUR_SECONDS,
};

// ---------------------------------------------------------------------------
// When a card standing for several things is ON. A group defined on the
// device behaves as an HA group does, and
// HA's group helper has exactly this choice - "The group's entity state is only
// ON if all members are on", off by default. A tap follows the same answer: a
// group that is on turns every member off; one that is off turns them all on.
// Native HA groups (light.office) are ONE entity and HA decides for them.
enum class GroupOn : uint8_t {
    GROUP_ON_ANY = 0,   // on when any member is on - HA's default
    GROUP_ON_ALL,       // on only when every member is on
};

// Which of a light's scenes its window offers: the ones not hidden in HA's own
// UI (its entity registry's hidden_by), all of them, or none. A card setting,
// saved with the card.
enum class SceneShow : uint8_t {
    SCENES_VISIBLE = 0,   // the default
    SCENES_ALL,
    SCENES_OFF,
};

// WHAT A TAP DOES (DECISIONS K37, K39). A long press always opens the window
// (K3); this is the tap. TAP_DEFAULT is the card type's own - toggle on a
// switch, a light or a button, History on anything else - and is what an
// unsaved card does. The scenes are the ones the card's window offers (its
// Scenes setting); Cycle starts from the first after each boot.
enum class TapAction : uint8_t {
    TAP_DEFAULT = 0,
    TAP_TOGGLE,
    TAP_DETAILS,        // opens the window, as a long press does
    TAP_MEMBERS,        // opens it on Members
    TAP_HISTORY,        // opens it on History
    TAP_CYCLE_SCENES,   // loads the next scene, with a toast naming it
    TAP_LOAD_SCENE,     // loads one chosen scene ("tap_scene"), with a toast
    TAP_NOTHING,
    TAP_GROUP,          // a cluster's group view (K154) - its default; saved by name, so last
};

inline const char *tapActionName(TapAction t) {
    switch (t) {
        case TapAction::TAP_TOGGLE:       return "toggle";
        case TapAction::TAP_DETAILS:      return "details";
        case TapAction::TAP_MEMBERS:      return "members";
        case TapAction::TAP_HISTORY:      return "history";
        case TapAction::TAP_CYCLE_SCENES: return "cycle_scenes";
        case TapAction::TAP_LOAD_SCENE:   return "load_scene";
        case TapAction::TAP_NOTHING:      return "nothing";
        case TapAction::TAP_GROUP:        return "group";
        default:                          return "default";
    }
}
inline bool tapActionFromName(const char *n, TapAction &out) {
    if (!n) return false;
    for (uint8_t i = 1; i <= (uint8_t)TapAction::TAP_GROUP; i++)
        if (!strcmp(n, tapActionName((TapAction)i))) { out = (TapAction)i; return true; }
    return false;
}

// The names these are saved under (Settings.h): a saved choice is a
// NAME, so reordering an enum never changes what it means. A name this
// firmware does not know reads as "not set".
inline const char *tempUnitName(TempUnit u) {
    switch (u) {
        case TempUnit::TEMP_SOURCE: return "source";
        case TempUnit::TEMP_C:      return "c";
        case TempUnit::TEMP_F:      return "f";
        default:                    return "inherit";
    }
}
inline bool tempUnitFromName(const char *n, TempUnit &out) {
    if (!n) return false;
    for (uint8_t i = 1; i <= (uint8_t)TempUnit::TEMP_F; i++)
        if (!strcmp(n, tempUnitName((TempUnit)i))) { out = (TempUnit)i; return true; }
    return false;
}

inline const char *groupOnName(GroupOn g) { return g == GroupOn::GROUP_ON_ALL ? "all" : "any"; }
inline bool groupOnFromName(const char *n, GroupOn &out) {
    if (!n) return false;
    if (!strcmp(n, "any")) { out = GroupOn::GROUP_ON_ANY; return true; }
    if (!strcmp(n, "all")) { out = GroupOn::GROUP_ON_ALL; return true; }
    return false;
}
inline const char *sceneShowName(SceneShow v) {
    return v == SceneShow::SCENES_ALL ? "all" : v == SceneShow::SCENES_OFF ? "off" : "visible";
}
inline bool sceneShowFromName(const char *n, SceneShow &out) {
    if (!n) return false;
    if (!strcmp(n, "visible")) { out = SceneShow::SCENES_VISIBLE; return true; }
    if (!strcmp(n, "all"))     { out = SceneShow::SCENES_ALL;     return true; }
    // "Off" is not offered (K43): a saved one reads as Visible rather than
    // leaving a card whose scenes nobody can bring back.
    if (!strcmp(n, "off"))     { out = SceneShow::SCENES_VISIBLE; return true; }
    return false;
}

// What the line under a state card's hero says.
//
// State is the icon and the colour, and the NAME is what tells two doors in
// one area apart - so the name is the default. But a user
// may prefer the state spelled out, or nothing at all. Resolved like TempUnit:
// fleet default, then page, then card.
//
// The state word comes from the device_class table in CardIcons.cpp - "Open",
// "Detected", "Locked" - never a raw "on"/"off" off the wire.
//
// Compound names per CLAUDE.md's macro hazard; checked against the framework
// headers with the grep in LESSONS.md, none of these is defined there.
enum class CardLabel : uint8_t {
    LBL_INHERIT = 0,  // use the page's setting (on a card) or the fleet's
    LBL_NAME,         // "Custom": the card's own name - one set on the device,
                      // else the dashboard's ("Desk"), else the source's.
                      // The default
    LBL_STATE,        // "Open" / "Closed", from the device_class table
    LBL_NONE,         // nothing; the hero stands alone
    LBL_HA,           // "From HA": the source's own name (Entity::sourceName,
                      // HA's friendly_name), else the card's (K40)
};

// Saved by name (Settings.h). "inherit" is never saved: it is the absence of
// a choice, so choosing it removes the key.
inline const char *cardLabelName(CardLabel l) {
    switch (l) {
        case CardLabel::LBL_NAME:  return "custom";
        case CardLabel::LBL_STATE: return "state";
        case CardLabel::LBL_NONE:  return "none";
        case CardLabel::LBL_HA:    return "ha";
        default:                   return "inherit";
    }
}
inline bool cardLabelFromName(const char *n, CardLabel &out) {
    if (!n) return false;
    if (!strcmp(n, "custom")) { out = CardLabel::LBL_NAME;  return true; }
    if (!strcmp(n, "state"))  { out = CardLabel::LBL_STATE; return true; }
    if (!strcmp(n, "none"))   { out = CardLabel::LBL_NONE;  return true; }
    if (!strcmp(n, "ha"))     { out = CardLabel::LBL_HA;    return true; }
    // Saved (K49): a named card's default is Custom, so Inherit is a choice.
    if (!strcmp(n, "inherit")) { out = CardLabel::LBL_INHERIT; return true; }
    return false;
}

// ---------------------------------------------------------------------------
// GROUP CARDS (DECISIONS K88-K92). A group card stands for a set of
// entities; its members are cards of their own (an id, their own settings). How
// it is SHOWN is its presentation (K90, K92 rule 6): one card, or a CLUSTER - a
// block of its members on the page, each a mini tile (K89). A cluster that
// cannot fit is shown as its card. A SUMMARY (K98): one card of
// members of one kind - one icon, active while any member is, its words naming
// the active ones ("North", "Both"); a tap opens its Members. The group page
// (K99) is not built. Saved by name: "present": "card" / "cluster" / "summary".
// ---------------------------------------------------------------------------
enum class GroupPresent : uint8_t { PRESENT_CARD = 0, PRESENT_CLUSTER, PRESENT_SUMMARY };
inline const char *groupPresentName(GroupPresent p) {
    return p == GroupPresent::PRESENT_CLUSTER ? "cluster" : p == GroupPresent::PRESENT_SUMMARY ? "summary" : "card";
}
inline bool groupPresentFromName(const char *n, GroupPresent &out) {
    if (!n) return false;
    if (!strcmp(n, "card"))    { out = GroupPresent::PRESENT_CARD;    return true; }
    if (!strcmp(n, "cluster")) { out = GroupPresent::PRESENT_CLUSTER; return true; }
    if (!strcmp(n, "summary")) { out = GroupPresent::PRESENT_SUMMARY; return true; }
    return false;
}

// A cluster header's style (K102): the group's colour as a SWATCH beside the
// cluster's name, or the header a BAR in that colour. Page-wide (the page's
// "cl_style"), each cluster able to take its own ("cl_style" on its card).
enum class ClusterStyle : uint8_t { CLSTYLE_SWATCH = 0, CLSTYLE_BAR };
inline const char *clusterStyleName(ClusterStyle s) { return s == ClusterStyle::CLSTYLE_BAR ? "bar" : "swatch"; }
inline bool clusterStyleFromName(const char *n, ClusterStyle &out) {
    if (!n) return false;
    if (!strcmp(n, "swatch")) { out = ClusterStyle::CLSTYLE_SWATCH; return true; }
    if (!strcmp(n, "bar"))    { out = ClusterStyle::CLSTYLE_BAR;    return true; }
    return false;
}

static constexpr uint8_t GROUP_MEMBERS_MAX = 12;   // a group card's members
static constexpr uint8_t CLUSTER_SLOTS_MAX = 4;    // the members a cluster header lists (a priority
                                                   // list - the header shows what fits, K104)

// ---------------------------------------------------------------------------
// Where a card wants to sit, in grid UNITS.
//
// The unit is a sub-cell (D-4): spans are written in units, so changing the
// subdivision never means re-reading every span.
//
//   A page is authored as N x M CELLS and allocates N*sub x M*sub UNITS.
//   With the default subdivision of 2, ONE CELL IS 2x2 UNITS, so an ordinary
//   card is prefSpan 2x2 and the defaults below say so. A quarter-page card on
//   a 3x3 page is 3x3 units; a half-cell card is 2x1 or 1x2.
//
// A card carries preferred span, minimum span and priority; priority is read
// by CardPage::plan(), which drops the lowest-priority cards until the rest fit.
//
// Deliberately NOT on EntityDescriptor. Entity.h says why: keeping size out of
// the entity is what lets the same temperature reading be a 1x1 tile on one
// page and a 4x2 chart on another.
// ---------------------------------------------------------------------------
struct CardPlacement {
    uint8_t prefSpanX = 2;   // UNITS. 2 = one cell at the default subdivision
    uint8_t prefSpanY = 2;
    uint8_t minSpanX  = 2;   // shrink to this before dropping to a lower row
    uint8_t minSpanY  = 2;
    uint8_t priority  = 128; // higher survives when the page runs out of room.
                             // 128 is the neutral middle of the range, so a
                             // card can be pushed either way without
                             // renumbering everything else

    // Explicit placement, in units, or PAGE_FLOW (-1) to be flowed.
    //
    // D-4 chose "placement at unit granularity, plus a validator" over a
    // constraint solver, so a layout is predictable: any card may
    // be pinned anywhere, and the page REPORTS overlaps and out-of-bounds
    // rather than quietly resolving them. See CardPage::plan().
    int8_t  col = -1;
    int8_t  row = -1;
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
// VAR_AUTO is the default and resolves at build time by measuring. A card can
// pin either variant.
// ---------------------------------------------------------------------------
// An "on" state drawn on a card's edge (D-30 to D-33; Card::activeFrame()).
// Compound names per CLAUDE.md's macro hazard.
enum class CardFrame : uint8_t {
    FRAME_NONE = 0,   // the edge restyle() gives
    FRAME_BRACKET,    // heavy top and bottom bars, light falling inward from them
    FRAME_CORNER,     // a frame, the top-right and bottom-left corners cut at 45 degrees
};

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
const char *cardVariantName(CardVariant v);   // "auto", "full", "compact": also how it is saved
inline bool cardVariantFromName(const char *n, CardVariant &out) {
    if (!n) return false;
    for (uint8_t i = 0; i < 3; i++)
        if (!strcmp(n, cardVariantName((CardVariant)i))) { out = (CardVariant)i; return true; }
    return false;
}

#endif // CARD_TYPES_H
