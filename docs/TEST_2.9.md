# Test sheet — 2.9 (#67), display stack

> **Step 1 (`/bench`), 2026-09-25: T1-T6 PASS (owner). Merged to `main` the same day on the owner's
> word; T7 (the CYD's network over time) is being watched while step 2 is worked on.**
> Every owner run reproduced `display-stack.md` §8.2 within ~2%. T5: both scripts finished with
> matching numbers. Because the two interleave request by request, each script's screenshots and its
> final "put it back" may describe the other's state; the board ended correctly this time, but that
> is by luck, not design. T6, Linen on `WS_P4_5`: full-screen drawing 107 -> 144 ms (+35%),
> one-card drawing 4.9 -> 7.4 ms (+50%), copy unchanged; recorded in §8.2. Linen on the CYD was not run.
> Steps 2-6 will add their own sections here.

---

## Step 2 — `WS_P4_5` on `esp_lcd` (branch `feat/67-step2-p45`)

> **Awaiting the owner's glass checks, 2026-09-26.** Built, flashed and tested by Claude on
> `WS_P4_5` only; no other board runs any of it (they build unchanged - `CYD_S3_3248` compiled).

**What changed.** `WS_P4_5` no longer uses Arduino_GFX. Its panel is brought up on raw `esp_lcd`
(`components/Fleet_Display/`), it has **three** frame buffers instead of one, and the P4's PPA
rotates each strip LVGL draws into the buffer being built, while LVGL draws the next strip. A
finished buffer is handed to the panel whole, at the start of its next frame. The one line that
does it is `-D DISPLAY_ESPLCD` in `WS_P4_5`'s environment; **deleting that line puts the board
back on Arduino_GFX exactly as before.**

**Already seen by me, not on glass:**
- Full-screen redraw **162.9 -> 90.1 ms (-45%)**, a ceiling of 11 frames/s instead of 6. Drawing
  itself 95.7 -> 75.5 ms: the CPU no longer spends its cache on copying. `display-stack.md` §8.5.
- **The panel's buffer matches LVGL's own picture to the pixel.** New: `/screenshot?fb=1` returns
  the frame buffer the panel is showing (portrait, as wired). Compared pixel by pixel with the
  normal screenshot: identical on both pages, with the deck, after card redraws and page changes.
  The one mismatch seen was the Uptime and Signal values changing between the two captures.
- That comparison also proves the orientation is **the same as Arduino_GFX's** rotation 1.
- **30-minute soak for the known PPA freeze: PASS.** 382 `/bench` runs back to back - full
  screen, single card (odd block sizes, which is what that freeze depends on), both pages, deck
  open and shut - 19,100 frames, no freeze, no reboot (uptime 176 -> 1969 s). Worst single frame
  298.8 ms, not explained: probably a full-frame repair coinciding with network work, but unproven.
- **Never seen by anyone:** the glass itself. Everything above reads memory; only your eyes can
  confirm the panel shows it upright, in the right colours, without tearing.

**Known cost:** the first two or three small updates after a whole-screen change (a page swipe)
each pay one full-frame copy (~33 ms): every buffer is two frames behind, and it is brought up to
date before reuse. After that a card update costs about what it did before.

**S1 — First light.** Power-cycle `WS_P4_5`.
- PASS: the dashboard comes up **upright, the same way round as before**, colours as before (a
  red/blue swap would show on the orange cards and the coloured area tags).
- FAIL: sideways, upside down, mirrored, wrong colours, noise, or a black screen. Note what you see
  at boot, before the dashboard appears, too (a moment of black is expected).

**S2 — Touch.** Tap a card in each of the four corners, and swipe pages both ways.
- PASS: every tap lands on the card under your finger; swipes go the right way.
- FAIL: taps land elsewhere (touch mapping is unchanged, so this would be a surprise worth a photo).

**S3 — Tearing and smoothness.** Swipe pages quickly, open and close the drawer, open the deck
panels, drag the Touch Points panel around.
- PASS: no torn frames (a horizontal or vertical seam where two frames meet), no flicker, no stale
  fragments left behind where something moved.
- Also tell me: does it *feel* faster than before? Page swipes and the drawer are where it should.

**S4 — The rest of the display.** Brightness slider; switch to Linen and back; take a normal
`python scripts/screenshot.py fleet-ws-p4-5`.
- PASS: brightness changes, the scheme switch repaints everything, screenshots look right.

**S5 — Overnight.** Leave it running, then `python scripts/bench.py fleet-ws-p4-5`.
- PASS: the uptime line shows no reboot, the numbers are near 90 ms full / under 10 ms card.
- FAIL: a reboot, or `/bench` answering "the UI thread did not respond" (that is what a PPA freeze
  would look like: the screen stops changing, the network keeps working).

> **Owner, 2026-09-26, both boards on `0.2.7.42+dirty` side by side: every functional test signed
> off** - swiping, taps, panels, sliders "all work the way they should" (S1-S4, S6/S7 here and
> T3-1..T3-4 below). The swap: "visually the two devices look extremely similar, maybe even the
> same"; whether the P4_5 stutter is still there is now hard to say. **Two honest observations,
> both expected rather than bugs:** a page swipe hesitates slightly longer on P4_5 than on the 4B,
> and the perf overlay's CPU runs a little higher on P4_5 during swipes and swaps. P4_5 draws 1.78x
> the 4B's pixels (921,600 vs 518,400): a full frame is 85 ms against 52 (`display-stack.md`
> §8.6-8.7), and drawing is now the whole of the difference. The remaining lever is drawing less,
> not the flush. The 4B was still at rotation 2 for these checks; the owner then set it to 0.

### The deck-panel stutter (branch `feat/67-bench-anim`, 2026-09-26, laptop weekend)

**What changed.** The owner's stutter - one deck panel opening while the other closes - was
measured on P4_5 with the new `/bench?what=anim` and traced to the esp_lcd flush's *repair* (bringing
a buffer up to date before drawing into it), which was re-copying almost everything each frame. It
now skips what the frame redraws anyway. Numbers: `display-stack.md` §8.6. In short: 6-7 frames per
swap with gaps up to 82 ms became 9 frames, gaps at most 44 ms, none late - the same as the 4B,
which you called smooth.

**Already seen by me, not on glass:** the anim numbers above, four swaps at a time, repeated; the
panel's frame buffer matching LVGL's render pixel for pixel in five different states (only the
blinking MQTT icon differed, which is expected while MQTT is unreachable); the standard matrix a
little faster than before. **Never seen by anyone:** how it looks.

**S6 — The swap.** Show the deck; open Audio; tap Display; tap Audio; repeat several times, quickly
and slowly. Compare with the 4B beside it.
- PASS: the swap looks as smooth as the 4B's, and no fragment of a panel is left behind when the
  animation ends (look at the cards just above the deck).
- FAIL: still a visible hitch, or a stale strip or corner anywhere - note where.

**S7 — Everything else still clean.** Swipe pages, open and close the drawer, drag the Touch Points
panel, change scheme.
- PASS: no stale fragments anywhere, no tearing (the change is in what gets copied between the
  three buffers, so a mistake would show as leftovers from two frames ago).

**For measuring yourself:** `python scripts/bench.py --anim fleet-ws-p4-5` (add `--frames` for every
frame; it saves the JSON in `bench/`). The panels are seen swapping while it runs.

---

## Step 3 — `WS_P4_4B` on `esp_lcd` (branch `feat/67-bench-anim`, 2026-09-26)

**What changed.** The 4B now runs the same esp_lcd path as P4_5: three frame buffers, the PPA
turning each strip (180 degrees here) into place, Waveshare's own ST7703 driver fed our BSP's init
sequence and timings. **Rotation is still 2**, so it should look exactly as before. `-D
DISPLAY_ESPLCD` in its environment; delete that line to go back.

**Already seen by me, not on glass:** full-screen redraw 87 -> 52 ms; a deck-panel swap frame 31 ->
18 ms, every frame on LVGL's 33 ms schedule; the frame buffer matching LVGL's render pixel for pixel
in five states (only the blinking MQTT icon differed). `display-stack.md` §8.7. **Never seen by
anyone:** the glass.

**T3-1 — First light.** Power-cycle the 4B.
- PASS: the dashboard comes up the same way round as before (USB port on the left), right colours.
- FAIL: upside down, mirrored, colours swapped (watch the orange cards), noise, black, or a
  flicker/roll the old build did not have (that would point at the panel timing).

**T3-2 — Touch.** Tap a card in each corner; swipe pages both ways.
- PASS: taps land under your finger.

**T3-3 — Smoothness and leftovers.** Swap the deck panels, swipe pages, open the drawer, drag the
Touch Points panel.
- PASS: no torn frames, no fragments left behind; at least as smooth as it was.

**T3-4 — Brightness, scheme, screenshot.** Brightness slider; Linen and back; `python
scripts/screenshot.py <4B's address>`.
- PASS: all as before.

> **T3-1..T3-4: PASS (owner, 2026-09-26)**, at rotation 2. The owner then set rotation 0.

> **Soak, 2026-09-26, final firmware on both boards (`b5e3587`+, DEBUG_CARDS off, 4B rotation 0):
> PASS.** 40 minutes of `/bench` cycling full-screen, one-card (odd block sizes - the PPA freeze's
> trigger), deck-swap and page-change runs: 392 runs each, 0 failures, 0 reboots. Worst single
> frame 108 ms on P4_5, 74 ms on the 4B (full-screen redraws, as expected). **All eight
> environments compile** with every change on the branch (39 min).

### After the sweep (2026-09-26, owner away) - what is flashed now, and what needs eyes

Both boards: `DEBUG_CARDS` off (page swipes ~90 ms quicker, `display-stack.md` §8.9). 4B: rotation
0, 50-line buffers stated explicitly, our timing. Nothing else changed in behaviour.

**T3-5 — The 4B at rotation 0.** Power-cycle. Tap the four corners; swipe both ways.
- PASS: upright with the USB port on the other side from before; every tap lands under the finger.
  Touch follows `ROTATION` in `TouchManager` (0 = the raw coordinates), untested by anyone at 0.
- FAIL: taps mirrored (left/right or top/bottom swapped) - a photo and which corner.

**T3-6 — Page swipes.** Swipe back and forth on both boards, side by side.
- Expect: both a little quicker than this morning; P4_5 still a little behind the 4B (it builds and
  draws a bigger page - §8.9). Tell me whether it is still noticeable.

**Optional A/B, each a rebuild I can do on request:**
- **Vendor timing on the 4B** (38 MHz, 59.2 Hz) against ours (46 MHz, 66.7 Hz). Measured identical
  in speed (§8.8); the question is only whether either looks better - flicker, colour, a shimmer.
- **16 ms refresh on the 4B** (`-D FLEET_LV_REFR_PERIOD=16`): ~48 frames/s for the deck panels
  instead of 30. Does it look smoother? Nothing else should change.

---

Branch `feat/67-bench`. The plan is `docs/design/display-stack.md`; the numbers are its §8; what
every JSON field means is the header comment of `src/UI/Bench.cpp`.

**What it does.** A board built with `-D ENABLE_BENCH` (every board, beside `ENABLE_SCREENSHOT`)
serves `http://<board>/bench`. It redraws the screen a set number of times and reports, in JSON, how
long LVGL spent **drawing** and how long it spent **getting the picture onto the panel**. **No
password**, like `/screenshot`. **The screen freezes while it measures**: about 4 s on
`CYD_S3_3248`, about 3.5 s on `WS_P4_5`, plus 2.5 s of settling whenever it had to change page.

**How to run.** From the repo root; results and a screenshot of each scenario land in `bench/`,
which git ignores:

    python scripts/bench.py                                # both dev boards
    python scripts/bench.py fleet-ws-p4-5                  # one board
    python scripts/bench.py --label linen fleet-ws-p4-5    # tag the saved files

or open `http://fleet-ws-p4-5/bench?n=10` in a browser for one full-screen reading.

**Also changed, fleet-wide:** LVGL's draw buffers are now allocated with an explicit alignment
(`LVGL_Startup.cpp`, `allocDrawBuf`). At today's settings this is the same 4-byte alignment as
before; it matters only once `LV_DRAW_BUF_ALIGN` is 64 (the PPA experiment, and step 2).

**Already seen by me, 2026-09-25, not on glass:**
- Both boards: four full matrices each (baseline, then after reflashing), all `ok`, agreeing within
  ~1%. Every screenshot shows the scenario it claims: page 0, page 1, page 0 with the deck.
- Each board was back on its starting page afterwards (checked through the reply's `was` field and
  the next run's starting state, not by looking).
- `WS_P4_5` with `LV_USE_PPA 1`: drew correctly in screenshots, 11% slower. **Nobody has looked at
  that build on the glass**, and it is no longer flashed, so there is nothing to test there now.
- **One unexplained failure:** the very first `CYD_S3_3248` run timed out (60 s) on its first page
  change. The board had not rebooted and was fine afterwards; every run since, 8 page changes, has
  worked. A screenshot straight after took 13 s, where 1 s is normal. That is the same slow-just-after-
  flashing pattern `TEST_58.md` recorded and never explained. The script allows 60 s per request;
  `--timeout 120` if it recurs.
- **Never tested by anyone:** touching the screen during a run (T4), two runs at once (T5), Linen
  (T6), and the other six boards.

---

## Speed optimisation (`-O2`), on `CYD_S3_3248` and `WS_P4_5`

Branch `exp/67-o2`. Both dev boards are now compiled for speed instead of size: 9-14% faster full
redraws, 22% faster one-card updates on the CYD (`display-stack.md` §8.3). Nothing is supposed to
look or behave differently except speed. Compiling for speed can expose latent bugs that size
optimisation happened to hide, which is why these tests exist.

**Already seen by me:** both boards ran four full `/bench` matrices without a reboot (the reply now
carries `uptime_s` and `reset_reason`, and `bench.py` prints them), and screenshots are correct.
**One dropped connection on the CYD** mid-matrix, before `uptime_s` existed, so whether it rebooted
is unknown; it answered normally straight after.

> **O1: PASS (owner, 2026-09-25)** - "they do seem a touch faster", placebo not ruled out. Found
> while testing, not caused by `-O2` as far as anyone knows: **#68**, the toast pushed flush-right.
> **O2: PASS (owner, 2026-09-26)** - overnight on both dev boards, `bench.py` showed no reboot and
> WiFi live. Also closes step 1's T7. `-O2` merged fleet-wide the same morning; all eight had
> already built clean with it.

**O1 — Feel.** Use both boards normally for a few minutes: swipe pages, open the drawer and the
deck, toggle a light.
- PASS: everything works as before, and the CYD feels at least no slower. Faster is the hope.
- FAIL: anything that worked before and does not now, however small.

**O2 — Overnight.** Leave both boards running overnight, then run `python scripts/bench.py`.
- PASS: the `uptime` line shows a number of seconds that covers the night (no reboot), and HA/MQTT
  cards are still updating.
- FAIL: a reboot (uptime small, reset reason 4 = panic, 5/6/7 = watchdog), or the board off HA.
  This one also serves as step 1's T7.

---

## Step 1 — `/bench`, on `CYD_S3_3248` and `WS_P4_5`

**T1 — Boot.** Power-cycle both boards.
- PASS: boots as before, dashboard and data unchanged. Where you have serial (`WS_P4_5`), the log
  shows `[HTTP] server up on port 80, 2 route(s): /screenshot /bench`.
- FAIL: a freeze at boot (that would be the draw-buffer alignment), a reboot loop, or HA/MQTT not
  coming up.

**T2 — Numbers from both boards.** Run `python scripts/bench.py`.
- PASS: six lines per board, no `FAIL`, and the `full` and `card` totals within about 10% of
  `display-stack.md` §8.2 (P4_5: ~179 and ~8 ms; CYD: ~222 and ~65 ms).
- FAIL: a `FAIL` line, or a number wildly different. Note which scenario.

**T3 — Watch the glass during T2.** Stand in front of each board while the script runs.
- PASS: the screen freezes briefly, switches to page 2, freezes, comes back to page 1, opens the
  deck, freezes, and ends **on the page and deck it started on**. The usual page toast appears on
  each switch.
- Then start from a different state: swipe to page 2 and open the deck, run it again.
- PASS: it ends on page 2 with the deck open.
- FAIL: it ends anywhere else, a toast or panel stays stuck, or the screen shows garbage while frozen.

**T4 — Touch during a run.** Run `python scripts/bench.py --n 60 fleet-cyd-s3-3248` (a longer
freeze) and tap and swipe the screen while it is frozen.
- PASS: no crash or reboot. A tap may be lost or acted on late; either is acceptable.
- FAIL: a reboot, a freeze that does not end, or the board left on the wrong page afterwards.

**T5 — Two at once.** In two terminals, start `python scripts/bench.py fleet-ws-p4-5` in both, a
second apart.
- PASS: both finish. One of them will have waited: the board answers `503` and the script retries.
- FAIL: a crash, or either script ends with `FAIL`.

**T6 — Linen (a measurement, not pass/fail).** On `WS_P4_5`, put the House page (`p0` in the script's
table) on Linen from the System drawer, then run `python scripts/bench.py --label linen fleet-ws-p4-5`. The `scheme` column
shows what each line measured. Send me the output, or just tell me it ran; I can read the saved JSON.
Do the same on the CYD if you are willing. This is the number the migration most needs next and
the one I cannot set up myself: the scheme is a setting on the glass.

**T7 — The CYD keeps its network.** After the runs, leave `CYD_S3_3248` for an hour.
- PASS: HA and MQTT cards keep updating (outdoor sensors being STALE overnight is normal).
- FAIL: everything goes STALE or the board drops off HA. The bench's replies show
  `internal_before` at 21-26 KB on this board, which is under the 40 KB line in `LESSONS.md`. I
  believe that was already so before this branch (the bench adds 0.2 KB of internal RAM, now that
  its reply buffer is in PSRAM), but no earlier figure was written down to prove it.
