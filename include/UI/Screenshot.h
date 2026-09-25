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
class HttpServer;

namespace Screenshot {

#ifdef ENABLE_SCREENSHOT
// Registers GET /screenshot. Call once LVGL is up.
void begin(HttpServer &http);

// Call every loop(), from the LVGL thread. Does nothing unless a request is
// waiting; when one is, renders it and the UI pauses for the render. How long
// that is has not been measured yet - the [Shot] log line reports it.
void service();
#else
inline void begin(HttpServer &) {}
inline void service() {}
#endif

} // namespace Screenshot
