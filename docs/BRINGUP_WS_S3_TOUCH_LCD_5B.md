# WS_S3_TOUCH_LCD_5B Bring-Up — Status & History

Working notes from bringing up the Waveshare ESP32-S3-Touch-LCD-5B. High volatility -
update or prune as things resolve. See `PROJECT_STATUS.md` for the rest of the fleet,
`FUTURE_IMPROVEMENTS.md` for deferred architectural items, `GUI_FRAMEWORK.md` for the UI
vision. This board's own planned use (garage door relays via DIO, RS485 sprinkler control)
is tracked in `FUTURE_IMPROVEMENTS.md`'s "New hardware to add" section.

## Current state

- **Display: working.** RGB panel, backlight, LCD reset all confirmed on real hardware.
- **CH422G expander: working**, and never actually the problem - see root cause below.
- **Touch (GT911): WORKING, confirmed on real hardware.** Correct status-byte sequence
  observed (`0x81` = buffer-ready + 1 point during a press, `0x80` transitional after
  release, settling to `0x00` idle - textbook-correct GT911 behavior).
- **New, separate issue: visible tearing/rough refresh**, worse than other RGB boards in
  the fleet (e.g. 8048W550). Not yet investigated - see "Known follow-up" at the end.

## ROOT CAUSE (found after a full day chasing red herrings - read this first)

`bb_captouch`'s chip-autodetection probes a hardcoded list of candidate I2C addresses in a
fixed order before ever trying GT911. One of them, `TMA445_ADDR` (`0x24`), is **numerically
identical to CH422G's own `WR_SET` register pseudo-address** - the one this board's
`ch422gRawWrite(0x24, ...)` calls hit constantly. This is not a false-positive/bug in
whichever I2C backend was active - CH422G genuinely, correctly ACKs at `0x24`, because it's
really there. `bb_captouch` concludes it found a TMA445 touch chip, and immediately sends it
a "soft reset + security key" sequence meant for a real TMA445 - garbage that likely left
CH422G's own internal register state disturbed - all before ever reaching the code that
would have tried GT911's actual address at all.

**This detection order is identical regardless of I2C backend** - Wire, the legacy driver,
and bit-banged software I2C all run through the exact same `bb_captouch.cpp` logic, hitting
this exact same collision, every single time. None of the driver-generation/timing/clock-speed
theories chased all day (sections below) were ever the real blocker. They were mostly real,
separately-useful fixes (the display bounce-buffer bug genuinely needed fixing; `FleetI2C`
is genuinely useful architecture) that happened to run in parallel with the actual bug,
which was hiding in a place none of that investigation was looking.

**Fix**: `bb_captouch.cpp`'s `TMA445_ADDR` probe block is now wrapped in `#ifndef
HAS_CH422G` - skipped entirely on any board using the CH422G expander, since it will always
collide there. Confirmed on real hardware with the bit-bang backend active
(`I2C_BACKEND_BITBANG`, still enabled for this board as of this note - see "what to do about
the I2C backend now" below).

**How this was actually found**: not by reasoning about it, by adding a print at the point
where `bb_captouch` prints its detected chip type, and noticing it read
`Matched TMA445_ADDR=0x24` instead of ever reaching GT911 detection at all. An earlier
attempt to add this print in the wrong place (right before the function's *final* return)
never fired at all, which - once understood *why* it never fired (several chip-type branches
return early, before reaching that point) - was itself the first real clue, not a dead end.

## What actually fixed the display (do not re-litigate this)

`components/GFX_Library_for_Arduino/src/databus/Arduino_ESP32RGBPanel.cpp` had
`.bounce_buffer_size_px` **hardcoded to `480 * 20`**, ignoring the constructor's own
`_bounce_buffer_size_px` parameter entirely (dead code sat right next to it, commented
out). That value divides evenly into 480px/800px panel widths (the only widths any board
using this class had run at) but not 1024px (9.375 scanlines) - a real ESP-IDF
`esp_lcd_rgb_panel` constraint (`bounce_buffer_size_px` must be a whole multiple of
`h_res`). Fixed to respect the real parameter; `DisplayManager.cpp`'s RGB panel
constructor call now threads `cfg.BOUNCE_BUFFER_SIZE_PX` through; `BSP_CYD_S3_8048W550.h`
and `BSP_WS_S3_Smart86.h` got explicit values preserving their exact prior (working)
behavior. Confirmed via origin history: this hardcode came from a Waveshare-modified
downstream fork of Arduino_GFX pulled in via a "Revert to Waveshare 1.6.0" commit, not
upstream Arduino_GFX and not a local mistake.

A second, smaller display fix: removed a low pulse on `EXIO_LCD_RST` that Waveshare's own
reference driver never does (their CH422G writes always assert that line high, matching
CH422G's power-on-default-high behavior) - contributed to the fix, not proven independently
load-bearing given both changes shipped together.

## Touch: everything tried, in order, all ruled out with real evidence

1. **RGB timing / LCD_RST pulse** - fixed the *display*, not touch. Touch errors identical
   before and after.
2. **`bb_captouch`'s GT911 reset block calling `pinMode`/`digitalWrite` on `iRST=-1`**
   (this board's `TP_RST` lives behind the expander, not a native pin - the only board in
   the fleet where that's true) - real bug, guarded with `if (iRST != -1)` /
   `if (iINT != -1)` in `bb_captouch.cpp`, matching the pattern already used for other
   controller types in that file. Fixed a real thing, did not touch the I2C errors (traced:
   they come from `Wire`'s own transaction layer via `I2CTest()`, not GPIO calls at all -
   `pinMode`/`digitalWrite` go through a completely different driver file and can't produce
   an `esp32-hal-i2c-ng.c` error message).
3. **Verbatim byte-for-byte port** of Waveshare's own
   `waveshare_esp32_s3_touch_reset()`/`wavesahre_rgb_lcd_bl_on()` (from their ESP-IDF
   reference, `08_lvgl_Porting/main/waveshare_rgb_lcd_port.c`) - same register addresses,
   same literal byte values, same order, same delays, at their confirmed 400kHz clock.
   **Identical errors.** This is strong evidence the bug is not in our own CH422G
   bit-manipulation logic - a real vendor implementation, ported exactly, hits the same wall.
4. **`Wire.end()` before `bb_captouch`'s own `Wire.begin()`** (theory: re-init over an
   already-active bus corrupts driver state) - **no difference at all.** Theory rejected by
   direct test, not just superseded.
5. **I2C clock speed 400kHz vs 100kHz** - tested both, properly (see build-cache gotcha
   below), both fail identically.
6. **`FleetI2C` + ESP-IDF's legacy `driver/i2c.h`** (matching what Waveshare's *entire*
   reference stack uses - touch and expander both, confirmed via source, they never touch
   Arduino `Wire` at all) - **caused a boot loop**, not a fix. `check_i2c_driver_conflict`
   aborts in a global constructor, before `setup()` ever runs - resolved via `addr2line`
   against the actual firmware.elf, not guessed. This means the conflict isn't about our own
   code's call order at all; something in the Arduino core appears to register I2C-related
   system hooks unconditionally at boot, and simply linking legacy `driver/i2c.h` alongside
   Arduino's `Wire` support trips it before any application code runs. **Reverted** - this
   board is back on the `Wire` backend (`FleetI2C`'s default), which is the known-working
   (display works, touch has the original errors) baseline.
7. **Physical scope measurement (real hardware data, not inference)** - probed SDA/SCL
   directly (OWON HDS272S, 1X probes) during a live failing transaction. Found genuinely
   slow, rounded (RC-curve-shaped) rising edges on both lines - looked consistent with weak
   pull-ups. **Added external 3.6k pull-up resistors (SDA/SCL to the board's 3.3V VOUT rail,
   confirmed 3.3V - not 5V - with a multimeter before connecting) - zero change in the
   failure.** Diagnosing *why* it made no difference turned up something more important than
   the pull-up test itself: measuring the installed 3.6k resistor **in-circuit** read ~2k,
   not 3.6k - not a bad connection (confirmed both bare-resistor and in-circuit readings were
   internally consistent), but two resistors in parallel. Solving `1/2000 = 1/3600 + 1/Rx`
   gives `Rx ≈ 4.5k` - **this board already has a real, healthy ~4.5k pull-up on SDA/SCL from
   the factory.** The "weak/relying-on-ESP32's-internal-45k-pullup-alone" theory was wrong.
   And critically: adding the 3.6k in parallel gave a *confirmed, measured* ~2k effective
   pull-up (meaningfully stronger than the factory ~4.5k) and **still made zero difference**
   to the failure - real evidence, not an invalidated experiment, against pull-up strength
   being the actual limiting factor. (The slow rise time seen on the scope may have been
   partly exaggerated by the 1X probes' own capacitive loading - worth re-checking with 10X
   probes or no probes at all if this comes up again, but the resistor swap result stands
   regardless of that caveat.)

## Bit-banged software I2C - built, and incidentally how the real bug was found

Added as a third `FleetI2C` backend (`I2C_BACKEND_BITBANG` in `platformio.ini`, alongside
the default `Wire` backend and the reverted `I2C_BACKEND_LEGACY`) - see `FleetI2C.cpp`.
Standard open-drain emulation, `delayMicroseconds()`-timed, with clock-stretch support.
Confirmed working cleanly on real hardware - zero `ESP_ERR_INVALID_STATE`-style errors, the
first fully clean I2C run all day on this board. It wasn't actually the fix for touch (see
ROOT CAUSE above - that bug existed regardless of backend), but debugging *why bit-bang
alone still didn't produce touch input* is what led directly to finding the real bug, so
it earned its place in the stack.

**Should this board go back to the `Wire` backend now that the real bug is fixed?**
Not evaluated - `Wire` + the `TMA445_ADDR` fix has never actually been tested (only bit-bang
+ the fix has been confirmed working). Bit-bang works, is confirmed on hardware, and as a
bonus structurally can't hit either I2C driver-generation category of bug again. Don't touch
it without a real reason - if someone wants to try reverting to `Wire` for simplicity's sake,
treat it as a fresh, from-scratch test, not an assumed-safe cleanup.

## A real process gotcha hit twice during this investigation

`build_cache_dir` (see `CLAUDE.md`) can silently serve a stale object for any file that only
depends on a changed BSP-header *value* through the macro-indirected include in
`bsp_loader.h` - burned real time here re-testing a value that hadn't actually changed.
When testing a BSP-header-only value (not a `.cpp` edit), `rm -rf .pio/build_cache
.pio/build/<env>` before trusting the result.

## Architecture that came out of this (keep regardless of how touch resolves)

- **`components/CH422G/`** - the hand-rolled CH422G driver, now its own component (was
  incorrectly nested inside `DisplayManager` originally - CH422G also owns `SD_CS` and
  general-purpose DIO, nothing to do with display specifically).
- **`components/FleetI2C/`** - uniform I2C access (`beginTransmission`/`write`/
  `endTransmission`/`requestFrom`/`read`/`test`) for every I2C consumer in the fleet
  (`DisplayManager`, `TouchManager`/`bb_captouch`, the boot-time I2C scan in
  `LVGL_Test_UI.cpp`, and any future sensor driver). Exactly one `#ifdef` picks the backend,
  living in `FleetI2C.cpp` only - never scattered through consuming files. `begin()` is
  idempotent (safe to call from multiple managers during startup). Every board except this
  one uses the default `Wire` backend; `I2C_BACKEND_LEGACY` exists but is **currently
  disabled** for this board (see above - caused the boot loop). A future third backend
  (bit-banged software I2C) would only need a new block inside `FleetI2C.cpp` - no changes
  to any consumer.

## TOUCH IS RESOLVED. Everything above this line is now history, not an open question.

The pull-up/scope/legacy-driver/bit-bang detour was real, useful work (fixed a real
architecture gap, produced `FleetI2C`, and incidentally is how the actual bug got found) -
but don't let it read as "the fix." The fix is the one-line `#ifndef HAS_CH422G` guard
around `TMA445_ADDR` in `bb_captouch.cpp`. If touch ever regresses on this board again,
check that guard is still there before re-opening any of the above.

## Known follow-up: display tearing

Now that touch works, tearing/rough refresh is visible on this board, worse than other RGB
boards in the fleet (e.g. 8048W550). Root-caused to `Arduino_ESP32RGBPanel` never actually
using its second framebuffer, made more visible here by this board's higher pixel count
(1024x600 vs. 8048W550's 800x480). Full technical writeup now lives in
`docs/FUTURE_IMPROVEMENTS.md` under "LVGL / display" (and the board's status is tracked in
`docs/PROJECT_STATUS.md`'s "Open bugs" section) - not duplicated here.
