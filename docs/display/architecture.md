# Display architecture — how it works now

The esp_lcd path as built (2026-09-29). Status per board: [README.md](README.md). The reasoning
behind each decision, in full: `docs/archive/display/display-stack.md` and `esplcd-step2.md`.

## 1. The pieces

| Piece | File | Owns | LVGL? |
|---|---|---|---|
| Board selection | `include/BoardDisplay.h` | `Fleet_Display` under `-D DISPLAY_ESPLCD`, else `DisplayManager` (Arduino_GFX) | no |
| **Fleet_Display** | `components/Fleet_Display/src/Fleet_Display.{h,cpp}` | Panel bring-up, backlight, touch reset, amp enable, the frame buffers, which buffer the panel is showing, the present mode | no |
| DSI bring-up | `fleet_dsi_panel.{h,c}` | PHY LDO, DSI bus, DBI command IO, DPI panel - one sequence for every chip | no |
| DSI chip wrappers | `fleet_dsi_<chip>.c` | One per vendored driver: its constructor fed our BSP's init list, its name and version | no |
| RGB bring-up | `fleet_rgb_panel.{h,c}`, `fleet_rgb_st7701.c` | RGB panel, bounce buffers; the ST7701's 3-wire SPI init through the expander | no |
| **Flush, DSI** | `src/LVGL_Flush_EspLcd.cpp` | `TRIPLE_PARTIAL`: PPA strip copies/rotation, repair of stale areas, buffer hand-off | yes |
| **Flush, RGB** | `src/LVGL_Flush_EspLcdDirect.cpp` | `DOUBLE_DIRECT`: LVGL draws into the panel's buffers; hand-off and wait | yes |
| Flush, Arduino_GFX | `src/LVGL_Flush_Gfx.cpp` | The old path, for boards not moved yet | yes |
| Common interface | `src/LVGL_Flush.h` | `begin`, `shownFrameBuffer`, `panelFramesScanned`, `softwareRotation`, bench hooks | - |

`SystemCore` owns `Fleet_Display` and brings it up; `LVGL_Startup::begin(display, touch)` borrows it
and installs the flush. `SystemCore` includes no LVGL header, and `Fleet_Display` includes none
either - the flush files are the only bridge (CLAUDE.md, Application layout).

Every source file in `Fleet_Display` is wholly inside `#if defined(DISPLAY_ESPLCD)` or `#if
SOC_MIPI_DSI_SUPPORTED`: PlatformIO's library finder ignores `#ifdef`s when choosing what to
compile, so each file must be empty on boards that do not use it.

## 2. Choosing the panel driver by name

The BSP says `#define BSP_PANEL_DRIVER HX8394` (a bare token). `Fleet_Display.cpp` pastes it into
`fleet_dsi_driver_HX8394` / `fleet_rgb_driver_ST7701`, defined in that chip's wrapper. There is no
list of chips and no if/else anywhere. `.PANEL_MODEL = BSP_STR(BSP_PANEL_DRIVER)` makes the reported
chip the built one.

- **Adding a DSI panel:** vendor its driver, copy a wrapper, change the chip name, types and
  vendor-config fields (they differ per driver), add its line to `fleet_display_versions.h`, set
  `BSP_PANEL_DRIVER`. No edit to `Fleet_Display`.
- **A chip with no wrapper fails to link:** `undefined reference to 'fleet_dsi_driver_<CHIP>'`.
  Wrappers nothing names are dropped by the linker.
- Driver versions live in one place, `fleet_display_versions.h`; the System Doctor prints them.

Vendored drivers (all in `components/Fleet_Display/src/`, each with its `FLEET LOCAL CHANGES` block
and licence file): HX8394 (Waveshare 2.1.0), ST7703 (Waveshare 2.0.0), EK79007, JD9165, ST7701
(Espressif 2.0.2 each), plus `esp_io_expander` + TCA9554 and `esp_lcd_panel_io_3wire_spi` for the
S3_4B. `components/Fleet_Display/README.md` has the per-driver notes.

## 3. Present modes — how frames reach the panel

`bspPresentMode()` (`bsp_loader.h`) derives the mode from the bus, the rotation and the TE pin;
`DisplayConfig.PRESENT_MODE` overrides it on one board (0 = use the rule). The number of frame
buffers follows from the mode (`bspPresentFrameBuffers()`) and is never set on its own. The boot
log and the System Doctor say which mode ran and why.

| Mode | Rule picks it for | FBs | Built | Flush |
|---|---|---|---|---|
| `TRIPLE_PARTIAL` | every DSI panel | 3 | **yes** | `LVGL_Flush_EspLcd.cpp` |
| `DOUBLE_DIRECT` | RGB at rotation 0 | 2 | **yes** | `LVGL_Flush_EspLcdDirect.cpp` |
| `TRIPLE_PARTIAL` | RGB, rotated | 3 | no (no RGB board is rotated) | - |
| `TE_SYNC` | QSPI with a TE pin | 0 | no (step 5) | - |
| `NONE` | QSPI without TE | 0 | no | - |
| `DOUBLE_PARTIAL`, `TRIPLE_FULL` | override only | 2 / 3 | no | - |

Each bus has one flush built (`BUILT_MODE` in `Fleet_Display.cpp`). A requested mode that is not
built is refused in `begin()` with a warning, and the bus's built mode runs, so the board still
lights up.

`DisplayConfig.NUM_FB` is Arduino_GFX-only and goes with it at step 6.

### 3.1 `TRIPLE_PARTIAL` (DSI)

LVGL draws 50-line strips into two draw buffers in PSRAM. For each strip:

1. Write it back from the CPU cache (the PPA reads memory, not cache).
2. **Queue** a PPA job that copies it into the frame buffer being built - rotating it on the way if
   the board is rotated (P4_5 90, 7B 180). LVGL draws the next strip while the PPA works.

On the last strip:

3. **Repair.** Each frame buffer keeps a list of areas it is behind on. Before a buffer is presented,
   everything on its list *minus what this frame just drew* (`lv_area_diff`) is copied in from the
   newest buffer. Overlapping stale areas merge instead of overflowing (16 kept). Repairs run on the
   **DMA2D copier** (`esp_async_fbcpy`, ~3x the PPA's throughput) from one worker task on core 0.
4. **Present.** `esp_lcd_panel_draw_bitmap()` with a buffer the panel owns is a *switch*, not a copy
   (IDF checks the pointer). The panel moves to it at its next refresh.
5. Take the next free buffer - free once `on_frame_buf_complete` reports the panel has stopped
   showing it.

### 3.2 `DOUBLE_DIRECT` (RGB)

LVGL draws straight into the panel's two PSRAM frame buffers (`lv_display_set_buffers(fb0, fb1, ...,
DIRECT)`). No strip buffer, no copy. LVGL itself keeps the off-screen buffer current by copying the
areas the previous frame changed (a CPU copy; the S3 has no PPA or DMA2D). On the last chunk:
write back the cache if there are no bounce buffers, present the finished buffer, and **wait** until
the panel is scanning it (100 ms timeout), because the other buffer is where LVGL draws next and it
is on the glass until then. DIRECT cannot rotate.

## 4. Rules that keep it correct — each one learned

- **At most one DMA2D copier job outstanding in the whole program.** `esp_async_fbcpy()` shares one
  `static` transfer config between every handle; queued jobs run with the last caller's settings.
  Repairs go through one worker, in order, on one handle.
- **Frame-complete callbacks must be `IRAM_ATTR`** and call nothing that is not: IDF refuses to
  register them otherwise (`esp_ptr_in_iram`).
- **Buffers the PPA or cache sync touch are 64-byte aligned** (`LV_DRAW_BUF_ALIGN 64` under the
  flag).
- **The PPA's angles are counter-clockwise**; ours are Arduino_GFX's. The mapping lives in the flush.
- **Panel-side mirroring does not replace the PPA:** EK79007 ignored MADCTL on glass; ST7703 has no
  X mirror; HX8394 has no mirror. Rotation stays in the flush.
- **The ST7701 driver sends MADCTL and COLMOD itself** before the BSP's list; a BSP list must not
  contain 0x36 or 0x3A.
- **Clear `.pio/build_cache` after a BSP edit.** A stale object once reported new timing while the
  panel ran the old one; `/bench`'s measured `panel_scan` rate caught it.

## 5. Bus-specific details

**DSI (P4):** PHY LDO channel and voltage and the lane count come from the BSP where a board sets
them, else LDO 3 / 2500 mV / 2 lanes. `PHY_CLK_SRC` is **not** wired through yet (IDF picks) -
worth doing before a board that needs a specific PHY clock. Reset polarity is per board
(`RST_ACTIVE_HIGH`; the P4_5's HX8394 needs 1).

**RGB (S3):**
- The S3_4B's ST7701 is initialised over 3-wire SPI with every line on the TCA9554 expander:
  Espressif's `esp_io_expander` + `esp_lcd_panel_io_3wire_spi`, sharing `Wire`'s bus through
  arduino-esp32's `i2cBusHandle()` (the `i2c_master` driver - never the legacy one, which cannot be
  linked beside it). `auto_del_panel_io` frees the expander lines after init. Touch reset and the
  amp enable moved from `DisplayManager` into `Fleet_Display`.
- Pixel clock is `DisplayConfig.PREFER_SPEED` (Arduino_GFX's field; `PCLK_HZ` is unused on both
  paths). Bounce buffers from `BOUNCE_BUFFER_SIZE_PX`. The framework already has
  `CONFIG_LCD_RGB_RESTART_IN_VSYNC`.
- The ST7262 boards (8048, S3_5B) have no init commands: plain RGB timing.
- `WS_S3_4B` only: `-D FLEET_LV_MEM_PSRAM` puts LVGL's 128 KB pool in PSRAM (`lv_conf.h`). #70.

**QSPI (`CYD_S3_3248`):** not moved. The vendor never sends a row address over QSPI (writes start
at row 0 or continue), so arbitrary partial windows are unproven; the panel has a TE pin (GPIO 38).
`docs/archive/display/waveshare-esp-lcd-survey.md` §5.

## 6. Decisions that still stand (owner, 2026-09-25)

| | Decision | Why |
|---|---|---|
| D1 | Raw `esp_lcd`, not `ESP32_Display_Panel` | Native IDF; no HX8394 driver there |
| D2 | Measure first, then one board at a time | Every change judged against `/bench` |
| D4 | Per-board `-D DISPLAY_ESPLCD`; Arduino_GFX deleted only when every board has moved | A board moves back by deleting one line |
| D5 | Our own glue, modelled on `esp_lvgl_adapter`, not the adapter itself | The adapter would change LVGL's threading (`LV_OS_FREERTOS`, own task) and allocator (`LV_USE_CLIB_MALLOC`) at the same time. Revisit at the ESP-IDF migration |

LVGL stays on `loop()` with `LV_OS_NONE`: measured, `LV_OS_FREERTOS` made drawing 20-51 % slower on
the P4_5 ([performance.md](performance.md)).
