#pragma once
//
// PanelDebug - GET /panel: ask a DSI panel how it is, and send it its init
// sequence again (2.10c round 9).
//
// The WS_P4_4B was seen twice with its whole picture washed out - near-white,
// lurid colours - while /screenshot showed the frame buffer was right. So the
// panel itself was showing a correct picture wrongly, as if its own settings
// (gamma, power) had been lost. This is for the next time it happens, before
// anyone resets the board:
//
//   http://<board>/panel            the panel's status registers, now and as
//                                   read at boot, and whether they differ
//   http://<board>/panel?resend=1   the same, then the BSP's init sequence sent
//                                   again (no reset), then the registers again
//
// If the resend brings the picture back, the panel lost its settings and a
// periodic check could put them back. Built only on DSI boards with the esp_lcd
// display (DISPLAY_ESPLCD + HAS_MIPI_PANEL) - SystemCore calls it there. No LVGL: the
// handler runs on the HTTP server's task and talks to the panel over the DSI
// command path, not the picture.
//
class HttpServer;
class Fleet_Display;

namespace PanelDebug {
void begin(HttpServer &http, Fleet_Display &display);
}
