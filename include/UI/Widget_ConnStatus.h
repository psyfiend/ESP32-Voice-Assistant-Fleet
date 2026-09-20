#pragma once
#include <lvgl.h>
#include "ConnectivityManager.h"

// ---------------------------------------------------------------------------
// Connectivity status glyph - a WiFi fan (base dot + up to three arcs) whose
// arc count encodes signal strength and whose colour reinforces it, plus an
// optional badge ("AP") and a diagonal strike for deliberately-disabled.
//
// Design and full state table: the "Fleet Status Glyphs" artifact.
// Two rules from it are load-bearing rather than decorative:
//   * The strike means CHOSEN, never BROKEN. Faults change colour and arc
//     count; only human intent draws the strike. If a failure can look like a
//     setting, nobody investigates it.
//   * Motion means TRANSIENT. Only connecting animates, so movement always
//     means "working" rather than being ambient decoration on a screen
//     somebody lives with.
//
// INSTANCE-BASED on purpose. Both reference projects use file-static widget
// handles, which permits exactly one on screen; the settings page will want a
// second copy of this same indicator.
//
// THREADING: tick() polls ConnectivityManager's atomic getters from the LVGL
// task. Nothing here is called from the WiFi event task - see the threading
// note in ConnectivityManager.h.
// ---------------------------------------------------------------------------

class Widget_ConnStatus {
public:
    // How the transient (connecting) state animates.
    //   PULSE - smooth 30-100% opacity ramp. Invalidates the icon region every
    //           frame, which is free on the DSI boards.
    //   BLINK - two-state toggle on a 1 s timer: ~2 invalidations/second
    //           instead of 30+. Chosen automatically on the QSPI board, whose
    //           software-rotation redraw path is already documented as sluggish.
    //   OFF   - no motion at all.
    enum class Motion : uint8_t { OFF, BLINK, PULSE };

    void init(lv_obj_t *parent, ConnectivityManager *mgr);
    void tick();                       // call from loop(); self-throttling
    void setMotion(Motion m);
    lv_obj_t *getRoot() { return _root; }

private:
    void applyVisual(ConnState st, SignalBand band, bool apUp,
                     LinkHealth health, bool retrying);
    void setArcs(int litCount, lv_color_t col, bool glow);
    // "Connected, strength unknown" - all three arcs ghosted in the link
    // colour with a solid dot. Distinct from every other state on purpose:
    // one arc would read as "weak", three as "excellent", and both would be
    // claims we cannot support once the RSSI sample has gone stale.
    void setArcsUnknown(lv_color_t col);
    void startMotion();
    void stopMotion();

    ConnectivityManager *_mgr  = nullptr;
    lv_obj_t *_root            = nullptr;
    lv_obj_t *_fan             = nullptr;   // fixed-size box holding the arcs
    lv_obj_t *_arc[3]          = {nullptr, nullptr, nullptr};
    lv_obj_t *_dot             = nullptr;
    lv_obj_t *_stalk           = nullptr;   // shown only when zero arcs are lit
    lv_obj_t *_strike          = nullptr;
    lv_obj_t *_badge           = nullptr;

    lv_point_precise_t _strikePts[2];

    Motion   _motion           = Motion::PULSE;
    bool     _motionRunning    = false;
    uint32_t _lastPollMs       = 0;
    // Change guard: state + band + ap + health + retrying, packed. Health and
    // retrying joined the key when they joined the visual - a state the glyph
    // can draw but the guard cannot see is a glyph that never updates, which
    // is a silent failure and exactly the sort this indicator exists to stop.
    int32_t  _lastKey          = -1;
};
