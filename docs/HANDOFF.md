# Handoff — 2026-11-04 (2.13 at round 4 on its branch; next: the owner's round-4 test, then round 5)

**Start here.** This file is only: what to read, where we are, what to do next, what will bite you,
and how to work with the owner. The history is in the test sheets, `DECISIONS.md` and
`CHANGELOG.md`; earlier handoffs are in `docs/archive/` and in this file's git history.

## Read in this order - all of it, before starting

1. `CLAUDE.md` - the HAL/BSP, the rules, **and the file-editing rule at the top**.
2. **`docs/PLAN_2.13.md`** - the rest of 2.13 in order: round 5's list, the questions still open
   with the owner, the bigger pieces (the thermostat's controls first), HA's side, the merge.
3. `docs/TEST_2.13.md` - four rounds, newest first; round 4 is what the boards run.
4. **`docs/design/ha-cards.md` §4** - what 2.13 does on the glass now: group and label, the group
   view, pinned members, Edit cards, a group's options, the limits and saved keys.
5. `docs/DECISIONS.md` - **2.13 is K138-K164** (K130-K137 before it; A18 is voice and ESPHome).
6. `docs/RECAP_2.13.md` - the same story told for the owner; a quick way in.
7. `docs/ROADMAP.md`, then `docs/design/ui-architecture.md` and what it points at (`components.md`,
   `tokens.md`, `header-slots.md`, `card-sheet.md` §14-18, `layout-engines.md`, `group-cards.md`
   §6-7). Each now says where 2.13 changed it.
8. **Home Assistant** (sibling folder `../Home Assistant/`, its own git repo): `HOUSE_RULES.md`,
   `FLEET_REFERENCE.md`, `docs/entity_reference.md`, and 2.13's log
   `projects/2026-10_fleet-2.13/README.md`.
9. `docs/LESSONS.md` before debugging anything; `docs/HTTP_REFERENCE.md` before calling a route (its
   2.13 section lists the Edit cards handles); `host-tests/README.md` before touching `src/Layout/`.

**Keep it short.** A finished investigation goes to `LESSONS.md`; a test run to its test sheet; a
decision to `DECISIONS.md`; a number to the doc that owns it; the order of work to `PLAN_2.13.md`.
This file is rewritten at a handoff.

---

## Where the project is

**2.13 (cards from Home Assistant) is being built** on `feat/2.13-add-from-ha`, off `main` at
`fd3d59e` (`v0.2.20`). Four rounds, 2026-11-02 to 11-04, the last at `5fbc9ee` (docs `22a17af`,
then this handoff's docs commit). **Nothing is pushed.** Round 4 was checked by Claude over `/popup`
and screenshots; **the owner has not tried it on the glass yet**. His reaction to what he has seen:
"I cannot believe how much functionality there is and how slick everything looks."

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
| `v0.2.20` | 2026-11-01 | 2.11 follow-ups (2 rounds) | 64,629 | 260 |
| *branch* | 2026-11-04 | **2.13, rounds 1-4** (`5fbc9ee`, not released) | **69,478** | **277** |

Counted with `git grep -c -e '^' <rev> -- src include components ':!components/lvgl' ...` (the
exclusions above, and `':!*font*' ':!*Font*'`), summing `.c .cpp .h .hpp`. **In PowerShell use
`-e '^'`, not `''`**: Windows PowerShell 5.1 drops an empty argument to a native program.

**What a board does now:** two pages (House from HA over the websocket, Fleet from MQTT and the
system), clusters and solo cards placed by the arranger; **cards added from HA on the glass**, kept
in the settings file; a tap on a cluster opens its **group view** (the cluster itself, tiles capped
in size, Tiles or Cards), a long press its Members; **Edit cards** adds, pins, hides, removes and
restores; a group keeps its GROUP panel however it is shown; notices queue; the thermostat is a
read-only card. The UI in one paragraph and where everything lives: `ui-architecture.md`.

## What is next

1. **The owner tests round 4** (`TEST_2.13.md`, T1-T13). He also has "a couple of minor bugs" he
   has not listed yet - ask for them. Record his results under each test.
2. **Round 5** - `PLAN_2.13.md` §1: his visual list (the pin's color, Corner cut on Linen in the
   group view, the drop-down's animation, the long-press animation, Edit cards' area rows, no panels
   under a group's Edit cards), round 4's two gaps, the structure check's drift. **Settle §2's
   questions with him first** - corner icons, Show as and Rename group moving, a GROUP panel on
   History and Scenes, Garage - Doors, how many made cards, the unit choice.
3. **The thermostat's controls** - `PLAN_2.13.md` §3.1, **with the owner present** (it commands the
   real thermostat; ask before any command). Then units and precision from HA, and the rest of §3.
4. **HA's side** - §4; nothing built there yet beyond wording.
5. **Sign-off and merge** - §5. The next version is `v0.2.21`.

**Issues:** #53 and #54 are closed (the arranger answered them). **#52** (auto-sort) is mostly
answered by groups from HA areas plus the arranger - the owner thinks it is superseded; close it
with a comment if he agrees. **#73 stays open**: chrome text still comes from two fixed sets chosen
by `bspUiScale() >= 1.3` (`UIToolkit.cpp:51`), and value cards still pick their face one by one.
**#116** is 2.13's issue: bring it up to date from `PLAN_2.13.md` when he says.

**After 2.13** (K131): **2.15** the bedroom alarm clock (with OTA 5.1/5.2 pulled forward); **2.14**
alerts (K138, K139, built once the night face exists); **4.3** the navbar; **Phase 3, shrunk**; then
**Phase 7**, voice and the ESPHome API (A18).

**After any merge**: pull `main`, tag the merge commit (annotated, `vX.Y.Z - <milestones>`), push the
tag, flash the boards on the desk. The CHANGELOG entry (from `RECAP_2.13.md`) and the archived test
sheet and plan go on the branch before the owner merges. **The owner merges** (W12); a PR is opened
only after his sign-off.

## Home Assistant - the other half of 2.13

**A session here may change HA directly** (K132) - through the `ha-mcp` tools (**read the server's
best-practices skill with `ha_get_skill_guide()` before any change**) and the HA folder's own
process (`HOUSE_RULES.md` §6: change, check config, reload, `tools/config_snapshot.py "message"`, a
full backup before anything irreversible). **The owner must be able to read and manage everything
built there without AI** - he knows YAML, not Python (K136). Nothing from HA is imposed on the
boards (K133). **Nothing is built on either side until it is agreed.**

**Where it stands:** only K133's wording changed (2026-11-02, HA commit `d8293c5`); the log's status
line still says nothing is built. Open: the YAML conversion (K136), whether per-room status entities
are needed at all, duplicate door entities, `delay_on` timers that reset on an HA restart, what HA
2026.11 and Spook now offer. The alert anatomy is decided (K138) and is 2.14's. The milestone is
bounded by the tags `fleet-2.13-start` and `fleet-2.13-done` (at sign-off).

**Where the HA repo is:** on the desktop it is the original; the laptop has a clone at
`C:\Users\eric\Documents\Projects\Home Assistant`. Push from the laptop to a `laptop` branch.
`private/` and `secrets.yaml` never leave the desktop; the ha-mcp URL is a secret. The owner signs
in to HA's web pages himself (the browser pane); never enter a password.

## Boards on the desk

| Board | Port | Runs |
|---|---|---|
| `WS_P4_5` | COM15 desktop / COM6 laptop (CH343 `5B90154141`) | round 4 `5fbc9ee`; the dev board; `DEBUG_POPUP`, `DEBUG_HANG` |
| `WS_P4_4B` | COM7 (CH343 `5AAF289419`) | round 4 `5fbc9ee`; `DEBUG_POPUP`. Flash with `--after watchdog-reset`; restore settings with `-NoReboot` (a software restart can boot-loop it). **Once stopped answering HTTP after a restore's reboot** (11-04) until its serial port was opened |
| `CYD_P4_1060` | COM9 (native USB, `30:ED:A0:E3:2C:7B`) | round 4 `5fbc9ee`; `DEBUG_POPUP`. Flash at **460800 with `--after watchdog-reset`** (from its bootloader also `--before no-reset`); two cables; `/screenshot` needs `curl -m 90` |
| `CYD_P4_4880` | COM19 (native USB) | round 2 `d8d3d26`; host `fleet-cyd-p4-4880`; flashed by hand (`flash_release.ps1` does not cover it) |
| `WS_P4_7B` | COM6 desktop (native USB) | `v0.2.19` - not plugged in since; needs two USB cables or it browns out |
| `WS_S3_4B` | COM8 | **locked up** (#69); compile it as the S3 check, never flash it until the owner says |
| `CYD_S3_3248` | - | USB broken (2026-09-28) |
| `CYD_S3_8048`, `WS_S3_5B` | - | not plugged in |

**Each board's settings were saved before round 4** (`bench/settings/<board>/20261104-01*.json`)
and checked unchanged after (`setdiff.py`: 0 differences). Hosts: `fleet-ws-p4-5`,
`fleet-ws-p4-4b`, `fleet-cyd-p4-1060`. Ports move when boards are re-plugged: `pio device list` and
match the serial. **Is a board alive?** Ask HA (`sensor.<device>_uptime`) or `GET
/settings?stats=1`. While developing, build and flash the P4_5, the 4B and the 1060 (and compile
`WS_S3_4B`); all nine compile once, right before a merge.

**Flashing, app only:** `esptool --chip esp32p4 -p COMn -b 921600 write-flash 0x10000
.pio/build/<env>/firmware.bin` (the 4B and the 1060 as above). **Tell the owner before each
flash**, and save the board's `/settings` first: `scripts/release/save_settings.ps1 <host>
<Board>`; put a copy back with `restore_settings.ps1`; compare with `setdiff.py`.

## Environment state git cannot see

**The ESP32-P4 framework libraries are REBUILT, not stock** (`esp32p4_es` in
`~/.platformio/packages/framework-arduinoespressif32-libs/`): #49's PSRAM and cache-line changes,
`LCD_DSI_ISR_CACHE_SAFE` (A14), IDF's PPA rotation patch (A15). **A `pio pkg update` or platform
reinstall silently puts the bugs back.** How: `docs/REBUILD_P4_LIBS.md`; check a build with `grep
-a -o "v5.5.5-832-g2553c5ad432[-a-z]*" .pio/build/<env>/firmware.bin` (it must end `-dirty`).

**The owner's local debug lines** in `platformio.ini` (`DEBUG_POPUP` on the P4_5, 4B and 1060;
`DEBUG_HANG` on the P4_5) keep the file modified - **never commit them**; the pre-commit hook
refuses them, and `gate.ps1` / `flash_release.ps1` stash them.

**`docs/HANDOFF_LAPTOP_2026-10-31.md` and the next-session prompt are untracked on purpose** -
never commit them.

**The laptop is different** (`C:\Users\eric\Documents\Projects\ESP32-Voice-Assistant-Fleet`): its
`platformio.ini` has its own `symlink://` prefix and is marked **skip-worktree**, so `gate.ps1` there
is a compile check only, a branch whose `platformio.ini` differs cannot be checked out without
un-skipping it first, and `core.hooksPath` is unset there (the owner sets it himself).

**WSL's Ubuntu has g++ 13.3** (host tests: `wsl bash host-tests/run_in_wsl.sh`, 78 pass); **node**
is on the desktop's PATH (the arranger's test generator, the icon font's `npx lv_font_conv`). **Do
not use NINA's C6 updater** - it hung `WS_P4_5`. `reference/` is gitignored.

## Things that will bite you

- **`PYTHONIOENCODING=utf-8` before any `pio` or `esptool` whose output is piped**; give an
  unattended upload a `timeout`. pyserial and esptool are in PlatformIO's own Python
  (`%USERPROFILE%\.platformio\penv\Scripts\python.exe`).
- **The repo's PowerShell scripts run from the PowerShell tool**; `powershell -File` from Git Bash is
  refused by the execution policy.
- **`pio run` with no `-e` builds the 7B only.** The all-nine gate: `powershell -ExecutionPolicy
  Bypass -File scripts/release/gate.ps1` (~15 min, in the background); it refuses a dirty tree.
  **Do not edit source while a build runs.**
- **PlatformIO keeps one environment's build at a time** - copy an image you want to keep. Clear
  `.pio/build_cache` after a BSP, struct-layout or `lv_conf.h` edit.
- **DECISIONS numbers are claimed by branch** - check the open branches before numbering a row. The
  next is **K165**.
- **A kept arranger answer outlives a search change** unless its version moves (`v4`).
- **A task that never blocks runs at `tskIDLE_PRIORITY`** or the task watchdog reboots the chip.
- **LVGL 9.5 clips `DRAW_POST` to the object** - draw outside it in `DRAW_MAIN_END`. **A child's
  shadow escapes an overflow-visible parent only as far as the parent's ext draw size** (LESSONS,
  2.13). Never add children to a kit component.
- **A screenshot is not the glass**; `/screenshot` returns a PNG Read cannot always show - re-encode
  it as plain RGB first. **Never read from a DSI panel that is showing a picture.**
- **A serial monitor resets the board when it opens** - attach with `--rts 0 --dtr 0`.
- **HTTP handlers and the HA websocket task never touch LVGL**; anything that writes flash runs on a
  task with an internal-RAM stack. The P4_5 serves 7 of `HttpServer::MAX_ROUTES` (8).
- **`Edit` on a CRLF file**: deleting a line by matching a leading newline joins two lines.
- **`git commit -m` and `gh ... --body` in PowerShell 5.1 drop double quotes** - write to a file and
  use `git commit -F` / `--body-file`. Commit and push in separate calls; with `main` checked out,
  push another branch as `git push origin b:b`.
- **Verify from outside the device** - the router, the broker, HA, `/bench?what=verify`, the harness.

## How to work with this owner

A hobbyist and ESP32 enthusiast, not a professional developer, and explicit about that - but reads
code, spots real bugs, and his instincts have shaped the best parts of the UI (the rail, the slots,
the arranger's rules, the move design, 2.13's pinning and Edit cards' tap cycle). **Treat them as
data.** A little colour-blind: never distinguish two states by colour alone.

- **Show, don't spec.** Mock artifacts first; he tests thoroughly against a sheet
  (`docs/TEST_<topic>.md`, numbered tests with PASS criteria, his results under them). Rounds go on
  one branch; the gate and the PR come once, at sign-off.
- **No on-screen words unless agreed with him first.** US English on screen ("Color"). When a case
  needs words he has not given, build the nearest and list them under "to confirm". **Label** and
  **group** are different words (K146, K158) - use them exactly.
- **Options never move** between panels; one that does not apply is greyed (K155).
- **Discuss a structural choice a turn or two before building it.** Give the top 2-3 options with a
  recommendation. Never assume. **Call out a past decision a new request conflicts with** - he
  welcomes it ("I can't keep up with all of the decisions I've made").
- **Plain language.** State what things are; skip "X, not Y" framings and metaphor.
- **Snappy first, eye candy second**; motion that jerks is a bug to him.
- **One report when he says the testing is done**; say what to expect before anything changes the
  screen; say what was not tested on the glass. Own mistakes plainly.
- **Unattended work** is fine when he grants it (overnight rounds were): say exactly what may be
  flashed and commanded, and resume by yourself after a usage limit (a watchdog job; remove it
  after).
- **Prefer ESP-IDF facilities over Arduino-only ones**; keep the structure modular - no giant files,
  no stray literals; run `structure_check.py` and flag drift when it starts.
- **Real lights: Office only**, put back as found; ask before any other room. **The thermostat
  commands the house** - only with him there.
- **Docs are part of the work**: DECISIONS after a settled discussion, design docs when decided,
  LESSONS as learned, ROADMAP as things are checked off, `PLAN_2.13.md` as items land. **HANDOFF
  only at a handoff**, with the lines recounted. **Recommend a handoff as context nears half.**

**Versioning (W3):** `v0.<phase>.<release>`; the release counts signed-off merges to `main` that
change the firmware. **The next is `v0.2.21`.**
