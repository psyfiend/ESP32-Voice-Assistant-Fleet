# Display stack migration — research (esp_lcd, PPA, ESP32_Display_Panel)

**Status: research only. No code outside this file was changed.** Written to answer whether and
how to move off Arduino_GFX, and what a hardware-accelerated rotation path would cost. Nothing
here is scheduled — `FUTURE_IMPROVEMENTS.md` already sequences PPA work *after* milestone 2.3
(the memory budget spike), and that sequencing stands. This document exists so that when 2.3 is
done, the next decision is informed rather than re-researched from zero.

## How to read this document

Every claim below is tagged:
- **Verified** — read directly from a file on this machine; the path is given so you can check it
  yourself.
- **Inferred** — a reasonable conclusion from verified facts, but not itself directly observed
  (e.g., "this would probably take about this much code" is inferred; "this header exists at this
  path" is verified).
- **Unknown** — I looked and could not settle it; see the last section.

This project has been burned before by confident claims that turned out untested
(`docs/LESSONS.md`), so the goal here is to keep those two categories visibly separate rather than
write one confident-sounding narrative.

---

## The one-paragraph answer

The headers and prebuilt libraries for `esp_lcd` (including the MIPI-DSI and RGB panel drivers)
and for the PPA hardware accelerator are **already sitting in this project's toolchain, already on
the default include and link path, for every one of the 8 build environments** — this cost nothing
to discover and there is no framework upgrade or new dependency needed to start using them.
LVGL 9.5 (the version already vendored in `components/lvgl`) even ships its own PPA-accelerated
draw unit, currently switched off. But **none of the three off-the-shelf paths (raw `esp_lcd`,
`ESP32_Display_Panel`, or LVGL's built-in PPA draw unit) solves the specific rotation-stutter
problem on its own** — each solves a related but different problem, and the piece that actually
rotates a whole framebuffer in hardware would have to be hand-written either way, following the
same approach Allsky already demonstrates. The realistic incremental step is small, board-scoped,
and does not touch Arduino_GFX at all for the other seven boards.

---

## Q1 — What would replacing Arduino_GFX with `esp_lcd` cost?

**Verified: the headers and static libraries are already present and already linked by default.**

Checked under `C:\Users\Marge\.platformio\packages\framework-arduinoespressif32-libs\`:

| Variant | `esp_lcd` headers | `esp_lcd_mipi_dsi.h` | `esp_lcd_panel_rgb.h` | `driver/ppa.h` | Prebuilt archive |
|---|---|---|---|---|---|
| `esp32p4_es` (ours, pre-rev3 P4) | `include/esp_lcd/` | `include/esp_lcd/dsi/include/` | `include/esp_lcd/rgb/include/` | `include/esp_driver_ppa/include/driver/ppa.h` | `lib/libesp_lcd.a`, `lib/libesp_driver_ppa.a` |
| `esp32p4` (rev3+ P4, not used by this fleet) | same | same | same | same | same |
| `esp32s3` | `include/esp_lcd/` | — (S3 has no DSI peripheral) | `include/esp_lcd/rgb/include/` (present, but S3 also has no RGB peripheral — see note) | header present, **but `SOC_PPA_SUPPORTED` undefined and driver not linked** | `lib/libesp_lcd.a` |

Note on the S3 row: the header for `esp_lcd_panel_rgb.h` exists in the S3 package tree (it is
part of the shared `esp_lcd` component source), but the ESP32-S3 chip itself does not have an RGB
LCD peripheral in hardware — this project's RGB-panel boards (`WS_S3_4B`, `WS_S3_5B`,
`CYD_S3_8048`) are S3 boards driving RGB panels over the S3's parallel/RGB-capable GPIO matrix via
the same `esp_lcd_rgb_panel` component that P4 also uses; this is confirmed by
`Arduino_ESP32RGBPanel.cpp` (below) targeting the same `esp_lcd_rgb_panel_config_t` API on both
chip families. **Correction, checked independently 2026-09-10:** `driver/ppa.h` *is* present in the `esp32s3`
package (and in `esp32c3`, `c5`, `c6`, `h2`) — ESP-IDF ships that header for every target
regardless of silicon. The conclusion is still right, but the evidence is different and it is
worth having the real one on record:

| Check | `esp32s3` | `esp32p4_es` |
|---|---|---|
| `SOC_PPA_SUPPORTED` in `soc_caps.h` | **not defined** | `1` |
| `-lesp_driver_ppa` in `flags/ld_libs` | **absent** | **present** |

So PPA is a P4-only *hardware block* gated by `SOC_PPA_SUPPORTED`, and only the P4 variants link
the driver. Header presence proves nothing on its own — a trap worth remembering, since
`#include <driver/ppa.h>` would compile happily on an S3 and then fail at link or at runtime. This directly confirms `FUTURE_IMPROVEMENTS.md`'s framing: **PPA rotation
is only ever possible on the four P4 boards, never on the four S3 boards.**

**Verified: the linker already pulls these libraries in by default, with no `platformio.ini`
change.** Each variant's `flags/ld_libs` file (a one-line file of linker flags used for every
build in that variant) already contains `-lesp_lcd` and, for the P4 variants, `-lesp_driver_ppa`.
Each variant's `flags/includes` file already contains `esp_lcd/dsi/include`, `esp_lcd/rgb/include`,
`esp_lcd/include`, `esp_lcd/interface`, and `esp_driver_ppa/include`. Static linking only pulls in
object files that are actually referenced, so this costs the seven boards that never call these
APIs exactly nothing in flash size — the archives are already being linked against for other
symbols arduino-esp32 itself needs.

**Verified: arduino-esp32 3.3.11 (this project's pin) is built on ESP-IDF v5.5.5**
(`esp32p4_es/versions.txt`). Both `esp_lcd_mipi_dsi` and `esp_driver_ppa` are mature, stable
components by that IDF version — this is not bleeding-edge or experimental territory.

**What "replacing Arduino_GFX" concretely means, and why it's not starting from zero.**
Arduino_GFX is not an *alternative* to `esp_lcd` — for the RGB and DSI panel paths, it is already
a thin wrapper *around* `esp_lcd`. Verified in `components/GFX_Library_for_Arduino/src/`:

- `databus/Arduino_ESP32RGBPanel.cpp` builds an `esp_lcd_rgb_panel_config_t` and calls
  `esp_lcd_new_rgb_panel()` / `esp_lcd_rgb_panel_get_frame_buffer()` directly.
- `display/Arduino_DSI_Display.cpp` (via `Arduino_ESP32DSIPanel`) drives the DSI panel through
  `esp_lcd_dsi_bus`/`esp_lcd_mipi_dsi` calls the same way.

So "replace Arduino_GFX with `esp_lcd`" for the RGB/DSI paths is really "delete the GFX-shaped
wrapper and its per-pixel rotation switch, and call the same underlying `esp_lcd` setup functions
directly, feeding an LVGL display driver instead of a GFX canvas." That is a real rewrite of
`DisplayManager::initPanel()`'s RGB/DSI branches and `LVGL_Startup::disp_flush()`, but every symbol
it would call already exists, is already linked, and the pattern to copy from is already sitting
in this project's own vendored fork.

**The QSPI/AXS15231B path is different and harder.** `esp_lcd` ships a *generic* SPI/QSPI panel
I/O layer (`esp_lcd_panel_io_spi.h`, confirmed present under `esp32s3/include/esp_lcd/include/`),
but it does **not** ship an AXS15231B-specific vendor driver in the prebuilt arduino-esp32 archive
— AXS15231B support only exists today in vendor libraries (see Q2). Building `CYD_S3_3248`
directly on raw `esp_lcd` would mean either writing the AXS15231B init-command sequence and vendor
glue by hand (comparable effort to what already exists for the QSPI panel and touch controller in
this project) or importing that one driver file from `ESP32_Display_Panel` (Apache-2.0 — see Q2,
legally reusable).

**Bottom line for Q1:** the *toolchain* cost is zero — nothing to install, no version bump, no new
`lib_deps` line. The *engineering* cost is a real rewrite of `DisplayManager` and the flush
callback, board-family by board-family, and it is not the same size for all three panel families
(RGB/DSI are "delete a wrapper," QSPI is "write or import a real driver").

---

## Q2 — `ESP32_Display_Panel` vs raw `esp_lcd`

Read from `reference/Waveshare Official Repos/WaveShare-S3-Touch-LCD-5B/Arduino/libraries/ESP32_Display_Panel/`.

**Verified: it is Espressif's own library** (`espresso/esp-arduino-libs/ESP32_Display_Panel`,
`library.properties` names `author=espressif`), dual-packaged for both Arduino
(`library.properties`) and ESP-IDF (`CMakeLists.txt` + `idf_component.yml`) use. **Verified
license: Apache License 2.0** (`license.txt`) — permissive, reusable with attribution. This is a
different, better licensing situation than the reference-project table in
`docs/REFERENCE_PROJECTS.md`, which covers unrelated hobbyist repos; this one is safe to actually
copy source from, not just read for approach.

**Panel/touch chip coverage — checked against every board's `PANEL_MODEL` field in
`components/Fleet_BSP/include/BSP_*.h`:**

| Board | `PANEL_MODEL` (BSP, verified) | Driver present in `ESP32_Display_Panel/src/drivers/lcd/`? |
|---|---|---|
| `WS_P4_7B` | `EK79007` | **Yes** (`esp_panel_lcd_ek79007.cpp`) |
| `WS_P4_4B` | `ST7703` | **Yes** (`esp_panel_lcd_...` — full ST7703 driver present) |
| `WS_P4_5` | `HX8394` | **No.** Searched the entire library tree case-insensitively for "hx8394" — zero matches. The library has `HX8399` (a different, similarly-named chip), not `HX8394`. |
| `CYD_P4_1060` | `JD9165` | **Yes** (`esp_panel_lcd_jd9165.cpp`) — note the library separately also has `JD9365`, a different chip; don't confuse the two when picking a driver. |
| `WS_S3_4B` | `ST7701` | **Yes** |
| `CYD_S3_3248` | `AXS15231B` | **Yes**, LCD *and* touch (`esp_panel_lcd_axs15231b.cpp`, `esp_panel_touch_axs15231b.cpp`) |
| `WS_S3_5B` | `ST7262` | **Yes** |
| `CYD_S3_8048` | `ST7262` | **Yes** (same driver as `WS_S3_5B`) |

**This means 7 of 8 boards' exact panel chip is already supported by Espressif's own library —
but `WS_P4_5` is not.** That is precisely the board that cost most of a session to bring up
(`docs/BRINGUP_WS_P4_TOUCH_LCD_5.md`, the HX8394 active-high-reset bug) — the one panel chip this
project fought hardest for is also the one this library doesn't know about. Adopting
`ESP32_Display_Panel` fleet-wide would require writing an HX8394 driver for it (using its
`esp_panel_lcd_st7703.cpp` or similar as a template — the interface is fairly uniform across their
drivers) before `WS_P4_5` could move.

**Touch coverage:** GT911 (used by most of the fleet) is present
(`esp_panel_touch_gt911.cpp`/`.hpp`); AXS15231B touch is present. `bb_captouch_fork`'s device list
was not cross-checked chip-by-chip beyond these two, since GT911 and AXS15231B cover the touch
controllers actually named in the BSP headers I read.

**Is it Arduino-usable as-is? Verified: yes, with two more libraries alongside it.**
`library.properties` declares dependencies `ESP32_IO_Expander (>=1.0.0 && <2.0.0)` and
`esp-lib-utils (>=0.1.0 && <0.2.0)`. Both are **already vendored in the same reference tree**,
sitting as sibling folders: `reference/Waveshare Official Repos/WaveShare-S3-Touch-LCD-5B/Arduino/libraries/ESP32_IO_Expander`
and `.../esp-lib-utils`. So all three pieces needed to compile it as a plain Arduino library are
already on disk, and could be vendored into `components/` the same way `GFX_Library_for_Arduino`
and `bb_captouch_fork` already are — no external download needed to try it. It also supports a
"custom board" configuration path (`src/board/custom/esp_panel_board_config_custom.h`), meaning it
does not require our exact board model to be in its named-board list; a board can be described
pin-by-pin, matching how this project's own BSP headers already work.

**Bottom line for Q2:** `ESP32_Display_Panel` is a real, legally-reusable, largely-complete
alternative to Arduino_GFX for RGB, QSPI/AXS15231B, and most MIPI-DSI boards — but it has the exact
same gap as raw `esp_lcd` for QSPI (AXS15231B is supported, so this is actually *better* than raw
`esp_lcd` there) and a **specific, confirmed hole at HX8394** that would need to be filled before
`WS_P4_5` could use it. It would not, by itself, give hardware rotation — see Q3.

---

## Q3 — PPA

### What LVGL 9.5 itself ships (verified, and this was a real find)

`components/lvgl/src/draw/espressif/ppa/` contains a complete PPA draw unit:
`lv_draw_ppa.c/.h`, `lv_draw_ppa_buf.c`, `lv_draw_ppa_fill.c`, `lv_draw_ppa_img.c`,
`lv_draw_ppa_private.h`. It is gated by `LV_USE_PPA`, which defaults to `0`
(`components/lvgl/src/lv_conf_internal.h:1158`), and **both** of this project's `lv_conf.h`
copies (`components/lv_conf.h` and `include/lvgl/lv_conf.h` — see the caveat below) explicitly set
`LV_USE_PPA 0`. It is currently completely inert.

**What it does if turned on:** it registers an LVGL "draw unit" (LVGL 9's internal term for a
render backend competing for draw tasks) that accelerates plain rectangle fills via
`ppa_do_fill`, and — only if the separate `LV_USE_PPA_IMG` sub-flag is also on — accelerates
image blends via `ppa_do_blend`. Both are useful, general LVGL rendering speedups on P4 hardware
with basically no code to write (it's already there; you flip two `#define`s).

**What it explicitly does NOT do, verified by reading the code (`lv_draw_ppa.c:109-137`): it
refuses to accelerate anything with `rotation != 0`.** The evaluate function that decides whether
an image draw is eligible for the PPA path checks `dsc->rotation == 0` and falls back to normal
software drawing otherwise. **This means LVGL's own built-in PPA support cannot be the fix for the
whole-panel rotation stutter** — it accelerates fills and un-rotated image blends inside the normal
LVGL render pipeline; it says nothing about rotating an entire framebuffer to match a
non-native mounting orientation, which is a display-orientation concern, not a widget-rendering
concern. These are two different problems that happen to share the word "rotation" and the same
hardware unit.

**Caveat worth fixing regardless of any PPA decision:** there are two `lv_conf.h` files in this
tree (`components/lv_conf.h`, header-commented "Configuration file for v9.4.0" — stale — and
`include/lvgl/lv_conf.h`). Both currently agree `LV_USE_PPA 0`, so this isn't an active bug, but
if `LV_USE_PPA` is ever flipped on, whoever does it needs to know which of the two files the
build actually picks up (`platformio.ini`'s `S3-options`/`P4-options` pass both
`-I components/lvgl` and `-I include/lvgl`), or the edit may land in the file that loses.
**I did not resolve which one wins** — see "what I could not determine."

### What the Allsky project's approach requires (read for approach only — repo is unlicensed, nothing copied)

`reference/Examples and related projects/ESP32-P4-Allsky-Display/ppa_accelerator.h/.cpp` and
`docs/developer/architecture.md` describe a working pattern for driving the raw PPA client API
(`driver/ppa.h`) directly, for scaling and rotating whole image buffers (not LVGL draw
primitives). The requirements it documents, and that I independently confirmed are correct
`esp_cache`/PPA API usage by reading the actual function calls in that file (`ppa_register_client`,
`ppa_do_scale_rotate_mirror`, `esp_cache_msync`):

- **Buffers must be 64-byte aligned** and allocated with `MALLOC_CAP_DMA | MALLOC_CAP_SPIRAM`
  (`heap_caps_aligned_alloc(64, size, MALLOC_CAP_DMA | MALLOC_CAP_SPIRAM)`, not plain `malloc()` or
  `ps_malloc()`, which don't guarantee alignment). A misaligned buffer handed to the PPA's DMA
  engine faults.
- **Explicit cache management around every PPA operation**, because the PPA reads/writes PSRAM
  directly via DMA, bypassing the CPU's data cache: `esp_cache_msync(..., ESP_CACHE_MSYNC_FLAG_DIR_C2M)`
  before the PPA reads a buffer the CPU just wrote (flush CPU cache to memory), and
  `esp_cache_msync(..., ESP_CACHE_MSYNC_FLAG_DIR_M2C)` after the PPA writes a buffer the CPU is
  about to read (invalidate CPU cache so it re-reads from memory). Get the direction wrong and you
  get silently corrupt or stale pixels, not a crash — the failure mode this project has already
  been burned by elsewhere (`docs/LESSONS.md`'s "verify from outside" theme, same shape of bug).
- Hardware angles only: 0°/90°/180°/270° — arbitrary angles aren't supported, which is fine, since
  display-orientation rotation is always one of those four anyway.
- This is exactly the same cache discipline LVGL's own PPA draw unit uses internally
  (`lv_draw_ppa_buf.c` calls `esp_cache_msync(..., ESP_CACHE_MSYNC_FLAG_DIR_C2M | ...)` on its draw
  buffer, and `lv_draw_ppa.c`'s dispatch function calls `lv_draw_buf_invalidate_cache()` after each
  operation) — so this isn't an idiosyncrasy of one hobbyist project, it's simply what correct PPA
  usage on this silicon requires, confirmed in two independent codebases.

**Also confirmed by reading the Allsky code: their own PPA use is for rotating/scaling a
downloaded *image* before it's blitted to the framebuffer — not for rotating the *whole
framebuffer* to correct display orientation.** `docs/REFERENCE_PROJECTS.md` already flags this
caveat. So even Allsky's approach, read purely for technique, would need to be re-purposed rather
than adopted wholesale: the actual fix for our rotation-stutter problem is "call
`ppa_do_scale_rotate_mirror` on the LVGL flush region (or the whole draw buffer) instead of handing
it to `Arduino_DSI_Display`'s CPU rotation switch," which nobody in either reference project has
built. It is a small, well-specified, but genuinely new piece of code.

### Buffer-requirements summary (for whoever eventually writes this)

| Requirement | Value | Source |
|---|---|---|
| Alignment | 64 bytes | ESP32-P4 DMA controller requirement; confirmed in both Allsky's code and LVGL's own PPA buffer handling |
| Allocator | `heap_caps_aligned_alloc()`, not `malloc()`/`ps_malloc()` | Same — plain allocators don't guarantee alignment |
| Capability flags | `MALLOC_CAP_DMA \| MALLOC_CAP_SPIRAM` | Same |
| Cache sync before hardware reads a CPU-written buffer | `esp_cache_msync(ptr, size, ESP_CACHE_MSYNC_FLAG_DIR_C2M)` | ESP-IDF `esp_cache` API, used identically in both codebases read |
| Cache sync after hardware writes a buffer the CPU will read | `esp_cache_msync(ptr, size, ESP_CACHE_MSYNC_FLAG_DIR_M2C)` | Same |
| Supported rotation angles | 0°/90°/180°/270° only | PPA hardware limitation, documented in Allsky's architecture doc |
| Color format | RGB565 (matches this project's display buffers already) | Both codebases; also matches `LVGL_Startup.cpp`'s existing `uint16_t` draw buffers |

---

## Q4 — What can be done right now, incrementally

Ranked by (value delivered) / (risk of breaking the 8 working boards), highest first:

### 1. (Highest value/risk ratio) — Nothing, yet, on this topic specifically
`FUTURE_IMPROVEMENTS.md` already places PPA work after milestone 2.3 (the memory-budget spike),
for a documented reason: 2.3 decides render-path buffer sizes and locations, and PPA has hard
buffer placement requirements. Building a PPA path now means possibly rebuilding it once 2.3
lands. This isn't a cop-out — it's the actual highest-value move, because it avoids doing the
PPA work twice. This document exists so that when 2.3 finishes, the next step doesn't require
re-deriving any of the above.

### 2. When ready: PPA rotation behind the existing `DisplayManager` interface, on `WS_P4_5` only, additive
This is the real answer to "smallest first step that delivers real value."

**Why this is low-risk:** `DisplayManager`'s public surface (verified,
`components/DisplayManager/DisplayManager.h`) is already narrow — `getGfx()` returns the
`Arduino_GFX*`, and `setBrightness`/`getBrightness`/`setBacklight` are the only other public
calls anything outside `DisplayManager` and `LVGL_Startup` uses (confirmed by grep: only
`LVGL_Startup.cpp`, `SystemCore.cpp`, and `DisplayManager.cpp`/`.h` itself reference `Arduino_GFX`
or `getGfx()` anywhere in `src/`, `include/`, or the two HAL components — `Panel_Display.cpp`
touches only brightness, never the GFX pointer). A PPA rotation stage can be inserted **only**
inside `LVGL_Startup::disp_flush()` (or as a new LVGL PPA-based draw unit, following the pattern
LVGL's own `lv_draw_ppa.c` already establishes), gated behind
`#if defined(WS_P4_5) && defined(USE_PPA_ROTATION)` or similar. It changes nothing about
`DisplayManager`, nothing about the BSP struct, nothing about any other board's build. If it goes
wrong, one board regresses, not eight.

**Why `WS_P4_5` specifically:** it's the board the owner has already reported visible stutter on
in non-native orientation (`FUTURE_IMPROVEMENTS.md`), it's MIPI-DSI (full-frame PSRAM buffers
already, per `LVGL_Startup.cpp`'s `HAS_MIPI_PANEL` branch — no internal-SRAM pressure to fight,
unlike `CYD_S3_3248`), and it's a P4 board (PPA exists at all only on P4 silicon).

**Concrete shape of the work** (not yet designed in detail — this is the scope, not a spec):
1. Allocate one extra PSRAM scratch buffer per LVGL draw buffer, 64-byte aligned,
   `MALLOC_CAP_DMA | MALLOC_CAP_SPIRAM`, sized to the panel's full frame (same size class
   `LVGL_Startup.cpp` already allocates for the MIPI/RGB "Case A" path).
2. In `disp_flush()`, when the board is `WS_P4_5` and PPA is enabled: instead of calling
   `gfx->draw16bitRGBBitmap()` (which triggers `Arduino_DSI_Display`'s CPU rotation switch),
   call `ppa_do_scale_rotate_mirror()` with the LVGL-rendered chunk as source and the DSI panel's
   framebuffer region as destination, angle taken from `bsp_display.ROTATION`, bracketed by the
   two `esp_cache_msync()` calls from the table above.
3. Everything else in `LVGL_Startup.cpp` — buffer allocation strategy, the `is_last`-triggered
   `gfx->flush()`, touch handling — stays exactly as-is; `Arduino_GFX` still owns the actual panel
   handle and DSI bus setup, only the rotation transform moves from GFX's CPU path to the PPA.
   This is the "additive, not big-bang" property the owner asked for.

### 3. One board fully on `esp_lcd`, seven stay on Arduino_GFX — feasible, but higher cost than #2 for the same immediate problem
Verified feasible: PlatformIO's `build_src_filter` and `#ifdef BOARDNAME` mechanisms already used
throughout this codebase (e.g. `WS_S3_TOUCH_LCD_5B`'s `-<Panel_Audio.cpp>` exclusion) would let
one environment build a different `DisplayManager::initPanel()` branch that skips Arduino_GFX
entirely. `WS_P4_5` is again the natural candidate (worst rotation symptom, and its `esp_lcd`-only
HX8394 driver already exists and works — it's literally what the *factory* firmware uses, per
`docs/BRINGUP_WS_P4_TOUCH_LCD_5.md`'s note that IDF's `esp_lcd_hx8394` driver honors the reset
polarity that `Arduino_DSI_Display` originally got wrong). But this replaces the *whole* display
init path, not just the rotation stage — much larger surface to get wrong, and it still doesn't
get you PPA rotation for free; you'd write both the `esp_lcd` panel setup *and* the PPA rotation
stage together. **Recommendation: do #2 first.** If it proves out, #3 becomes a natural follow-on
once there's a working PPA flush path to reuse, rather than a prerequisite for it.

### 4. Adopt `ESP32_Display_Panel` fleet-wide
Highest total value (real double-buffering fix for `WS_S3_5B`'s tearing, since the library's own
RGB driver would need to be checked for the same `num_fbs`-index bug — **not yet checked**, see
below — plus real per-chip drivers instead of a hand-rolled fork) but also highest risk and cost:
touches all 8 environments' display init, needs an HX8394 driver written first for `WS_P4_5`, and
is a wholesale swap rather than an incremental change. Not recommended as a first step; worth
revisiting only after #2 and #3 have been tried on one board each and the team has real experience
with `esp_lcd`-level code in this project.

### On the `Arduino_ESP32RGBPanel` tearing bug specifically (separate from PPA)
Verified independently of the PPA question: `Arduino_ESP32RGBPanel.cpp` line 74 requests
`.num_fbs = 2`, but `getFrameBuffer()` (line 161) calls
`esp_lcd_rgb_panel_get_frame_buffer(_panel_handle, 1, &frame_buffer)` — hardcoded index `1`,
always the same buffer, confirming `FUTURE_IMPROVEMENTS.md`'s claim exactly. This is unrelated to
PPA and would need its own fix (real buffer-swap logic, matching Waveshare's own
`switchFrameBufferTo()`) regardless of which display stack is chosen — GitHub issue #40 already
tracks it.

---

## Q5 — What breaks, board by board and system by system

Being specific rather than hand-wavy, as instructed:

**`bb_captouch_fork` touch — mostly unaffected.** `TouchManager` talks to `bb_captouch`
independently of `Arduino_GFX`; verified `TouchManager.cpp` never calls into `DisplayManager`'s
`getGfx()` or any Arduino_GFX type. The one coupling point is `TouchManager::mapCoordinates()`
reading `bsp_display.ROTATION` and `bsp_display.WIDTH/HEIGHT` (`components/TouchManager/TouchManager.cpp:148-212`)
to un-rotate raw touch coordinates to match whatever rotation the display layer applied. **If a
board's display rotation moves from a CPU transform to a PPA transform, touch mapping doesn't
care how the pixels got rotated — it only needs to agree with the display on the final rotated
coordinate space.** So this is a "make sure both sides still agree" risk, not a "this breaks"
risk, as long as `bsp_display.ROTATION` continues to mean the same thing to both subsystems.
`CLAUDE.md`'s existing note that `WS_P4_7B`'s `#ifndef WS_P4_7B` raw-passthrough special case is
unverified/unretested is unrelated to this migration but sits in the same function — worth not
tangling the two together in one change.

**LVGL flush callback shape — this is the actual seam, and it's already narrow.** Verified
`LVGL_Startup.cpp`'s `disp_flush()` is the only place that calls `gfx->draw16bitRGBBitmap()` /
`gfx->flush()`. A PPA path replaces the body of this one function (or adds a board-gated branch
inside it); the function signature LVGL requires (`lv_display_flush_cb_t`) doesn't change, and
`lv_display_set_buffers(..., LV_DISPLAY_RENDER_MODE_PARTIAL)` doesn't need to change either — PPA
scale/rotate operates per-chunk exactly like the current per-chunk `draw16bitRGBBitmap()` call
does.

**BSP struct fields that describe Arduino_GFX-specific things — real, and would need new fields,
not replaced ones.** Verified in `Fleet_BSP.h`: `DisplayConfig.INIT_CMDS_RGB` and
`INIT_CMDS_DSI` are typed for Arduino_GFX's two different init-command encodings
(`lcd_init_cmd_t` is explicitly a fallback typedef Arduino_GFX's own header would otherwise
provide). A raw `esp_lcd` or `ESP32_Display_Panel` path would want panel init sequences in
whatever shape *those* APIs expect, which is not guaranteed to be the same shape. This is a real,
if narrow, per-field migration cost on any board that switches — not a blocker, since these two
fields are additive (a board that doesn't populate them just leaves them zeroed, per the existing
convention), but every board that moves off Arduino_GFX needs its init table re-expressed.

**`Arduino_Canvas` rotation (QSPI boards) — confirmed same CPU-per-pixel pattern as DSI.**
Verified `components/GFX_Library_for_Arduino/src/canvas/Arduino_Canvas.cpp` has the identical
`switch(_rotation) { case 1: ... case 2: ... case 3: ... }` structure as `Arduino_DSI_Display.cpp`.
PPA doesn't help this board at all (`CYD_S3_3248` is an S3, no PPA silicon), so this pain point
has no hardware-acceleration fix available under this fleet's current hardware — the QSPI
rotation-cost question is separate from the PPA question entirely, and any fix there would be
algorithmic (e.g. only rotating the changed region) rather than hardware-offloaded.

**Per-board init command arrays — same shape concern as the BSP fields above.** Each
`BSP_<NAME>.h`'s `INIT_CMDS_DSI`/`INIT_CMDS_RGB` array is written once in Arduino_GFX's expected
format (`CLAUDE.md`'s documented reason `.INIT_CMDS_SIZE` can't move above the struct literals).
Nothing about that ordering constraint changes under a different display stack, but the arrays'
*content type* would need re-authoring per board if that board moves off Arduino_GFX.

**What does NOT break, verified:** `AudioManager`, `TouchManager`'s bb_captouch integration
(beyond the rotation-agreement point above), `FleetI2C`, the entity/MQTT/HA pipeline, and the
`SystemCore`/`SystemReport`/`GUIManager` split from Phase 2.1 are all untouched by any of this —
`SystemCore.cpp` only calls `displayMgr.begin()` and reads `getGfx()` for a null-check-style
success signal (verified: it does not otherwise depend on the concrete display class), and
`GUIManager`/`Panel_*` code never references Arduino_GFX types at all (verified by grep across
`src/` and `include/`). The Phase 2.1 architecture's promise — hardware owned low, borrowed
above — is exactly what keeps this migration's blast radius small.

---

## Recommended path

1. **Do nothing until milestone 2.3 (memory budget spike) lands.** Already the plan; this
   research doesn't change that.
2. **First concrete step, when ready:** build the PPA rotation stage described in Q4 option 2,
   scoped to `WS_P4_5` only, inside `LVGL_Startup::disp_flush()`, gated by a board `#ifdef` and a
   new opt-in build flag (following the existing `DEBUG_<AREA>` convention's spirit — an opt-in
   flag, not a silent behavior change). Arduino_GFX keeps owning the DSI bus and panel handle on
   every board, including `WS_P4_5`; only the per-chunk rotation transform is swapped for one
   board. This is additive, reversible with one `#ifdef` removed, and cannot regress the other
   seven boards because it touches no shared code path.
3. **Only after that works on hardware:** consider whether `WS_P4_5` is worth moving fully onto
   `esp_lcd` (option 3) to shed the Arduino_GFX HX8394 workaround entirely — at that point the
   PPA flush code from step 2 is already proven and gets reused, not rewritten.
4. **`ESP32_Display_Panel` fleet-wide adoption** stays a "someday, not now" — it's real, it's
   legally reusable (Apache 2.0), and it's more complete than this project's own fork in most
   places, but it has its own gap (no HX8394) and touches all 8 boards at once. Revisit after
   steps 2–3 have produced real experience with `esp_lcd`-level code in this codebase.
5. **The RGB double-buffering bug (issue #40) is independent of all of the above** and can be
   fixed on its own schedule without waiting on any PPA decision — it's a bug in the currently-used
   Arduino_GFX fork, not a reason to migrate away from it.

---

## What I could not determine

- **Which of the two `lv_conf.h` files (`components/lv_conf.h` vs `include/lvgl/lv_conf.h`) the
  build actually resolves `#include "lv_conf.h"` to**, given both `-I components/lvgl` and
  `-I include/lvgl` are passed as build flags. Both currently agree `LV_USE_PPA 0`, so this has no
  effect today, but it needs resolving (pick one file, delete or clearly mark the other as unused)
  before anyone edits LVGL config to enable PPA, or the edit may silently land in the file that
  loses the include-path race.
- **Whether `ESP32_Display_Panel`'s own RGB driver has the same `num_fbs`/frame-index bug as this
  project's Arduino_GFX fork.** I confirmed the bug exists in `Arduino_ESP32RGBPanel.cpp`; I did
  not read `ESP32_Display_Panel`'s RGB driver source closely enough to say whether adopting it
  would fix `WS_S3_5B`'s tearing as a side effect or not. `FUTURE_IMPROVEMENTS.md`'s existing
  caution — diff a vendor's known-working copy before theorising, which is what actually found the
  `WS_P4_5` reset-polarity fix — applies equally here: a diff of `ESP32_Display_Panel`'s RGB driver
  against the six vendor Arduino_GFX trees in `reference/Waveshare Official Repos/` would be worth
  doing together, not separately, since both bear on the same `num_fbs` question.
- **Whether `bb_captouch_fork`'s touch controllers beyond GT911 and AXS15231B** (if any other BSP
  uses a different chip I didn't cross-reference by name) are covered by `ESP32_Display_Panel`'s
  touch driver list. I checked every `PANEL_MODEL` (LCD chip) field but did not separately
  enumerate every `TouchConfig.NAME` value against the touch driver list beyond the two chips the
  display-chip cross-reference happened to also need.
- **Real-world PPA rotation performance on this project's actual panels.** Allsky's documented
  numbers (roughly 80ms for a 90° rotation, ~120-200ms for combined scale+rotate on their image
  sizes) are for their specific buffer sizes and image-rotation use case, not for rotating an
  entire LVGL framebuffer at this project's panel resolutions. This would need to be measured on
  real `WS_P4_5` hardware once step 2 above is built — it is plausible but unverified that it beats
  the current CPU rotation by enough to matter, given the CPU rotation cost that produces the
  reported stutter has itself never been profiled (only observed qualitatively as "visible
  stutter").
- **Whether PlatformIO's Arduino registry or an existing `lib_deps` entry could pull
  `ESP32_Display_Panel` + its two dependencies directly (vs. vendoring the copy already sitting in
  `reference/`).** Not checked; not needed for this research, but relevant if it's ever actually
  adopted, since vendoring vs. registry-fetching affects how upstream updates get pulled later
  (the same tradeoff `CLAUDE.md` already documents for `GFX_Library_for_Arduino`).
