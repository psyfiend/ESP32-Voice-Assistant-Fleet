# Display — problems already solved

Read this when something on the display looks familiar, not every session. Each entry: the symptom,
the cause, the fix, and where the full story is. General lessons that apply beyond the display live
in `docs/LESSONS.md`; the complete experiment write-ups in `docs/archive/display/`.

## The P4_5 held in reset (bring-up, 2026-09-06)
DSI init commands filled the host FIFO and `begin()` blocked part-way. The HX8394's reset is
active-HIGH; the generic active-low sequence left it asserted. **Fix:** `RST_ACTIVE_HIGH`. Found in
Waveshare's own copy of the library - diff the vendor's whole tree before theorising.
`docs/BRINGUP_WS_P4_TOUCH_LCD_5.md`.

## The deck-panel stutter on the P4_5 (2026-09-26)
Swaps ran 50-58 ms a frame with late frames, on both paths. On esp_lcd the time was the **repair**:
a stale area was skipped only if one area of the new frame covered it whole, and past 8 areas a
buffer went wholly stale - so nearly everything was re-copied and then drawn over. **Fix:** subtract
this frame's redraw with `lv_area_diff`, merge overlapping stale areas (16 kept). 9 frames a swap,
none late. A forced full-screen benchmark could not see this; `/bench?what=anim` was built for it.
Archive: `display-stack.md` §8.6.

## Stale pixels from the DMA2D copier (2026-09-26)
Repairs on a pool of 16 copier handles put stale pixels on the P4_5. `esp_async_fbcpy()` hands the
DMA2D driver a pointer to one `static` config shared by every handle, read only when a queued job
starts. **Fix and rule: at most one copier job outstanding in the program** - one worker, one
handle. Isolated tests of the copier all passed; only the real use failed. Archive: §8.10.

## A PPA freeze that matched our exact configuration (risk, never seen)
`esp_lvgl_adapter` ships an ESP-IDF patch for a PPA hang with partial mode and 90/270 rotation - the
P4_5's setup. The same code is in our IDF 5.5.5. Built anyway (owner); 30-minute and later soaks
never froze. If it ever does: rebuild the P4 libraries with the one-line patch
(`reference/esp-registry/`, `docs/REBUILD_P4_LIBS.md`). Archive: §9.

## Page swipes 90 ms slower than they needed to be (2026-09-26)
`DEBUG_CARDS` had been left on since 2.7, printing ~3 KB per page change through a UART that
blocks. **Fix:** off in `[P4-options]`. LESSONS: "a debug print is not free".

## A timing change that did nothing - a stale build (2026-09-26)
The first vendor-timing build reported the new timing while the panel ran the old refresh rate:
`Fleet_Display.cpp` came from the build cache. **Rule:** clear `.pio/build_cache` after a BSP edit;
trust `panel_scan`, the measured rate, over the reported config.

## The 7B's "free" 180 degrees (2026-09-28)
The EK79007 driver has `mirror()`; the panel ignored it in video mode, three ways. The PPA keeps the
rotation. LESSONS, Hardware: a driver's `mirror()` is not proof the panel mirrors.

## The 4880 freezing when a PC was attached (2026-09-28)
Native-USB boards (HWCDC) froze whenever a PC held the port open without reading: each
`Serial.print` blocked 20 x 100 ms. Not the display. **Fix:** `Serial.setTxTimeoutMs(0)` in
`main.cpp`. LESSONS, Hardware.

## The S3_4B starved of internal RAM (2026-09-29)
HA could not open its socket, benches died, a panic, the deck animation hung - on the Arduino_GFX
path too. Internal free was 4-12 KB. **Fix:** LVGL's pool in PSRAM on that board
(`-D FLEET_LV_MEM_PSRAM`): 133 KB free. Costs ~5-10 % of drawing; #70 is trying to win it back.
LESSONS: "LVGL's 128 KB pool was the biggest thing in the S3s' internal RAM". Setting build flags
through `PLATFORMIO_BUILD_FLAGS` wiped `.pio/build` mid-build - put them in `platformio.ini`.

## The S3_4B's roll and left-edge lines (2026-09-29, being watched)
Came back after hours, on both display paths, with plenty of RAM. Traced to panel timing against
the ST7701S datasheet; the RTNI change (`C2 31 02`) left the glass clean. Matching `C1` to the host
made it worse. Not closed until #69's 7-day watch passes. [panels.md](panels.md), ST7701.
