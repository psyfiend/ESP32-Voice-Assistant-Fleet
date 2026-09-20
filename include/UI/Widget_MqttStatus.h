#pragma once
#include <lvgl.h>
#include "MqttManager.h"

// ---------------------------------------------------------------------------
// MQTT status glyph - "glyph A" from the Fleet Status Glyphs artifact: a
// rounded square holding two opposing arrows, one out and one back. Chosen
// over a cloud because the broker is on the LAN and a cloud quietly says
// otherwise, and because the shape survives a change of transport - it means
// "exchanging information", not "MQTT specifically". That matters with #43
// coming, where HA arrives over a websocket and this indicator should not have
// to be redrawn to keep telling the truth.
//
// Sibling to Widget_ConnStatus and deliberately the same shape of object: same
// instance-based design, same self-throttling tick(), same change guard, same
// Motion vocabulary. The two indicators sit next to each other in the header
// and must not behave differently for no reason.
//
// The two rules inherited from the artifact are load-bearing, not decorative:
//   * The strike means CHOSEN, never BROKEN. MQTT switched off draws a strike;
//     a broker that cannot be reached does not. If a failure can look like a
//     setting, nobody investigates it.
//   * Motion means TRANSIENT. Only connecting/backoff animates.
//
// WHY IT IS SEPARATE FROM THE WIFI GLYPH. They fail independently, and a
// device on a perfectly good network with a dead broker is routine. One
// combined light would hide which half broke - a debugging tax paid every
// time, and the exact confusion that cost days on issue #49, where "broker
// unreachable" was really a statement about the link.
//
// THREADING: tick() polls MqttManager from the LVGL task.
// Nothing here is called from another task.
// ---------------------------------------------------------------------------

class Widget_MqttStatus {
public:
    enum class Motion : uint8_t { OFF, BLINK, PULSE };

    // No registry, deliberately. Everything this widget reports comes from the
    // broker session itself - see FEED_SILENT_MS in the .cpp for why asking
    // the entity layer was the wrong answer twice.
    void init(lv_obj_t *parent, MqttManager *mqtt);
    void tick();
    void setMotion(Motion m);
    lv_obj_t *getRoot() { return _root; }

private:
    // Mirrors the artifact's state table rather than MqttState 1:1, because
    // several transport states share one appearance: CONNECTING and BACKOFF
    // are both "working on it" to a person walking past.
    enum class Look : uint8_t { OK, CONNECTING, STALE, UNREACHABLE, NO_LINK, SESSION_OFF };

    Look resolveLook() const;
    void applyVisual(Look look);
    void tint(lv_color_t col);
    void startMotion();
    void stopMotion();

    MqttManager *_mqtt = nullptr;

    lv_obj_t *_root   = nullptr;
    lv_obj_t *_box    = nullptr;   // the rounded square outline
    lv_obj_t *_out    = nullptr;   // upper arrow, pointing right (publish)
    lv_obj_t *_in     = nullptr;   // lower arrow, pointing left (subscribe)
    lv_obj_t *_strike = nullptr;

    lv_point_precise_t _outPts[3];
    lv_point_precise_t _inPts[3];
    lv_point_precise_t _strikePts[2];

    Motion   _motion        = Motion::PULSE;
    bool     _motionRunning = false;
    uint32_t _lastPollMs    = 0;
    int32_t  _lastLook      = -1;
};
