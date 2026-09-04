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

| Board | Env | Display | Touch | Audio out | Audio in (mic) | WiFi (STA) |
|---|---|---|---|---|---|---|
| **ESP32-P4-WIFI6-Touch-LCD-7B** (WaveShare, macro `WS_P4_7B`) | `WS_P4_TOUCH_LCD_7B` | ✅ | ✅ (portrait; rotation untested) | untested (no speaker access — enclosure doesn't expose it) | untested | ✅ connects (2026-09-03) |
| **ESP32-P4-WIFI6-Touch-LCD-4B** (WaveShare, macro `WS_P4_4B`) | `WS_P4_TOUCH_LCD_4B` | ✅ | ✅ | ✅ | ✅ | ✅ connects, real DHCP IP (see below) |
| **ESP32-P4-WIFI6-Touch-LCD-5** (WaveShare, macro `WS_P4_5`) | `WS_P4_TOUCH_LCD_5` | untested (not flashed yet) | untested | untested | untested | untested |
| **ESP32-S3-Touch-LCD-4B** (WaveShare, macro `WS_S3_4B`) | `WS_S3_TOUCH_LCD_4B` | ✅ | ✅ | ✅ | ✅ | ✅ connects (native radio, no hosted-WiFi complexity) |
| Guition P4 7" (JC1060P470C, macro `CYD_P4_1060`) | `CYD_P4_1060P470` | ✅ | ✅ | ✅ | ✅ (first-ever test of the ES8311-as-sole-mic-input path) | untested (builds clean, not flash-tested for WiFi) |
| Guition 3.5" (JC3248W535, macro `CYD_S3_3248`) | `CYD_S3_3248W535` | ✅ (both rotations confirmed) | ✅ (both rotations confirmed) | ✅ | ✅ | ✅ connects, boot resets no longer reproduce (see below) |
| Guition 5" (JC8048W550, macro `CYD_S3_8048`) | `CYD_S3_8048W550` | ✅ (brightness slider broken, see below) | ✅ | ✅ (notably quieter than other boards, unexplained) | ✅ | untested |
| WaveShare ESP32-S3-Touch-LCD-5B (macro `WS_S3_5B`) | `WS_S3_TOUCH_LCD_5B` | ✅ (visible tearing, see `FUTURE_IMPROVEMENTS.md`) | ✅ (5 simultaneous points confirmed) | N/A (no audio hardware) | N/A | untested |

## Fleet-wide investigations

- **WiFi (station mode)** — in progress; 4 of 8 boards flash-tested so far (3 confirmed
  connecting — `WS_P4_4B`, `WS_S3_4B`, `WS_P4_7B` — and 1 connecting with an open issue). AP/captive-portal fallback
  and MQTT not started. See `FUTURE_IMPROVEMENTS.md`'s Connectivity section for the full
  phased plan and the fleet-wide platform/framework upgrade this required.
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
- **WiFi (STA) confirmed working (2026-09-03).** Connects without serious issue. Shows the same
  benign `hostedHasUpdate()` / `Req_GetCoprocessorFwVersion` RPC warning as `WS_P4_4B` — expected,
  since both share the P4 + ESP32-C6 hosted-WiFi architecture, and confirmed cosmetic (see that
  board's notes below for the full trace). This also settles the open question of whether each
  unit's C6 co-processor firmware might differ: two separate physical units now behave identically.
- **Now the primary development target** — see `docs/ROADMAP.md` Q11. Every milestone builds for
  this board and `CYD_S3_3248W535` (the fleet's weakest board) as a matched pair.
- Nothing else board-specific outstanding — remaining items are fleet-wide, see above.

### WS_P4_5 — ESP32-P4-WIFI6-Touch-LCD-5

No testing done yet at all — BSP built and compiles, not flashed. See
`FUTURE_IMPROVEMENTS.md`'s New Hardware section.

### WS_P4_4B — ESP32-P4-WIFI6-Touch-LCD-4B

Does **not** have battery headers (the only P4 board without one, as far as known). Has 2x
14-pin expansion headers (7x2, 2.0mm pitch), intended for an optional relay/ethernet
component not currently owned. Believed to expose: 5V bus, USB data (`USB_IN1_P`/`USB_IN1_N`),
and SDA/SCL.

- **Battery power: still unconfirmed.** `VCC_5V` header pin measured 5V with the board
  powered over USB - doesn't itself prove it'd accept 5V *input* there (could be USB power
  passed straight through, one-directional). Needs an actual battery/external-5V-source test,
  not just a meter reading under USB power.
- **This board ships in two hardware variants** - this project's unit is the plain
  ESP32-P4-WIFI6-Touch-LCD-4B (has a CSI camera port, unpopulated on this unit). A second
  variant, ESP32-P4-86-Panel-ETH-2RO, adds an add-on board plugging into the second 7x2
  header bank (physical relays, RS485, Ethernet) that this unit's board does *not* come
  with. This unit still exposes that same second header bank in hardware, unpopulated.
- **GPIO47/GPIO48 on that second header bank are labeled TXD/RXD and map to UART1** - the
  same UART Waveshare's ESP-IDF RS485 example uses. But per that example's own docs: *"the
  standalone 4B board does not expose the RS485 A/B pair, and its main PCB must not be
  assumed interchangeable"* with the ETH-2RO variant's add-on board. So UART1 is there, but
  it's raw TTL-UART on this unit, not RS485 - would need an external RS485 transceiver
  breakout to actually use it for Modbus, unlike WS_S3_5B's onboard-relay-board setup.
- **WiFi (STA) confirmed working** — this board's WiFi runs through an onboard ESP32-C6
  co-processor over SDIO (`esp_hosted`), since the P4 itself has no WiFi radio at all.
  Connects cleanly and gets a real DHCP IP. Serial shows a benign
  `rpc_core: Response not received for [0x15e] (Req_GetCoprocessorFwVersion)` /
  `hostedHasUpdate(): Could not get slave firmware version` warning on every boot — traced to
  the exact source in `esp32-hal-hosted.c` (`hostedInit()` calls `hostedHasUpdate()` purely
  for diagnostic version logging, discards its return value, and returns success regardless).
  Confirmed structurally incapable of blocking anything; safe to ignore. Required bumping the
  fleet's platform to pioarduino 55.03.311 / arduino-esp32 3.3.11 to get this far at all — see
  `FUTURE_IMPROVEMENTS.md`'s Connectivity section.

### WS_S3_4B — ESP32-S3-Touch-LCD-4B

On-board hardware not yet touched by firmware at all — develop libraries for each, reference
Waveshare's own repo:
- **AXP2101** power management chip
- **PCF85063** RTC clock chip
- **QMI8658** 6-axis IMU
- Controllable **PWRKEY** button

- **WiFi (STA) confirmed working** — native S3 radio, no co-processor/hosted-transport layer
  involved, so the simplest connect path in the fleet.
- **RGB display timing regression, found and fixed during WiFi testing.** After the fleet's
  platform bump to arduino-esp32 3.3.11, this board showed a ~25px vertical frame shift plus
  tearing/flicker. Root cause unconfirmed (bad pre-existing BSP timing values vs. a
  framework-side RGB driver behavior change - both plausible, see `FUTURE_IMPROVEMENTS.md`),
  but fixed by matching `BSP_WS_S3_TOUCH_LCD_4B.h`'s HSYNC/VSYNC pulse-width and porch values
  to Waveshare's own Arduino demo exactly. Confirmed stable across repeated power-cycle/reflash
  testing. Note: these exact values were apparently tried once before (pre-upgrade) and
  reverted for an unrecorded reason - worth revisiting if a similar-looking problem resurfaces.

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
- **WiFi (STA) boot resets: no longer reproducing as of 2026-09-04 — but not root-caused.**
  Previously this board reset several times before connecting, each reset re-entering `setup()`
  from scratch. After the `v0.1.0` Phase 0 work it boots fast and connects cleanly, confirmed on
  hardware.

  **Treat this as "symptom gone", not "fixed".** Nothing about the power supply, cable or USB port
  changed, which argues *against* the leading brownout hypothesis rather than confirming it. The
  changes that landed in between were: LVGL v9.4.0+134 (untagged) → v9.5.0, the platform pinned to
  the already-installed 55.03.311 (no actual version change), and the board JSON moving from
  PlatformIO's install directory to `boards/`.

  That last one is worth a look if this ever returns: the vendored JSON came from
  `~/.platformio/boards/boards 3.3.0/`, and it was never verified byte-identical to the copy that
  had been sitting in the platform directory. A difference in `f_flash`, `flash_mode` or
  `psram_type` between them could plausibly change boot-time power behaviour. Unverified, and the
  environment overrides several of those values in `platformio.ini` anyway.

  If it comes back: capture serial from cold power-on including the ROM bootloader, and look for
  `Brownout detector was triggered!`.

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
- **WS_S3_4B RGB timing regression** (see per-device notes above) — fixed by matching
  Waveshare's own Arduino demo's HSYNC/VSYNC timing values exactly.
- **Brightness slider toast showing the wrong range** — `WS_S3_4B`/`WS_P4_4B`'s artificial
  slider floor (45–100, to avoid the backlight cutting out at low values) was leaking into
  the toast popup (e.g. showing "45%" as the dimmest setting). `Panel_Display` now remaps the
  displayed percentage to a user-facing 0–100 via `lv_map()`; the slider and actual backlight
  hardware behavior are unchanged.
- **CYD_P4_1060P470 missing partition table override** — every other P4 environment set
  `board_build.partitions = default_16MB.csv`; this one silently fell back to a much smaller
  default and started overflowing once the arduino-esp32 3.3.11 upgrade grew the framework
  slightly. Fixed in `platformio.ini`.
