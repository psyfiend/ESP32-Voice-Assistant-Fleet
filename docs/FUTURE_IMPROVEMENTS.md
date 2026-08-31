# Future Improvements

Ideas, enhancements, and fixes that affect the **entire build environment / HAL** — not tied
to one device or a small group of devices (that's `PROJECT_STATUS.md`'s job). Deliberately
deferred work, not bugs, not blocking anything currently. Lower volatility than
`PROJECT_STATUS.md`; safe to leave stale for a while, but prune entries once actually done.

## LVGL / Display

- **Per-board minimum-brightness floor as a real BSP field.** Currently hardcoded directly
  in `Panel_Display.cpp` (43 for the two 4B boards, 3 for everyone else, based on one
  hardware measurement on the 7B) rather than being a per-device, per-hardware-measured value.
- **Per-device LVGL buffering optimization, backed by actual testing.** The
  `DOUBLE_BUFFERING`/`BUFFER_SIZE_PX` BSP fields exist on every board but are currently
  unused/dead in `GuiManager.cpp` — every board gets the same buffering strategy regardless
  of what these fields say. Measure which boards actually benefit and build real per-device
  logic around it.
- **True LVGL+PPA hardware-accelerated rotation** for MIPI/DSI boards (ESP32-P4 has a PPA —
  Pixel Processing Accelerator — capable of this in hardware). Current rotation on those
  boards is pure CPU-based per-pixel transform. Not causing a known problem today — a
  "if it ever becomes a problem" item, not proactive.
- **`Arduino_ESP32RGBPanel` `num_fbs` investigation.** Requests two hardware framebuffers but
  only ever draws into/reads back one (`getFrameBuffer()` always fetches index 1) — no real
  double-buffering on any board using this class, causing visible tearing on
  `WS_S3_TOUCH_LCD_5B` (worst case, 1024x600). Real fix needs genuine buffer-swap support
  added to the class, matching Waveshare's own `switchFrameBufferTo()` — real `Arduino_GFX`
  internals work, not a config tweak. See `docs/BRINGUP_WS_S3_TOUCH_LCD_5B.md` for the
  investigation that found this.

## Audio

- **AEC stays deprioritized indefinitely**, not just paused. If from-scratch
  voice-assistant work (wake word, on-device AEC) resumes, the plan is a full ESP-IDF
  rewrite (separate project) or adopting/extending ESPHome, not continuing to build AEC out
  here. `ENABLE_AEC` stays off by default.
- **ES8311/ES7210 mic gain and output volume tuning**, based on Waveshare's recent official
  sample repos/documentation rather than chip defaults. Directly relevant: `CYD_S3_8048W550`
  audio is notably quieter than other boards, unexplained — could be a gain issue.
- **Generic capability pattern for "extraneous" sensors/peripherals** (IMU, RTC, power
  management chips, temp/humidity, etc.) — extend the existing `HAS_X` BSP flag pattern to
  cover these too, each paired with a small manager class (mirroring `AudioManager`/
  `TouchManager`'s no-op-unless-flagged shape) and a GUI panel gated the same way
  `Panel_Audio.cpp` already gates its widgets. Directly relevant once the
  AXP2101/PCF85063/QMI8658/SHTC3-bearing boards get real firmware attention.
- **Third, simpler `AudioManager` audio path** for bare I2S mic/amp modules with no
  register-based codec chip at all (ICS43434/INMP441 mics, MAX98357A amps) — the current
  driver only knows how to talk to ES7210/ES8311-style codecs over I2C/registers. Open
  question: is this still needed given the current board lineup, or only relevant for a
  future "basic boards" project? Investigate/test external hardware of this kind and how it
  would actually plug into `AudioManager`'s existing shape before committing to a design.

## Connectivity (WiFi / MQTT / Home Assistant)

"WiFi tested" requires **both** of the following, not just joining a network:
- **Station mode**: on-device method to scan for and connect to an AP, with no hardcoded
  credentials.
- **AP / captive-portal fallback mode**: if the device can't connect to a known AP after
  boot, it stands up its own AP with a captive portal to collect WiFi credentials.
- Once connected: a modular, portable **MQTT component/library**, usable across every board,
  to register the device and whatever peripherals it has with Home Assistant, including MQTT
  discovery (auto-registering entities without manual HA config).

This is the actual definition of "WiFi tested" for this project — device discovery and
entity surfacing the way an ESPHome device would, not just "joins an AP."

## bb_captouch_fork

- **Detach from the original author's repo.** `components/bb_captouch_fork` is currently a
  nested, separate git repo (own remote, `psyfiend/bb_captouch`) rather than a normal part of
  this project — investigate what it'd take to fully absorb it as plain project files with no
  outside remote tie. Treat cautiously either way: an earlier, extensive set of local
  modifications here was reverted to stock and is unrecoverable via git.
- **Investigate the library's own built-in per-controller orientation system**
  (`setOrientation()` + `_iOrientation` + `fixSamples()`) — never used in this project;
  all rotation handling has always been the custom `TouchManager::mapCoordinates()` instead.
  Worth a real comparison against the vendor path.

## Battery / power management

Keep as-is for now — see `PROJECT_STATUS.md` for the one low-priority fleet-wide item
(battery ADC "gauge" investigation).

## General

- **Sync BSPs with Waveshare's latest repos.** WS has very recently updated repos for all
  their devices, particularly the P4 boards, which until now were ESP-IDF-only with no
  Arduino code. Do an exhaustive pass per board: compare audio, display, and LVGL bring-up
  against this project's own; import/examine vendor demos for any hardware unique to that
  board (IMU, RTC, power management, etc.) that isn't in this HAL yet.

## Lithium battery power

3D-printed enclosures exist for most devices with room for a battery. Goal: a single USB-C
port on the enclosure that powers the device, charges the battery, and provides a data
connection to a PC simultaneously. Some boards have native battery headers (see
`PROJECT_STATUS.md` per-device notes for which). Fallback for boards without one: TP4056
modules on hand (USB-C input, plus separate +/- pads for input, battery, and device output).

## P4 silicon revision — what it is and why it matters

ESP32-P4 chips ship in two silicon revision families, `rev1_3` and `rev3_x`, each requiring a
different Arduino `Chip Variant` build setting (`prev3` / `postv3` respectively) and — more
importantly for this project — a different MIPI-DSI PHY clock source (`PLL_F20M` for
`rev1_3`, XTAL default for `rev3_x`). This is a **silicon** revision, not a board/PCB
revision — two physical units of the same board model can ship with different chip
revisions depending on manufacture date, and it's confirmed only via a chip-ID probe, never
from the PCB silkscreen alone.

This project's patched `GFX_Library_for_Arduino` currently hardcodes `PLL_F20M` (`rev1_3`
behavior) — it does not read or branch on anything at runtime. Every P4 board's BSP has a
`bsp_hw.SI_REV` field (`"rev1_3"` / `"rev3_x"` / `"unconfirmed"`) for documentation purposes,
but it's inert — nothing consumes it yet. Wiring it up to actually select the PHY clock
source at build time would mean touching the vendored GFX library, not just the BSP.

## New hardware

### WaveShare ESP32-P4-WIFI6-Touch-LCD-5 (`WS_P4_5`) — BSP built, not yet flashed

BSP modeled directly against the latest Waveshare repo for this exact device, which (unlike
past P4 boards) included real Arduino-specific samples/code, not just ESP-IDF. Compiles
clean across all 8 environments; has not been flashed or tested on physical hardware yet.

### WaveShare ESP32-S3 RGB Matrix board + 128x64 HUB75 panel

- Has ES8311, ES7210, and dual mics — same audio codec pairing as several boards already in
  this fleet, so `AudioManager` should mostly just work here once BSP'd.
- Also has a QMI8658 (IMU) and an SHTC3 temp/humidity sensor.
- Plan is to drive the HUB75 panel using the `mrcodetastic/ESP32-HUB75-MatrixPanel-DMA`
  library (external dependency, not part of this project's existing display stack).

### WaveShare Modbus-RTU-Relay-B (8-channel RS485 relay module)

No microcontroller on this board — a dumb RS485-controlled relay bank, intended to be driven
by `WS_S3_TOUCH_LCD_5B`. Its intended use (a sprinkler controller) is a **special project
outside the scope of this environment** — see `PROJECT_STATUS.md`'s `WS_S3_5B` entry for the
immediate to-do (basic Modbus communication + relay switching only).

## Done

Kept brief — enough to know what changed and why, not a full narrative. See git history for
the rest.

- **Unified `Fleet_BSP.h`.** Was two separately-drifting struct families
  (`Fleet_BSP.h`/`Fleet_BSP_P4.h`), then one nested struct, now seven independent flat
  structs (`BoardHardware`, `ExpanderConfig`, `DisplayConfig`, `TouchConfig`, `LvglConfig`,
  `AudioConfig`, `StorageConfig`) aliased to `bsp_hw`/`bsp_display`/etc. See `CLAUDE.md`'s BSP
  pattern section and `PROJECT_STATUS.md` for the summary.
- **Board-identity macro moved out of `build_flags`.** Each `BSP_<NAME>.h` now defines its
  own short device macro at the top of the file instead of a redundant separate build flag.
  `HAS_X` capability flags are unaffected, still in `build_flags`.
- **`bb_captouch_fork` GT911 "Invalid IO 255" noise fixed** — guarded `iINT` GPIO calls with
  `if (iINT != -1)`, matching the pattern already used for every other controller type in
  that file.
