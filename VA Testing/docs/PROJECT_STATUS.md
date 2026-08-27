# Project Status

Volatile — reflects state as of the current recovery/audit pass. Update or prune entries as
they get resolved; don't let this file grow stale.

## Per-board test status

| Board | Env | Display | Touch | Audio out | Audio in (mic) |
|---|---|---|---|---|---|
| **ESP32-P4-WIFI6-Touch-LCD-7B** (WaveShare, aka WS_P4_7B) | `waveshare_p4_7b_base` | ✅ | ✅ (portrait; rotation untested) | untested (no speaker access — enclosure doesn't expose it) | untested |
| WaveShare P4 Smart86 | `waveshare_p4_smart86_base` | ✅ | ✅ | ✅ | ✅ |
| **ESP32-S3-Touch-LCD-4B** (WaveShare, aka "S3 Smart86") | `waveshare_s3_smart86_base` | ✅ | ✅ | ✅ | ✅ |
| Guition P4 7" (JC1060P470C) | `guition_1060p470c_base` | ✅ | ✅ | ✅ | ✅ (first-ever test of the ES8311-as-sole-mic-input path) |
| Guition 3.5" (JC3248W535) | `guition_3_5_base` | ✅ (both rotation 0 and 1 confirmed) | ✅ **fully confirmed, both rotations** — see below for the fix | ✅ | ✅ |
| Guition 5" (JC8048W550) | `guition_8048w550_base` | ✅ (brightness slider broken, see below) | ✅ (fixed, see below) | ✅ (notably quieter than other boards, unexplained) | ✅ |
| WaveShare ESP32-S3-Touch-LCD-5B | `waveshare_s3_touch_lcd_5b_base` | ✅ (visible tearing, see below) | ✅ (confirmed 5 simultaneous points) | N/A (no audio hardware on this board) | N/A |

SD card and WiFi are **untested on every board** — not touched this session at all. Note on
naming: the codebase still uses `WS_S3_SMART86`/"S3 Smart86" internally (BSP filenames,
build flags, this doc's earlier sections) — going forward, prefer the official model names
**ESP32-S3-Touch-LCD-4B** and **ESP32-P4-WIFI6-Touch-LCD-7B** in conversation/docs, even
though the code itself hasn't been renamed to match.

## Connectivity testing (WiFi/SD) — not yet meaningfully started

"WiFi tested" doesn't mean "can join an access point" here — the actual bar is **MQTT +
Home Assistant integration**: device discovery, entity/component surfacing, and talking to
HA the way an ESPHome device would. That requires a modular, portable MQTT
library/component first (see `FUTURE_IMPROVEMENTS.md`) — building that is a prerequisite
for WiFi testing to mean anything close to what these boards are actually for (home
automation dashboards driven by Home Assistant data).

SD card testing is more straightforward (mount, read, write) and doesn't have the same
prerequisite — can happen independently, per board, whenever convenient.

## Per-board testing to-do

### ESP32-S3-Touch-LCD-4B ("S3 Smart86")

On-board hardware not yet touched by firmware at all:
- **AXP2101** power management chip
- **PCF85063** RTC clock chip
- **QMI8658** 6-axis IMU
- Controllable **PWRKEY** button

### ESP32-P4-WIFI6-Touch-LCD-7B (WS_P4_7B)

WaveShare published official Arduino examples/libraries for this board on GitHub as of
2026-08-20 — worth mining for comparison and reference:
- They use **GFX Library 1.6.0** (matches what this project already uses) and **Arduino
  core 3.3.11** — may need a core version bump locally to match.
- **Board hardware variant split**: there's a pre-v3 and post-v3 revision of this board,
  and some of WaveShare's own sketches/demos need to branch on which one they're targeting.
  The pre-v3 variant uses `PLL_F20M` as the MIPI DSI PHY clock source — **this matches the
  `MIPI_DSI_PHY_PLLREF_CLK_SRC_PLL_F20M` fix already committed in this project's
  `GFX_Library_for_Arduino` submodule** (see CLAUDE.md history / the pre-session triage that
  confirmed and kept that fix). Good independent confirmation that fix was correct — but
  worth double-checking which hardware revision this project's physical 7B unit actually is.
- They provide their own `es8311`/`es7210` driver versions — worth comparing against this
  project's `AudioManager`-adapted versions, and worth looking at their playback/record demo
  sketches for anything done differently.
- They have an **LVGL9 demo** — worth studying for buffer configuration and init sequencing,
  especially anything related to rotation handling (relevant to the open DSI-rotation
  question in `FUTURE_IMPROVEMENTS.md`).
- Additional display config/libraries and a substantial touch API surface provided — worth a
  look before assuming this project's touch handling is already doing everything available.
- **To physically test**: SD card, WiFi, and RS485 (user has a WaveShare Modbus board to
  test against).

### Guition boards (all)

Just need WiFi and SD card functionality tested — no other outstanding to-do beyond that
noted above.

### WaveShare ESP32-S3-Touch-LCD-5B

Display and touch are confirmed working (see `docs/BRINGUP_WS_S3_TOUCH_LCD_5B.md` for the
full bring-up investigation). Still untested:
- **RS485** — user has a WaveShare Modbus-RTU-Relay-B board to test against; see the
  sprinkler-controller project note in `FUTURE_IMPROVEMENTS.md`.
- **Digital IO (relay/DIO pins)**
- **SD card**
- **WiFi/connectivity** — same fleet-wide MQTT/Home Assistant prerequisite as every other
  board, see above.
- Fleet-wide **per-device LVGL settings** item (double-buffering, etc.) applies here too —
  see `FUTURE_IMPROVEMENTS.md`.

## Open bugs / unresolved investigations

### Guition 3.5" (3248W535) — touch dead in rotation 1 — ✅ RESOLVED, confirmed on hardware

Root cause: `TouchManager::mapCoordinates()`'s sanity-clip step used `cfg.WIDTH`/`cfg.HEIGHT`
directly — the BSP's raw, *unrotated* native dimensions (`320x480`), which never swap for
rotation. In landscape, any touch past X=319 was silently clamped to exactly X=319 instead of
its real position (explained the "slider knob bulges but never tracks/commits" symptom).
Fixed by making the clip bounds rotation-aware. Confirmed working on hardware: full landscape
touch responsiveness restored, all panels/toggles/sliders functional in both rotations.

The apparent "no visible change between rotation 1 and 3" red herring from earlier in the
investigation turned out to be an unrelated, separate PlatformIO build-cache issue (BSP
header edits weren't reaching the compiled binary at all in some of those tests) — see
`CLAUDE.md`'s build-cache section. Not a rotation-math problem; resolved once the cache was
cleared.

**Cleanup still pending**: two temp diagnostics remain in the code and should come out —
`TouchManager::read()`'s `RAW`/`MAPPED` coordinate `Serial.printf`s, and
`DisplayManager::begin()`'s rotation-size debug print block.

### Guition 3.5" (3248W535) — touch panel edge margins (informational, not a bug)

In rotation 0 (native portrait), the physical touch panel does not register touches across
its full nominal `320x480` extent. Observed usable range: **X from ~12 to ~310, Y from ~14
to ~461** — a real, sizable dead margin around all four edges of the digitizer itself.
Worth keeping in mind for UI margin/hit-target sizing near screen edges on this board, and
worth checking whether other boards using the same or similar touch controllers have a
comparable margin.

### Guition 3.5" (3248W535) — sluggish panel animations

Reported as noticeably slow/chunky (panel slide-open/close). Not root-caused. Two plausible
contributors, not yet isolated from each other:
- QSPI bus bandwidth ceiling (this board is the only tested QSPI panel; inherently
  lower-bandwidth than the RGB/DSI boards).
- CPU cost of `Arduino_Canvas`'s per-pixel software rotation transform, paid on every draw
  call (see CLAUDE.md's Display/touch pipeline section for why this exists).

### WS_P4_7B `TouchManager` rotation bypass — unverified

`TouchManager::mapCoordinates()` has `#ifndef WS_P4_7B` gating the generic rotation
transform; WS_P4_7B instead always does raw passthrough. This was inherited from the commit
being audited this session, never actually re-tested against the generic transform on real
7B hardware, and the reasoning behind it isn't documented anywhere. Not currently causing a
known problem (7B touch works as-is), but flagged as an assumption, not a verified fact.

### Guition 5" (8048W550) — touch fixed (bad IRQ pin, same root cause as a known CYD_550 regression from months ago)

`.TP_INT` was `18`, pulled from the wrong section of a Guition schematic (the *resistive*
touch sub-circuit's IRQ, not the actual capacitive/GT911 IRQ, which the schematic doesn't
give a real pin for at all). This is the same wrong value (`18` instead of `-1`) that broke
CYD_550 touch months ago via a different path (`bb_captouch`'s own config table) — reverted
back then, but reintroduced independently through this board's BSP field. Fixed by setting
`.TP_INT = -1` (pure I2C polling, no interrupt pin — same pattern already working on
WS_P4_7B and WS_P4_Smart86). Confirmed working on hardware.

Also corrected while investigating: `.I2S_BCLK` was `19`, conflicting with `.TP_SDA` (also
`19`, confirmed correct/working via successful I2C touch detection). Vendor docs disagreed
with each other here — an `Audio_test.ino` demo sketch claimed BCLK=19, but the board's own
pinout spreadsheet said BCLK=GPIO0. Went with the spreadsheet (matches a pre-existing `// 0,`
comment already in the file, and doesn't conflict with the confirmed-working I2C pins).
GPIO0 is the boot-strapping pin — works fine as a runtime I2S clock line on ESP32, but worth
a few power-cycle tests to confirm nothing about claiming that pin for I2S interferes with a
clean boot.

### Guition 5" (8048W550) — brightness slider has no effect, starts at 0

This board's BSP has `.LCD_BL_FREQ = 0` with a `// Not PWM` comment — meaning someone
(previously) determined this board's backlight is on/off only, no PWM dimming circuit. Given
that, `DisplayManager::initBacklightPWM()` correctly skips PWM setup entirely per existing
logic (`if (cfg.LCD_BL_FREQ <= 0) { ...skip...; return; }`) — but two things are still wrong:
1. The brightness slider UI in `Panel_Display.cpp` doesn't know or care that this board has
   no PWM capability, so it still renders and lets you drag it, with no effect.
2. Skipping PWM setup also skips the `setBrightness(_defaultBrightness)` call at the end of
   `initBacklightPWM()`, meaning `_currentBrightness` never gets set — likely defaulting to
   `0` via static zero-initialization (matching "starts at 0"), rather than the intended
   default of 75.

The misleading "0%"/confusing log output is fixed (`_currentBrightness` now correctly
reports 100% when there's no PWM, instead of an uninitialized-looking 0%). The slider itself
still does nothing on this board — needs a decision on whether it should hide the brightness slider
entirely (matching `HAS_ES7210`/`HAS_ES8311`-style capability gating already used elsewhere
in `Panel_Audio.cpp`) or get real PWM dimming wired up if the hardware actually supports it
after all (worth double-checking `LCD_BL_FREQ = 0` is actually correct before assuming the
UI is the only thing to fix).

### WaveShare ESP32-S3-Touch-LCD-5B — visible tearing

Root-caused during bring-up: `Arduino_ESP32RGBPanel` requests two hardware framebuffers but
only ever draws into/reads back one, so there's no real double-buffering happening — every
draw writes into the same buffer that's simultaneously being scanned out. Not unique to this
board (affects every board using that class), but most visible here since this board's
1024x600 panel pushes ~60% more pixels per frame than `8048W550` through the same
single-buffer bottleneck. Real fix needs genuine buffer-swap support added to the class; see
the `Arduino_ESP32RGBPanel` bullet under "LVGL / display" in `FUTURE_IMPROVEMENTS.md` and
`docs/BRINGUP_WS_S3_TOUCH_LCD_5B.md` for the full investigation.

### CYD_S3_8048W550 — possible GPIO17 conflict between battery ADC and I2S DOUT

Schematic for this board shows the same battery voltage-divider circuit as the 3248W535
tapped on GPIO17 — which is also this board's `I2S_DOUT` pin in the BSP. A GPIO can't
usefully be both a live I2S output and a stable ADC input at the same time. Most likely
explanation is an outdated/misrepresented Guition schematic (their documentation has a
pattern of this), but **unconfirmed** — needs a physical multimeter continuity check between
the two chips before trusting `BAT_ADC` on this board.
