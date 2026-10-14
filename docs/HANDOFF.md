# Handoff — 2026-10-14 (`v0.2.13`; the UI follow-ups built on `feat/ui-followups`, waiting for the owner)

**Start here.** `CLAUDE.md` is the stable how-it-works; **`docs/design/ui-architecture.md` is how the
UI is built** - read both before touching UI code. This file is only: where we are, what to do next,
what will bite you, and how to work with the owner. It was rewritten on 2026-10-14; the previous
one, with 2.10's milestone-by-milestone detail, is `docs/archive/HANDOFF_to_2026-10-14.md` (and the
one before, `HANDOFF_to_2026-09-29.md`) - read them only when chasing the history of something.

**Keep it short.** A finished investigation goes to `docs/LESSONS.md` (general) or
`docs/display/history.md` (display) as a few lines; a test run goes to its test sheet; a decision to
`DECISIONS.md`; a number to the doc that owns it. This file says where things are, not how they got
there.

---

## Where the project is

**`v0.2.13`, 2026-10-14: milestone 2.12, the UI foundation** - the card window's code rebuilt into
a foundation, with nothing changed on screen, proved phase by phase on three boards
(`docs/design/ui-foundation.md` is the record). Before it: `v0.2.12` K17 (a group's member as a
card of its own), `v0.2.8`-`v0.2.11` milestone 2.10, the card popup. `CHANGELOG.md` matches
versions to milestones.

What a board does: boots into two pages (House = the owner's 18 HA entities over the websocket,
Fleet = MQTT/system/virtual cards), swiped with wrap-around; three colour schemes (Midnight default,
Fleet, Linen); card types with state icons; **a long press opens a card's window** - a light's
controls (on the virtual lamps and HA's lights), an HA group's members, the light's HA scenes,
**every card's History** (a chart, or anything on/off as its activity), SETTINGS and Entity
properties; the owner's choices saved by card id on LittleFS; HTTP routes for screenshots, benches
and debugging (`docs/HTTP_REFERENCE.md`).

**The UI now, in one paragraph** (details: `ui-architecture.md`): every window is a `Kit::Sheet`
client; buttons, dialogs, chips, rows, panels and the selector are the component kit (`Kit::`),
each look a *role* - shared styles that follow the scheme; every colour and size is a token, and
the window's sizes are millimetres (`UIShape`); where cards go on a page is a `Layout::Engine`'s
decision (`FlowEngine` today; `layout-engines.md` for 2.11's). Changes are proved with the harness
(`scripts/ui_golden.py`: object trees and screenshots, A/B), host tests (`host-tests/`), the
structure check and the all-nine gate. The cards, `GUIManager` and the system panel's pages are not
on the kit yet - bring each over when it is next changed (`ui-architecture.md` §6).

## What is next

**Built unattended on 2026-10-14 (the owner's overnight brief), waiting for the owner:**

1. **`feat/ui-followups`** (PR_UI) - #96 the dialog's buttons share a narrow row; #97 About in every
   build (a button on the system panel), fact lists in two left-aligned columns, the countdown bar
   2 px on the window's edge, the near-equal tokens merged; #23 the Remove dialog shows the bar;
   #92 History's span buttons are the selector, compact (K75); #98 the flash test NVS-only (A16).
   One commit each. The sheet: **`docs/TEST_UI_FOLLOWUPS.md`** - tests, before/after pictures, and
   **Q1-Q7**. Two need the owner first: **Q1, the P4_5 lost its settings file** during the run (a
   copy from 00:26 exists; not restored), and Q3/Q4, which candidate for Members' spacing and the
   controls' radius (1.6 mm built for both).
2. **#99** - a quieter "glass" finish: the mock **"Glass, Satin, Flat"**
   (https://claude.ai/artifact/G5XANwGMdgWKR19HS1ZQvj, linked on #99): glass, satin and flat on the
   real elements - Members, scenes, the Remove dialog, Power on and off - each pressed, Midnight at
   the P4_5's window. **The owner likes satin and agreed its uses** (K77); glass only where
   something asks for attention. Nothing built; a kit role each, frames measured on an S3 first.
3. **#100** - NINA's clock: PR_CLOCK.

Then **the design interview §3 onward**, **2.8** header slots (#19), **2.11** group cards with the
page arranger (#66, #78) - whose hero region (`layout-engines.md`) is where #100's clock and a music
player belong (NINA's code is non-commercial-use only, `REFERENCE_PROJECTS.md`). `ROADMAP.md`'s
running order.

**After any merge**: `git pull`, tag the merge commit (annotated, `vX.Y.Z - <milestones>`), push the
tag, flash the boards on the desk. The CHANGELOG entry and the archived test sheet go on the branch
before the owner merges.

## The card window - what 2.10 taught, in brief

What the window is, view by view: **`card-sheet.md` §14-18** (a later section wins), and its parts
have names ("Words", K22). Every round and measurement of 2.10 is in `docs/archive/TEST_2.10*.md`.

- **Usable first**: complete in its first frame, closed in one; no grow, no dim. **Nothing moves**
  within one window. Sizes are shares of the P4_5's window (`pm()`); touch targets and text are real
  millimetres.
- **Light state lives in the registry**; a command is confirmed only by a matching report. Groups
  act as HA's light group does (read from HA's `group/light.py`); a paused member is out of its
  group.
- **History**: `HaHistory` (`components/Fleet_HA`) asks HA's websocket from the loop task and parses
  on the websocket task; `TimeFormat` words every time and duration (`card-sheet.md` §18).
- **Saving**: one JSON file, `Settings` (CLAUDE.md "Saved settings", `card-sheet.md` §17); cards
  added on the device are `CardAdded` (K17).
- **The P4 freezes were IDF's PPA hanging on a rotated strip** (A15), fixed by a patch in the rebuilt
  libraries (below). None since. If one returns: `GET /hang` on the P4_5 (`-D DEBUG_HANG`) before
  anyone resets it.
- **Never read from a DSI panel that is showing a picture** - it blanked all three P4s. The 4B
  washed out twice ever: next time, `GET /panel?resend=1` before resetting it.
- Open and not blocking: **#91** the P4_5's white flash under sustained drawing; **#90**, **#89**,
  #84, #85; the rest is in `FUTURE_IMPROVEMENTS.md` ("Carried out of HANDOFF").

## Where the design interview stands

`docs/design/interview-phase2-3.md` is the blueprint for 2.8, 2.11 and Phase 3; Claude facilitates.
§1 look and feel closed 2026-10-01 (`look-and-feel.md`); §2 card popups closed 2026-10-02 (built as
2.10). **§3 onward is next**, and #87 (the companion web app with a true preview, D-8) should be
scoped there, before 3.1. The group taxonomy draft is `group-cards.md` (K67, K68). Artifacts on
claude.ai: "Card Popup Mock", "Layout Playground" v4 (the arranger, #78), "Beyond the Grid", "Linen
Fill and Header Glyphs", "Fleet Status Glyphs" - links in the design docs.

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
| `CYD_S3_8048`, `WS_S3_5B` | - | not plugged in |

The P4_5, 4B and 1060 run `feat/ui-followups` (with the local lines). **The P4_5's settings file
was lost on 2026-10-14** (TEST_UI_FOLLOWUPS Q1): it runs on the dashboard's defaults until restored.
Ports move when boards are
re-plugged: `Get-CimInstance Win32_PnPEntity | ? Name -match 'COM\d+' | select Name, DeviceID`.
**Is a board alive?** Ask Home Assistant (`sensor.<device>_uptime`), not the cable. While developing,
build and flash the P4_5 (plus the S3_4B compile); while the owner tests, the P4_5 alone unless the
owner says otherwise; all nine compile before a merge. **Away from the desk** the owner reflashes the P4_5 from the
laptop (COM6) for that Wi-Fi; it is then at `http://10.0.0.2` (that router resolves no names).
RTC cells: the Waveshare P4s need rechargeable ones (the 7B's holder is ML1220); nothing reads an RTC
yet (#32).

## Read in this order

1. `CLAUDE.md` - the HAL/BSP, the rules, **and the file-editing rule at the top**.
2. `docs/ROADMAP.md` (the plan, and where each kind of note belongs) and `docs/DECISIONS.md` (every
   decision in one line).
3. **For UI work: `docs/design/ui-architecture.md`**, then what it points at - `components.md`,
   `tokens.md`, `card-sheet.md` §14-18, `layout-engines.md`, `cards.md`, `pages.md`, `card-layout.md`
   (before moving anything inside a card), `interview-phase2-3.md`.
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
  and an unattended upload gets a `timeout` (one sat two hours). All of 2.12's builds ran from Git
  Bash by calling `~/.platformio/penv/Scripts/pio.exe` directly; an older note says pioarduino
  55.03.311 rejected MSYS shells, so if a build refuses there, use PowerShell.
- **Windows PowerShell 5.1 makes a native program's stderr fatal under `$ErrorActionPreference =
  "Stop"`** - `gate.ps1` handles it; a new script that runs `pio` must too.
- **`pio run` with no `-e` builds the 7B only.** The all-nine gate: `powershell -ExecutionPolicy
  Bypass -File scripts/release/gate.ps1` (~11 min; run it in the background).
- **PlatformIO keeps one environment's build at a time**: building another wipes the last one's
  `firmware.bin` - copy an image you want to keep. Clear `.pio/build_cache` after a BSP,
  struct-layout or `lv_conf.h` edit.
- **Do not edit source while a build runs** - it compiles the half-finished edit. Docs are safe.
- **`/panel?flash=N` keeps only its NVS mode** (#98, `feat/ui-followups`): without `&nvs=1` it is
  refused. A build older than that branch still erases the settings partition - never run it there.
- **Save a board's `/settings` before flashing it** (`curl -o`): on 2026-10-14 the P4_5 booted with
  its LittleFS formatted after an app-only flash, cause unknown (TEST_UI_FOLLOWUPS Q1). The file is
  plain JSON; nothing else on the board holds those choices.
- **The harness**: never two runs at one board; the 1060 alone (it is slow). A window closes itself
  after a minute.
- **A serial monitor resets the board when it opens** - attach with `--rts 0 --dtr 0`, or read it
  through HA, `/screenshot` or `scripts/ui_golden.py bootlog`. `Serial.setTxTimeoutMs(0)` in
  `main.cpp` stops native-USB boards freezing when the port is open but unread: keep it.
- **HTTP handlers and the HA websocket task never touch LVGL.** Prefer ESP-IDF facilities to
  Arduino-only ones (W4).
- **`Edit` on a CRLF file**: deleting a line by matching a leading newline joins two lines.
- **`gh ... --body $text` and `git commit -m` in PowerShell 5.1 drop double quotes** - write the text
  to a file and use `--body-file` / `git commit -F`.
- **LVGL clips children to their parent** (`UI::unclipShadows()`); never toggle HIDDEN on a
  screen-sized object when an animation starts; anything drawn stays ASCII except `°`.
- **Verify from outside the device** - the router, the broker, HA, `/bench?what=verify`, the harness.

## How to work with this owner

A hobbyist and ESP32 enthusiast, not a professional developer, and explicit about that - but reads
code, spots real bugs, and has caught several that were not obvious. **Treat the owner's instincts
as data.** A little colour-blind: never distinguish two states by colour alone.

**What works:**
- **Show, don't spec.** Build something to react to (a mock artifact for a look); the owner flashes
  fast and tests thoroughly against a sheet - `docs/TEST_<milestone>.md`, numbered tests with PASS
  criteria, the owner's results under them (`docs/archive/TEST_2.12.md` is the latest).
- **Snappy and responsive first, eye candy second** (W6). Measure a frame before and after anything
  visual.
- **When the owner is testing, give ONE report when the owner says the testing is done.**
- **Plain language, not metaphor.** Explain a question in plain terms when asked again (TEST_2.12
  Q1 needed it).
- **Never assume. Give the top 2-3 options with a recommendation, and ask** before building on an
  assumption; discuss a structural choice a turn before implementing it. Make the case for any
  install first; the owner decides.
- **Read the tool, not the name**; **own mistakes plainly** (a finding that was wrong is withdrawn,
  in the sheet); **say plainly what is unexplained**; **say what to expect** before anything that
  changes the screen.
- **Unattended work is welcome** when the brief is clear: 2.12 ran start to finish with one
  interruption (the 1060's USB). Commit and push at the end of each step.
- **Avoid compacting context**: the owner would rather end a session with a clean handoff.

**What to avoid:** committing straight to `main` (the guard refuses it anyway); guessing a fourth
time - instrument it or ask the far end; claiming a script worked because it printed something.

**Real lights (W9):** any Office light may be commanded while testing, put back as found; ask
before the Kitchen. **The issue tracker, HANDOFF and ROADMAP are Claude's to keep current** - part
of the work, not a follow-up.

**Versioning (W3):** `v0.<phase>.<release>`; the release counts signed-off merges to `main`.
**`v0.2.13` was tagged at the 2.12 merge (2026-10-14); the next merge in Phase 2 is `v0.2.14`.**
