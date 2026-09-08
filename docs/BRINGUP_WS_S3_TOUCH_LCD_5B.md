# WS_S3_5B Bring-Up — Waveshare ESP32-S3-Touch-LCD-5B

**Both original problems are resolved.** This is the record of what was wrong and what the
board needs — not the investigation. The transferable lessons are in `LESSONS.md`.

Two bugs, unrelated to each other, found during the same bring-up.

---

## Touch: `bb_captouch` was probing an address CH422G owns

**Root cause.** `bb_captouch`'s chip autodetection probes a hardcoded list of candidate I2C
addresses in a fixed order before ever trying GT911. One of them, `TMA445_ADDR` (`0x24`), is
**numerically identical to CH422G's own `WR_SET` register pseudo-address** — the one this
board's `ch422gRawWrite(0x24, ...)` calls hit constantly.

CH422G genuinely and correctly ACKs at `0x24`, because it really is there. So `bb_captouch`
concluded it had found a TMA445, sent it a soft-reset-plus-security-key sequence meant for a
real TMA445 — garbage that likely disturbed CH422G's internal state — and never reached the
code that would have tried GT911's actual address.

**The fix is one line:** a `#ifndef HAS_CH422G` guard around `TMA445_ADDR` in
`bb_captouch.cpp`. **If touch ever regresses on this board, check that guard is still there
before investigating anything else.**

Worth knowing: the detection order is identical regardless of I2C backend. Wire, the legacy
driver and bit-banged software I2C all run the same `bb_captouch.cpp` logic, so swapping
backends could never have fixed it — which is why a long detour through I2C backends found
nothing. That detour was not wasted (it produced `FleetI2C`, below, and is incidentally how
the real bug surfaced) but it was not the fix.

Confirmed working: GT911, five simultaneous touch points.

---

## Display: a hardcoded bounce buffer that only divided into other boards' widths

`Arduino_ESP32RGBPanel.cpp` had `.bounce_buffer_size_px` **hardcoded to `480 * 20`**,
ignoring its own constructor parameter (the dead code sat commented out beside it).

That value divides evenly into 480px and 800px panel widths — the only widths any board using
this class had ever run at — but not into 1024px, which gives 9.375 scanlines. ESP-IDF's
`esp_lcd_rgb_panel` requires `bounce_buffer_size_px` to be a whole multiple of `h_res`.

**Fixed** to respect the real parameter. `DisplayManager` now threads
`cfg.BOUNCE_BUFFER_SIZE_PX` through, and the boards that were already working got explicit
values preserving their exact prior behaviour.

Provenance, confirmed from origin history: the hardcode came from a Waveshare-modified
downstream fork pulled in via a "Revert to Waveshare 1.6.0" commit — not upstream Arduino_GFX
and not a local mistake.

A second, smaller change shipped alongside: removing a low pulse on `EXIO_LCD_RST` that
Waveshare's own reference driver never does. Contributed to the fix but was never proven
independently load-bearing, since both changes shipped together.

---

## Architecture that came out of this — keep

- **`components/CH422G/`** — the hand-rolled CH422G driver, as its own component. It was
  originally nested inside `DisplayManager`, which was wrong: CH422G also owns `SD_CS` and
  general-purpose DIO, and has nothing display-specific about it.
- **`components/FleetI2C/`** — uniform I2C access for every consumer in the fleet
  (`DisplayManager`, `TouchManager`/`bb_captouch`, the boot-time bus scan, and any future
  sensor driver). Exactly one `#ifdef` picks the backend and it lives in `FleetI2C.cpp`
  alone, never scattered through consumers. `begin()` is idempotent, so multiple managers can
  safely call it during startup. Every board except this one uses the default `Wire` backend.
  A future bit-banged backend would need a new block in `FleetI2C.cpp` and no consumer
  changes.

---

## Known follow-up: display tearing

Now that touch works, tearing is visible on this board and worse than on other RGB boards —
root-caused to `Arduino_ESP32RGBPanel` never actually using its second framebuffer, made more
obvious here by the higher pixel count (1024x600 vs the 8048W550's 800x480).

Tracked as GitHub issue #40, with the technical writeup in `FUTURE_IMPROVEMENTS.md` under
"LVGL / Display". The real fix needs genuine buffer-swap support in the class, matching
Waveshare's own `switchFrameBufferTo()`.
