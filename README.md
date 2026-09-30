# ESP32 Voice Assistant Fleet

One firmware for a fleet of ESP32-S3 and ESP32-P4 touchscreen panels - wall-mounted Home Assistant
dashboards today, local voice assistants later. Nine boards from Waveshare and Guition, 3.5" to
7", one codebase, the board picked at build time.

**Status:** hobby project, in active development (`v0.2.7`, Phase 2 of the
[roadmap](docs/ROADMAP.md)). The dashboard runs on every board; voice is parked, not abandoned.
**License: not chosen yet** - until one is added, all rights are reserved.

## The dashboard idea

The screen is a grid of **cards**, and every card is bound to an **entity** - a light, a
temperature, a door, the panel's own Wi-Fi signal. Entities come from **providers** that write into
one thread-safe registry: Home Assistant over its websocket API, MQTT (Zigbee2MQTT and friends),
and the panel's own telemetry, which it publishes back to Home Assistant through MQTT discovery.
Cards never talk to a source; they render whatever the registry holds, and grey out when it
goes stale.

Pages are declared as data, not code: a list of card specs with a label, an area and a
**priority**. The grid is **derived from the screen**, never declared: a card has a target width
in millimetres, the panel's real pixel density gives the column count, and when a small screen
runs out of room the lowest-priority cards drop first. One page definition gives 7 columns on a 7"
panel and 2 on a 3.5" portrait one - a true subset, not a different page. Swipe between pages;
tap a card to toggle it (long-press controls - brightness, colour - are next on the roadmap).

## What makes it interesting

- **One codebase, nine boards.** Each board is a single header (`BSP_<BOARD>.h`) of flat `const`
  structs - display, touch, audio, storage - that app code reads through fixed names
  (`bsp_display.WIDTH`). Adding a board is a header and an environment, not an `#ifdef` hunt.
- **Scale from physics.** UI size comes from each panel's measured pixels per inch, so text and
  touch targets are the same size in millimetres from 165 to 294 PPI. Design tokens (colour,
  spacing, type) live in one header; three colour schemes switch live.
- **A display stack moved to raw `esp_lcd`**, board by board, measured at every step: on the P4s
  the hardware 2D engine (PPA) rotates each strip while LVGL draws the next, with triple buffering
  and on-device verification that the glass gets exactly what LVGL rendered. Full-screen redraws
  went from 163 to 85 ms on the 5" P4. How frames reach each panel is derived from the bus and
  rotation, and the panel driver is picked by name from the board header - no per-chip branches.
- **Verify from outside the device.** Every board serves `/screenshot` (a PNG of the screen) and
  `/bench` (draw/flush timing, animation traces, and a frame-buffer check), and a built-in
  **System Doctor** reports firmware, memory, network, display, power and I2C on screen and serial.
- **ESP-IDF first.** Arduino-ESP32 is the framework today, but new code prefers IDF facilities
  (`esp_lcd`, `esp_http_server`, the oneshot ADC driver) so a move to pure ESP-IDF stays a port,
  not a rewrite.

## The boards

| Environment | Board | SoC | Screen |
|---|---|---|---|
| `WS_P4_TOUCH_LCD_7B` | Waveshare ESP32-P4-WIFI6-Touch-LCD-7B | P4 + C6 | 7" 1024x600 MIPI-DSI |
| `WS_P4_TOUCH_LCD_5` | Waveshare ESP32-P4-WIFI6-Touch-LCD-5 | P4 + C6 | 5" 720x1280 MIPI-DSI |
| `WS_P4_TOUCH_LCD_4B` | Waveshare ESP32-P4-WIFI6-Touch-LCD-4B | P4 + C6 | 4" 720x720 MIPI-DSI |
| `CYD_P4_1060P470` | Guition JC1060P470 | P4 + C6 | 7" 1024x600 MIPI-DSI |
| `CYD_P4_4880P443` | Guition JC4880P443 | P4 + C6 | 4.3" 480x800 MIPI-DSI |
| `WS_S3_TOUCH_LCD_4B` | Waveshare ESP32-S3-Touch-LCD-4B | S3 | 4" 480x480 RGB |
| `WS_S3_TOUCH_LCD_5B` | Waveshare ESP32-S3-Touch-LCD-5B | S3 | 5" 1024x600 RGB |
| `CYD_S3_8048W550` | Guition JC8048W550 | S3 | 5" 800x480 RGB |
| `CYD_S3_3248W535` | Guition JC3248W535 | S3 | 3.5" 320x480 QSPI |

Per-board status and quirks: [docs/HARDWARE_STATUS.md](docs/HARDWARE_STATUS.md) and, for the
display, [docs/display/README.md](docs/display/README.md).

## How it is built

- **PlatformIO** with [pioarduino](https://github.com/pioarduino/platform-espressif32)
  (Arduino-ESP32 3.3 on ESP-IDF 5.5), **LVGL 9.5**, one environment per board.
- **Layers.** `src/main.cpp` does `setup()`/`loop()` only. `SystemCore` owns every piece of hardware
  and every non-UI subsystem, and includes no LVGL header; `LVGL_Startup` owns the LVGL engine;
  `GUIManager` owns screen content. UI code registers with lower layers - lower layers never reach
  up into the UI. LVGL runs on the loop task; providers never touch it.
- **Components** (`components/`): `Fleet_BSP` (the board headers), `Fleet_Display` (the esp_lcd
  stack and vendored panel drivers), `Fleet_Entities` (the registry), `Fleet_Providers`,
  `Fleet_HA`, `Fleet_MQTT`, `Fleet_Connectivity`, `AudioManager`, `TouchManager`, plus two
  submodules: LVGL (stock) and a fork of Arduino_GFX (on its way out).

### Building

```
git clone --recurse-submodules https://github.com/psyfiend/ESP32-Voice-Assistant-Fleet.git
pio run -e WS_P4_TOUCH_LCD_5 -t upload --upload-port COMx
```

Things to know first:

- `platformio.ini` links the local libraries by **absolute path** - edit the prefix if you clone
  somewhere else (why: [CLAUDE.md](CLAUDE.md)).
- Wi-Fi, MQTT and Home Assistant credentials go in two git-ignored headers,
  `components/Fleet_Connectivity/ConnectivityLocalSecrets.h` and
  `components/Fleet_MQTT/MqttLocalSecrets.h`; the matching `*Defaults.h` files say what they hold.
  Without them a board still boots and shows local content.
- The P4 boards run on **rebuilt** framework libraries that fix Wi-Fi dropouts on the C6
  co-processor link ([docs/REBUILD_P4_LIBS.md](docs/REBUILD_P4_LIBS.md)).

## Documentation

| | |
|---|---|
| [CLAUDE.md](CLAUDE.md) | How the HAL, BSP and the rest work - the stable reference |
| [docs/HANDOFF.md](docs/HANDOFF.md) | Where development is right now |
| [docs/ROADMAP.md](docs/ROADMAP.md) | What is being built, in what order |
| [docs/LESSONS.md](docs/LESSONS.md) | Mistakes worth not repeating |
| [docs/display/](docs/display/README.md) | The display stack |
| [docs/design/](docs/design/) | Cards, pages, tokens, startup and the rest of the UI design |

Built with a great deal of help from Claude (Anthropic), whose session notes are much of the above.
