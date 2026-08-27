# Future Improvements

Deliberately deferred work — things discussed and explicitly marked "later," not bugs and
not blocking anything currently. Lower volatility than `PROJECT_STATUS.md`; safe to leave
stale for a while, but prune entries once actually done.

## BSP / architecture

- **Merge `Fleet_BSP.h` and `Fleet_BSP_P4.h`** into one shared struct definition instead of
  two independently-drifting ones. Explicitly deferred as too large a change to take on
  alongside everything else.
- **Move `HAS_X` capability `#define`s from BSP headers into PlatformIO `build_flags`**,
  mirroring how the board-name macro already works on 5 of 6 boards. Removes the
  include-order fragility of `#ifdef HAS_X` checks (currently only defined once
  `bsp_loader.h` has been included) and cleanly separates "which board is this" (build
  concern) from "what are this board's values" (BSP struct concern).
- **Per-board minimum-brightness floor as a real BSP field.** Currently hardcoded directly
  in `Panel_Display.cpp` (43 for the two Smart86 boards, 3 for everyone else, based on one
  hardware measurement on the 7B) rather than being a per-device, per-hardware-measured
  value.
- **Static-assert the `hw_cfg.MIC_SELECTED`/etc. sanity checks** instead of the current
  runtime check. Would require re-qualifying every BSP struct instance across all 6 device
  headers from `const` to `constexpr` — a real, if mechanical, change, deferred as
  out-of-scope for a quick sanity check.

## LVGL / display

- **Per-device LVGL double-buffering decision, backed by actual testing.** The
  `DOUBLE_BUFFERING`/`BUFFER_SIZE_PX` struct fields exist on every board's BSP but are
  currently unused/dead in `GuiManager.cpp` — copied in from examples early on, never wired
  up. All boards currently get the same buffering strategy regardless of what these fields
  say. Intent is to eventually measure which boards actually benefit from double buffering
  and build real per-device logic around it.
- **True LVGL+PPA hardware-accelerated rotation** for MIPI/DSI boards (ESP32-P4 has a PPA —
  Pixel Processing Accelerator — capable of this in hardware). Current rotation on those
  boards is pure CPU-based per-pixel transform, same technique as the QSPI `Canvas` wrapper.
  Not causing a known problem today (no reported tearing/stutter on DSI boards), so this is
  purely a "if it ever becomes a problem" item, not a proactive priority.
- **`Arduino_ESP32RGBPanel` requests two hardware framebuffers (`num_fbs = 2`) but only ever
  uses one** (`getFrameBuffer()` always fetches index 1, index 0 is allocated and never
  touched) — confirmed via source while bringing up `WS_S3_TOUCH_LCD_5B`. No board using this
  class gets real hardware double-buffering/tear-avoidance today; every draw writes into the
  same buffer that's simultaneously being scanned out. Wastes ~1.2MB of PSRAM per board on
  the unused buffer, and is now causing *visible* tearing on `WS_S3_TOUCH_LCD_5B` (1024x600 —
  ~60% more pixels per frame than `8048W550` through the same single-buffer bottleneck, so the
  same underlying limitation is more noticeable there). Real fix is giving the class genuine
  buffer-swap capability, matching what Waveshare's own ESP-IDF reference does via
  `switchFrameBufferTo()` — real `Arduino_GFX`-internals work, not a config tweak. See
  `docs/BRINGUP_WS_S3_TOUCH_LCD_5B.md` for the full investigation that found this.

## Audio

- **AEC is back-burnered indefinitely for this Arduino codebase**, not just paused. If
  from-scratch home-assistant-style voice assistant work (wake word, on-device AEC, etc.)
  resumes, the plan is either a full rewrite on ESP-IDF directly (a separate project from
  this one) or adopting/extending an existing well-developed project (ESPHome or similar)
  rather than continuing to build AEC out in this codebase. `ENABLE_AEC` stays off by
  default and the 4-channel TDM path stays unfinished/untested — treat as effectively
  frozen, not "next up."
- **ES8311 mic gain tuning.** Currently relies on chip register defaults (the
  `es8311_set_mic_gain()` call is commented out, never invoked) rather than an explicit,
  tested value.
- **Check WaveShare's official example sketches for device-specific mic/output gain
  settings** before assuming defaults are fine everywhere. Directly relevant: the 8048W550
  board's audio came through notably quieter than every other tested board, unexplained —
  could easily be a gain issue rather than anything structural. WaveShare's own P4-7B
  examples (see per-board to-do in `PROJECT_STATUS.md`) include their own `es8311`/`es7210`
  driver versions worth diffing for any gain values this project's adapted drivers dropped
  or got wrong.
- **Generic capability pattern for "extraneous" sensors/peripherals** (IMU, RTC, power
  management chips, temp/humidity, etc.) — extend the existing `HAS_X` BSP flag pattern
  (already used for `HAS_ES7210`/`HAS_RGB_PANEL`/etc.) to cover these too, each paired with a
  small manager class (mirroring `AudioManager`/`TouchManager`'s shape: no-op when the flag
  isn't defined, real logic when it is) and a GUI panel gated the same way `Panel_Audio.cpp`
  already gates its widgets. Directly relevant once the AXP2101/PCF85063/QMI8658/SHTC3-bearing
  boards get real firmware attention.
- **`AudioManager` needs a third, simpler audio path** for bare I2S mic/amp modules with no
  register-based codec chip at all (ICS43434/INMP441 mics, MAX98357A amps) — the current
  driver only knows how to talk to ES7210/ES8311-style codecs over I2C/registers. Relevant
  for whatever the planned "basic boards" project ends up being.

## Connectivity (MQTT / Home Assistant)

The actual purpose of this fleet is home-automation dashboards driven by Home Assistant
data (via MQTT or otherwise) — this is a prerequisite for WiFi testing to mean anything, see
`PROJECT_STATUS.md`.

- **Build a modular, portable MQTT component/library**, usable across every board in this
  fleet, that can quickly spin up a new device with whatever sensors are attached at the
  time. Needs to support:
  - Talking to a Home Assistant MQTT broker
  - Home Assistant **MQTT discovery** (auto-registering devices/entities without manual HA
    config)
  - Surfacing arbitrary components/entities per device (sensors, switches, etc. — whatever
    a given board happens to have attached)
- This is the actual definition of "WiFi tested" for this project, not just "joins an AP."

## bb_captouch_fork

- **The library has its own built-in, per-controller-type orientation system**
  (`setOrientation()` + `_iOrientation` + `fixSamples()`) that this project has never used —
  `setOrientation()` is never called anywhere, so `_iOrientation` stays `0` forever and
  `fixSamples()` never runs. All rotation handling in this project has always been the
  custom `TouchManager::mapCoordinates()` instead. Worth comparing at some point — the
  vendor path is presumably tuned per-chip by the library author, versus one hand-written
  generic formula here that already had one real bug found and fixed this session. Not
  urgent; the current approach works now that its bug is fixed.
- The stock library's GT911 reset block in `init()` calls `pinMode`/`digitalWrite` on
  `iINT` unconditionally, even when it's `-1` (unused). This produces harmless but noisy
  "Invalid IO 255" errors in the boot log on boards without a dedicated INT pin. A small,
  well-scoped fix exists (guard with `if (iINT != -1)`, matching the pattern already used
  for every *other* controller type in that same file) but was deliberately not applied
  this session, out of caution around touching that library again after an earlier set of
  extensive local modifications had to be reverted. Worth doing eventually — it's isolated
  and low-risk, unlike the modifications that were reverted.

## Battery / power management

- **IP5306 battery gauge implementation.** Nothing implemented yet on any board. Voltage
  divider math confirmed from schematic for the 3248W535: `Vbat = V(ADC_pin) × 1.33`
  (33K/100K divider, direct 0Ω tap to the ADC pin). The Guition P4 7" has its own vendor
  `adc_test` reference sketch with real charged/dead-battery calibration values, worth
  reusing when that board's gauge gets implemented. Remember: raw voltage → percentage
  needs a real Li-ion discharge curve, not a naive linear map, if accuracy matters.
- **S3 Smart86 power-monitoring/charging UI** — chip is present on that board, no firmware
  work started.

## Build system

- **Decide on a permanent fix for the `build_cache_dir` BSP-header staleness gotcha**
  (see `CLAUDE.md`) — currently only worked around manually (`rm -rf .pio/build_cache`).
  Two real options discussed, not yet chosen: (1) remove `build_cache_dir` from
  `platformio.ini` entirely — simplest, but loses cross-environment object-file reuse, a
  real speedup across this many board environments; untested whether PlatformIO's *default*
  incremental tracking (without this extra cache layer) has the same macro-`#include` blind
  spot or not. (2) Add a small `extra_scripts` pre-build Python hook using SCons's
  `env.Depends()` to explicitly declare the dependency on the active `BSP_HEADER` file,
  keeping `build_cache_dir` and keeping `bsp_loader.h` exactly as generic as it is now
  (deliberately rejected hardcoding a per-board `#elif` chain in `bsp_loader.h` — adds a
  board name to maintain in two places instead of one).

## New hardware to add to the fleet/HAL

Not yet integrated into this codebase at all — no BSP header, nothing. Listed here so
intent isn't lost before the work starts. (`WS_S3_TOUCH_LCD_5B` used to be listed here —
it's integrated and working now, see `PROJECT_STATUS.md` and
`docs/BRINGUP_WS_S3_TOUCH_LCD_5B.md`.)

### WaveShare ESP32-P4-WIFI6-Touch-LCD-5 (arrived, not yet started)

The P4 sibling of `WS_S3_TOUCH_LCD_5B` — same general concept (utilitarian/functional HMI,
not a voice-assistant board), bigger MCU. Waveshare has been actively publishing Arduino
support for their P4 boards recently (previously ESP-IDF-only for several of them) — worth
checking their GitHub for this board specifically, and worth a broader pass (see next item)
before assuming this board's bring-up needs to be as much of an from-scratch investigation as
`WS_S3_TOUCH_LCD_5B`'s was.

### General: sync with Waveshare's latest example repos across the fleet

Waveshare has been shipping real Arduino example code/libraries for boards that were
previously ESP-IDF-only, and updating existing ones. Worth a periodic pass across every board
already in this fleet (not just new ones) checking their current GitHub repos for anything
that's changed since this project's own bring-up — pin corrections, timing values, library
updates, newly-added Arduino support. The `WS_S3_TOUCH_LCD_5B` bring-up (see
`docs/BRINGUP_WS_S3_TOUCH_LCD_5B.md`) leaned on exactly this kind of vendor reference material
(their real ESP-IDF driver source, byte-for-byte) to make real progress after guessing alone
had stalled — worth doing proactively, not just when something's already broken.

### WaveShare ESP32-S3 RGB Matrix board + 128x64 HUB75 panel

- Has ES8311, ES7210, and dual mics — same audio codec pairing as several boards already in
  this fleet, so `AudioManager` should mostly just work here once BSP'd.
- Also has a QMI8658 (IMU) and an SHTC3 temp/humidity sensor.
- Plan is to drive the HUB75 panel using the `mrcodetastic/ESP32-HUB75-MatrixPanel-DMA`
  library (external dependency, not part of this project's existing display stack).

### WaveShare Modbus-RTU-Relay-B (8-channel RS485 relay module)

- No microcontroller on this board — it's a dumb RS485-controlled relay bank, driven by
  another board (intended to be the ESP32-S3-Touch-LCD-5B above).
- **Planned project**: sprinkler controller firmware on the Touch-LCD-5B, managing 8
  sprinkler zones through this relay board. The Home Assistant-side automation/software is
  already finished and has been running in production for months, and was deliberately
  designed to be compatible with **ESPHome's Sprinkler component** — so the firmware side
  should be able to talk to the existing HA automation without redesigning that half.
- For now: just get basic Modbus communication working with the relay board and confirm the
  relays actually switch. Full sprinkler firmware is later work, not immediate.
