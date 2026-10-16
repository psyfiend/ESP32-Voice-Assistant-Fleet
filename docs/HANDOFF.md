# Handoff — 2026-10-15 (`v0.2.17` flashed; 2.8 round 2 built - three places on claims - for the owner's test)

**Start here.** `CLAUDE.md` is the stable how-it-works; **`docs/design/ui-architecture.md` is how the
UI is built** - read both before touching UI code. This file is only: where we are, what to do next,
what will bite you, and how to work with the owner. It was rewritten on 2026-10-15 for a fresh
session; the earlier ones, with each milestone's detail, are `docs/archive/HANDOFF_to_2026-10-14.md`
and `HANDOFF_to_2026-09-29.md` - read them only when chasing the history of something.

**Keep it short.** A finished investigation goes to `docs/LESSONS.md` (general) or
`docs/display/history.md` (display) as a few lines; a test run goes to its test sheet; a decision to
`DECISIONS.md`; a number to the doc that owns it. This file says where things are, not how they got
there.

---

## Where the project is

**`v0.2.16`, 2026-10-15: #99, satin and glass** (`archive/TEST_99.md`; K77, K85-K87). Before it, on
2026-10-14: `v0.2.15` the UI follow-ups from 2.12's test (`archive/TEST_UI_FOLLOWUPS.md`), `v0.2.14`
#93 (members' HA areas), **`v0.2.13` milestone 2.12, the UI foundation** - the card window's code
rebuilt on a component kit, with nothing changed on screen (`docs/design/ui-foundation.md`). Before
them: `v0.2.12` K17, `v0.2.8`-`v0.2.11` milestone 2.10, the card popup. `CHANGELOG.md` matches
versions to milestones.

What a board does: boots into two pages (House = the owner's 18 HA entities over the websocket,
Fleet = MQTT/system/virtual cards), swiped with wrap-around; three colour schemes (Midnight default,
Fleet, Linen); card types with state icons; **a long press opens a card's window** - a light's
controls, an HA group's members, the light's HA scenes, **every card's History** (a chart, or an
on/off thing's activity), SETTINGS and Entity properties; About this board from the system panel;
the owner's choices saved by card id on LittleFS; HTTP routes for screenshots, benches, settings and
debugging (`docs/HTTP_REFERENCE.md`).

**The UI now, in one paragraph** (details: `ui-architecture.md`): every window is a `Kit::Sheet`
client; buttons, dialogs, rows, panels and the selector are the component kit (`Kit::`), each look
a *role* - shared styles that follow the scheme - and **a tappable surface has a finish**
(`Kit::finish()`: satin = can be tapped, glass = chosen, painted and dithered; K77, K85-K86); every
colour and size is a token, the window's sizes millimetres (`UIShape`); where cards go on a page is
a `Layout::Engine`'s decision. Changes are proved with the harness (`scripts/ui_golden.py`: object
trees and screenshots, A/B), host tests (`host-tests/`), the structure check and the all-nine gate.
The cards, `GUIManager`, the header (`Panel_Header`) and the system panel's pages are not on the kit
yet - bring each over when it is next changed (`ui-architecture.md` §6).

## Where things stand (2026-10-15, evening)

- **`v0.2.17` - A17** (#108, merged, tagged, **flashed as the release** on the P4_5, the 4B and the
  1060 on 2026-10-15 by `flash_release.ps1` from the tag - each board's file saved first and intact
  after). Copies of every board's file are in `bench/settings/<board>/`.
- **2.8 round 2 is built, on `feat/2.8-header-slots`** (no PR; **`TEST_2.8.md` round 2 is the
  owner's sheet**, R1-R11, Q2-Q4, Q6, Q8, Q9). `main` merged in (no conflict). What it is
  (`header-slots.md` §9-10; DECISIONS D-16 to D-20):
  - **Three places**: left the clock and the deck's temperature; the page in the middle, on the
    bar's centre; right the alarm (while set) and the link and Wi-Fi. **Square glyphs** by default.
  - **Claims** - the owner's idea, asked how a page names its places: each slot claims its room in
    its forms, each held with a keep (a priority); when a side would run into the middle the lowest
    keep gives way. A pure planner, `include/Layout/HeaderPlan.h`, ten host tests. **The notice
    queue (NINA's toasts), detailed / minimal forms, a window taking the middle and claims' panel
    rows are #76** - new claim sources, no new layout code.
  - **The alarm**: a slot kind with its panel, on the bar only while set (D-20). `Alarms.h` holds
    the next one; only a PC sets one until K84 (`/popup?alarm=HHMM`, RAM only).
  - **The system panel's frame is `Kit::Drawer`** (Q7): every panel one component, all joined.
  - Proved: harness A/B on the P4_5 (`v0.2.17` `r2b` vs round 2 `r2c`: every window unchanged;
    `desk_temp`'s mark is #104), the 4B's header states (`r2h`), frame cost +0.9 ms (<1%), the
    structure check, the gate (see the sheet).
- **Boards**: the P4_5 and the 4B run round 2 (local lines), the 1060 `v0.2.17`. The 4B's own file
  closes a window after 10 s (`system.window.close`), which stops the harness's full walk there -
  use `--only` for header states, or ask the owner.
- **The mock**: the "2.8 Header Slots" canvas
  (https://claude.ai/artifact/9dLvFUbWZePzgjLnnXZ5Sw); its last artboards show three places with
  the alarm still on the left (D-18 moved it; the build is the reference now).
- **Routes**: the P4_5 serves 7 of `HttpServer::MAX_ROUTES` (8); the next route needs it raised.

## Round 3 - the unattended build's progress (2026-10-16 night; the watchdog resumes from here)

Decided: D-21 to D-28 (DECISIONS); the look: canvas "2.8 Header Slots", the four "Round 3" artboards.
Each step committed and pushed on `feat/2.8-header-slots`; tick it here when it is.

- [x] 1. Kit selector: up to 5 choices, even widths (`SELECTOR_MAX`, `SelectorSpec::slotW`)
- [x] 2. Type: the P4_5's TAG and header words 24 (D-26; `UIToolkit::Font_Header`)
- [x] 3. Glyphs as data (`UI/GlyphShapes`, `Kit::strokeGlyph`), per-scheme status colours (`SIG_*`), one size bare or framed, the AP mast as its own claim (D-24, D-27)
- [x] 4. The bell in the clock's slot (7 days), the 12-hour notice claim and its panel, the clock panel's NEXT ALARM and "Warn me before" (D-21); `/popup?alarmin=N`
- [x] 5. One shape in the accent: the open tab, coves, flares, panels aligned at a screen edge (D-23)
- [x] 6. The system panel: CONNECTIVITY, SYSTEM - DIAGNOSTICS (Log / UI Tweaks / About, Scheme + Status bar, Col / Row) (D-22, D-25)
- [x] 7. UI Tweaks (a Sheet): selector rows, saved, Reset all, Tokens; the deck rises under it; the left-half swipe; header panels follow the timeout
- [x] 8. Card UI: a SETTINGS row opening a view of selector rows (D-28); Entity / Card label leave SETTINGS
- [x] 9. The page panel's Shown / Hidden as a selector; a PC key to show the sensor's "!" (T10)
- [ ] 10. Harness states, A/B against round 2, the gate, the structure check, `TEST_2.8.md` round 3, flash the P4_5 and the 4B (settings saved first)

## What is next with the owner

1. **2.8 round 3** - round 2 tested 2026-10-16, every test PASS; the owner's results and new
   decisions (D-21 the alarm by the clock and its 12-hour notice, D-22 the system panel as
   CONNECTIVITY + SYSTEM - DIAGNOSTICS and the knobs in a UI Tweaks window, D-23 the slot and panel
   as one shape through the accent line, D-24 the glyph overhaul) are in `TEST_2.8.md` and
   `header-slots.md` §11. Being scoped with the owner: how the mocks' glyphs reach the device, the
   border colour, the bell beside the clock, one round or two. Then sign-off -> CHANGELOG
   `v0.2.18`, archive the sheet, `gh pr create`; the owner merges.
2. **#76 next, on the claim model** (D-19): the notice queue ported from NINA's `nina_toast` (a
   queue any task posts to, drained on the UI thread; severity, lifetime, repeats counted, tap to
   dismiss), a notice's detailed form in the middle for a while then its minimal "!" in status, a
   place that grows for a high-priority claim, a window taking the middle, claims' rows in their
   panel. Mock first. Cluster headers come with 2.11.
3. Waiting: **#104** (a light's temperature mark moves between builds); **#100** the clocks (three
   agreed, K81-K84, the record on #100; the prototype on `feat/100-nina-clock`, PR #102 open, not for
   merging); **#106** an active card's glowing border (the owner's idea, mock first); **#84** "no data
   yet" (a proposal on the issue); **#99's S3 frame check** (W6) and History's activity rows in satin
   (K86), once an S3 is on the desk.

Then **2.11** group cards with the page arranger (#66, #78) - whose hero region (`layout-engines.md`)
is where the full clock and a music player belong. `ROADMAP.md`'s running order.

**After any merge**: `git pull`, tag the merge commit (annotated, `vX.Y.Z - <milestones>`), push the
tag, flash the boards on the desk. The CHANGELOG entry and the archived test sheet go on the branch
before the owner merges. **The owner merges** (W12); a PR is opened only after the owner's sign-off.

## The card window - what 2.10 taught, in brief

What the window is, view by view: **`card-sheet.md` §14-18** (a later section wins), and its parts
have names ("Words", K22). Every round and measurement of 2.10 is in `docs/archive/TEST_2.10*.md`.

- **Usable first**: complete in its first frame, closed in one; no grow, no dim. **Nothing moves**
  within one window. Sizes are shares of the P4_5's window (`pm()`); touch targets and text are real
  millimetres.
- **Light state lives in the registry**; a command is confirmed only by a matching report. Groups
  act as HA's light group does; a paused member is out of its group.
- **History**: `HaHistory` (`components/Fleet_HA`) asks HA's websocket from the loop task and parses
  on the websocket task; `TimeFormat` words every time and duration (`card-sheet.md` §18).
- **Saving**: one JSON file, `Settings` (CLAUDE.md "Saved settings"); cards added on the device are
  `CardAdded` (K17); the windows' timeout and bar are system settings (K79).
- **The P4 freezes were IDF's PPA hanging on a rotated strip** (A15), fixed by a patch in the rebuilt
  libraries (below). None since. If one returns: `GET /hang` on the P4_5 (`-D DEBUG_HANG`) before
  anyone resets it.
- **Never read from a DSI panel that is showing a picture** - it blanked all three P4s. The 4B
  washed out twice ever: next time, `GET /panel?resend=1` before resetting it.
- Open and not blocking: **#91** the P4_5's white flash under sustained drawing; #90, #89, #85; the
  rest is in `FUTURE_IMPROVEMENTS.md` ("Carried out of HANDOFF").

## Where the design interview stands

`docs/design/interview-phase2-3.md` is the blueprint for 2.8, 2.11 and Phase 3; Claude facilitates
with the owner. §1 look and feel closed 2026-10-01 (`look-and-feel.md`); §2 card popups closed
2026-10-02 (built as 2.10); **§3 the header's slots answered 2026-10-15** (`header-slots.md`; its
open points are the build's questions). **§4 (2.11 group cards) is next**; #87 (the companion web
app with a true preview, D-8) should be scoped before 3.1. The group taxonomy draft is
`group-cards.md` (K67, K68). Artifacts on claude.ai: "2.8 Header Slots", "Glass, Satin, Flat"
(#99), "Card Popup Mock", "Layout Playground" v4 (the arranger, #78), "Beyond the Grid", "Linen Fill
and Header Glyphs", "Fleet Status Glyphs" - links in the design docs.

## Fires still burning - each has an owner doc or issue, none needs the next session

| What | State | Where |
|---|---|---|
| **Battery** | prototype merged; `-D HAS_BATTERY` (7B, 4880); the inferred power state cannot see a missing cell on USB. **Owner: fuel gauges (MAX17043, ordered), not a better inference** | #72, `design/power-battery.md` |
| **2.9 last boards** | `CYD_S3_8048`, `WS_S3_5B` to esp_lcd when on the desk | #67, `display/README.md` |
| **4B panel timing** | **S3_4B locked up 2026-10-05**, left as found - read #69's latest comment before resetting it | #69 |
| **S3 LVGL speed** | the S3_4B feels heavier since LVGL's pool moved to PSRAM; paused by the owner | #70 |
| Rotation / IMU; type ladder | recorded | #71, #73 |

## Boards on the desk

| Board | Port | Notes |
|---|---|---|
| `WS_P4_5` | COM15 (CH343 `5B90154141`) | the dev board, in its enclosure; local lines `DEBUG_POPUP` + `DEBUG_HANG` |
| `WS_P4_4B` | COM7 (CH343 `5AAF289419`) | P4 rev v1.0; local `DEBUG_POPUP` |
| `CYD_P4_1060` | COM9 (native USB, serial `30:ED:A0:E3:2C:7B`) | needs two cables; local `DEBUG_POPUP`. **Flash it at 460800 and end with `--after watchdog-reset`** - two 1.5 Mbaud uploads lost its USB on 2026-10-13; BOOT held while plugging in, then a watchdog reset, brought it back (LESSONS) |
| `WS_P4_7B` | COM6 (native USB) | needs two USB cables or it browns out on boot |
| `CYD_P4_4880` | COM19 (native USB) | LiPo fitted; browned out on the PC's USB alone |
| `WS_S3_4B` | COM8 (CH343 `5A67166678`) | **locked up** (#69); compile it as the S3 check, never flash it until the owner says |
| `CYD_S3_3248` | - | USB broken (2026-09-28) |
| `CYD_S3_8048`, `WS_S3_5B` | - | not plugged in - #99's S3 frame check waits for one |

The P4_5 and 4B run A17's final build, the 1060 `v0.2.16` (all with the local lines). Ports move when boards are re-plugged:
`Get-CimInstance Win32_PnPEntity | ? Name -match 'COM\d+' | select Name, DeviceID`. **Is a board
alive?** Ask Home Assistant (`sensor.<device>_uptime`), not the cable. While developing, build and
flash the P4_5 (plus the S3_4B compile); while the owner tests, the P4_5 alone unless the owner says
otherwise; all nine compile before a merge. **Away from the desk** the owner reflashes the P4_5 from
the laptop (COM6) for that Wi-Fi; it is then at `http://10.0.0.2` (that router resolves no names).
RTC cells: the Waveshare P4s need rechargeable ones (the 7B's holder is ML1220); nothing reads an RTC
yet (#32). Flash and PSRAM per board: `HARDWARE_STATUS.md` (the 1060 and 4880 have 16 MB of flash).

## Read in this order

1. `CLAUDE.md` - the HAL/BSP, the rules, **and the file-editing rule at the top**.
2. `docs/ROADMAP.md` (the plan, and where each kind of note belongs) and `docs/DECISIONS.md` (every
   decision in one line).
3. **For UI work: `docs/design/ui-architecture.md`**, then what it points at - `components.md`
   (the kit, the finishes), `tokens.md`, `header-slots.md` (2.8), `card-sheet.md` §14-18,
   `layout-engines.md`, `cards.md`, `pages.md`, `card-layout.md` (before moving anything inside a
   card), `interview-phase2-3.md`.
4. `docs/LESSONS.md` - before debugging anything. `docs/HTTP_REFERENCE.md` - before calling a route.
5. For the display: `docs/display/README.md` only - it indexes the rest.

## Environment state git cannot see

**The ESP32-P4 framework libraries are REBUILT, not stock** (`esp32p4_es` in
`~/.platformio/packages/framework-arduinoespressif32-libs/`): `MEMPOOL_PREFER_SPIRAM` and a 64-byte
L2 line (esp-hosted-mcu#243, #49), `LCD_DSI_ISR_CACHE_SAFE` (flash writes flashed the panels blue,
A14) and IDF's **PPA rotation patch** (the freezes, A15, `scripts/idf-patches/`). Beside it:
`esp32p4_es.cache_safe`, `esp32p4_es.hosted_fix`, `esp32p4_es.stock.55.03.311` - rollback is a
rename. **A `pio pkg update` or platform reinstall silently puts the bugs back.** How:
`docs/REBUILD_P4_LIBS.md`; the build tree is in WSL (`~/esp32-arduino-lib-builder`). The folder is a
GitHub pre-release, `p4-libs-2026-10-09` (97 MB, install steps there); check a build with
`grep -a -o "v5.5.5-832-g2553c5ad432[-a-z]*" .pio/build/<env>/firmware.bin` (it must end `-dirty`).
**Per machine**: the laptop has the same folder (2026-10-10). The S3 libraries are stock.

**The owner's local debug lines** in `platformio.ini` (above, per board) are uncommitted and never
committed - the pre-commit hook refuses them; `scripts/release/gate.ps1` stashes and restores them.
The laptop's copy (skip-worktree, its own path prefix) has `DEBUG_POPUP` and `DEBUG_FRAMES` on the
P4_5.

**WSL's Ubuntu has g++ 13.3** - the host tests run there (`host-tests/run_in_wsl.sh`); Windows has
no compiler (the owner's choice to install one, `host-tests/README.md`).

**Do not use NINA's C6 updater** - it hung `WS_P4_5`. `reference/` is gitignored.

## Things that will bite you

- **`PYTHONIOENCODING=utf-8` before any `pio` whose output is piped** - it dies silently otherwise -
  and an unattended upload gets a `timeout` (one sat two hours). Builds run from Git Bash by calling
  `~/.platformio/penv/Scripts/pio.exe` directly.
- **Windows PowerShell 5.1 makes a native program's stderr fatal under `$ErrorActionPreference =
  "Stop"`** - `gate.ps1` handles it; a new script that runs `pio` must too.
- **`pio run` with no `-e` builds the 7B only.** The all-nine gate: `powershell -ExecutionPolicy
  Bypass -File scripts/release/gate.ps1` (10-30 min; run it in the background). It refuses a dirty
  tree; do not build anything else while it runs (it stashes `platformio.ini`).
- **PlatformIO keeps one environment's build at a time**: building another wipes the last one's
  `firmware.bin` - copy an image you want to keep. Clear `.pio/build_cache` after a BSP,
  struct-layout or `lv_conf.h` edit.
- **Do not edit source while a build runs** - it compiles the half-finished edit. Docs are safe.
- **Save a board's `/settings` before flashing it**: `scripts/release/save_settings.ps1 <host>
  <board>` once A17 merges (until then `curl -o`, or the script from that branch).
- **The harness**: never two runs at one board; the 1060 alone (it is slow). A window closes itself
  after a minute (or what `/settings?window_close=` says, K79). A change to the header will show in
  every state - read the differences, do not wave them through.
- **A serial monitor resets the board when it opens** - attach with `--rts 0 --dtr 0`, or read it
  through HA, `/screenshot` or `scripts/ui_golden.py bootlog`. `Serial.setTxTimeoutMs(0)` in
  `main.cpp` stops native-USB boards freezing when the port is open but unread: keep it.
- **HTTP handlers and the HA websocket task never touch LVGL.** Prefer ESP-IDF facilities to
  Arduino-only ones (W4).
- **Faces** (`KitFaces.cpp`): LVGL's gradients band on these 16-bit panels, so faces are painted and
  dithered; a recoloured image loses its corner clip, so a press tint is a veil (LESSONS). 16 face
  slots in PSRAM; the first window after a boot pays ~35 ms painting them.
- **`Edit` on a CRLF file**: deleting a line by matching a leading newline joins two lines.
- **`gh ... --body $text` and `git commit -m` in PowerShell 5.1 drop double quotes** - write the text
  to a file and use `--body-file` / `git commit -F`.
- **LVGL clips children to their parent** (`UI::unclipShadows()`); never toggle HIDDEN on a
  screen-sized object when an animation starts; anything drawn stays ASCII except `°`.
- **Verify from outside the device** - the router, the broker, HA, `/bench?what=verify`, the harness.

## How to work with this owner

A hobbyist and ESP32 enthusiast, not a professional developer, and explicit about that - but reads
code, spots real bugs, and has caught several that were not obvious. **Treat the owner's instincts
as data** - #99's rail through the lens and 2.8's four slots were the owner's ideas. A little
colour-blind: never distinguish two states by colour alone.

**What works:**
- **Show, don't spec.** Build something to react to (a mock artifact for a look); the owner flashes
  fast and tests thoroughly against a sheet - `docs/TEST_<topic>.md`, numbered tests with PASS
  criteria, the owner's results under them (`docs/archive/TEST_99.md` is the latest).
- **A PR only after the owner's sign-off** (2026-10-14): push the branch, the owner tests and answers,
  then `gh pr create`; the owner merges.
- **Snappy and responsive first, eye candy second** (W6). Measure a frame before and after anything
  visual (`/bench what=full` with the window open).
- **When the owner is testing, give ONE report when the owner says the testing is done.**
- **Plain language, not metaphor.** State what things are; avoid "X, not Y" framings.
- **Never assume. Give the top 2-3 options with a recommendation, and ask** before building on an
  assumption; discuss a structural choice a turn before implementing it. Make the case for any
  install first; the owner decides.
- **Read the tool, not the name**; **own mistakes plainly** (a finding that was wrong is withdrawn,
  in the sheet); **say plainly what is unexplained**; **say what to expect** before anything that
  changes the screen.
- **Unattended work is welcome** when the brief is clear. Commit and push at the end of each step.
- **Avoid compacting context**: the owner would rather end a session with a clean handoff.

**What to avoid:** committing straight to `main` (the guard refuses it anyway); guessing a fourth
time - instrument it or ask the far end; claiming a script worked because it printed something.

**Real lights (W9):** any Office light may be commanded while testing, put back as found; ask
before the Kitchen. **The issue tracker, HANDOFF and ROADMAP are Claude's to keep current** - part
of the work, not a follow-up.

**Versioning (W3):** `v0.<phase>.<release>`; the release counts signed-off merges to `main` that
change the firmware (a docs-only merge is not tagged). **`v0.2.16` was tagged at #99's merge
(2026-10-15); the next is `v0.2.17`.**
