# Handoff — 2026-10-15 (`v0.2.16`, #99 satin; next: A17 and 2.8's header slots, unattended)

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

## For the next session: unattended work (owner, 2026-10-15)

The owner reset the usage after #99's merge and handed the next session two things to do
unattended, effort "high": **A17**, then **as much of 2.8 as can be done well**. The owner: "I'm
more interested in getting back to working on the bones of the dashboard!" #104 and the NINA clocks
wait.

**The rules:**
- Read `CLAUDE.md`, this file, `ui-architecture.md`, `DECISIONS.md` and the task's documents first.
- **Set the usage-limit watchdog before anything else** (auto-memory "Auto-resume after limits": a
  recurring `CronCreate` at an off-minute about every 20 minutes that resumes from a progress record
  in your scratchpad); keep the record current after each step; delete the watchdog when done.
- **One branch per task, off `main`**; commit and push at the end of each step. **No pull request**
  (owner, 2026-10-14): push the branch, write `docs/TEST_<topic>.md` - tests with PASS criteria,
  before/after pictures, questions each with a recommendation - and leave it for the owner.
- **Decisions the owner has not made** (auto-memory "Decide before building"): the owner is away,
  so take the conservative option, build it so it is easy to change, and **say in the sheet which
  choices were yours** and what the alternatives were. Never make a choice the documents leave to
  the owner without saying so.
- **Boards**: confirm the ports first. Build and flash the P4_5 (COM15) and the 4B (COM7) freely;
  the 1060 (COM9) app-only at 460800 ending `--after watchdog-reset`, and alone in the harness;
  **never flash WS_S3_4B** (#69, compile it as the S3 check); **never rebuild libraries**. Save a
  board's `/settings` (`curl -o`) before flashing it. Leave the boards on `main`'s build when done,
  unless a sheet says otherwise.
- **Prove each change with the harness**: capture a fresh `v0.2.16` baseline first, then A/B.
  Before calling a branch ready: the all-nine gate (`scripts/release/gate.ps1`, background) and
  `scripts/structure_check.py`, both quoted in the sheet.
- **Real lights**: Office only, put back as found; nothing else is commanded.
- A board that stops answering: note it and leave it.

**1. A17 - a failed mount never costs the settings** (`fix/a17-settings-safety`; DECISIONS A17,
`archive/TEST_UI_FOLLOWUPS.md` Q1). The P4_5 lost its settings file on 2026-10-14, most likely
LittleFS failing to mount and `format_if_mount_failed` formatting it.
- `Settings::begin()`: mount without formatting. If the mount fails, look at the partition's start:
  erased (all `0xFF`) is a first boot - format and mount; anything else stays untouched, the store
  runs in RAM, read-only, and says so (the boot note, `/settings?stats=1`, the System Doctor's
  [SETTINGS]). Flash is read only from a task with an internal-RAM stack (CLAUDE.md).
- **A restore address**: `POST /settings` with a settings file as its body - parsed and checked
  (`schema`) on the httpd task, written by the settings task, never by the handler (its stack is in
  PSRAM: a flash write there reboots the board, LESSONS).
- **The flash scripts keep a copy**: `scripts/release/flash_release.ps1` saves each board's
  `/settings` to `bench/settings/<board>/<yyyymmdd-hhmm>.json` (gitignored) before writing, and a
  `scripts/release/restore_settings.ps1 <host> <file>` posts one back.
- Test: a local-only `-D DEBUG_SETTINGS_NOMOUNT` (never committed) that makes the mount report a
  failure - the boot is read-only and the file survives the next boot; a restore round trip on the 4B
  with its own file (save, rename a card from the PC, restore, check).
- Docs: `Settings.h`, CLAUDE.md "Saved settings", `HTTP_REFERENCE.md`, DECISIONS A17 (built),
  `TEST_A17.md`.

**2. 2.8 - the header's slots** (`feat/2.8-header-slots`; #19). **Read `docs/design/header-slots.md`
first** - the owner's answers to the interview's §3, in the owner's words, and the build order (§8);
DECISIONS D-11 to D-15. In short: the header is optional, so nothing critical lives only there; four
evenly spaced slots - clock, the page, status, climate / secondary; a tap on a slot slides down that
slot's own panel; shown / hidden is a page setting; slots for the page header and clusters, never a
solo card; dashes for no live value.
- **(a) The mock first** (show, don't spec): add artboards to the owner's canvas
  [2.8 Header Slots](https://claude.ai/artifact/9dLvFUbWZePzgjLnnXZ5Sw) - the four-slot header in the
  three glyph styles the owner asked to see (no background, rounded square, circle), on the P4_5 and
  at every board's width, and one slot's panel open.
- **(b) The slot mechanism**, data-driven (`header-slots.md` §8): a slot is a spec (its kind, its
  entity where it has one, its panel); a page's header is a list of them plus shown / hidden; one
  kit component draws it (roles in `KitStyles.cpp`, sizes as tokens); a new kind is a new entry. The
  four default slots on every page; the glyph style a setting with all three, so the owner can
  compare on glass. Today's header is `src/UI/Panel_Header.cpp`; today's drawer is
  `src/UI/Panel_System.cpp` (from the header's status icon).
- **(c) The slots' panels**: the system panel's drawer generalised - it slides from the slot's place;
  today's system panel becomes the status slot's. The other three start small (the clock: time,
  date, the next alarm's place; the page: its pages to jump to and its header shown / hidden; the
  climate slot: its sensor's value) - say in the sheet what each could hold later.
- **(d) The page setting** (header shown / hidden) and **(e) dashes** for no live value.
- **Open, for the owner** (`header-slots.md`): the glyph style; the clock's form (the owner's example
  writes "1:55am"); narrow boards; how a hidden header comes back; how a slot shows an error. Build
  each the conservative way and ask in the sheet.
- Measure: the clock redraws only on a new minute, a slot only when its value changes - one slot's
  area, never the bar (#19); `/bench` before and after.
- Stop at a clean point: a mock and a working four-slot header with one panel is a good night.

## What is next with the owner

1. **The unattended session's sheets**: `TEST_A17.md`, `TEST_2.8.md` (or how far it got).
2. **2.8**: the mock's three glyph styles, then the build's rounds. Cluster headers come with 2.11;
   the notification queue (the owner's, 2026-10-15) with #76.
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

The P4_5, 4B and 1060 run `v0.2.16` (with the local lines). Ports move when boards are re-plugged:
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
- **Save a board's `/settings` before flashing it** (`curl -o`) until A17 makes the scripts do it.
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
