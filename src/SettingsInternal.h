#pragma once
//
// SettingsInternal - what SettingsHttp.cpp needs from the store, and nothing
// else uses. Private to src/ (not in include/): the store's state stays in
// Settings.cpp, and the PC's addresses reach it through these few calls.
//
#include <stddef.h>
#include <stdint.h>

class HttpServer;

namespace Settings {
namespace detail {

constexpr size_t FILE_MAX = 64 * 1024;   // a settings file larger than this is refused

// GET /settings and POST /settings. Called from Settings::begin().
void registerRoutes(HttpServer &http);

// ?stats=1's text up to the unclaimed entries: the file, its counters, the
// boot note, the files in /cfg (as the settings task last listed them) and the
// last changes. Returns its length.
int statusText(char *out, size_t cap);

// The whole document, pretty-printed, in PSRAM; the caller frees it with
// heap_caps_free(). nullptr when out of memory.
char *serialiseCopy(size_t &len);

// True when this boot writes the settings file; else why not, into `why`.
bool writable(char *why, size_t cap);

// Replace the whole document with `json` (a restore, A17): checked - JSON, an
// object, a schema this firmware knows - then written by the settings task,
// waited for (up to 5 s). Returns an HTTP status (200, 400, 409, 500) and one
// line in `msg` saying what happened.
int restore(const char *json, size_t len, char *msg, size_t cap);

}  // namespace detail
}  // namespace Settings
