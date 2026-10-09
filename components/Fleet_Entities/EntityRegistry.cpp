#include "EntityRegistry.h"
#include "LightColor.h"

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

// ESP-IDF logging rather than Serial: this translation unit deliberately pulls
// in no Arduino header, which is the point of using nvs.h here in the first
// place. The output lands on the same console either way.
static const char *ENT_TAG = "Entities";

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
// Groups defined at the source - 2.10c (#65)
// ---------------------------------------------------------------------------

// "light.office_lamp" -> "ha_light_office_lamp": stable across reboots, since
// it is made from the source's own id, so a saved pause (and, at 2.10d, a saved
// setting) finds the member again. A clash, which needs two refs that differ
// only after the 36th character, gets a digit.
static void learntId(const char *ref, char *out, size_t cap) {
    size_t n = (size_t)snprintf(out, cap, "ha_");
    for (const char *p = ref; *p && n < cap - 1; p++) out[n++] = (*p == '.') ? '_' : *p;
    out[n] = '\0';
}

uint8_t EntityRegistry::learnMembers(const char *groupId, const char *const *refs, uint8_t n) {
    std::lock_guard<std::mutex> lk(_mx);
    const int gi = indexOf(groupId);
    if (gi < 0 || !refs) return 0;
    Entity &g = _items[gi];
    return learnInto(gi, refs, nullptr, nullptr, n, g.desc.kind, g.desc.valueType,
                     g.members, g.nMembers, ENTITY_MEMBERS_MAX);
}

uint8_t EntityRegistry::learnScenes(const char *lightId, const char *const *refs,
                                    const char *const *names, const bool *hidden, uint8_t n) {
    std::lock_guard<std::mutex> lk(_mx);
    const int li = indexOf(lightId);
    if (li < 0 || !refs) return 0;
    Entity &l = _items[li];
    return learnInto(li, refs, names, hidden, n, EntityKind::BUTTON, ValueType::TEXT_VAL,
                     l.scenes, l.nScenes, ENTITY_SCENES_MAX);
}

uint8_t EntityRegistry::learnInto(int gi, const char *const *refs, const char *const *names,
                                  const bool *hidden, uint8_t n, EntityKind kind, ValueType vt,
                                  uint8_t *dst, uint8_t &dstN, uint8_t max) {
    Entity &g = _items[gi];
    const char *groupId = g.desc.id;

    uint8_t idx[ENTITY_SCENES_MAX > ENTITY_MEMBERS_MAX ? ENTITY_SCENES_MAX : ENTITY_MEMBERS_MAX];
    if (max > sizeof(idx)) max = sizeof(idx);
    uint8_t k = 0, added = 0;
    for (uint8_t r = 0; r < n && k < max; r++) {
        const char *ref = refs[r];
        if (!ref || !ref[0]) continue;

        int m = -1;
        for (uint8_t i = 0; i < _count; i++) {
            if (_items[i].desc.source == g.desc.source &&
                strcmp(_items[i].desc.externalRef, ref) == 0) { m = i; break; }
        }
        if (m == gi) continue;   // a group never lists itself, but never loop
        if (m < 0) {
            if (_count >= _capacity) {
                ESP_LOGW(ENT_TAG, "table full; member %s of %s not learnt", ref, groupId);
                break;
            }
            EntityDescriptor d;
            learntId(ref, d.id, sizeof(d.id));
            for (uint8_t tries = 0; indexOf(d.id) >= 0 && tries < 9; tries++) {
                const size_t len = strlen(d.id);
                if (len >= sizeof(d.id) - 1) d.id[len - 1] = (char)('1' + tries);
                else { d.id[len] = (char)('1' + tries); d.id[len + 1] = '\0'; }
            }
            if (indexOf(d.id) >= 0) continue;
            // Its name: the one given (a scene's), else its ref until the
            // source says otherwise (adoptName).
            const char *nm = (names && names[r] && names[r][0]) ? names[r] : ref;
            snprintf(d.name, sizeof(d.name), "%s", nm);
            snprintf(d.externalRef, sizeof(d.externalRef), "%s", ref);
            d.kind      = kind;
            d.source    = g.desc.source;
            d.valueType = vt;
            d.writable  = g.desc.writable;
            d.advertise = false;   // someone else's, like the group

            // The slot is complete before the count admits it - see count().
            const uint8_t slot = _count;
            Entity &e = _items[slot];
            e = Entity{};
            e.desc       = d;
            e.value.type = d.valueType;
            e.learnt     = true;
            _count.store((uint8_t)(slot + 1), std::memory_order_release);
            m = slot;
            added++;
            ESP_LOGI(ENT_TAG, "learnt %s (%s) for %s", d.id, ref, groupId);
        }
        // Hidden in the source's UI: may change between sessions, so set on
        // every learning, new or not. The LVGL thread reads it unlocked; a
        // bool is written whole.
        if (hidden && _items[m].sourceHidden != hidden[r]) {
            _items[m].sourceHidden = hidden[r];
            g.dirty = true;
        }
        idx[k++] = (uint8_t)m;
    }

    // The list first, then its length, for a reader on the LVGL thread.
    if (k != dstN || memcmp(idx, dst, k) != 0) {
        memcpy(dst, idx, k);
        dstN    = k;
        g.dirty = true;
    }
    if (added) _learnt.store(true);
    return added;
}

bool EntityRegistry::press(const char *id, uint32_t nowMs) {
    Entity snapshot;
    {
        std::lock_guard<std::mutex> lk(_mx);
        const int i = indexOf(id);
        if (i < 0) return false;
        Entity &e = _items[i];
        if (e.desc.kind != EntityKind::BUTTON || !e.desc.writable || e.paused) return false;
        e.cmdFailed    = false;
        e.lastUpdateMs = nowMs;
        snapshot = e;
    }   // the sink may block on a socket (#44)
    if (_cmdFn) _cmdFn(snapshot, EntityValue::makeBool(true), _cmdCtx);
    return true;
}

bool EntityRegistry::adoptName(const char *id, const char *name) {
    if (!name || !name[0]) return false;
    std::lock_guard<std::mutex> lk(_mx);
    const int i = indexOf(id);
    if (i < 0 || !_items[i].learnt) return false;
    Entity &e = _items[i];
    // Only over the placeholder: once named, by the source or at learning
    // (a scene's "Relax", where friendly_name says "Office Relax"), it stays.
    if (strcmp(e.desc.name, e.desc.externalRef) != 0) return false;
    snprintf(e.desc.name, sizeof(e.desc.name), "%s", name);
    e.dirty = true;
    return true;
}

bool EntityRegistry::setSourceName(const char *id, const char *name) {
    if (!name || !name[0]) return false;
    std::lock_guard<std::mutex> lk(_mx);
    const int i = indexOf(id);
    if (i < 0) return false;
    Entity &e = _items[i];
    if (strncmp(e.sourceName, name, sizeof(e.sourceName) - 1) == 0) return false;
    snprintf(e.sourceName, sizeof(e.sourceName), "%s", name);
    e.dirty = true;
    return true;
}

bool EntityRegistry::setSourceArea(const char *id, const char *areaId, const char *area) {
    if (!area) area = "";
    if (!areaId) areaId = "";
    std::lock_guard<std::mutex> lk(_mx);
    const int i = indexOf(id);
    if (i < 0) return false;
    Entity &e = _items[i];
    if (strncmp(e.sourceArea, area, sizeof(e.sourceArea) - 1) == 0 &&
        strncmp(e.sourceAreaId, areaId, sizeof(e.sourceAreaId) - 1) == 0) return false;
    snprintf(e.sourceArea, sizeof(e.sourceArea), "%s", area);
    snprintf(e.sourceAreaId, sizeof(e.sourceAreaId), "%s", areaId);
    e.dirty = true;
    return true;
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
//
// Since 2.10d the list goes through a PauseStore when one is set: SystemCore
// keeps it in the owner's settings file, written by the settings task. The NVS
// functions below are the default and the one-time import's source.

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

bool EntityRegistry::readNvsPauseList(char *out, size_t cap) { return loadPauseList(out, cap); }

// Through the store SystemCore set, else NVS as before (2.10d).
bool EntityRegistry::loadPauses(char *out, size_t cap) const {
    if (_pauseStore.load) return _pauseStore.load(out, cap, _pauseStore.ctx);
    return loadPauseList(out, cap);
}

bool EntityRegistry::savePauses(const char *list) const {
    if (_pauseStore.save) return _pauseStore.save(list, _pauseStore.ctx);
    return savePauseList(list);
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
            e.pending     = false;
            e.attrPending = false;
            e.cmdFailed   = false;
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

        // A GROUP DEFINED AT THE SOURCE IS PAUSED EXACTLY WHEN ALL ITS MEMBERS
        // ARE (2.10c round 9). Pausing the group pauses its members (K18);
        // this is the other direction. Without it the group's own flag could
        // outlive its members' - set from the group's window, then each
        // member resumed from its own - and the card stayed PAUSED with every
        // member running (owner). One level: the groups this entity is in.
        for (uint8_t g = 0; g < _count; g++) {
            Entity &G = _items[g];
            if (!G.nMembers) continue;
            bool isMember = false, all = true;
            for (uint8_t k = 0; k < G.nMembers; k++) {
                if (G.members[k] == (uint8_t)i) isMember = true;
                if (G.members[k] >= _count || !_items[G.members[k]].paused) all = false;
            }
            if (!isMember || G.paused == all) continue;
            G.paused = all;
            if (all) { G.pending = false; G.attrPending = false; G.cmdFailed = false; }
            else     G.needsRefresh = true;
            G.lastUpdateMs = nowMs;
            G.dirty        = true;
        }
    }

    // Rebuild and persist OUTSIDE the lock: the NVS write can take hundreds of
    // milliseconds and nothing else may be blocked behind it. See the note at
    // the top of this section.
    char list[PAUSE_BLOB_MAX];
    size_t used = 0;
    list[0] = '\0';
    // A PAUSE FOR AN ENTITY NOT LEARNT YET (2.10c) must survive this rewrite:
    // a member of an HA group is registered only once its group reports, so
    // for the first seconds after boot its id is in the saved list but not in
    // the table. Those ids are carried over from what was saved.
    char saved[PAUSE_BLOB_MAX];
    loadPauses(saved, sizeof(saved));
    {
        std::lock_guard<std::mutex> lk(_mx);
        for (const char *p = saved; *p; ) {
            const char *end = strchr(p, ',');
            const size_t len = end ? (size_t)(end - p) : strlen(p);
            char eid[ENTITY_ID_MAX];
            if (len && len < sizeof(eid)) {
                memcpy(eid, p, len);
                eid[len] = '\0';
                if (indexOf(eid) < 0 && used + len + 2 < sizeof(list)) {
                    if (used) list[used++] = ',';
                    memcpy(list + used, eid, len + 1);
                    used += len;
                }
            }
            if (!end) break;
            p = end + 1;
        }
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

    if (!savePauses(list)) {
        ESP_LOGW(ENT_TAG, "pause store write failed; pause is live but will "
                          "not survive a reboot.");
    }
    return true;
}

void EntityRegistry::restorePaused() {
    char list[PAUSE_BLOB_MAX];
    if (!loadPauses(list, sizeof(list)) || !list[0]) return;

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
    if (n) ESP_LOGI(ENT_TAG, "restored %u paused entities", (unsigned)n);
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

// Does a report carry what a light command asked for? Within the rounding
// HA's own conversions bring: brightness through percent, kelvin through
// mireds, hue and saturation through xy for a Hue bulb.
static bool lightAttrsMatch(const EntityAttrs &a, const LightCommand &c) {
    auto near = [](int x, int y, int tol) { return (x > y ? x - y : y - x) <= tol; };
    if (c.brightness > 0 && !near(a.brightness, c.brightness, 3)) return false;
    if (c.hue >= 0) {
        if (a.lightMode != LightMode::LMODE_COLOUR || a.hue < 0) return false;
        const int dh = ((a.hue - c.hue) % 360 + 360) % 360;
        if (dh > 3 && dh < 357) return false;
        if (c.sat >= 0 && !near(a.sat, c.sat, 3)) return false;
    } else if (c.colorTempK > 0) {
        if (a.lightMode != LightMode::LMODE_TEMP) return false;
        if (!near(a.colorTempK, c.colorTempK, c.colorTempK * 3 / 100)) return false;
    }
    return true;
}

bool EntityRegistry::setAttrs(const char *id, const EntityAttrs &a) {
    std::lock_guard<std::mutex> lk(_mx);

    const int i = indexOf(id);
    if (i < 0) return false;
    Entity &e = _items[i];

    // A paused entity holds still - attributes included, or a paused light's
    // fill would go on moving under a PAUSED badge. Issue #60's rule. EXCEPT
    // its first reading: a pause restored at boot reaches the entity before
    // HaRest's fetch, and setValue() takes that fetch's value - so the levels
    // come with it, rather than a paused light that is "on" at no brightness
    // (2.10c, a paused member of light.office after a reboot).
    if (e.paused && e.everSet) return false;

    // A LIGHT COMMAND IN FLIGHT (2.10b): only a report carrying the latest
    // commanded levels ends it - the same rule #63 gave the value. Anything
    // else is the source's current word, kept as the fall-back; the screen
    // keeps showing what the finger asked for. The icon and what the light can
    // do are not levels, and follow the source at once.
    if (e.attrPending) {
        if (!lightAttrsMatch(a, e.attrCmd)) {
            e.prevAttrs = a;
            bool changed = false;
            if (strcmp(e.attrs.icon, a.icon) != 0) { memcpy(e.attrs.icon, a.icon, sizeof(a.icon)); changed = true; }
            if (e.attrs.lightCaps != a.lightCaps)  { e.attrs.lightCaps = a.lightCaps; changed = true; }
            if (changed) e.dirty = true;
            return changed;
        }
        e.attrPending = false;   // confirmed; take the report as it is
    }

    if (e.attrs.equals(a)) return false;
    e.attrs = a;
    e.dirty = true;
    return true;
}

bool EntityRegistry::setValue(const char *id, const EntityValue &v, uint32_t nowMs) {
    std::lock_guard<std::mutex> lk(_mx);

    const int i = indexOf(id);
    if (i < 0) return false;
    Entity &e = _items[i];

    // A VALUE IS ITSELF PROOF OF AVAILABILITY, so recovery needs no separate
    // announcement. HA sends a real state the moment an entity comes back and
    // never sends an explicit "available" - if this were not here, anything
    // that went unavailable once would stay marked so for ever while happily
    // reporting fresh readings.
    if (!e.available) {
        e.available = true;
        e.dirty     = true;
    }

    // RESOLVE A COMMAND IN FLIGHT - and only a MATCHING echo resolves it. #63.
    //
    // This used to treat any echo as the verdict: one carrying something other
    // than what we optimistically applied meant "it did not take". True for an
    // instant device, and wrong for a TRANSITIONING one. A light fading from
    // on to off goes on reporting "on" for the whole fade - HA sends a state
    // event per brightness step - so the first echo after the tap looked like
    // a refusal, the card said FAILED, and it corrected itself when the fade
    // finished. The owner's "Desk" group did exactly this.
    //
    // So a non-matching echo is no longer evidence. It is recorded as what the
    // source currently says (the revert target, should the window expire) and
    // the command stays pending. Only two things end it: an echo carrying the
    // commanded value, or the reconcile window running out in tick(). The cost
    // is the speed of a genuine failure report - it now takes the window rather
    // than one round trip - and nothing depends on that speed.
    //
    // Compared against e.value, which IS the optimistic value while pending -
    // not against prevValue. A second tap inside the window can legitimately
    // command the value back to prevValue, and comparing against it declared a
    // perfectly successful command failed ("Obeys can still show FAILED").
    if (e.pending) {
        if (!v.equals(e.value)) {
            // Replacing prevValue here does not break the rule commandValue()
            // keeps - "never revert to a state the hardware never had" - it
            // honours it: this is the hardware's own report, which is exactly
            // the state a revert should land on.
            e.prevValue    = v;
            e.lastUpdateMs = nowMs;
            e.everSet      = true;
            return false;   // the display keeps the commanded value meanwhile
        }

        // Confirmed. The commanded value is already displayed, so nothing is
        // dirtied below - but it IS a change of state, and lastChangeMs must
        // say so or "on since 6:03" would read from before the tap.
        e.pending   = false;
        e.cmdFailed = false;
        if (!v.equals(e.prevValue)) e.lastChangeMs = nowMs;
    }

    const bool changed = !e.value.equals(v) || !e.everSet;

    if (changed) {
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

bool EntityRegistry::commandLight(const char *id, const LightCommand &c, uint32_t nowMs) {
    Entity snapshot;
    {
    std::lock_guard<std::mutex> lk(_mx);

    const int i = indexOf(id);
    if (i < 0) return false;
    Entity &e = _items[i];
    if (!e.desc.writable || e.paused) return false;   // as commandValue()

    // --- On or off: the value's own bookkeeping ---------------------------
    const bool wantOn = c.wantsOn();
    if (c.on >= 0 || wantOn) {
        if (!e.pending) e.prevValue = e.value;
        e.value          = EntityValue::makeBool(wantOn);
        e.pending        = true;
        e.pendingSinceMs = nowMs;
        e.cmdFailed      = false;
    }

    // --- The levels -------------------------------------------------------
    // Applied as HA would report them once the light has them, so the card
    // and the popup draw the result at once.
    const bool levels = c.brightness > 0 || c.colorTempK > 0 || c.hue >= 0;
    if (levels && wantOn) {
        if (!e.attrPending) { e.prevAttrs = e.attrs; e.attrCmd = LightCommand(); }
        EntityAttrs &a = e.attrs;
        LightCommand &pc = e.attrCmd;
        if (c.brightness > 0) { a.brightness = c.brightness; pc.brightness = c.brightness; }
        if (c.hue >= 0) {
            a.hue = c.hue;
            a.sat = (c.sat >= 0) ? c.sat : (a.sat >= 0 ? a.sat : 100);
            a.colorTempK = -1;                         // HA: null in a colour mode
            a.lightMode  = LightMode::LMODE_COLOUR;
            pc.hue = c.hue; pc.sat = a.sat; pc.colorTempK = -1;
        } else if (c.colorTempK > 0) {
            a.colorTempK = c.colorTempK;
            lightKelvinToHs(c.colorTempK, a.hue, a.sat);   // HA sends hs_color in every mode
            a.lightMode  = LightMode::LMODE_TEMP;
            pc.colorTempK = c.colorTempK; pc.hue = -1; pc.sat = -1;
        }
        if (a.lightMode == LightMode::LMODE_UNKNOWN || a.lightMode == LightMode::LMODE_ONOFF)
            a.lightMode = LightMode::LMODE_DIM;
        a.rgb    = lightShownRgb(a);
        a.hasRgb = (a.rgb != 0);
        e.attrPending        = true;
        e.attrPendingSinceMs = nowMs;
        e.cmdFailed          = false;
    }

    e.lastUpdateMs = nowMs;
    e.everSet      = true;
    e.dirty        = true;
    snapshot       = e;
    }   // lock released: the sink may block on a socket (#44)

    if (_lightFn) _lightFn(snapshot, c, _lightCtx);
    return true;
}

bool EntityRegistry::failCommand(const char *id, const char *why) {
    std::lock_guard<std::mutex> lk(_mx);
    const int i = indexOf(id);
    if (i < 0) return false;
    Entity &e = _items[i];
    // A pressed button (a scene) waits for nothing, so its refusal is
    // reported however long after the press it comes.
    if (!e.pending && !e.attrPending && e.desc.kind != EntityKind::BUTTON) return false;
    if (e.pending)     { e.value = e.prevValue; e.pending = false; }
    if (e.attrPending) { e.attrs = e.prevAttrs; e.attrPending = false; }
    e.cmdFailed = true;
    e.dirty     = true;
    noteFail(e, why, e.lastUpdateMs);
    return true;
}

void EntityRegistry::noteFail(const Entity &e, const char *why, uint32_t nowMs) {
    FailNote &n = _fails[_failNext];
    _failNext = (uint8_t)((_failNext + 1) % FAIL_NOTES);
    snprintf(n.id,  sizeof(n.id),  "%s", e.desc.id);
    snprintf(n.why, sizeof(n.why), "%s", why ? why : "?");
    n.atMs = nowMs;
    _failTotal++;
    ESP_LOGW(ENT_TAG, "FAILED %s: %s", n.id, n.why);
}

uint8_t EntityRegistry::failNotes(FailNote *out, uint8_t cap) const {
    std::lock_guard<std::mutex> lk(_mx);
    uint8_t n = 0;
    for (uint8_t k = 1; k <= FAIL_NOTES && n < cap; k++) {
        const FailNote &f = _fails[(_failNext + FAIL_NOTES - k) % FAIL_NOTES];
        if (f.id[0]) out[n++] = f;
    }
    return n;
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
            char why[80];
            snprintf(why, sizeof(why), "no matching report in %lu ms (on/off; last said %s)",
                     (unsigned long)_reconcileMs,
                     e.prevValue.type == ValueType::BOOL ? (e.prevValue.b ? "on" : "off") : "?");
            e.value     = e.prevValue;
            e.pending   = false;
            e.cmdFailed = true;   // the echo never came: it did not take
            e.dirty     = true;
            noteFail(e, why, nowMs);
        }
        // The same for a light's levels: back to what the source last said.
        // Recorded with both, since a near miss and no report at all look the
        // same on the card (G8).
        if (e.attrPending && (nowMs - e.attrPendingSinceMs) > _reconcileMs) {
            const LightCommand &c = e.attrCmd;
            const EntityAttrs  &a = e.prevAttrs;
            char why[80];
            snprintf(why, sizeof(why), "no match in %lus: asked b%d k%d h%d; last b%d k%d h%d m%d",
                     (unsigned long)(_reconcileMs / 1000), c.brightness, c.colorTempK, c.hue,
                     a.brightness, a.colorTempK, a.hue, (int)a.lightMode);
            e.attrs       = e.prevAttrs;
            e.attrPending = false;
            e.cmdFailed   = true;
            e.dirty       = true;
            noteFail(e, why, nowMs);
        }
    }
}
