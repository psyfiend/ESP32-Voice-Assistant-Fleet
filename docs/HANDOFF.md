# Handoff — 2026-11-01 (2.11's follow-ups released as `v0.2.20`; next: designing 2.13 with Home Assistant)

**Start here.** This file is only: what to read, where we are, what to do next, what will bite you,
and how to work with the owner. The history of how things got here is in the test sheets
(`docs/archive/TEST_*.md`), `DECISIONS.md` and `CHANGELOG.md`; earlier handoffs are
`docs/archive/HANDOFF_to_2026-10-14.md` and `HANDOFF_to_2026-09-29.md`, and this file's own history
is in git.

## Read in this order - all of it, before starting

1. `CLAUDE.md` - the HAL/BSP, the rules, **and the file-editing rule at the top**.
2. `docs/ROADMAP.md` (the plan; **its running order was rewritten after 2.11**, K131) and
   `docs/DECISIONS.md` (every decision in one line - 2.11's are K88-K129, **2.13's so far K130-K136**,
   the follow-ups' K137).
3. **[#116](https://github.com/psyfiend/ESP32-Voice-Assistant-Fleet/issues/116)** - 2.13's issue:
   what is decided, what is open, the order of work. Keep it current as things settle.
4. **`docs/design/ui-architecture.md`**, then what it points at - `components.md`, `tokens.md`,
   `header-slots.md` (the claim model cluster and header slots build on), `card-sheet.md` §14-18,
   `layout-engines.md` (the arranger), `cards.md`, `pages.md`, `card-layout.md`, `group-cards.md`
   §6-7 (2.11 as built, and since `v0.2.20`), `interview-phase2-3.md`.
5. **Home Assistant** (sibling folder `../Home Assistant/`, its own git repo): `HOUSE_RULES.md`,
   `FLEET_REFERENCE.md`, `docs/entity_reference.md`, its `ROADMAP.md`, and **2.13's log
   `projects/2026-10_fleet-2.13/README.md`**. 2.13 is half HA work - see "Home Assistant" below.
6. `docs/archive/TEST_2.11.md` and `TEST_2.11_FOLLOWUPS.md` - how the last milestone was tested and
   what the owner said.
7. `docs/LESSONS.md` before debugging anything; `docs/HTTP_REFERENCE.md` before calling a route;
   `host-tests/README.md` before touching `src/Layout/`; `docs/display/README.md` for the display.

**Keep it short.** A finished investigation goes to `LESSONS.md`; a test run to its test sheet; a
decision to `DECISIONS.md`; a number to the doc that owns it. This file is rewritten at a handoff.

---

## Where the project is

**2.11 and its follow-ups are finished.** 2.11 (group cards, the arranger) is `v0.2.19`. The
owner's notes on it became the follow-ups, two rounds signed off 2026-10-31: moving a card in his own
words, **Restore** as the one way back, an emptied cluster hiding, HA's full names in Members, a
swipe in a sheet never a tap (K137). It is **`v0.2.20`**:
[PR #115](https://github.com/psyfiend/ESP32-Voice-Assistant-Fleet/pull/115), merged 2026-11-01 and
tagged on the merge commit `19cd4bb`. The owner: "I feel like we finally have full control over what
cards go where now on the dashboard!"

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
| `v0.2.19` | 2026-10-30 | 2.11, group cards (16 rounds) | 63,694 | 259 |
| `v0.2.20` | 2026-11-01 | **2.11 follow-ups** (2 rounds) | **64,629** | **260** |

Counted with `git grep -c -e '^' <rev> -- src include components ':!components/lvgl' ...` (the
exclusions above, and `':!*font*' ':!*Font*'`), summing `.c .cpp .h .hpp`. **In PowerShell use
`-e '^'`, not `''`**: Windows PowerShell 5.1 drops an empty argument to a native program, and the
count comes back 0. The same command gives `v0.2.19`'s 63,694 / 259.

What a board does: boots into two pages (House = the owner's HA entities over the websocket, Fleet =
MQTT/system/virtual cards), swiped with wrap-around; three colour schemes; **clusters** and solo
cards **placed by the arranger**; a long press opens a card's window (controls, members, scenes,
History, SETTINGS, Visibility, Card UI) or a cluster's (its Members, with the CLUSTER panel); cards
moved, copied, regrouped and hidden on the glass, each with Restore; the header's slots open panels;
the owner's choices saved by card id on LittleFS; HTTP routes for screenshots, benches, settings and
debugging (`HTTP_REFERENCE.md`).

**The UI, in one paragraph** (details: `ui-architecture.md`): every window is a `Kit::Sheet` client;
buttons, dialogs, rows, panels and the selector are the component kit (`Kit::`), each look a *role*
that follows the scheme, a tappable surface has a finish (satin, glass, lit glass); every colour and
size is a token, a window's sizes millimetres (`UIShape`); where cards go is the arranger's
(`Layout::arrange()`), where the header's slots go `Layout::HeaderPlan`'s. Changes are proved with
the harness (`scripts/ui_golden.py`), host tests, the structure check and the all-nine gate.
`GUIManager` and the system panel's pages are not on the kit yet - bring each over when next changed.

**Where card management lives** (2.11 and `v0.2.20`): copies, moves, the pool and Restore's records
`Cards/CardAdded` (`home_name`, `home_group`, `moved_to`, `orig_group`; `MAX` = 24 added cards,
`CardAdded.h:39`); the moves' views and dialogs `Popup/PopupMove.cpp` (every word in it is the
owner's - change none without him); Visibility `Popup/PopupVisibility.cpp`; Members
`Popup/PopupMembers.cpp`; a cluster's window `Popup/PopupCluster.cpp`; the hidden cards
`UI/HiddenSheet`; a cluster `Cards/ClusterCard` + `Cards/MiniTile.cpp`; the arranger
`Layout/Arranger.h` (host-tested) and `Cards/PageArranger` (kept answers `v4` - bump it when the
search changes).

## What is next - 2.13, design first

**Nothing is built until it is agreed** (K132). The owner's order, with the reasoning in #116:

1. **Settle the two open HA choices** - artifact
   [HA Side of 2.13](https://claude.ai/artifact/Y3WwPvo81obqDRYEVpsMoQ) lays them out with mocks:
   - **Choice 3, per-room status entities** for cluster headers. Recommended: one small entity per
     room per kind (`sensor.office_lights_on`, `sensor.garage_doors_open`, `sensor.office_alerts`),
     about 20-25. The alternative - boards reading `sensor.lights_on`'s `by_area` - breaks K134 (a
     slot reads a state).
   - **Choice 4, an alert's and a notice's anatomy.** Recommended: attributes `message` / `area` /
     `cause` on each alert, and `script.fleet_notice` firing an event the boards listen for. Also:
     HA's own **Security dashboard** must work as fully as the boards - check its pickers can show
     what the boards will.
2. **The firmware questions** (#116), each to answer before designing on it:
   - **Entities at runtime** - the main firmware work. HA entities are compiled today
     (`components/Fleet_Providers/ExternalEntities_HA.h`); an added card needs an entity created at
     runtime, kept in the settings file, subscribed on the websocket. The registry holds
     `ENTITY_MAX` = 128 (`EntityRegistry.h:51`) and `CardAdded::MAX` = 24: size both against the
     owner's house before promising anything.
   - Add card to cluster listing the linked HA area's entities (K133); Add cards from the `fleet`
     label (K119, K123); the group view (K118, artifact "2.11 Round 9 Mocks").
   - A cluster's "all off" with no HA light groups: its member list, or a service call targeting
     the area - check what the command path can target today (entity ids only, as far as known).
   - `unknown` after an HA restart rendered as unavailable (with #84).
   - What an entity with no card type yet becomes (the thermostat).
   - From 2.11's final pass: a header panel's temperature opening its History; a cluster's own card
     state; UI Tweaks' "Show helper text".
3. **Mocks**, as artifacts - the owner reacts to things he can see, not to specs.
4. **A design doc** (`docs/design/`, say `ha-cards.md`), DECISIONS rows as things settle, #116 kept
   current.
5. **The first HA job: the YAML conversion** (K136) - the logical and status layers, generated today
   by `logical/house_logical.py` into `packages/house/house_rooms.yaml` and `house_status.yaml`,
   become hand-written YAML organised by function, in discrete files the way
   `packages/sprinklers/v4_*` are. The layout is still to agree. Log every change in the project
   folder's README (what, why, how to undo).
6. **Then build**, on a branch off `main`, in rounds against `docs/TEST_2.13.md`.

**After 2.13** (K131): **2.15** the bedroom alarm clock (#35, #100, #74, #75, night dimming, weather
and temperature cards, **OTA 5.1/5.2 pulled forward** - a new partition table, one full flash per
board with the settings carried across); then **2.14** alerts (#76, built once the night face
exists); then **4.3** the navbar; then **Phase 3, shrunk** to exporting and importing the settings
file, with #87. The issues for 2.15 (#35, #28, #29, #34) do not say this yet - update them when
2.15 starts.

**Waiting, not 2.13's**: #104 (a light's temperature mark moves between builds); #99's S3 frame
check once an S3 is on the desk; the header clock's format (interview §3 Q3); #71's rotation
switcher (rotate-and-restart is small) and the 4880 in portrait.

**After any merge**: pull `main`, tag the merge commit (annotated, `vX.Y.Z - <milestones>`), push the
tag, flash the boards on the desk. The CHANGELOG entry and the archived test sheet go on the branch
before the owner merges. **The owner merges** (W12); a PR is opened only after his sign-off.

## Home Assistant - the other half of 2.13

**Access is settled (K132):** a session here may change HA directly - through the `ha-mcp` tools
(**read the server's best-practices skill with `ha_get_skill_guide()` before any change**) and the HA
folder's own process (`HOUSE_RULES.md` §6: change, check config, reload, `tools/config_snapshot.py
"message"`, a full backup before anything irreversible). **The owner must be able to read and manage
everything built there without AI** - he knows YAML, not Python (K136). Nothing from HA is imposed on
the boards (K133).

**How a milestone is recorded there (K135):** `projects/2026-10_fleet-2.13/README.md` is the log;
the tags `fleet-2.13-start` (set, on `3d331bd`) and `fleet-2.13-done` (at sign-off) bound it. No
branches: HA has one live configuration. Its change log says "Nothing yet".

**What HA offers now** (`HOUSE_RULES.md`, `FLEET_REFERENCE.md`): three layers - physical
(swappable), logical (`sensor.office_temperature`, permanent; `source` names the member in use,
`min` / `max` / `mean` with two or more) and status (roll-ups and alerts). Labels: `fleet`,
`room_climate`, `perimeter`, `alert_high`, `alert_low`, `no_alerts`, `test`. The contract:
`sensor.alerts_active`, `sensor.doors_open`, `sensor.lights_on` (`by_area`, keyed by area id),
`sensor.house_temperature`, `sensor.outdoor_temperature`, `binary_sensor.garage_doors`,
`sensor.batteries_low`, `sensor.devices_unavailable`, `schedule.night_hours`. No room-wide light
groups: a room is controlled by targeting its area. **To reword** (K133): HA's `FLEET_REFERENCE.md`
rule 3 and `HOUSE_RULES.md` §7, which read as if the boards must follow HA.

**Where the HA repo is:** on the desktop it is the original; the laptop has a clone of
`\\control.lan\Home Assistant` at `C:\Users\eric\Documents\Projects\Home Assistant` (in step at
`543ff2b`). Push from the laptop to a `laptop` branch - the desktop's checked-out branch refuses a
push. `private/` and `secrets.yaml` never leave the desktop; the ha-mcp URL is a secret.

## Boards on the desk

| Board | Port | Notes |
|---|---|---|
| `WS_P4_5` | COM15 desktop / COM6 laptop (CH343 `5B90154141`) | the dev board. **Runs round 2's test build `34fdd2e`, and its settings are the tests'** - restore them (below) |
| `WS_P4_4B` | COM7 (CH343 `5AAF289419`) | `v0.2.19`. **A software restart can boot-loop it**: restore with `-NoReboot`, then `esptool ... -p COM7 --after watchdog-reset read-mac` |
| `CYD_P4_1060` | COM9 (native USB, `30:ED:A0:E3:2C:7B`) | `v0.2.19`; two cables; **flash at 460800 with `--after watchdog-reset`**; same boot-loop care as the 4B |
| `WS_P4_7B` | COM6 desktop (native USB) | needs two USB cables or it browns out on boot |
| `CYD_P4_4880` | COM19 (native USB) | `v0.2.19`, flashed by hand (`flash_release.ps1` does not cover it - build from a clean worktree at the tag); host `fleet-cyd-p4-4880` |
| `WS_S3_4B` | COM8 (CH343 `5A67166678`) | **locked up** (#69); compile it as the S3 check, never flash it until the owner says |
| `CYD_S3_3248` | - | USB broken (2026-09-28) |
| `CYD_S3_8048`, `WS_S3_5B` | - | not plugged in |

**First job at the desktop, before any 2.13 work:**

1. `flash_release.ps1` for `v0.2.20` (it saves each board's settings first).
2. Put the P4_5's settings back from `bench/settings/WS_P4_TOUCH_LCD_5/20261031-1416.json` with
   `restore_settings.ps1`; `setdiff.py` must print "0 difference(s)". The round-2 tests ran on the
   laptop with no network, so they changed his dashboard with nothing saved there.
3. The 4880 by hand from a clean worktree at `v0.2.20`.

Hosts: `fleet-ws-p4-5`, `fleet-ws-p4-4b`, `fleet-cyd-p4-1060` (`HTTP_REFERENCE.md`). Ports move when
boards are re-plugged: `pio device list` and match the serial. **Is a board alive?** Ask HA
(`sensor.<device>_uptime`) or `GET /settings?stats=1`. While developing, build and flash the P4_5,
the 4B and the 1060; all nine compile once, right before a merge.

## Environment state git cannot see

**The ESP32-P4 framework libraries are REBUILT, not stock** (`esp32p4_es` in
`~/.platformio/packages/framework-arduinoespressif32-libs/`): #49's PSRAM and cache-line changes,
`LCD_DSI_ISR_CACHE_SAFE` (A14), IDF's PPA rotation patch (A15). **A `pio pkg update` or platform
reinstall silently puts the bugs back.** How: `docs/REBUILD_P4_LIBS.md`; the pre-release
`p4-libs-2026-10-09`; check a build with `grep -a -o "v5.5.5-832-g2553c5ad432[-a-z]*"
.pio/build/<env>/firmware.bin` (it must end `-dirty`). The laptop has the same folder.

**The owner's local debug lines** in `platformio.ini` (`DEBUG_POPUP`, `DEBUG_HANG` on the P4_5) are
never committed - the pre-commit hook refuses them; `gate.ps1` and `flash_release.ps1` stash them.

**The laptop is different** (`C:\Users\eric\Documents\Projects\ESP32-Voice-Assistant-Fleet`, no
spaces): its `platformio.ini` has its own `symlink://` prefix and is marked **skip-worktree**. So:
- **`gate.ps1` there does not stash it** - it sees "no local changes" and builds with the laptop's
  paths and debug lines: a compile check only (say so in the PR). It took 1:38 there, cold.
- **Switching to a branch whose `platformio.ini` differs is refused.** Tag a merge without checking
  `main` out: `git fetch origin main:main`, then `git tag -a vX.Y.Z <merge> -m "..."`. Before a real
  switch: `--no-skip-worktree`, restore the file, switch, redo the prefix, `--skip-worktree`.
- **`core.hooksPath` is unset there** - the guard will not let Claude set it; the owner runs
  `git config core.hooksPath scripts/hooks` himself. Until then byte-scan by hand before committing.
- `platformio.ini.laptop` is an untracked backup; leave it.

**WSL's Ubuntu has g++ 13.3** (host tests); **node** is on the desktop's PATH (the arranger's test
generator). **Do not use NINA's C6 updater** - it hung `WS_P4_5`. `reference/` is gitignored.

## Things that will bite you

- **`PYTHONIOENCODING=utf-8` before any `pio` or `esptool` whose output is piped**; give an
  unattended upload a `timeout`.
- **The repo's PowerShell scripts run from the PowerShell tool**; `powershell -File` from Git Bash is
  refused by the execution policy.
- **`pio run` with no `-e` builds the 7B only.** The all-nine gate: `powershell -ExecutionPolicy
  Bypass -File scripts/release/gate.ps1` (~15 min on the desktop, in the background); it refuses a
  dirty tree. **Do not edit source while a build runs.**
- **PlatformIO keeps one environment's build at a time** - copy an image you want to keep. Clear
  `.pio/build_cache` after a BSP, struct-layout or `lv_conf.h` edit.
- **Save a board's `/settings` before flashing it**; put a copy back with `restore_settings.ps1`
  (`-NoReboot` on the 4B and the 1060); check with `setdiff.py`: "0 difference(s)". Flash app-only:
  `esptool.exe --chip esp32p4 -p COMn -b 921600 write-flash 0x10000 <image>`.
- **DECISIONS numbers are claimed by branch.** Two branches each took K131 this time; check
  `git log --all -S "| K1"` (or the open branches' DECISIONS) before numbering a row.
- **A kept arranger answer outlives a search change** unless its version moves (`v4`). Time a search
  change on the 4B, unpinned.
- **A task that never blocks runs at `tskIDLE_PRIORITY`** or the task watchdog reboots the chip.
- **LVGL 9.5 clips `DRAW_POST` to the object** - draw outside it in `DRAW_MAIN_END`. Animate what
  appears inside a panel, not the panel; never add children to a kit component.
- **A screenshot is not the glass**; `/screenshot?fb=1` is the frame buffer. **Never read from a DSI
  panel that is showing a picture.**
- **A serial monitor resets the board when it opens** - attach with `--rts 0 --dtr 0`.
- **HTTP handlers and the HA websocket task never touch LVGL**; anything that writes flash runs on a
  task with an internal-RAM stack. The P4_5 serves 7 of `HttpServer::MAX_ROUTES` (8).
- **`Edit` on a CRLF file**: deleting a line by matching a leading newline joins two lines.
- **`git commit -m` and `gh ... --body` in PowerShell 5.1 drop double quotes** - write to a file and
  use `git commit -F` / `--body-file`. Commit and push in separate calls.
- **Verify from outside the device** - the router, the broker, HA, `/bench?what=verify`, the harness.

## How to work with this owner

A hobbyist and ESP32 enthusiast, not a professional developer, and explicit about that - but reads
code, spots real bugs, and his instincts have shaped the best parts of the UI (#99's rail, 2.8's
slots, the arranger's rules, K129, the follow-ups' whole move design). **Treat them as data.** A
little colour-blind: never distinguish two states by colour alone.

- **Show, don't spec.** Mock artifacts first; he tests thoroughly against a sheet
  (`docs/TEST_<topic>.md`, numbered tests with PASS criteria, his results under them). Rounds go on
  one branch; the gate and the PR come once, at sign-off.
- **No on-screen words unless agreed with him first.** US English on screen ("Color"). When a case
  needs words he has not given, build the nearest and list them under "to confirm".
- **Discuss a structural choice a turn or two before building it.** Give the top 2-3 options with a
  recommendation. Never assume.
- **Plain language.** State what things are; skip "X, not Y" framings and metaphor.
- **Snappy first, eye candy second**; motion that jerks is a bug to him.
- **One report when he says the testing is done**; say what to expect before anything changes the
  screen; say what was not tested on the glass. Own mistakes plainly.
- **Prefer ESP-IDF facilities over Arduino-only ones**; keep the structure modular - no giant files,
  no stray literals; flag drift when it starts (`structure_check.py`: `GUIManager.cpp` 1,520 lines,
  `Card.cpp` 1,459).
- **Real lights: Office only**, put back as found; ask before any other room.
- **Docs are part of the work**: DECISIONS after a settled discussion, design docs when decided,
  LESSONS as learned, ROADMAP as things are checked off, the issue kept current. **HANDOFF only at
  a handoff**, with the lines recounted. **Recommend a handoff as context nears half.**

**Versioning (W3):** `v0.<phase>.<release>`; the release counts signed-off merges to `main` that
change the firmware (a docs-only merge is not tagged). **The next is `v0.2.21`.**
