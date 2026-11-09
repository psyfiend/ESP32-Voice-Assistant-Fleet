#pragma once
//
// PanelDebug - GET /panel: ask a DSI panel how it is, and send it its init
// sequence again.
//
// For a panel whose picture is washed out (near-white, lurid colours) while
// /screenshot shows the frame buffer is right: the panel is showing a correct
// picture wrongly, as if its own settings (gamma, power) had been lost. Use
// it before resetting the board:
//
//   http://<board>/panel            the panel and its driver, and the underrun
//                                   count (below)
//   http://<board>/panel?resend=1   the same, then the BSP's init sequence sent
//                                   again (no reset)
//
// The panel's status registers are not read: a DSI read while the panel
// streams video stops the picture. If the resend brings the picture back, the
// panel lost its settings and a periodic check could put them back. Built only on DSI boards with the esp_lcd
// display (DISPLAY_ESPLCD + HAS_MIPI_PANEL) - SystemCore calls it there. No LVGL: the
// handler runs on the HTTP server's task and talks to the panel over the DSI
// command path, not the picture.
//
// UNDERRUNS. The DSI bridge feeds the panel from the frame buffer in
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
//   http://<board>/panel?flash=N&nvs=1    N small NVS writes (the kind a Pause
//                                         makes), 150 ms apart, each timed,
//                                         underruns counted around it, in a
//                                         scratch namespace erased afterwards
//
// It tests whether a flash write starves the panel: while the flash is busy the
// CPU cache is off, and the interrupt that starts each frame's transfer needs
// CONFIG_LCD_DSI_ISR_CACHE_SAFE (A14, in the rebuilt libs) to survive that.
//
// /panel?flash=N without nvs=1 is REFUSED (A16, #98): an erase mode would
// destroy the start of the "spiffs" partition, which is the LittleFS partition
// that holds the settings file.
//
class HttpServer;
class Fleet_Display;

namespace PanelDebug {
void begin(HttpServer &http, Fleet_Display &display);
void poll();
}
