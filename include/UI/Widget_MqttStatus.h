#pragma once
#include <lvgl.h>
#include "MqttManager.h"
#include "UI/HeaderSlots.h"   // GlyphStyle
#include "UI/Kit.h"           // Kit::StrokeGlyph

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
    //
    // `style` frames the glyph as the Wi-Fi glyph's is (2.8, #19); the arrows
    // are the same size in every style and have no square of their own (2.8
    // round 3, D-24: UI/GlyphShapes.h). `px` (0: the header's glyph) is the
    // drawing box.
    void init(lv_obj_t *parent, MqttManager *mqtt, GlyphStyle style = GLYPH_DEFAULT, int32_t px = 0);
    void tick();
    void repaint();                    // a scheme change: draw the current state again
    void setMotion(Motion m);
    lv_obj_t *getRoot() { return _frame; }

private:
    lv_color_t glyphCol(lv_color_t state);   // as Widget_ConnStatus's
    // Mirrors the artifact's state table rather than MqttState 1:1, because
    // several transport states share one appearance: CONNECTING and BACKOFF
    // are both "working on it" to a person walking past.
    enum class Look : uint8_t { OK, CONNECTING, STALE, UNREACHABLE, NO_LINK, SESSION_OFF };

    Look resolveLook() const;
    void applyVisual(Look look);
    void tint(uint32_t col, bool struck = false);
    void startMotion();
    void stopMotion();

    MqttManager *_mqtt = nullptr;
    GlyphStyle   _style = GLYPH_DEFAULT;

    lv_obj_t *_frame  = nullptr;   // the glyph's frame (Kit::glyphFrame())
    Kit::StrokeGlyph _g;           // the two arrows (UI/GlyphShapes.h)

    Motion   _motion        = Motion::PULSE;
    bool     _motionRunning = false;
    uint32_t _lastPollMs    = 0;
    int32_t  _lastLook      = -1;
};
