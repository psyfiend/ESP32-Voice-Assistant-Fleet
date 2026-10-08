// Settings - see Settings.h.
#include "Settings.h"

#include <Arduino.h>   // Serial, millis()
#include <ctype.h>
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <new>         // placement new into PSRAM
#include <dirent.h>
#include <sys/stat.h>
#include <ArduinoJson.h>   // header-only, plain C++ - nothing Arduino-specific is used
#include "esp_heap_caps.h"
#include "esp_littlefs.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "HttpServer.h"

namespace {

constexpr int         SCHEMA        = 1;
constexpr const char *BASE          = "/cfg";
constexpr const char *PARTITION     = "spiffs";   // the 16 MB layout's data partition
constexpr const char *PATH          = "/cfg/settings.json";
constexpr const char *TMP_PATH      = "/cfg/settings.tmp";
constexpr const char *BAD_PATH      = "/cfg/settings.bad";
constexpr size_t      FILE_MAX      = 64 * 1024;  // refuse anything larger at read
constexpr size_t      CARD_ID_ARG   = 48;         // CARD_ID_MAX (PageSpec.h) + 1

// The document lives in PSRAM: it is small, but internal RAM is the scarce
// thing on this fleet (LESSONS) and nothing here is on a hot path.
struct PsramAllocator : ArduinoJson::Allocator {
    void *allocate(size_t n) override { return heap_caps_malloc(n, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT); }
    void deallocate(void *p) override { heap_caps_free(p); }
    void *reallocate(void *p, size_t n) override {
        return heap_caps_realloc(p, n, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    }
};
PsramAllocator s_alloc;
JsonDocument  *s_doc = nullptr;

SemaphoreHandle_t s_mutex   = nullptr;
TaskHandle_t      s_task    = nullptr;
volatile uint32_t s_delayMs = 0;

char     s_bootNote[96] = "not started";   // what readFile() found, for ?stats=1
char     s_listing[256] = "";              // the files in BASE, as of boot or the last save
bool     s_mounted  = false;
bool     s_readOnly = false;   // a newer schema, or a file that would not read: never overwritten
bool     s_dirty    = false;
uint32_t s_saves = 0, s_failures = 0, s_lastMs = 0, s_lastUs = 0, s_bytes = 0;

struct Lock {
    Lock()  { xSemaphoreTake(s_mutex, portMAX_DELAY); }
    ~Lock() { xSemaphoreGive(s_mutex); }
};

void copyOut(JsonVariantConst v, char *out, size_t cap, bool &found) {
    found = v.is<const char *>();
    if (cap) snprintf(out, cap, "%s", found ? v.as<const char *>() : "");
}

// ArduinoJson 7: obj[key].to<JsonObject>() REPLACES what is there, so the
// section and the entry are found or made explicitly, never through to<>().
JsonObject objectAt(JsonObject parent, const char *key) {
    JsonVariant v = parent[key];
    if (v.is<JsonObject>()) return v.as<JsonObject>();
    return parent[key].to<JsonObject>();
}

// Set or remove doc[section][id][key]; drops [id], then [section], once empty.
// Caller holds the lock. True when the document changed.
bool setValue(const char *section, const char *id, const char *key, const char *str,
              bool isBool, bool b, bool remove) {
    if (!s_doc || !id || !*id || !key || !*key) return false;
    JsonObject root = s_doc->as<JsonObject>();
    if (remove) {
        JsonVariant secV = root[section];
        if (!secV.is<JsonObject>()) return false;
        JsonObject sec = secV.as<JsonObject>();
        JsonVariant entV = sec[id];
        if (!entV.is<JsonObject>()) return false;
        JsonObject ent = entV.as<JsonObject>();
        if (ent[key].isNull()) return false;
        ent.remove(key);
        if (ent.size() == 0) sec.remove(id);
        if (sec.size() == 0) root.remove(section);
        return true;
    }
    JsonObject ent = objectAt(objectAt(root, section), id);
    JsonVariant cur = ent[key];
    if (isBool) {
        if (cur.is<bool>() && cur.as<bool>() == b) return false;
        ent[key] = b;
    } else {
        if (cur.is<const char *>() && strcmp(cur.as<const char *>(), str) == 0) return false;
        ent[key] = str;   // copied into the document
    }
    return true;
}

// Serialise under the lock into a PSRAM buffer; the caller frees it.
char *serialise(size_t &len) {
    len = measureJsonPretty(*s_doc);
    char *buf = (char *)heap_caps_malloc(len + 1, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (buf) serializeJsonPretty(*s_doc, buf, len + 1);
    return buf;
}

// The files in BASE with their sizes, into s_listing. Reads the flash, so only
// from a task whose stack is in internal RAM: begin()'s caller or the save task.
void listFiles() {
    char buf[sizeof(s_listing)];
    int n = 0;
    buf[0] = '\0';
    if (DIR *d = s_mounted ? opendir(BASE) : nullptr) {
        while (struct dirent *de = readdir(d)) {
            char p[96];
            snprintf(p, sizeof(p), "%s/%s", BASE, de->d_name);
            struct stat st = {};
            stat(p, &st);
            if (n < (int)sizeof(buf) - 48)
                n += snprintf(buf + n, sizeof(buf) - n, "  %-20s %ld bytes\n", de->d_name, (long)st.st_size);
        }
        closedir(d);
    }
    Lock l;
    snprintf(s_listing, sizeof(s_listing), "%s", buf);
}

void writeIfDirty() {
    size_t len = 0;
    char *buf = nullptr;
    {
        Lock l;
        if (!s_dirty || s_readOnly || !s_mounted) return;
        buf = serialise(len);
        if (!buf) { s_failures++; return; }
        s_dirty = false;   // a change made while writing marks it again
    }
    const int64_t t0 = esp_timer_get_time();
    bool ok = false;
    FILE *f = fopen(TMP_PATH, "w");
    if (f) {
        ok = fwrite(buf, 1, len, f) == len;
        ok = (fclose(f) == 0) && ok;
    }
    if (ok && rename(TMP_PATH, PATH) != 0) {
        // Some VFS layers refuse to rename over an existing file.
        remove(PATH);
        ok = rename(TMP_PATH, PATH) == 0;
    }
    const uint32_t us = (uint32_t)(esp_timer_get_time() - t0);
    heap_caps_free(buf);
    {
        Lock l;
        if (ok) { s_saves++; s_lastMs = millis(); s_lastUs = us; s_bytes = (uint32_t)len; }
        else    { s_failures++; s_dirty = true; }
    }
    listFiles();
    if (ok) Serial.printf("[Settings] saved %u bytes in %.1f ms\n", (unsigned)len, us / 1000.0);
    else    Serial.printf("[Settings] SAVE FAILED (%u bytes) - the change stays in RAM\n", (unsigned)len);
}

void saveTask(void *) {
    for (;;) {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        const uint32_t d = s_delayMs;
        if (d) vTaskDelay(pdMS_TO_TICKS(d));
        writeIfDirty();
    }
}

// One line about what the file was at boot: printed, and kept for ?stats=1.
void note(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(s_bootNote, sizeof(s_bootNote), fmt, ap);
    va_end(ap);
    Serial.printf("[Settings] %s\n", s_bootNote);
}

// A FILE THAT EXISTS BUT CANNOT BE READ IS NEVER OVERWRITTEN. 2026-10-08: one
// boot of WS_P4_5 started with an empty store although the file was there (no
// settings.bad, so it was not bad JSON), and the next save replaced it - a
// card's saved Scenes was lost. Whatever the cause, the cure is the same:
// such a boot is read-only, and says so.
void readFile() {
    errno = 0;
    FILE *f = fopen(PATH, "r");
    if (!f) {
        if (errno == ENOENT) { note("no settings file yet - every value is the dashboard's"); return; }
        s_readOnly = true;
        note("settings file did not open (errno %d) - read-only this boot, the file is kept", errno);
        return;
    }
    fseek(f, 0, SEEK_END);
    const long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (size <= 0 || (size_t)size > FILE_MAX) {
        fclose(f);
        note("settings file is %ld bytes - ignored, kept as %s", size, BAD_PATH);
        remove(BAD_PATH);
        rename(PATH, BAD_PATH);
        return;
    }
    char *buf = (char *)heap_caps_malloc((size_t)size + 1, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    const size_t got = buf ? fread(buf, 1, (size_t)size, f) : 0;
    fclose(f);
    if (!buf || got != (size_t)size) {
        heap_caps_free(buf);
        s_readOnly = true;
        note("could not read the file (%u of %ld bytes) - read-only this boot, the file is kept", (unsigned)got, size);
        return;
    }
    buf[size] = '\0';
    const DeserializationError err = deserializeJson(*s_doc, buf, (size_t)size);
    heap_caps_free(buf);
    if (err || !s_doc->is<JsonObject>()) {
        note("settings file does not parse (%s) - kept as %s, starting empty",
             err ? err.c_str() : "not an object", BAD_PATH);
        s_doc->clear();
        s_doc->to<JsonObject>();
        remove(BAD_PATH);
        rename(PATH, BAD_PATH);
        return;
    }
    s_bytes = (uint32_t)size;
    const int schema = (*s_doc)["schema"] | 0;
    if (schema > SCHEMA) {
        s_readOnly = true;
        Serial.printf("[Settings] file is schema %d, this firmware knows %d - read, never overwritten\n",
                      schema, SCHEMA);
    }
    const size_t cards = (*s_doc)["cards"].size(), schemes = (*s_doc)["schemes"].size(),
                 ents = (*s_doc)["entities"].size();
    note("%s: %ld bytes - %u card(s), %u scheme(s), %u entit%s", PATH, size,
         (unsigned)cards, (unsigned)schemes, (unsigned)ents, ents == 1 ? "y" : "ies");
}

// %XX and '+' in a query value, in place. No escapes are written anywhere.
void urlDecode(char *s) {
    char *o = s;
    for (const char *p = s; *p; p++) {
        if (*p == '+') { *o++ = ' '; continue; }
        if (*p == '%' && isxdigit((unsigned char)p[1]) && isxdigit((unsigned char)p[2])) {
            const char hex[3] = { p[1], p[2], 0 };
            *o++ = (char)strtol(hex, nullptr, 16);
            p += 2;
            continue;
        }
        *o++ = *p;
    }
    *o = '\0';
}

// The panels draw ASCII and the degree sign only (CLAUDE.md): anything else
// would show as boxes, so a name is refused rather than stored broken.
bool printableAscii(const char *s) {
    for (; *s; s++) if ((unsigned char)*s < 0x20 || (unsigned char)*s > 0x7E) return false;
    return true;
}

esp_err_t handleSettings(httpd_req_t *req) {
    char q[160], v[4];
    bool stats = false;
    const bool haveQ = httpd_req_get_url_query_str(req, q, sizeof(q)) == ESP_OK;
    if (haveQ && httpd_query_key_value(q, "stats", v, sizeof(v)) == ESP_OK)
        stats = atoi(v) != 0;

    // A card's custom name, from a PC (K37, until the web UI): name= sets it,
    // an empty name= removes it (the Reset). Kept and written like any other
    // change; the card shows it the next time its page is built.
    char id[CARD_ID_ARG], name[64];
    // The other card settings too, as SETTINGS' rows would set them - for
    // tests from a PC: label=, tap=, tap_scene= (the scene's HA id). The
    // value is not checked against the choices here; a name the firmware
    // does not know reads as "not set". An empty value removes the key.
    static const char *const KEYS[] = { "label", "tap", "tap_scene" };
    for (const char *key : KEYS) {
        if (!haveQ || httpd_query_key_value(q, "card", id, sizeof(id)) != ESP_OK ||
            httpd_query_key_value(q, key, name, sizeof(name)) != ESP_OK) continue;
        urlDecode(name);
        const bool reset = !name[0] || !strcmp(name, "inherit") || !strcmp(name, "default");
        Settings::setCard(id, key, reset ? nullptr : name);
        Settings::save();
        char out[160];
        const int n = snprintf(out, sizeof(out), "%s: %s %s - in effect when its page is next built\n",
                               id, key, reset ? "removed" : name);
        httpd_resp_set_type(req, "text/plain");
        return httpd_resp_send(req, out, n);
    }
    if (haveQ && httpd_query_key_value(q, "card", id, sizeof(id)) == ESP_OK &&
        httpd_query_key_value(q, "name", name, sizeof(name)) == ESP_OK) {
        urlDecode(name);
        char out[200];
        int n;
        if (strlen(name) >= 40 || !printableAscii(name)) {
            n = snprintf(out, sizeof(out), "refused: a name is at most 39 characters of plain ASCII\n");
        } else {
            const bool changed = Settings::setCard(id, "name", name[0] ? name : nullptr);
            Settings::save();
            n = snprintf(out, sizeof(out), "%s: name %s%s%s%s - shown when its page is next built\n", id,
                         name[0] ? "\"" : "removed", name, name[0] ? "\"" : "", changed ? "" : " (no change)");
        }
        httpd_resp_set_type(req, "text/plain");
        return httpd_resp_send(req, out, n);
    }
    if (stats) {
        const Settings::Stats s = Settings::stats();
        char out[768];
        int n = snprintf(out, sizeof(out),
                               "file %s (%s)%s\nsaves %lu, failures %lu, pending %s\nlast save at %lu ms, "
                               "took %.1f ms, %lu bytes\n",
                               PATH, s_mounted ? "mounted" : "NOT MOUNTED - RAM only",
                               s_readOnly ? ", READ-ONLY this boot (see boot:)" : "",
                               (unsigned long)s.saves, (unsigned long)s.failures, s.pending ? "yes" : "no",
                               (unsigned long)s.lastMs, s.lastUs / 1000.0, (unsigned long)s.bytes);
        // The listing was taken by the settings task (listFiles()): reading the
        // flash from this task, whose stack is in PSRAM, reboots the board.
        { Lock l; n += snprintf(out + n, sizeof(out) - n, "boot: %s\nfiles in %s:\n%s", s_bootNote, BASE, s_listing); }
        httpd_resp_set_type(req, "text/plain");
        return httpd_resp_send(req, out, n);
    }
    size_t len = 0;
    char *buf = nullptr;
    { Lock l; buf = serialise(len); }
    if (!buf) return httpd_resp_send_500(req);
    httpd_resp_set_type(req, "application/json");
    const esp_err_t r = httpd_resp_send(req, buf, len);
    heap_caps_free(buf);
    return r;
}

}  // namespace

namespace Settings {

bool begin(HttpServer &http) {
    s_mutex = xSemaphoreCreateMutex();
    void *mem = heap_caps_malloc(sizeof(JsonDocument), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    s_doc = mem ? new (mem) JsonDocument(&s_alloc) : nullptr;
    if (!s_mutex || !s_doc) { Serial.println("[Settings] out of memory - no settings this boot"); return false; }
    s_doc->to<JsonObject>();

    esp_vfs_littlefs_conf_t conf = {};
    conf.base_path = BASE;
    conf.partition_label = PARTITION;
    conf.format_if_mount_failed = 1;
    const int64_t t0 = esp_timer_get_time();
    const esp_err_t err = esp_vfs_littlefs_register(&conf);
    s_mounted = err == ESP_OK;
    if (s_mounted) {
        size_t total = 0, used = 0;
        esp_littlefs_info(PARTITION, &total, &used);
        Serial.printf("[Settings] LittleFS on \"%s\" mounted at %s in %.1f ms: %u KB, %u KB used\n", PARTITION,
                      BASE, (esp_timer_get_time() - t0) / 1000.0, (unsigned)(total / 1024), (unsigned)(used / 1024));
        readFile();
        listFiles();
    } else {
        note("no LittleFS (%s) - settings live in RAM this boot and are lost at a reset", esp_err_to_name(err));
    }
    if (!s_readOnly) (*s_doc)["schema"] = SCHEMA;

    // The save task: its stack in internal RAM (xTaskCreate), which a flash
    // write requires - see Settings.h.
    xTaskCreate(saveTask, "settings", 4096, nullptr, 2, &s_task);
    http.addRoute("/settings", HTTP_GET, handleSettings);
    return s_mounted;
}

bool persistent() { return s_mounted && !s_readOnly; }

bool card(const char *cardId, const char *key, char *out, size_t cap) {
    bool found = false;
    if (cap) out[0] = '\0';
    if (!s_doc || !cardId || !*cardId) return false;
    Lock l;
    copyOut((*s_doc)["cards"][cardId][key], out, cap, found);
    return found;
}

bool scheme(const char *schemeName, const char *key, char *out, size_t cap) {
    bool found = false;
    if (cap) out[0] = '\0';
    if (!s_doc || !schemeName) return false;
    Lock l;
    copyOut((*s_doc)["schemes"][schemeName][key], out, cap, found);
    return found;
}

bool entityFlag(const char *entityId, const char *key, bool dflt) {
    if (!s_doc || !entityId) return dflt;
    Lock l;
    JsonVariantConst v = (*s_doc)["entities"][entityId][key];
    return v.is<bool>() ? v.as<bool>() : dflt;
}

void forEachEntityFlag(const char *key, void (*fn)(const char *, void *), void *ctx) {
    if (!s_doc || !fn) return;
    // Collected first, called after: fn may call back into the store.
    constexpr uint8_t MAXN = 64;
    char ids[MAXN][48];
    uint8_t n = 0;
    {
        Lock l;
        JsonObjectConst ents = (*s_doc)["entities"];
        for (JsonPairConst kv : ents) {
            if (n >= MAXN) break;
            if (kv.value()[key].is<bool>() && kv.value()[key].as<bool>())
                snprintf(ids[n++], sizeof(ids[0]), "%s", kv.key().c_str());
        }
    }
    for (uint8_t i = 0; i < n; i++) fn(ids[i], ctx);
}

bool setCard(const char *cardId, const char *key, const char *value) {
    if (!s_doc) return false;
    Lock l;
    const bool changed = setValue("cards", cardId, key, value, false, false, value == nullptr);
    if (changed) s_dirty = true;
    return changed;
}

bool setScheme(const char *schemeName, const char *key, const char *value) {
    if (!s_doc) return false;
    Lock l;
    const bool changed = setValue("schemes", schemeName, key, value, false, false, value == nullptr);
    if (changed) s_dirty = true;
    return changed;
}

bool setEntityFlag(const char *entityId, const char *key, bool value, bool dflt) {
    if (!s_doc) return false;
    Lock l;
    // The default is not stored: an entity that is not paused has no entry.
    const bool changed = setValue("entities", entityId, key, nullptr, true, value, value == dflt);
    if (changed) s_dirty = true;
    return changed;
}

bool imported(const char *name) {
    if (!s_doc || !name) return false;
    Lock l;
    JsonVariantConst v = (*s_doc)["imported"][name];
    return v.is<bool>() && v.as<bool>();
}

void markImported(const char *name) {
    if (!s_doc || !name) return;
    Lock l;
    JsonObject imp = objectAt(s_doc->as<JsonObject>(), "imported");
    if (imp[name].is<bool>() && imp[name].as<bool>()) return;
    imp[name] = true;
    s_dirty = true;
}

void save(uint32_t delayMs) {
    if (!s_task) return;
    s_delayMs = delayMs;
    xTaskNotifyGive(s_task);
}

Stats stats() {
    Stats s = {};
    if (!s_mutex) return s;
    Lock l;
    s = { s_saves, s_failures, s_lastMs, s_lastUs, s_bytes, s_dirty };
    return s;
}

}  // namespace Settings
