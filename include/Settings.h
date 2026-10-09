#pragma once
//
// Settings - the owner's saved changes, in one JSON file on LittleFS (2.10d,
// DECISIONS K34; design: docs/design/card-sheet.md section 17).
//
// The board boots from the compiled dashboard and applies this file on top: a
// value comes from the first layer that sets it - this file, the card's spec,
// the page's spec, the built-in default. At 3.1 the build sheet replaces the
// spec layers and this file stays on top unchanged (ROADMAP Q2's runtime
// layer), so nothing here is a second store to reconcile later.
//
//   /cfg/settings.json, on the "spiffs" data partition (unused before 2.10d):
//   { "schema": 1,
//     "schemes":  { "<scheme>": { "selector": "silver_round" } },
//     "entities": { "<entity id>": { "paused": true } },
//     "cards":    { "<card id>": { "scenes": "all", "active": "all" } } }
//
// Rules that are load-bearing:
//   - Only what was changed is stored; a missing key means "inherit". Setting a
//     value to nullptr is the Reset (D-7): the key is removed.
//   - Choices are stored by NAME ("all"), never as enum numbers, so reordering
//     an enum cannot change what a saved choice means.
//   - Keys this firmware does not know are kept when it rewrites the file (the
//     whole document stays in RAM and is edited in place), so a file from a
//     newer firmware or the companion app survives a downgrade.
//   - A file with a newer "schema" than this firmware's is read but never
//     overwritten. A file that does not parse is kept as settings.bad.
//   - No credentials here, ever - they stay in NVS. The file is served as-is
//     at GET /settings (?stats=1: its counters). Until the web UI, a card's
//     custom name is set from a PC: /settings?card=<id>&name=<text> (an empty
//     name removes it).
//
// Writing: set*() changes RAM only. save() wakes the save task, which writes
// the whole file to settings.tmp and renames it over settings.json (a power
// cut leaves the old file) - once, and only if something changed. The task's
// stack is in internal RAM: a flash write from a task whose stack is in PSRAM
// reboots the board (LESSONS, 2.10d). The card window calls save() when it
// closes (K8).
//
// Thread safety: every call takes the store's mutex. Values are copied out;
// nothing returns a pointer into the document. No LVGL here (SystemCore owns
// it; UI code calls in).
//
#include <stddef.h>
#include <stdint.h>

class HttpServer;

namespace Settings {

// Mount, read, start the save task, register GET /settings. Before anything
// reads a setting - SystemCore::begin(). Without a usable partition the store
// still works, in RAM only, and says so.
bool begin(HttpServer &http);
bool persistent();   // false: no partition, or it would not mount

// Reading. True and the value copied into `out` when the file sets it; false
// (out = "") when it does not - inherit.
bool card(const char *cardId, const char *key, char *out, size_t cap);
bool scheme(const char *schemeName, const char *key, char *out, size_t cap);
// Entity flags (Paused). `dflt` when the file says nothing.
bool entityFlag(const char *entityId, const char *key, bool dflt);
// Every entity whose `key` is true, for a restore at boot. Calls fn(id, ctx).
void forEachEntityFlag(const char *key, void (*fn)(const char *entityId, void *ctx), void *ctx);

// Writing, in RAM. nullptr = Reset (removes the key, and the card's or
// scheme's object once empty). True when the document changed.
bool setCard(const char *cardId, const char *key, const char *value);
bool setScheme(const char *schemeName, const char *key, const char *value);
bool setEntityFlag(const char *entityId, const char *key, bool value, bool dflt);

// SAVED SETTINGS NOBODY CLAIMS (K35). A card's entry whose id is on no page,
// an entity's whose id is not in the table. Never removed on their own: listed
// (forEachUnclaimed(), the System Doctor's [SETTINGS]) and cleared only when
// asked - GET /settings?prune=cards, or prune=entities (a group's member is
// only learnt once HA reports the group, so right after boot a paused member
// can look missing). The claims come from whoever knows the cards and the
// entities - GUIManager - as two predicates; until set, everything is claimed.
using ClaimFn = bool (*)(const char *id);
void setClaims(ClaimFn card, ClaimFn entity);
void forEachUnclaimed(void (*fn)(const char *section, const char *id, void *ctx), void *ctx);
uint8_t prune(const char *section);   // "cards" or "entities"; asks for a save

// One-time imports, so each happens once per file: "imported": {"<name>": true}.
bool imported(const char *name);
void markImported(const char *name);

// Ask the save task to write the file, `delayMs` from now, if anything has
// changed since the last write. Cheap; call it freely.
void save(uint32_t delayMs = 0);

struct Stats {
    uint32_t saves;        // files written since boot
    uint32_t failures;     // writes that failed (the change stays pending)
    uint32_t lastMs;       // millis() of the last write, 0 if none
    uint32_t lastUs;       // how long it took
    uint32_t bytes;        // the file's size at the last write or read
    bool     pending;      // a change not yet written
};
Stats stats();

}  // namespace Settings
