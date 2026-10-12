#include "HaHistory.h"

#include <Arduino.h>
#include <ArduinoJson.h>
#include <math.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "esp_heap_caps.h"

#include "HaClient.h"

namespace {

// HA's big answers are parsed in PSRAM where there is any: a day of statistics
// is ~1,500 JSON values, and ArduinoJson's pool for them is tens of KB that
// internal RAM has better uses for.
struct PsramAllocator : ArduinoJson::Allocator {
    void *allocate(size_t n) override {
        void *p = heap_caps_malloc(n, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
        return p ? p : malloc(n);
    }
    void deallocate(void *p) override { free(p); }
    void *reallocate(void *p, size_t n) override {
        void *q = heap_caps_realloc(p, n, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
        return q ? q : realloc(p, n);
    }
};
PsramAllocator s_psram;

// HA's time format, in UTC.
void iso(char *out, size_t n, uint32_t sec) {
    const time_t t = (time_t)sec;
    struct tm tm;
    gmtime_r(&t, &tm);
    strftime(out, n, "%Y-%m-%dT%H:%M:%SZ", &tm);
}

void copyText(char *dst, size_t n, const char *src) {
    snprintf(dst, n, "%s", src ? src : "");
}

constexpr uint32_t TIMEOUT_MS = 8000;
// A clock before this has not been set by SNTP yet.
constexpr uint32_t CLOCK_SET_SEC = 1700000000UL;

} // namespace

void HaHistory::begin(HaClient *ha) { _ha = ha; }

// The tables, once: in PSRAM when the board has it (16 KB of points, ~20 KB of
// events), a quarter of that in internal RAM when it has not.
bool HaHistory::ensureTables() {
    if (_points && _events) return true;
    const bool psram = heap_caps_get_total_size(MALLOC_CAP_SPIRAM) > 0;
    const uint16_t pc = psram ? 1024 : 256;
    const uint16_t ec = psram ? 120 : 30;
    const uint32_t caps = psram ? (MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT) : (MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    if (!_points) _points = (HistPoint *)heap_caps_malloc(sizeof(HistPoint) * pc, caps);
    if (!_events) _events = (HistEvent *)heap_caps_malloc(sizeof(HistEvent) * ec, caps);
    if (!_points || !_events) return false;
    _pointCap = pc;
    _eventCap = ec;
    Serial.printf("[HaHist] tables: %u points, %u events, %u B in %s\n", (unsigned)pc, (unsigned)ec,
                  (unsigned)(sizeof(HistPoint) * pc + sizeof(HistEvent) * ec), psram ? "PSRAM" : "internal RAM");
    return true;
}

// ---------------------------------------------------------------------------
// Requests - loop task
// ---------------------------------------------------------------------------

bool HaHistory::start(HistKind kind, const char *entityRef, uint32_t spanSec) {
    _gen++;
    _wantId.store(0);   // whatever was in flight is no longer wanted
    _kind  = kind;
    _stats = false;
    copyText(_ref, sizeof(_ref), entityRef);
    auto failNow = [&](HistFail why) {
        _fail.store((uint8_t)why);
        _state.store((uint8_t)HistState::HIST_FAILED);
        _doneSeq.fetch_add(1);
        return false;
    };
    if (!ensureTables()) return failNow(HistFail::FAIL_NO_MEMORY);
    if (!_ha || !_ha->isReady()) return failNow(HistFail::FAIL_NO_LINK);
    const time_t now = time(nullptr);
    if ((uint32_t)now < CLOCK_SET_SEC) return failNow(HistFail::FAIL_NO_CLOCK);
    _endSec   = (uint32_t)now;
    _startSec = _endSec - spanSec;
    _fail.store((uint8_t)HistFail::FAIL_NONE);
    _state.store((uint8_t)HistState::HIST_WAITING);
    _oversizedAt = _ha->oversized();
    _sentMs = millis();
    return true;
}

bool HaHistory::requestChart(const char *entityRef, uint32_t spanSec) {
    if (!start(HistKind::HIST_CHART, entityRef, spanSec)) return false;
    if (sendHistory()) return true;
    finish(HistFail::FAIL_NO_LINK);
    return false;
}

bool HaHistory::requestActivity(const char *entityRef, uint32_t spanSec) {
    if (!start(HistKind::HIST_ACTIVITY, entityRef, spanSec)) return false;
    // People's names, once a session, ahead of the first activity asked for.
    if (_namesSession != _ha->sessions()) sendNames();
    if (sendLogbook()) return true;
    finish(HistFail::FAIL_NO_LINK);
    return false;
}

// The id is published before the frame goes, so even an instant answer is
// recognised; it cannot arrive before the frame is sent.
bool HaHistory::sendHistory() {
    char frame[400], a[24], b[24];
    iso(a, sizeof(a), _startSec);
    iso(b, sizeof(b), _endSec);
    const uint32_t id = _ha->nextId();
    const int n = snprintf(frame, sizeof(frame),
                           "{\"id\":%lu,\"type\":\"history/history_during_period\",\"start_time\":\"%s\","
                           "\"end_time\":\"%s\",\"entity_ids\":[\"%s\"],\"minimal_response\":true,"
                           "\"no_attributes\":true}",
                           (unsigned long)id, a, b, _ref);
    if (n <= 0 || n >= (int)sizeof(frame)) return false;
    _wantId.store(id);
    if (_ha->sendText(frame, n)) return true;
    _wantId.store(0);
    return false;
}

bool HaHistory::sendStatistics() {
    char frame[400], a[24], b[24];
    iso(a, sizeof(a), _startSec);
    iso(b, sizeof(b), _endSec);
    const uint32_t id = _ha->nextId();
    const int n = snprintf(frame, sizeof(frame),
                           "{\"id\":%lu,\"type\":\"recorder/statistics_during_period\",\"start_time\":\"%s\","
                           "\"end_time\":\"%s\",\"statistic_ids\":[\"%s\"],\"period\":\"5minute\","
                           "\"types\":[\"mean\",\"min\",\"max\"]}",
                           (unsigned long)id, a, b, _ref);
    if (n <= 0 || n >= (int)sizeof(frame)) return false;
    _wantId.store(id);
    if (_ha->sendText(frame, n)) return true;
    _wantId.store(0);
    return false;
}

bool HaHistory::sendLogbook() {
    char frame[400], a[24], b[24];
    iso(a, sizeof(a), _startSec);
    iso(b, sizeof(b), _endSec);
    const uint32_t id = _ha->nextId();
    const int n = snprintf(frame, sizeof(frame),
                           "{\"id\":%lu,\"type\":\"logbook/get_events\",\"start_time\":\"%s\","
                           "\"end_time\":\"%s\",\"entity_ids\":[\"%s\"]}",
                           (unsigned long)id, a, b, _ref);
    if (n <= 0 || n >= (int)sizeof(frame)) return false;
    _wantId.store(id);
    if (_ha->sendText(frame, n)) return true;
    _wantId.store(0);
    return false;
}

// HA's person entities carry their user's id: "uid>name|uid>name|". A template
// is a subscription, so loop() cancels it once it has answered.
void HaHistory::sendNames() {
    char frame[256];
    const uint32_t id = _ha->nextId();
    const int n = snprintf(frame, sizeof(frame),
                           "{\"id\":%lu,\"type\":\"render_template\",\"template\":\"{%% for p in states.person %%}"
                           "{{ p.attributes.user_id }}>{{ p.name }}|{%% endfor %%}\"}",
                           (unsigned long)id);
    if (n <= 0 || n >= (int)sizeof(frame)) return;
    _namesDone.store(false);
    _namesId.store(id);
    if (_ha->sendText(frame, n)) _namesSession = _ha->sessions();
    else                         _namesId.store(0);
}

void HaHistory::loop(uint32_t nowMs) {
    if (!_ha) return;

    const uint32_t nid = _namesId.load();
    if (nid && _namesDone.exchange(false)) {
        char frame[96];
        const int n = snprintf(frame, sizeof(frame),
                               "{\"id\":%lu,\"type\":\"unsubscribe_events\",\"subscription\":%lu}",
                               (unsigned long)_ha->nextId(), (unsigned long)nid);
        _ha->sendText(frame, n);
        _namesId.store(0);
    }

    if (state() != HistState::HIST_WAITING) return;
    auto failLoop = [&](HistFail why) {
        _wantId.store(0);
        finish(why);
        Serial.printf("[HaHist] %s: failed (%u)\n", _ref, (unsigned)why);
    };
    // HaClient dropped a message too big for its buffer while we waited: ours.
    if (_ha->oversized() != _oversizedAt) {
        _oversizedAt = _ha->oversized();
        if (_kind == HistKind::HIST_CHART && !_stats) {
            // TOO CHATTY FOR ITS REAL CHANGES: HA's 5-minute statistics instead.
            Serial.printf("[HaHist] %s: history too big, asking for statistics\n", _ref);
            _stats  = true;
            _sentMs = nowMs;
            if (!sendStatistics()) failLoop(HistFail::FAIL_NO_LINK);
            return;
        }
        failLoop(HistFail::FAIL_TOO_BIG);
        return;
    }
    if (nowMs - _sentMs > TIMEOUT_MS) failLoop(HistFail::FAIL_TIMEOUT);
}

const char *HaHistory::personName(const char *userId) const {
    if (!userId || !userId[0]) return nullptr;
    const uint8_t n = _nPeople.load();
    for (uint8_t i = 0; i < n; i++)
        if (!strcmp(_people[i].id, userId)) return _people[i].name;
    return nullptr;
}

// ---------------------------------------------------------------------------
// Answers - WEBSOCKET TASK
// ---------------------------------------------------------------------------

bool HaHistory::onRaw(const char *json, size_t len, void *ctx) {
    HaHistory *self = (HaHistory *)ctx;
    return self && self->handle(json, len);
}

// HA writes the id first ({"id":N,...}), so ours is told from everything else
// without a parse.
bool HaHistory::handle(const char *json, size_t len) {
    if (len < 8 || strncmp(json, "{\"id\":", 6) != 0) return false;
    const uint32_t id = strtoul(json + 6, nullptr, 10);
    if (!id) return false;

    if (id == _namesId.load()) {
        // The template's "result" (null) comes first, then its event.
        if (strstr(json, "\"type\":\"event\"")) parseNames(json, len);
        return true;
    }
    if (id != _wantId.load()) return false;

    _lastBytes = (uint32_t)len;
    _lastMs    = millis() - _sentMs;
    char start[80];   // the result's head: the error is there, before any data
    copyText(start, sizeof(start), json);
    if (strstr(start, "\"success\":false")) {
        Serial.printf("[HaHist] %s: HA refused: %.120s\n", _ref, json);
        if (_wantId.load() == id) finish(HistFail::FAIL_REFUSED);
        return true;
    }
    if (_kind == HistKind::HIST_ACTIVITY) parseLogbook(json, len);
    else if (_stats)                      parseStatistics(json, len);
    else                                  parseHistory(json, len);
    Serial.printf("[HaHist] %s: %lu B in %lu ms -> %u points, %u events%s\n", _ref, (unsigned long)len,
                  (unsigned long)_lastMs, (unsigned)_nPoints, (unsigned)_nEvents, _stats ? " (statistics)" : "");
    return true;
}

void HaHistory::finish(HistFail why) {
    _fail.store((uint8_t)why);
    _state.store((uint8_t)(why == HistFail::FAIL_NONE ? HistState::HIST_READY : HistState::HIST_FAILED));
    _doneSeq.fetch_add(1);
}

// Keeps every point up to the table's size; past it, every second one, then
// every fourth... so a long answer thins evenly instead of losing its end.
void HaHistory::addPoint(uint32_t t, float v, float lo, float hi) {
    const uint32_t c = _strideAt++;
    if (c % _stride) return;
    if (_nPoints >= _pointCap) {
        for (uint16_t i = 0; i < _nPoints / 2; i++) _points[i] = _points[2 * i];
        _nPoints /= 2;
        _stride *= 2;
        if (c % _stride) return;
    }
    _points[_nPoints++] = { t, v, lo, hi };
}

// {"result":{"sensor.x":[{"s":"72.0","lu":1791621000.0},...]}}
void HaHistory::parseHistory(const char *json, size_t len) {
    const uint32_t id = _wantId.load();
    _nPoints = 0; _stride = 1; _strideAt = 0;
    JsonDocument doc(&s_psram);
    if (deserializeJson(doc, json, len)) { if (_wantId.load() == id) finish(HistFail::FAIL_REFUSED); return; }
    for (JsonPairConst kv : doc["result"].as<JsonObjectConst>()) {
        for (JsonObjectConst p : kv.value().as<JsonArrayConst>()) {
            const char *st = p["s"] | "";
            char *end = nullptr;
            float v = strtof(st, &end);
            if (end == st) v = NAN;   // unavailable, unknown: a gap
            addPoint((uint32_t)(p["lu"] | 0.0), v, NAN, NAN);
        }
        break;
    }
    if (_wantId.load() == id) finish(HistFail::FAIL_NONE);
}

// {"result":{"sensor.x":[{"start":ms,"end":ms,"mean":..,"min":..,"max":..},...]}}
// An entity HA keeps no statistics for answers {}.
void HaHistory::parseStatistics(const char *json, size_t len) {
    const uint32_t id = _wantId.load();
    _nPoints = 0; _stride = 1; _strideAt = 0;
    JsonDocument doc(&s_psram);
    if (deserializeJson(doc, json, len)) { if (_wantId.load() == id) finish(HistFail::FAIL_REFUSED); return; }
    bool any = false;
    for (JsonPairConst kv : doc["result"].as<JsonObjectConst>()) {
        any = true;
        for (JsonObjectConst p : kv.value().as<JsonArrayConst>()) {
            const double startMs = p["start"] | 0.0;
            JsonVariantConst mean = p["mean"], lo = p["min"], hi = p["max"];
            // The bucket's middle: five minutes, so 150 s after its start.
            addPoint((uint32_t)(startMs / 1000.0) + 150, mean.isNull() ? NAN : mean.as<float>(),
                     lo.isNull() ? NAN : lo.as<float>(), hi.isNull() ? NAN : hi.as<float>());
        }
        break;
    }
    if (_wantId.load() == id) finish(any ? HistFail::FAIL_NONE : HistFail::FAIL_NO_DATA);
}

// {"result":[{"state":"off","entity_id":..,"when":s, "context_...":..},...]},
// oldest first. The table keeps the newest; what did not fit is counted.
void HaHistory::parseLogbook(const char *json, size_t len) {
    const uint32_t id = _wantId.load();
    _nEvents = 0; _eventsDropped = 0;
    JsonDocument doc(&s_psram);
    if (deserializeJson(doc, json, len)) { if (_wantId.load() == id) finish(HistFail::FAIL_REFUSED); return; }
    JsonArrayConst arr = doc["result"].as<JsonArrayConst>();
    uint16_t withState = 0;
    for (JsonObjectConst e : arr) if (e["state"].is<const char *>()) withState++;
    uint16_t skip = withState > _eventCap ? withState - _eventCap : 0;
    _eventsDropped = skip;
    for (JsonObjectConst e : arr) {
        const char *st = e["state"] | (const char *)nullptr;
        if (!st) continue;   // a message line, not a change
        if (skip) { skip--; continue; }
        HistEvent &ev = _events[_nEvents++];
        ev.t = (uint32_t)(e["when"] | 0.0);
        copyText(ev.state, sizeof(ev.state), st);
        const char *etype = e["context_event_type"] | "";
        const char *user  = e["context_user_id"] | "";
        const char *cname = e["context_name"] | "";
        const char *cmsg  = e["context_message"] | "";
        const char *cent  = e["context_entity_id"] | "";
        const char *centN = e["context_entity_id_name"] | "";
        const char *cst   = e["context_state"] | "";
        const char *cdom  = e["context_domain"] | "";
        const char *csvc  = e["context_service"] | "";
        ev.detail[0] = '\0';
        // The forms seen in a week of the Office group (2026-10-11): a person's
        // call_service (50), an automation with what triggered it (21), and
        // nothing at all (24 - the Hue app, a wall switch, the bulb itself).
        if (!strcmp(etype, "automation_triggered")) {
            ev.cause = CauseKind::CAUSE_AUTOMATION;
            copyText(ev.who, sizeof(ev.who), cname);
            copyText(ev.detail, sizeof(ev.detail), cmsg);
        } else if (user[0]) {
            ev.cause = CauseKind::CAUSE_USER;
            copyText(ev.who, sizeof(ev.who), user);
            if (cdom[0] && csvc[0]) snprintf(ev.detail, sizeof(ev.detail), "%s.%s", cdom, csvc);
        } else if (cent[0]) {
            ev.cause = CauseKind::CAUSE_ENTITY;
            copyText(ev.who, sizeof(ev.who), centN[0] ? centN : cent);
            if (cst[0]) snprintf(ev.detail, sizeof(ev.detail), "%s: %s", cent, cst);
        } else if (!strcmp(etype, "call_service") && cdom[0]) {
            ev.cause = CauseKind::CAUSE_ACTION;
            snprintf(ev.who, sizeof(ev.who), "%s.%s", cdom, csvc);
        } else if (cname[0]) {
            ev.cause = CauseKind::CAUSE_OTHER;
            copyText(ev.who, sizeof(ev.who), cname);
            copyText(ev.detail, sizeof(ev.detail), cmsg);
        } else {
            ev.cause = CauseKind::CAUSE_NONE;
            ev.who[0] = '\0';
        }
    }
    if (_wantId.load() == id) finish(HistFail::FAIL_NONE);
}

// {"id":N,"type":"event","event":{"result":"uid>name|uid>name|",...}}
void HaHistory::parseNames(const char *json, size_t len) {
    JsonDocument filter;
    filter["event"]["result"] = true;
    JsonDocument doc;
    if (deserializeJson(doc, json, len, DeserializationOption::Filter(filter))) return;
    char buf[512];
    copyText(buf, sizeof(buf), doc["event"]["result"] | "");
    _nPeople.store(0);   // not read while it is being rewritten
    uint8_t n = 0;
    char *save = nullptr;
    for (char *tok = strtok_r(buf, "|", &save); tok && n < PEOPLE_MAX; tok = strtok_r(nullptr, "|", &save)) {
        char *gt = strchr(tok, '>');
        if (!gt || gt == tok) continue;   // a person with no user: nothing to match
        *gt = '\0';
        copyText(_people[n].id, sizeof(_people[n].id), tok);
        copyText(_people[n].name, sizeof(_people[n].name), gt + 1);
        n++;
    }
    _nPeople.store(n);
    _namesDone.store(true);
    Serial.printf("[HaHist] %u people named\n", (unsigned)n);
}
