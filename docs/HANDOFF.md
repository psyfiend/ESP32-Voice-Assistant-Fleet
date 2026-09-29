# Handoff — 2026-09-27 (end of the laptop weekend)

**Start here.** `CLAUDE.md` is the stable how-it-works. This file is only: where we are, what to do
next, what will bite you, and how to work with the owner. It was cut from ~680 lines to this on
2026-09-24; resolved investigations live in `docs/LESSONS.md` (the conclusions) and git history
(the detail). If you are about to add a long "how we found it" story here, it belongs in LESSONS.

**A warning about this file.** When it paraphrases a spec, the paraphrase becomes the spec for
whoever reads it first. Say *which* document a summary compresses, and treat vocabulary that does
not appear in the source as suspect.

---

## 2026-09-28 - back at the desktop

- **Step 5 below is done** for the two P4s: `.pio/build_cache` cleared, `WS_P4_5` and `WS_P4_4B`
  rebuilt with the desktop's secrets and flashed; both rejoined the home network.
- **`CYD_S3_3248`'s USB connector broke off the PCB** (owner). Treat it as unavailable; it can
  still be powered from the battery/headers and flashed over UART. **The slow dev target is now
  `WS_S3_TOUCH_LCD_4B` (COM8)**; the fast one is still `WS_P4_5` (COM15). The 4B P4 is COM7.
- **`WS_S3_4B` on Arduino_GFX loses its picture after days of uptime**: flickering horizontal lines,
  then the image rolls vertically (the header shows at the bottom); a reboot clears it. The owner
  has seen the same while trying porch/clock values on the 7B and the P4 4B. Leading theory,
  unconfirmed: the RGB peripheral losing sync ("drift"), not static timing. **Owner's plan: bring
  it to esp_lcd (2.9 step 4) first, then troubleshoot there** - it has the resync hook and the
  timing controls. It runs v0.2.7 (no `/screenshot`); HTTP is refused.
- **Branch `feat/67-present-mode`** (off `feat/67-bench-anim`), owner-approved:
  `BSP_PANEL_DRIVER` in every BSP (the driver chosen by name, no per-chip if/else;
  `PANEL_MODEL` derived from it) and the **present mode** - derived by `bspPresentMode()`,
  overridable per board by `DisplayConfig.PRESENT_MODE`, framebuffer count following from it,
  shown in the boot log and System Doctor. Only `TRIPLE_PARTIAL` is built. CLAUDE.md and
  `components/Fleet_Display/README.md` describe both.
- **`CYD_P4_1060` runs esp_lcd** (JD9165, first light ever for that driver): owner signed off
  C1-C4 on glass, `/bench` verify 0 bad, full frame 104 -> 64 ms (scheme differed between the
  runs - see `TEST_2.9.md`). The CYD has a camera - noted in FUTURE_IMPROVEMENTS.
- **`WS_P4_7B` runs esp_lcd** (EK79007, rotation 2 via the PPA): `/bench` verify 0 bad, full frame
  101 -> 64 ms (same scheme). **Owner's glass checks B1-B4 not yet reported** - B2 (touch, the
  unverified `#ifndef WS_P4_7B` passthrough) is the one to watch. **Panel-side 180 degrees was tried
  and does not work** (the driver's `mirror()`/MADCTL, three builds) - LESSONS, Hardware.
- `scripts/soak.py` (new): N boards in parallel through `/bench` for `--minutes`; counts failures,
  bad verify checks, reboots, UI-thread freezes. **2-hour soak of the 7B and CYD_P4_1060,
  2026-09-28 19:02-21:02: PASS on both** - 2,859 runs, 9,520 verify checks, 0 failures, 0 bad,
  0 reboots, worst frame 74 ms (`TEST_2.9.md`). All four P4s now run esp_lcd and have soaked clean.
- `reference/esp-registry/` gained `esp_display_present`, `esp_lv_present` and the
  `lvgl_present_benchmark` example (IDF >= 6.0, read-only; `REFERENCE_PROJECTS.md` says what is in
  them for us).

## Start here - 2026-09-27

**1. The design interview is the next piece of work.** `docs/design/interview-phase2-3.md` is the
blueprint-to-be for the rest of Phase 2 and all of Phase 3 (including how the build sheet, the
on-device UI and the web UI live together). **Claude facilitates it; the owner answers.** Follow
its §0.1a protocol exactly - in short: one section per sitting, in order, starting with §1 (look
and feel); 3-4 questions at a time with context and a recommendation, in plain words; show a mock
before asking about looks; push back with evidence where an answer conflicts with a decision or a
gotcha; write the answers into the section's design doc as you go; close by updating ROADMAP and
this file and committing (docs only). **Build nothing a session decides until the owner says go.**
Its §0.2 lists what is already decided - do not ask those again.

**2. The deck is NOT part of the default dashboard** (owner, 2026-09-27) - a throwback to his first
design, kept in code in case it is worth repurposing (inside popups, or on route-only room pages).
Device-wide settings come from the system panel at the top. **All three edge-swipe zones are to be
user-customizable**, a target being a page, a card popup, settings, a deep link into a settings
view (the log, e.g.), or anything else - one "go to" addressing scheme (interview §5).

**3. The card popup blueprint** is `docs/design/card-sheet.md` (draft 2; decisions D1-D7 open, and
§2 of the interview).

**4. esp_lcd for the boards not yet moved (7B, CYD_P4_1060) - prep done, runbook here.** Both
panel drivers (EK79007, JD9165) are vendored in `Fleet_Display` and compile and link (not run);
DSI lanes and PHY power come from their BSPs. The PHY reference clock: IDF's "0" resolves to
PLL_F20M on our pre-rev3 build, the same as Arduino_GFX used, so both boards get the clock they rely
on. **UNCOMMITTED in the laptop's tree:** `Fleet_Display` now also honours an explicit
`PHY_CLK_SRC` code (no board sets one) - it could not be built because Windows began blocking
`pio.exe` on 2026-09-27 ("An Application Control policy has blocked this file"). **It is parked,
unbuilt, on branch `wip/phy-clk-src`**: build it on both dev boards, `/bench?what=verify`, then merge
into `feat/67-bench-anim` - or drop it. To move a board, with it on the desk:
1. `-D DISPLAY_ESPLCD` in its environment (the committed `platformio.ini` via the blob method if on
   the laptop - see the laptop memory note; plain edit at home).
2. Build, flash, read the boot log: `[Fleet_Display] Ready ... driver ...` and
   `[LVGL] repair copier: DMA2D`. Check `Refresh: NN Hz measured` in the System Doctor.
3. `/bench?what=verify&n=40&act=anim&gap=100` and `act=page` - `bad_checks` must be 0.
4. `bench.py`, `--anim`, `--page` with the flag off, then on (the before/after table).
5. A soak (the 40-minute pattern in TEST_2.9), then the owner's glass checks (a T3-style sheet).
Open per board: **the 7B STAYS at `ROTATION = 2`** (owner, 2026-09-27: its 3D-printed enclosure puts
the USB on the left; revisit later) - on esp_lcd that is a PPA 180-degree turn, measured to cost the
same as rotation 0 on the 4B. The 7B's `TouchManager` passthrough special case (`#ifndef WS_P4_7B`)
is unverified (CLAUDE.md) and must be re-checked on the glass after the move. **CYD_P4_1060's reset
pin** is uncertain in its BSP ("0 in schema, 27 in GFX Library, 5 in examples") - if the panel stays
black, that is the first suspect, then reset polarity (the P4_5 lesson).

**6. Order of operations from here:** (1) the desktop sync in item 5; (2) owner's sign-off, then
merge `feat/67-bench-anim` to `main` (`--no-ff`, all-eight build first); (3) the design interview,
§1 first; (4) 2.9 continues opportunistically - 7B and CYD_P4_1060 whenever each is on the desk,
S3s later; (5) whatever the interview orders next (2.10a is drafted and ready to start on go).

**7. NOT tested, NOT built, NOT decided - be honest about these:**
- **7B and CYD_P4_1060 on esp_lcd**: compile and link only; no panel has ever been driven by these
  drivers. First light is untested.
- **`wip/phy-clk-src`**: never built (Windows blocked `pio.exe` on the laptop).
- **S3 boards on esp_lcd**: not started (steps 4 and 5). They run Arduino_GFX, compile-checked only
  with this branch's changes - **no S3 was flashed or run this weekend.** Same for the 7B and
  CYD_P4_1060 on Arduino_GFX: compiled, not run.
- **`DEBUG_CARDS` off** measured on P4_5/4B only; the 7B keeps its own explicit line; `[S3-options]`
  untouched.
- **Waveshare's timing on the 4B** and the **16 ms refresh** (`FLEET_LV_REFR_PERIOD`) - measured,
  never looked at on the glass; on no board.
- **Home Assistant with this branch**: nothing was tested against HA or MQTT all weekend (both
  unreachable from the laptop's network) - at home, check HA and MQTT cards still update.
- **Card popups, header slots, group cards, the build sheet**: designed or asked about, nothing
  built. The interview has not started.
- Unknowns flagged in the designs: wall-clock time (SNTP/TZ) on the boards; whether flash writes
  glitch the esp_lcd panel; the HA websocket's 8 KB receive buffer vs history replies.

**5. Back at the desktop after the laptop weekend.** Everything is on GitHub; no kit needed.
- **The work is on branch `feat/67-bench-anim`, NOT merged to `main`** (it contains
  `feat/67-step2-p45` too). `git fetch`, `git checkout feat/67-bench-anim`, `git pull`,
  `git submodule update --init`.
- **`platformio.ini` needs nothing:** the committed file has the desktop's `c:/Users/Marge/...`
  paths plus every weekend change (the 4B's `DISPLAY_ESPLCD`, `DEBUG_CARDS` off in `[P4-options]`).
  The laptop's paths were never committed (skip-worktree there). If `git status` on the desktop shows
  `platformio.ini` modified from before the weekend, look at the diff before discarding it.
- **Delete `.pio/build_cache`** before the first build: BSP headers and `lv_conf.h` changed, and the
  cache serves stale objects for those (CLAUDE.md).
- The rebuilt P4 libraries and `reference/esp-registry` are already on the desktop. **Do not copy the
  laptop's `ConnectivityLocalSecrets.h` back** - it holds the weekend network.
- What changes on the glass: **P4_5 and 4B run esp_lcd** (4B now rotation 0); the 7B, CYD_P4_1060
  and all S3s build and run exactly as before (Arduino_GFX). The **S3s are not esp_lcd-ready**:
  `Fleet_Display` drives MIPI-DSI only; RGB (steps 4) and QSPI (step 5) are not started.
- Optional: the laptop's `bench/` folder (gitignored) holds every measurement JSON from the weekend.
- Before merging to `main`: the owner's sign-off, then an all-eight build (last done 2026-09-27, all
  SUCCESS), `--no-ff`.

## Remote weekend, 2026-09-26 to 28 - read this first if you are on the laptop

The owner is away with **only two boards, `WS_P4_5` and `WS_P4_TOUCH_LCD_4B`, and a laptop**. The
7B, the S3s and the CYD stayed home and are **unreachable**. Setup was done from
`Desktop\FleetLaptopKit\README.md` on the home desktop (rebuilt P4 libraries, secrets, fetched
drivers, Claude memory); if a P4 drops WiFi, first check the rebuilt `esp32p4_es` is really in place.

- **Network:** boards and laptop share a local network (phone hotspot or the host's WiFi). Home
  Assistant is NOT reachable from the boards - HA cards read unavailable, which is expected. MQTT
  likewise. Display work needs neither. Hostnames may not resolve; use IPs.
- **The WiFi SSID is compiled in** from the gitignored `ConnectivityLocalSecrets.h`; the laptop's
  copy was edited for the weekend network. Never commit it.
- **"Build the two dev boards" this weekend means `WS_P4_5` and `WS_P4_4B`.** The all-eight build
  before any merge still applies - it only compiles, no boards needed, ~25 min.
- **If the laptop's username is not `Marge`,** its `platformio.ini` has a local, uncommitted path
  replacement. `git status` will show it modified: **never stage it.**

**The work, in the owner's order of interest:**

0. **Status, 2026-09-26 (laptop session):** item 1 is **measured and fixed, awaiting glass
   (`TEST_2.9.md` S6/S7)**, on branch `feat/67-bench-anim` (off `feat/67-step2-p45`). Cause: the
   esp_lcd repair re-copied nearly everything each frame; it now subtracts what the frame redraws.
   `display-stack.md` §8.6. Rotation 0 is no longer needed for it. On the laptop's network
   hostnames do not resolve: P4_5 was `10.0.0.2`, the 4B `10.0.0.83` (find them with
   `curl http://<ip>/bench?what=x`, which answers with an error naming `anim`). The 4B's BSP is
   still `ROTATION = 2` on every branch - the rotation-0 decision below was never applied.
   **Item 2 (step 3) is BUILT and verified from outside, awaiting glass (`TEST_2.9.md` step 3):**
   the 4B runs esp_lcd with the vendored ST7703 driver, our timings, rotation still 2 on purpose
   (same picture as before; rotation 0 is a separate, visible change for the owner to see).
   `display-stack.md` §8.7. **Owner signed off every functional test on both boards (2026-09-26)**
   and set the 4B to rotation 0 (done). Then, with the owner away, a configuration sweep on the 4B
   (§8.8: timing makes no speed difference - a picture-quality choice for the owner's eyes; 50-line
   buffers are best; a 16 ms refresh period gives the 4B ~48 fps animation, now a per-board
   `-D FLEET_LV_REFR_PERIOD`, set on no board), and the page-swipe hesitation traced (§8.9:
   `DEBUG_CARDS` was ~90 ms of every swipe - off in `[P4-options]` now; the rest is building and
   laying out ~15 cards, ~115 ms each half). The 7B and CYD_P4_1060 drivers (EK79007, JD9165) are
   vendored and compile; **neither board runs them** - one line each, with the board on the desk.
   **Then (same day): the System Doctor reports versions and silicon** (P4_5 rev v1.3, 4B rev v1.0;
   rebuilt libs are IDF `v5.5.5-832-g2553c5ad432`), **and repairs moved to the DMA2D copier**
   (`esp_async_fbcpy`, §8.10): 3x the PPA's speed, the frames after a page swipe 43 -> 14 ms on
   P4_5. It exposed a bug in Espressif's copier (one static config for every handle): **never more
   than one copier job outstanding** - `esp_async_fbcpy_priv.h`. New instrument:
   `/bench?what=verify` checks the panel's buffer against LVGL's render on the device.
1. **The P4_5 animation stutter** (owner, 2026-09-26): opening one deck panel while the other closes
   stutters slightly on P4_5, while the 7B and 4B stay smooth. Not tearing - frames are late. The
   leading theory, unmeasured: P4_5 draws 1.5x the 7B's pixels at 1.73x UI scale, so a two-panel
   frame overruns LVGL's 33 ms refresh period (`LV_DEF_REFR_PERIOD`). **Plan agreed with the owner:
   measure first** - add an animation mode to `/bench` that triggers exactly that two-panel swap and
   records every frame (count, per-frame time, render vs PPA). Compare P4_5 on `esp_lcd` against P4_5
   on Arduino_GFX (delete `-D DISPLAY_ESPLCD` to switch), and against the 4B. Rotation 0 on P4_5 is
   the owner's suggested experiment if the PPA turns out to be the bottleneck.
2. **2.9 step 3 on the 4B:** `esp_lcd` for the ST7703. The owner set it back to **rotation 0**
   (2026-09-26; its old reason is forgotten), so no PPA rotation is needed - an angle-0 copy, or
   `draw_bitmap` with `use_dma2d`. Driver: `reference/esp-registry/waveshare__esp_lcd_st7703-v2.0.0`
   (MIT); vendor it into `Fleet_Display` like the HX8394 (its README). **Timings:** ours (46 MHz,
   66.7 Hz) and Waveshare's (38 MHz, 59.2 Hz) are both candidates - the owner keeps alternatives as
   BSP comments on purpose (CLAUDE.md); compare on glass, never "correct" silently. Survey §3.
3. **S5** (step 2 overnight) if not done at home: leave P4_5 up overnight, then `bench.py`.

Use `/screenshot?fb=1` plus a pixel diff against `/screenshot` to check a new display path from
the laptop before asking for eyes - it is how step 2 was verified (`display-stack.md` §8.5).

## Where the project is

**`v0.2.7`, tagged on `main` 2026-09-24** (merge `c7f082e`). Phases 0, 1 and milestones 2.1–2.7
are done and signed off on glass. Boards report `v0.2.7.x`. Since the tag: #58 screenshots (merged
2026-09-25, not a milestone, so no tag).

What a board does today: boots into **two pages** (House = the owner's 18 HA entities over the
websocket, Fleet = MQTT/system/virtual cards), swiped horizontally with wrap-around; per-page
settings from the System drawer; three colour schemes (Fleet, **Midnight** default, **Linen** the
light one, with real drop shadows); card types with corner/hero icons, a `device_class`-driven
binary-sensor card, light brightness fill and colour; an FPS/CPU overlay.

**Five boards attached, all on v0.2.7:**

| Board | Port | Serial log |
|---|---|---|
| `CYD_S3_3248` | COM10 (native USB) | quiet - read it from HA instead |
| `WS_P4_5` | COM15 (CH343) | yes, UART0 |
| `WS_P4_7B` | COM6 (native USB) | - |
| `WS_P4_4B` | COM7 (CH343) | - |
| `WS_S3_4B` | COM8 (CH343) | no - `Serial` is native USB, not cabled |

Ports move when boards are re-plugged. **Identify a board by its USB serial**, not its COM number:
`Get-CimInstance Win32_PnPEntity | ? Name -match 'COM\d+' | select Name, DeviceID`. The P4_5 is
`...5B90154141`.

**Is a board alive?** Ask Home Assistant, not the cable: each board publishes
`sensor.<device>_uptime`, and its `last_updated` answers for the whole fleet at once.

## What is next — ROADMAP §7 "Running order"

**#58 screenshots is DONE and merged (2026-09-25).** `http://<board>/screenshot` gives a PNG of
exactly what the board shows. `python scripts/screenshot.py [host]` saves them to `screenshots/`.
**Use it to judge a layout before asking the owner for a photo.** Hostnames resolve on the owner's
network (`fleet-ws-p4-5` and so on). There is no auth; it is gated by `-D ENABLE_SCREENSHOT`.

1. **2.9 (#67), display stack: Arduino_GFX -> `esp_lcd`. IN PROGRESS, and the plan is written:
   `docs/design/display-stack.md`. Read all of it first.** Decisions are taken (raw `esp_lcd`;
   measure first; `WS_P4_5` first; one board at a time behind a build flag).
   **Done, all merged (2026-09-25/26):** step 1, `GET /bench` + `python scripts/bench.py [host]`
   (numbers in `display-stack.md` §8 - re-run it after every step, that is what it is for);
   **`-O2` fleet-wide** (-9% P4_5, -14% CYD per full frame; overnight soak clean); **#68** (LVGL's
   top layer scrolled - LESSONS). **Measured and rejected:** `LV_USE_PPA` (drawing 11% slower),
   `LV_OBJ_STYLE_CACHE` (2-4% for pool space P4_5 lacks), and **`LV_OS_FREERTOS` + 2 draw units
   (drawing 51% slower on P4_5)** - all in §8.3, none merged.

   **Step 2 is BUILT on `feat/67-step2-p45` (2026-09-26), awaiting the owner's glass checks
   (`TEST_2.9.md` step 2, S1-S5).** `WS_P4_5` runs on `esp_lcd`: full-screen redraw 162.9 -> 90.1 ms,
   and `/screenshot?fb=1` (the panel's own buffer) matches LVGL's render to the pixel. Nothing
   else moved. After sign-off: step 3, the other P4s. The design: `docs/design/esplcd-step2.md` - its
   §7 holds the owner's decisions and overrides its §2. Triple-partial with PPA rotation on `WS_P4_5`, in a new
   `components/Fleet_Display/` (`DisplayManager` untouched until step 6), HX8394 driver vendored with
   its legacy-I2C code compiled out, `LV_OS_NONE`. Read `docs/research/waveshare-esp-lcd-survey.md`
   first. The branch `spike/67-esplcd-compile` proves every IDF call and the driver compile and link
   (never merge it; its §4a lists four vendoring traps). Back off only if the known PPA freeze
   (`display-stack.md` §9) appears.
   `reference/Guition Examples/` (owner) holds Guition's packs, including a **new, not-yet-onboarded
   board, `JC4880P433`** (P4, 480x800 ST7701 over DSI, real IDF examples). The CYD panel's TE pin is
   GPIO 38 and Guition's own driver never sends a row address over QSPI - both matter for step 5.
2. **2.10 (#65)** long-press popup groundwork - brightness/colour sliders, colour picker; pause
   moves into it. Tap stays a one-touch toggle.
3. **2.8 (#19)** header slots. 4. **2.11 (#66)** group cards. 5. **3.1 (#20)** schema.

The page overview and snapshot transitions (`pages.md` §6-7) come after 2.9: on the S3s they are
only worth building once drawing is fast.

**Loose ends, all small:**
- Three HA test entities (Avail / Reading / Refuse) sit at the top of the Fleet page, with three
  helpers in the owner's HA. Remove both when no longer wanted (`ExternalEntities_HA.h`,
  `Dashboard_Fleet.h`).
- #44's MQTT half is still open: nothing is both writable AND advertised yet.
- Ideas recorded, not scheduled: #52-#55; the owner's page ideas (linked pages, custom swipe
  targets, an alt-tab overview) are in `docs/design/pages.md`.

## Read in this order

1. `CLAUDE.md` — the HAL/BSP, the token and paint rules, **and the file-editing rule at the top**.
2. `docs/LESSONS.md` — before debugging anything.
3. `docs/design/cards.md` §13 — every card decision of 2.7, in rounds, and why.
4. `docs/design/pages.md` — the page model and what 2.6 did and did not build.
5. `docs/design/dashboard.md`, `card-layout.md` (before moving anything inside a card),
   `ha-websocket.md` (what HA's API gives us, measured), `tokens.md`, `startup.md`.
6. `docs/ROADMAP.md` §7.

## Environment state git cannot see

**The ESP32-P4 framework libraries are REBUILT, not stock.** `esp32p4_es` in
`~/.platformio/packages/framework-arduinoespressif32-libs/` carries `MEMPOOL_PREFER_SPIRAM` and a
64-byte L2 line - the fix for esp-hosted-mcu#243, the P4 WiFi dropouts (#49). The stock copy is
kept beside it as `esp32p4_es.stock.55.03.311`; rollback is a rename. A `pio pkg update` or a
platform reinstall silently puts the bug back. Procedure: `docs/REBUILD_P4_LIBS.md`.

**Do not use NINA's C6 updater** on these boards - it hung `WS_P4_5`. The C6 update is not needed.

## Things that will bite you

- **Build from PowerShell, not Git Bash** - pioarduino rejects MSYS shells.
- **`PYTHONIOENCODING=utf-8` before any `pio` whose output you pipe**, or it dies silently
  (LESSONS). A killed `pio` leaves orphaned `esptool`s holding the port.
- **Do not run two `pio` at once, and do not edit the tree while one builds** - it compiles your
  half-finished edit. Docs are safe to edit; source is not.
- **Clear `.pio/build_cache` after changing a struct's layout, a BSP header or `lv_conf.h`.**
  Stale objects against a changed struct corrupt memory rather than failing.
- **`pio run` with no `-e` builds ONE environment.** Before a merge, build all eight - but only
  then. While developing, build and flash just `CYD_S3_3248` and `WS_P4_5` (owner); the all-eight
  build takes ~25 minutes and blocks source edits while it runs.
- **A serial monitor resets the board when it opens.** Attach with `--rts 0 --dtr 0`. On
  `CYD_S3_3248`'s native USB port, even a pyserial open with RTS/DTR set false appeared to reset it
  (2026-09-24; not proven). Read that board through HA or `/screenshot`, not its port.
- **Prefer ESP-IDF facilities to Arduino-only ones** (owner): see CLAUDE.md. The HTTP server is
  `esp_http_server`, and its handlers run on their own task, so they must never touch LVGL.
- **`Edit` on a CRLF file**: deleting a line by matching a leading `\n` joins two lines. See
  CLAUDE.md; check `git diff` for a `+` line holding two statements.
- **HaProvider receives on the WEBSOCKET TASK, not `loop()`.** It must never touch LVGL; it writes
  through `EntityRegistry`'s mutex. Sends happen on the loop task only. HA request ids must strictly
  increase per connection, and a reconnect is a cold start (subscriptions are gone) -
  `ha-websocket.md`.
- **`staleAfterMs` is 0 on the HA entities on purpose**: `subscribe_trigger` fires on change, so
  silence means nothing. Death arrives as HA's word "unavailable" (`ST_UNAVAILABLE`).
- **LVGL clips children to their parent** - shadows, tags, anything that overhangs. Use
  `UI::unclipShadows()`; the flag alone is not enough (LESSONS).
- **Never toggle HIDDEN on a screen-sized object when an animation starts** - it redraws the whole
  screen in the first frame (LESSONS). Switch `CLICKABLE` instead.
- **Anything drawn on a panel stays ASCII, except `°`.**
- **Verify from outside the device** - the router, the broker, HA. It is the project's oldest rule.

## How to work with this owner

He is a hobbyist and an ESP32 enthusiast, not a professional developer, and explicit about that —
but he reads code, spots real bugs, and has caught several that were not obvious. **Treat his
instincts as data.** He is also a little colour-blind: never distinguish two states by colour alone.

**What works:**

- **Show, don't spec.** Build something he can react to; he flashes fast and tests thoroughly,
  reporting back test by test against a sheet. Write `docs/TEST_<milestone>.md` with PASS/FAIL
  criteria for anything he will judge on glass.
- **Plain language, not metaphor.** His words: "sometimes I get a little lost in the slang."
- **Give a recommendation, not a menu** - but ask before building on an assumption, and discuss a
  structural choice a turn before implementing it.
- **Read the tool, not the name.** Check the source before asserting how something behaves.
- **Own mistakes plainly and move on.** Report negative results as clearly as wins.
- **Tell him what has NOT been tested.** He acts on it immediately.
- **Structure long answers**: what was done, what it took, how it looks different, issues hit,
  what you are unsure of, caveats, test steps with pass/fail.
- **He is often right about the shape of a thing before he can name it.** "I'm wondering
  whether..." is usually a design instinct - engage with it.
- **He will solve your problem if you tell him what you are stuck on.** #49 ended because he found
  the upstream issue. Say plainly what is unexplained.

**What to avoid:**

- Don't commit straight to `main`. Feature branch, then `--no-ff` merge once he signs off.
- **Don't guess a fourth time.** Instrument it or ask the far end.
- **Don't claim a script worked because it printed something.** Assert the anchor, then grep the
  file. This cost three separate features on 2026-09-20.
- Don't say "we should wait until phase X" as a reflex, and don't trust a browser mock - the glass
  decides.

**Versioning:** `A.B.C.D`, where **C is the milestone within the phase**. Tag on `main` at merge
when a milestone completes (`v0.2.7` = through 2.7), never during development. D is commits since
the tag; a dirty tree appends `+dirty`.

**The issue tracker is yours to manage**, and keeping it, HANDOFF and ROADMAP current is part of the
work, not a follow-up.
