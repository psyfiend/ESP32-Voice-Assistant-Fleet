# ESP32 Voice Assistant Fleet — VA Testing

PlatformIO + Arduino-framework firmware for a shared codebase running across multiple
ESP32-S3 and ESP32-P4 boards (WaveShare and Guition displays), each with a touchscreen,
audio codec(s), and LVGL-based UI. One codebase, many boards, selected at build time via
PlatformIO environments.

See `docs/PROJECT_STATUS.md` for current bugs and unconfirmed/untested items,
`docs/FUTURE_IMPROVEMENTS.md` for deliberately deferred work, `docs/GUI_FRAMEWORK.md` for
the UI layer's own longer-term vision/architecture (separate from the HAL concerns this file
covers), and `docs/BRINGUP_WS_S3_TOUCH_LCD_5B.md` for that board's own detailed bring-up
history (RGB bounce-buffer bug, the still-open GT911 touch issue, everything tried and
ruled out). `docs/BRINGUP_WS_P4_TOUCH_LCD_5.md` covers the P4-5 bring-up and the ESP32-P4
silicon-revision investigation. All of these are more volatile than this file and worth
checking first for "is X already known/planned."

## Board selection

Each PlatformIO environment in `platformio.ini` picks a board via `build_flags`:
`-D BSP_HEADER='"BSP_<NAME>.h"'` tells `bsp_loader.h` which header to include — that's the
*only* board-select build flag now. Each `BSP_<NAME>.h` itself declares a single short
device-identity `#define` at its own top (e.g. `#define WS_P4_7B`, `#define CYD_S3_3248`),
used fleet-wide for `#ifdef <BOARDNAME>`-style device-specific branches. This used to be a
second, redundant `-D <BOARDNAME>` build flag (BSP_HEADER already implies exactly which
board it is) — collapsed down to the one flag plus the in-header `#define`. `HAS_X`
capability flags (`HAS_ES7210`, `HAS_MIPI_PANEL`, etc.) are unrelated to this and stay as
`build_flags`, unchanged. Board macro names are deliberately short (`WS_P4_7B`, `WS_P4_5`,
`WS_P4_4B`, `WS_S3_5B`, `WS_S3_4B`, `CYD_P4_1060`, `CYD_S3_3248`, `CYD_S3_8048`) for
readability at `#ifdef` call sites — shorter than the BSP filename's full model name and
deliberately distinct from any struct instance name in the same file (see below for why that
distinction matters).

## The BSP pattern

`components/Fleet_BSP/include/Fleet_BSP.h` declares **seven independent, flat struct types**
— not one struct nested inside another, and not two separately-drifting families (there used
to be a `Fleet_BSP.h` for S3 and `Fleet_BSP_P4.h` for P4; later merged into one file, then
further split into these seven flat types instead of one big nested struct):
`BoardHardware`, `ExpanderConfig`, `DisplayConfig`, `TouchConfig`, `LvglConfig`,
`AudioConfig`, `StorageConfig`. `lcd_init_cmd_t` (needed by `DisplayConfig`'s DSI
init-command field) only really exists on P4 targets — it's normally defined by
`Arduino_ESP32DSIPanel.h`, gated `#if CONFIG_IDF_TARGET_ESP32P4` — so `Fleet_BSP.h` declares
an identical fallback typedef for non-P4 builds rather than making the field itself
conditional.

- **`BoardHardware`** holds device-identity strings (`device_name` — GUI header label,
  unchanged; `MANUFACTURER`; `MODEL` — board model, *not* the LCD driver chip, see
  `DisplayConfig.PANEL_MODEL` for that; `SI_REV` — P4 silicon revision, `"rev1_3"` /
  `"rev3_x"` / `"n/a"` on S3 boards, documentary only for now, nothing branches on it yet),
  the shared I2C bus (`SDA_PIN`, `SCL_PIN`, `I2C_CLOCK_SPEED`), and a few standalone scalars
  not worth their own struct (`BOOT_BUTTON_PIN`, `BAT_ADC`, `I2S_AMP_EN` — the last one lives
  here rather than on `AudioConfig` because it's a plain GPIO enable line, not an I2S signal
  or codec setting). `I2C_CLOCK_SPEED` used to be two separate fields — a bus-level one on
  the old I2C struct (confirmed dead: nothing ever read it) and a touch-controller-specific
  one (the value that actually reached hardware, fed into `bb_captouch`'s init call) —
  collapsed into this one field using the touch-side value, since that was the only one ever
  live.
- **`DisplayConfig`** merges the old separate panel + backlight structs — backlight fields
  are prefixed `BL_` (`BL_PIN`, `BL_ON_LEVEL`, `BL_FREQ`) to stay unambiguous now that they
  sit alongside the panel's own generic-sounding fields. The LCD driver chip name field is
  `PANEL_MODEL`, not `MODEL` — deliberately distinct from `BoardHardware.MODEL` (the board,
  not the chip) since both are visible in the same file.
- Every board declares up to seven per-board `const` instances (skipping any struct it
  doesn't need — e.g. only 2 of 8 boards populate `ExpanderConfig`) and aliases each to a
  fixed lowercase name app code actually uses: `bsp_hw`, `bsp_expander`, `bsp_display`,
  `bsp_touch`, `bsp_lvgl`, `bsp_audio`, `bsp_storage`. App code never references the concrete
  per-board instance name (e.g. `WS_P4_TOUCH_LCD_7B_DISPLAY`), only the `bsp_*` alias —
  `bsp_display.WIDTH`, `bsp_touch.I2C_ADDR`, `bsp_audio.MIC_GAIN_DB`. **The concrete instance
  name must not collide with the board's own short `#define`** (e.g.
  `WS_P4_TOUCH_LCD_7B_HARDWARE`, not bare `WS_P4_7B`) — once that macro is defined, the
  preprocessor rewrites any bare occurrence of its name, including a struct's own
  declaration, to `1`.
- **`components/Fleet_BSP/src/` must exist, even though Fleet_BSP is header-only.** It holds
  nothing but a README. Because Fleet_BSP ships a `library.properties`, PlatformIO's LDF
  builds it with `ArduinoLibBuilder`, whose `include_dir` returns `None` unless `include/`
  *and* `src/` both exist (`platformio/builder/tools/piolib.py`). Without `src/`, only the
  library root lands on the include path and every `#include "bsp_loader.h"` fails - in the
  compiler *and* in VSCode IntelliSense. Git cannot track empty directories, which is why
  the README is what keeps it alive. Do not delete either.

- **C++'s designated initializers require members to be listed in declaration order** — when
  writing a new board header, list each struct's fields in the same order they appear in
  `Fleet_BSP.h` (skipping unset ones is fine; reordering the ones you do set is not, and
  fails to compile with `designator order for field 'X' does not match declaration order`).
- Each board's `BSP_<NAME>.h`:
  1. Defines the short device-identity macro (see Board selection above), then `HAS_X`
     capability flags stay in `platformio.ini`, not here.
  2. Declares the panel init command array next (if any). **This can't move below the struct
     literals** — `.INIT_CMDS_SIZE = sizeof(array)/sizeof(element)` needs the array's
     complete (sized) type at the point it's used; a forward declaration would leave it
     incomplete and fail to compile.
  3. Declares each `const <Type> <BOARD>_<GROUP> = { ... }` and its `inline const <Type>&
     bsp_<alias> = ...;` line, one pair per group the board actually needs.

`bsp_audio.MIC_SELECTED`/`MIC_GAIN_DB`/`AEC_MIC_SELECTED`/`AEC_MIC_GAIN_DB` are raw `uint8_t`,
not the real `es7210_input_mics_t`/`es7210_gain_value_t` enum types — BSP headers
deliberately avoid including `es7210.h` to keep their dependency chain simple. A runtime
check (`checkAudioBspSanity()` in `AudioManager.cpp`, gated by `HAS_ES7210`) logs a warning
if these values ever drift from the real enum values it's meant to mirror.

## Audio (`components/AudioManager/`)

Modeled after Espressif's `audio_hal` driver style (`es7210.cpp`/`es8311.cpp` are adapted
from Espressif reference driver code, not original) — deliberately chosen so the codebase
reads closer to official ESP-IDF audio examples.

- `ENABLE_AEC` in `AudioManager.h` is **off by default**. The 4-channel TDM/AEC (acoustic
  echo cancellation) code path exists but is unfinished and untested on real hardware —
  AEC development is on hold, not abandoned.
- Two mutually-exclusive mic input paths: a dedicated ES7210 ADC chip (`HAS_ES7210`), or —
  on boards without one — the ES8311's own built-in ADC (`initCodecOutput()` sets
  `AUDIO_HAL_CODEC_MODE_BOTH` instead of `DECODE`-only in that case). Never assume ES8311 is
  output-only; check which mode a given board actually needs.
- Boards with neither codec chip (NS4168-only amp boards) get raw I2S output with no codec
  register configuration at all — `I2S_AMP_EN` should be explicitly `-1` on those boards
  ("no direct amp control"), or `AudioManager::begin()` will try to drive whatever GPIO
  number the field defaults to as a phantom amp-enable pin.

## Display / touch pipeline

`DisplayManager::initPanel()` has three mutually-exclusive branches gated by
`HAS_RGB_PANEL` / `HAS_QSPI_PANEL` / `HAS_MIPI_PANEL`.

- **QSPI boards**: wrapped in `Arduino_Canvas`, a software-rotation layer. The raw panel
  driver (e.g. `Arduino_AXS15231B`) is always constructed with `rotation=0`; `Canvas` alone
  gets the real `bsp_display.ROTATION`. This is intentional, not an oversight — QSPI panel driver
  chips generally have no hardware rotation, so `Canvas` transforms coordinates in software
  at every draw call and pushes an already-rotated framebuffer to the raw driver, which
  never needs to know rotation is happening. Setting rotation on both layers would
  double/cancel the effect — exactly one layer should ever own it.
- **MIPI/DSI boards**: `Arduino_DSI_Display` uses the *same* CPU-only per-pixel rotation
  transform internally — confirmed via source, no PPA (ESP32-P4's hardware 2D accelerator)
  involvement despite it being available. Fine so far (no reported performance issues on
  DSI boards), but worth revisiting with true LVGL+PPA rotation if that ever changes.
- `TouchManager::mapCoordinates()` has a per-board special case (`#ifndef WS_P4_7B`): most
  boards get a Guition-style rotation transform switch; `WS_P4_7B` instead always does raw
  coordinate passthrough regardless of its own `bsp_display.ROTATION` value. The reasoning behind
  this split is undocumented and was never actually re-tested against the alternative —
  treat it as unverified, not settled.

- **`DisplayConfig.PHY_CLK_SRC` selects the DSI PHY PLL reference clock per board.** It
  holds a small Fleet-defined `BSP_PHY_CLK_SRC_*` code, deliberately *not* a raw
  `mipi_dsi_phy_pllref_clock_source_t` — those are positional ordinals in
  `soc_module_clk_t`, so storing their integers would silently repoint every board if
  Espressif reordered that enum. `Arduino_ESP32DSIPanel` maps the codes to the real
  constants by name. `0` is both the zero-fill default and "change nothing", so a board that
  omits the field keeps the library's historical `PLL_F20M` choice untouched — which is what
  `WS_P4_7B`, `WS_P4_4B` and `CYD_P4_1060` rely on.
- **`pioarduino` ships two prebuilt P4 lib variants and picks one from the board definition.**
  `esp32p4_es` (pre-rev3 silicon: `SELECTS_REV_LESS_V3=y`, `REV_MIN_1`) and `esp32p4`
  (`REV_MIN_301`). `board = esp32-p4-evboard` selects **`esp32p4_es`**, so the fleet is already
  built for pre-rev3 P4 silicon. When checking framework config, confirm which variant is on
  the include path before reading an `sdkconfig` — reading the wrong one wasted most of a
  session. `BoardHardware.SI_REV` stays `"unconfirmed"` on every P4 board and should: silicon
  revision is a per-chip property, not a per-board-model one, so a BSP header cannot represent
  it correctly. See `docs/BRINGUP_WS_P4_TOUCH_LCD_5.md`.
- **Panel reset polarity is per-board and is not currently modelled in the BSP.**
  `Arduino_DSI_Display::begin()` hardcodes an active-LOW reset sequence. The Waveshare P4-5's
  HX8394 declares reset **active HIGH**, so that sequence leaves it held in reset. Suspected
  cause of the open `WS_P4_5` display hang; the fix is a `DisplayConfig` field defaulting to
  active-low so existing boards are unaffected. See `docs/BRINGUP_WS_P4_TOUCH_LCD_5.md`.

## PlatformIO build cache can silently ignore BSP header edits

`platformio.ini` sets `build_cache_dir = .pio/build_cache` — a content-addressed compiler
cache, separate from the per-environment `.pio/build/<env>/` folder. **`pio run -t clean`
does not clear it.** `bsp_loader.h` includes the active board header via `#include
BSP_HEADER`, a macro-indirected include (`BSP_HEADER` expands to a quoted filename, set via
`-D BSP_HEADER=...` in `build_flags`) — real compilers handle this fine, but if this cache's
dependency scanner doesn't correctly resolve it, files that only depend on the BSP header
through that indirection can keep serving a stale cached object indefinitely, even after a
full clean, silently ignoring BSP field edits (confirmed happening with a `bsp_display.ROTATION`
change during this session's recovery pass). Files edited directly still rebuild correctly,
since their own content hash changes regardless of the BSP-dependency issue — it's *only*
files that depend on a BSP value change through the header alone that can go stale. If a
BSP struct field change doesn't seem to take effect no matter what: `rm -rf
.pio/build_cache` before assuming the bug is in your code.

## Debug flag convention

Diagnostic `Serial.print`s worth keeping (not deleting) are gated behind a `#ifdef
DEBUG_<AREA>` (e.g. `DEBUG_TOUCH` in `TouchManager.cpp`, `DEBUG_DISPLAY` in
`DisplayManager.cpp`), enabled per-build by adding `-D DEBUG_<AREA>` to a specific
environment's `build_flags` in `platformio.ini` when actively debugging that area, left out
otherwise. Prefer this over deleting a diagnostic once its immediate investigation is done —
add a new `DEBUG_<AREA>` flag for future debugging needs rather than ad-hoc uncommenting.

## Working conventions established this session

- BSP header layout: device-identity `#define` → panel init array (forced position, see
  above) → the seven `const <Type> <BOARD>_<GROUP>` / `inline const <Type>& bsp_<alias>`
  pairs, skipping any group the board doesn't need.
- Prefer runtime checks over `static_assert` for validating BSP struct field values — the
  struct instances are declared `const`, not `constexpr`, so they aren't usable in constant
  expressions as-is (confirmed via compiler error, not assumption).
## Vendored code and submodules

As of Phase 0 (2026-09-03) there are exactly **two submodules**, both mapped in `.gitmodules`
(which previously did not exist at all — five gitlinks were recorded with no URL mapping, so a
fresh clone produced empty directories and `git submodule status` failed outright):

- **`components/lvgl`** — stock upstream, no local modifications. Pinned to a release tag.
- **`components/GFX_Library_for_Arduino`** — fork `psyfiend/Arduino_GFX`, based on the
  `1.6.0-Waveshare` tag plus local commits (RGB bounce-buffer fix, two `srcFilter` exclusions
  for files broken against arduino-esp32 3.3.11). An `upstream` remote pointing at
  `moononournation/Arduino_GFX` is configured so newer releases can be pulled — **preserve this
  fork's exact structure**; it's the library most likely to need upstream updates for future
  boards, and pulling them is the whole reason it stays a submodule.

**`components/bb_captouch_fork` is no longer a submodule** — it was vendored into this repo in
Phase 0. Its 11 files are tracked here directly, so modifications are versioned in this repo's
own history and can't be lost the way the earlier revert was (an extensive set of local
modifications was once reverted to stock and proved unrecoverable — fresh re-clone, no
stash/branch survived). Its pre-vendoring history remains published at `psyfiend/bb_captouch`
(HEAD was `586cf77`). Edit these files directly and normally; the old "prefer the smallest
possible fix, don't edit the vendored file" caution no longer applies, though the
`build_flags`-scoped `-D FUTURE` trick for the AXS15231 path is still in use and still correct.

## `platformio.ini` hardcodes this machine's project path — don't move the folder

Every local library is declared as `NAME =symlink://c:/Users/Marge/Documents/PlatformIO/Projects/ESP32 Voice Assistant Fleet/components/NAME` — 26 of them. `symlink://` (as opposed to `file://`, which **copies**) is a deliberate choice: it links the library in place so all 8 environments share one copy instead of each getting its own, which matters most for LVGL.

**Consequence: renaming or moving the project folder breaks all 8 environments at once**, and so does restoring a backup to a different path. The fix is a search-and-replace of that prefix across `platformio.ini` — 30 seconds once you know, mystifying if you don't.

The absolute path is load-bearing and **four alternatives were tested and all failed** (2026-09-03, each with a cold `.pio` in a scratch clone):

| Attempt | Result |
|---|---|
| `symlink://components/NAME` (relative) | fails — `bsp_loader.h` not found |
| `lib_extra_dirs = components` + bare names | fails — same |
| ...plus explicit `-I components/Fleet_BSP/include` | gets further, then fails on `Arduino_GFX_Library.h` |
| `symlink://${platformio.src_dir}/../components/NAME` | fails — links are created (no copies) but include dirs still unregistered |
| **literal absolute path** | **works** |

Root cause of the first two: `Fleet_BSP` is header-only with its headers under `include/` and no `library.json`, so PlatformIO's LDF never registers its include directory unless the package was installed via a literal absolute symlink path. Don't "clean up" these paths without re-running that matrix.

Path-independence is deliberately deferred as a *distribution* concern, not a development one: at public-release time the answer is to ship a `platformio.ini` whose `lib_deps` pull stock libraries (LVGL, SensorLib, XPowersLib) from the PlatformIO registry, leaving only original and modified components in-repo. Tracked as a GitHub issue.
