#include "EntityRegistry.h"

// ESP-IDF's NVS directly, NOT Arduino's Preferences.
//
// Preferences is a thin Arduino wrapper over exactly this, and the owner's
// standing constraint (2026-09-19) is to avoid Arduino-specific libraries so an
// ESP-IDF port stays straightforward. ConnectivityManager predates that rule and
// still uses Preferences; new code should not add to the pile. The two
// interoperate at the namespace level anyway, so this is not a split brain.
#include "esp_log.h"
#include "nvs.h"
#include "nvs_flash.h"
#include <string.h>
#include <stdio.h>
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


// ---------------------------------------------------------------------------
// Pause - issue #60
// ---------------------------------------------------------------------------
//
// Stored as ONE comma-delimited string of entity ids rather than a key each.
//
// That is a deliberate trade. A key per entity means a write per entity and a
// scan to read them back; one string means one write, whatever changed. NVS
// writes matter here beyond the usual wear argument: #41 records NINA's claim
// that a write runs with the CPU cache disabled and "can starve the esp-hosted
// SDIO transport and trigger a host restart" on exactly our P4 hardware.
//
// Pausing is a deliberate long-press, so it is rare by construction and nothing
// like the slider-drag case that motivated that warning. One write per press is
// the correct cost, and it is bounded.

// ESP-IDF logging rather than Serial: this translation unit deliberately pulls
// in no Arduino header, which is the point of using nvs.h here in the first
// place. The output lands on the same console either way.
static const char *ENT_TAG = "Entities";

static const char *PAUSE_NS  = "fleet_ent";
static const char *PAUSE_KEY = "paused";
static constexpr size_t PAUSE_BLOB_MAX = 512;

// Is `id` present in the comma-delimited list? Compares whole fields, so
// "deck_temp" does not match inside "deck_temp_2".
static bool listHas(const char *list, const char *id) {
    const size_t n = strlen(id);
    for (const char *p = list; *p; ) {
        const char *e = strchr(p, ',');
        const size_t len = e ? (size_t)(e - p) : strlen(p);
        if (len == n && strncmp(p, id, n) == 0) return true;
        if (!e) break;
        p = e + 1;
    }
    return false;
}

static bool loadPauseList(char *out, size_t cap) {
    out[0] = '\0';
    nvs_handle_t h;
    if (nvs_open(PAUSE_NS, NVS_READONLY, &h) != ESP_OK) return false;
    size_t len = cap;
    esp_err_t err = nvs_get_str(h, PAUSE_KEY, out, &len);
    nvs_close(h);
    if (err != ESP_OK) { out[0] = '\0'; return false; }
    return true;
}

static bool savePauseList(const char *list) {
    nvs_handle_t h;
    if (nvs_open(PAUSE_NS, NVS_READWRITE, &h) != ESP_OK) return false;
    esp_err_t err = nvs_set_str(h, PAUSE_KEY, list);
    if (err == ESP_OK) err = nvs_commit(h);
    nvs_close(h);
    return err == ESP_OK;
}

bool EntityRegistry::isPaused(const char *id) const {
    std::lock_guard<std::mutex> lk(_mx);
    const int i = indexOf(id);
    return (i >= 0) && _items[i].paused;
}

bool EntityRegistry::takeNeedsRefresh(char *idOut, size_t cap, EntitySource *srcOut,
                                      char *refOut, size_t refCap) {
    std::lock_guard<std::mutex> lk(_mx);
    for (uint8_t i = 0; i < _count; i++) {
        Entity &e = _items[i];
        if (!e.needsRefresh) continue;

        // Cleared HERE, under the lock, whatever the caller does next. A
        // provider that cannot re-fetch must not leave the flag set and be
        // asked again on every loop; and if a fetch fails, the next genuine
        // change from the source still corrects the value.
        e.needsRefresh = false;

        if (idOut  && cap)    snprintf(idOut,  cap,    "%s", e.desc.id);
        if (refOut && refCap) snprintf(refOut, refCap, "%s", e.desc.externalRef);
        if (srcOut)           *srcOut = e.desc.source;
        return true;
    }
    return false;
}

uint8_t EntityRegistry::pausedCount() const {
    std::lock_guard<std::mutex> lk(_mx);
    uint8_t n = 0;
    for (uint8_t i = 0; i < _count; i++) if (_items[i].paused) n++;
    return n;
}

bool EntityRegistry::setPaused(const char *id, bool paused, uint32_t nowMs) {
    {
        std::lock_guard<std::mutex> lk(_mx);
        const int i = indexOf(id);
        if (i < 0) return false;
        Entity &e = _items[i];
        if (e.paused == paused) return true;

        e.paused = paused;

        // A pause freezes the entity where it stands. Any optimistic write in
        // flight is abandoned rather than left to time out and be reported as
        // a failure the user did not cause - the owner's rule is that a paused
        // entity changes no state at all.
        if (paused) {
            e.pending   = false;
            e.cmdFailed = false;
        } else {
            // UNPAUSING LEAVES US HOLDING A VALUE FROM THE PAST.
            //
            // While paused we DROPPED every inbound update rather than applying
            // it, which is the whole point - but it means the held value is
            // whatever was true at the moment of the pause, and the world has
            // moved on since. Under a change-driven feed nothing will correct
            // it either: subscribe_trigger fires on CHANGE, so an occupancy
            // sensor that went busy while we were paused and has stayed busy
            // will not say so again.
            //
            // The owner found this within minutes: unpaused an occupancy card
            // he knew was occupied - confirmed on another board and in HA - and
            // it still read unoccupied.
            //
            // So resuming asks for the current value rather than waiting to be
            // told. The registry does not know HOW - it just says this entity
            // needs refreshing, and whichever provider owns it decides what
            // that means. HaRest re-fetches it; a transport that cannot
            // re-fetch simply clears the flag.
            e.needsRefresh = true;
        }

        e.lastUpdateMs = nowMs;
        e.dirty        = true;
    }

    // Rebuild and persist OUTSIDE the lock: the NVS write can take hundreds of
    // milliseconds and nothing else may be blocked behind it. See the note at
    // the top of this section.
    char list[PAUSE_BLOB_MAX];
    size_t used = 0;
    list[0] = '\0';
    {
        std::lock_guard<std::mutex> lk(_mx);
        for (uint8_t i = 0; i < _count; i++) {
            if (!_items[i].paused) continue;
            const char *eid = _items[i].desc.id;
            const size_t need = strlen(eid) + (used ? 1 : 0);
            if (used + need + 1 >= sizeof(list)) {
                // Refuse to write a truncated list rather than silently
                // forgetting a pause on the next boot.
                ESP_LOGW(ENT_TAG, "paused list full; not persisting. "
                                  "Raise PAUSE_BLOB_MAX.");
                return true;   // the in-RAM pause still stands
            }
            if (used) list[used++] = ',';
            strcpy(list + used, eid);
            used += strlen(eid);
        }
    }

    if (!savePauseList(list)) {
        ESP_LOGW(ENT_TAG, "NVS write failed; pause is live but will not "
                          "survive a reboot.");
    }
    return true;
}

void EntityRegistry::restorePaused() {
    char list[PAUSE_BLOB_MAX];
    if (!loadPauseList(list, sizeof(list)) || !list[0]) return;

    uint8_t n = 0;
    {
        std::lock_guard<std::mutex> lk(_mx);
        for (uint8_t i = 0; i < _count; i++) {
            if (listHas(list, _items[i].desc.id)) {
                _items[i].paused = true;
                _items[i].dirty  = true;
                n++;
            }
        }
    }
    if (n) ESP_LOGI(ENT_TAG, "restored %u paused entities from NVS", (unsigned)n);
}

bool EntityRegistry::setAvailable(const char *id, bool available, uint32_t nowMs) {
    std::lock_guard<std::mutex> lk(_mx);

    const int i = indexOf(id);
    if (i < 0) return false;
    Entity &e = _items[i];

    if (e.available == available) return true;   // no change, no repaint

    e.available = available;

    // The timestamp moves either way, because hearing "unavailable" IS hearing
    // from the source. Not moving it would let an entity be both unavailable
    // and stale, which says the same thing twice and badly.
    e.lastUpdateMs = nowMs;
    e.dirty        = true;
    return true;
}

bool EntityRegistry::setValue(const char *id, const EntityValue &v, uint32_t nowMs) {
    std::lock_guard<std::mutex> lk(_mx);

    const int i = indexOf(id);
    if (i < 0) return false;
    Entity &e = _items[i];

    // An authoritative value always clears a pending optimistic write: this IS
    // the echo we were waiting for. It clears even when the value disagrees
    // with what we optimistically applied - the source is right and we were
    // wrong, which is exactly the case the reconcile exists to catch.
    const bool wasPending = e.pending;
    e.pending = false;

    // A VALUE IS ITSELF PROOF OF AVAILABILITY, so recovery needs no separate
    // announcement. HA sends a real state the moment an entity comes back and
    // never sends an explicit "available" - if this were not here, anything
    // that went unavailable once would stay marked so for ever while happily
    // reporting fresh readings.
    if (!e.available) {
        e.available = true;
        e.dirty     = true;
    }

    const bool changed = !e.value.equals(v) || !e.everSet;

    // Resolve the command this echo answers. A cooperative device echoes the
    // value we optimistically applied, so `changed` is false and the entity is
    // not even dirtied - which is why the outcome has to be recorded here
    // rather than left for a listener to infer from a notification that never
    // arrives.
    //
    // The test is against prevValue, the state from BEFORE the command: an
    // echo carrying that value means the command did not take. That is correct
    // in both failure modes - a device that silently ignored us and a device
    // that actively reported it stayed put both report the old value, and both
    // mean the same thing to whoever is looking at the screen.
    if (wasPending) {
        // Compared against what we OPTIMISTICALLY APPLIED, which is e.value at
        // this moment, not against prevValue.
        //
        // prevValue is the state from before the FIRST command in a burst - it
        // is deliberately not overwritten by a second command inside the same
        // window, so the revert cannot restore a state the hardware never had.
        // That makes it the wrong baseline for "did this take": tap a switch
        // twice quickly and the value legitimately returns to prevValue, and
        // comparing against it declared a perfectly successful command failed.
        // That is the residual "Obeys can still show FAILED" case.
        e.cmdFailed = !v.equals(e.value);
    } else if (changed) {
        // An unsolicited change means we now know the current state, so an
        // older failure is history rather than news.
        e.cmdFailed = false;
    }

    e.value        = v;

    // ALWAYS: we heard from it. Issue #57 - this is the transport's clock.
    e.lastUpdateMs = nowMs;

    // ONLY ON A REAL CHANGE: this is the thing's own clock. `changed` is true
    // for the first value too, so an entity's first reading counts as its
    // first change rather than leaving this at zero and making "how long has
    // it been like this" answer "since boot".
    if (changed) e.lastChangeMs = nowMs;

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
    // Snapshot taken under the lock; the SINK is called after it is released.
    Entity snapshot;
    {
    std::lock_guard<std::mutex> lk(_mx);

    const int i = indexOf(id);
    if (i < 0) return false;
    Entity &e = _items[i];
    if (!e.desc.writable) return false;

    // A PAUSED ENTITY REFUSES COMMANDS. Issue #60.
    //
    // Not "accepts and ignores" - refuses, so the caller knows. The owner:
    // "I would expect a PAUSED button to not respond". Accepting silently
    // would leave the card showing an optimistic value for something that was
    // never sent, which is the lying-diagnostic shape all over again.
    if (e.paused) return false;

    // Remember what to fall back to. Guard against a second tap inside the
    // window overwriting prevValue with the optimistic value from the first -
    // that would make the revert restore a state the hardware never had.
    if (!e.pending) e.prevValue = e.value;

    // A fresh command supersedes the previous verdict. Leaving it set would
    // report the old failure while the new attempt is still in flight.
    e.cmdFailed      = false;

    e.value          = v;
    e.pending        = true;
    e.pendingSinceMs = nowMs;
    e.lastUpdateMs   = nowMs;
    e.everSet        = true;
    e.dirty          = true;

    snapshot         = e;
    }   // lock released here

    // OUTSIDE THE LOCK, and that placement is the point. The sink publishes or
    // calls a service, which can block on a socket for milliseconds; holding
    // the registry mutex across that would stall every provider and the LVGL
    // thread queued behind them. Issue #44.
    if (_cmdFn) _cmdFn(snapshot, v, _cmdCtx);
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
            e.value     = e.prevValue;
            e.pending   = false;
            e.cmdFailed = true;   // the echo never came: it did not take
            e.dirty     = true;
        }
    }
}
