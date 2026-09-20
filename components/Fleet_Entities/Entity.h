#pragma once
#ifndef ENTITY_H
#define ENTITY_H

#include "EntityTypes.h"

// ---------------------------------------------------------------------------
// One entity. Fields follow ROADMAP section 4.1's table.
//
// Split deliberately into two halves:
//
//   EntityDescriptor - what the thing IS. Static, declared once, never
//                      changes at runtime. Identity, semantics, capabilities.
//   Entity           - the descriptor plus its live value and bookkeeping.
//
// What is NOT here, and must never be added: anything about LAYOUT. No grid
// span, no page, no card type, no position. ROADMAP Q3b/Q4 place cards in grid
// *units* from the build sheet, with preferred_span / min_span / priority for
// responsive degradation. Keeping size out of the entity is what lets the same
// temperature reading appear as a 2x2 tile on one page and a 4x2 graph on
// another. Put a span in here and that becomes impossible to express.
// ---------------------------------------------------------------------------

static constexpr uint8_t ENTITY_ID_MAX     = 40;  // "living_room_temp"
static constexpr uint8_t ENTITY_NAME_MAX   = 40;  // "Living Room Temperature"
static constexpr uint8_t ENTITY_SHORT_MAX  = 20;  // unit, device_class, state_class

// ICONS NEED MORE ROOM THAN THE OTHER SHORT FIELDS, AND 20 WAS NOT ENOUGH.
//
// Material Design Icon names are long and the ones this fleet actually wants
// are among the longest: `mdi:ceiling-light-outline` is 25 characters and
// `mdi:motion-sensor-off` is 21. Both overflowed, and the second matters more
// than the first, because HA ships the live state-dependent icon in
// `attributes.icon` - so the limit was not only clipping what we declare, it
// would have clipped what the server sends us.
//
// Found at compile time on the owner's 18-entity table (2026-09-20), which is
// the good outcome; a char array one byte too small for a string that arrives
// at runtime truncates silently and renders as the wrong glyph or as tofu.
//
// 32 costs 12 bytes per entity over the old size, and entity storage lives in
// PSRAM (see SystemCore::beginEntityStorage), so 128 entities pay 1.5 KB of
// the cheap memory. Splitting it out rather than raising ENTITY_SHORT_MAX
// keeps unit / device_class / state_class - which are genuinely short - from
// paying for it.
static constexpr uint8_t ENTITY_ICON_MAX   = 32;  // "mdi:ceiling-light-outline"
static constexpr uint8_t ENTITY_TOPIC_MAX  = 128; // external topic / HA entity id

struct EntityDescriptor {
    // Stable key a card binds to. Never renumbered, never reused - the same
    // frozen-identity discipline the reference project's page registry learned
    // the hard way (see docs/REFERENCE_PROJECTS.md). A build sheet persists
    // these, so changing one silently re-points a user's saved dashboard.
    char id[ENTITY_ID_MAX] = {0};

    char       name[ENTITY_NAME_MAX] = {0};  // human label, shown in HA and on cards
    EntityKind kind = EntityKind::SENSOR;
    EntitySource source = EntitySource::LOCAL;
    ValueType  valueType = ValueType::NONE;

    // Display + HA discovery metadata. device_class and unit are what make HA
    // long-term statistics work; state_class especially - see issue #11.
    char unit[ENTITY_SHORT_MAX]        = {0};  // "C", "%", "lx", "W"
    char deviceClass[ENTITY_SHORT_MAX] = {0};  // "temperature", "occupancy"
    char stateClass[ENTITY_SHORT_MAX]  = {0};  // "measurement", "total_increasing"
    char icon[ENTITY_ICON_MAX]         = {0};  // "mdi:thermometer"

    // Can the UI command it? Drives whether a card offers a control at all.
    bool writable = false;

    // Maps to HA's `entity_category: diagnostic`. Diagnostic entities are
    // hidden from a device's main controls and tucked into its diagnostics
    // section - correct for RSSI, uptime and free heap, which are about the
    // panel's health rather than about the room it is in. Without this every
    // board contributes four pieces of debug telemetry to the user's primary
    // HA view, which is how a device page becomes unreadable.
    bool diagnostic = false;

    // Should this be published in our HA discovery payload?
    //
    // This is the real difference between the two groups of entity, and it is
    // one flag rather than a separate code path:
    //   advertise = true  -> WE own it. It is ours to publish and ours to
    //                        report. Local sensors, board telemetry.
    //   advertise = false -> SOMEONE ELSE owns it. We subscribe and render it,
    //                        and must not tell HA it is one of our entities -
    //                        doing so would create a duplicate of a thing that
    //                        already exists.
    bool advertise = true;

    // For NUMBER (and LIGHT brightness): the range a control may command.
    float minValue = 0.0f;
    float maxValue = 100.0f;
    float step     = 1.0f;

    // How long a value stays trustworthy. 0 = never goes stale (a switch we
    // command holds its value; a temperature feed does not). Past this, cards
    // grey out rather than confidently displaying a number from last Tuesday.
    uint32_t staleAfterMs = 0;

    // Only meaningful when advertise == false: where someone else's value
    // arrives from. For EntitySource::HA this is the HA entity_id; for
    // EntitySource::MQTT a full topic. Left empty for entities we own, whose
    // topics are derived centrally from the device identity - ROADMAP 4.1 is
    // explicit that a descriptor never spells its own topics out.
    char externalRef[ENTITY_TOPIC_MAX] = {0};

    // Which field to read out of the payload that arrives on externalRef.
    //
    // Empty  -> the whole payload IS the value ("ON", "21.4").
    // Set    -> the payload is JSON and this names the key, e.g. "temperature"
    //           from {"temperature":21.4,"humidity":48,"battery":100}.
    //
    // This is what lets several entities share one topic - a Zigbee2MQTT
    // sensor publishes temperature, humidity and battery in a single message,
    // and three entities read three keys from it. HA solves the same problem
    // with `val_tpl`; a plain key is enough for us and needs no template
    // engine on the device.
    char valueKey[ENTITY_SHORT_MAX] = {0};
};

struct Entity {
    EntityDescriptor desc;

    EntityValue value;
    // WHEN WE LAST HEARD ANYTHING, changed or not. Issue #57.
    //
    // This is a statement about the TRANSPORT, not about the thing: it moves
    // on every message, including one carrying a value identical to the last.
    // Staleness is derived from it, because staleness is a question about
    // whether we are still in touch.
    uint32_t    lastUpdateMs = 0;

    // WHEN THE VALUE LAST ACTUALLY CHANGED. Issue #57.
    //
    // A different question, and the one a card usually wants: "the garage has
    // been open for 40 minutes", "the light went on at 6:03". Deriving that
    // from lastUpdateMs is wrong in both directions - an MQTT sensor
    // republishing an unchanged reading every 30 s would claim it just
    // changed, and an HA entity on a change-driven feed would be indefinitely
    // "unchanged" only because nothing has been said.
    //
    // Set together with the first value, so "changed" and "first seen" are the
    // same instant rather than zero.
    uint32_t    lastChangeMs = 0;

    bool        everSet      = false;   // distinguishes "0" from "no reading yet"

    // --- Optimistic write bookkeeping ------------------------------------
    //
    // Owner's decision (2026-09-07): a tap updates the entity immediately so
    // the control responds, then reconciles when the source echoes the change
    // back - reverting if it never does.
    //
    // pending* holds the value we optimistically applied and the moment we
    // applied it. On echo, the pending state clears. On timeout, `value`
    // reverts to prevValue and the card shows what the hardware actually did.
    // Without the revert this becomes the third lying diagnostic in a project
    // that has already been bitten by three.
    bool        pending        = false;
    EntityValue prevValue;             // value before the optimistic write
    uint32_t    pendingSinceMs = 0;

    // Did the LAST command on this entity fail to take?
    //
    // This lives on the entity rather than on whichever card issued the
    // command, and that placement is the whole point. A card bound to several
    // switches was previously tracking its own commands in a bitmask, so a
    // parent card and a child card bound to the same switch could disagree
    // about whether it had failed - the answer depended on which card you had
    // tapped, not on what the switch was actually doing. The owner's rule is
    // that a parent reflects "the current state of the children, NOT the
    // method or path in which child cards arrived at their state", and that is
    // only expressible if the fact belongs to the thing itself.
    //
    // Set when the reconcile window expires, or when an echo arrives carrying
    // the value from BEFORE the command. Cleared by the next command, and by
    // any genuine change from the source - at which point we know the current
    // state and the old failure is history.
    //
    // It is one bool and no new dependency, so the zero-dependency guarantee
    // of ROADMAP Q9 is untouched.
    bool        cmdFailed      = false;

    // Needs a UI update. Lives here rather than in a parallel array in the
    // registry so that entity storage is ONE allocation - which is what lets
    // the whole table be placed in PSRAM with a single call. See
    // EntityRegistry::begin().
    bool        dirty          = false;

    // --- Availability, as stated by the source. Issue #56. -----------------
    //
    // NOT derived from age. `lastUpdateMs` answers "when did we last hear
    // anything", which under a change-driven feed is not evidence of health:
    // HA's subscribe_trigger fires on CHANGE, so a thermostat holding steady
    // and a thermostat that has been unplugged are indistinguishable by age.
    //
    // HA states this directly - it sends the literal state "unavailable" - and
    // that word is strictly better information than any timeout we could
    // invent. Before this the word was simply dropped on the floor and the
    // card went on displaying its last good reading indefinitely.
    //
    // Starts TRUE so that an entity nobody has said anything about is not born
    // broken; `everSet` is what distinguishes "no reading yet".
    bool        available      = true;

    // --- The user's own choice. Issue #60. --------------------------------
    //
    // PAUSED LIVES ON THE ENTITY, NOT ON THE CARD, and that placement is the
    // whole point. A card is a VIEW; "this temperature probe is broken, stop
    // sending bogus data to HA" is a statement about the PROBE, and stays true
    // whether or not a tile happens to be showing it.
    //
    // This project already made the identical call once, for cmdFailed just
    // below: a fact tracked per-card let a parent card and a child card bound
    // to the same switch disagree. Pause has the same shape - leave it on the
    // card and pausing from the wrong tile leaves a local sensor publishing.
    //
    // What it means, by who owns the entity:
    //   advertise == true  (ours)    -> stop sampling, stop publishing, and
    //                                   tell HA "unavailable" once.
    //   advertise == false (theirs)  -> stop APPLYING inbound values.
    //
    // Either way the value stops changing, so the frozen display is free -
    // deliberately NOT implemented as "stop rendering", which would leave the
    // registry holding one number while the screen showed another.
    //
    // Owner's rule, and it is stronger than "looks frozen": a paused entity
    // stops ALL conditional evaluation. No staleness transition, no cmdFailed
    // transition. Held still, not merely quiet.
    //
    // Persisted in NVS: "if I want a card paused I do not want a reboot to
    // unpause it."
    bool        paused         = false;
};

#endif // ENTITY_H
