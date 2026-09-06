# Project Status

Bugs, fixes, additions, and enhancements that apply to a **specific device or a small group
of devices** — not fleet-wide HAL/environment concerns (that's `FUTURE_IMPROVEMENTS.md`'s
job). Volatile; update or prune entries as they get resolved, don't let this file grow stale.

---

## ⚡ WORK IN FLIGHT — read this first (2026-09-05)

**Branch: `feat/connectivity-state-machine` @ `aa4baab`. Not merged to `main`, deliberately —
`main` is meant to be always-flashable and this has unverified behaviour on it.**

Phase 1 connectivity. The full mode × failure-type behaviour is **signed off** and lives in the
"Connectivity Behavior Spec" artifact; `docs/ROADMAP.md` Q1/Q9 carry the decisions. The
implementation follows that spec.

### Verified on hardware
- STA connect, both `CYD_S3_3248` and `WS_P4_7B`.
- `STA_PLUS_AP` (mode 2) works on **both** chip families — this answers GitHub issue #5, the
  APSTA-over-`esp_hosted` spike, in the affirmative. P4 boards *can* run AP and STA together.
- AP fallback raises, phone connects, `192.168.4.1` reachable.
- Auth-failure classification: reason 15 correctly reported as "wrong password", reason 2
  (`AUTH_EXPIRE`) correctly treated as transient rather than a credentials problem.
- Header WiFi glyph: blue blink while connecting → green arcs when connected → amber + "AP"
  badge in AP mode. Per-board motion default confirmed (pulse on DSI, blink on the QSPI 3248).
- TX power cap of 13 dBm on `CYD_S3_3248` — brownouts stopped. **Confounded**: a USB cable
  swap happened at the same time, so the cap is not independently proven. `WS_P4_7B` runs with
  no cap and is stable, which is the control.

### Built but NOT yet verified
- **Hostname reaching DHCP.** Fixed in `be6e60e`; never confirmed against a real DHCP server.
  This one must be checked in the router's lease table, **not** in serial output — see the
  Known-issues entry below for why the device's own report was untrustworthy.
- **`APPEND_MAC_SUFFIX = false`.** Fixed in `be6e60e`, unverified.
- The signed-off retry policy: two auth rounds with a 45 s gap, permanent stop when unproven,
  45-minute recheck when proven, environmental backoff 2→4→8→16→30 min, AP-client deferral.
  All compiles, none observed.

### Known bug, not yet fixed
**`_proven` is not tied to the credentials that proved themselves.** It is a bare bool in NVS,
so changing `LOCAL_STA_PASSWORD` and reflashing leaves a device thinking a password it has
never tried is proven. Fix is to store a fingerprint of SSID+password alongside the flag and
only honour `proven` on a match. Deliberately left in place so the proven path could be
observed at all — changing credentials is otherwise the only easy way to trigger an auth
failure, and the fix makes that mark them unproven.

### Next tests, in order
1. Hostname in the DHCP lease table with `APPEND_MAC_SUFFIX = false` → expect `fleet-cyd-s3-3248`
   / `fleet-ws-p4-5`, no MAC suffix, and `macSuffix=off` in the `[Conn:debug]` line.
2. Wrong password on a **proven** board → expect two auth rounds then `Next STA retry in 2700 s`.
   Attach a phone to the AP during the wait to see the deferral line.
3. `pio run -t erase` then wrong password → **unproven** path → `These credentials have never
   worked` and a permanent stop, no retry ladder.
4. Junk SSID → environmental → reason 201, ladder that continues indefinitely and never stops.

### Working on a second machine
`platformio.ini`'s 26 `symlink://` paths are absolute and machine-specific (see `CLAUDE.md`).
A clone on another machine must rewrite that prefix — **and must not commit the change**, or
it breaks the original machine. `components/Fleet_Connectivity/ConnectivityLocalSecrets.h` is
gitignored and has to be recreated by hand.

---

### WS_P4_5 display bring-up — RESOLVED 2026-09-06

Branch **`feat/p4-5-display-bringup`**. The board is fully up: display, touch, audio, WiFi STA
and AP all working.

Root cause was **panel reset polarity** — the HX8394 resets active HIGH while the GFX library
hardcoded an active-LOW sequence that ends with the pin asserted, holding the panel in reset
through the whole init. Fixed with a per-board `DisplayConfig.RST_ACTIVE_HIGH` defaulting to
active-low, so no other board changed. Found by diffing Waveshare's bundled copy of the
library against our fork.

Full history, the ruled-out list, corrections to several wrong theories, and remaining
bring-up items: `docs/BRINGUP_WS_P4_TOUCH_LCD_5.md`.

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
| **ESP32-P4-WIFI6-Touch-LCD-5** (WaveShare, macro `WS_P4_5`) | `WS_P4_TOUCH_LCD_5` | ✅ (rotation untested) | ✅ | ✅ | ✅ codec init (capture untested) | ✅ connects, AP + STA_PLUS_AP confirmed |
| **ESP32-S3-Touch-LCD-4B** (WaveShare, macro `WS_S3_4B`) | `WS_S3_TOUCH_LCD_4B` | ✅ | ✅ | ✅ | ✅ | ✅ connects (native radio, no hosted-WiFi complexity) |
| Guition P4 7" (JC1060P470C, macro `CYD_P4_1060`) | `CYD_P4_1060P470` | ✅ | ✅ | ✅ | ✅ (first-ever test of the ES8311-as-sole-mic-input path) | untested (builds clean, not flash-tested for WiFi) |
| Guition 3.5" (JC3248W535, macro `CYD_S3_3248`) | `CYD_S3_3248W535` | ✅ (both rotations confirmed) | ✅ (both rotations confirmed) | ✅ | ✅ | ✅ connects, boot resets no longer reproduce (see below) |
| Guition 5" (JC8048W550, macro `CYD_S3_8048`) | `CYD_S3_8048W550` | ✅ (brightness slider broken, see below) | ✅ | ✅ (notably quieter than other boards, unexplained) | ✅ | untested |
| WaveShare ESP32-S3-Touch-LCD-5B (macro `WS_S3_5B`) | `WS_S3_TOUCH_LCD_5B` | ✅ (visible tearing, see `FUTURE_IMPROVEMENTS.md`) | ✅ (5 simultaneous points confirmed) | N/A (no audio hardware) | N/A | untested |

## Fleet-wide investigations

- **Two diagnostics have now lied to us. Verify connectivity claims from OUTSIDE the device.**
  This is the most transferable lesson of the Phase 1 work so far:
  - `WiFi.getHostname()` reads back the same global buffer `WiFi.setHostname()` writes, so the
    firmware happily reported a hostname the interface had never been given. Only a look at the
    router's DHCP lease table exposed it. (`setHostname` writes a *default* applied at netif
    creation; the interface-level call is `WiFi.STA.setHostname()`.)
  - `esp_wifi_connect()` returns `ESP_ERR_WIFI_CONN` when a connect is already in flight, so a
    "re-issuing connect" log line described a retry that never happened.

  Consequence for the MQTT and HA-discovery work: "the device says it published" and "the
  broker received it" are different claims. Issues #9 and #11 require `mosquitto_sub` and the
  HA UI for exactly this reason.

- **WiFi (station mode)** — in progress; 4 of 8 boards flash-tested so far (3 confirmed
  connecting — `WS_P4_4B`, `WS_S3_4B`, `WS_P4_7B` — and 1 connecting with an open issue). AP/captive-portal fallback
  and MQTT not started. See `FUTURE_IMPROVEMENTS.md`'s Connectivity section for the full
  phased plan and the fleet-wide platform/framework upgrade this required.
- **ESP32-C6 co-processor firmware (P4 boards)** — on `WS_P4_5` the host cannot read the slave
  firmware version (`Req_GetCoprocessorFwVersion` times out, reports `0.0.0` vs host
  `2.12.11`). WiFi works regardless, but it costs roughly a second of boot time in failed RPC
  retries. Not yet checked on `WS_P4_7B` / `WS_P4_4B`; if they behave the same it is a
  fleet-wide P4 item, and Espressif publish a matching slave binary.
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

**Never flashed. Currently the active test target (2026-09-05) alongside `CYD_S3_3248`.**

BSP is written and all 8 environments compile, but no code has ever run on this board — so
display bring-up, touch, rotation and WiFi are all genuinely unknown, not merely unconfirmed.
Treat a first-flash failure as ordinary rather than as a regression in the connectivity work:
its sibling `WS_P4_7B` is DSI + P4/C6 hosted WiFi and works, which makes this board *likely*
to work, but the panel timings and touch controller have never been exercised.

Useful property for connectivity testing: with factory-fresh NVS it is **unproven**, so it
exercises the permanent-stop path without needing `pio run -t erase`, while the 3248 carries a
`proven` flag and exercises the 45-minute recheck path. One board for each branch of the spec.

See `FUTURE_IMPROVEMENTS.md`'s New Hardware section.

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
