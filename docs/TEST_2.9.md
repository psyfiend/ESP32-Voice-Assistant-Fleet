# Test sheet — 2.9 (#67), display stack

> **Step 1 (`/bench`), 2026-09-25: T1-T6 PASS (owner). T7 (an hour of CYD network) outstanding.**
> Every owner run reproduced `display-stack.md` §8.2 within ~2%. T5: both scripts finished with
> matching numbers. Because the two interleave request by request, each script's screenshots and its
> final "put it back" may describe the other's state; the board ended correctly this time, but that
> is by luck, not design. T6, Linen on `WS_P4_5`: full-screen drawing 107 -> 144 ms (+35%),
> one-card drawing 4.9 -> 7.4 ms (+50%), copy unchanged; recorded in §8.2. Linen on the CYD was not run.
> Steps 2-6 will add their own sections here.

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
