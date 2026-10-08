# Display performance — method, numbers, levers

Current numbers per board and every lever tried, with its verdict. Tuning in progress is tracked in
#70 (S3s). The full experiment write-ups are `docs/archive/display/display-stack.md` §8.

## 1. How we measure

`GET /bench` (`src/UI/Bench.cpp` - every JSON field is explained at its top), driven by
`python scripts/bench.py <host>` (add `--anim`, `--page`). It forces frames with
`lv_obj_invalidate()` + `lv_refr_now()` and times them with `esp_timer`.

| Field | Meaning |
|---|---|
| `total` | the whole frame |
| `render` | LVGL drawing (`total - copy - present - wait`) |
| `copy` | getting strips into the frame buffer: on esp_lcd DSI, the time to *queue* PPA jobs |
| `present` | repair + hand-off on the last strip |
| `wait` | LVGL waiting for a free buffer or the panel |
| `panel_scan` | the refresh rate the panel really ran at, counted in the frame-complete interrupt |

Scenarios: **full** (the whole screen - a page change, the worst case), **card** (one card and its
shadow - the common case), **anim** (a deck-panel swap tapped as a finger would, every frame LVGL
draws on its own schedule), **page** (a page change, rebuild timed apart from drawing), **copy** (raw
copier throughput), **verify** (render vs the panel's buffer, in the same instant).

`python scripts/soak.py --minutes N <hosts>` cycles all of them and reports failures, reboots, UI
freezes (a 503 "did not respond" is what a PPA freeze would look like), the worst frame and the
lowest internal heap. **Frame time is not FPS.** Two runs reflashed in between agree within ~1 %.
**The screen freezes while a run measures** (a few seconds each; over a minute is a problem).

## 2. Where each board is (esp_lcd, Midnight, page 0, ms)

| Board | Full (render) | One card | Deck swap frame | Page change, swipe to glass | Before (Arduino_GFX) full |
|---|---|---|---|---|---|
| `WS_P4_5` | 84.5 (~73) | 5.7-8.2 | 32, 9 per swap, 0 late | ~400 | 162.9 |
| `WS_P4_4B` | 51.1 (44) | 4.4-5.2 | 17.6, 10 per swap | ~300 | 87.2 |
| `WS_P4_7B` | 64 (56) | 4.5 | 18, 0 late | 418-479 | 101 |
| `CYD_P4_1060` | 64 (56) | 4.2 | 18, 0 late | 396-530 | 104 (other scheme) |
| `WS_S3_4B` (pool in PSRAM) | 212 (198) | 22-26 | ~90, 5 per swap, 3 late | ~1,250 (rebuild ~1,000) | 221 |

- **Drawing is now the limit everywhere.** On the P4s drawing got faster once the CPU stopped
  copying (the copy was evicting LVGL's working set from the cache).
- **A page change is mostly rebuilding the page**, not the display: building ~15 cards (~7.5 ms each
  on the P4_5) and laying them out. The fix is design, not flush: keep the neighbour page built, or
  show a snapshot while it builds (`docs/design/pages.md` §6-7).
- The S3_4B's numbers are #70's baseline.
- **The card window's folder panels (SETTINGS, CHART) sliding, `WS_P4_5`** (2.10c round 9, timed per
  frame with `DEBUG_FRAMES`): 35-105 ms a frame while the holder was window-wide and transparent;
  **8-40 ms** sized to the tab and pane. A moving object redraws its whole area; where nothing
  opaque covers a strip, LVGL draws everything beneath it (LESSONS).

## 3. Levers tried, and the verdict

| Lever | Board | Result | Verdict |
|---|---|---|---|
| `-O2` instead of `-Os` (our code and LVGL) | CYD, P4_5 | full -14 % / -9 %, flash +8 % | **kept** on the dev boards; fleet-wide is the owner's call |
| `LV_USE_PPA` (LVGL's PPA draw unit) | P4_5 | drawing **11 % slower** | off; gate `-D FLEET_LV_PPA` kept. It takes only square solid fills and unrotated blits, and cache-syncs the whole draw buffer twice per fill |
| `LV_OBJ_STYLE_CACHE` | CYD, P4_5 | 2-4 % faster, 1.8-2.7 KB of pool | off |
| `LV_OS_FREERTOS`, 1 draw unit | P4_5 | drawing **+20 %** | not adopted |
| `LV_OS_FREERTOS`, 2 draw units | P4_5 | drawing **+51 %**; core 0 idle, the 2nd unit got ~1 % of the work | not adopted. Our screens are one stream of dependent jobs. Revisit only if the UI changes shape |
| Draw buffers 25 / 100 / full lines | 4B | +37 % / swap +29 % / worst | **50 lines** is the sweet spot |
| `LV_DEF_REFR_PERIOD` 16 ms | 4B | 48 instead of 30 frames/s in a swap, no cost per frame | `-D FLEET_LV_REFR_PERIOD=16`, **set on no board** - owner's call. P4_5 gains nothing (frame-bound) |
| Panel timing (4 combinations) | 4B | identical speed | choose by the picture only |
| Rotation 0 vs 2 | 4B | same on full frames; card ~1 ms cheaper at 0 | - |
| DMA2D copier for repairs | P4_5, 4B | ~140-150 MB/s vs the PPA's ~45-50; frames after a page change 43 -> 14 ms | **kept** (one job at a time - architecture §4) |
| Panel-side 180 degrees (MADCTL) | 7B | no effect on glass | rejected |
| LVGL pool in PSRAM | S3_4B | internal free 7 -> 133 KB; drawing ~5-10 % slower; one card 17 -> 22-26 ms | **kept on S3_4B only** as a fix for starvation; #70 |
| `DEBUG_CARDS` off | P4s | ~90 ms off every page swipe (UART) | off in `[P4-options]`; the 7B and S3s still have their own lines |

## 4. Framework settings — what we control and what is prebuilt

A setting of code we compile (LVGL, vendored drivers) is a `#define` or `-D`. A setting of the
prebuilt framework libraries (cache, PSRAM, `esp_lcd`, Wi-Fi) means rebuilding them
(`docs/REBUILD_P4_LIBS.md`, done once for the P4). **pioarduino ships separate libraries per chip**,
so an S3 rebuild cannot touch the P4s.

| Setting | P4 today | S3 today | Note |
|---|---|---|---|
| `FREERTOS_HZ` | 1000 | 1000 | right |
| Data / L2 cache | L2 256 KB, **64 B line** (the #49 Wi-Fi fix) | **32 KB / 32 B line** | P4: Wi-Fi stability wins, keep. S3: 64 KB / 64 B is Espressif's RGB recommendation - #70 |
| PSRAM XIP (`SPIRAM_FETCH_INSTRUCTIONS` + `RODATA`) | off | off | S3: stops flash writes stalling the RGB scan-out; #70 |
| PSRAM speed | 200 MHz | 80 MHz (octal) | S3 120 MHz is experimental |
| `SPIRAM_MALLOC_ALWAYSINTERNAL` | - | 4096 | allocations under 4 KB go to SRAM - relevant to #70 candidate 2 |
| `SPIRAM_TRY_ALLOCATE_WIFI_LWIP`, `MBEDTLS_EXTERNAL_MEM_ALLOC` | - | off / internal | would free S3 SRAM for LVGL; #70 |
| `LCD_RGB_RESTART_IN_VSYNC` | - | on | right |
| `LCD_*_ISR_IRAM_SAFE` | off | off | only if flash writes glitch the panel |

## 5. Memory

- **LVGL's pool (`lv_mem`)**: 512 KB in PSRAM on every P4 (2026-10-06, #88 - full-screen frames
  ~10% slower, measured on WS_P4_5: 91-97 -> 100-108 ms; a card frame unchanged), 128 KB in PSRAM on
  `WS_S3_4B`, a 128 KB static array in internal RAM on the other S3s. A card costs ~715 B of it;
  `lv_mem_monitor()` is the instrument, not `ESP.getFreeHeap()`.
- **S3_4B peak pool use over a 6-hour soak: ~78 KB** (never under 50 KB free) - the pool is
  oversized for today's pages, which is what makes "back in SRAM, smaller" plausible (#70).
- Internal free under soak: P4s ~110 KB lowest; S3_4B 130 KB with the pool in PSRAM, 4-12 KB with it
  internal.
