# 2.9 step 2 — `WS_P4_5` on `esp_lcd` DSI, triple-partial with PPA rotation

**Status: PROPOSED 2026-09-25, for the owner's review. No code yet.** Parent plan:
`display-stack.md` (decisions D1-D5, the numbers in §8, the PPA-freeze risk in §9). Sources:
`docs/research/waveshare-esp-lcd-survey.md` §1, and `esp_lvgl_adapter` 0.6.4 read as the reference
implementation (`reference/esp-registry/`). The four structural choices the owner should make are
marked **CHOICE** and collected in §7.

---

## 1. What changes, in one paragraph

Today LVGL draws 50-line chunks, and Arduino_GFX copies and rotates each one into its single
framebuffer on the CPU (71 ms of a 179 ms full frame, `display-stack.md` §8.2). After step 2, the
panel owns **three** framebuffers. Each chunk LVGL finishes is rotated into the framebuffer being
built by the **PPA** (the P4's 2D hardware); when the frame is complete, that buffer is handed to
the panel, which switches to it at the next refresh, and LVGL moves on to the third. No CPU copy,
no tearing, and the panel never shows a half-drawn frame.

## 2. Who owns what

Following `CLAUDE.md`'s rules - hardware is owned by `SystemCore`, and `SystemCore` includes no
LVGL header - the work splits in two, exactly as `esp_lvgl_adapter` splits it (`hw_init` vs
`bridge`):

| Piece | Owns | LVGL? | Where (**CHOICE A**) |
|---|---|---|---|
| **Panel bring-up** | DSI PHY power (LDO 3, 2500 mV), DSI bus, DBI command IO, the HX8394 driver, 3 framebuffers, `use_dma2d`, reset, backlight (unchanged) | no | `components/DisplayManager/EspLcdDsi.{h,cpp}`, reached from `DisplayManager::initPanel()` under `#ifdef DISPLAY_ESPLCD` |
| **Flush bridge** | LVGL's flush callback: PPA rotation, the buffer rotation and repair bookkeeping, the refresh-done wait | yes | `src/Display_TriplePartial.{h,cpp}`, selected by `LVGL_Startup::begin()` under the same flag |
| **HX8394 driver** | init sequence, reset polarity, DSI video-mode setup | no | vendored as `components/esp_lcd_hx8394/` (MIT), **fed our own BSP init commands** (`hx8394_vendor_config_t.init_cmds`) so the sequence proven on this board stays the one we run (**CHOICE B**) |

`DisplayManager::getGfx()` returns `nullptr` under the flag; its only callers are the two in
`LVGL_Startup.cpp`, which switch to the display size from the BSP. Touch, backlight, audio, the
header and every card are untouched.

## 3. The flush, step by step (LVGL thread, `LV_OS_NONE` - unchanged)

For every chunk LVGL hands us (50 lines, landscape, unrotated):

1. Write the chunk back from the CPU cache (`esp_cache_msync`, C2M), because the PPA reads memory,
   not cache.
2. `ppa_do_scale_rotate_mirror()`, **blocking**, rotating the chunk into its place in the back
   framebuffer. `/bench` reports this as **`copy`**.
3. Tell LVGL the chunk is done, so it draws the next one.

On the **last** chunk of a frame, additionally:

4. **Repair**: copy into the back buffer every area that changed in the *previous two* frames but
   was not redrawn in this one - otherwise the buffer shows stale content from two frames ago. The
   adapter's diff tracker (`lvgl_bridge_v9.c:3088-3168`) is the model: each framebuffer keeps a
   list of areas it is behind on; repair = that list minus what was just drawn. Copies by **PPA with
   rotation 0** (public API) rather than the adapter's `esp_async_fbcpy`, which our framework
   compiles in (`libesp_lcd.a`) but does not export a header for (**CHOICE C**).
5. `esp_lcd_panel_draw_bitmap(panel, 0, 0, w, h, back_fb)` - with a buffer the panel owns, this
   *switches* to it rather than copying: IDF 5.5.5 checks whether the pointer lies inside one of its
   framebuffers, and if so only writes back the cache and changes `cur_fb_index`
   (`esp_lcd_panel_dpi.c:502-526`, read 2026-09-25). A full-frame write-back measured 0.7 ms in
   step 1's `AUTO_FLUSH` experiment.
6. Take the next free buffer. It is free once the panel reports it has stopped showing it: the
   **`on_frame_buf_complete`** callback (IDF 5.5.5's name; `on_refresh_done` is deprecated) gives a
   semaphore. **IDF refuses to register that callback unless it is in IRAM**
   (`esp_lcd_panel_dpi.c:640`, `esp_ptr_in_iram`), so it must be `IRAM_ATTR` and call nothing that
   is not. Time spent waiting is `/bench`'s **`wait`**; steps 4-5 are **`present`**.

At 55 Hz (the real P4_5 refresh, survey §1) a wait is at most ~18 ms, and with three buffers it
should normally be zero.

## 4. Things that must be right, each from source

- **Alignment.** PPA and cache sync want 64-byte aligned buffers. Under the flag, `lv_conf.h`
  sets `LV_DRAW_BUF_ALIGN 64` (the gate step 1 already built for the PPA experiment), and
  `LVGL_Startup`'s allocator already honours it.
- **Rotation direction.** The PPA's angles are counter-clockwise, so the adapter maps its 90 to the
  PPA's 270 (`lvgl_bridge_v9.c:2833-2836`). Our `ROTATION = 1` has only ever been Arduino_GFX's
  meaning. **First light is a picture with an unmistakable "up"**; getting the angle wrong shows as a
  sideways or upside-down dashboard, not as a crash.
- **Touch** maps to LVGL's logical landscape coordinates today and LVGL still draws landscape, so
  `TouchManager` should need nothing. Verified on glass, not assumed (research doc Q5).
- **Screenshots** render from the object tree, not a framebuffer: unaffected.
- **Memory.** 3 x 1.8 MB framebuffers in PSRAM (P4_5 has ~26 MB free), minus Arduino_GFX's
  1.8 MB one. Internal RAM: unchanged apart from a semaphore.
- **The PPA freeze** (`display-stack.md` §9): our exact configuration. Test for it deliberately
  (§6); if it happens, the fix is a P4 library rebuild with Espressif's one-line patch, on IDF 5.5.5.

## 4a. Compile spike, 2026-09-25 — it builds and links

Branch `spike/67-esplcd-compile` (throwaway, never merged): a C function calling every IDF API
above once, in order, plus Waveshare's HX8394 driver compiled straight from `reference/`, linked
into the `WS_P4_5` firmware behind a condition that can never be true at runtime. **Result: compiles,
links, and every symbol is in the ELF** (`esp_lcd_new_dsi_bus`, `esp_lcd_new_panel_dpi`,
`esp_lcd_new_panel_hx8394`, `ppa_do_scale_rotate_mirror`, `esp_ldo_acquire_channel`,
`esp_lcd_dpi_panel_register_event_callbacks`). +22 KB of flash. The IRAM refresh callback landed at
`0x4FF00330`, the P4's internal L2 memory, so `IRAM_ATTR` does what IDF's check wants.

**Four vendoring gotchas it surfaced**, all for the real `components/esp_lcd_hx8394/`:

1. **The driver does board-level I2C by default.** Unless `CONFIG_ESP_LCD_HX8394_SKIP_I2C_INIT`
   is set, its constructor opens I2C bus 1 on pins 7/8 with IDF's **legacy** driver, writes
   registers `0x95`/`0x96` on a device at `0x45`, and sleeps a second (`esp_lcd_hx8394.c:115-143`).
   Pins 7/8 are our shared I2C bus, and the legacy driver cannot coexist with Arduino's `Wire`
   (`display-stack.md` §7). **Must be set.** Waveshare's own P4_5 BSP sets it too, and does not do
   the `0x45` writes itself (its README, "HX8394 initialization"); our board has never done them.
2. **It includes `i2c_bus.h` unconditionally** (line 16), a registry component we do not have, even
   with (1) set. Our copy guards that include: a documented one-line local change.
3. **Its version macros come from IDF's component build** (`ESP_LCD_HX8394_VER_*`); PlatformIO
   does not inject them. Define them in the vendored copy's build flags or header.
4. **Its config macros are C, not C++**: they list designated initialisers out of declaration order
   (`HX8394_720_1280_PANEL_30HZ_DPI_CONFIG`). The panel bring-up file is therefore **C**
   (`EspLcdDsi.c`), or builds the struct field by field in C++.

## 5. Build flag and fallback

`-D DISPLAY_ESPLCD` in `WS_P4_5`'s environment only (D4). Removing the line puts the board back on
Arduino_GFX, byte for byte. The other seven boards do not see any of this code.

## 6. How it will be judged

- `/bench` before and after, same matrix. Expected: `copy` falls from 71 ms to the PPA's time (not
  yet measured), `render` unchanged at ~96 ms with `-O2`; a full frame from ~163 ms to roughly
  `96 + PPA` ms. A one-card update should fall sharply, because only the card is rotated.
- `docs/TEST_2.9.md` step 2: first light (orientation), touch at the four corners, no tearing
  during a drawer animation, screenshots unchanged, and a **soak**: `bench.py` in a loop for an hour
  (constant partial rotation) plus page swipes, watching `uptime_s` for a freeze-reboot.

## 7. Choices for the owner

| | Choice | Recommendation |
|---|---|---|
| **A** | Panel bring-up inside `DisplayManager` (new file, `#ifdef` branch) vs. a new component | Inside `DisplayManager`: it already owns every panel type, and step 6 deletes the GFX branches beside it |
| **B** | Vendor Waveshare's HX8394 driver, fed our init commands, vs. writing the DBI init ourselves | Vendor it: it is 350 lines, MIT, and it is exactly what their BSP and demo run |
| **C** | Repair copies by PPA (public API) vs. declaring the unexported `esp_async_fbcpy` ourselves | PPA: public, documented, one engine to reason about |
| **D** | Keep `LV_OS_NONE` (flush blocks `loop()` while the PPA works) vs. moving LVGL to its own task now | Keep it. One change of model at a time (`display-stack.md` §6.1) |

Nothing here needs the owner at the keyboard except first light, which needs eyes on the glass.
