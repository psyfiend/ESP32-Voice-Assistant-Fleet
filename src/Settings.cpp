// Settings - see Settings.h. The PC's addresses are SettingsHttp.cpp.
#include "Settings.h"
#include "SettingsInternal.h"

#include <Arduino.h>   // Serial, millis()
#include <ctype.h>
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <new>         // placement new into PSRAM
#include <dirent.h>
#include <sys/stat.h>
#include <ArduinoJson.h>   // header-only, plain C++ - nothing Arduino-specific is used
#include "esp_heap_caps.h"
#include "esp_littlefs.h"
#include "esp_partition.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

namespace {

constexpr int         SCHEMA        = 1;
constexpr const char *BASE          = "/cfg";
constexpr const char *PARTITION     = "spiffs";   // the 16 MB layout's data partition
constexpr const char *PATH          = "/cfg/settings.json";
constexpr const char *TMP_PATH      = "/cfg/settings.tmp";
constexpr const char *BAD_PATH      = "/cfg/settings.bad";
constexpr size_t      FILE_MAX      = Settings::detail::FILE_MAX;   // refuse anything larger at read

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

char     s_bootNote[160] = "not started";  // what the mount and readFile() found, for ?stats=1
char     s_listing[256] = "";              // the files in BASE, as of boot or the last save
bool     s_mounted  = false;
// A newer schema, a file that would not read, or a partition that would not
// mount but holds data (A17): never overwritten.
bool     s_readOnly = false;
bool     s_dirty    = false;
uint32_t s_saves = 0, s_failures = 0, s_lastMs = 0, s_lastUs = 0, s_bytes = 0;
// Which version of the document the last good write held, so a restore can wait
// for ITS write rather than one that was already under way (A17).
uint32_t s_docGen = 0, s_savedGen = 0;

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

// WHO CHANGED WHAT (2.10d). Desk's Tap action was found set on the P4_5 with
// nobody at the panel, and nothing said how. Every change is now logged with
// its time and the task that made it - "loopTask" is the card window (the LVGL
// thread), "httpd" a browser - and the last few are kept for ?stats=1.
constexpr uint8_t CHANGES_KEPT = 10;
char    s_changes[CHANGES_KEPT][112];
uint8_t s_changeHead = 0, s_changeN = 0;

// Caller holds the lock.
void noteLine(const char *what) {
    char when[24] = "";
    time_t now = time(nullptr);
    struct tm tmv;
    if (now > 1700000000 && localtime_r(&now, &tmv)) strftime(when, sizeof(when), "%m-%d %H:%M:%S", &tmv);
    else snprintf(when, sizeof(when), "%lus", (unsigned long)(millis() / 1000));
    const char *task = pcTaskGetName(nullptr);
    char *line = s_changes[s_changeHead];
    snprintf(line, sizeof(s_changes[0]), "%s %s: %s", when, task ? task : "?", what);
    s_changeHead = (uint8_t)((s_changeHead + 1) % CHANGES_KEPT);
    if (s_changeN < CHANGES_KEPT) s_changeN++;
    Serial.printf("[Settings] %s\n", line);
}

void noteChange(const char *section, const char *id, const char *key, const char *value, bool remove) {
    char what[112];
    snprintf(what, sizeof(what), "%s.%s.%s %s%s", section, id, key, remove ? "removed" : "= ",
             remove ? "" : (value ? value : "?"));
    noteLine(what);
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
        noteChange(section, id, key, nullptr, true);
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
    noteChange(section, id, key, isBool ? (b ? "true" : "false") : str, false);
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
    uint32_t gen = 0;
    {
        Lock l;
        if (!s_dirty || s_readOnly || !s_mounted) return;
        buf = serialise(len);
        if (!buf) { s_failures++; return; }
        s_dirty = false;   // a change made while writing marks it again
        gen = s_docGen;
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
        if (ok) { s_saves++; s_lastMs = millis(); s_lastUs = us; s_bytes = (uint32_t)len; s_savedGen = gen; }
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

// A FAILED MOUNT NEVER COSTS THE SETTINGS (A17). Until 2026-10-15 the mount
// formatted the partition whenever LittleFS would not mount it, and the P4_5
// lost its file on 2026-10-14, most likely that way. Now the partition is
// formatted only when it is blank - every byte 0xFF, a first boot - and
// anything else is left exactly as it is: the boot runs read-only from RAM
// and says so. The WHOLE partition is checked, not only its start: an erased
// start with the file's blocks still behind it is what A16's erase mode left.
// Reads the flash, so begin()'s caller only (a task whose stack is internal).
//   0: blank; 1: holds data, `at` is the first byte that is not 0xFF;
//   -1: could not be read.
int partitionState(const esp_partition_t *p, uint32_t &at) {
    constexpr size_t CHUNK = 4096;
    uint32_t *buf = (uint32_t *)heap_caps_malloc(CHUNK, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    if (!buf) return -1;
    int state = 0;
    at = 0;
    while (state == 0 && at < p->size) {
        const size_t n = (p->size - at) < CHUNK ? (size_t)(p->size - at) : CHUNK;
        if (esp_partition_read(p, at, buf, n) != ESP_OK) { state = -1; break; }
        size_t i = 0;
        while (i < n / 4 && buf[i] == 0xFFFFFFFFu) i++;
        if (i < n / 4) { state = 1; at += (uint32_t)(i * 4); }
        else at += (uint32_t)n;
    }
    heap_caps_free(buf);
    return state;
}

esp_err_t mountFs() {
#ifdef DEBUG_SETTINGS_NOMOUNT
    // A17's test, a local line only (never committed): the mount reports a
    // failure, so the failed-mount path runs against a partition that holds a
    // real file - which must come through the next normal boot untouched.
    Serial.println("[Settings] DEBUG_SETTINGS_NOMOUNT: the mount reports a failure");
    return ESP_FAIL;
#else
    esp_vfs_littlefs_conf_t conf = {};
    conf.base_path = BASE;
    conf.partition_label = PARTITION;
    conf.format_if_mount_failed = 0;   // never: see partitionState()
    return esp_vfs_littlefs_register(&conf);
#endif
}

// Mount, or format a blank partition and mount it, or leave it alone and run
// read-only. Sets s_mounted / s_readOnly and the boot note.
void mountOrKeep() {
    const int64_t t0 = esp_timer_get_time();
    esp_err_t err = mountFs();
    if (err != ESP_OK) {
        const esp_partition_t *part =
            esp_partition_find_first(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_ANY, PARTITION);
        uint32_t at = 0;
        const int state = part ? partitionState(part, at) : -2;
        const double ms = (esp_timer_get_time() - t0) / 1000.0;
        if (state == 0) {
            Serial.printf("[Settings] the \"%s\" partition is blank (%u KB, checked in %.1f ms) - a first boot: "
                          "formatting it\n", PARTITION, (unsigned)(part->size / 1024), ms);
            err = esp_littlefs_format(PARTITION);
            if (err == ESP_OK) err = mountFs();
            if (err != ESP_OK) note("formatted a blank partition, but it did not mount (%s) - settings in RAM this "
                                    "boot", esp_err_to_name(err));
        } else if (state == 1) {
            s_readOnly = true;
            note("LittleFS did not mount (%s); the partition holds data (from 0x%lx) - left untouched, read-only "
                 "this boot, settings in RAM", esp_err_to_name(err), (unsigned long)at);
        } else if (state == -1) {
            s_readOnly = true;
            note("LittleFS did not mount (%s) and the partition would not read - left untouched, read-only this "
                 "boot", esp_err_to_name(err));
        } else {
            note("no \"%s\" partition - settings live in RAM this boot and are lost at a reset", PARTITION);
        }
    }
    s_mounted = err == ESP_OK;
    if (s_mounted) {
        size_t total = 0, used = 0;
        esp_littlefs_info(PARTITION, &total, &used);
        Serial.printf("[Settings] LittleFS on \"%s\" mounted at %s in %.1f ms: %u KB, %u KB used\n", PARTITION,
                      BASE, (esp_timer_get_time() - t0) / 1000.0, (unsigned)(total / 1024), (unsigned)(used / 1024));
    }
}

}  // namespace

namespace Settings {

bool begin(HttpServer &http) {
    s_mutex = xSemaphoreCreateMutex();
    void *mem = heap_caps_malloc(sizeof(JsonDocument), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    s_doc = mem ? new (mem) JsonDocument(&s_alloc) : nullptr;
    if (!s_mutex || !s_doc) { Serial.println("[Settings] out of memory - no settings this boot"); return false; }
    s_doc->to<JsonObject>();

    mountOrKeep();   // A17: formats only a blank partition
    if (s_mounted) {
        readFile();
        listFiles();
    }
    if (!s_readOnly) (*s_doc)["schema"] = SCHEMA;

    // The save task: its stack in internal RAM (xTaskCreate), which a flash
    // write requires - see Settings.h.
    xTaskCreate(saveTask, "settings", 4096, nullptr, 2, &s_task);
    detail::registerRoutes(http);   // SettingsHttp.cpp
    return s_mounted;
}

bool persistent() { return s_mounted && !s_readOnly; }

void bootNote(char *out, size_t cap) {
    if (!cap) return;
    if (!s_mutex) { snprintf(out, cap, "%s", s_bootNote); return; }
    Lock l;
    snprintf(out, cap, "%s", s_bootNote);
}

// --- For SettingsHttp.cpp (SettingsInternal.h) --------------------------------

int detail::statusText(char *out, size_t cap) {
    const Stats s = stats();
    int n = snprintf(out, cap,
                     "file %s (%s)%s\nsaves %lu, failures %lu, pending %s\nlast save at %lu ms, "
                     "took %.1f ms, %lu bytes\n",
                     PATH, s_mounted ? "mounted" : "NOT MOUNTED - RAM only",
                     s_readOnly ? ", READ-ONLY this boot (see boot:)" : "",
                     (unsigned long)s.saves, (unsigned long)s.failures, s.pending ? "yes" : "no",
                     (unsigned long)s.lastMs, s.lastUs / 1000.0, (unsigned long)s.bytes);
    // The listing was taken by the settings task (listFiles()): reading the
    // flash from the HTTP task, whose stack is in PSRAM, reboots the board.
    Lock l;
    n += snprintf(out + n, cap - n, "boot: %s\nfiles in %s:\n%s", s_bootNote, BASE, s_listing);
    if (s_changeN) n += snprintf(out + n, cap - n, "last changes (newest last):\n");
    for (uint8_t i = 0; i < s_changeN && n < (int)cap - 120; i++) {
        const uint8_t k = (uint8_t)((s_changeHead + CHANGES_KEPT - s_changeN + i) % CHANGES_KEPT);
        n += snprintf(out + n, cap - n, "  %s\n", s_changes[k]);
    }
    return n;
}

char *detail::serialiseCopy(size_t &len) {
    len = 0;
    if (!s_doc) return nullptr;
    Lock l;
    return serialise(len);
}

bool detail::writable(char *why, size_t cap) {
    Lock l;
    if (s_mounted && !s_readOnly) return true;
    if (cap) snprintf(why, cap, "%s", s_bootNote);
    return false;
}

int detail::restore(const char *json, size_t len, char *msg, size_t cap) {
    char why[160];
    if (!writable(why, sizeof(why))) {
        snprintf(msg, cap, "refused: this boot does not write the settings file (%s) - nothing changed", why);
        return 409;
    }
    JsonDocument incoming(&s_alloc);
    const DeserializationError err = deserializeJson(incoming, json, len);
    if (err || !incoming.is<JsonObject>()) {
        snprintf(msg, cap, "refused: not a settings file (%s) - nothing changed",
                 err ? err.c_str() : "not a JSON object");
        return 400;
    }
    const int schema = incoming["schema"] | 0;
    if (schema < 1 || schema > SCHEMA) {
        snprintf(msg, cap, "refused: the file is schema %d, this firmware knows 1 to %d - nothing changed",
                 schema, SCHEMA);
        return 400;
    }
    const unsigned cards = (unsigned)incoming["cards"].size();
    uint32_t gen = 0;
    {
        Lock l;
        s_doc->set(incoming);
        s_dirty = true;
        gen = ++s_docGen;
        char what[80];
        snprintf(what, sizeof(what), "the whole file restored (%u bytes, %u card(s))", (unsigned)len, cards);
        noteLine(what);
    }
    const int64_t t0 = esp_timer_get_time();
    save();
    bool written = false;
    for (int i = 0; i < 250 && !written; i++) {   // 5 s; a write takes ~10-100 ms
        vTaskDelay(pdMS_TO_TICKS(20));
        Lock l;
        written = s_savedGen >= gen;
    }
    if (!written) {
        snprintf(msg, cap, "the file is in RAM but was not written (failures %lu) - it is written with the "
                 "next change", (unsigned long)stats().failures);
        return 500;
    }
    snprintf(msg, cap, "restored: %u bytes, %u card(s), written in %.0f ms", (unsigned)len, cards,
             (esp_timer_get_time() - t0) / 1000.0);
    return 200;
}

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

bool system(const char *area, const char *key, char *out, size_t cap) {
    bool found = false;
    if (cap) out[0] = '\0';
    if (!s_doc || !area || !*area) return false;
    Lock l;
    copyOut((*s_doc)["system"][area][key], out, cap, found);
    return found;
}

bool page(const char *slug, const char *key, char *out, size_t cap) {
    bool found = false;
    if (cap) out[0] = '\0';
    if (!s_doc || !slug || !*slug) return false;
    Lock l;
    copyOut((*s_doc)["pages"][slug][key], out, cap, found);
    return found;
}

bool setPage(const char *slug, const char *key, const char *value) {
    if (!s_doc) return false;
    Lock l;
    const bool changed = setValue("pages", slug, key, value, false, false, value == nullptr);
    if (changed) s_dirty = true;
    return changed;
}

bool group(const char *groupId, const char *key, char *out, size_t cap) {
    bool found = false;
    if (cap) out[0] = '\0';
    if (!s_doc || !groupId || !*groupId) return false;
    Lock l;
    copyOut((*s_doc)["groups"][groupId][key], out, cap, found);
    return found;
}

bool setGroup(const char *groupId, const char *key, const char *value) {
    if (!s_doc) return false;
    Lock l;
    const bool changed = setValue("groups", groupId, key, value, false, false, value == nullptr);
    if (changed) s_dirty = true;
    return changed;
}

void forEachGroup(void (*fn)(const char *, const char *, void *), void *ctx) {
    if (!s_doc || !fn) return;
    constexpr uint8_t MAXN = 24;
    struct G { char id[48]; char name[40]; };
    G *g = (G *)heap_caps_malloc(MAXN * sizeof(G), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!g) return;
    uint8_t n = 0;
    {
        Lock l;
        JsonObjectConst groups = (*s_doc)["groups"];
        for (JsonPairConst kv : groups) {
            if (n >= MAXN) break;
            const char *name = kv.value()["name"] | "";
            if (!name[0]) continue;
            snprintf(g[n].id, sizeof(g[n].id), "%s", kv.key().c_str());
            snprintf(g[n].name, sizeof(g[n].name), "%s", name);
            n++;
        }
    }
    for (uint8_t i = 0; i < n; i++) fn(g[i].id, g[i].name, ctx);
    heap_caps_free(g);
}

bool newGroup(const char *name, char *id, size_t cap) {
    if (!s_doc || !name || !*name || !cap) return false;
    // <name>_<yymmdd>_<hhmm>, as card ids (K33); a letter when taken.
    char slug[24];
    size_t k = 0;
    for (const char *p = name; *p && k < sizeof(slug) - 1; p++) {
        const char ch = (char)tolower((unsigned char)*p);
        if ((ch >= 'a' && ch <= 'z') || (ch >= '0' && ch <= '9')) slug[k++] = ch;
        else if (k && slug[k - 1] != '_') slug[k++] = '_';
    }
    while (k && slug[k - 1] == '_') k--;
    slug[k] = '\0';
    if (!k) snprintf(slug, sizeof(slug), "group");
    char stamp[16] = "000000_0000";
    time_t now = time(nullptr);
    struct tm tmv;
    if (now > 1700000000 && localtime_r(&now, &tmv))
        strftime(stamp, sizeof(stamp), "%y%m%d_%H%M", &tmv);
    Lock l;
    JsonObject groups = objectAt(s_doc->as<JsonObject>(), "groups");
    for (char suffix = 0; suffix <= 'z'; suffix = suffix ? (char)(suffix + 1) : 'b') {
        if (suffix) snprintf(id, cap, "%s_%s%c", slug, stamp, suffix);
        else        snprintf(id, cap, "%s_%s", slug, stamp);
        if (!groups[id].is<JsonObject>()) {
            JsonObject g = groups[id].to<JsonObject>();
            g["name"] = name;
            s_dirty = true;
            return true;
        }
    }
    return false;
}

uint8_t countCards(const char *key, const char *value) {
    if (!s_doc || !key || !value) return 0;
    Lock l;
    uint8_t n = 0;
    JsonObjectConst cards = (*s_doc)["cards"];
    for (JsonPairConst kv : cards) {
        JsonVariantConst v = kv.value()[key];
        if (v.is<const char *>() && !strcmp(v.as<const char *>(), value)) n++;
    }
    return n;
}

void forEachCardWith(const char *key, void (*fn)(const char *, const char *, void *), void *ctx) {
    if (!s_doc || !key || !fn) return;
    constexpr uint8_t MAXN = 96;   // in PSRAM: the pool of removed cards counts too
    struct C { char id[48]; char v[48]; };
    C *c = (C *)heap_caps_malloc(MAXN * sizeof(C), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!c) return;
    uint8_t n = 0;
    {
        Lock l;
        JsonObjectConst cards = (*s_doc)["cards"];
        for (JsonPairConst kv : cards) {
            if (n >= MAXN) break;
            JsonVariantConst v = kv.value()[key];
            if (!v.is<const char *>()) continue;
            snprintf(c[n].id, sizeof(c[n].id), "%s", kv.key().c_str());
            snprintf(c[n].v, sizeof(c[n].v), "%s", v.as<const char *>());
            n++;
        }
    }
    for (uint8_t i = 0; i < n; i++) fn(c[i].id, c[i].v, ctx);
    heap_caps_free(c);
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

bool dropCard(const char *cardId) {
    if (!s_doc || !cardId || !*cardId) return false;
    Lock l;
    JsonObject root = s_doc->as<JsonObject>();
    JsonVariant secV = root["cards"];
    if (!secV.is<JsonObject>() || secV[cardId].isNull()) return false;
    JsonObject sec = secV.as<JsonObject>();
    sec.remove(cardId);
    if (sec.size() == 0) root.remove("cards");
    noteChange("cards", cardId, "(entry)", nullptr, true);
    s_dirty = true;
    return true;
}

bool setScheme(const char *schemeName, const char *key, const char *value) {
    if (!s_doc) return false;
    Lock l;
    const bool changed = setValue("schemes", schemeName, key, value, false, false, value == nullptr);
    if (changed) s_dirty = true;
    return changed;
}

bool setSystem(const char *area, const char *key, const char *value) {
    if (!s_doc) return false;
    Lock l;
    const bool changed = setValue("system", area, key, value, false, false, value == nullptr);
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

// --- Unclaimed entries (K35) -------------------------------------------------
namespace {
ClaimFn s_claimCard = nullptr, s_claimEntity = nullptr;

// The ids under a section that its claim does not know, copied out under the
// lock so the claim (which may take the registry's lock) runs without ours.
uint8_t unclaimedIn(const char *section, ClaimFn claim, char (*out)[48], uint8_t cap) {
    if (!s_doc || !claim) return 0;
    // In PSRAM: 3 KB is a lot for the HTTP task's 8 KB stack.
    constexpr uint8_t MAXN = 64;
    char (*ids)[48] = (char (*)[48])heap_caps_malloc(MAXN * 48, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!ids) return 0;
    uint8_t n = 0, k = 0;
    {
        Lock l;
        JsonObjectConst sec = (*s_doc)[section];
        for (JsonPairConst kv : sec) {
            if (n >= MAXN) break;
            snprintf(ids[n++], 48, "%s", kv.key().c_str());
        }
    }
    for (uint8_t i = 0; i < n && k < cap; i++)
        if (!claim(ids[i])) snprintf(out[k++], 48, "%s", ids[i]);
    heap_caps_free(ids);
    return k;
}
}  // namespace

void setClaims(ClaimFn card, ClaimFn entity) {
    s_claimCard   = card;
    s_claimEntity = entity;
}

void forEachUnclaimed(void (*fn)(const char *, const char *, void *), void *ctx) {
    if (!fn) return;
    char ids[32][48];
    uint8_t n = unclaimedIn("cards", s_claimCard, ids, 32);
    for (uint8_t i = 0; i < n; i++) fn("cards", ids[i], ctx);
    n = unclaimedIn("entities", s_claimEntity, ids, 32);
    for (uint8_t i = 0; i < n; i++) fn("entities", ids[i], ctx);
}

uint8_t prune(const char *section) {
    const bool cards = section && !strcmp(section, "cards");
    if (!cards && !(section && !strcmp(section, "entities"))) return 0;
    char ids[32][48];
    const uint8_t n = unclaimedIn(section, cards ? s_claimCard : s_claimEntity, ids, 32);
    if (!n) return 0;
    {
        Lock l;
        JsonObject root = s_doc->as<JsonObject>();
        JsonVariant secV = root[section];
        if (secV.is<JsonObject>()) {
            JsonObject sec = secV.as<JsonObject>();
            for (uint8_t i = 0; i < n; i++) sec.remove(ids[i]);
            if (sec.size() == 0) root.remove(section);
        }
        s_dirty = true;
    }
    for (uint8_t i = 0; i < n; i++) Serial.printf("[Settings] pruned %s.%s\n", section, ids[i]);
    save();
    return n;
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
