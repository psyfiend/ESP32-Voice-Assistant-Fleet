# ESP32 Voice Assistant Fleet — VA Testing

PlatformIO + Arduino-framework firmware for a shared codebase running across multiple
ESP32-S3 and ESP32-P4 boards (WaveShare and Guition displays), each with a touchscreen,
audio codec(s), and LVGL-based UI. One codebase, many boards, selected at build time via
PlatformIO environments.

See `docs/PROJECT_STATUS.md` for current bugs and unconfirmed/untested items, and
`docs/FUTURE_IMPROVEMENTS.md` for deliberately deferred work. Both are more volatile than
this file and worth checking first for "is X already known/planned."

## Board selection

Each PlatformIO environment in `platformio.ini` picks a board via `build_flags`:
`-D BSP_HEADER='"BSP_<Name>.h"'` (tells `bsp_loader.h` which header to include) plus a
`-D <BOARDNAME>` flag (e.g. `-D WS_P4_SMART86`) used throughout the codebase for
`#ifdef <BOARDNAME>`-style device-specific branches.

## The BSP pattern

`components/Fleet_BSP/include/`:
- `Fleet_BSP.h` (S3 family) and `Fleet_BSP_P4.h` (P4 family) each declare the same *shape*
  of two structs — `Fleet_BSP` (display/touch config) and `Fleet_Hardware_Config`
  (audio/SD/misc peripheral pins) — but are two **separate, non-unified** definitions with
  some field drift between them. Deliberately left unmerged for now (see FUTURE_IMPROVEMENTS.md).
- Each board gets its own `BSP_<Name>.h`, which:
  1. Defines `HAS_X` capability flags (`HAS_ES7210`, `HAS_ES8311`, `HAS_RGB_PANEL`,
     `HAS_QSPI_PANEL`, `HAS_MIPI_PANEL`, `HAS_IO_EXPANDER`, `HAS_TOUCH`, etc.) at the top of
     the file, right after the board's own `#define`.
  2. Declares the panel init command array next. **This can't move below `cfg`/`hw_cfg`** —
     `.INIT_CMDS_SIZE = sizeof(array)/sizeof(element)` needs the array's complete (sized)
     type at the point it's used inside the `cfg` struct literal; a forward declaration
     would leave it incomplete and fail to compile.
  3. Declares `const Fleet_BSP <Name>_LCD = { ... }` and aliases it via
     `inline const Fleet_BSP& cfg = <Name>_LCD;` — all app code reads the board's config
     through the generic `cfg`/`hw_cfg` names, never the concrete instance name.
  4. Same pattern for `const Fleet_Hardware_Config <Name>_Hardware` → `inline const
     Fleet_Hardware_Config& hw_cfg`.

`hw_cfg.MIC_SELECTED`/`MIC_GAIN_DB`/`AEC_MIC_SELECTED`/`AEC_MIC_GAIN_DB` are raw `uint8_t`,
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
  gets the real `cfg.ROTATION`. This is intentional, not an oversight — QSPI panel driver
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
  coordinate passthrough regardless of its own `cfg.ROTATION` value. The reasoning behind
  this split is undocumented and was never actually re-tested against the alternative —
  treat it as unverified, not settled.

## PlatformIO build cache can silently ignore BSP header edits

`platformio.ini` sets `build_cache_dir = .pio/build_cache` — a content-addressed compiler
cache, separate from the per-environment `.pio/build/<env>/` folder. **`pio run -t clean`
does not clear it.** `bsp_loader.h` includes the active board header via `#include
BSP_HEADER`, a macro-indirected include (`BSP_HEADER` expands to a quoted filename, set via
`-D BSP_HEADER=...` in `build_flags`) — real compilers handle this fine, but if this cache's
dependency scanner doesn't correctly resolve it, files that only depend on the BSP header
through that indirection can keep serving a stale cached object indefinitely, even after a
full clean, silently ignoring BSP field edits (confirmed happening with a `cfg.ROTATION`
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

- BSP header layout: `#define`s → panel init array (forced position, see above) → `cfg` →
  `hw_cfg`.
- Prefer runtime checks over `static_assert` for validating BSP struct field values — the
  struct instances are declared `const`, not `constexpr`, so they aren't usable in constant
  expressions as-is (confirmed via compiler error, not assumption).
- `components/bb_captouch_fork` is a nested, separate git repo (own remote,
  `psyfiend/bb_captouch`) — **not** a proper git submodule of this parent repo, and the
  parent has zero history for it. Treat modifications there cautiously: an earlier,
  extensive set of local modifications was reverted to stock, and the revert is
  unrecoverable via git (fresh re-clone, no stash/branch survived). When something in that
  library needs to change, prefer the smallest possible fix (e.g. enabling a dead
  `#ifdef FUTURE` code path via a `build_flags`-scoped `-D FUTURE`, rather than editing the
  vendored file) over broad changes.
