# Future Improvements

Ideas, enhancements, and fixes that affect the **entire build environment / HAL** — not tied
to one device or a small group of devices (that's `PROJECT_STATUS.md`'s job). Deliberately
deferred work, not bugs, not blocking anything currently. Lower volatility than
`PROJECT_STATUS.md`; safe to leave stale for a while, but prune entries once actually done.

## Startup / GUI reorganization

Raised during connectivity design work, deliberately out of scope for that branch — a
naming/responsibility mismatch noticed in passing, not a bug. Currently: `LVGL_Test_UI.cpp`'s
`setup()` does hardware bring-up (`gui.begin()`, `audioMgr.begin()`) *and* builds the LVGL
dashboard (root screen, header, deck panels) in the same function; `GuiManager.cpp` is
entirely LVGL engine plumbing (buffer-alloc matrix, driver registration, tick/log callbacks)
despite its name, and `DisplayManager::begin()` prints generic device-info lines (`device_name`,
PSRAM, flash size, `bsp_touch.NAME`) that aren't display-specific at all.

Proposed three-way split:
- **`main`** — hardware bring-up only (`displayMgr.begin()`, `touchMgr.begin()`,
  `audioMgr.begin()`, `connMgr.begin()`), plus `debug_dump_config()` and the GPIO-register
  checker. No LVGL code. Also absorbs the generic device-info Serial prints currently
  misplaced in `DisplayManager::begin()`.
- **`LVGL_Startup`** (new file) — the LVGL engine plumbing currently in `GuiManager.cpp`:
  buffer-alloc matrix, `lv_init()`/tick/log setup, display+indev driver registration, and the
  `flush_cb`/`touch_read` callbacks (these bridge LVGL to `DisplayManager`/`TouchManager` and
  are needed by any screen content, so they belong with engine plumbing, not screen content).
  Invoked conditionally from **`main`** (build-flag or parallel no-GUI env gated), *not* from
  `DisplayManager` itself — `DisplayManager` is a reusable HAL component and should stay
  LVGL-agnostic, same as it is today; having it call into UI-layer code would invert that
  dependency. A no-LVGL build variant can reuse the `build_src_filter` exclusion mechanism
  `WS_S3_TOUCH_LCD_5B` already uses to drop `Panel_Audio.cpp`, extended to
  `GuiManager.cpp`/`LVGL_Startup.cpp`/all `Panel_*.cpp`.
- **`GuiManager`** — becomes actual screen content: root screen, header, deck panels. Name
  finally matches what's in the file.

Worth doing before connectivity/MQTT GUI panels and future peripheral panels add more
`Panel_*.cpp` files through the current setup(), but not blocking anything today.

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

### Per-device DPI and font scaling

`HIGH_DPI_DISPLAY` is currently a single on/off build flag that does two things: sets LVGL's
DPI to 150 (`GuiManager.cpp`) and swaps in a larger font set (`UIToolkit.cpp` — caption 10->16,
label 12->20, button/header ->22, hero ->34). The high-DPI font block is commented "P4 Smart86
(High Res)", i.e. it was sized for one specific panel.

That is too coarse for the fleet. The boards differ in both pixel count *and* physical size, and
those are independent: a 720x1280 4-inch panel and a 1024x600 7-inch panel want different
scaling even though a single flag treats them as one case. Confirmed good on `WS_P4_5`
(720x1280, 2026-09-06) and on the 4B, but that is two data points on a boolean.

Better shape: derive scaling from BSP values rather than a flag — physical diagonal (or DPI)
alongside the existing `WIDTH`/`HEIGHT`, and pick fonts and LVGL DPI from that. Would also let
the diagnostics dump report a real computed scale instead of `STANDARD (1.0x Scaling)` versus
an implicit "high". Low priority while the fleet is small; worth doing before the card library
expands (GitHub issue #31).

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

**Status: in progress on the `wifi-testing` branch, WiFi first, MQTT deferred until every
board joins an AP.**

**Progress so far:**
- `Fleet_Connectivity`/`ConnectivityManager` Phase 1 (STA connect with NVS-fallback-to-compile-
  default, per the layering below) is implemented and confirmed connecting on real hardware:
  `WS_P4_TOUCH_LCD_4B` (P4/ESP32-C6 hosted-WiFi, real DHCP IP) and `WS_S3_TOUCH_LCD_4B` (native
  radio). `CYD_S3_3248W535` also connects but has an open, unrelated issue (see
  `docs/PROJECT_STATUS.md`'s per-device notes) - full per-board WiFi status lives there, not
  here.
- Getting the P4/C6 boards to connect at all **required a fleet-wide platform/framework
  upgrade**: pioarduino `platform-espressif32` 55.03.34 → 55.03.311 (arduino-esp32 3.3.4 →
  3.3.11), plus a PlatformIO Core bump (6.1.18 → 6.1.19, pioarduino 55.03.311's minimum). The
  P4 has no WiFi radio at all - it depends entirely on an onboard ESP32-C6 co-processor over
  SDIO (`esp_hosted`), and the older host-driver version couldn't complete its RPC handshake
  with the C6's factory firmware. Waveshare's own compatibility docs named 3.3.11 as their
  tested Arduino-core version for this hardware.
  - All 8 environments rebuilt clean against the new platform. Two real regressions surfaced
    and got fixed along the way: `GFX_Library_for_Arduino`'s `Arduino_ESP32SPI`/
    `Arduino_ESP32SPIDMA` databus classes called `spiFrequencyToClockDiv()` with 3.3.4's old
    single-argument signature, broken against 3.3.11's new `(spi_t*, uint32_t)` signature -
    both confirmed unused anywhere in this fleet and excluded via the library's `library.json`
    `srcFilter` (files kept on disk, just not compiled) rather than fixed or deleted;
    `CYD_P4_1060P470` was missing a `board_build.partitions` override every other P4 env has,
    silently running on a too-small default partition table that only started overflowing
    once 3.3.11's larger framework didn't fit.
  - **Operational gotcha worth knowing**: pioarduino 55.03.311's tooling actively rejects
    being invoked from a Git Bash/MSYS shell (fails with `MSys/Mingw is not supported`, plus
    knock-on failures like the toolchain compiler not being found on `PATH`). Run `pio`
    commands from PowerShell or cmd.exe, not Git Bash, for this platform version. VS Code's
    PlatformIO IDE extension is unaffected either way - it doesn't invoke Core through MSYS.
  - The `hostedHasUpdate()` RPC warning that still prints on every P4/C6 boot
    (`Req_GetCoprocessorFwVersion` timing out) is confirmed cosmetic - traced to source, it's a
    diagnostic-only version check whose result is discarded, structurally incapable of
    blocking the actual connection. Safe to ignore; see `docs/PROJECT_STATUS.md`'s `WS_P4_4B`
    notes for the full trace.
  - Not yet pinned: `platformio.ini` still says `platform = espressif32` with no version/commit
    pin, so this upgrade currently only "sticks" because it's installed globally on this one
    dev machine. A fresh machine or a routine `pio pkg update` wouldn't reproduce it. Worth
    pinning the exact fork commit once the platform choice feels settled.

**Still open:** AP/captive-portal fallback (Phase 2) not started; GUI integration (Phase 3)
not started; 5 of 8 boards not yet flash-tested for WiFi at all (`WS_P4_TOUCH_LCD_7B`
physically inaccessible mid-enclosure-build, `WS_P4_TOUCH_LCD_5`/`CYD_P4_1060P470`/
`CYD_S3_8048W550`/`WS_S3_TOUCH_LCD_5B` simply not attempted yet).

Architecture agreed so far:

- **Not part of `Fleet_BSP.h`.** BSP structs are hardware-wiring facts; WiFi/MQTT settings
  are deployment config, a different axis entirely. New component,
  `components/Fleet_Connectivity/`, holds a `ConnectivityDefaults.h` with flat structs in the
  BSP's style (`WiFiDefaults`, `MqttDefaults` kept separate — WiFi has to succeed before MQTT
  is meaningful, and they'll get separate GUI panels).
- **One fleet-wide default, not per-device files.** All devices are expected to join the same
  home AP, so WiFi needs little to no per-device variation. Where a device genuinely needs to
  differ, override individual fields with `#ifdef WS_P4_7B` / etc. blocks in the same header —
  reusing the unique per-board identity macro every `BSP_<NAME>.h` already defines, no new
  build_flag or file-selection mechanism needed. Promote a field to a real per-board file only
  if it turns out to need heavy variation later; don't build that machinery speculatively.
- **Compile-time struct is only the fresh-flash fallback.** Source of truth at runtime is NVS
  (`Preferences` library) via a `ConnectivityManager`: read NVS first, fall back to the
  compile-time default if unconfigured. Every GUI-driven change (scan-and-connect, portal
  settings) writes straight to NVS. Unencrypted for now — see encryption note below.
- **MQTT device-identity fields** (device name, device ID, broker address, base topic) follow
  the same default-plus-NVS-override layering as WiFi. **Per-peripheral MQTT entities do
  not** — a peripheral (audio, and eventually IMU/RTC/battery/relay per this file's own
  "generic capability pattern" item above) should be able to advertise its own HA discovery
  entity regardless of which board it's wired to. Planned shape once the MQTT phase starts:
  each `HAS_X`-gated peripheral registers a small `MqttEntityDescriptor` (component type,
  object_id, name, device_class, state topic suffix) into a central registry; one
  `MqttManager` walks it at connect time and publishes discovery payloads with the device ID
  injected centrally (`<device_id>_<object_id>` as `unique_id`), so descriptors stay portable
  across boards and never need to know their own device ID. This is the same underlying
  problem as `docs/GUI_FRAMEWORK.md`'s manifest/data-source-abstraction vision (layer 3) —
  worth building with an eye toward that reuse, not as a throwaway.
- **NVS encryption: deferred, not designed out.** The standard scheme ties NVS encryption to
  full flash encryption (irreversible eFuse burn in release mode, not something to flip while
  still actively reflashing boards for bring-up); chips with an HMAC peripheral (S3 confirmed,
  P4 unconfirmed) support a narrower key-derivation scheme without encrypting all of flash.
  Either way it's a one-time per-physical-device provisioning step (eFuse + a `nvs_keys`
  partition-table entry), not a code change — `Preferences` call sites are identical with or
  without it, so starting unencrypted doesn't force a later rewrite. Needs its own
  investigation into whether PlatformIO's Arduino-framework build exposes the necessary
  sdkconfig options before committing to a scheme.
- **Custom lean AP/captive-portal, not `tzapu/WiFiManager`.** That library owns the whole
  scan/connect/portal flow with its own web UI, which would fight the LVGL touchscreen being
  the primary settings surface instead of complementing it. Native `WiFi.h` (STA scan/connect,
  `softAP()`) + `DNSServer` for the captive-portal DNS redirect; a web page only as a minimal
  phone/laptop fallback for credential entry, not the main UX.

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

**Updated 2026-09-06 (`feat/p4-5-display-bringup`).** Three things are now known that were not
when the above was written:

- **`pioarduino` already handles the build-setting half automatically.** It ships two prebuilt
  lib variants — `esp32p4_es` (pre-rev3: `SELECTS_REV_LESS_V3=y`, `REV_MIN_1`) and `esp32p4`
  (`REV_MIN_301`) — and selects between them from the board definition. `board =
  esp32-p4-evboard` picks **`esp32p4_es`**, so the fleet is already built for pre-rev3 silicon
  and the `Chip Variant` concern is handled. Beware: `framework-arduinoespressif32-libs/` holds
  *both* variants' `sdkconfig` files, and reading the wrong one is very easy — it cost most of
  a session.
- **The PHY clock source is now selectable per board.** `DisplayConfig.PHY_CLK_SRC` plus a
  constructor parameter on `Arduino_ESP32DSIPanel` does exactly the wiring-up this section
  anticipated. `0` = keep the library's `PLL_F20M`, so no existing board changed.
- **It did not matter in practice.** `PLL_F20M` and IDF-auto were tested head-to-head on a
  confirmed rev1.3 chip and behaved identically. The display bug that prompted the
  investigation turned out to be panel reset polarity, not the clock source. So treat the
  rev1_3/rev3_x PHY-clock distinction as real but so far unobserved — do not assume it explains
  a DSI failure.

One measured data point exists: the `WS_P4_5` unit is **rev v1.3** (eFuse
`WAFER_VERSION_MAJOR=1`, `MINOR=3`). The other three P4 boards have never been probed — one
`esptool flash-id` each would settle what the fleet actually contains.

`bsp_hw.SI_REV` remains inert and should stay `"unconfirmed"`: silicon revision is a per-chip
property, so a per-board-model header cannot represent it correctly. If it is ever made
load-bearing, read the revision at runtime and compare, the way `checkAudioBspSanity()` does.

Full detail: `docs/BRINGUP_WS_P4_TOUCH_LCD_5.md`.

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
