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
// UNDERRUNS (2.10d). The DSI bridge feeds the panel from the frame buffer in
// PSRAM; when that feed falls behind, the bridge sends filler instead of the
// picture and the panel flashes. The bridge latches each such event in a status
// bit. poll() - from SystemCore::loop() - reads it, counts it and logs it, so a
// flash is recorded whether or not anyone is watching. The count and the last
// times are in /panel's reply. begin() turns off ESP-IDF's own underrun
// interrupt, which would otherwise clear the bit first (PanelDebug.cpp).
//
// The test's writes run on a task of their own: a flash write asserts if the
// calling task's stack is in PSRAM, and the HTTP server's is.
//
// The recorder is blind to a frame's first 413 lines, where the bridge masks
// underruns by design - which is where a flash write's late frame start lands.
// So the flash test's zero means "no starvation later in a frame"; whether a
// write shows on the glass takes eyes (PanelDebug.cpp, "What the recorder
// cannot see").
//
//   http://<board>/panel?flash=N          N 4 KB erase+write cycles on the
//                                         unused data partition, 150 ms apart,
//                                         each timed, underruns counted around it
//   http://<board>/panel?flash=N&kb=K     the same with K KB erased per cycle
//                                         (4..256; 64 KB aligned is one block)
//   http://<board>/panel?flash=N&nvs=1    N small NVS writes instead (the kind
//                                         a Pause makes), in a scratch namespace
//                                         erased afterwards
//
// Both test whether a flash write starves the panel: while the flash is busy
// the CPU cache is off, and the interrupt that starts each frame's transfer is
// not built cache-safe in our framework (CONFIG_LCD_DSI_ISR_CACHE_SAFE off).
//
class HttpServer;
class Fleet_Display;

namespace PanelDebug {
void begin(HttpServer &http, Fleet_Display &display);
void poll();
}
