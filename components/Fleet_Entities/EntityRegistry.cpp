#include "EntityRegistry.h"
#include <new>   // placement new over caller-provided storage

// ---------------------------------------------------------------------------
// Names. Kept in the .cpp so the header stays free of string tables.
// ---------------------------------------------------------------------------

const char *entityKindName(EntityKind k) {
    switch (k) {
        case EntityKind::SENSOR:        return "sensor";
        case EntityKind::BINARY_SENSOR: return "binary_sensor";
        case EntityKind::SWITCH:        return "switch";
        case EntityKind::LIGHT:         return "light";
        case EntityKind::BUTTON:        return "button";
        case EntityKind::NUMBER:        return "number";
        case EntityKind::TEXT:          return "text";
        case EntityKind::CLIMATE:       return "climate";
        case EntityKind::WEATHER:       return "weather";
    }
    return "?";
}

const char *entitySourceName(EntitySource s) {
    switch (s) {
        case EntitySource::LOCAL:   return "local";
        case EntitySource::SYSTEM:  return "system";
        case EntitySource::MQTT:    return "mqtt";
        case EntitySource::HA:      return "ha";
        case EntitySource::VIRTUAL: return "virtual";
    }
    return "?";
}

// The "p" key in a device-based discovery payload's cmps map (issue #11).
// Deliberately a separate function from entityKindName(): they happen to agree
// today, but ours is a debug label and this one is wire format that Home
// Assistant parses. Letting a rename of one silently change the other is how a
// whole device disappears from HA after a cosmetic commit.
const char *entityKindHaPlatform(EntityKind k) {
    switch (k) {
        case EntityKind::SENSOR:        return "sensor";
        case EntityKind::BINARY_SENSOR: return "binary_sensor";
        case EntityKind::SWITCH:        return "switch";
        case EntityKind::LIGHT:         return "light";
        case EntityKind::BUTTON:        return "button";
        case EntityKind::NUMBER:        return "number";
        case EntityKind::TEXT:          return "text";
        case EntityKind::CLIMATE:       return "climate";
        case EntityKind::WEATHER:       return "weather";
    }
    return "sensor";
}

// ---------------------------------------------------------------------------
// Registration
// ---------------------------------------------------------------------------

int EntityRegistry::indexOf(const char *id) const {
    if (!id || !id[0] || !_items) return -1;
    for (uint8_t i = 0; i < _count; i++) {
        if (strcmp(_items[i].desc.id, id) == 0) return (int)i;
    }
    return -1;
}

bool EntityRegistry::begin(Entity *storage, uint8_t capacity) {
    if (!storage || capacity == 0) return false;

    std::lock_guard<std::mutex> lk(_mx);

    // The caller may hand us raw malloc'd memory, and Entity is not trivially
    // constructible - EntityValue has a user-provided constructor and several
    // members have default initialisers. Without this, every field would start
    // as whatever was in that memory.
    // The cast to void* is not decoration. Placement new is declared as
    //   void *operator new(size_t, void *)
    // and passing an Entity* relies on an implicit conversion that GCC accepts
    // but VSCode's IntelliSense parser rejects, reporting "no instance of
    // overloaded operator new matches the argument list". Being explicit
    // compiles identically and keeps the Problems pane honest - a permanent
    // false error there is worse than none, because it teaches you to ignore it.
    for (uint8_t i = 0; i < capacity; i++) {
        ::new (static_cast<void *>(&storage[i])) Entity();
    }

    _items    = storage;
    _capacity = capacity;
    _count    = 0;
    return true;
}

Entity *EntityRegistry::add(const EntityDescriptor &d) {
    if (!d.id[0]) return nullptr;

    std::lock_guard<std::mutex> lk(_mx);

    if (!_items) return nullptr;              // begin() was never called
    if (indexOf(d.id) >= 0) return nullptr;   // duplicate id: caller's bug
    if (_count >= _capacity) return nullptr;

    Entity &e = _items[_count];
    e = Entity{};
    e.desc = d;
    e.value.type = d.valueType;

    _count++;
    return &e;
}

Entity *EntityRegistry::find(const char *id) {
    std::lock_guard<std::mutex> lk(_mx);
    const int i = indexOf(id);
    return (i < 0) ? nullptr : &_items[i];
}

const Entity *EntityRegistry::find(const char *id) const {
    std::lock_guard<std::mutex> lk(_mx);
    const int i = indexOf(id);
    return (i < 0) ? nullptr : &_items[i];
}

// ---------------------------------------------------------------------------
// Provider side
// ---------------------------------------------------------------------------

bool EntityRegistry::setValue(const char *id, const EntityValue &v, uint32_t nowMs) {
    std::lock_guard<std::mutex> lk(_mx);

    const int i = indexOf(id);
    if (i < 0) return false;
    Entity &e = _items[i];

    // An authoritative value always clears a pending optimistic write: this IS
    // the echo we were waiting for. It clears even when the value disagrees
    // with what we optimistically applied - the source is right and we were
    // wrong, which is exactly the case the reconcile exists to catch.
    e.pending = false;

    const bool changed = !e.value.equals(v) || !e.everSet;

    e.value        = v;
    e.lastUpdateMs = nowMs;
    e.everSet      = true;

    // Unchanged values are not dirtied. This is most of ROADMAP 4.2's
    // rate-limiting: a sensor republishing the same reading every second
    // causes no redraws at all.
    if (changed) e.dirty = true;
    return changed;
}

// ---------------------------------------------------------------------------
// UI side
// ---------------------------------------------------------------------------

bool EntityRegistry::commandValue(const char *id, const EntityValue &v, uint32_t nowMs) {
    std::lock_guard<std::mutex> lk(_mx);

    const int i = indexOf(id);
    if (i < 0) return false;
    Entity &e = _items[i];
    if (!e.desc.writable) return false;

    // Remember what to fall back to. Guard against a second tap inside the
    // window overwriting prevValue with the optimistic value from the first -
    // that would make the revert restore a state the hardware never had.
    if (!e.pending) e.prevValue = e.value;

    e.value          = v;
    e.pending        = true;
    e.pendingSinceMs = nowMs;
    e.lastUpdateMs   = nowMs;
    e.everSet        = true;
    e.dirty          = true;
    return true;
}

void EntityRegistry::drainDirty(DirtyFn fn, void *ctx) {
    if (!fn) return;

    for (uint8_t i = 0; i < _count; i++) {
        Entity snapshot;
        {
            std::lock_guard<std::mutex> lk(_mx);
            if (!_items || i >= _count || !_items[i].dirty) continue;
            snapshot = _items[i];
            _items[i].dirty = false;
        }
        // Callback runs OUTSIDE the lock. A slow widget update must never
        // block a provider that is mid-publish on another task.
        fn(snapshot, ctx);
    }
}

// ---------------------------------------------------------------------------
// Housekeeping
// ---------------------------------------------------------------------------

bool EntityRegistry::isStale(const Entity &e, uint32_t nowMs) const {
    if (!e.everSet) return true;
    if (e.desc.staleAfterMs == 0) return false;
    return (nowMs - e.lastUpdateMs) > e.desc.staleAfterMs;
}

void EntityRegistry::tick(uint32_t nowMs) {
    for (uint8_t i = 0; i < _count; i++) {
        std::lock_guard<std::mutex> lk(_mx);
        if (!_items || i >= _count) break;
        Entity &e = _items[i];

        // An optimistic write whose echo never arrived. Revert, and dirty the
        // entity so the control visibly springs back. Showing a switch as on
        // when the command was never acted upon is precisely the class of lie
        // this project has already been bitten by three times.
        if (e.pending && (nowMs - e.pendingSinceMs) > _reconcileMs) {
            e.value   = e.prevValue;
            e.pending = false;
            e.dirty   = true;
        }
    }
}
