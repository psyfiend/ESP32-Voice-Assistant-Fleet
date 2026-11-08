# Hardware Status

**Scope: the boards themselves.** What works on which board, what is untested, per-device
quirks, and build-environment issues. The dashboard project's plan is elsewhere:

| Question | Where |
|---|---|
| What are we building, in what order, what is done | `ROADMAP.md` |
| What is being worked on right now, what is blocked | GitHub issues |
| Which board does what, what is untested | **this file** |
| How the HAL/BSP works | `CLAUDE.md` |
| Fleet-wide deferred work | `FUTURE_IMPROVEMENTS.md` |
| Mistakes worth not repeating | `LESSONS.md` |

The BSP architecture is documented in full in `CLAUDE.md` - briefly, each board's
`components/Fleet_BSP/include/BSP_<NAME>.h` declares a short device-identity `#define` plus
up to seven `const` struct instances, each aliased to a fixed lowercase name app code reads
(`bsp_hw`, `bsp_display`, `bsp_touch`, ...). `platformio.ini` selects a board per environment
with `-D BSP_HEADER='"BSP_<NAME>.h"'` alone.

---

## Per-board test status

For each board's display path, present mode, soak and glass sign-off, `docs/display/README.md` is
the authority; the Display column here is a summary.

| Board | Env | Display | Touch | Audio out | Audio in | WiFi STA | MQTT + HA |
|---|---|---|---|---|---|---|---|
| **WS_P4_7B** ESP32-P4-WIFI6-Touch-LCD-7B | `WS_P4_TOUCH_LCD_7B` | ✅ esp_lcd, rotation 2 | ✅ | untested - the enclosure hides the speaker | untested | ✅ | ✅ |
| **WS_P4_4B** ESP32-P4-WIFI6-Touch-LCD-4B | `WS_P4_TOUCH_LCD_4B` | ✅ esp_lcd | ✅ | ✅ | ✅ | ✅ | ✅ |
| **WS_P4_5** ESP32-P4-WIFI6-Touch-LCD-5 | `WS_P4_TOUCH_LCD_5` | ✅ esp_lcd, landscape (rotation 1) | ✅ | ✅ | ✅ codec init only | ✅ | ✅ |
| **WS_S3_4B** ESP32-S3-Touch-LCD-4B | `WS_S3_TOUCH_LCD_4B` | ✅ esp_lcd (RGB) | ✅ | ✅ | ✅ | ✅ native radio | ✅ |
| **CYD_P4_1060** Guition JC1060P470C 7" | `CYD_P4_1060P470` | ✅ esp_lcd | ✅ | ✅ | ✅ | ✅ | ✅ |
| **CYD_P4_4880** Guition JC4880P443 4.3" | `CYD_P4_4880P443` | ✅ esp_lcd, portrait | ✅ | not tested | not tested | ✅ | ✅ |
| **CYD_S3_3248** Guition JC3248W535 3.5" | `CYD_S3_3248W535` | ✅ Arduino_GFX, portrait (rotation 0) | ✅ both rotations | ✅ | ✅ | ✅ | ✅ |
| **CYD_S3_8048** Guition JC8048W550 5" | `CYD_S3_8048W550` | ✅ Arduino_GFX; brightness slider dead | ✅ | ✅ notably quiet | ✅ | ✅ | ✅ |
| **WS_S3_5B** ESP32-S3-Touch-LCD-5B | `WS_S3_TOUCH_LCD_5B` | ✅ Arduino_GFX; visible tearing | ✅ 5 points | N/A - no audio hardware | N/A | ✅ | ✅ |

### What runs on each board now

The 2.13 build (Add cards from Home Assistant, the group view, Edit cards; the dashboard is the
settings file, K171) is flashed on the four P4 dev boards, each with its settings file converted:
`WS_P4_5`, `WS_P4_4B`, `CYD_P4_1060` and `CYD_P4_4880`. **`WS_P4_7B` and every S3 have not been
flashed with 2.13**; they build, but nothing has been seen on their glass. Before a board takes the
2.13 build its settings file must be converted (`scripts/release/retire_compiled.py`); without a
settings file a board shows only Panel (Fleet page) and Garage - Doors (House page).

- **`CYD_S3_3248`** has a broken USB connector. It can still be powered by battery or headers and
  flashed over UART.
- **`WS_S3_4B`** locked up (#69) and waits to be troubleshot. It is the S3 compile check: it builds
  with every release but is flashed only when the owner says so.
- The Arduino_GFX boards (`CYD_S3_8048`, `WS_S3_5B`, `CYD_S3_3248`) have not moved to esp_lcd.

### WiFi and the MQTT/HA pipeline

All nine boards joined the network, connected to the broker, published discovery and appeared in
Home Assistant. Reaching HA exercises the whole chain, so a board that shows up there has
demonstrated WiFi, MQTT, discovery and the registry together.

**The AP path is the real gap, not the boards.** Every board uses `STA_WITH_AP_FALLBACK`, so a board
with working credentials never calls `softAP()`. That path has not run since entity storage moved to
PSRAM. It matters because the AP is the rescue path - it runs precisely when something has already
gone wrong (GitHub issue #45).

### The internal-RAM constraint, and which board has it

`LVGL_Startup` places LVGL's draw buffers by bus type: MIPI and RGB boards get full frame buffers
in **PSRAM**; QSPI boards get partial buffers in **internal SRAM**, because a QSPI panel cannot
stream from PSRAM fast enough. LVGL's own pool is 512 KB in PSRAM on every P4 and 128 KB in PSRAM
on every S3 (`FLEET_LV_MEM_PSRAM` in `[P4-options]` and `[S3-options]`). Large fixed window buffers
on the S3s use `PsramArray` (`include/PsramArray.h`).

`CYD_S3_3248` is the fleet's **only QSPI panel**, so it is the only board spending ~61 KB of
internal SRAM on display. That makes it the fleet's real memory constraint and the board any
new static allocation should be checked against first. A 21 KB entity table in internal
`.bss` was enough to make `softAP()` panic inside the WiFi driver; the table is in PSRAM now.

---

## The ESP32-C6 coprocessor firmware - the P4 boards' hidden variable

The five P4 boards have no radio of their own; WiFi runs on an onboard ESP32-C6 over SDIO
(`esp_hosted`). **Host and slave firmware must be interoperable.** Waveshare publish the matrix in
`docs/P4_C6_HOSTED_WIFI.md` in two of the vendor trees under `reference/Waveshare Official
Repos/`: esp_hosted 1.4.x pairs with ESP-IDF before 6.0, 2.12-3.0 with 6.0 and later. Our host runs
**2.12.11**.

A P4 boot that prints a warning that the host could not read the slave's version is the mismatch
announcing itself, not a cosmetic message: an older slave does not implement that RPC at all.
GitHub issue #59.

| Board | C6 firmware (last known) | Boot RPC warning |
|---|---|---|
| `WS_P4_4B` | **2.12.9** | gone |
| `WS_P4_5` | unknown, factory | present |
| `WS_P4_7B` | unknown, factory | present |
| `CYD_P4_1060` | unknown, factory | not checked |
| `CYD_P4_4880` | not checked | not checked |

### How it is updated

`reference/Examples and related projects/ESP32-P4-NINA-Display/firmware/merged-flash.bin` is a P4
APPLICATION, not a C6 image. Flashed to the P4 at `0x0`, it updates the C6 over the internal link,
shows progress on screen, restarts to resync, and then normal firmware is flashed again. No second
cable, no separate programmer. Full procedure and rollback in #59. **It hung `WS_P4_5` once; do not
use it there without the owner's yes.**

**It is a full-flash write, so it wipes NVS** - stored credentials included. The board falls back to
the compile-time defaults in `ConnectivityLocalSecrets.h` and the `_proven` flag resets.

### Facts about the C6 link

- The slave firmware lives in a partition on the P4's OWN flash: `slave_fw`, 2 MB, project name
  `network_adapter`.
- The host resets the C6 over **GPIO 54**.
- SDIO on the 4B: `CLK[18] CMD[19] D0[14] D1[15] D2[16] D3[17]`.
- The 4B's flash chip reports 32 MB; the image header says 16 MB and the header wins.

Whether the C6 update fixes the overnight dropouts (#49) is unproven; that needs a soak, with
`WS_P4_5` and `WS_P4_7B` left alone as controls.

## Memory and flash

| | PSRAM | Flash chip | Partition table in use |
|---|---|---|---|
| `WS_P4_4B`, `WS_P4_5`, `WS_P4_7B` | **32 MB** | 32 MB | 16 MB layout |
| `CYD_P4_1060`, `CYD_P4_4880` | **32 MB** | 16 MB | the same 16 MB layout |
| Every S3 board | **8 MB** | 16 MB | `default_16MB.csv` |

The three Waveshare P4s' extra 16 MB of flash is unused because nothing has needed it. Worth remembering at
**5.1**, when the partition layout is fixed for OTA. The 32 MB PSRAM is why a full-screen
screenshot on `WS_P4_5` (about 2.8 MB at peak, #58) costs nothing that matters. Measured there:
28.7 MB free at runtime, with full DSI framebuffers already allocated.

## Fleet-wide, still open

- **ESP32-C6 co-processor firmware (P4 boards).** On `WS_P4_5` the host cannot read the
  slave firmware version - `Req_GetCoprocessorFwVersion` times out and reports `0.0.0`
  against host `2.12.11`. WiFi works regardless, but it costs roughly a second of boot time
  in failed RPC retries. The same warning shows on `WS_P4_7B`; `WS_P4_4B` lost it with the C6 update.
- **NVS writes may disturb the P4 hosted-WiFi transport.** Unverified third-party claim
  affecting the P4 boards - GitHub issue #41.
- **SD card** - untested on every board. Low priority.
- **Battery ADC gauge** - no board reads battery percentage yet. Low priority; the
  voltage-divider math for the 3248W535 is in `FUTURE_IMPROVEMENTS.md`.
- **Onboard buttons** - nothing beyond BOOT is inventoried, and no firmware uses one.
- **Silicon revision** - `BoardHardware.SI_REV` stays `"unconfirmed"` on every P4 (it is a per-chip
  property); the System Doctor reads the real one at runtime. `WS_P4_5` reports rev v1.3.
- **RTC cells** - the Waveshare P4s need rechargeable ones (the 7B's holder is ML1220); nothing
  reads an RTC yet (#32).
- **The S3 RGB boards probably flash on flash writes**, as the P4s did before A14: `CONFIG_LCD_RGB_ISR_IRAM_SAFE`
  is off in the stock S3 libraries. Untested.

---

## Build environment

**Working on a second machine.** `platformio.ini`'s 26 `symlink://` paths are absolute.
A clone elsewhere must rewrite that prefix and **must not commit the change**, or it breaks
the original machine. Only the paths are machine-specific - board flags like
`ARDUINO_USB_CDC_ON_BOOT` belong in the repo. `Fleet_Connectivity/ConnectivityLocalSecrets.h`
and `Fleet_MQTT/MqttLocalSecrets.h` are gitignored and must be recreated by hand.

**Submodules need their own fetch.** The parent repo records only a SHA. After checking out
a branch that moves one: `git submodule update --init --recursive`. Skipping it leaves all
five P4 environments unable to compile.

**Build traps that waste time** - stale build cache, libraries silently not compiled, full
rebuilds on every commit: all in `LESSONS.md` under Build system.

---

## Per-device notes

### WS_P4_7B - ESP32-P4-WIFI6-Touch-LCD-7B

- **Touch mapping is unverified here.** `TouchManager::mapCoordinates()` has an `#ifndef WS_P4_7B`
  special case: this board always gets raw coordinate passthrough, whatever its rotation. The
  generic rotation transform is proven on DSI at a non-zero rotation (`WS_P4_5`, rotation 1, touch
  tracks correctly), which removes the main reason to believe DSI needs the special case; it is not
  proof for this board. Cheapest test: delete the `#ifndef` and flash the 7B.
- Test RS485 (Modbus board available).
- WiFi (STA) works. It shows the same `hostedHasUpdate()` / `Req_GetCoprocessorFwVersion` RPC
  warning as the other factory-C6 boards; the warning is harmless to the connection (see the C6
  section above).
- The 7B was the primary development target. The dev boards are `WS_P4_5`, `WS_P4_4B`, `CYD_P4_1060`
  and `CYD_P4_4880` (`WS_S3_4B` is the compile-only S3 check, #69); all nine build once before each merge.

### WS_P4_5 - ESP32-P4-WIFI6-Touch-LCD-5

Display, GT911 touch, ES8311 out, ES7210 in (codec init), WiFi STA on a real DHCP lease, AP with a
client attached, and `STA_PLUS_AP` are all confirmed on hardware.

Bring-up cost most of a session to one root cause: **the HX8394 panel resets ACTIVE HIGH**
while the generic sequence resets active LOW and ends with the pin asserted, holding the panel in
reset through the entire init. `DisplayConfig.RST_ACTIVE_HIGH` (zero-defaulted, so no other board
changed) is the fix. Ruled-out list and the full account: `docs/BRINGUP_WS_P4_TOUCH_LCD_5.md`.

Its credentials are `proven` in NVS (`[Conn] Credentials confirmed working.`), so it no longer
exercises the unproven permanent-stop path. Reaching that path again needs `pio run -t erase`
(which wipes the whole chip, not just NVS).

Two practical traps specific to this board:
- **`ARDUINO_USB_CDC_ON_BOOT=0` is mandatory or there is no serial output at all.** Its second
  USB-C never enumerates in any state; UART0 via the onboard CH343 is the only channel.
- It enumerates as `USB-Enhanced-SERIAL CH343` (VID_1A86/PID_55D3). Pass `--upload-port`
  explicitly rather than trusting auto-detect.

Unexercised: actual mic capture, SD card.

See `FUTURE_IMPROVEMENTS.md`'s New Hardware section.

### WS_P4_4B - ESP32-P4-WIFI6-Touch-LCD-4B

Does **not** have battery headers (the only P4 board without one, as far as known). Has 2x
14-pin expansion headers (7x2, 2.0mm pitch), intended for an optional relay/ethernet
component not currently owned. Believed to expose: 5V bus, USB data (`USB_IN1_P`/`USB_IN1_N`),
and SDA/SCL.

- **Battery power: unconfirmed.** `VCC_5V` header pin measured 5V with the board
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
- **WiFi (STA)** runs through the onboard ESP32-C6 co-processor over SDIO (`esp_hosted`), since the
  P4 has no radio. It connects cleanly and gets a real DHCP IP. With a factory C6, serial shows
  `rpc_core: Response not received for [0x15e] (Req_GetCoprocessorFwVersion)` and
  `hostedHasUpdate(): Could not get slave firmware version` on every boot: `hostedInit()` in
  `esp32-hal-hosted.c` calls `hostedHasUpdate()` only to log the version, discards its return value
  and returns success, so the warning cannot block the connection (the C6 section above covers why
  it is still worth fixing). This board's C6 is 2.12.9 and no longer warns. It needs pioarduino
  55.03.311 / arduino-esp32 3.3.11 - see `FUTURE_IMPROVEMENTS.md`'s Connectivity section.
- The panel can wash out to near-white and lurid while `/screenshot` is right (the panel, not the
  frame buffer). `GET /panel?resend=1` sends the init sequence again.

### WS_S3_4B - ESP32-S3-Touch-LCD-4B

On-board hardware not yet touched by firmware - develop libraries for each, reference
Waveshare's own repo:
- **AXP2101** power management chip
- **PCF85063** RTC clock chip
- **QMI8658** 6-axis IMU
- Controllable **PWRKEY** button

WiFi (STA) uses the native S3 radio, no co-processor or hosted-transport layer, so it is the
simplest connect path in the fleet.

**RGB timing:** the BSP's HSYNC/VSYNC pulse-width and porch values match Waveshare's own Arduino
demo exactly. With other values the panel showed a ~25 px vertical frame shift plus
tearing/flicker after the arduino-esp32 3.3.11 bump; whether the cause was the BSP values or a
framework-side RGB driver change is unconfirmed (`FUTURE_IMPROVEMENTS.md`). The ST7701's own
timing against its datasheet is in `docs/display/panels.md`.

### WS_S3_5B - ESP32-S3-Touch-LCD-5B

- **RS485** controlling a WaveShare Modbus-RTU-Relay-B board (planned sprinkler-controller
  project, out of this environment's scope - see `FUTURE_IMPROVEMENTS.md`).
- **Digital IO (relay/DIO pins)** - planned use: wired garage door opener buttons.
- Tearing: `Arduino_ESP32RGBPanel` allocates two frame buffers and never double-buffers. The real
  fix is the move to esp_lcd (#67, #40).

### CYD_P4_1060 - Guition P4 7" (JC1060P470C)

Has a physical **Ethernet port** in addition to WiFi - worth testing as an alternate
connectivity path.

**Its panel looks better than the 7B's** side by side (same 1024x600 and 7"): colors closer to the
rest of the fleet, and noticeably less fuzzy, more crisp. The owner had expected the 7B to be the
better board for its features and its maker. Unexplained; nobody has looked at why.

### CYD_P4_4880 - Guition P4 4.3" portrait (JC4880P443C_I_W)

The owner's successor to `CYD_S3_3248` as the small portrait board: native 480x800, 217 PPI (UI
scale 1.28), ST7701 over 2-lane MIPI-DSI, esp_lcd only. It booted on the PC's USB without its LiPo.
Flashed by hand (`flash_release.ps1` does not cover it); COM19.

Settings: a board coming from firmware older than `v0.2.11` may hold an empty SPIFFS in the
`spiffs` partition (block headers only, no files), which the settings store leaves alone
(DECISIONS A17) and mounts read-only. Erase it with the owner's yes (`esptool erase-region 0xc90000
0x360000`, a copy kept first); the next boot formats it and saves.

Everything in its BSP came from `reference/Guition Examples/Guition-P4-JC4880P433/` (folder
misnamed P433; the contents are the P443) - the schematic for pins, Guition's modified IDF BSP and
ST7701 driver for the panel. **Guition's own two examples disagree** on three things, and the BSP
keeps both values: pixel clock 28 MHz (IDF, 49.8 Hz) vs 34 MHz (Arduino, 60.5 Hz); lane rate 750 vs
500 Mbps; panel reset none (IDF) vs GPIO5 (Arduino - and the schematic routes GPIO5 to the panel
connector, so the BSP uses 5). Also on the board, not yet in the BSP: an **OV02C10 camera** on
MIPI-CSI, **RS485**, and RMII Ethernet pins on the schematic (whether a PHY is fitted on the C_I_W
variant is unconfirmed). ES8311 only - the schematic's "ES7210_SDOUT" net is the ES8311's own mic
output. Battery on GPIO53 through a 68K/100K divider.

### CYD_S3_8048 - Guition 5" (JC8048W550)

- **Brightness slider is broken** (`.BL_FREQ = 0`, on/off backlight only - no PWM circuit).
  Needs a decision: hide the slider entirely (matching the `HAS_ES7210`/`HAS_ES8311`-style
  capability gating already used in `Panel_Audio.cpp`), or confirm the hardware genuinely has
  no PWM dimming before ruling out a real fix.
- **Battery ADC pin needs double-checking** - the schematic shows the same voltage-divider circuit
  as the 3248W535 tapped on GPIO17, which is also this board's `I2S_DOUT` pin. Unconfirmed;
  needs a multimeter continuity check. Likely an outdated or wrong Guition schematic
  (they have a pattern of this), but not yet verified either way.
- **Investigate external audio hardware** - audio is notably quieter than other boards,
  unexplained; see `FUTURE_IMPROVEMENTS.md`'s Audio section for the gain-tuning angle.

### CYD_S3_3248 - Guition 3.5" (JC3248W535)

- **Investigate external audio hardware** - not yet reviewed against Waveshare's recent
  sample/documentation updates (see `FUTURE_IMPROVEMENTS.md`).
- **Touch panel edge margins** (informational, not a bug) - in rotation 0 (native portrait),
  the physical digitizer doesn't register touches across its full nominal 320x480 extent.
  Observed usable range: X ~12-310, Y ~14-461. Worth keeping in mind for UI margin/hit-target
  sizing near screen edges, and worth checking whether other boards with the same/similar
  touch controller have a comparable margin.
- **Sluggish panel animations** - panel slide-open/close is slow and chunky. Not root-caused. Two
  plausible, not-yet-isolated contributors: QSPI bus bandwidth ceiling (the only QSPI panel in the
  fleet, inherently lower-bandwidth than RGB/DSI boards), or the CPU cost of `Arduino_Canvas`'s
  per-pixel software rotation transform, paid on every draw call (see `CLAUDE.md`'s Display/touch
  pipeline section for why that exists).
- **WiFi (STA) boot resets are not reproducing, and were not root-caused.** Earlier firmware reset
  the board several times before it connected, each reset re-entering `setup()` from scratch.
  Current firmware boots fast and connects cleanly. Nothing about the power supply, cable or USB
  port changed, which argues against the leading brownout hypothesis rather than confirming it. The
  firmware changes in between were LVGL v9.4.0+134 (untagged) to v9.5.0, the platform pinned to the
  already-installed 55.03.311, and the board JSON moving from PlatformIO's install directory to
  `boards/`. That last one is the one to check if the resets return: the vendored JSON came from
  `~/.platformio/boards/boards 3.3.0/` and was never verified byte-identical to the copy that had
  been in the platform directory, and a difference in `f_flash`, `flash_mode` or `psram_type` could
  change boot-time power behaviour (the environment overrides several of those in `platformio.ini`
  anyway). If it comes back: capture serial from cold power-on including the ROM bootloader and look
  for `Brownout detector was triggered!`.

## Resolved board issues worth remembering

Kept for the board-specific facts; git history is the changelog.

- **CYD_S3_8048 touch**: `.TP_INT` must be `-1` (pure I2C polling, matching `WS_P4_7B`/`WS_P4_4B`);
  `18` is the resistive-touch sub-circuit's IRQ, not the GT911's. `.I2S_BCLK` is `0` per the board's
  pinout spreadsheet; `19` conflicts with `.TP_SDA`.
- **CYD_S3_3248 touch in rotation 1**: `TouchManager::mapCoordinates()` clips against
  rotation-aware bounds, not the raw unrotated BSP dimensions.
- **CYD_P4_1060 partition**: every P4 environment needs `board_build.partitions = default_16MB.csv`,
  or the image overflows the default table.
- **WS_P4_5 display**: the HX8394 resets ACTIVE HIGH. Full account in
  `BRINGUP_WS_P4_TOUCH_LCD_5.md`.
- **WS_S3_5B tearing**: `Arduino_ESP32RGBPanel` never double-buffers despite allocating two
  framebuffers. Tracked in `FUTURE_IMPROVEMENTS.md` and GitHub issue #40.

## Pixel density and UI scale

`-D HIGH_DPI_DISPLAY` is retired. Scale is derived from `DisplayConfig.DIAGONAL_IN` plus the
resolution: `PPI / 170`. See `docs/design/tokens.md` §2 for how the reference figure was chosen.

| Board | Resolution | Diagonal | PPI | Scale |
|---|---|---|---|---|
| `CYD_S3_3248` | 320×480 | 3.5" | 165 | 0.97 |
| `CYD_P4_1060` | 1024×600 | 7" | 170 | 1.00 |
| `WS_P4_7B` | 1024×600 | 7" | 170 | 1.00 |
| `WS_S3_4B` | 480×480 | 4" | 170 | 1.00 |
| `CYD_S3_8048` | 800×480 | 5" | 187 | 1.10 |
| `CYD_P4_4880` | 480×800 | 4.3" | 217 | 1.28 |
| `WS_S3_5B` | 1024×600 | 5" | 237 | 1.39 |
| `WS_P4_4B` | 720×720 | 4" | 255 | 1.50 |
| `WS_P4_5` | 720×1280 | 5" | 294 | 1.73 |

`WS_P4_4B` and `WS_S3_4B` are the same layout problem in different pixels: 720×720 at 1.5× is the
same effective UI space as 480×480 at 1.0×. If the scaling is right they look the same apart from
sharpness, which makes the pair a free correctness check.
