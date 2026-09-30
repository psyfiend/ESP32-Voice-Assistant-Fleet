#pragma once
//
// Screenshot - GET /screenshot returns a PNG of exactly what is on the glass.
// Issue #58.
//
// "Exactly": the active screen, then LVGL's top layer (toasts, the touch
// overlay) and system layer (the FPS/CPU monitor) composited over it, in the
// same order the display draws them. Rendered by LVGL from the object tree in
// LOGICAL orientation, so a board whose panel is rotated in software still
// gives an upright picture.
//
// Compiled in only with -D ENABLE_SCREENSHOT. The endpoint has NO
// authentication: anyone on the LAN who can reach the board can fetch its
// screen. Accepted by the owner until the Phase 4 web UI brings optional auth;
// the build flag is how a release build leaves it out.
//
// Threading, which is the whole design (see Screenshot.cpp):
//   - the HTTP handler runs on the server task and never touches LVGL
//   - it asks the LVGL thread for a capture and waits
//   - service(), called from loop(), renders the capture and hands it back
//   - the handler encodes the PNG and sends it, back on the server task
//
#include <stdint.h>

class HttpServer;

namespace Screenshot {

#ifdef ENABLE_SCREENSHOT
// Registers GET /screenshot. Call once LVGL is up.
void begin(HttpServer &http);

// Call every loop(), from the LVGL thread. Does nothing unless a request is
// waiting; when one is, renders it and the UI pauses for the render. How long
// that is has not been measured yet - the [Shot] log line reports it.
void service();

#ifdef DISPLAY_ESPLCD
// THE FLUSH'S CORRECTNESS CHECK (2.9): on the LVGL thread, flush everything
// pending, then compare - in one instant, so nothing can change in between -
// LVGL's render of the object tree with the frame buffer the panel is
// showing, pixel by pixel at RGB565 precision. `bad` is the number of pixels
// that differ and x1..y2 (logical) where. Any difference is a flush error:
// a missed repair, a strip in the wrong place, a stale buffer. Used by
// /bench?what=verify. False if a capture could not be made.
struct VerifyResult {
    bool     ok  = false;
    uint32_t bad = 0;        // more than 2 steps off in a channel: a flush error
    uint32_t near = 0;       // within 2 steps: overlay blending rounds differently
    int32_t  x1 = 0, y1 = 0, x2 = -1, y2 = -1;
};
bool verifyFrameBuffer(VerifyResult &out);
#endif
#else
inline void begin(HttpServer &) {}
inline void service() {}
#endif

} // namespace Screenshot
