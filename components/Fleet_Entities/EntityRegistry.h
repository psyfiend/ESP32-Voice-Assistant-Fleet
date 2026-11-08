#pragma once
#ifndef ENTITY_REGISTRY_H
#define ENTITY_REGISTRY_H

#include "Entity.h"
#include <mutex>
#include <atomic>

// ---------------------------------------------------------------------------
// The Entity Registry (DECISIONS A3): what it is, and the rule that keeps it
// from corrupting LVGL.
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
// DECISIONS A6. Nothing in this library includes Arduino.h.
// ---------------------------------------------------------------------------

// Default capacity. NOT a hard limit - it is what the application asks for when
// it allocates storage, and it may pass something else.
//
// The table lives in PSRAM, never in an inline array: an inline array puts its
// size in internal SRAM on every board, and CYD_S3_3248 - the fleet's only QSPI
// board, whose LVGL buffers must live in INTERNAL SRAM rather than PSRAM (see
// LVGL_Flush_Gfx.cpp) - is then left with too little internal RAM for the WiFi
// driver to bring up an AP (a crash inside ieee80211_hostap_attach). See
// docs/LESSONS.md.
//
// The number is chosen rather than inherited. A home HA carries over a
// thousand entities and answers /api/states with hundreds of KB, so the panel
// never mirrors a house - it subscribes to a named subset, and this is the
// ceiling on that subset. Cards are added from HA at runtime (design/ha-cards.md
// 2.2): the board's own entities, the entities labelled "fleet" in HA, and the
// members and scenes learnt from them. An Entity is ~1 KB (the boot log prints
// it), so 192 costs ~192 KB of PSRAM and nothing in internal SRAM. HaPublisher's
// per-slot arrays are in PSRAM too. The index type caps the count at 255
// regardless.
static constexpr uint8_t ENTITY_MAX = 192;

class EntityRegistry {
public:
    // Hand the registry its storage. The caller owns the memory and it must
    // outlive the registry.
    //
    // Storage is injected rather than allocated here ON PURPOSE: this library
    // has zero dependencies (DECISIONS A6) so it can compile and unit-test on a
    // PC, and calling heap_caps_malloc() would end that. The application knows
    // it is on an ESP32 and can place the block in PSRAM; a host test can pass
    // a plain array.
    //
    // Runs a placement-new over each slot, so raw malloc'd memory is fine -
    // Entity is not trivially constructible and its members would otherwise be
    // uninitialised.
    //
    // Returns false if storage is null or capacity is 0. Calling any other
    // method before this simply does nothing.
    bool begin(Entity *storage, uint8_t capacity);
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

    // Acquire: a reader on another task that sees the new count also sees the
    // entity add() or learnMembers() wrote into that slot before raising it.
    uint8_t       count() const { return _count.load(std::memory_order_acquire); }

    // Raw access by index, for diagnostics and for iterating at startup.
    //
    // DELIBERATELY UNLOCKED, and therefore NOT safe to call while another task
    // is writing. It returns a pointer into the table, so a concurrent
    // setValue() could tear the value out from under the reader.
    //
    // Safe only for providers that run on the loop() task. Callers that need a
    // consistent read while a provider runs on its own task (HA, MQTT) must go
    // through drainDirty() (which snapshots under the lock) or
    // find() a copy. Kept unlocked rather than made safe-by-default because a
    // locking accessor invites exactly the pattern A3 forbids: holding the
    // registry lock while doing LVGL work.
    const Entity *at(uint8_t i) const { return (i < count()) ? &_items[i] : nullptr; }

    // --- Groups defined at the source (#65) --------------------------------
    //
    // A group's members, as the source names them (HA's `entity_id` attribute
    // on light.office). Each ref not yet in the table is REGISTERED NOW - the
    // one way an entity is added after startup - as a copy of the group's
    // kind, source and writability, with a stable id derived from the ref
    // ("ha_light_office_lamp") so a pause or a saved setting finds it again
    // after a reboot. Then the group's member list is set. Any task; under the
    // lock. Returns how many entities were added.
    uint8_t learnMembers(const char *groupId, const char *const *refs, uint8_t n);

    // AN ENTITY ADDED WHILE RUNNING: a card added from HA names one the
    // compiled table does not have. As learnMembers(): the entity already in
    // the table with this source and ref is returned as it is; else `d` is
    // registered now with an id made from its ref ("ha_switch_office_3d_printer"
    // - the same one every boot) and its ref for a name until the source sends
    // one. Any task; under the lock. Null when the table is full or `d` has no ref.
    const Entity *adopt(const EntityDescriptor &d);
    // The entity with this source and ref, or null. Under the lock.
    const Entity *findRef(EntitySource src, const char *ref) const;

    // Member `i` of a group, or null.
    const Entity *memberOf(const Entity &g, uint8_t i) const {
        return (i < g.nMembers) ? at(g.members[i]) : nullptr;
    }

    // A light's scenes: the same, with each scene's name as the source
    // gives it ("Relax"), registered as a BUTTON with a TEXT value (HA's
    // scene state is when it was last activated).
    // `hidden` (may be null): hidden in the source's own UI - sourceHidden.
    uint8_t learnScenes(const char *lightId, const char *const *refs,
                        const char *const *names, const bool *hidden, uint8_t n);
    const Entity *sceneOf(const Entity &l, uint8_t i) const {
        return (i < l.nScenes) ? at(l.scenes[i]) : nullptr;
    }

    // LOAD A SCENE, OR PRESS A BUTTON: handed to the command sink with no
    // optimistic value and nothing to confirm - a scene says nothing back
    // about whether it is still showing. A refusal by the source still marks
    // it FAILED (failCommand()). False if unknown, not a writable BUTTON, or
    // paused.
    bool press(const char *id, uint32_t nowMs);

    // A learnt entity's name, from the source's own word for it (HA's
    // friendly_name) - only while it still has the placeholder it was learnt
    // with (its ref). A declared name is the user's; a scene's is its own.
    bool adoptName(const char *id, const char *name);

    // The source's own name (Entity::sourceName), for any entity. Marks it
    // dirty only when it changed, so a card showing the entity label "HA name"
    // repaints.
    bool setSourceName(const char *id, const char *name);
    // Entity::sourceArea and sourceAreaId, the same way; "" clears them (HA
    // says no area).
    // `device`: its HA device's name, or null to leave it as it is.
    bool setSourceArea(const char *id, const char *areaId, const char *area, const char *device = nullptr);
    // Counts area changes on any entity: a group's colour depends on every
    // group's, so one entity's area can recolour cards on others.
    uint32_t areaEpoch() const { return _areaEpoch.load(); }

    // True once after learnMembers() added anything: the caller (the loop
    // task) then re-applies the saved pauses, which could not reach an entity
    // that did not exist at boot.
    bool takeLearnt() { return _learnt.exchange(false); }

    // --- Provider side (any task) -----------------------------------------

    // Write a value from its source of truth. Takes the lock briefly.
    // Returns true if the value actually changed (and the entity was dirtied).
    //
    // An echo of a pending optimistic write clears the pending state, which is
    // how a tap gets confirmed.
    bool setValue(const char *id, const EntityValue &v, uint32_t nowMs);

    // Record what the SOURCE says about an entity being reachable. Issue #56.
    //
    // Separate from setValue() on purpose: an unavailable entity has no value
    // to write, and inventing one - zero, or the previous reading - is exactly
    // the lie this exists to stop. Marks dirty only on a CHANGE, so a source
    // that repeats "unavailable" does not repaint the screen.
    bool setAvailable(const char *id, bool available, uint32_t nowMs);

    // Record the source's attributes - live icon, brightness, colour.
    //
    // Separate from setValue() because the two change independently: a light
    // fading down sends "on", "on", "on" with a falling brightness, and none of
    // those is a change of VALUE. Dirties only when an attribute actually
    // changed, so a sensor whose attributes never move costs no repaints.
    //
    // Does not move lastUpdateMs or touch the command bookkeeping - the value
    // that arrives with the attributes does that, through setValue().
    bool setAttrs(const char *id, const EntityAttrs &a);

    // The user's pause. Issue #60. Persisted (see PauseStore) so a reboot does
    // not undo it. Returns false if the id is unknown.
    bool setPaused(const char *id, bool paused, uint32_t nowMs);
    bool isPaused(const char *id) const;

    // Restore paused ids from the pause store. Call AFTER every provider has
    // registered, because an id that is not in the table yet cannot be marked.
    void restorePaused();

    // WHERE PAUSES ARE KEPT. A comma-delimited list of entity ids,
    // loaded and saved through these two functions. Unset, the registry keeps
    // it in NVS itself (fleet_ent/paused); SystemCore points it at the
    // settings file (Settings.h) - the registry knows nothing about that file. save() may be called on any task that already
    // calls setPaused().
    struct PauseStore {
        bool (*load)(char *out, size_t cap, void *ctx) = nullptr;
        bool (*save)(const char *list, void *ctx) = nullptr;
        void *ctx = nullptr;
    };
    void setPauseStore(const PauseStore &s) { _pauseStore = s; }
    // The list in NVS, for a one-time import into the settings file. "" if none.
    static bool readNvsPauseList(char *out, size_t cap);

    // How many entities are currently paused - for the system dump, which is
    // the only place a pause that outlives a reboot is discoverable from.
    uint8_t pausedCount() const;

    // Hand back the next entity whose pause was lifted, and CLEAR the flag in
    // the same locked step. Returns false when there are none.
    //
    // One call rather than "scan with at(), then clear" because at() is const
    // by design - the registry hands out read-only views and owns every
    // mutation itself. Doing the find and the clear together also means two
    // providers cannot both claim the same entity.
    //
    // The caller gets the id and the source, which is all it needs to decide
    // whether the refresh is its job.
    bool takeNeedsRefresh(char *idOut, size_t cap, EntitySource *srcOut,
                          char *refOut, size_t refCap);

    // --- UI side ----------------------------------------------------------

    // Optimistically apply a commanded value so a control responds instantly,
    // and start the reconcile window. The CALLER is responsible for actually
    // sending the command (publishing to MQTT); this only updates local state.
    //
    // Returns false if the entity is unknown or not writable.
    bool commandValue(const char *id, const EntityValue &v, uint32_t nowMs);

    // WHERE A COMMAND LEAVES THE DEVICE. Issue #44.
    //
    // commandValue() applies a value optimistically; the sink TRANSMITS it.
    // The registry knows nothing about transports: it hands the command to
    // whatever registered here and that thing decides whether it is a
    // websocket call_service or an MQTT publish.
    //
    // CALLED WITH THE REGISTRY LOCK RELEASED. The sink sends over a socket,
    // which can block for milliseconds; holding the mutex across that would
    // stall every provider and the LVGL thread behind it.
    typedef void (*CommandSink)(const Entity &e, const EntityValue &v, void *ctx);
    void setCommandSink(CommandSink fn, void *ctx) { _cmdFn = fn; _cmdCtx = ctx; }

    // A LIGHT'S LEVELS - brightness, colour temperature, colour (#65).
    //
    // The same contract as commandValue(): applied at once so the control and
    // the card answer the finger, then handed to the sink, then confirmed or
    // reverted by what the source says. A level implies ON (as in HA), and the
    // on half goes through the value's own bookkeeping, so "is it on" and "is
    // it at 40%" resolve independently - see Entity::attrPending.
    //
    // Only a MATCHING report ends the wait (within the rounding HA's own
    // conversions introduce: brightness +-3, kelvin +-3%, hue and saturation
    // +-3). A slider sends a command every 300 ms; the echoes of the earlier
    // ones, and a light's reports while it fades, are recorded as the fall-back
    // and change nothing on screen.
    //
    // For on/off alone use commandValue(): it is the path every source already
    // speaks. Returns false if unknown, not writable or paused.
    bool commandLight(const char *id, const LightCommand &c, uint32_t nowMs);

    typedef void (*LightSink)(const Entity &e, const LightCommand &c, void *ctx);
    void setLightSink(LightSink fn, void *ctx) { _lightFn = fn; _lightCtx = ctx; }

    // THE SOURCE REFUSED A COMMAND - end the wait now instead of in 3 s.
    //
    // For a transport that can say so: HA answers call_service with
    // success:false when it sent nothing (ha-websocket.md section 9). Reverts
    // whatever is still waiting, value and levels alike, and marks FAILED, as
    // tick() would. Does nothing if nothing is waiting. Any task. `why` goes
    // into the failure record below.
    bool failCommand(const char *id, const char *why = "refused by the source");

    // WHY THE LAST FEW COMMANDS FAILED, so a FAILED that cannot be reproduced
    // can be explained. Each FAILED - a confirming report that never came, or a
    // refusal - is recorded with what was asked and what the source last said,
    // and logged. The newest first. Any task.
    struct FailNote {
        char     id[ENTITY_ID_MAX];
        char     why[80];
        uint32_t atMs;
    };
    static constexpr uint8_t FAIL_NOTES = 4;
    uint8_t  failNotes(FailNote *out, uint8_t cap) const;
    uint16_t failTotal() const { return _failTotal; }

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

public:
    // How long the value has held its current reading, in ms. Issue #57.
    //
    // Answers "the garage has been open for 40 minutes", which is NOT what age
    // since lastUpdateMs answers. Returns 0 for an entity that has never had a
    // value, which callers must treat as "unknown" rather than "just now".
    static uint32_t heldForMs(const Entity &e, uint32_t nowMs) {
        return e.everSet ? (nowMs - e.lastChangeMs) : 0;
    }

private:

private:
    // Caller-owned storage; see begin(). The registry object itself stays tiny,
    // which is the point - only the table is large, and only the table moves.
    Entity *_items    = nullptr;
    uint8_t _capacity = 0;
    // Atomic: learnMembers() adds entities while other tasks walk
    // the table with at(). The slot is written first, then the count raised.
    std::atomic<uint8_t> _count{0};
    std::atomic<bool>    _learnt{false};
    std::atomic<uint32_t> _areaEpoch{0};

    CommandSink _cmdFn  = nullptr;
    void       *_cmdCtx = nullptr;
    LightSink   _lightFn  = nullptr;
    void       *_lightCtx = nullptr;

    mutable std::mutex _mx;
    PauseStore _pauseStore;   // unset: NVS, see setPauseStore()
    bool loadPauses(char *out, size_t cap) const;
    bool savePauses(const char *list) const;
    // 3 s (#63). Only a MATCHING echo confirms, so the window must outlast a
    // fading light: a Hue group takes ~1.5 s to settle, and FAILED should
    // show after 2-3 s. The websocket echo itself is ~91 ms.
    uint32_t _reconcileMs = 3000;

    int  indexOf(const char *id) const;   // caller holds the lock (or startup)

    // Record a failure (caller holds the lock). See failNotes().
    void noteFail(const Entity &e, const char *why, uint32_t nowMs);
    FailNote _fails[FAIL_NOTES] = {};
    uint8_t  _failNext  = 0;
    uint16_t _failTotal = 0;

    // learnMembers() and learnScenes(): find or register each ref as a copy of
    // `like`'s source, then set `dst`. Caller holds the lock.
    uint8_t learnInto(int gi, const char *const *refs, const char *const *names,
                      const bool *hidden, uint8_t n, EntityKind kind, ValueType vt,
                      uint8_t *dst, uint8_t &dstN, uint8_t max);
};

#endif // ENTITY_REGISTRY_H
