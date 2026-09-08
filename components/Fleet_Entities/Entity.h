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
static constexpr uint8_t ENTITY_SHORT_MAX  = 20;  // unit, device_class, icon
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
    char icon[ENTITY_SHORT_MAX]        = {0};  // "mdi:thermometer"

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
};

struct Entity {
    EntityDescriptor desc;

    EntityValue value;
    uint32_t    lastUpdateMs = 0;
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
};

#endif // ENTITY_H
