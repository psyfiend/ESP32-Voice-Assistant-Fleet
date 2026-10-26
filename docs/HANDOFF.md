# Handoff — 2026-10-26 (2.11 group cards: ten rounds; the last features, then sign-off)

**Start here.** This file is only: what to read, where we are, what to do next, what will bite you,
and how to work with the owner. The history of how things got here is in the test sheets
(`docs/archive/TEST_*.md`, `docs/TEST_2.11.md`), `DECISIONS.md` and `CHANGELOG.md`; earlier
handoffs are `docs/archive/HANDOFF_to_2026-10-14.md` and `HANDOFF_to_2026-09-29.md`.

## Read in this order - all of it, before starting

1. `CLAUDE.md` - the HAL/BSP, the rules, **and the file-editing rule at the top**.
2. `docs/ROADMAP.md` (the plan, and where each kind of note belongs) and `docs/DECISIONS.md` (every
   decision in one line - 2.11's are K88-K113).
3. **`docs/design/ui-architecture.md`**, then what it points at - `components.md` (the kit, the
   finishes), `tokens.md`, `header-slots.md`, `card-sheet.md` §14-18, `layout-engines.md` (v8, the
   arranger to port), `cards.md`, `pages.md`, `card-layout.md` (before moving anything inside a
   card), `group-cards.md` §6-7 (2.11's design), `interview-phase2-3.md`.
4. **`docs/TEST_2.11.md`** - the milestone in progress: nine rounds, the owner's results after the
   first seven, and round 9's questions for him.
5. `docs/LESSONS.md` - before debugging anything. `docs/HTTP_REFERENCE.md` - before calling a route.
6. For the display: `docs/display/README.md` only - it indexes the rest.

**Keep it short.** A finished investigation goes to `LESSONS.md`; a test run to its test sheet; a
decision to `DECISIONS.md`; a number to the doc that owns it. This file is rewritten at a handoff.

---

## Where the project is

**The last release is `v0.2.18`** (2026-10-17, milestone 2.8, the header bar v2); `CHANGELOG.md`
matches every version to its milestones. **The next is `v0.2.19`, milestone 2.11**, on its branch.

**Lines of code** - the project's own C/C++ (`src/`, `include/`, its own `components/`; generated
fonts, `lvgl`, `GFX_Library_for_Arduino`, `SensorLib`, `XPowersLib`, `bb_captouch_fork`,
`esp_websocket_client` and `include/third_party` left out; every line counted, comments too):

| Release | Date | Milestone | Lines | Files |
|---|---|---|---|---|
| `v0.1.0` | 2026-09-03 | Phase 1 | 9,039 | 44 |
| `v0.2.4` | 2026-09-15 | 2.4 (the tag's number was the milestone's until 2.7) | 18,745 | 98 |
| `v0.2.7` | 2026-09-24 | 2.7 | 28,207 | 117 |
| `v0.2.8`-`v0.2.11` | 10-05 to 10-12 | 2.10, the card popup | 50,115 | 183 |
| `v0.2.12` | 2026-10-13 | K17 | 50,872 | 185 |
| `v0.2.13` | 2026-10-14 | 2.12, the UI foundation | 52,957 | 222 |
| `v0.2.16` | 2026-10-15 | #99, satin and glass | 53,449 | 222 |
| `v0.2.18` | 2026-10-17 | 2.8, the header bar v2 | 57,224 | 244 |
| branch | 2026-10-20 | 2.11, seven rounds | 62,077 | 259 |
| branch | 2026-10-20, night | **2.11 so far** (nine rounds) | **62,907** | **259** |

Counted with `git grep -c '' <tag> -- <those paths>` over `.c .cpp .h .hpp`; recount the same way.

What a board does: boots into two pages (House = the owner's HA entities over the websocket, Fleet =
MQTT/system/virtual cards), swiped with wrap-around; three colour schemes (Midnight default, Fleet,
Linen); **clusters** - a group as one block of tiles under a header - and solo cards, **placed by the
arranger**; a long press opens a card's or a cluster's window (controls, members, scenes, History,
SETTINGS, Visibility, Card UI); the header's slots open panels (pages, hidden cards, the system
panel); the owner's choices saved by card id on LittleFS; HTTP routes for screenshots, benches,
settings and debugging (`HTTP_REFERENCE.md`).

**The UI, in one paragraph** (details: `ui-architecture.md`): every window is a `Kit::Sheet` client;
buttons, dialogs, rows, panels and the selector are the component kit (`Kit::`), each look a *role*
- shared styles that follow the scheme - and a tappable surface has a finish (satin, glass, lit
glass); every colour and size is a token, a window's sizes millimetres (`UIShape`); where cards go
is the arranger's (`Layout::arrange()`), where the header's slots go `Layout::HeaderPlan`'s. Changes
are proved with the harness (`scripts/ui_golden.py`), host tests (`host-tests/`), the structure
check and the all-nine gate. `GUIManager` and the system panel's pages are not on the kit yet -
bring each over when it is next changed (`ui-architecture.md` §6).

## Where 2.11 stands (2026-10-20, night)

- **Branch `feat/2.11-group-cards`** (off `main` at `6bd834f`), pushed at `7626049` (+ this
  handoff), **no PR** (it comes at sign-off). Nine rounds (`TEST_2.11.md`); rounds 8 and 9 were
  built while the owner was away and **wait for his tests and answers**. Built: clusters and the
  summary card; **the arranger, Playground v8 in units** (round 9, host-tested against the
  playground's own answers; pages arrange themselves; Col / Row pin a count, Auto unpins); the
  cluster's window; Visibility by where a card is; the hidden cards (with "No room"); Copy / Move
  into and out of a cluster, tracked (K111, K112); **a development Reset** in the system panel;
  **the roomy hero face** on the 7" boards (tiles' icons grow with their room); the halo on windows.
- **Boards**: the P4_5, the 4B and the 1060 run `7626049` with their local `DEBUG_POPUP` lines. Each
  page's earlier Col / Row choice is now a pin (the P4_5's House 4 x 2 - the owner's - the 4B's 4
  rows, the 1060's 5 x 2; `TEST_2.11.md` round 9 lists them). Settings copies from every flash in
  `bench/settings/<board>/20261020-*.json`.
- **Mocks waiting for the owner**: [2.11 Round 9 Mocks](https://claude.ai/artifact/SpSG1hhpbJsUmxBLv3cdfY)
  - tile borders (four ways) and the group view (a window of a group's members as tiles; a cluster
  as one card with its icon). The Layout Playground (v8) stays the place to tune the knobs.
- **Where 2.11's pieces live**: a cluster `Cards/ClusterCard` + `Cards/MiniTile.cpp`, its span and
  tile grid `Layout/Cluster.h`; the arranger `Layout/Arranger.h` (host-tested) and
  `Cards/PageArranger` (its task, 24 KB stack; kept answers `pages.<slug>.layout` = `v2 fp CxR`
  and `c,r,w,h,tc` in units), `CardPage::arrangePage()` / `useUnits()`; the pins in `GUIManager`
  (`_pinCols`, `saveGrid()` in `GUIManagerTweaks.cpp`); the Reset `CardAdded::resetToCompiled()`
  + `GUIManager::resetDashboard()`; the halo
  `UI/Kit/KitHalo.cpp`; the keyboard `UI/Kit/KitKeyboard.cpp`; the cluster's window
  `Popup/PopupCluster.cpp`; Visibility, Add to, which group `Popup/PopupVisibility.cpp`; the hidden
  cards `UI/HiddenSheet` + `GUIManagerHeader.cpp` (`collectHidden`); copies, moves and the pool
  `Cards/CardAdded`.

## What is next

1. **2.11's last features, then sign-off.** The owner answered round 9 on 2026-10-26 (DECISIONS
   K114-K120; `TEST_2.11.md` "Your results" after round 9, and round 10 - the names and the
   cluster headers, built). **He tests once, at the end** - no more test rounds for him until the
   features are done. Left, in `TEST_2.11.md` round 10 "Left for the next session":
   - **Bugs on the glass**: in Fleet and Midnight, where a slot panel's tab meets the bar's accent
     line, a small square of the panel's face covers the line just outside each of the tab's lower
     corners (the line breaks in a notch beside the tab, both sides of the page slot's, the left
     of the status slot's); in Linen a slot panel at the screen's edge shows the bar's drop shadow
     along its own outer edge; check the Band now reaches the border (round 10's fix, untested in
     Linen).
   - **The 4B's arranger bug** (rows added become empty space) - read `/popup`'s second line on a
     test copy, row by row; likely "no page" and the flow fallback.
   - **The arranger, next (K120)**: solo cards no bigger than a cluster; centred rows and symmetric
     solo cards, or even space round them (the owner liked the 1060 at 7 x 5); a card's preferred
     span (the Fleet page's Free Heap and Uptime, 1.5 cells). **Playground v9 first** - the owner
     reacts to a page, then the port.
   - **Active tiles wear the card state** (K116; inactive: C or E in the mock), **sensor icons in
     the header's glyph styles** (K115).
   - Two of his answers wait on a question: K117 (copies made on the board - keep Remove?) and
     K118 (tiles or cards in a group view and a group page).
   Then **item 8**: **one final test sheet** for the owner (he asked for it), the all-nine gate with
   the structure check, the CHANGELOG entry, the sheet archived, the PR; the owner merges; tag
   `v0.2.19`. The owner is bringing `dashboard-target-7b.md` up to date with clusters as the new
   baseline, with entities added to fill the screen.
2. **#76, on the claim model** (D-19): the notice queue ported from NINA's `nina_toast`, a notice's
   detailed form then its minimal "!" in status, a place that grows for a high-priority claim. Mock
   first.
3. Waiting: **#104** (a light's temperature mark moves between builds); **#100** the clocks (K81-K84;
   prototype on `feat/100-nina-clock`, PR #102 not for merging); **#84** "no data yet"; **#99's S3
   frame check** (W6) once an S3 is on the desk; the header clock's format (interview §3 Q3).

**After any merge**: `git pull`, tag the merge commit (annotated, `vX.Y.Z - <milestones>`), push the
tag, flash the boards on the desk. The CHANGELOG entry and the archived test sheet go on the branch
before the owner merges. **The owner merges** (W12); a PR is opened only after the owner's sign-off.

## Where the design interview stands

`docs/design/interview-phase2-3.md` is the blueprint for 2.8, 2.11 and Phase 3. §1 look and feel,
§2 card popups and §3 the header's slots are closed (§3 Q3, the clock's format, still open); **§4
(2.11) is answered and being built** (`group-cards.md` §6-7, K88-K113; Q8 - "back" from a group
page - open). #87 (the companion web app, D-8) should be scoped before 3.1; the owner would start it
with dashboard arranging (FUTURE_IMPROVEMENTS). Artifacts on claude.ai: "Layout Playground" (v8),
"2.11 Clusters", "Panel Edges", "2.8 Header Slots", "Glass, Satin, Flat", "Card Popup Mock",
"Beyond the Grid", "Linen Fill and Header Glyphs", "Fleet Status Glyphs" - links in the design docs.

## Fires still burning - each has an owner doc or issue, none needs the next session

| What | State | Where |
|---|---|---|
| **Battery** | prototype merged; `-D HAS_BATTERY` (7B, 4880); **owner: fuel gauges (MAX17043, ordered)** | #72, `design/power-battery.md` |
| **2.9 last boards** | `CYD_S3_8048`, `WS_S3_5B` to esp_lcd when on the desk | #67, `display/README.md` |
| **4B panel timing** | **S3_4B locked up 2026-10-05**, left as found - read #69's latest comment first | #69 |
| **S3 LVGL speed** | the S3_4B feels heavier since LVGL's pool moved to PSRAM; paused | #70 |
| The card window | #91 (the P4_5's white flash under sustained drawing), #90, #89, #85 | issues; `FUTURE_IMPROVEMENTS.md` |
| Rotation / IMU; type ladder | recorded | #71, #73 |

## Boards on the desk

| Board | Port | Notes |
|---|---|---|
| `WS_P4_5` | COM15 (CH343 `5B90154141`) | the dev board, in its enclosure; local lines `DEBUG_POPUP` + `DEBUG_HANG` |
| `WS_P4_4B` | COM7 (CH343 `5AAF289419`) | P4 rev v1.0; local `DEBUG_POPUP` |
| `CYD_P4_1060` | COM9 (native USB, serial `30:ED:A0:E3:2C:7B`) | needs two cables; local `DEBUG_POPUP`. **Flash at 460800 with `--after watchdog-reset`** (LESSONS). The owner asked for it to be flashed with the dev boards (2026-10-20). **A software restart can boot-loop it** (its Wi-Fi co-processor): put a settings copy back WITHOUT `?reboot=1`, then `esptool ... --after watchdog-reset read-mac` (LESSONS, round 9) |
| `WS_P4_7B` | COM6 (native USB) | needs two USB cables or it browns out on boot |
| `CYD_P4_4880` | COM19 (native USB) | LiPo fitted; browned out on the PC's USB alone |
| `WS_S3_4B` | COM8 (CH343 `5A67166678`) | **locked up** (#69); compile it as the S3 check, never flash it until the owner says |
| `CYD_S3_3248` | - | USB broken (2026-09-28) |
| `CYD_S3_8048`, `WS_S3_5B` | - | not plugged in |

The P4_5, the 4B and the 1060 run 2.11 round 9; the rest `v0.2.18` or older. Ports move when boards
are re-plugged: `pio device list` and match the serial. **Is a board alive?** Ask Home Assistant
(`sensor.<device>_uptime`) or `GET /settings?stats=1`, not the cable. While developing, build and
flash the P4_5, the 4B and the 1060; all nine compile once, right before a merge. **Away from the
desk** the owner reflashes the P4_5 from the laptop (COM6); it is then at `http://10.0.0.2`.
Flash and PSRAM per board: `HARDWARE_STATUS.md`.

## Environment state git cannot see

**The ESP32-P4 framework libraries are REBUILT, not stock** (`esp32p4_es` in
`~/.platformio/packages/framework-arduinoespressif32-libs/`): `MEMPOOL_PREFER_SPIRAM` and a 64-byte
L2 line (#49), `LCD_DSI_ISR_CACHE_SAFE` (A14) and IDF's **PPA rotation patch** (the freezes, A15,
`scripts/idf-patches/`). Beside it: `esp32p4_es.cache_safe`, `.hosted_fix`, `.stock.55.03.311` -
rollback is a rename. **A `pio pkg update` or platform reinstall silently puts the bugs back.** How:
`docs/REBUILD_P4_LIBS.md`; the GitHub pre-release `p4-libs-2026-10-09`; check a build with
`grep -a -o "v5.5.5-832-g2553c5ad432[-a-z]*" .pio/build/<env>/firmware.bin` (it must end `-dirty`).
The laptop has the same folder. The S3 libraries are stock.

**The owner's local debug lines** in `platformio.ini` are uncommitted and never committed - the
pre-commit hook refuses them; `scripts/release/gate.ps1` stashes and restores them. The laptop's copy
(skip-worktree, its own path prefix) has `DEBUG_POPUP` and `DEBUG_FRAMES` on the P4_5.

**WSL's Ubuntu has g++ 13.3** - the host tests run there (`host-tests/run_in_wsl.sh`).

**Do not use NINA's C6 updater** - it hung `WS_P4_5`. `reference/` is gitignored.

## Things that will bite you

- **`PYTHONIOENCODING=utf-8` before any `pio` whose output is piped** - it dies silently otherwise;
  give an unattended upload a `timeout`. Builds run from Git Bash calling
  `~/.platformio/penv/Scripts/pio.exe` directly.
- **Windows PowerShell 5.1 makes a native program's stderr fatal under `$ErrorActionPreference =
  "Stop"`** - `gate.ps1` handles it; a new script that runs `pio` must too.
- **`pio run` with no `-e` builds the 7B only.** The all-nine gate: `powershell -ExecutionPolicy
  Bypass -File scripts/release/gate.ps1` (10-30 min, in the background); it refuses a dirty tree.
- **PlatformIO keeps one environment's build at a time** - copy an image you want to keep. Clear
  `.pio/build_cache` after a BSP, struct-layout or `lv_conf.h` edit.
- **Do not edit source while a build runs** - it compiles the half-finished edit.
- **Save a board's `/settings` before flashing it**: `scripts/release/save_settings.ps1 <host>
  <board>`; put a copy back with `restore_settings.ps1` (it restarts the board; a timeout can still
  mean it landed - read `/settings` to check). Flash app-only: `esptool.exe --chip esp32p4 -p COMn
  -b 921600 write-flash 0x10000 <image>`, the 1060 at 460800 with `--after watchdog-reset`. Chain a
  flash after a build only on the build's success.
- **LVGL 9.5 clips an object's `DRAW_POST` to its own area** - anything drawn outside an object goes
  in `DRAW_MAIN_END` (LESSONS). An image meant to continue LVGL's drawing must blend in 16 bits.
- **A screenshot is not the glass**: `/screenshot` renders the object tree afresh, so it can show
  what the panel does not (round 6's missing shading). `/screenshot?fb=1` is the panel's own frame
  buffer (portrait on the P4_5).
- **Never read from a DSI panel that is showing a picture** - it blanked all three P4s. If a freeze
  returns, `GET /hang` on the P4_5 before anyone resets it. The 4B washed out twice ever: next time
  `GET /panel?resend=1` before resetting it.
- **LVGL clips children to their parent** (`UI::unclipShadows()` where a shadow must show); never
  toggle HIDDEN on a screen-sized object as an animation starts (LESSONS); anything drawn on a panel
  stays ASCII except `°`.
- **The harness**: never two runs at one board; the 1060 alone. A window closes itself after a minute
  (the 4B's own file: 10 s - use `--only`).
- **A serial monitor resets the board when it opens** - attach with `--rts 0 --dtr 0`.
- **HTTP handlers and the HA websocket task never touch LVGL.** Prefer ESP-IDF to Arduino-only (W4).
- **The P4_5 serves 7 of `HttpServer::MAX_ROUTES` (8)** - the next route needs it raised.
- **`Edit` on a CRLF file**: deleting a line by matching a leading newline joins two lines.
- **`git commit -m` and `gh ... --body` in PowerShell 5.1 drop double quotes** - write the text to a
  file and use `git commit -F` / `--body-file`.
- **Verify from outside the device** - the router, the broker, HA, `/bench?what=verify`, the harness.

## How to work with this owner

A hobbyist and ESP32 enthusiast, not a professional developer, and explicit about that - but reads
code, spots real bugs, and has caught several that were not obvious. **Treat the owner's instincts
as data** - #99's rail, 2.8's four slots and 2.11's arranger rules were the owner's. A little
colour-blind: never distinguish two states by colour alone.

**What works:**
- **Show, don't spec.** Build something to react to (a mock artifact, the Layout Playground); the
  owner flashes fast and tests thoroughly against a sheet - `docs/TEST_<topic>.md`, numbered tests
  with PASS criteria, the owner's results under them. Rounds go on one branch; the all-nine gate and
  the PR come once, at sign-off.
- **A PR only after the owner's sign-off**: push the branch, the owner tests and answers, then
  `gh pr create`; the owner merges.
- **Snappy and responsive first, eye candy second** (W6) - he noticed round 5's faster panels.
- **While the owner is testing, give ONE report when he says the testing is done.**
- **Read the tool, not the name**; **make the case for any install first** - the owner decides.
- **Plain language, not metaphor.** State what things are; avoid "X, not Y" framings.
- **Never assume. Give the top 2-3 options with a recommendation, and ask** before building on an
  assumption; discuss a structural choice a turn before implementing it.
- **Own mistakes plainly**; **say plainly what is unexplained**; **say what to expect** before
  anything that changes the screen; say what was not tested on the glass.
- **Unattended work is welcome** when the brief is clear. Commit and push at the end of each step.
- **Docs when there is something to record** (2026-10-20): DECISIONS after a settled discussion,
  design docs when something is decided, LESSONS as learned, ROADMAP as achievements are checked
  off, the test sheet each round. **HANDOFF only at a handoff** - rewritten then, with the lines of
  code above recounted.
- **Recommend a handoff as context nears half** - before starting something big, or at a good
  stopping point mid-task - and prepare it (docs, this file, the next session's brief).

**What to avoid:** committing straight to `main` (the guard refuses it anyway); guessing a fourth
time - instrument it or ask the far end; claiming a script worked because it printed something.

**Real lights (W9):** any Office light may be commanded while testing, put back as found; ask
before the Kitchen.

**Versioning (W3):** `v0.<phase>.<release>`; the release counts signed-off merges to `main` that
change the firmware (a docs-only merge is not tagged). **The next is `v0.2.19`.**
