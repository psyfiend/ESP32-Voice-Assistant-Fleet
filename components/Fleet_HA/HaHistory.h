#pragma once
#ifndef HA_HISTORY_H
#define HA_HISTORY_H

#include <stdint.h>
#include <stddef.h>
#include <atomic>

class HaClient;

// ---------------------------------------------------------------------------
// One entity's past, from Home Assistant, for a card window's History view
// (2.10e). No LVGL here: the window asks, this asks HA, the window draws.
//
// WHAT IT ASKS, MEASURED AGAINST THE OWNER'S HA ON 2026-10-11 (card-sheet §18):
//
//   A CHART (a number): history/history_during_period, minimal and without
//   attributes - the real changes, as HA's own graph shows them, and small for
//   the dashboard's sensors (0.5 KB a day for the thermostat, 5.5 KB for the
//   deck's light level). A sensor too chatty for the receive buffer falls back
//   to recorder/statistics_during_period: 5-minute means with their minimum
//   and maximum, never more than ~23 KB a day.
//
//   ACTIVITY (anything that turns on and off): logbook/get_events - each change
//   WITH WHAT CAUSED IT (a person, an automation and what triggered it, another
//   entity, an action), which history does not carry. 0.5-2.3 KB a day here,
//   20 KB for a week of the Office group. A person is named through a
//   render_template over HA's person entities (no admin rights needed).
//
// ONE REQUEST AT A TIME. A new one replaces the last; a late answer to the old
// one is recognised by its id and dropped. gen() counts requests, so a window
// can tell its own answer from an earlier one.
//
// THREADS. request*() and loop() run on the loop task (sending is loop-task
// only - HaClient.h). onRaw() runs on the websocket task: it parses into the
// tables below and then publishes READY. The loop task reads the tables only
// while state() is READY, and nothing writes them again until the next
// request, which only the loop task makes.
// ---------------------------------------------------------------------------

enum class HistKind  : uint8_t { HIST_CHART, HIST_ACTIVITY };
enum class HistState : uint8_t { HIST_IDLE, HIST_WAITING, HIST_READY, HIST_FAILED };
enum class HistFail  : uint8_t {
    FAIL_NONE,
    FAIL_NO_LINK,     // no HA session
    FAIL_NO_CLOCK,    // the clock is not set yet: no time to ask from
    FAIL_TIMEOUT,     // HA did not answer
    FAIL_TOO_BIG,     // the answer did not fit the receive buffer
    FAIL_REFUSED,     // HA answered with an error
    FAIL_NO_MEMORY,   // no room for the tables
    FAIL_NO_DATA,     // HA keeps no history of this entity
};

// What caused a change, from the logbook's context fields.
enum class CauseKind : uint8_t { CAUSE_NONE, CAUSE_USER, CAUSE_AUTOMATION, CAUSE_ENTITY, CAUSE_ACTION, CAUSE_OTHER };

struct HistPoint {
    uint32_t t;        // seconds since 1970, UTC
    float    v;        // NAN: no value then (unavailable, unknown)
    float    lo, hi;   // a statistics bucket's min and max; NAN for a real change
};

struct HistEvent {
    uint32_t  t;
    char      state[16];   // HA's state word: "on", "off", "open"...
    CauseKind cause;
    char      who[48];     // a user id (CAUSE_USER: see personName()), an automation's name, an entity id, a service
    char      detail[96];  // HA's own sentence or the service, for the row's details
};

class HaHistory {
public:
    void begin(HaClient *ha);

    // Loop task: timeouts, the statistics fallback, cancelling the names
    // template once it has answered.
    void loop(uint32_t nowMs);

    // Loop task. False when nothing was sent - state() says why.
    bool requestChart(const char *entityRef, uint32_t spanSec);
    bool requestActivity(const char *entityRef, uint32_t spanSec);

    HistState state() const { return (HistState)_state.load(); }
    HistFail  fail()  const { return (HistFail)_fail.load(); }
    uint32_t  gen()   const { return _gen; }
    HistKind  kind()  const { return _kind; }
    bool      fromStatistics() const { return _stats; }
    uint32_t  startSec() const { return _startSec; }
    uint32_t  endSec()   const { return _endSec; }

    // Valid while state() is READY.
    uint16_t         points() const { return _nPoints; }
    const HistPoint &point(uint16_t i) const { return _points[i]; }
    uint16_t         events() const { return _nEvents; }   // oldest first
    const HistEvent &event(uint16_t i) const { return _events[i]; }
    uint16_t         eventsDropped() const { return _eventsDropped; }   // the oldest, past the table

    // A person's name for a user id, or null if HA has not said.
    const char *personName(const char *userId) const;

    // The size and time of the last answer, for the log and the System Doctor.
    uint32_t lastBytes() const { return _lastBytes; }
    uint32_t lastMs()    const { return _lastMs; }

    // HaProvider's raw hook (websocket task): true when the message was ours.
    static bool onRaw(const char *json, size_t len, void *ctx);

private:
    bool ensureTables();
    bool start(HistKind kind, const char *entityRef, uint32_t spanSec);
    bool sendHistory();
    bool sendStatistics();
    bool sendLogbook();
    void sendNames();
    bool handle(const char *json, size_t len);
    void parseHistory(const char *json, size_t len);
    void parseStatistics(const char *json, size_t len);
    void parseLogbook(const char *json, size_t len);
    void parseNames(const char *json, size_t len);
    void finish(HistFail why);
    void addPoint(uint32_t t, float v, float lo, float hi);

    HaClient *_ha = nullptr;

    HistKind _kind  = HistKind::HIST_CHART;
    bool     _stats = false;          // the chart came from statistics (the fallback)
    char     _ref[64] = {0};
    uint32_t _startSec = 0, _endSec = 0;
    uint32_t _gen = 0;
    uint32_t _sentMs = 0;
    uint32_t _oversizedAt = 0;        // HaClient::oversized() when the request went

    std::atomic<uint8_t>  _state{(uint8_t)HistState::HIST_IDLE};
    std::atomic<uint8_t>  _fail{(uint8_t)HistFail::FAIL_NONE};
    std::atomic<uint32_t> _wantId{0};

    // The tables, allocated on the first request: in PSRAM where there is any.
    HistPoint *_points = nullptr;
    HistEvent *_events = nullptr;
    uint16_t   _pointCap = 0, _eventCap = 0;
    uint16_t   _nPoints = 0, _nEvents = 0, _eventsDropped = 0;
    uint32_t   _stride = 1, _strideAt = 0;   // thinning a chart that outgrew its table

    // People: user id -> name, from HA's person entities, once per session.
    struct Person { char id[33]; char name[24]; };
    static constexpr uint8_t PEOPLE_MAX = 8;
    Person   _people[PEOPLE_MAX] = {};
    std::atomic<uint8_t>  _nPeople{0};
    uint32_t              _namesSession = 0;
    std::atomic<uint32_t> _namesId{0};
    std::atomic<bool>     _namesDone{false};

    uint32_t _lastBytes = 0, _lastMs = 0;
};

#endif // HA_HISTORY_H
