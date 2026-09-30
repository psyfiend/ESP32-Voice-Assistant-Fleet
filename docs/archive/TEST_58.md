# Test sheet — #58, LVGL screenshots

> **RESULT, 2026-09-25: T1-T7 PASS on glass (owner). T8 PASS (Claude).** Merged to `main`.
> The owner's notes: "punch in the IP/screenshot and bam it's in my browser in like a second". The
> pause on the device is "less than half a second". A capture taken while dragging the Show Touches
> panel paused briefly and did not drop the panel. `CYD_S3_3248`'s heap stayed steady over many
> captures, including a 5x3 page. T8: with `ENABLE_SCREENSHOT` undefined the capture code and stb
> are gone (-9.6 KB of flash on `CYD_S3_3248`) and the server never starts. Its code stays linked
> but never runs; Phase 4 will use it.

Branch `feat/58-screenshots`. Design and every reason behind it: the header comments of
`src/UI/Screenshot.cpp` and `src/HttpServer.cpp`.

**What it does.** A board built with `-D ENABLE_SCREENSHOT` (currently every board) serves
`http://<board>/screenshot` on port 80: a PNG of exactly what is on the glass. That means the active
page, plus anything LVGL draws above it (toasts, the Show Touches panel, the FPS/CPU monitor).
**There is no password.** Anyone on your LAN can fetch it, as agreed until the Phase 4 web UI adds
authentication.

**How to fetch.** From the repo root, pictures land in `screenshots/`, which git ignores:

    python scripts/screenshot.py                    # all five boards
    python scripts/screenshot.py fleet-ws-p4-5      # one board

or open `http://fleet-ws-p4-5/screenshot` in a browser.

**Already seen by me, 2026-09-24**, not on glass. Everything else below is new information.
- `CYD_S3_3248`, final build: 23 fetches, all OK. By IP, a steady 0.6-1.0 s each with 125 ms of
  render and a 20 KB PNG, and a correct portrait image.
- On the earlier build, the first fetch of all stalled half-way while I had the board's USB serial
  open. I believe opening that port reset the board. **Not proven** (T6 covers it).
- **Unexplained:** three fetches by *hostname* in the first minute after a reflash took 10-18 s.
  They succeeded, with normal render times. Fetches by IP straight after were all fast, and so were
  later ones by hostname. My guess is name resolution or WiFi settling after boot, but I have not
  shown it. If a fetch is slow, try the IP (`python scripts/screenshot.py 192.168.0.165`) and note
  which was slow.
- `WS_P4_5`: 13 of 13 OK. Render 220 ms, encode 870 ms with the final settings, send under 0.3 s,
  75 KB, about 1.5 s end to end. PSRAM low-water mark 25.9 MB free of 28.7 MB. The image matches
  the dashboard.
- **The system layer has been seen once.** A `WS_P4_5` capture at 18:13 included the FPS/CPU monitor
  bottom-right ("30 FPS, 3% CPU") and the open deck, with the render up to 357 ms. Someone had
  switched both on at the glass. T4 is still worth doing, to confirm the picture matched.
- **Never tested by anyone:** the top layer with translucency (T3), the three boards I did not
  flash, and how the UI feels during a capture (T5).

---

## A. Every board

**T1 — Boot.** Flash and power-cycle each board.
- PASS: boots as before, and the dashboard and data are unchanged. Where you have serial, the log
  shows `[HTTP] server up on port 80, 1 route(s): /screenshot` shortly after WiFi connects.
- FAIL: a freeze, a reboot loop, or MQTT/HA not coming up. Internal RAM is the suspect on
  `CYD_S3_3248`, so note its `[Heap]` lines if you have them.

**T2 — A picture from every board.** Run `python scripts/screenshot.py`.
- PASS: five `ok` lines, and each PNG matches its glass: same page, same cards, same values
  (allowing for anything that changed between looking and fetching).
- PASS also needs all three of:
  - **Upright.** The landscape boards come out landscape, and `CYD_S3_3248` portrait.
  - **Red and blue not swapped.** Easiest to check without trusting colour: the temperature
    icons should be the same warm colour as on the glass, and the Garage area bars match.
    Compare the two side by side rather than judging from memory.
  - **Crisp.** No banding or smearing.
- FAIL: a `FAIL` line (it says why), a mirrored, rotated or sheared image, or wrong colours.

## B. As-is: what is drawn above the page

**T3 — Show Touches panel (LVGL's top layer, 80% opaque).** On `WS_P4_5`: open the Display panel
in the deck, switch **Show Touches** on, then fetch.
- PASS: the touch panel appears in the picture, in the same place. The cards behind it show
  faintly through it, as they do on the glass.
- FAIL: the panel is missing, or it is fully solid. Either means the overlay compositing is wrong.

**T4 — FPS/CPU monitor (LVGL's system layer).** Swipe up from the bottom-right corner to show the
monitor, then fetch.
- PASS: the monitor is in the picture, bottom-right.
- FAIL: it is missing.

**T4b — A toast (optional, timing is fiddly).** Start
`python scripts/screenshot.py fleet-ws-p4-5` and swipe to the other page within about a second.
The page toast lasts 1.5 s.
- PASS: some attempts catch the toast, and it sits where it does on the glass.

## C. Behaviour while capturing

**T5 — How the UI feels during a capture.** On `WS_P4_5`, keep swiping between pages while
someone runs the script, or run it in a loop.
- Expected: a hitch of about 0.2 s when the capture starts, then about a second of slower
  animation while the PNG is encoded. Touch keeps working throughout.
- FAIL: a freeze longer than about half a second, a dropped touch, or anything worse. I have not
  seen this myself; it is the one thing only the glass can judge.

**T6 — Ten in a row, then check the board is still healthy.** For each board, including
`CYD_S3_3248`, and in PowerShell:

    1..10 | % { python scripts/screenshot.py fleet-cyd-s3-3248 }

- PASS: ten `ok` lines. The board's `sensor.<device>_uptime` in HA keeps counting up (no reboot).
  MQTT and HA stay connected, with cards still updating.
- PASS also on `CYD_S3_3248`: **internal** free heap on the System panel before and after is
  within about 2 KB. A steady fall across the ten would be a leak (LESSONS.md).
- FAIL: any reboot, a leak, or the board dropping off the network.

**T7 — Two at once.** Run the script in two windows at the same moment against one board.
- PASS: both get their picture. One of them is told the board is busy and retries by itself after
  2 s.
- FAIL: a hang or a reboot.

## D. Build flag

**T8 — The flag removes it.** I will test this myself before merge: building with the flag
switched off must leave no screenshot code and no HTTP server in the firmware. You do not need to.

---

## Encoder knobs, if you are curious

`?z=5..9` (compression effort) and `?f=-1..4` (PNG row filter) are passed through, e.g.
`python scripts/screenshot.py --query "z=9" fleet-ws-p4-5`. The defaults (`z=5`, `f=0`) were
measured to be both the fastest and the smallest. The table is in `Screenshot.cpp`.
