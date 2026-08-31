# Project Status

Bugs, fixes, additions, and enhancements that apply to a **specific device or a small group
of devices** — not fleet-wide HAL/environment concerns (that's `FUTURE_IMPROVEMENTS.md`'s
job). Volatile; update or prune entries as they get resolved, don't let this file grow stale.

## The BSP, briefly

Each board's `components/Fleet_BSP/include/BSP_<NAME>.h` declares a short device-identity
`#define` (e.g. `WS_P4_7B`) plus up to seven `const` struct instances — one per functional
group (`BoardHardware`, `ExpanderConfig`, `DisplayConfig`, `TouchConfig`, `LvglConfig`,
`AudioConfig`, `StorageConfig`) — each aliased to a fixed lowercase name app code reads
(`bsp_hw`, `bsp_display`, `bsp_touch`, etc.). `platformio.ini` selects a board per environment
via `-D BSP_HEADER='"BSP_<NAME>.h"'` alone; that one flag is the only thing that varies
per-environment for board selection. Full architecture/rationale: `CLAUDE.md`'s BSP pattern
section.

## Per-board test status

| Board | Env | Display | Touch | Audio out | Audio in (mic) |
|---|---|---|---|---|---|
| **ESP32-P4-WIFI6-Touch-LCD-7B** (WaveShare, macro `WS_P4_7B`) | `WS_P4_TOUCH_LCD_7B` | ✅ | ✅ (portrait; rotation untested) | untested (no speaker access — enclosure doesn't expose it) | untested |
| **ESP32-P4-WIFI6-Touch-LCD-4B** (WaveShare, macro `WS_P4_4B`) | `WS_P4_TOUCH_LCD_4B` | ✅ | ✅ | ✅ | ✅ |
| **ESP32-P4-WIFI6-Touch-LCD-5** (WaveShare, macro `WS_P4_5`) | `WS_P4_TOUCH_LCD_5` | untested (not flashed yet) | untested | untested | untested |
| **ESP32-S3-Touch-LCD-4B** (WaveShare, macro `WS_S3_4B`) | `WS_S3_TOUCH_LCD_4B` | ✅ | ✅ | ✅ | ✅ |
| Guition P4 7" (JC1060P470C, macro `CYD_P4_1060`) | `CYD_P4_1060P470` | ✅ | ✅ | ✅ | ✅ (first-ever test of the ES8311-as-sole-mic-input path) |
| Guition 3.5" (JC3248W535, macro `CYD_S3_3248`) | `CYD_S3_3248W535` | ✅ (both rotations confirmed) | ✅ (both rotations confirmed) | ✅ | ✅ |
| Guition 5" (JC8048W550, macro `CYD_S3_8048`) | `CYD_S3_8048W550` | ✅ (brightness slider broken, see below) | ✅ | ✅ (notably quieter than other boards, unexplained) | ✅ |
| WaveShare ESP32-S3-Touch-LCD-5B (macro `WS_S3_5B`) | `WS_S3_TOUCH_LCD_5B` | ✅ (visible tearing, see `FUTURE_IMPROVEMENTS.md`) | ✅ (5 simultaneous points confirmed) | N/A (no audio hardware) | N/A |

## Fleet-wide investigations

- **WiFi / MQTT** — not started. See `FUTURE_IMPROVEMENTS.md`'s Connectivity section for what
  "tested" actually requires.
- **SD card** — untested on every board. Low priority.
- **Battery ADC "gauge"** — no board has a working battery percentage readout yet. Low
  priority. See `FUTURE_IMPROVEMENTS.md` for the voltage-divider math already confirmed for
  the 3248W535.
- **Onboard buttons** — inventory which boards have a usable button beyond BOOT, and whether
  firmware does anything with it yet (currently: nothing does).
- **LVGL optimization** — see `FUTURE_IMPROVEMENTS.md`'s LVGL/Display section (buffering,
  brightness floor, PPA rotation, RGB panel double-buffering).

## Per-device notes

### WS_P4_7B — ESP32-P4-WIFI6-Touch-LCD-7B

- Test rotation on real hardware to determine whether `TouchManager`'s `#ifndef WS_P4_7B`
  raw-passthrough special case is still needed, or whether the generic rotation transform
  works fine here too (unverified assumption, not a known problem).
- Test RS485 (Modbus board available).
- Nothing else board-specific outstanding — remaining items are fleet-wide, see above.

### WS_P4_5 — ESP32-P4-WIFI6-Touch-LCD-5

No testing done yet at all — BSP built and compiles, not flashed. See
`FUTURE_IMPROVEMENTS.md`'s New Hardware section.

### WS_P4_4B — ESP32-P4-WIFI6-Touch-LCD-4B

Does **not** have battery headers (the only P4 board without one, as far as known). Has 2x
14-pin expansion headers (7x2, 2.0mm pitch), intended for an optional relay/ethernet
component not currently owned. Believed to expose: 5V bus, USB data (`USB_IN1_P`/`USB_IN1_N`),
and SDA/SCL.

### WS_S3_4B — ESP32-S3-Touch-LCD-4B

On-board hardware not yet touched by firmware at all — develop libraries for each, reference
Waveshare's own repo:
- **AXP2101** power management chip
- **PCF85063** RTC clock chip
- **QMI8658** 6-axis IMU
- Controllable **PWRKEY** button

### WS_S3_5B — ESP32-S3-Touch-LCD-5B

- **RS485** controlling a WaveShare Modbus-RTU-Relay-B board (planned sprinkler-controller
  project, out of this environment's scope — see `FUTURE_IMPROVEMENTS.md`).
- **Digital IO (relay/DIO pins)** — planned use: wired garage door opener buttons.

### CYD_P4_1060 — Guition P4 7" (JC1060P470C)

Has a physical **Ethernet port** in addition to WiFi — worth testing as an alternate
connectivity path.

### CYD_S3_8048 — Guition 5" (JC8048W550)

- **Brightness slider is broken** (`.BL_FREQ = 0`, on/off backlight only — no PWM circuit).
  Needs a decision: hide the slider entirely (matching the `HAS_ES7210`/`HAS_ES8311`-style
  capability gating already used in `Panel_Audio.cpp`), or confirm the hardware genuinely has
  no PWM dimming before ruling out a real fix.
- **Battery ADC pin needs double-checking** — schematic shows the same voltage-divider circuit
  as the 3248W535 tapped on GPIO17, which is also this board's `I2S_DOUT` pin. Unconfirmed;
  needs a multimeter continuity check. Likely an outdated/misrepresented Guition schematic
  (they have a pattern of this), but not yet verified either way.
- **Investigate external audio hardware** — audio is notably quieter than other boards,
  unexplained; see `FUTURE_IMPROVEMENTS.md`'s Audio section for the gain-tuning angle.

### CYD_S3_3248 — Guition 3.5" (JC3248W535)

- **Investigate external audio hardware** — no specific issue reported, just not yet reviewed
  against Waveshare's recent sample/documentation updates (see `FUTURE_IMPROVEMENTS.md`).
- **Touch panel edge margins** (informational, not a bug) — in rotation 0 (native portrait),
  the physical digitizer doesn't register touches across its full nominal 320x480 extent.
  Observed usable range: X ~12–310, Y ~14–461. Worth keeping in mind for UI margin/hit-target
  sizing near screen edges, and worth checking whether other boards with the same/similar
  touch controller have a comparable margin.
- **Sluggish panel animations** — reported as noticeably slow/chunky (panel slide-open/close).
  Not root-caused. Two plausible, not-yet-isolated contributors: QSPI bus bandwidth ceiling
  (the only tested QSPI panel in the fleet, inherently lower-bandwidth than RGB/DSI boards),
  or the CPU cost of `Arduino_Canvas`'s per-pixel software rotation transform, paid on every
  draw call (see `CLAUDE.md`'s Display/touch pipeline section for why that exists).

## Done

Kept brief — enough to know what changed and why, not a full narrative. See git history and
`docs/BRINGUP_WS_S3_TOUCH_LCD_5B.md` for deeper investigation logs where they exist.

- **CYD_S3_3248W535 touch dead in rotation 1** — `TouchManager::mapCoordinates()`'s sanity-clip
  step used raw unrotated BSP dimensions instead of rotation-aware bounds. Fixed; confirmed
  working both rotations on hardware.
- **CYD_S3_8048W550 touch dead** — `.TP_INT` was `18` (wrong pin, pulled from the resistive-
  touch sub-circuit's IRQ, not GT911's). Fixed to `-1` (pure I2C polling, matching WS_P4_7B/
  WS_P4_4B). Also corrected `.I2S_BCLK` from `19` (conflicted with `.TP_SDA`) to `0`
  (GPIO0, matching the board's pinout spreadsheet).
- **CYD_S3_8048W550 brightness reporting** — `_currentBrightness` now correctly reports 100%
  when there's no PWM circuit, instead of an uninitialized-looking 0%. (Slider itself is
  still non-functional — see per-device notes above.)
- **WS_S3_5B visible tearing** — root-caused to `Arduino_ESP32RGBPanel` never actually
  double-buffering despite allocating two framebuffers. Real fix tracked in
  `FUTURE_IMPROVEMENTS.md`; see `docs/BRINGUP_WS_S3_TOUCH_LCD_5B.md` for the investigation.
- **Fleet-wide board/BSP rename** — all BSP filenames, device macros, and struct names now
  match official vendor model designations; old internal nicknames (`WS_S3_SMART86`,
  `WS_P4_SMART86`, "Smart86") no longer appear anywhere in code.
