# Waveshare `esp_lcd` survey — what their ESP-IDF code knows that ours does not

**2026-09-25, for milestone 2.9 (#67).** The long-standing "some day, diff the whole Waveshare
collection" item, retargeted: not how they drive Arduino_GFX, but how they bring panels up on raw
`esp_lcd` and LVGL in ESP-IDF. Everything below was read in source, and each claim names its file.
Where something is a guess, it says so.

**Sources read.** `reference/Waveshare Official Repos/` (P4-5, P4-7B, P4-4B, S3-4B, S3-5B; the
AMOLED 2.06 is not our board and was skipped), plus the Espressif-registry components they pull in,
now in `reference/esp-registry/` (`REFERENCE_PROJECTS.md`). **Not covered: the three Guition boards
(`CYD_P4_1060`, `CYD_S3_3248`, `CYD_S3_8048`)** - there is no Guition source in `reference/`. The
JD9165 driver (`CYD_P4_1060`'s panel) was fetched and read; the rest of those boards was not.

---

## 1. For step 2 — `WS_P4_5` on `esp_lcd` DSI

**The bring-up to copy** is `bsp_display_new_with_handles()` in
`esp-registry/waveshare__esp32_p4_wifi6_touch_lcd_5-v1.0.4/esp32_p4_wifi6_touch_lcd_5.c:427-505`:

| Step | Their value | Ours today | Note |
|---|---|---|---|
| DSI PHY power | LDO channel 3, 2500 mV (`display.h:33-34`) | same, but done inside Arduino_GFX (`Arduino_ESP32DSIPanel.h:23-24`) | **our `esp_lcd` path must acquire it itself** |
| DSI bus | 2 lanes, 700 Mbps, `phy_clk_src = 0` ("let IDF pick for the revision") | 2 lanes, 700, `PHY_CLK_SRC` default | match |
| Command IO | `esp_lcd_new_panel_io_dbi`, 8-bit cmd, 8-bit param | - | |
| Panel | `esp_lcd_new_panel_hx8394`, RGB565, `reset_active_high = 1` | `RST_ACTIVE_HIGH = 1` | match - our bring-up lesson is theirs too |
| Timing | 58 MHz, 20/20/40 H, 4/10/24 V | identical | see below |
| Frame buffers | `num_fbs = CONFIG_BSP_LCD_DPI_BUFFER_NUMS`, **default 3** (`Kconfig:88-90`) | 1 (GFX's own) | triple-partial needs 3 |
| `use_dma2d` | **true** (`esp_lcd_hx8394.h:26`) | - | makes the panel's own `draw_bitmap` copy by DMA, not CPU |
| LVGL glue | `esp_lv_adapter`, `TRIPLE_PARTIAL`, `buffer_height = 50`, PSRAM, **rotation 0** | 50-line PSRAM buffers, rotation 1 | |

- **The refresh rate is 55 Hz, not 30.** The macro is named `HX8394_720_1280_PANEL_30HZ_DPI_CONFIG`
  and our BSP comment repeats "@30Hz", but 58 MHz / (800 x 1318) = 55.0 Hz. Read the arithmetic,
  not the name. (Comment corrected on this branch.)
- **How `esp_lvgl_adapter` does triple-partial with rotation** (`lvgl_bridge_v9.c:2720-2798`):
  each LVGL chunk is rotated by the PPA straight into the back framebuffer, **in blocking mode**
  (`:2885`), inside the flush callback; on the last chunk it repairs areas LVGL did not redraw by
  copying them from the previous frame (`copy_diff_repair_from_front_to_back`, `:3009`), hands the
  finished buffer to the panel, and takes the next free one. So **LVGL does not draw while the PPA
  rotates** - the overlap triple buffering buys is between LVGL drawing and the panel *showing*,
  not between drawing and rotating. The frame cost becomes `render + PPA time` rather than
  `render + 71 ms of CPU copy`. How long the PPA takes is unmeasured; `/bench`'s `copy` field will
  report it. The diff-repair bookkeeping is the part of the adapter most worth lifting carefully.
- **The adapter always registers the PPA for rotation on a P4** (`lvgl_bridge_v9.c:1103-1110`),
  whatever `enable_ppa_accel` says. That flag is the separate *PPA-helps-LVGL-draw* feature -
  `LV_USE_PPA`, measured 11% slower here - and **every Waveshare BSP leaves it off**, which agrees
  with our measurement.
- **Rotation choices in their own BSPs:** P4_5 and 4B run `ROTATE_0`; the 7B runs `ROTATE_180`
  through the adapter with triple-partial. The 7B's older BSP (`WS Github/...7b.c:571-573`) says
  outright "Only SW rotation is supported for 90 and 270" under `esp_lvgl_port`; the adapter is the
  newer answer to that, with the PPA-freeze caveat in `display-stack.md` §9.

## 2. Drawing faster — levers on the other half of the frame

Every Waveshare P4 LVGL demo sets these (`08_lvgl_demo_v9/sdkconfig.defaults` and the 7B/4B
equivalents). We control the first five, because LVGL is compiled by us:

| Setting | Theirs | Ours | Worth |
|---|---|---|---|
| **`COMPILER_OPTIMIZATION_PERF` (-O2)** | all three demos | **`-Os`** - confirmed from `pio run -t compiledb`, on `lv_draw_sw_fill.c` and our own sources | **New, cheap: one build flag, then `/bench`.** Costs flash |
| `LV_DRAW_SW_DRAW_UNIT_CNT` | 2 | 1 | draw on both cores; needs `LV_OS_FREERTOS` (§6.2) |
| `LV_OS_FREERTOS` | on | `LV_OS_NONE` | the threading-model decision (§6.1) |
| `LV_DEF_REFR_PERIOD` | 15 ms | 33 ms | caps animation at ~30 fps today whatever else improves |
| `LV_OBJ_STYLE_CACHE` | on (P4_5 demo) | off | cheap `/bench` experiment |
| `LV_USE_CLIB_MALLOC` | on | builtin 128 KB pool | internal-RAM risk on the CYD (§6.1) |
| `SPIRAM_XIP_FROM_PSRAM` | on | off | prebuilt; a library rebuild |
| `CACHE_L2_CACHE_LINE_128B` | on | 64B | our #49 WiFi fix; stays |
| PSRAM speed | 200 MHz on rev<3, 250 on rev3 | **200 MHz** | already right |

## 3. For step 3 — the other P4s

**Can a panel do a 180-degree turn itself?** IDF's DSI panel has no mirror or rotate at all - its
only methods are init, delete and draw (`esp_lcd_panel_dpi.c:356-358`, IDF v5.5.5). So it comes
down to each chip's driver:

| Board | Panel | Driver's mirror | 180 degrees free in the panel? |
|---|---|---|---|
| `WS_P4_7B` | EK79007 | X and Y, MADCTL (`esp_lcd_ek79007.c:234`) | **on paper, yes** |
| `CYD_P4_1060` | JD9165 | X and Y, MADCTL (`esp_lcd_jd9165.c:237`) | on paper, yes - but it runs rotation 0, so moot |
| `WS_P4_4B` | ST7703 | **Y only**: "Mirror X is not supported" (`esp_lcd_st7703.c:319`) | no - **moot: owner, 2026-09-25, set it back to rotation 0** |
| `WS_P4_5` | HX8394 | no mirror function | n/a - 90 degrees needs the PPA regardless |

"On paper": whether a DSI panel in video mode honours MADCTL has to be seen on glass. Waveshare's
current 7B BSP rotates through the PPA instead, while its older one used the panel mirror; both
shipped, so either should work. The 7B is the one board where "free" is worth testing first.

**`WS_P4_4B` runs faster than either Waveshare source.** Ours: 46 MHz, porches 20/80/80 H and
4/12/30 V, lanes 1000 Mbps = **66.7 Hz**. Waveshare's Arduino config
(`examples/arduino/libraries/displays/displays_config.h:113-116`) and the ST7703 driver agree on
38 MHz, 20/50/50 and 4/20/20, lanes 480 Mbps = **59.2 Hz**. Ours matches neither, and that is
deliberate: the owner keeps every earlier or conflicting value as a comment beside the one in use,
because published "working" timings disagree - Waveshare's own documents included. Step 3 should
treat the vendor values as one more candidate to compare on glass, not as a correction.

**`WS_P4_7B`** matches its vendor timing exactly (52 MHz, 60.4 Hz, 1000 Mbps).

## 4. For step 4 — the RGB S3s

**`WS_S3_4B`'s ST7701 needs no home-made driver.** Waveshare's BSP
(`esp-registry/waveshare__esp32_s3_touch_lcd_4b-v2.0.0/esp32_s3_touch_lcd_4b.c:391-489`) uses
Espressif's `esp_lcd_panel_io_additions` 3-wire SPI with every line (`CS`, `SCL`, `SDA`) on the
TCA9554 expander (`IO_TYPE_EXPANDER`), then `esp_lcd_new_panel_st7701` with the RGB config. That
closes the open item in `display-stack.md` §7. Details worth keeping:

- The reset is a dance on expander pins 5 and 6 with **200 ms** between steps, after which pin 6 is
  turned back into an input (`:399-406`).
- `auto_del_panel_io = 1`: the 3-wire IO is deleted once the panel is initialised, freeing the
  expander lines.
- Their expander driver wants an ESP-IDF `i2c_master` bus handle. **arduino-esp32 exposes one:
  `i2cBusHandle(num)`** (`cores/esp32/esp32-hal-i2c.h:44`), so it can share `Wire`'s bus rather
  than open a second driver - and it is the new `i2c_master` driver, so §7's legacy-driver hazard
  does not arise.
- Their defaults for this board are conservative: **one** framebuffer, tear avoidance **off**,
  20-line bounce buffer (`Kconfig:65-80`).
- Our timings differ from the driver's (`ST7701_480_480_PANEL_60HZ_RGB_TIMING`: 16 MHz, 10/10/20,
  10/10/10), but our BSP keeps those originals as comments beside the new values, so the change was
  deliberate. Leave them.

**`WS_S3_5B`** (`WaveShare-S3-Touch-LCD-5B/ESP-IDF/08_lvgl_Porting`): timings and the 10-line
bounce buffer match ours exactly. Its tear avoidance is the classic three modes - two framebuffers
+ LVGL full refresh, three + full refresh, or two + LVGL direct mode (`Kconfig.projbuild:62-67`) -
synchronised on the RGB panel's vsync callback (`waveshare_rgb_lcd_port.c:10-12`). Its
`sdkconfig.defaults` sets the S3 library options `display-stack.md` §6.2 already listed as
candidates: **PSRAM XIP** (`SPIRAM_FETCH_INSTRUCTIONS` + `SPIRAM_RODATA`), **64-byte data-cache
lines**, 80 MHz octal PSRAM, `FREERTOS_HZ=1000`. Ours: 80 MHz yes, 32 KB/32-byte cache, no XIP.
That is independent confirmation that the S3 rebuild is worth measuring at step 4.

## 5. For step 5 — the CYD's AXS15231B, from Guition's own demo (added 2026-09-25)

The owner added Guition's packs as `reference/Guition Examples/`. The CYD's
`Guition-S3-JC3248W535/1-Demo/Demo_Arduino/DEMO_LVGL/` carries an `esp_lcd` driver for the panel.
**That demo is written for LVGL 8** (owner). So its LVGL-side choices (`full_refresh`, its
`lv_port.c`) describe an LVGL 8 port, not what LVGL 9 could do; the panel-driver findings below sit
underneath LVGL and hold either way.

- **Over QSPI the vendor never sends a row address.** `panel_axs15231b_draw_bitmap()`
  (`esp_lcd_axs15231b.c:287-324`) sets the column window (`CASET`) but skips `RASET` when
  `use_qspi_interface` is set, then writes with `RAMWR` (0x2C) if the area starts at row 0 and
  `RAMWRC` (0x3C, "continue") otherwise. So every write either starts at the top or carries on
  exactly where the last one stopped. **Arbitrary partial windows are not what the vendor does**,
  and their LVGL port runs `full_refresh = 1` with a full-screen buffer (`lv_port.c:242`,
  `DEMO_LVGL.ino:35`).
- **What that leaves for step 5**, unproven, needing the glass: (a) whether `RASET` over QSPI
  works anyway on this controller revision - some community drivers claim so, and the vendor's
  choice may be caution; (b) a column-windowed write from row 0 down to the dirty area's bottom
  row, which is cheap for anything near the top of the screen and a full-height strip for anything
  near the bottom. The honest expectation is now lower than §8.4 of the plan hoped.
- **Tearing: QSPI is not out of luck.** The panel has a **TE (tearing effect) pin, GPIO 38**
  (`esp_bsp.h:41`). Guition's BSP times each write to it (`esp_bsp.c:175-218`, with
  `time_Tvdl = 13` / `time_Tvdh = 3` ms from `display.h:49-50`), and `esp_lvgl_adapter` has a
  `TE_SYNC` mode for the same thing (`display_te_sync.c`). Our BSP has no TE field yet.

The other Guition packs: `JC1060P470` (our `CYD_P4_1060`) and `JC8048W550` (`CYD_S3_8048`) are
Arduino-only demos; not read further. `JC4880P433` is the **new, not-yet-onboarded board**: a P4
with a 480x800 **ST7701 over MIPI-DSI** (`esp_lcd_st7701_mipi.c`), and real ESP-IDF 5.5.4
examples built on Espressif's `esp32_p4_function_ev_board` BSP - the best-documented Guition
board we have, and the natural place to start its onboarding.
