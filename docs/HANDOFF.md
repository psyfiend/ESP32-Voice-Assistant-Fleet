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

**Milestone 2.5 is code-complete on `feat/2.5-page-grid-engine` and awaiting sign-off.** All eight
environments build. The device boots into a dashboard rendered from a data table, on three very
different panels.

**To close 2.5, two things:**

1. **Confirm issue #16's last two paths on hardware** — see "Signing off 2.5" below. Everything
   else in #16 is done and observed.
2. **An overnight soak.** Every fault this week was time-dependent, so a milestone that has not
   survived a night has not been tested.

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

**What is already confirmed on glass:** the dashboard as boot screen, the page built from
`PageSpec`, priority degradation (CYD_S3_3248 drops its three `PRI_DEBUG` cards first, correctly),
the row-count-from-cards rule, the Col/Row/Deck/Header/Scheme/Bar knobs, and the same page
definition rendering on 1280×800, 1024×600 and 320×480 portrait.

**What has not been seen yet — both now instrumented in `Dashboard_Fleet.h`:**

| Path | How to confirm |
|---|---|
| **Sub-grid units** | Lamp 1 and Lamp 2 are `prefSpanX = 3` — three units, i.e. **1.5 cells each**. Side by side they occupy exactly 3 cells. On a 2-column board (4 units) the second wraps via `minSpanX` |
| **Explicit placement** | "Obeys" is pinned to unit `col 2, row 2` and must land there |
| **The validator** | "Ignores" is pinned out of bounds at `col 40`. It must be **reported and then flowed**, never dropped. Look for `[pin rejected]` in the Log page and the `DBG_CARDS` line |

Boards flashed with this: `CYD_S3_3248` (COM10), `WS_P4_5` (COM15), `WS_S3_4B` (COM8).
`WS_P4_4B` is built and unflashed. `WS_P4_7B` is on `v0.2.4.23`, in an enclosure that blocks a USB
port — leave it alone (and note that is the argument for OTA, Phase 5).

**A span is in UNITS and does not rescale with the column count.** Three units is 1.5 cells on
every board. Two such cards occupy three cells' worth of grid; add a column and they still occupy
three of the now-four cells, each one narrower. That distinction caused confusion once already.

Once confirmed: close #16, merge with `--no-ff`, tag `v0.2.5` on `main`.

---

## The state of the network, which is three separate faults

Do not treat "MQTT keeps dropping" as one bug. It was three, and two are fixed.

| | Status |
|---|---|
| **Duplicate MQTT subscriptions** filling the table (16 rows of one topic) | **Fixed** — `addSub()` dedupes |
| **A leaked TCP socket per reconnect** — the broker was reaping ghosts, hence `exceeded timeout` in its log | **Fixed** — `_client.disconnect()` on all three exits |
| **Internal heap starvation**, twice: `LV_MEM_SIZE` 192 KB on P4, and `DOUBLE_BUFFERING` ignored on CYD_S3_3248 | **Fixed** — see `LESSONS.md` |
| **#49: a board reports `STA_CONNECTED` with a frozen RSSI and is unpingable** | **OPEN, and not any of the above** |

**#49 is the one that remains.** Confirmed on `WS_P4_5` with **75.9 KB of free heap**, so it is not
a memory problem. Both boards that show it are P4, where WiFi runs over an ESP32-C6 co-processor on
SDIO, and the P4_5's boot log carries:

```
E rpc_core: Response not received for [0x15e](Req_GetCoprocessorFwVersion)
hostedHasUpdate(): Could not get slave firmware version: ESP_FAIL
```

**That is the best lead there has been** — if the RPC link to the C6 is already unreliable at boot,
"WiFi says connected but isn't" follows naturally, and it explains why `CYD_S3_3248` (native S3
radio) has never shown it. Worth pulling #41 in as a possible cause. Fix direction in #49: stop
treating `WiFi.status()` as proof of liveness, let repeated MQTT failure count as evidence about
the *link*, and let RSSI go stale like any other value.

**The diagnostic you will need is already there.** Disconnects print
`state=-3 after 45983 ms, heap 5644, wifi up`; `-3` means the far end closed it, `-4` a timeout.
`SystemCore::heapMark()` traces internal heap through startup, every dashboard rebuild and every
MQTT reconnect.

---

## What is next, after 2.5

The running order was agreed on 2026-09-15 and it overrides the milestone numbering. Reasoning is
in `ROADMAP.md`; the short version is that **entity supply, not card features, is what limits this
project.**

1. **#43 — Home Assistant over the websocket.** Design-and-build, not research: the API has been
   measured against the owner's live instance. `docs/design/ha-websocket.md` has the numbers and
   one finding that changes the architecture (`subscribe_trigger`, never `subscribe_events`).
2. **#44 — outbound commands** through the same client. This is where the panel stops being a
   display and becomes an interface. `call_service` is deliberately untested — running it turns on
   a light in the owner's house.
3. **2.6 tileview / 2.8 slots**, once there are enough entities to need pages.
4. **3.1 + 3.3 — the build sheet**, with a schema informed by what HA actually gives.

---

## Deliberately postponed — do not rediscover these

Everything here was raised, discussed and consciously deferred.

| What | Where it goes |
|---|---|
| **System panel rework** — see the section below. Fully specified, not started | next session |
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

Asked for at the end of the 2.5 session and **deliberately declined** — it is a new page, a layout
rewrite, width and anchoring work and a dynamic height, and starting it on an exhausted context
would have left it half-done. The owner agreed. One piece was done: **the log moved to its own
page** (`LogPage`), which is the likely cause of the panel's choppy animation.

Remaining, as he specified it:

- **Button layout.** Row 1: `Log` `Tokens` `Cards`. Row 2: `Col -/+` `Row -/+`. Row 3: `Deck`
  `Compact` (manual variant override) `Theme`. Row 4: card-header mode, `Fill` (fill/icon for
  active states), `Area` (show/hide).
- **Width:** ~1/2 screen on `WS_P4_5` and `WS_P4_7B`, ~3/4 on the 4B boards, unchanged on
  `CYD_S3_3248`.
- **Anchor right**, so it reads as coming from the status icon you tapped — with the same margin
  the deck panels have, not hard against the edge.
- **Dynamic height:** expand only as far as the content needs.
- **Fix the slide origin.** It was meant to slide out from under the header bar; since the header
  became resizable it appears from nothing and sits disconnected.

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
