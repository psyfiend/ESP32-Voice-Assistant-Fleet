# Display stack — milestone 2.9: Arduino_GFX -> `esp_lcd`

**Status: PLANNED, decisions taken 2026-09-25 with the owner. No code yet.** This is the design and
the plan. The earlier research it builds on is `docs/research/display-stack-migration.md`. Read
that for the per-chip driver survey, and read this for what we are actually doing. Where the two
disagree, this file is newer.

---

## 1. Why now

The owner's four reasons, and they hold up:

1. It is a step towards ESP-IDF. `esp_lcd` is plain C, configured through structs.
2. The S3 boards, and to a lesser degree `WS_P4_5`, are visibly slow, and more so as the dashboard
   grows.
3. #58 (screenshots) means a session can see a screen without asking for a photograph.
4. Page transitions and the page overview (`pages.md` §6-7) are only worth building once drawing is
   fast.

## 2. What the pipeline does today — read from source, 2026-09-25

`src/LVGL_Startup.cpp` `disp_flush()` calls `gfx->draw16bitRGBBitmap()` on every chunk,
`gfx->flush()` on the last chunk, and then `lv_display_flush_ready()`. The flush is synchronous:
LVGL cannot start drawing the next frame until the copy below has finished. **So the second LVGL
buffer buys nothing on any board, whatever the library does.** The owner's "GFX only uses one
buffer" was right in effect, but the cause is the shape of the whole pipeline, not one bug.

| Panel type | Boards | After LVGL draws a chunk | Per-frame cost we pay for nothing |
|---|---|---|---|
| **DSI** | all four P4s | CPU copies the chunk into GFX's single framebuffer, per pixel, rotating as it goes (`Arduino_DSI_Display.cpp:396-408`), then `esp_cache_msync()`s the rows it touched, **per chunk** (`:413-435`). Every board sets `AUTO_FLUSH = true`, so `flush()` (`:508-512`) does nothing. *Corrected 2026-09-25 by `/bench`: this row used to say the whole framebuffer is synced on the last chunk, which is only the `AUTO_FLUSH = false` path* | A full-screen CPU copy. `WS_P4_5` is `ROTATION = 1`, a 90-degree turn and the most cache-hostile kind: each chunk's sync then spans the whole framebuffer. `WS_P4_7B`/`4B` are `2` (180 degrees) |
| **RGB** | `WS_S3_4B`, `WS_S3_5B`, `CYD_S3_8048` | Same CPU copy into a framebuffer, then `Cache_WriteBack_Addr()` of the rows touched, per chunk (`Arduino_RGB_Display.cpp:413-435`; `AUTO_FLUSH = true` again, so the whole-buffer write-back at `:567` never runs). `num_fbs = 2` is allocated but `getFrameBuffer()` always returns index 1 (`Arduino_ESP32RGBPanel.cpp:74,161`, #40) | A full-screen copy PSRAM-to-PSRAM, plus a whole frame of PSRAM wasted |
| **QSPI** | `CYD_S3_3248` | CPU copies the chunk into an `Arduino_Canvas` framebuffer. On the last chunk `Canvas::flush()` sends the **entire 320x480 frame** to the panel (`Arduino_Canvas.cpp:577-582`) | 307 KB over QSPI for every update, even one label |

LVGL's own buffers today, which also matter: P4_5 and 7B use 50-line partial buffers in PSRAM
(`DRAW_BUF_HEIGHT = 50`); the RGB S3s use full-frame buffers in PSRAM; the CYD uses 20-line
buffers in internal SRAM.

## 3. The caveat that decides the order: measure first

Nobody has timed where a frame's time goes. The research doc says the same about the rotation
cost: observed, never profiled. If drawing dominates, which is plausible on an S3 with shadows at
800x480, then changing the flush helps less there, and the gains have to come from drawing less.
So **step 1 is a measurement, and every later step is judged against it.**

## 4. Decisions (owner, 2026-09-25)

| # | Decision | Why |
|---|---|---|
| D1 | **Raw `esp_lcd`, not Espressif's `ESP32_Display_Panel`** | Native IDF, plain C structs. `ESP32_Display_Panel` has no HX8394 driver, which is exactly the `WS_P4_5` panel |
| D2 | **Measure first, then move one board at a time** | See §3 |
| D3 | **`WS_P4_5` first.** Then the other P4s, which should go faster once the kinks are out. Then the RGB S3s, then the CYD | P4_5 is a dev board with the best-understood problem. The owner expects the P4s to follow quickly |
| D4 | **Per-board opt-in build flag** (working name `-D DISPLAY_ESPLCD`) | A board moves by one line in `platformio.ini`, and moves back the same way. `main` works throughout. Arduino_GFX is deleted only after all eight have moved |
| D5 | **Our own LVGL glue, modelled on `esp_lvgl_adapter`; the adapter itself is not adopted (yet)** | See §6 |

The owner will plug in `WS_P4_7B` when its turn comes. Say so when ready; it was not on a COM port
on 2026-09-24.

## 5. The plan

### Step 1 — Measure (both dev boards, no display changes) — BUILT 2026-09-25, results in §8

- **`GET /bench`** beside `/screenshot`, same pattern: the handler asks the LVGL thread, and
  `loop()` does the work. It forces N full-screen redraws (`lv_obj_invalidate(screen)` then
  `lv_refr_now()`), timing **drawing** and **flushing** separately through `disp_flush`. It returns
  JSON. Run it on each page and with the deck open, so the numbers cover the real screens.
- **Experiment:** on `WS_P4_5`, `LV_USE_PPA 1` (LVGL's own PPA draw unit, which does fills and
  unrotated image blends) and measure again. It is two defines in `lv_conf.h` and is independent
  of the flush work.
- Write the numbers into this file (§8) and into the ROADMAP 2.9 row.

### Step 2 — `WS_P4_5` on `esp_lcd` DSI

- Panel: `esp_lcd_new_dsi_bus` / `esp_lcd_new_dbi_io` / `esp_lcd_new_panel_dpi`, with the HX8394
  init sequence. **The HX8394 driver is not on this machine.** Waveshare's IDF demo pulls
  `waveshare/esp_lcd_hx8394 ^2.1.0` and `waveshare/esp32_p4_wifi6_touch_lcd_5 ^1.0.4` from the
  Espressif component registry (`reference/.../Waveshare-P4-WIFI6-Touch-LCD-5/examples/esp-idf/08_lvgl_demo_v9/main/idf_component.yml`).
  Those sources need fetching, with the owner's OK. Our BSP's `RST_ACTIVE_HIGH` lesson applies.
- Frame buffers: the DPI panel's own (`num_fbs = 2`, `esp_lcd_dpi_panel_get_frame_buffer`).
- **Rotation through the PPA**, since `ROTATION = 1`. LVGL draws unrotated partial chunks; the PPA
  rotates each one into the panel's back framebuffer by DMA; `lv_display_flush_ready()` is called
  from the PPA's completion callback, **not** inline. That makes the flush asynchronous, which is
  what lets two LVGL buffers actually overlap. On the last chunk, swap framebuffers at vsync. The
  cache rules and 64-byte alignment are tabled in the research doc, Q3.
- Screenshots keep working unchanged: they render from the object tree, not the framebuffer.
- Touch mapping must still agree with the display's rotation (research doc Q5). It is not
  otherwise touched.
- `docs/TEST_2.9.md` for the owner, with `/bench` before and after.

### Step 3 — the other three P4s

Same DSI path, different panel chips: EK79007 (7B), ST7703 (4B), JD9165 (CYD_P4_1060). **Their
180-degree rotation may cost nothing**: many DSI panels can mirror X and Y in their own registers
(`esp_lcd_panel_mirror`). That has to be checked per chip. If it works, those three need no PPA
rotation at all.

### Step 4 — the RGB S3s

`esp_lcd_new_rgb_panel` with two framebuffers, LVGL in direct mode, swap at vsync, and no copy.
- **`WS_S3_4B`'s ST7701** is initialised over a 3-wire SPI through the XCA9554 expander. Today
  that is `Arduino_XCA9554SWSPI`. `esp_lcd` needs an equivalent; see §7.
- **Framework settings will probably matter here** and they are prebuilt (§6.2). Measure first,
  and rebuild the S3 libraries only if the numbers say so.

### Step 5 — the CYD (QSPI, AXS15231B)

Send only the changed area instead of the whole frame. **Unknown: whether the AXS15231B accepts
partial window writes over QSPI.** Many projects push full frames to it for a reason. Research it
before promising anything. If it cannot, the CYD's gains come from drawing less, not from the flush.

### Step 6 — delete Arduino_GFX

Once all eight boards run `DISPLAY_ESPLCD`: remove the submodule, `lcd_init_cmd_t`'s fallback
typedef and the GFX-shaped BSP fields.

## 6. `esp_lvgl_adapter`, and framework settings

### 6.1 Why not adopt it outright (D5)

`esp_lvgl_adapter` (Espressif, esp-iot-solution; the newer replacement for `esp_lvgl_port`) is
exactly the glue we need: tear-avoidance modes (`DOUBLE_FULL`, `TRIPLE_FULL`, `DOUBLE_DIRECT`,
`TRIPLE_PARTIAL`), PPA rotation, FPS stats, and a lock API. **Waveshare's own P4_5 IDF demo uses it**
(`08_lvgl_demo_v9/main/main.c`, `ESP_LV_ADAPTER_ROTATE_0`). It is not in our framework's prebuilt
libraries. Neither is `esp_lvgl_port`.

Adopting it as a dependency would mean three changes of model at once:

- It owns LVGL's init and runs LVGL on its **own task** (`LV_OS_FREERTOS`). Ours runs on `loop()`
  with `LV_OS_NONE`. `LVGL_Startup::lock()` exists precisely so this can change later, but it is a
  separate decision with its own risks.
- It recommends `LV_USE_CLIB_MALLOC`: LVGL allocating from the system heap instead of our 128 KB
  pool. On `CYD_S3_3248`, small allocations land in **internal** RAM, the fleet's scarcest resource
  (LESSONS.md).
- It assumes an IDF component build with Kconfig.

**So we read it as the reference implementation and write our own glue**, lifting code where it
helps. esp-iot-solution is Apache-2.0; confirm the header of each file before copying. Adopting it
wholesale is worth revisiting at the ESP-IDF migration, when we have its build system anyway. The
source needs fetching into `reference/` (the owner's OK first).

### 6.2 Settings: which ones we control, and which are baked in

A setting of code **we compile** (LVGL, anything vendored) is a `#define` or `-D`: free to change.
A setting of the **prebuilt framework libraries** (`esp_lcd`, PPA, cache, PSRAM) means rebuilding
them (`docs/REBUILD_P4_LIBS.md`, which we have done once for the P4).

| Setting (Espressif's recommendation for LVGL) | Ours today | Kind | Verdict |
|---|---|---|---|
| `FREERTOS_HZ=1000` | 1000 | prebuilt | already right |
| `LV_DRAW_SW_DRAW_UNIT_CNT=2` (draw on both cores) | 1 | ours | Needs `LV_OS_FREERTOS`. **A candidate big win on every dual-core board**, but it changes the threading model. Measure it as its own experiment, not bundled with the migration |
| `LV_OS_FREERTOS`, `LV_USE_CLIB_MALLOC` | `LV_OS_NONE`, builtin pool | ours | Not now; see 6.1 |
| `LV_DEF_REFR_PERIOD=15`, `LV_OBJ_STYLE_CACHE` | check `lv_conf.h` | ours | cheap experiments for step 1 |
| **P4:** `CACHE_L2_CACHE_LINE_128B` | **64B** | prebuilt | **Conflict.** We rebuilt to 64B as part of the #49 WiFi fix (esp-hosted-mcu#219/#243). WiFi stability wins. Only reconsider if measurement blames it, and never without a WiFi soak |
| **P4:** `CACHE_L2_CACHE_256KB` | 256KB | prebuilt | already right (and it is the one the #49 rebuild nearly halved) |
| **P4:** `SPIRAM_XIP_FROM_PSRAM` | not set | prebuilt | not needed to start |
| **P4:** `LCD_DSI_ISR_IRAM_SAFE` | not set | prebuilt | only matters if flash writes glitch the panel |
| **S3:** `ESP32S3_DATA_CACHE_64KB` + `DATA_CACHE_LINE_64B` | **32KB / 32B** | prebuilt | Likely matters for the RGB S3s (step 4). An S3 rebuild, measured first |
| **S3:** `SPIRAM_XIP_FROM_PSRAM` | not set | prebuilt | Same. It stops flash operations stalling RGB scan-out |
| **S3:** `LCD_RGB_ISR_IRAM_SAFE` | not set | prebuilt | Same family |
| **S3:** `LCD_RGB_RESTART_IN_VSYNC` | set | prebuilt | already right |

**Nothing needs changing for steps 1-3.** DSI, `esp_lcd` and the PPA driver are already prebuilt
and linked for the P4 (research doc Q1). Step 4 may need an S3 library rebuild, decided by numbers.

## 7. I2C, FleetI2C and the CH422G driver — no conflict

Checked 2026-09-25:

- **`esp_lcd`'s DSI, RGB and QSPI paths do not use I2C at all.** Touch stays on `bb_captouch` over
  Arduino `Wire` and is not part of 2.9.
- **Neither expander is on a P4 board.** `HAS_CH422G` is set only on `WS_S3_5B` (backlight and
  reset lines, driven from `DisplayManager`). `HAS_IO_EXPANDER` (the XCA9554) is set only on
  `WS_S3_4B`. Steps 1-3 cannot touch either.
- **At step 4 they come back into scope, as callers rather than conflicts.** `WS_S3_5B`'s CH422G
  sequence (`DisplayManager.cpp`, `ch422gRawWrite()`) moves across as-is. `WS_S3_4B`'s ST7701 init
  over the expander's 3-wire SPI needs a replacement for `Arduino_XCA9554SWSPI`, probably a small
  bit-bang over FleetI2C or Espressif's `esp_io_expander` + `esp_lcd_panel_io_3wire_spi`.
- **The one real I2C hazard is unchanged and not 2.9's:** IDF's legacy `driver/i2c.h` and the new
  `i2c_master` driver cannot be linked together (`FleetI2C.cpp`'s header notes). Arduino 3.x `Wire`
  is built on `i2c_master`. Any new code must use `i2c_master` or `Wire`, never the legacy driver.
  That matters at the ESP-IDF migration, when FleetI2C moves to `i2c_master`.

## 8. Measurements

### 8.1 How: `GET /bench` and `scripts/bench.py`

`src/UI/Bench.cpp` (field meanings at its top). It forces `n` identical frames with
`lv_obj_invalidate()` + `lv_refr_now()`, timed in microseconds with `esp_timer`, and
`LVGL_Startup::disp_flush()` splits the flush in two:

| Field | What it is today (Arduino_GFX) |
|---|---|
| `total` | the whole `lv_refr_now()` |
| `copy` | inside `draw16bitRGBBitmap()`, all chunks: the CPU copy (and rotation), **plus the per-chunk cache write-back** on DSI/RGB (§2) |
| `present` | inside `gfx->flush()` on the last chunk: nothing on DSI/RGB; the whole-frame QSPI send on the CYD |
| `wait` | LVGL waiting on the flush: ~0 while it is synchronous. Step 2 makes it real |
| `render` | `total - copy - present - wait`: LVGL itself |

Two scenarios: **full** = the whole screen (a page change, the worst case) and **card** = one card's
area including its shadow (a value changing, the common case). `bench.py` runs both on page 0,
page 1 and page 0 with the deck, 20 frames each, and saves a screenshot of each to `bench/`.
**Frame time is not FPS**: `fps_ceiling` is what that scenario could never beat.

Repeatability: two runs on each board, reflashed in between, agree within ~1%.

### 8.2 Baseline, 2026-09-25, firmware `0.2.7.6+dirty` (branch `feat/67-bench`), Midnight scheme

Milliseconds per frame, averages of 20. Page 0 = House; page 1 and the deck move these by under 5%.

| Board | Scenario | total | render | copy | present | px | chunks |
|---|---|---|---|---|---|---|---|
| `WS_P4_5` DSI 1280x720, rot 1, 2 x 50-line PSRAM bufs | full | **178.6** | 107.4 (60%) | 71.0 (40%) | 0.0 | 921,600 | 15 |
| | card | **7.7** | 4.9 | 2.8 | 0.0 | 54,356 | 1 |
| `CYD_S3_3248` QSPI 320x480, rot 0, 1 x 20-line internal buf | full | **221.7** | 163.7 (74%) | 9.1 | 48.6 (22%) | 153,600 | 24 |
| | card | **64.7** | 14.5 | 1.5 | **48.7 (75%)** | 16,677 | 3 |
| `WS_P4_5`, **Linen** (owner's run, T6) | full | **215.9** | 144.2 (67%) | 71.6 | 0.0 | 921,600 | 15 |
| | card | **10.2** | 7.4 | 2.9 | 0.0 | 54,356 | 1 |

**Linen costs drawing, not flushing:** +35% render on a full screen, +50% on a card, copy
unchanged. Its real drop shadows are the difference. No flush change will touch that; it is the
first concrete target for "draw less" after step 2. The owner reproduced every Midnight row within
~2% in the owner's own runs.

### 8.3 Experiments on `WS_P4_5` (each reverted; only the build flag for PPA remains)

| Change | full: total / render / copy | Verdict |
|---|---|---|
| `LV_USE_PPA 1` (`-D FLEET_LV_PPA`) | 192.3 / **119.2** / 73.0 | **11% slower drawing.** Off. Screenshots drew correctly |
| `AUTO_FLUSH = false` | 171.8 / 110.2 / **60.8** (+0.7 present) | 4% faster. Not adopted: step 2 replaces this path |

**Why PPA loses** (read in `components/lvgl/src/draw/espressif/ppa/`, then measured): it takes only
square-cornered, solid, opaque fills and unrotated image copies, so our rounded cards and all text
stay on the CPU; and for every fill it does take, it cache-syncs the **whole** draw buffer twice
(`lv_draw_ppa_buf.c:44`), 125 KB each time on P4_5. The small fills it wins do not pay for the syncs.
It does not rotate the display: that is step 2's flush, a different use of the same hardware. The
`lv_conf.h` gate stays in, off, so this can be re-run after step 2 changes the buffers.

`AUTO_FLUSH = false` measured what the per-chunk cache syncs cost: ~10 ms of `copy`. The one
whole-framebuffer sync that replaces them costs 0.7 ms. **So ~61 ms of P4_5's 71 ms `copy` is the
rotating CPU copy itself.**

### 8.4 What the numbers say about the plan

- **P4_5, step 2 (DSI + PPA rotation + async flush):** takes the 71 ms `copy` off the CPU. The
  frame becomes render-bound at ~107 ms: roughly **5.6 -> 9 frames/s** on a full redraw, and a card
  update from 7.7 to ~5 ms. Worth doing, and it is the ceiling: after it, only drawing less helps.
- **CYD, step 5 (partial window writes):** a one-card update is **75% QSPI send** of a frame that is
  90% unchanged. If the AXS15231B accepts partial windows, a card update goes from ~65 ms to about
  15 ms plus a card-sized send. That is the biggest single ratio anywhere in these numbers. The
  open question in §9 is now the most valuable one to answer.
- **Drawing itself is the larger cost on both boards** (60% / 74% of a full frame), and no flush
  change touches it. The CYD draws at ~1.07 us/px against the P4's ~0.12, with 24 chunks per frame:
  every chunk walks the whole widget tree again. Larger CYD draw buffers would cut the walks but
  cost internal RAM it does not have. `LV_DRAW_SW_DRAW_UNIT_CNT=2` (§6.2) is the other lever. Both
  are separate experiments, after step 2.
- **Not yet measured:** Linen on the CYD, and animation frames such as a swipe or the drawer,
  which redraw partial areas every tick rather than one full frame.

## 9. Open questions

- AXS15231B partial window writes over QSPI (step 5).
- Which DSI panels can mirror X/Y in their own registers (step 3).
- Whether `LV_DRAW_SW_DRAW_UNIT_CNT=2` is worth changing the threading model for. That is a
  decision for after step 1's numbers.
- #40 (diff the GFX fork against the vendor trees) is mostly superseded. The vendor trees are still
  where each panel's init sequence and timings come from.
