#include "HaProvider.h"

#include <ArduinoJson.h>
#include "HaValue.h"
#include <string.h>
#include <stdlib.h>

#ifdef DEBUG_HA
  #define DBG_HAP(...) Serial.printf("[HaProv] " __VA_ARGS__)
#else
  #define DBG_HAP(...) do {} while (0)
#endif

void HaProvider::begin(EntityRegistry *reg, HaClient *ha, HaRest *rest) {
    _reg  = reg;
    _ha   = ha;
    _rest = rest;
    if (_ha) _ha->setMessageHandler(&HaProvider::onMessage, this);
}

// ---------------------------------------------------------------------------
// Subscription - loop task
// ---------------------------------------------------------------------------

int HaProvider::sendSubscribe(uint8_t from, uint8_t to) {
    if (!_reg || !_ha) return -1;

    // Built from the REGISTRY, not from a list, so an entity added anywhere
    // gets subscribed without touching this file.
    //
    // The frame is assembled by hand rather than with ArduinoJson. It is a
    // fixed shape with one repeated field, and serialising a document to build
    // it would mean holding both the document and the output at once on the
    // loop task for no benefit.
    static char frame[1536];

    uint32_t id = _ha->nextId();
    int n = snprintf(frame, sizeof(frame),
                     "{\"id\":%lu,\"type\":\"subscribe_trigger\","
                     "\"trigger\":{\"platform\":\"state\",\"entity_id\":[",
                     (unsigned long)id);

    uint8_t named = 0;
    for (uint8_t i = from; i < to; i++) {
        const Entity *e = _reg->at(i);
        if (!e) continue;
        if (e->desc.source != EntitySource::HA) continue;
        if (e->desc.externalRef[0] == '\0') continue;

        int add = snprintf(frame + n, sizeof(frame) - n, "%s\"%s\"",
                           named ? "," : "", e->desc.externalRef);
        if (add < 0 || n + add >= (int)sizeof(frame) - 8) {
            // Refuse rather than send a truncated subscription. A half list
            // would silently drop entities and look like HA not reporting
            // them - the exact class of lying diagnostic this project keeps
            // paying for.
            Serial.printf("[HaProv] subscription frame full at %u entities; "
                          "raise the buffer or split the request.\n",
                          (unsigned)named);
            return -1;
        }
        n += add;
        named++;
    }

    if (named == 0) {
        DBG_HAP("no HA-sourced entities registered; nothing to subscribe to\n");
        return 0;
    }

    int tail = snprintf(frame + n, sizeof(frame) - n, "]}}");
    if (tail < 0 || n + tail >= (int)sizeof(frame)) return -1;
    n += tail;

    if (!_ha->sendText(frame, n)) return -1;

    // Remembered so the reply can be matched. Cleared when it arrives.
    _pendingSubId = id;
    _subAccepted  = false;

    Serial.printf("[HaProv] subscribe_trigger id %lu for %u entities (%d B)%s\n",
                  (unsigned long)id, (unsigned)named, n, from ? ", learnt since" : "");
    return named;
}

bool HaProvider::sendSceneQuery() {
    if (!_reg || !_ha) return false;

    // The lights we were given (not learnt members - a single bulb's device
    // carries no scenes). Single quotes inside, so the JSON needs no escapes.
    // 3 KB since 2.10d: the same template also asks each entity's area.
    static char frame[3072];
    const uint32_t id = _ha->nextId();
    int n = snprintf(frame, sizeof(frame),
                     "{\"id\":%lu,\"type\":\"render_template\",\"template\":\"{%% for l in [",
                     (unsigned long)id);
    uint8_t named = 0;
    for (uint8_t i = 0; i < _reg->count(); i++) {
        const Entity *e = _reg->at(i);
        if (!e || e->learnt || e->desc.source != EntitySource::HA) continue;
        if (strncmp(e->desc.externalRef, "light.", 6) != 0) continue;
        const int add = snprintf(frame + n, sizeof(frame) - n, "%s'%s'", named ? "," : "",
                                 e->desc.externalRef);
        if (add < 0 || n + add >= (int)sizeof(frame) - 1400) {
            Serial.printf("[HaProv] scene query frame full at %u lights\n", (unsigned)named);
            break;
        }
        n += add;
        named++;
    }
    // No lights is no longer "nothing to ask": the areas below are asked of
    // every entity, and an empty list is a loop that does nothing.

    // Measured from the PC, 2026-10-06: 367 characters of template, 300 bytes
    // back for light.office's seven Hue scenes, sorted by name. The last field
    // says whether the owner hid the scene in HA's UI (is_hidden_entity, HA's
    // own test of the entity registry's hidden_by - checked 2026-10-07: four
    // of Office's seven are hidden).
    const int tail = snprintf(frame + n, sizeof(frame) - n,
        "] %%}{%% set d = device_id(l) %%}{%% if d %%}"
        "{%% for s in device_entities(d) | sort if s.startswith('scene.') %%}"
        "{{ l }}>{{ s }}>{{ state_attr(s,'name') or state_attr(s,'friendly_name') }}>"
        "{{ 'H' if is_hidden_entity(s) else 'V' }}|"
        "{%% endfor %%}{%% endif %%}{%% endfor %%}");
    if (tail < 0 || n + tail >= (int)sizeof(frame)) return false;
    n += tail;

    // EVERY ENTITY'S AREA (2.10d, K44, K46): "@>sensor.x>office>Office|" -
    // HA's area_id() and area_name(), the entity's own area or else its
    // device's; empty when it has none. For a card label set to "HA area". Learnt members are asked
    // too: a member can be a card's primary one day (K17).
    int add = snprintf(frame + n, sizeof(frame) - n, "{%% for e in [");
    if (add < 0 || n + add >= (int)sizeof(frame)) return false;
    n += add;
    uint8_t asked = 0;
    for (uint8_t i = 0; i < _reg->count(); i++) {
        const Entity *e = _reg->at(i);
        if (!e || e->desc.source != EntitySource::HA || e->desc.kind == EntityKind::BUTTON) continue;
        add = snprintf(frame + n, sizeof(frame) - n, "%s'%s'", asked ? "," : "", e->desc.externalRef);
        if (add < 0 || n + add >= (int)sizeof(frame) - 120) {
            Serial.printf("[HaProv] area query frame full at %u entities\n", (unsigned)asked);
            break;
        }
        n += add;
        asked++;
    }
    add = snprintf(frame + n, sizeof(frame) - n,
                   "] %%}@>{{ e }}>{{ area_id(e) or '' }}>{{ area_name(e) or '' }}|{%% endfor %%}\"}");
    if (add < 0 || n + add >= (int)sizeof(frame)) return false;
    n += add;
    if (!named && !asked) return true;   // nothing to ask this session

    _tplDone.store(false);
    _tplId.store(id);
    if (!_ha->sendText(frame, n)) { _tplId.store(0); return false; }
    Serial.printf("[HaProv] render_template id %lu: the scenes of %u lights, the areas of %u entities (%d B)\n",
                  (unsigned long)id, (unsigned)named, (unsigned)asked, n);
    return true;
}

// "light.office>scene.office_bright>Bright>V|light.office>scene...>H|".
// Grouped by light, as the template writes them; each group is learnt in one
// call. The last field: V visible, H hidden in HA's UI.
void HaProvider::parseScenes(const char *result) {
    static char buf[4096];   // websocket task only; the areas (2.10d) follow the scenes
    snprintf(buf, sizeof(buf), "%s", result ? result : "");

    const char *refs[ENTITY_SCENES_MAX], *names[ENTITY_SCENES_MAX];
    bool hidden[ENTITY_SCENES_MAX];
    uint8_t n = 0, lights = 0, scenes = 0, nHidden = 0;
    const char *light = nullptr;

    auto flush = [&]() {
        if (!light || !n) return;
        for (uint8_t i = 0; i < _reg->count(); i++) {
            const Entity *e = _reg->at(i);
            if (!e || e->desc.source != EntitySource::HA) continue;
            if (strcmp(e->desc.externalRef, light) != 0) continue;
            _reg->learnScenes(e->desc.id, refs, names, hidden, n);
            lights++;
            scenes += n;
            break;
        }
        n = 0;
    };

    uint8_t areas = 0, withArea = 0;
    char *save = nullptr;
    for (char *tok = strtok_r(buf, "|", &save); tok; tok = strtok_r(nullptr, "|", &save)) {
        char *a = strchr(tok, '>');
        char *b = a ? strchr(a + 1, '>') : nullptr;
        if (!a || !b) continue;
        // "@>sensor.x>office>Office": an entity's area id and name (2.10d),
        // not a scene.
        if (tok[0] == '@' && a == tok + 1) {
            *b = '\0';
            char *c2 = strchr(b + 1, '>');
            if (c2) *c2 = '\0';
            const char *ref = a + 1, *areaId = b + 1, *area = c2 ? c2 + 1 : "";
            for (uint8_t i = 0; i < _reg->count(); i++) {
                const Entity *e = _reg->at(i);
                if (!e || e->desc.source != EntitySource::HA || strcmp(e->desc.externalRef, ref) != 0) continue;
                _reg->setSourceArea(e->desc.id, areaId, area);
                areas++;
                if (area[0]) withArea++;
                break;
            }
            continue;
        }
        char *c = strchr(b + 1, '>');   // absent from an older template: visible
        *a = '\0';
        *b = '\0';
        if (c) *c = '\0';
        if (!light || strcmp(light, tok) != 0) { flush(); light = tok; }
        if (n < ENTITY_SCENES_MAX) {
            refs[n] = a + 1;
            names[n] = b + 1;
            hidden[n] = c && c[1] == 'H';
            if (hidden[n]) nHidden++;
            n++;
        }
    }
    flush();
    Serial.printf("[HaProv] scenes: %u for %u lights, %u hidden in HA; areas: %u of %u entities have one\n",
                  (unsigned)scenes, (unsigned)lights, (unsigned)nHidden, (unsigned)withArea, (unsigned)areas);
}

void HaProvider::loop(uint32_t nowMs) {
    (void)nowMs;
    if (!_ha) return;

    const uint32_t session = _ha->sessions();

    // Not ready, or no session yet: nothing to do. Note we do NOT clear
    // _subscribedForSession here - it holds the session number it was valid
    // for, so a reconnect is detected by the number CHANGING rather than by
    // having seen a disconnected state in between. A drop and recovery that
    // both happen between two loop() calls would be invisible to a boolean.
    if (!_ha->isReady() || session == 0) return;

    if (_subscribedForSession == session) {
        // ENTITIES LEARNT SINCE (2.10c): the members of a group, registered
        // when it reported. A second subscribe_trigger for just them - HA keeps
        // both - and HaRest fetches their values on its own, since it walks
        // the table to its end whenever the table has grown.
        const uint8_t count = _reg->count();
        if (count > _subscribedCount) {
            if (sendSubscribe(_subscribedCount, count) < 0) return;   // retried next loop
            _subscribedCount = count;
        }
        // A pause saved for a member could not be applied at boot: it did not
        // exist yet.
        if (_reg->takeLearnt()) _reg->restorePaused();

        // The lights' scenes, once per session; then cancel the template,
        // which would otherwise go on re-rendering.
        if (_scenesForSession != session && sendSceneQuery()) _scenesForSession = session;
        const uint32_t tpl = _tplId.load();
        if (tpl && _tplDone.exchange(false)) {
            char frame[96];
            const int n = snprintf(frame, sizeof(frame),
                                   "{\"id\":%lu,\"type\":\"unsubscribe_events\",\"subscription\":%lu}",
                                   (unsigned long)_ha->nextId(), (unsigned long)tpl);
            _ha->sendText(frame, n);
            _tplId.store(0);
        }
        return;
    }

    const uint8_t count = _reg->count();
    if (sendSubscribe(0, count) <= 0) return;
    _subscribedForSession = session;
    _subscribedCount      = count;
    _tplId.store(0);   // a template from the last session died with it
    _tplDone.store(false);

    // Subscribing and re-fetching are ONE event, so they happen in one place.
    //
    // The order matters and it is subscribe-then-fetch, not the reverse. The
    // subscription is live from the moment HA accepts it, so anything that
    // changes DURING the fetch pass still arrives; the fetch then fills in
    // everything that was already sitting still. Fetching first would leave a
    // window where a change could land between the read and the subscribe and
    // be lost silently - the value would sit wrong on screen until the next
    // time that entity happened to move, which for a light could be days.
    if (_rest) _rest->restart();
}

// ---------------------------------------------------------------------------
// Receive - WEBSOCKET TASK
// ---------------------------------------------------------------------------

void HaProvider::onMessage(const char *json, size_t len, void *ctx) {
    HaProvider *self = (HaProvider *)ctx;
    if (self) self->handle(json, len);
}

void HaProvider::handle(const char *json, size_t len) {
    if (!_reg) return;

    // A FILTERED PARSE, because ~95% of every trigger event is discardable.
    //
    // ha-websocket.md section 3 measured one trigger event at 1,365 bytes of
    // which we keep 71: the entity id, the state and the icon. old_state alone
    // is 488 bytes we never read. ArduinoJson's filter skips the unwanted
    // subtrees during parsing rather than after, so the document never holds
    // them and the websocket task's stack never has to.
    static JsonDocument filter;
    if (filter.isNull() || filter.size() == 0) {
        JsonObject ts = filter["event"]["variables"]["trigger"]
                              .to<JsonObject>()["to_state"].to<JsonObject>();
        ts["entity_id"]               = true;
        ts["state"]                   = true;
        // A light's attributes, and a group's members. Helpers shared with
        // HaRest, so the two readers cannot disagree about what is kept - see
        // HaValue.h. One object: to<JsonObject>() would clear it a second time.
        JsonObject attrs = ts["attributes"].to<JsonObject>();
        haAttrFilter(attrs);
        haEntityFilter(attrs);
        filter["event"]["result"]     = true;   // a template's answer (2.10c)
        filter["type"]                = true;
        // Needed to read the subscription's own reply. Without these three the
        // filtered parse drops them and every result looks like success=false.
        filter["id"]                  = true;
        filter["success"]             = true;
        filter["error"]["code"]       = true;
        filter["error"]["message"]    = true;
    }

    JsonDocument doc;
    DeserializationError err = deserializeJson(
        doc, json, len, DeserializationOption::Filter(filter));
    if (err) {
        _parseFails++;
        DBG_HAP("parse failed (%u B): %s\n", (unsigned)len, err.c_str());
        return;
    }

    const char *type = doc["type"] | "";

    // THE REPLY TO OUR OWN SUBSCRIPTION, AND IT IS WORTH READING.
    //
    // This was originally discarded with every other "result", which meant a
    // REFUSED subscription looked exactly like an accepted one that nothing
    // had happened on yet - and since these entities can be quiet for hours,
    // "no events" is the normal case. The failure would have surfaced as "the
    // dashboard never updates", days later, with nothing in the log.
    //
    // HA answers {"id":N,"type":"result","success":true|false,...}. We match
    // on the id we sent and say so either way.
    if (strcmp(type, "result") == 0) {
        uint32_t rid = doc["id"] | 0UL;
        if (rid != 0 && rid == _pendingSubId) {
            _pendingSubId = 0;
            bool ok = doc["success"] | false;
            _subAccepted = ok;
            if (ok) {
                Serial.printf("[HaProv] HA ACCEPTED the subscription (id %lu)\n",
                              (unsigned long)rid);
            } else {
                const char *code = doc["error"]["code"]    | "?";
                const char *msg  = doc["error"]["message"] | "";
                Serial.printf("[HaProv] HA REFUSED the subscription (id %lu): %s - %s\n",
                              (unsigned long)rid, code, msg);
            }
        } else if (rid != 0 && rid == _tplId.load()) {
            if (!(doc["success"] | false)) {
                Serial.printf("[HaProv] HA REFUSED the scene query: %s - %s\n",
                              doc["error"]["code"] | "?", doc["error"]["message"] | "");
                _tplId.store(0);
            }
        } else if (rid != 0 && _resultFn) {
            const bool ok = doc["success"] | false;
            _resultFn(rid, ok, ok ? "" : (doc["error"]["code"] | "?"),
                      ok ? "" : (doc["error"]["message"] | ""), _resultCtx);
        }
        return;
    }

    if (strcmp(type, "event") != 0) return;

    // The scene query's answer: learnt here, unsubscribed by loop().
    const uint32_t tpl = _tplId.load();
    if (tpl && (doc["id"] | 0UL) == tpl) {
        if (!_tplDone.load()) parseScenes(doc["event"]["result"] | "");
        _tplDone.store(true);
        return;
    }

    JsonVariantConst to = doc["event"]["variables"]["trigger"]["to_state"];
    if (to.isNull()) return;

    const char *ref   = to["entity_id"] | "";
    const char *state = to["state"]     | "";
    if (!ref[0] || !state[0]) return;

    // Map HA's entity id back to ours. A linear scan over the registry is
    // right at this size - 18 entities, a handful of events a minute - and it
    // keeps the mapping in exactly one place: the descriptor's externalRef.
    const Entity *match = nullptr;
    for (uint8_t i = 0; i < _reg->count(); i++) {
        const Entity *e = _reg->at(i);
        if (!e || e->desc.source != EntitySource::HA) continue;
        if (strcmp(e->desc.externalRef, ref) == 0) { match = e; break; }
    }
    if (!match) {
        _unmatched++;
        DBG_HAP("event for unregistered entity %s\n", ref);
        return;
    }

    // PAUSED: stop applying inbound values. Issue #60.
    //
    // Nothing to stop transmitting for an entity someone else owns, so pause
    // means the held value is whatever was there at the moment of the pause.
    // The display freezes as a CONSEQUENCE of the data stopping, not because
    // anything stopped rendering - which is what keeps the registry and the
    // screen telling the same story.
    //
    // This guard was written once and silently lost: the scripted edit that
    // was meant to insert it never matched, and the script reported success
    // anyway. The owner found it in thirty seconds by pausing a light and
    // toggling it in HA. See CLAUDE.md on confirming a scripted edit changed
    // the file.
    if (match->paused) {
        DBG_HAP("%s paused; inbound value dropped\n", match->desc.id);
        return;
    }

    // "unavailable" is not a missing value, it is a STATEMENT. Issue #56.
    //
    // Recording it is the whole point of #56: under a change-driven feed this
    // word is the only evidence of death that exists, because silence means
    // "unchanged" and carries no information at all.
    if (strcmp(state, "unavailable") == 0) {
        _reg->setAvailable(match->desc.id, false, millis());
        _unavailable++;
        _handled++;
        return;
    }

    // ATTRIBUTES BEFORE THE VALUE, and read on every event - including the ones
    // whose value did not change. A light fading down sends "on" per step with
    // a falling brightness; the value never moves and the attributes are the
    // whole story. setAttrs() dirties only on a real change.
    EntityAttrs attrs;
    haReadAttrs(to["attributes"], attrs);
    _reg->setAttrs(match->desc.id, attrs);
    // A learnt member's name; a group's members (2.10c). Anything learnt here
    // is subscribed to by loop(), on the loop task.
    haLearn(*_reg, *match, to["attributes"]);

    EntityValue v;
    if (!haCoerceState(match->desc, state, v)) {
        // "unknown" and anything unparseable. Distinct from unavailable: the
        // entity is there, it just has nothing meaningful to say yet, so the
        // last good value stays and no claim is made either way.
        DBG_HAP("%s -> %s (not a value; left alone)\n", match->desc.id, state);
        return;
    }

    // setValue() marks it available again - a value IS proof of life.
    _reg->setValue(match->desc.id, v, millis());
    _handled++;
}
