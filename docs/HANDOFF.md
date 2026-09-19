# Handoff — 2026-09-18

**Start here.** `CLAUDE.md` is the stable how-it-works. This is where we are, what will bite you,
and what to do next. Kept lean on purpose: anything that is "why we did X and not Y" now lives in
`docs/LESSONS.md`, and anything that is a design lives in `docs/design/`.

**A warning about this file.** When it paraphrases a spec, the paraphrase becomes the spec for
whoever reads it first. Say *which* document a summary is compressing, and treat vocabulary that
does not appear in the source as suspect.

---

## Where the project is

Phases 0, 1 and 2.1–2.4 are merged and tagged `v0.2.4`.

**Milestone 2.5 is DONE — merged to `main` and tagged `v0.2.5` on 2026-09-18.** Issue #16 is
closed. One page definition renders on five panels. The device boots into a dashboard built from a
data table.

**The single most urgent thing in the project is now connectivity — see the next section.** It is
not a 2.5 problem and it did not arrive with 2.5; it is the oldest open fault here and it has
graduated from "one board is annoying" to "four of five boards drop off overnight."

### Read in this order

1. `CLAUDE.md` — the HAL/BSP, the startup split, the token rules, the traps.
2. **`docs/LESSONS.md`** — read before debugging anything. Most of this week is in there.
3. `docs/design/dashboard.md` — the page spec, the grid, the two knobs, the unit policy.
4. `docs/design/card-layout.md` — **before moving anything inside a card.**
5. `docs/design/ha-websocket.md` — what HA's API actually gives us, measured. Read before #43.
6. `docs/design/cards.md` — the card spec. Its "Implementation notes" first.
7. `docs/design/tokens.md`, `docs/design/startup.md` — the design system, and boot order.
8. `docs/REFERENCE_PROJECTS.md` — "The page/view/grid back-end" before any navigation work.
9. `docs/ROADMAP.md` §7 — the milestone list.

---

## Signing off 2.5

**Issue #16 is CLOSED** — confirmed on hardware 2026-09-18. Sub-grid units, explicit placement and
the validator were all observed: "Obeys" lands at unit 2,2, "Ignores" is rejected at 40,0 and
flowed to 1,1, and on the 4B boards with a column added Lamp 1 and Lamp 2 each span 3 units.

One `FLEET_PAGE` now renders on **five** panels — 1280x800, 1024x600, 720x720, 480x480 and 320x480
portrait — with priority degradation trimming the small ones. The acceptance criterion asked for
three.

**What remains before tagging `v0.2.5`: an overnight soak, and nothing else.**

Every fault this week was time-dependent, so a milestone that has not survived a night has not been
tested. Watch for: boards staying in Home Assistant, `[Mqtt] Disconnected` lines (which now carry
`state=`, elapsed and heap), and `[Heap]` trending flat rather than down.

Flashed and running this build: `CYD_S3_3248` (COM10), `WS_P4_5` (COM15), `WS_S3_4B` (COM8),
`WS_P4_4B` (COM7). `WS_P4_7B` has a build waiting but is in an enclosure that blocks a USB port —
which is the argument for OTA, Phase 5.

### Two things found at the very end, both fixed, one with a visible cost

**`clip_corner` in `HDR_BAR` froze `WS_P4_5` on boot.** A 482 px two-cell card asked for a
32,776-byte layer buffer out of the `lv_mem` pool and failed, with 80 KB of system heap still free.
It only hit that board because **a layer is sized by the object's WIDTH**, and the P4_5 has the
fleet's widest cells: 1280 px across only 5 columns gives 230 px cells, so its two-cell cards are
the largest in the fleet. The other boards asked for smaller layers that fitted. It became fatal
rather than occasional the moment `HDR_BAR` became the default header mode, because then every card
wants one.

**The cost, and it is visible:** the band no longer gets masked by the card's rounded corners, so
its corners sit slightly outside them. Cosmetic, deliberate, and the honest price of not allocating
a layer per card. **Resolve it properly with the slot rework at 2.8** — the band wants to be part
of the card's own background rather than a child that has to be clipped.

**A pin lost to the flow.** `pin 2,2 rejected (occupied)` where 2,2 was perfectly valid: an earlier
card had flowed into it first. Placement is two passes now, pinned then flowed. Until this was
fixed the pin test had never actually proved anything.


---

## IN PROGRESS: the fleet cannot stay online overnight (#49)

**Status 2026-09-18: a fix is written and flashed, and NOTHING about it is verified yet.** It is
on `fix/49-link-liveness`, it builds on all three flashed boards, and it has not yet seen the
fault. Read "What the fix does" below before touching it, and "How to test it" before believing
it.


**Soak result, 2026-09-18 into 09-19.** Four of five boards were left running the same build.

| Board | Result |
|---|---|
| `CYD_S3_3248`, `WS_S3_4B` | still receiving MQTT in the morning |
| `WS_P4_7B` (on the older `v0.2.4.23`) | still receiving MQTT |
| `WS_P4_4B` | **went unavailable after ~5-6 hours** |
| `WS_P4_5` | **went unavailable after ~3-4 hours** |

Every board still responds to touch, animates, and runs its UI. Nothing is frozen. **This is purely
a network fault**, and it has now been seen on `WS_P4_5`, `WS_P4_4B` and `WS_P4_7B` — so it is not
one board and not a 2.5 regression. `WS_S3_4B` is the only board never to have shown it.

### What the serial says, and it is conclusive

```
[Mqtt] Disconnected from broker. state=-4 after 6080715 ms, heap 73548, wifi up
[Mqtt:debug] connect failed raw=-2 -> broker unreachable      (x dozens, forever)
```

- **101 minutes of healthy session**, then a keepalive timeout.
- **73 KB of free heap.** Not memory. All the memory faults fixed during 2.5 are genuinely fixed.
- **`wifi up`** — but that is only `WiFi.status() == WL_CONNECTED`, which is exactly the value that
  lies.
- **`raw=-2` is `MQTT_CONNECT_FAILED`** — the TCP connect never completes. The broker is not
  refusing anything; there is no path to it.
- **Not one `[Conn]` line appears in the rest of the log.** The connectivity layer never notices,
  never re-associates, never falls back to AP. It has nothing to react to, because the only thing
  it polls still claims success.

The owner's summary, and it is correct: *"the device never realizes that wifi has become
disconnected. The MQTT reconnects are the obvious result of retrying when there is no network."*

### What the fix does (commit d11fb40, `fix/49-link-liveness`)

Recovery first, root cause second. A recovery layer works whatever the cause, and it instruments
the board so the next soak ANSWERS the cause instead of us guessing at it a fifth time.

Three layers, and the unifying rule is that **none of them asks the driver how it is doing.**
`WiFi.status()` appears nowhere in any of them. Every signal is either a round trip that left the
board, or a report from a layer above about something it could not reach.

1. **RSSI is re-read on a timer and carries an age.** Past `RSSI_STALE_MS` the band is `NONE` and
   the glyph stops claiming a strength. A read that returns 0 is logged, not stored - and on P4
   that call is an RPC to the C6, so a FAILING read would itself be a liveness signal. Whether it
   fails during the fault is unmeasured; the logging is there to find out.
2. **`LinkHealth`** - a second axis beside `ConnState`, because one enum cannot express "the driver
   says connected and it is wrong". Evidence: an ICMP gateway probe, plus MQTT's verdict relayed
   down by `SystemCore`. `Fleet_MQTT` still knows nothing about what carries it (ROADMAP Q9); it
   only exposes `consecutiveEnvFailures()` and `msSinceLastConnected()`, and `SystemCore` - which
   owns both objects - is what joins them up.
3. **A three-rung recovery ladder**: re-associate, cycle the radio, hand back to the state machine
   so AP fallback and the normal retry path engage. That last rung is what was missing entirely.

**Two guards matter more than the feature itself, and must not be removed casually:**

- **A probe that has NEVER been answered is not evidence.** Plenty of routers drop ICMP by policy.
  Without this guard the fix would be far worse than the fault - every board would convict its own
  healthy link within minutes and cycle its radio forever. One reply latches the instrument as
  trustworthy; until then failures are counted, logged, and draw no conclusion.
- **Each completed ladder widens the cool-off.** A board whose problem is upstream settles into
  checking occasionally rather than thrashing.

**The probe is demand-driven, and that is a MEMORY decision.** A held MQTT session is continuous
evidence - PubSubClient's keepalive is a real round trip, which is exactly how the original fault
announced itself - so a healthy board sends no probes at all. That keeps a 2.5 KB task stack out of
internal RAM on `CYD_S3_3248`, the only QSPI board and so the only one whose LVGL buffers are also
internal. Static cost on that board: +120 bytes.

`esp_ping` needed no new dependency. Verified before designing to it: `esp_ping_new_session` is in
`liblwip.a` and `lwip` is in `flags/ld_libs` for both `esp32p4_es` and `esp32s3` - the same check
that found `esp_websocket_client` ABSENT during the #43 spike.

### How to test it, and why the soak is second

The owner can block a single board at the router, which turns a 3-6 hour wait into a two-minute
iteration. **Bench first, soak second** - the soak should CONFIRM the fix, not discover whether it
works.

What to watch for, in order:

1. `[Conn] Link health healthy -> suspect (...)` within ~2 probe intervals of the block.
2. `[Conn] Link health suspect -> DEAD (gateway unreachable)` about 90 s later.
3. `[Conn] RECOVERY 1/3`, then `2/3`, then `3/3` at 30 s intervals.
4. The WiFi glyph dropping to a bare red stalk with a `!` badge WHILE the board still thinks it is
   associated. That specific frame is the whole point of the change.
5. On unblocking: `[Conn] Online:` and health back to healthy.

**The failure mode to watch for is the opposite one:** a healthy board convicting itself. If any
board reports `LINK_DEAD` while it is demonstrably reachable, the guards above are the first thing
to read. `Dump Config` prints the probe's own status, including whether it has ever been answered.

### The "#41 is the lead" theory does NOT hold up - checked 2026-09-18

An earlier version of this section named the P4 boot-time RPC warning, and #41 behind it, as the
lead worth chasing first. **It does not survive contact with the code**, and the next reader should
not spend a night on it.

Every NVS write in this firmware is a CONFIGURATION event - the complete list is
`ConnectivityManager.cpp:247` (one-shot migration), `:280`, `:291-293` (`setStationCredentials`),
`:331` (`markProven`, once ever), `:357-358` and `:376-377`. Nothing writes NVS on a timer, per
message or per reconnect, and every affected board is already `proven`.

**So no NVS writes occur at all during the 3-6 hour window in which the fault appears.** A
mechanism that only fires on NVS writes cannot explain it.

#41 stays open and stays right - as a constraint on **Phase 4**, when the settings UI starts
writing NVS at runtime on four P4 boards. It is simply not the cause here.

The boot RPC timeout is likewise a boot-time event. Keep it as weak evidence that the host-to-C6
channel is not perfectly healthy; do not build a theory on it.

Caveat still worth keeping: `WS_P4_7B` has also dropped, and the S3 boards have not been soaked as
long. Do not over-fit to "P4 only" on a sample of one night.

### A sharper statement of the RSSI symptom

"Stale RSSI" undersells it. `captureLinkInfo()` (`ConnectivityManager.cpp:810`) was called from
exactly one place - the `GOT_IP` handler - so `_rssi` was written **once per association and never
again**. The header was not showing a cached reading going stale; it was showing a value nobody
ever asked for a second time.

### The instruments are already in place

- Disconnects print `state=`, how long the socket held, free heap and WiFi state.
- `SystemCore::heapMark()` traces internal heap through startup, every dashboard rebuild and every
  MQTT reconnect.
- The broker's own log is the fastest route to the truth and settled the last fault in one step —
  HA → Settings → Add-ons → Mosquitto → Log.

### The three faults that ARE fixed

Do not re-investigate these; they are separate and done. Full reasoning in `LESSONS.md`.

| | |
|---|---|
| Duplicate MQTT subscriptions filling the table | `addSub()` dedupes by topic |
| A leaked TCP socket per reconnect — the broker was reaping ghosts | `_client.disconnect()` on all three exits |
| Internal heap starvation, twice — `LV_MEM_SIZE` 192 KB on P4, and `DOUBLE_BUFFERING` ignored on CYD_S3_3248 | both reverted/honoured |


## What is next, after 2.5

The running order was agreed on 2026-09-15 and it overrides the milestone numbering. Reasoning is
in `ROADMAP.md`; the short version is that **entity supply, not card features, is what limits this
project.**

0. **#49 — connectivity.** Not optional and not negotiable against the rest: a panel that leaves
   Home Assistant every few hours is not a dashboard, and #43 puts MORE weight on the same link.
   See the section above. **A fix is written and flashed, unverified.**
1. **#50 — the System panel rework.** Moved ahead of #43 by the owner on 2026-09-18: *"relatively
   minor task but will make a big improvement in the visual experience."* It was specified at the
   end of 2.5 and lived only in this file's postponed table until it got an issue; the spec is now
   in #50 and the table below points there instead of carrying it.
2. **#43 — Home Assistant over the websocket.** Design-and-build, not research: the API has been
   measured against the owner's live instance. `docs/design/ha-websocket.md` has the numbers and
   one finding that changes the architecture (`subscribe_trigger`, never `subscribe_events`).
3. **#44 — outbound commands** through the same client. This is where the panel stops being a
   display and becomes an interface. `call_service` is deliberately untested — running it turns on
   a light in the owner's house.
4. **2.6 tileview / 2.8 slots**, once there are enough entities to need pages.
5. **3.1 + 3.3 — the build sheet**, with a schema informed by what HA actually gives.

---

## Deliberately postponed — do not rediscover these

Everything here was raised, discussed and consciously deferred.

| What | Where it goes |
|---|---|
| **System panel rework** — fully specified, not started. Spec now lives in **#50**, not here | NEXT, after #49 |
| **The system header bar needs its OWN colour**, not the scheme's. Paper makes it unreadable | 2.8 |
| **Corner icon = the DOMAIN; the hero = the specific fixture** | 2.7 |
| **State-dependent hero glyphs** (`motion-sensor-off`, `garage-open`). HA already ships these in `attributes.icon` — see `ha-websocket.md` §5 | 2.7 |
| **A `door` card type**; a `LightCard` that handles dimming/RGB/colour-temp | 2.7 |
| **Icons look undersized on large cards** — both faces are picked by density alone, never by cell size. Fixable; costs flash | 2.7 |
| **Per-card full-screen detail page**, long-press, background dimmed, deck headers sliding up | 2.7 / 3.2 / 4.4 |
| **Manila-folder tag shape** — tag's bottom corners curving outward | low priority |
| **Card press feedback** — shrink on hold | low priority |
| **`clip_corner` in `HDR_BAR`** is a standing layer-buffer liability on wide cards | 2.8, with the slot rework |
| **Arduino_GFX only uses one draw buffer**, so buffer two may be dead weight on all seven other boards. Confirm against its source first | **2.9**, and now a concrete thing 2.9 buys |
| **Auto-hiding system header**, swipe down to reveal. Overlay it; do not re-lay-out the grid | 2.6 |
| **Priority's vocabulary** — four bands was my choice, never ratified. Worth revisiting before the build-sheet schema freezes | 3.1 |
| **OTA** — the 7B is in an enclosure that blocks a USB port. This is the argument | Phase 5 |
| Irrigation card; thermostat card; first-boot AP + web config; HA-facing device entities (brightness, sleep, toast, battery) | `FUTURE_IMPROVEMENTS.md` |

### The system panel rework, specified

Asked for at the end of the 2.5 session and **deliberately declined** - it is a new page, a layout
rewrite, width and anchoring work and a dynamic height, and starting it on an exhausted context
would have left it half-done. The owner agreed. One piece was done: **the log moved to its own
page** (`LogPage`), which is the likely cause of the panel's choppy animation.

**The full spec now lives in GitHub issue #50** and is not duplicated here. That is deliberate:
this file having been the only home for a fully-specified piece of work is exactly how it nearly
got rediscovered instead of built, and a paraphrase in a handoff becomes the spec for whoever
reads it first (see `LESSONS.md`, "A paraphrase can outrank the spec").

One correction worth carrying, because the opposite was assumed once in this session: the panel is
`lv_pct(100)` today (`Panel_System.cpp:108`), so the rework makes it **narrower**. A layer buffer
is sized by object WIDTH, so the resize RELIEVES the `clip_corner` liability rather than worsening
it. Do not add `clip_corner` here, but do not fear the resize.

---

## Things that will bite you

**Build from PowerShell, not Git Bash.** pioarduino rejects MSYS shells.

**`pio run` with no `-e` builds ONE environment.** The fleet is eight explicit `-e` flags.

**Clear `.pio/build_cache` after editing any BSP header or `lv_conf.h`.** `pio run -t clean` does
not clear it, and the macro-indirected `#include BSP_HEADER` defeats its dependency scanner.

**There is exactly one `lv_conf.h`** and it is `include/lvgl/lv_conf.h`. Settled by experiment; see
`CLAUDE.md`.

**Do not run two `pio` invocations at once** — they contend for `.pio/build`.

**Never put a backslash escape in text a script writes.** Six occurrences in two sessions. See
`LESSONS.md`.

**Anything drawn on a panel stays ASCII, except `°`.**

**Verify from outside the device.** The MQTT fault was solved in one step by reading the broker's
own log after four rounds of device-side guessing.

---

## What is measured vs. what is assumed

Say "verified at `<path>:<line>`" or say "I believe".

| Measured | |
|---|---|
| Card cost | ~2.8 KB in `lv_mem`; a 13-card dashboard is ~5 KB of *internal heap* |
| `lv_mem` pool | 128 KB static array in internal DRAM, fleet-wide. 192 KB on P4 links and breaks the network |
| Internal heap after boot | `CYD_S3_3248` ~10 KB before the draw-buffer fix; the WiFi driver alone takes ~61 KB |
| One font face | ~96 KB of flash |
| HA `/api/states` | 742 KB across 1,662 entities. The area registry is 3.6 KB |
| HA event rate | 12.2/s and 913 KB/min on `subscribe_events`; **zero** for the same entities via `subscribe_trigger` |
| Full-card threshold, 7B | 129 px row height (bar/tag), 109 px (none) |

**Still assumed:** `WS_P4_4B` unflashed this milestone; `CYD_S3_8048`, `CYD_P4_1060`, `WS_S3_5B`
never flashed since the type scale changed; `TouchManager::mapCoordinates()`'s `WS_P4_7B` special
case remains undocumented and untested against the alternative.

---

## How to work with this owner

He is a hobbyist and an ESP32 enthusiast, not a professional developer, and explicit about that —
but he reads code, spots real bugs, and has caught several this week that were not obvious. **Treat
his instincts as data.** The broker log, the "is it holding cards in memory" question and the
column-count regression were all his.

**What works:**

- **Show, don't spec.** Build something he can react to. He knows what he likes when he sees it.
- **Plain language, not metaphor.** His words: "sometimes I get a little lost in the slang." For
  each change say what it does, why, what it affects downstream, and what he would see if it were
  wrong. Name the takeaway explicitly.
- **Give a recommendation, not a menu.**
- **Own mistakes plainly and move on.** Several changes this week were regressions of mine. Saying
  so directly, once, and fixing them is the expected behaviour.
- **He flashes fast** and will often hand you a COM port mid-turn.
- **Push back on scope when it is real.** He asked for a large batch at the end of an exhausted
  context; declining most of it with a reason was welcomed, not resented.

**What to avoid:**

- Don't say "we should wait until phase X" as a reflex.
- Don't trust a browser mock. The bench narrows the options; the glass decides.
- Don't commit straight to `main`. Feature branch, then merge.
- **Don't guess a fourth time.** When a theory needs another iteration, instrument it or ask the
  far end.

**Versioning:** `A.B.C.D`, where **C is the roadmap phase**. Tag on `main` at merge, never during
development. A dirty tree appends `+dirty`, which is working correctly.
