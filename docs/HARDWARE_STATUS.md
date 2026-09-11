# Hardware Status

**Scope: the boards themselves.** What works on which board, what is untested, per-device
quirks, and build-environment issues. Deliberately *not* the dashboard project's plan.

Previously `PROJECT_STATUS.md`, which had grown to cover both hardware and project
planning. Those are now separate:

| Question | Where |
|---|---|
| What are we building, in what order, what is done | `ROADMAP.md` |
| What is being worked on right now, what is blocked | GitHub issues |
| Which board does what, what is untested | **this file** |
| How the HAL/BSP works | `CLAUDE.md` |
| Fleet-wide deferred work | `FUTURE_IMPROVEMENTS.md` |
| Mistakes worth not repeating | `LESSONS.md` |

The BSP architecture is documented in full in `CLAUDE.md` — briefly, each board's
`components/Fleet_BSP/include/BSP_<NAME>.h` declares a short device-identity `#define` plus
up to seven `const` struct instances, each aliased to a fixed lowercase name app code reads
(`bsp_hw`, `bsp_display`, `bsp_touch`, …). `platformio.ini` selects a board per environment
with `-D BSP_HEADER='"BSP_<NAME>.h"'` alone.

---

## Per-board test status

Updated 2026-09-08.

| Board | Env | Display | Touch | Audio out | Audio in | WiFi STA | MQTT + HA |
|---|---|---|---|---|---|---|---|
| **WS_P4_7B** ESP32-P4-WIFI6-Touch-LCD-7B | `WS_P4_TOUCH_LCD_7B` | ✅ | ✅ (portrait; rotation untested) | untested — enclosure hides the speaker | untested | ✅ | ✅ |
| **WS_P4_4B** ESP32-P4-WIFI6-Touch-LCD-4B | `WS_P4_TOUCH_LCD_4B` | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ |
| **WS_P4_5** ESP32-P4-WIFI6-Touch-LCD-5 | `WS_P4_TOUCH_LCD_5` | ✅ landscape (rot 1) | ✅ confirmed at rot 1 | ✅ | ✅ codec init only | ✅ | ✅ **on current build** |
| **WS_S3_4B** ESP32-S3-Touch-LCD-4B | `WS_S3_TOUCH_LCD_4B` | ✅ | ✅ | ✅ | ✅ | ✅ native radio | ✅ |
| **CYD_P4_1060** Guition JC1060P470C 7" | `CYD_P4_1060P470` | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ |
| **CYD_S3_3248** Guition JC3248W535 3.5" | `CYD_S3_3248W535` | ✅ **portrait (rot 0) as of 2026-09-10** | ✅ both rotations | ✅ | ✅ | ✅ | ✅ **on current build** |
| **CYD_S3_8048** Guition JC8048W550 5" | `CYD_S3_8048W550` | ✅ brightness slider dead | ✅ | ✅ notably quiet | ✅ | ✅ | ✅ |
| **WS_S3_5B** ESP32-S3-Touch-LCD-5B | `WS_S3_TOUCH_LCD_5B` | ✅ visible tearing | ✅ 5 points | N/A no audio hw | N/A | ✅ | ✅ |

**WiFi and the MQTT/HA pipeline: 8 of 8.** The fleet-wide flash pass on 2026-09-08 put the
full stack on every board — each one joined the network, connected to the broker, published
discovery and appeared in Home Assistant. Reaching HA exercises the whole chain, so a board
that shows up there has demonstrated WiFi, MQTT, discovery and the registry together.

**One nuance worth recording rather than glossing.** Six boards were verified at `dcbe9ff`.
`WS_P4_5` and `CYD_S3_3248` were re-verified at `fa35c21`, after entity storage moved to
PSRAM and the default mode changed. Those two changes are **strictly relieving** — the first
removes ~22 KB from internal RAM, the second does *less* work at boot by not raising an AP —
so neither can plausibly break a board that already worked with more pressure. The six are
treated as verified; if something ever contradicts that, this is the paragraph to re-read.

**⚠ The AP path is the real gap, not the boards.** Every board now uses
`STA_WITH_AP_FALLBACK`, so a board with working credentials never calls `softAP()`. It has
not run anywhere since the crash that motivated the PSRAM change. Tracked as GitHub issue
#45, and it matters because the AP is the rescue path — it runs precisely when something has
already gone wrong.

### The internal-RAM constraint, and which board has it

`GuiManager` places LVGL buffers by bus type: MIPI/RGB boards get full framebuffers in
**PSRAM**; SPI/QSPI boards get partial buffers in **internal SRAM**, because a QSPI panel
cannot stream from PSRAM fast enough.

`CYD_S3_3248` is the fleet's **only QSPI panel**, so it is the only board spending ~61 KB of
internal SRAM on display. That makes it the fleet's real memory constraint and the board any
new static allocation should be checked against first. A 21 KB entity table in internal
`.bss` was enough to make `softAP()` panic inside the WiFi driver; moving that table to PSRAM
took static internal RAM from 209,895 back to 187,960 bytes.

---

## Fleet-wide, still open

- **ESP32-C6 co-processor firmware (P4 boards).** On `WS_P4_5` the host cannot read the
  slave firmware version — `Req_GetCoprocessorFwVersion` times out and reports `0.0.0`
  against host `2.12.11`. WiFi works regardless, but it costs roughly a second of boot time
  in failed RPC retries. Not yet checked on `WS_P4_7B` / `WS_P4_4B`; if they match it is a
  fleet-wide P4 item and Espressif publish a matching slave binary.
- **NVS writes may disturb the P4 hosted-WiFi transport.** Unverified third-party claim
  affecting four of eight boards — GitHub issue #41.
- **SD card** — untested on every board. Low priority.
- **Battery ADC gauge** — no board reads battery percentage yet. Low priority; the
  voltage-divider math for the 3248W535 is in `FUTURE_IMPROVEMENTS.md`.
- **Onboard buttons** — nothing beyond BOOT is inventoried, and no firmware uses one.
- **Silicon revision** — only the `WS_P4_5` has been probed (rev v1.3). One `esptool
  flash-id` each would settle what the fleet actually contains.

---

## Build environment

**Working on a second machine.** `platformio.ini`'s 26 `symlink://` paths are absolute.
A clone elsewhere must rewrite that prefix and **must not commit the change**, or it breaks
the original machine. Only the paths are machine-specific — board flags like
`ARDUINO_USB_CDC_ON_BOOT` belong in the repo. `Fleet_Connectivity/ConnectivityLocalSecrets.h`
and `Fleet_MQTT/MqttLocalSecrets.h` are gitignored and must be recreated by hand.

**Submodules need their own fetch.** The parent repo records only a SHA. After checking out
a branch that moves one: `git submodule update --init --recursive`. Skipping it leaves all
four P4 environments unable to compile.

**Build traps that waste time** — stale build cache, libraries silently not compiled, full
rebuilds on every commit: all in `LESSONS.md` under Build system.

---

## Per-device notes

### WS_P4_7B — ESP32-P4-WIFI6-Touch-LCD-7B

- Test rotation on real hardware to determine whether `TouchManager`'s `#ifndef WS_P4_7B`
  raw-passthrough special case is still needed, or whether the generic rotation transform
  works fine here too (unverified assumption, not a known problem).

  **Evidence gathered 2026-09-07: the generic branch works on a DSI panel at rotation 1.**
  `WS_P4_5` (MIPI/DSI, 720x1280, rotation 1) takes the generic transform and touch tracks
  flawlessly. That is the first time the generic path has run on DSI at a non-zero rotation,
  and it removes the main reason to assume DSI needs the special case. It is **not** proof for
  `WS_P4_7B` itself — that board may have had a different underlying problem — but it shifts
  the burden: the special case should now be treated as suspect rather than as a safe default.
  Cheapest test is to delete the `#ifndef` and flash the 7B.
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

**Fully up as of 2026-09-06 and at parity with the rest of the fleet.** Display, GT911 touch,
ES8311 out, ES7210 in (codec init), WiFi STA on a real DHCP lease, AP with a client attached,
and `STA_PLUS_AP` — all confirmed on hardware. `HIGH_DPI_DISPLAY` confirmed good on this panel.

Bring-up cost most of a session to one root cause: **the HX8394 panel resets ACTIVE HIGH**
while `Arduino_DSI_Display` hardcoded a generic active-LOW sequence that *ends with the pin
asserted*, holding the panel in reset through the entire init. Fixed by
`DisplayConfig.RST_ACTIVE_HIGH`, zero-defaulted so no other board changed. Full history,
ruled-out list and corrections: `docs/BRINGUP_WS_P4_TOUCH_LCD_5.md`.

**Note this board is now `proven` in NVS** — it logged `[Conn] Credentials confirmed working.`
It therefore no longer exercises the unproven permanent-stop path, which was its original
value as a test target. Reaching that path again needs `pio run -t erase` (which wipes the
whole chip, not just NVS).

Two practical traps specific to this board, both expensive to rediscover:
- **`ARDUINO_USB_CDC_ON_BOOT=0` is mandatory or there is no serial output at all.** Its second
  USB-C never enumerates in any state; UART0 via the onboard CH343 is the only channel.
- It enumerates as `USB-Enhanced-SERIAL CH343` (VID_1A86/PID_55D3). Pass `--upload-port`
  explicitly rather than trusting auto-detect.

Still unexercised: rotation (BSP is `ROTATION = 0`), touch mapping at any non-zero rotation,
actual mic capture, SD card.

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

## Resolved board issues worth remembering

Kept for the board-specific facts, not as a changelog - git history is the changelog.

- **CYD_S3_8048 touch was dead**: `.TP_INT` was `18`, a pin from the resistive-touch
  sub-circuit's IRQ rather than the GT911's. Correct value is `-1` (pure I2C polling,
  matching `WS_P4_7B`/`WS_P4_4B`). `.I2S_BCLK` was also wrong at `19`, conflicting with
  `.TP_SDA`; corrected to `0` per the board's pinout spreadsheet.
- **CYD_S3_3248 touch was dead in rotation 1**: `TouchManager::mapCoordinates()` clipped
  against raw unrotated BSP dimensions instead of rotation-aware bounds.
- **WS_S3_4B RGB timing**: a ~25px vertical shift plus tearing after the arduino-esp32
  3.3.11 upgrade, fixed by matching Waveshare's own demo HSYNC/VSYNC pulse-width and porch
  values exactly. Note those values were apparently tried and reverted once before, for an
  unrecorded reason.
- **CYD_P4_1060 partition overflow**: it was the only P4 environment missing
  `board_build.partitions = default_16MB.csv` and began overflowing when arduino-esp32
  3.3.11 grew the framework.
- **WS_P4_5 display**: the HX8394 resets ACTIVE HIGH. Full account in
  `BRINGUP_WS_P4_TOUCH_LCD_5.md`.
- **WS_S3_5B tearing**: root-caused to `Arduino_ESP32RGBPanel` never actually
  double-buffering despite allocating two framebuffers. Real fix tracked in
  `FUTURE_IMPROVEMENTS.md` and GitHub issue #40.

## Pixel density and UI scale — measured 2026-09-10

`-D HIGH_DPI_DISPLAY` is retired. Scale is derived from `DisplayConfig.DIAGONAL_IN` plus the
resolution: `PPI / 170`. See `docs/design/tokens.md` §2 for how the reference figure was chosen.

| Board | Resolution | Diagonal | PPI | Scale | Was |
|---|---|---|---|---|---|
| `CYD_S3_3248` | 320×480 | 3.5" | 165 | 0.97 | 1.0 |
| `CYD_P4_1060` | 1024×600 | 7" | 170 | 1.00 | 1.0 |
| `WS_P4_7B` | 1024×600 | 7" | 170 | 1.00 | 1.0 |
| `WS_S3_4B` | 480×480 | 4" | 170 | 1.00 | 1.0 |
| `CYD_S3_8048` | 800×480 | 5" | 187 | **1.10** | 1.0 — was under-scaled |
| `WS_S3_5B` | 1024×600 | 5" | 237 | 1.39 | 1.5 |
| `WS_P4_4B` | 720×720 | 4" | 255 | 1.50 | 1.5 |
| `WS_P4_5` | 720×1280 | 5" | 294 | **1.73** | 1.5 — was under-scaled |

Two boards were visibly wrong under the old blanket split and one of them is a dev target, so
every padding and font judgement made on `WS_P4_5` before this date was made ~15% too small.

**`WS_P4_4B` and `WS_S3_4B` are the same layout problem in different pixels** — 720×720 at 1.5×
is the same effective UI space as 480×480 at 1.0×. If the scaling approach is right they should be
visually indistinguishable apart from sharpness, which makes them a free correctness check. **Not
yet flashed as a pair.**

Only `WS_P4_5` and `CYD_S3_3248` have been flashed since the change. The other six build clean but
their appearance at the new scales is unverified — `CYD_S3_8048` is the one to look at first, since
it moved.
