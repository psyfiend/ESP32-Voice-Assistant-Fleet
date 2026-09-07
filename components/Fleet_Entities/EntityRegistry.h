#pragma once
#ifndef ENTITY_REGISTRY_H
#define ENTITY_REGISTRY_H

#include "Entity.h"
#include <mutex>

// ---------------------------------------------------------------------------
// The Entity Registry. ROADMAP section 4.1 (what it is) and 4.2 (the rule that
// keeps it from corrupting LVGL).
//
// THE THREADING CONTRACT, restated because getting it wrong does not crash
// immediately - it corrupts LVGL and crashes randomly, hours later:
//
//   Providers (MQTT, system telemetry, I2C sensors) run on their own tasks.
//   They call setValue(). That takes a short mutex, writes the value, marks
//   the entity dirty, and returns. A provider NEVER touches LVGL.
//
//   The LVGL task calls drainDirty() from an lv_timer, roughly every 100 ms.
//   That is the ONLY place bound widgets are updated, and it runs on the one
//   task LVGL is safe on.
//
// Suppressing unchanged values (see EntityValue::equals) also rate-limits for
// free: a topic firing 50x/sec produces at most 10 redraws/sec, and none at
// all if the value is not actually moving.
//
// std::mutex rather than a FreeRTOS handle, deliberately: it works on the
// device AND on a PC, which is what keeps this library unit-testable per
// ROADMAP Q9. Nothing in this library includes Arduino.h.
// ---------------------------------------------------------------------------

// Fixed capacity, no heap. At ~400 bytes per entity this is ~19 KB of static
// storage - affordable on every board in the fleet, and predictable, which
// matters more. Raise it deliberately rather than by reflex; the memory-budget
// spike (issue #14) should measure the real number on the smallest board.
static constexpr uint8_t ENTITY_MAX = 48;

class EntityRegistry {
public:
    // --- Registration (startup only, single-threaded) ---------------------
    //
    // Called by each provider for the entities it owns. Deliberately a
    // registration call rather than a central table: a new sensor library
    // declares its own entities and becomes placeable on a dashboard and
    // visible in HA without editing any shared list.
    //
    // Returns nullptr if the id is empty, already taken, or the table is full.
    Entity *add(const EntityDescriptor &d);

    Entity       *find(const char *id);
    const Entity *find(const char *id) const;

    uint8_t       count() const { return _count; }
    const Entity *at(uint8_t i) const { return (i < _count) ? &_items[i] : nullptr; }

    // --- Provider side (any task) -----------------------------------------

    // Write a value from its source of truth. Takes the lock briefly.
    // Returns true if the value actually changed (and the entity was dirtied).
    //
    // An echo of a pending optimistic write clears the pending state, which is
    // how a tap gets confirmed.
    bool setValue(const char *id, const EntityValue &v, uint32_t nowMs);

    // --- UI side ----------------------------------------------------------

    // Optimistically apply a commanded value so a control responds instantly,
    // and start the reconcile window. The CALLER is responsible for actually
    // sending the command (publishing to MQTT); this only updates local state.
    //
    // Returns false if the entity is unknown or not writable.
    bool commandValue(const char *id, const EntityValue &v, uint32_t nowMs);

    // Drain the dirty set. Call from the LVGL task only.
    //
    // The callback is invoked OUTSIDE the lock, with a snapshot, so a slow
    // widget update cannot block a provider mid-publish. `ctx` is passed
    // straight through so this stays free of std::function and heap.
    using DirtyFn = void (*)(const Entity &snapshot, void *ctx);
    void drainDirty(DirtyFn fn, void *ctx);

    // Housekeeping: expires stale values and reverts optimistic writes whose
    // echo never arrived. Safe from any task. Call every loop.
    void tick(uint32_t nowMs);

    // How long an optimistic write waits for its echo before reverting.
    void setReconcileTimeoutMs(uint32_t ms) { _reconcileMs = ms; }

    // True when the value is older than its staleAfterMs, or was never set.
    // Cards use this to grey out rather than display a confident stale number.
    bool isStale(const Entity &e, uint32_t nowMs) const;

private:
    Entity  _items[ENTITY_MAX];
    bool    _dirty[ENTITY_MAX] = {};
    uint8_t _count = 0;

    mutable std::mutex _mx;
    uint32_t _reconcileMs = 5000;   // generous: a round trip through a broker
                                    // and back via HA can take a moment

    int  indexOf(const char *id) const;   // caller holds the lock (or startup)
};

#endif // ENTITY_REGISTRY_H
