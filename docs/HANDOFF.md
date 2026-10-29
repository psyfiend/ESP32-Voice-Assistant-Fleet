# Handoff — 2026-10-29, afternoon (2.11 group cards: rounds 1-14 built; round 14's test, then item 8)

**Start here.** This file is only: what to read, where we are, what to do next, what will bite you,
and how to work with the owner. The history of how things got here is in the test sheets
(`docs/archive/TEST_*.md`, `docs/TEST_2.11.md`), `DECISIONS.md` and `CHANGELOG.md`; earlier
handoffs are `docs/archive/HANDOFF_to_2026-10-14.md` and `HANDOFF_to_2026-09-29.md`.

## Read in this order - all of it, before starting

1. `CLAUDE.md` - the HAL/BSP, the rules, **and the file-editing rule at the top**.
2. `docs/ROADMAP.md` (the plan, and where each kind of note belongs) and `docs/DECISIONS.md` (every
   decision in one line - 2.11's are K88-K128).
3. **`docs/design/ui-architecture.md`**, then what it points at - `components.md` (the kit, the
   finishes), `tokens.md`, `header-slots.md`, `card-sheet.md` §14-18, `layout-engines.md` (the
   arranger: v8 as built, version 9, round 12), `cards.md`, `pages.md`, `card-layout.md` (before
   moving anything inside a card), `group-cards.md` §6-7 (2.11's design), `interview-phase2-3.md`.
4. **`docs/TEST_2.11.md`** - the milestone in progress: rounds 1-14, the owner's results, **the
   final pass (A1-F3, all PASS on 2026-10-29)** and round 14's tests R1-R9 (his to run).
5. **Home Assistant** (sibling folder `../Home Assistant/`, a separate git repo, read-only from
   here): `FLEET_REFERENCE.md` (the panels' entity map, alerts, header entities) and
   `HOUSE_RULES.md` (naming, labels, the logical layer). The House page reads its names since
   round 13 (K127). Changes to HA are asked for there, never made from this project.
6. `docs/LESSONS.md` - before debugging anything. `docs/HTTP_REFERENCE.md` - before calling a route.
   `host-tests/README.md` - before touching `src/Layout/`.
7. For the display: `docs/display/README.md` only - it indexes the rest.

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
| branch | 2026-10-26, morning | 2.11, eleven rounds | 63,201 | 259 |
| branch | 2026-10-26, evening | 2.11, rounds 1-12b | 63,373 | 259 |
| branch | 2026-10-29 | **2.11 so far** (rounds 1-14) | **63,569** | **259** |

Counted with `git grep -c '' <rev> -- <those paths>` over `.c .cpp .h .hpp`; recount the same way.

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

## Where 2.11 stands (2026-10-29)

- **The owner's final pass passed - every test** (A1-F3, 2026-10-29). His notes became **round 14**
  (K128: checkboxes, the 4-slot limit, Hide from the cluster asking, Add card to dashboard
  redesigned, panel sections, his words), built and flashed; **its tests R1-R9 are his to run**.
  Round 13 (K125-K127) moved the House page to HA's new entity ids, kept cluster-hidden members off
  the page's hidden list, sized solo cards to the tiles, and set the arranger's budget to 7,000
  (the 4B's search 10.2 s -> 4.9 s, the same page).
- **Branch `feat/2.11-group-cards`** (off `main` at `6bd834f`), pushed, **no PR** (it comes at
  sign-off). Rounds 1-14 (`TEST_2.11.md`). Built: clusters and
  the summary card; **the arranger, Layout Playground v9 in units** (host-tested against the
  playground's own answers: `big`, a card's preferred shape, **clusters grown into the spare height
  first** and what is left shared out, horizontal clusters preferred, a block's tightest shape kept;
  pages arrange themselves; Col / Row pin a count, Auto unpins); the cluster's window; Visibility by
  where a card is; the hidden cards (with "No room"); Copy / Move into and out of a cluster, tracked
  (K111, K112); **a development Reset** in the system panel; the roomy hero face on the 7" boards;
  **tiles as E** (K116), drawn **near square** with the hero and label centred and a bigger label
  where there is room (K122), **tile heights evened across a page** (K124); cluster headers in one order, on a
  faint band, with the glyph styles (K115).
- **Boards**: the P4_5, the 4B and the 1060 run round 14 (`0b6c978`) with their local `DEBUG_POPUP`
  lines. The P4_5's House pinned 5 x 4, its Fleet page 7 columns + 3 rows; the 4B's House pinned
  4 x 5 with Thermo hidden (left by his final pass - he may say they were tests); the 1060's House
  5 x 5, on Linen. Window close is off on all three (his). Kept layouts are `v4`. Settings copies
  from every flash: `bench/settings/<board>/202610*-*.json`; the P4_5's `20261029-1449-restore.json`
  is its dashboard as he wants it (his F3 Reset undone).
- **Artifacts**: [Layout Playground](https://claude.ai/artifact/SBSanCnwviNbKzQmkwiJXp) (page v12,
  engine "v9: grow, spread" - the spec for `Layout::arrange()`; its page is also
  `host-tests/tools/playground.html`); [2.11 Round 9 Mocks](https://claude.ai/artifact/SpSG1hhpbJsUmxBLv3cdfY)
  (tile borders A-E - E chosen - and the group view, after 2.11).
- **Where 2.11's pieces live**: a cluster `Cards/ClusterCard` + `Cards/MiniTile.cpp` (a tile's
  near-square height `ClusterCard::tileHeight()`, the page's shortest handed to every cluster in
  `CardPage::commit()` before the build; the label face in `MiniTile.cpp`'s `labelFace()`), its span
  and tile grid `Layout/Cluster.h`; the arranger `Layout/Arranger.h` (host-tested; the finish
  `finishPage()`) and `Cards/PageArranger` (its task at the idle priority, 24 KB stack; kept answers
  `pages.<slug>.layout` = `v4 fp CxR` then `c,r,w,h,tc` in units), `CardPage::arrangePage()` /
  `placeArranged()`; the pins in `GUIManager` (`_pinCols`, `saveGrid()` in `GUIManagerTweaks.cpp`);
  the Reset `CardAdded::resetToCompiled()` + `GUIManager::resetDashboard()`; the halo
  `UI/Kit/KitHalo.cpp`; the keyboard `UI/Kit/KitKeyboard.cpp`; the cluster's window
  `Popup/PopupCluster.cpp`; Visibility, Add to, which group `Popup/PopupVisibility.cpp`; the hidden
  cards `UI/HiddenSheet` + `GUIManagerHeader.cpp` (`collectHidden`); copies, moves and the pool
  `Cards/CardAdded`.
- **Changing the arranger**: the playground first (republish the artifact), copy its page to
  `host-tests/tools/playground.html`, regenerate `test_arranger` with
  `host-tests/tools/gen_arranger_tests.py` (~10 min, needs node), port to `src/Layout/Arranger.cpp`
  until `wsl bash host-tests/run_in_wsl.sh` passes. Bump the kept-answer version in
  `PageArranger.cpp` when the search changes. `host-tests/README.md` has the commands. **The
  boards' search budget is `Layout::ARR_BUDGET_BOARD` (7,000), a knob (`?arranger=budget`); the
  host tests run the playground's `ARR_BUDGET` (20,000)** through `pg()` in the generated tests.
- **Round 14's pieces**: the kit's check row, words row and chip grid (`UI/Kit/KitChoices.cpp`),
  the section title `Kit::drawerTitle()` (`KitDrawer.cpp`, `KS_SECTION_RULE_L/R`); the cluster's
  window `Popup/PopupCluster.cpp`; the Visibility note and Add card to dashboard
  `Popup/PopupVisibility.cpp` (`buildJoin()`, its picker `s_joinPicking`); the dialogs
  `Popup/PopupDialog.cpp`; the page panel `UI/SlotPanels.cpp`.
- **HA's names (round 13)**: `components/Fleet_Providers/ExternalEntities_HA.h` - the same entity
  ids (`office_desk`...), new `externalRef`s; `outdoor_temp` (`sensor.outdoor_temperature`) is on
  no card - the page header's sensor slot reads it (`UI/HeaderSlots.cpp`). The `ha_outside` group
  no longer exists in HA; a card saved with it falls back to its dashboard area's group
  (`CardGroups::ofCard()`), so nothing was rewritten.

## What is next

1. **2.11's sign-off.** Round 12 (2026-10-26) built the owner's notes after round 11 (K121-K123):
   clusters grow to fill the spare height (solo cards keep their size), horizontal clusters over
   vertical, the 4B's pinned 4 x 3 / 4 x 5 pages found (a block keeps its tightest shape), tiles
   near square with the hero and label centred and clear of the active frame, the bigger label on
   the P4_5 and 1060, and a fix for kept pages whose clusters could not grow. Round 12b fixed the
   4B's boot loop on Auto (the arranger's task starved the idle task; LESSONS), evened the tile
   heights across a page's clusters (halfway to the shortest) and settled the tile panel at 3%
   toward the text (K124). The owner's final pass then passed everything (2026-10-29), and round
   14 built his notes (K128). **Next: he runs round 14's R1-R9** (`TEST_2.11.md`); anything he
   finds is round 15, on the three dev boards. No questions are open (the 4B at 4 x 4 filling
   unevenly was left by him). When he signs off, **item 8**: the all-nine gate
   (`scripts/release/gate.ps1`, with `python scripts/structure_check.py`), the CHANGELOG entry for
   `v0.2.19`, the test sheet archived to `docs/archive/`, then `gh pr create` (body via
   `--body-file`). He merges; you pull, tag `v0.2.19` (annotated), push the tag, flash the boards
   on the desk. The consolidated final sheet he asked for is round 13's list - done.
2. **After 2.11** (ROADMAP 5a; K118, K119, K123, K127): HA's organisation, offered - the group
   view; Add cards from the entities labelled `fleet` (the linked area's first); a cluster linked
   to the HA area closest to its intent, or none, its header able to adopt that area's logical
   entities as members; an outdoor average (asked of the HA project). From the final pass: a header
   panel's temperature opening its History; a cluster's own card state; **UI Tweaks' "Show helper
   text"**. The owner was firm: the dashboard never has to follow HA's organisation, and the
   project must not depend on a particular HA layout (no manifest template).
3. **#76, on the claim model** (D-19): the notice queue ported from NINA's `nina_toast`, a notice's
   detailed form then its minimal "!" in status, a place that grows for a high-priority claim. Mock
   first.
4. Waiting: **#104** (a light's temperature mark moves between builds); **#100** the clocks (K81-K84;
   prototype on `feat/100-nina-clock`, PR #102 not for merging); **#84** "no data yet"; **#99's S3
   frame check** (W6) once an S3 is on the desk; the header clock's format (interview §3 Q3).

**After any merge**: `git pull`, tag the merge commit (annotated, `vX.Y.Z - <milestones>`), push the
tag, flash the boards on the desk. The CHANGELOG entry and the archived test sheet go on the branch
before the owner merges. **The owner merges** (W12); a PR is opened only after the owner's sign-off.

## Where the design interview stands

`docs/design/interview-phase2-3.md` is the blueprint for 2.8, 2.11 and Phase 3. §1 look and feel,
§2 card popups and §3 the header's slots are closed (§3 Q3, the clock's format, still open); **§4
(2.11) is answered and built** (`group-cards.md` §6-7, K88-K124; Q8 - "back" from a group page -
open, with the group view after 2.11). #87 (the companion web app, D-8) should be scoped before
3.1; the owner would start it with dashboard arranging (FUTURE_IMPROVEMENTS). Artifacts on
claude.ai: "Layout Playground" (page v12), "2.11 Round 9 Mocks", "2.11 Clusters", "Panel Edges",
"2.8 Header Slots", "Glass, Satin, Flat", "Card Popup Mock", "Beyond the Grid", "Linen Fill and
Header Glyphs", "Fleet Status Glyphs" - links in the design docs.

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
| `WS_P4_4B` | COM7 (CH343 `5AAF289419`) | P4 rev v1.0; local `DEBUG_POPUP`. **It boot-looped once on a software restart too** (round 11): restore with `-NoReboot`, then `esptool ... -p COM7 --after watchdog-reset read-mac` |
| `CYD_P4_1060` | COM9 (native USB, serial `30:ED:A0:E3:2C:7B`) | needs two cables; local `DEBUG_POPUP`. **Flash at 460800 with `--after watchdog-reset`** (LESSONS). The owner asked for it to be flashed with the dev boards (2026-10-20). **A software restart can boot-loop it** (its Wi-Fi co-processor): put a settings copy back with `-NoReboot`, then `esptool ... --after watchdog-reset read-mac` (LESSONS, round 9) |
| `WS_P4_7B` | COM6 (native USB) | needs two USB cables or it browns out on boot |
| `CYD_P4_4880` | COM19 (native USB) | LiPo fitted; browned out on the PC's USB alone |
| `WS_S3_4B` | COM8 (CH343 `5A67166678`) | **locked up** (#69); compile it as the S3 check, never flash it until the owner says |
| `CYD_S3_3248` | - | USB broken (2026-09-28) |
| `CYD_S3_8048`, `WS_S3_5B` | - | not plugged in |

The P4_5, the 4B and the 1060 run 2.11 round 14; the rest `v0.2.18` or older. Hosts:
`fleet-ws-p4-5`, `fleet-ws-p4-4b`, `fleet-cyd-p4-1060` (all in `HTTP_REFERENCE.md`). Environments:
`WS_P4_TOUCH_LCD_5`, `WS_P4_TOUCH_LCD_4B`, `CYD_P4_1060P470`. Ports move when boards are re-plugged:
`pio device list` and match the serial. **Is a board alive?** Ask Home Assistant
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

**WSL's Ubuntu has g++ 13.3** - the host tests run there (`host-tests/run_in_wsl.sh`). **node** is on
the PC's PATH - the arranger's test generator needs it.

**Do not use NINA's C6 updater** - it hung `WS_P4_5`. `reference/` is gitignored.

## Things that will bite you

- **`PYTHONIOENCODING=utf-8` before any `pio` or `esptool` whose output is piped** - pio dies
  silently otherwise, esptool with a UnicodeEncodeError; give an unattended upload a `timeout`.
  Builds run from Git Bash calling `~/.platformio/penv/Scripts/pio.exe` directly.
- **The repo's PowerShell scripts run from the PowerShell tool** (`& .\scripts\release\save_settings.ps1
  <host> <env>`); `powershell -File` from Git Bash is refused by the execution policy.
- **Windows PowerShell 5.1 makes a native program's stderr fatal under `$ErrorActionPreference =
  "Stop"`** - `gate.ps1` handles it; a new script that runs `pio` must too.
- **`pio run` with no `-e` builds the 7B only.** The all-nine gate: `powershell -ExecutionPolicy
  Bypass -File scripts/release/gate.ps1` (10-30 min, in the background); it refuses a dirty tree.
- **PlatformIO keeps one environment's build at a time** - copy an image you want to keep. Clear
  `.pio/build_cache` after a BSP, struct-layout or `lv_conf.h` edit.
- **Do not edit source while a build runs** - it compiles the half-finished edit.
- **Save a board's `/settings` before flashing it**: `save_settings.ps1 <host> <env>`; put a copy
  back with `restore_settings.ps1 <host> <file>` (it restarts the board unless `-NoReboot` - use
  that on the 4B and the 1060; a timeout can still mean it landed - read `/settings` to check).
  Compare with `python scripts/release/setdiff.py <saved>.json <now>.json` (`curl
  http://<host>/settings -o <now>.json`): a test that changed his settings is not done until it
  prints "0 difference(s)". Flash app-only: `esptool.exe --chip esp32p4 -p COMn
  -b 921600 write-flash 0x10000 <image>`, the 1060 at 460800 with `--after watchdog-reset`. Chain a
  flash after a build only on the build's success.
- **A kept arranger answer outlives a search change** unless its version moves (`v4` now, in
  `PageArranger.cpp`): bump it whenever the search's shapes or costs change, or the boards keep
  showing the old search's pages. **Time a search change on the 4B, unpinned** (its `[Arranger]`
  serial line gives steps and ms): it is the slowest board, and a page the owner pinned does not
  run the unpinned search at all - round 12's Auto went unseen until it boot-looped (LESSONS).
- **A task that never blocks runs at `tskIDLE_PRIORITY`** (or yields every second) - above it, the
  idle task starves and the task watchdog reboots the chip after 5 s. A boot loop: read the serial
  first (`task_wdt: ... CPU 0: <task>` names the culprit).
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
  file and use `git commit -F` / `--body-file`. Commit and push in separate calls; push with
  `git push origin feat/2.11-group-cards:feat/2.11-group-cards`.
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
