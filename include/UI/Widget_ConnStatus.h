#pragma once
#include <lvgl.h>
#include "ConnectivityManager.h"
#include "UI/HeaderSlots.h"   // GlyphStyle
#include "UI/Kit.h"           // Kit::StrokeGlyph

// ---------------------------------------------------------------------------
// Connectivity status glyph - a WiFi fan (base dot + up to three arcs) whose
// arc count encodes signal strength and whose colour reinforces it, and a
// diagonal strike for deliberately-disabled.
//
// Design and full state table: the "Fleet Status Glyphs" artifact; the fan
// itself, since 2.8 round 3 (D-24, D-27), the "Linen Fill and Header Glyphs"
// mock's, drawn from UI/GlyphShapes.h - centred, and the same size bare or in
// a frame. Its colours are the scheme's (UIPalette SIG_*). An access point is
// no longer a badge under the fan: it is a glyph of its own, the AP mast, a
// claim in the status place (D-24); the words a badge used to carry ("RETRY",
// a dead link) are the system panel's CONNECTIVITY row's.
//
// Two rules from the mock are load-bearing rather than decorative:
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

    // `style` frames the fan (2.8, #19): bare, in the header's rounded square,
    // or in a circle of the state's colour with the fan drawn in the bar's.
    // Called again on new objects (the header rebuilds its slots), it starts
    // over. `px` (0: the header's glyph) is the drawing box.
    void init(lv_obj_t *parent, ConnectivityManager *mgr, GlyphStyle style = GLYPH_DEFAULT, int32_t px = 0);
    void tick();                       // call from loop(); self-throttling
    void repaint();                    // a scheme change: draw the current state again
    void setMotion(Motion m);
    lv_obj_t *getRoot() { return _frame; }

private:
    // Every colour the glyph is given passes through here: for the circle it
    // fills the frame and hands back the bar's colour for the strokes.
    lv_color_t glyphCol(lv_color_t state);
    lv_color_t groundCol(lv_color_t state) const;   // what a strike's halo is drawn in
    void applyVisual(ConnState st, SignalBand band, LinkHealth health, bool retrying);
    void setArcs(int litCount, uint32_t col);
    // "Connected, strength unknown" - all three arcs ghosted in the link
    // colour with a solid dot. Distinct from every other state on purpose:
    // one arc would read as "weak", three as "excellent", and both would be
    // claims we cannot support once the RSSI sample has gone stale.
    void setArcsUnknown(uint32_t col);
    void startMotion();
    void stopMotion();

    ConnectivityManager *_mgr  = nullptr;
    GlyphStyle _style          = GLYPH_DEFAULT;
    lv_obj_t *_frame           = nullptr;   // the glyph's frame (Kit::glyphFrame())
    Kit::StrokeGlyph _g;                    // the fan (UI/GlyphShapes.h)

    Motion   _motion           = Motion::PULSE;
    bool     _motionRunning    = false;
    uint32_t _lastPollMs       = 0;
    // Change guard: state + band + health + retrying, packed. Health and
    // retrying joined the key when they joined the visual - a state the glyph
    // can draw but the guard cannot see is a glyph that never updates, which
    // is a silent failure and exactly the sort this indicator exists to stop.
    int32_t  _lastKey          = -1;
};
