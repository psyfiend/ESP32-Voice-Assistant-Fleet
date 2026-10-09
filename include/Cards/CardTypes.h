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

    // THE SOURCE SAID SO. Issue #56.
    //
    // Distinct from ST_STALE, and the distinction is the entire point: stale
    // means "we have not heard from this in a while and are guessing", while
    // this means "the thing that owns it TOLD us it is not there". One is an
    // inference from silence, the other is a fact from the horse's mouth, and
    // a panel that renders them identically throws away the better signal.
    //
    // It matters most on the HA path, where silence carries no information at
    // all: subscribe_trigger fires on change, so a quiet sensor and a dead one
    // look the same by age. HA sends the word "unavailable" as a state, and
    // that word is the only reliable evidence available. Without this the
    // value was simply not written and the card kept displaying its last good
    // reading for ever - the fourth lying diagnostic in a project that has
    // already paid for three.
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
           // UNAVAILABLE MUST TAKE THE BODY, not merely wear a tag. Issue #56.
           //
           // This was nearly missed: adding the state and its colours is not
           // enough, because stateColor() returns early for anything this
           // predicate rejects. The card would have kept displaying its last
           // good reading under a small "N/A" badge - which is precisely the
           // lie the whole issue exists to remove, just with a label on it.
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
//   HDR_BAND   a filled band inside the card, edge to edge (was HDR_BAR; the
//              owner named it "band", card-sheet 11.3 - "bar" read like the
//              system header bar)
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
//
// HDR_TAG_FLOAT (owner, 2026-10-01, design interview §1): the same pills, but
// lying ACROSS the card's top corners - half above the card's top edge and
// sticking out the same amount past its side - so a tag reads as resting on
// the card and, on Linen, casts its shadow onto it. The owner's new default.
// It costs the card exactly what HDR_TAG does (one headerHeight()), split
// between the row above the card and an inset at the top of the body that
// keeps the corner icon out from under the pill. Appended, so the values
// already stored as uint8_t keep their meaning.
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
// The owner's rule, 2026-09-15: "I will always prefer temp in F not C but both
// should be supported and customizable by users if the source is not in the
// unit they want." Confirmed against his real Home Assistant, where 34 of 35
// temperature sensors report Fahrenheit and one reports Celsius - so a fleet
// with one preference and no conversion would render that one card wrong,
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
// How a DURATION is rendered (issue #51).
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
    // "3d 04:15" beyond. The owner's pick, 2026-09-19, and the default - it is
    // what a person says out loud, and it keeps the card narrow while a board
    // is young without lying about a board that has been up for weeks.
    DUR_AUTO,
    // Always H:MM:SS, hours growing past two digits rather than wrapping.
    // Honest, and wide - three weeks reads "504:00:00".
    DUR_CLOCK,
    // The raw count with its source unit, exactly as it arrives. What every
    // duration did before this existed, kept so the change is reversible from
    // a settings screen rather than from a rebuild.
    DUR_SECONDS,
};

// ---------------------------------------------------------------------------
// When a card standing for several things is ON. 2.10b (#65), owner
// 2026-10-05: a group defined on the device behaves as an HA group does, and
// HA's group helper has exactly this choice - "The group's entity state is only
// ON if all members are on", off by default. A tap follows the same answer: a
// group that is on turns every member off; one that is off turns them all on.
// Native HA groups (light.office) are ONE entity and HA decides for them.
enum class GroupOn : uint8_t {
    GROUP_ON_ANY = 0,   // on when any member is on - HA's default
    GROUP_ON_ALL,       // on only when every member is on
};

// Which of a light's scenes its window offers (owner, 2026-10-07): the ones
// not hidden in HA's own UI (its entity registry's hidden_by), all of them,
// or none. A card setting, kept in RAM until saving arrives (2.10d).
enum class SceneShow : uint8_t {
    SCENES_VISIBLE = 0,   // the default
    SCENES_ALL,
    SCENES_OFF,
};

// WHAT A TAP DOES (2.10d, DECISIONS K37, K39). A long press always opens the
// window (K3); this is the tap. TAP_DEFAULT is the card type's own - toggle on
// a switch, a light or a button, nothing on a sensor - and is what an unsaved
// card does. The scenes are the ones the card's window offers (its Scenes
// setting); Cycle starts from the first after each boot.
enum class TapAction : uint8_t {
    TAP_DEFAULT = 0,
    TAP_TOGGLE,
    TAP_DETAILS,        // opens the window, as a long press does
    TAP_MEMBERS,        // opens it on Members
    TAP_HISTORY,        // opens it on History
    TAP_CYCLE_SCENES,   // loads the next scene, with a toast naming it
    TAP_LOAD_SCENE,     // loads one chosen scene ("tap_scene"), with a toast
    TAP_NOTHING,
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
        default:                          return "default";
    }
}
inline bool tapActionFromName(const char *n, TapAction &out) {
    if (!n) return false;
    for (uint8_t i = 1; i <= (uint8_t)TapAction::TAP_NOTHING; i++)
        if (!strcmp(n, tapActionName((TapAction)i))) { out = (TapAction)i; return true; }
    return false;
}

// The names these are saved under (2.10d, Settings.h): a saved choice is a
// NAME, so reordering an enum never changes what it means. A name this
// firmware does not know reads as "not set".
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
    // "Off" was dropped (K43): nothing offers it, so a saved one reads as
    // Visible rather than leaving a card whose scenes nobody can bring back.
    if (!strcmp(n, "off"))     { out = SceneShow::SCENES_VISIBLE; return true; }
    return false;
}

// What the line under a state card's hero says. Milestone 2.7.
//
// The owner, 2026-09-22: state is the icon and the colour, and the NAME is what
// tells two doors in one area apart - so the name is the default. But a user
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
                      // HA's friendly_name), else the card's (2.10d, K40)
    // A fourth mode, LBL_NO_ICON - name kept, hero and disc dropped - was
    // built and tried on glass 2026-09-22 and REJECTED by the owner: "We're
    // not building a minimal dashboard. We're building an awesome dashboard."
    // Removed rather than left switched off.
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
    return false;
}

// ---------------------------------------------------------------------------
// Where a card wants to sit, in grid UNITS.
//
// CHANGED AT 2.5, and the change is the unit rather than the fields: these
// used to be CELLS. ROADMAP Q3b decided sub-grid placement on 2026-09-03 and
// the reason it is being honoured now rather than later is that document's own
// warning - it is very expensive to retrofit, because every span already
// written has to be re-read in the new unit.
//
//   A page is authored as N x M CELLS and allocates N*sub x M*sub UNITS.
//   With the default subdivision of 2, ONE CELL IS 2x2 UNITS, so an ordinary
//   card is prefSpan 2x2 and the defaults below say so. A quarter-page card on
//   a 3x3 page is 3x3 units; a half-cell card is 2x1 or 1x2.
//
// Issue #15: "MUST carry preferred_span / min_span / priority from the very
// first version. Retrofitting responsive sizing after 8 card types exist means
// rewriting all 8." All three are now actually READ - priority by
// CardPage::plan(), which drops the lowest-priority cards until the rest fit.
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
    // ROADMAP Q3b chose "placement at unit granularity, plus a validator" over
    // a constraint solver, precisely so a layout is predictable: any card may
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
