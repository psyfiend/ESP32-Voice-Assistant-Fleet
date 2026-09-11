# Future Improvements

Ideas, enhancements, and fixes that affect the **entire build environment / HAL** — not tied
to one device or a small group of devices (that's `HARDWARE_STATUS.md`'s job). Deliberately
deferred work, not bugs, not blocking anything currently. Lower volatility than
`HARDWARE_STATUS.md`; safe to leave stale for a while, but prune entries once actually done.

## Startup / GUI reorganization — **DONE 2026-09-09**

Landed as ROADMAP Phase 2.1 / issue #12. Ended up a five-way split rather than the three-way
one sketched here: `main` / `SystemCore` / `SystemReport` / `LVGL_Startup` / `GUIManager`.
`SystemCore` and `SystemReport` were not anticipated when this entry was written — the ten
subsystem globals needed an owner, and the System Doctor turned out to be the thing standing
between us and an LVGL-free `main`.

Design, reasoning and acceptance criteria: `docs/design/startup.md`. **Code complete, not yet
flashed** — identical on-device behaviour is still unverified.

## LVGL / Display

> **The three display items below have one answer: move off Arduino_GFX onto `esp_lcd`.**
> Scheduled as **ROADMAP milestone 2.9**, after the card system and before Phase 3 — deliberately
> given a slot rather than left as an ephemeral "later". Full analysis in
> `docs/research/display-stack-migration.md`. The short version is here so this file stands alone.

### 2.9 — Arduino_GFX to `esp_lcd`

**This is not a framework migration.** arduino-esp32 3.x *is* ESP-IDF 5.x underneath, and the
libraries are already linked into every binary we ship. Verified on this machine:

- `esp_lcd`, `esp_lcd_mipi_dsi.h` and `esp_lcd_panel_rgb.h` are on the include path for both P4
  package variants, and `-lesp_lcd` is in **every** environment's default link flags.
- `-lesp_driver_ppa` is already linked on the P4 variants. `SOC_PPA_SUPPORTED` is `1` there and
  undefined on S3, which is the real gate — the header ships for every target, so its presence
  proves nothing.
- Arduino_GFX's own RGB and DSI classes turn out to be thin wrappers over these same `esp_lcd`
  calls.

So "port to `esp_lcd`" means *removing a wrapper*, not adopting a new stack. No `platformio.ini`
change is needed to start calling it.

**Why it is worth doing, in the order the reasons actually matter:**

1. **Framebuffers. `Arduino_GFX::getFrameBuffer()` returns a single `uint16_t *`** — the API shape
   is the ceiling, and no fix behind that signature can express double buffering. The RGB path
   already allocates `.num_fbs = 2` and hands out only `fb0`: a whole framebuffer of PSRAM
   allocated and never touched, on every RGB board. The DSI path has a `NUM_FB` BSP field that
   would waste memory the same way. See the correction note below.
2. **Rotation.** Both the QSPI (`Arduino_Canvas`) and MIPI/DSI paths do CPU per-pixel transforms.
   The stutter the owner sees on P4 boards in non-native orientation is exactly this. **Note the
   cheapest fix is not code:** `CYD_S3_3248` was moved to portrait (rotation 0) on 2026-09-10
   precisely because native orientation runs no transform at all — worth checking per board before
   assuming rotation is a requirement.
3. **`esp_lvgl_adapter`.** Espressif's current LVGL porting layer, which the newest Waveshare BSPs
   (P4-4B and P4-7B, `3.0.1`) depend on; older repos use `esp_lvgl_port`. Reaching it means real
   framebuffer management and tear-avoidance modes rather than our hand-rolled flush path.
4. **Alignment with ESP-IDF design philosophy**, which matters beyond this milestone: it is the
   same direction the voice-assistant work would eventually pull (`esp_codec_dev`, `esp-sr`), so
   the two long-term goals stop fighting each other.
5. **PPA** is a P4-only bonus, not the point. LVGL 9.5 already ships a PPA draw unit at
   `components/lvgl/src/draw/espressif/ppa/`, disabled (`LV_USE_PPA 0`) — **and enabling it will
   not fix the stutter**, because `lv_draw_ppa.c:123` explicitly declines rotated draws
   (`dsc->rotation == 0`). Rotation needs hand-written code against the raw PPA API.

**Why 2.9 and not sooner.** Cards are insulated from the display stack by LVGL — a card talks to
LVGL, LVGL talks to `LVGL_Startup`, `LVGL_Startup` talks to `DisplayManager`. Building twenty cards
makes this swap exactly as hard as building zero, so the "do it before X or it gets expensive"
logic that made 2.1 urgent does not apply. What *does* apply: **you cannot judge a render-stack
change without a demanding workload running on it.** Swapping before the card system exists means
measuring six static test panels, which proves nothing about tearing, tileview swipes or rotation
under load. Phase 3 is pure data-layer work and does not care about the display stack, so nothing
downstream is blocked by waiting.

**Known obstacles, from the research:**

- `ESP32_Display_Panel` (Espressif's own, Apache-2.0, so legally reusable) covers 7 of 8 boards'
  controllers but has **no HX8394 driver** — the `WS_P4_5` panel that already cost a full bring-up
  session. It does have AXS15231B. Writing a vendor init for a panel whose sequence we already
  possess is bounded work, not a blocker.
- Touch mapping, the BSP init-command fields, and `Arduino_Canvas` rotation all assume the current
  stack and will need revisiting.
- Eight working boards is the real risk. The research recommends one board converted first, behind
  the existing `DisplayManager` interface, with the other seven untouched.

**Deliberately skipped:** a scoped "PPA rotation inside `LVGL_Startup::disp_flush()`" experiment.
It would work, but it is throwaway work if the wrapper is coming out anyway. The cost of that
decision is living with the P4 stutter until 2.9 — accepted, on the grounds that the *S3* boards
are where the real sluggishness is and PPA cannot help them at all.

### Smaller display items

- **Per-board minimum-brightness floor as a real BSP field.** Currently hardcoded directly
  in `Panel_Display.cpp` (43 for the two 4B boards, 3 for everyone else, based on one
  hardware measurement on the 7B) rather than being a per-device, per-hardware-measured value.
- **Per-device LVGL buffering optimization, backed by actual testing.** The
  `DOUBLE_BUFFERING`/`BUFFER_SIZE_PX` BSP fields exist on every board but are currently
  unused/dead in `GuiManager.cpp` — every board gets the same buffering strategy regardless
  of what these fields say. Measure which boards actually benefit and build real per-device
  logic around it.
- **PPA rotation** — folded into 2.9 above. Kept as a pointer because the buffer discipline is
  the part that will bite: 64-byte alignment, `MALLOC_CAP_DMA | MALLOC_CAP_SPIRAM`, and explicit
  cache writeback/invalidate around every operation. `REFERENCE_PROJECTS.md` covers what is
  minable from Allsky's `ppa_accelerator.h` — approach only, that repo is unlicensed.

- **A written PSRAM allocation-order budget.** *(2.3 measured the LVGL side and found cards cost
  ~715 B in a 124 KB pool, so this is no longer urgent — but the internal-SRAM table below is
  still the useful artefact and nobody has written it.)* Raised
  2026-09-09 after reading Allsky's `docs/developer/architecture.md`, which publishes an exact
  PSRAM map and states that its buffers must be allocated *before* display init so the
  framebuffer still finds contiguous space.

  **Their problem is not ours, and the numbers say so.** Allsky juggles ~15.7 MB in four
  multi-megabyte image buffers where fragmentation genuinely bites. Our PSRAM residents are the
  entity registry (~21 KB) and the LVGL draw buffers; on a MIPI/RGB board the draw buffers are
  full-frame and the largest thing we allocate, and there is nothing else competing.

  **But the *practice* transfers, and we have already been bitten by its sibling.** The
  `CYD_S3_3248` softAP crash was an allocation-order and allocation-location problem in internal
  SRAM — the scarce pool for us — found the expensive way. 2.3 already has to measure per-card
  heap on the smallest board; it should also produce the table: what lives where, in what order,
  and what the headroom is on the worst board. Cheap as a deliverable of a measuring milestone,
  expensive as an archaeology exercise in Phase 6.

  **Starting numbers, measured on `CYD_S3_3248` during the #45 test (2026-09-09):**

  | Measurement | Value |
  |---|---|
  | Static internal RAM (link time) | 188,048 bytes of 327,680 - **57.4%** |
  | Free internal heap just before `softAP()` | 21,968 bytes |
  | Largest free internal block at that moment | 13,300 bytes |
  | LVGL draw buffers (internal SRAM, x2) | 30,720 bytes each |
  | Entity registry (PSRAM) | 22,080 bytes |

  The largest-free-block figure is the one to watch: `softAP()` succeeded from 13,300 bytes, and
  the WiFi driver allocates more per associated station. That is the real headroom, and it is
  thinner than the free-heap total suggests. The 57.4% is only visible at all since the
  `maximum_ram_size` fix - the board previously reported 35.9% against a denominator 1.6x too
  large.
- **`Arduino_ESP32RGBPanel` `num_fbs` investigation.** Requests two hardware framebuffers but
  only ever draws into/reads back one — no real
  double-buffering on any board using this class, causing visible tearing on
  `WS_S3_TOUCH_LCD_5B` (worst case, 1024x600). Real fix needs genuine buffer-swap support
  added to the class, matching Waveshare's own `switchFrameBufferTo()` — real `Arduino_GFX`
  internals work, not a config tweak. See `docs/BRINGUP_WS_S3_TOUCH_LCD_5B.md` for the
  investigation that found this.

### ⚠️ BEFORE tuning LVGL buffering: diff the fork against EVERY vendor GFX tree

**Tracked as GitHub issue #40. Do this first — it is the highest-value hour available on this
topic and it has never been done.**

`reference/Waveshare Official Repos/` holds six vendor repos, each shipping its own
**confirmed-working** copy of `Arduino_GFX` for the board it came with:

| repo | fleet board |
|---|---|
| `Waveshare-P4-WIFI6-Touch-LCD-4B` | `WS_P4_4B` |
| `Waveshare-P4-WIFI6-Touch-LCD-5` | `WS_P4_5` |
| `Waveshare-P4-WIFI6-Touch-LCD-7B` | `WS_P4_7B` |
| `Waveshare-S3-Touch-LCD-4B` | `WS_S3_4B` |
| `WaveShare-S3-Touch-LCD-5B` | `WS_S3_5B` (the tearing board) |
| `Waveshare-S3-Touch-AMOLED-2.06` | not in the fleet — still a free source of fixes |

**Exactly one of these has ever been diffed** (the P4-5), and within it only two files.
That single partial diff is what produced the reset-polarity fix that unblocked the P4-5
after most of a session of wrong theories — the patch was sitting in Waveshare's own
`Arduino_DSI_Display.cpp` with a comment naming the exact failure mode. Assume the other
five carry fixes nobody here has seen.

Files known to differ and never examined (from the P4-5 tree alone): `Arduino_GFX.h`,
`Arduino_ESP32RGBPanel.cpp`, `Arduino_ESP32SPIDMA.cpp`, `Arduino_DSI_Display.h`,
`Arduino_RGB_Display.h`. The RGB ones bear directly on the `num_fbs` item above.

**Why this belongs to the buffering work specifically.** Framebuffer count is the one setting
where the fork and the vendors are known to disagree, and the disagreement is not consistent:

> **Correction, 2026-09-10 — "index 1" was a misreading, and the truth is worse.**
>
> `esp_lcd_rgb_panel_get_frame_buffer()` and `esp_lcd_dpi_panel_get_frame_buffer()` are
> **variadic**, and their second parameter is `fb_num` — *"Number of frame buffer(s) to get. This
> value must be the same as the number of the followed parameters."* Verified in
> `esp_lcd_mipi_dsi.h:131` and `esp_lcd_panel_rgb.h:249`.
>
> So `get_frame_buffer(panel, 1, &frame_buffer)` does not fetch *index 1*. It says **"give me one
> framebuffer"** and returns **fb0**. Both wrappers are asking for exactly one, and getting the
> first.
>
> Which makes the real defect a shape problem, not an off-by-one:
>
> | Path | Allocated | Reachable through the wrapper |
> |---|---|---|
> | RGB (`Arduino_ESP32RGBPanel.cpp:74`) | `.num_fbs = 2` | **fb0 only** |
> | DSI (`Arduino_ESP32DSIPanel.cpp:89`) | `NUM_FB` BSP field, default 1 | **fb0 only** |
>
> On every RGB board we are **allocating a second full framebuffer and never touching it** — pure
> wasted PSRAM. And raising `DisplayConfig.NUM_FB` on a DSI board would do the same thing, because
> the getter cannot express a second buffer either way.
>
> **`Arduino_GFX::getFrameBuffer()` returns a single `uint16_t *`.** No amount of fixing behind
> that signature produces double buffering; the API shape is the ceiling. That is the concrete,
> verified argument for going to `esp_lcd` directly rather than patching the wrapper — see
> `docs/research/display-stack-migration.md`.

- **RGB path** — our fork requests `.num_fbs = 2` but `getFrameBuffer()` returns index
  1, so it never actually double-buffers (the item above). Two buffers of PSRAM paid for, one
  used.
- **DSI path** — the fork hardcoded `num_fbs = 1` until `DisplayConfig.NUM_FB` was added.
  Waveshare's P4-5 copy uses `2`. Only `WS_P4_5` currently sets it, so `WS_P4_7B`,
  `WS_P4_4B` and `CYD_P4_1060` are still single-buffered at the panel level.

So the fleet currently has one class allocating a buffer it never uses and another not
allocating one it might want. Settle both against the vendor trees before writing any
per-board buffering logic — otherwise that logic gets built on top of two unexamined
defaults.

Caution learned the hard way: the 1-vs-2 `num_fbs` test run during P4-5 bring-up came back
"no difference," but it was run against the reset-polarity hang, which masked everything
downstream. **It is not evidence about buffering.** Treat `num_fbs` as untested on this fleet.

### Per-device DPI and font scaling - **DONE 2026-09-10 (milestone 2.2)**

Retired. `HIGH_DPI_DISPLAY` and the `UI_SCALE` macro are gone; scale is derived from
`DisplayConfig.DIAGONAL_IN` plus the resolution via `bspUiScale()` in `bsp_loader.h`, and
`lv_display_set_dpi()` gets the board's real PPI.

This entry predicted the problem exactly - *"a 720x1280 4-inch panel and a 1024x600 7-inch panel
want different scaling even though a single flag treats them as one case"* - and the measurement
bore it out. The old boolean was better than it looked (the fleet clusters at 165-187 and 237-294
PPI with a clean gap) but the high cluster spans 24%, so `WS_P4_5` at 294 PPI and `WS_S3_5B` at
237 PPI both got 1.5x. Two boards were visibly mis-scaled and one was a dev target.

Density table and per-board scales: `docs/HARDWARE_STATUS.md`. Reasoning: `docs/design/tokens.md`
section 2. The diagnostics dump now reports the real computed scale, as this entry asked for.

**One piece deliberately not built: a viewing-distance term.** Pure density scaling makes
everything the same *physical* size, which is right for touch targets (a fingertip is 9 mm on
every board) and arguably wrong for text on a 7-inch panel across a room. Add a small per-board
nudge only if something still looks wrong on glass.

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

**Phase 1 is COMPLETE — see `ROADMAP.md` for what was built and what was descoped.** The
narrative that used to live here (progress logs, the `wifi-testing` branch, the platform
upgrade) has been removed: it described work that is now done, and git history is a better
record of how it went than a stale status section.

What this project originally meant by "WiFi tested" — station mode, AP fallback, and a
portable MQTT component that registers the device and its peripherals with Home Assistant
via discovery, the way an ESPHome device does — is now largely satisfied. Discovery works,
and entities flow in both directions.

**Still deferred, each with an open GitHub issue:** the captive portal (#6), the on-device
settings screen (#7), HA access without an MQTT broker (#43), and outbound entity commands
(part of #10).

The build-environment consequence worth keeping: reaching the P4/C6 boards at all required a
fleet-wide bump to pioarduino `55.03.311` / arduino-esp32 `3.3.11` and PlatformIO Core
`6.1.19`. The P4 has no radio of its own and depends entirely on an onboard ESP32-C6 over
SDIO (`esp_hosted`); older host drivers could not complete the RPC handshake. Do not float
that platform pin without rebuilding all eight environments.

The settings-layering design below still stands and still describes the intended shape.

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

Keep as-is for now — see `HARDWARE_STATUS.md` for the one low-priority fleet-wide item
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
`HARDWARE_STATUS.md` per-device notes for which). Fallback for boards without one: TP4056
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
outside the scope of this environment** — see `HARDWARE_STATUS.md`'s `WS_S3_5B` entry for the
immediate to-do (basic Modbus communication + relay switching only).

## Done

Kept brief — enough to know what changed and why, not a full narrative. See git history for
the rest.

- **Unified `Fleet_BSP.h`.** Was two separately-drifting struct families
  (`Fleet_BSP.h`/`Fleet_BSP_P4.h`), then one nested struct, now seven independent flat
  structs (`BoardHardware`, `ExpanderConfig`, `DisplayConfig`, `TouchConfig`, `LvglConfig`,
  `AudioConfig`, `StorageConfig`) aliased to `bsp_hw`/`bsp_display`/etc. See `CLAUDE.md`'s BSP
  pattern section and `HARDWARE_STATUS.md` for the summary.
- **Board-identity macro moved out of `build_flags`.** Each `BSP_<NAME>.h` now defines its
  own short device macro at the top of the file instead of a redundant separate build flag.
  `HAS_X` capability flags are unaffected, still in `build_flags`.
- **`bb_captouch_fork` GT911 "Invalid IO 255" noise fixed** — guarded `iINT` GPIO calls with
  `if (iINT != -1)`, matching the pattern already used for every other controller type in
  that file.
