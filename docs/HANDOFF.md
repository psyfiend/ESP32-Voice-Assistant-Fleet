# Handoff — 2026-11-05 (2.13 at round 5 on its branch; next: the owner's round-5 test, then one large batch to finish 2.13)

**Start here.** This file is only: what to read, where we are, what to do next, what will bite you,
and how to work with the owner. The history is in the test sheets, `DECISIONS.md` and
`CHANGELOG.md`; earlier handoffs are in `docs/archive/` and in this file's git history.

## Read in this order - all of it, before starting

1. `CLAUDE.md` - the HAL/BSP, the rules, **and the file-editing rule at the top**.
2. **`docs/PLAN_2.13.md`** - the rest of 2.13: **§7 is what is not built and what was discussed but
   not explored enough**; §2's settled questions; §3 the bigger pieces (the thermostat's controls);
   §4 HA's side; §5 the merge.
3. `docs/TEST_2.13.md` - five rounds, newest first; round 5 (T1-T27) is what the boards run.
4. **`docs/design/ha-cards.md` §4** - what 2.13 does on the glass (its round-5 note says what moved).
5. `docs/DECISIONS.md` - **2.13 is K138-K178** (W7 restates "never assume"); A18 is voice and ESPHome.
6. `docs/RECAP_2.13.md` - the same story told for the owner, five rounds.
7. `docs/ROADMAP.md`, then `docs/design/ui-architecture.md` and what it points at (`components.md`,
   `tokens.md`, `header-slots.md`, `card-sheet.md` §14-18, `layout-engines.md`, `group-cards.md`
   §6-7). Each says where 2.13 changed it.
8. **Home Assistant** (sibling folder `../Home Assistant/`, its own git repo): `HOUSE_RULES.md`,
   `FLEET_REFERENCE.md`, `docs/entity_reference.md`, and 2.13's log
   `projects/2026-10_fleet-2.13/README.md`.
9. `docs/LESSONS.md` before debugging anything; `docs/HTTP_REFERENCE.md` before calling a route;
   `host-tests/README.md` before touching `src/Layout/`.

**Keep it short.** A finished investigation goes to `LESSONS.md`; a test run to its test sheet; a
decision to `DECISIONS.md`; a number to the doc that owns it; the order of work to `PLAN_2.13.md`.
This file is rewritten at a handoff.

---

## Where the project is

**2.13 (cards from Home Assistant) is being built** on `feat/2.13-add-from-ha`, off `main` at
`fd3d59e` (`v0.2.20`). Five rounds, 2026-11-02 to 11-05; 39 commits; **pushed** (`fcc0c28`, the
docs after round 5). Round 5's firmware is `835b870` (`b588cab` after it turned four numbers into
tokens - nothing on the glass changes). **The owner has not tried round 5 on the glass**; he will
bring its results (`TEST_2.13.md` round 5, T1-T27).

**The compiled dashboard is retired (K171).** The House page compiles only Garage - Doors; every
other card is a card made on the board, in each board's settings file, with its old id. The Fleet
page compiles only the built-in **Panel** group. **A board's settings file now IS its dashboard**:
losing it loses the House page. `scripts/release/retire_compiled.py` converts a pre-round-5 settings
file; `scripts/release/test_cards.json` holds the retired test cards.

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
| *branch* | 2026-11-04 | 2.13, rounds 1-4 (`5fbc9ee`) | 69,478 | 277 |
| *branch* | 2026-11-05 | **2.13, round 5** (`fcc0c28`, not released) | **70,266** | **277** |

Counted with `git grep -c -e '^' <rev> -- src include components ':!components/lvgl' ...` (the
exclusions above, and `':!*font*' ':!*Font*'`), summing `.c .cpp .h .hpp`. **In PowerShell use
`-e '^'`, not `''`**: Windows PowerShell 5.1 drops an empty argument to a native program. The
retired specs are kept under `#if 0` and still counted.

**What a board does now:** two pages (House from HA over the websocket, Fleet with Panel and, on the
dev boards, MQTT); groups placed by the arranger; cards added from HA on the glass; a tap on a group
opens its group view, a long press its Members; Edit cards adds, pins, hides, removes and restores,
every membership change asked first; New group is Edit cards' flow; a group's options are GROUP's
(its members) and Visibility's (how it stands on the page); notices queue; the thermostat is a
read-only card. The UI in one paragraph and where everything lives: `ui-architecture.md`.

## What is next

1. **The owner tests round 5** and brings the results; record them under round 5 in
   `TEST_2.13.md`.
2. **Discuss everything left in `PLAN_2.13.md` §7 and §3 with him**, as much as possible up front:
   he wants to **finish 2.13 in one large unattended batch**. The next-session prompt
   (`docs/HANDOFF_PROMPT_2026-11-05.md`, untracked) has a plan per item, what each needs, the holes
   and the interactions to raise. **K170** (HA group entities as groups on the board) is the biggest.
3. **The thermostat's controls** (§3.1) still only **with the owner present** - it commands the
   house. If the batch is unattended, it waits.
4. **Sign-off and merge** - §5. The next version is `v0.2.21`. Before the release flash, the boards
   not yet converted (CYD_P4_4880, WS_P4_7B, the S3s) need their settings files converted (K171).

**Issues:** **#52** (auto-sort) - the owner thinks it is superseded; close with a comment if he
agrees. **#73 stays open** (chrome text from two fixed sets, `UIToolkit.cpp:51`). **#116** is 2.13's
issue: bring it up to date from `PLAN_2.13.md` when he says.

**After 2.13** (K131): **2.15** the bedroom alarm clock (with OTA 5.1/5.2 pulled forward); **2.14**
alerts (K138, K139; Garage - Doors and roll-up entities are discussed there); **4.3** the navbar;
**Phase 3, shrunk**; then **Phase 7**, voice and the ESPHome API (A18).

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

**Where it stands:** only K133's wording changed (2026-11-02, HA commit `d8293c5`); nothing built.
Open: the YAML conversion (K136), per-room status entities (probably unneeded), duplicate door
entities, `delay_on` timers that reset on an HA restart, what HA 2026.11 and Spook now offer. On
11-05 three lights read **unavailable** in HA (`light.kitchen_light_sink`, `light.kitchen_light_table`,
`light.porch_light`) - the boards' N/A is right. The milestone is bounded by the tags
`fleet-2.13-start` and `fleet-2.13-done` (at sign-off).

**Where the HA repo is:** on the desktop it is the original; the laptop has a clone at
`C:\Users\eric\Documents\Projects\Home Assistant`. Push from the laptop to a `laptop` branch.
`private/` and `secrets.yaml` never leave the desktop; the ha-mcp URL is a secret. The owner signs
in to HA's web pages himself (the browser pane); never enter a password.

## Boards on the desk

| Board | Port | Runs |
|---|---|---|
| `WS_P4_5` | COM15 desktop / COM6 laptop (CH343 `5B90154141`) | round 5 `835b870`, settings converted (K171); the dev board; `DEBUG_POPUP`, `DEBUG_HANG` |
| `WS_P4_4B` | COM7 (CH343 `5AAF289419`) | round 5, converted; `DEBUG_POPUP`. Flash with `--after watchdog-reset`; restore settings with `-NoReboot` |
| `CYD_P4_1060` | COM9 (native USB, `30:ED:A0:E3:2C:7B`) | round 5, converted; `DEBUG_POPUP`. Flash at **460800 with `--after watchdog-reset`**; two cables; `/screenshot` needs `curl -m 90` |
| `CYD_P4_4880` | COM19 (native USB) | **round 2 `d8d3d26` with its compiled cards** - convert its settings (`retire_compiled.py`) before it takes round 5; flashed by hand (`flash_release.ps1` does not cover it) |
| `WS_P4_7B` | COM6 desktop (native USB) | `v0.2.19` - not plugged in since; two USB cables. Takes the 1060's settings file at the release (K171) |
| `WS_S3_4B` | COM8 | **locked up** (#69); compile it as the S3 check; never flash it until the owner says. Takes the P4_4B's file (K171) |
| `CYD_S3_3248` | - | USB broken (2026-09-28) |
| `CYD_S3_8048`, `WS_S3_5B` | - | not plugged in; need a settings file at the release (K171: Panel alone otherwise) |

**Each board's settings were saved before round 5** (`bench/settings/<board>/20261105-0310.json`,
and the pre-conversion `20261105-1557` / `-1604` / `-1605`). Hosts: `fleet-ws-p4-5`,
`fleet-ws-p4-4b`, `fleet-cyd-p4-1060`. Ports move when boards are re-plugged: `pio device list` and
match the serial. **Is a board alive?** `GET /settings?stats=1`.

**Flashing, app only:** `esptool --chip esp32p4 -p COMn -b 921600 write-flash 0x10000
.pio/build/<env>/firmware.bin` (the 4B and the 1060 as above). **Tell the owner before each flash**,
save the board's `/settings` first (`scripts/release/save_settings.ps1 <host> <Board>`), compare
after with `setdiff.py`; put a copy back with `restore_settings.ps1` (`-NoReboot` to write it before
a flash). An app flash never touches the settings partition.

## Environment state git cannot see

**The ESP32-P4 framework libraries are REBUILT, not stock** (`esp32p4_es` in
`~/.platformio/packages/framework-arduinoespressif32-libs/`): #49's PSRAM and cache-line changes,
`LCD_DSI_ISR_CACHE_SAFE` (A14), IDF's PPA rotation patch (A15). **A `pio pkg update` or platform
reinstall silently puts the bugs back.** How: `docs/REBUILD_P4_LIBS.md`; check a build with `grep
-a -o "v5.5.5-832-g2553c5ad432[-a-z]*" .pio/build/<env>/firmware.bin` (it must end `-dirty`).

**The owner's local debug lines** in `platformio.ini` (`DEBUG_POPUP` on the P4_5, 4B and 1060;
`DEBUG_HANG` on the P4_5) keep the file modified - **never commit them**; the pre-commit hook
refuses them, and `gate.ps1` / `flash_release.ps1` stash them.

**`docs/HANDOFF_LAPTOP_2026-10-31.md` and the next-session prompts are untracked on purpose** -
never commit them.

**The laptop is different** (`C:\Users\eric\Documents\Projects\ESP32-Voice-Assistant-Fleet`): its
`platformio.ini` has its own `symlink://` prefix and is marked **skip-worktree**, so `gate.ps1` there
is a compile check only, a branch whose `platformio.ini` differs cannot be checked out without
un-skipping it first, and `core.hooksPath` is unset there (the owner sets it himself).

**WSL's Ubuntu has g++ 13.3** (host tests: `wsl bash host-tests/run_in_wsl.sh`, 78 pass); **node**
is on the desktop's PATH. **PIL is not installed**: `/screenshot`'s PNG reads fine with Read as it
is. **Do not use NINA's C6 updater** - it hung `WS_P4_5`. `reference/` is gitignored.

## Things that will bite you

- **A board's settings file is its dashboard now (K171).** A wiped or formatted partition leaves
  Panel and Garage - Doors. Save before anything that could touch it; `bench/settings/` has copies.
- **`PYTHONIOENCODING=utf-8` before any `pio` or `esptool` whose output is piped**; give an
  unattended upload a `timeout`. pyserial and esptool are in PlatformIO's own Python
  (`%USERPROFILE%\.platformio\penv\Scripts\python.exe`).
- **The repo's PowerShell scripts run from the PowerShell tool**; `powershell -File` from Git Bash is
  refused by the execution policy.
- **`pio run` with no `-e` builds the 7B only.** The all-nine gate: `powershell -ExecutionPolicy
  Bypass -File scripts/release/gate.ps1` (~15 min, in the background); it refuses a dirty tree.
  **Do not edit source while a build runs.**
- **Clear `.pio/build_cache`** after a BSP, struct-layout or `lv_conf.h` edit.
- **DECISIONS numbers are claimed by branch** - check the open branches before numbering a row. The
  next is **K179**.
- **The arranger's kept answer is v5** (K167: it records the height it was made for). A page arranged
  with the header hidden and shown again fits its cards to the shorter room. Changing the
  fingerprint makes every page arrange once (a few seconds, in the background).
- **A task that never blocks runs at `tskIDLE_PRIORITY`** or the task watchdog reboots the chip.
- **LVGL 9.5 clips `DRAW_POST` to the object** - draw outside it in `DRAW_MAIN_END`. **A child's
  shadow escapes an overflow-visible parent only as far as the parent's ext draw size**. Never add
  children to a kit component.
- **A screenshot is not the glass.** **Never read from a DSI panel that is showing a picture.**
  `/popup` cannot change page: the Fleet page has not been seen since K171.
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
the arranger's rules, the move design, 2.13's pinning, Edit cards' tap cycle, New group's flow).
**Treat them as data.** A little colour-blind: never distinguish two states by colour alone.

- **Never assume** (W7, restated 11-05): where you would normally assume, say what the assumption
  would have been and ask at least two questions that fill the gap.
- **Show, don't spec.** Mock artifacts first; he tests thoroughly against a sheet
  (`docs/TEST_<topic>.md`, numbered tests with PASS criteria, his results under them). Rounds go on
  one branch; the gate and the PR come once, at sign-off.
- **No on-screen words unless agreed with him first.** US English on screen ("Color"). When a case
  needs words he has not given, build the nearest and list them under "to confirm". **Label** and
  **group** are different words (K146, K158). Names in dialogs are in the accent (11-05).
- **Where an option lives is a design choice; it never moves with a card's state** (K155 as restated
  in K174). One that does not apply is greyed.
- **Discuss a structural choice a turn or two before building it.** Give the top 2-3 options with a
  recommendation. **Call out a past decision a new request conflicts with** - he welcomes it.
- **Plain language.** State what things are; skip "X, not Y" framings and metaphor.
- **Snappy first, eye candy second**; motion that jerks is a bug to him; waiting on the arranger for
  nothing annoys him (K167).
- **One report when he says the testing is done**; say what to expect before anything changes the
  screen; say what was not tested on the glass. Own mistakes plainly.
- **Unattended work** is fine when he grants it: say exactly what may be flashed and commanded, and
  resume by yourself after a usage limit (a CronCreate watchdog; remove it after).
- **Prefer ESP-IDF facilities over Arduino-only ones**; keep the structure modular - no giant files,
  no stray literals; run `structure_check.py` and flag drift when it starts (`PopupCluster.cpp` is
  at 1,005 lines).
- **Real lights: Office only**, put back as found; ask before any other room. **The thermostat
  commands the house** - only with him there.
- **Docs are part of the work**: DECISIONS after a settled discussion, design docs when decided,
  LESSONS as learned, ROADMAP as things are checked off, `PLAN_2.13.md` as items land. **HANDOFF
  only at a handoff**, with the lines recounted. **Recommend a handoff as context nears half.**

**Versioning (W3):** `v0.<phase>.<release>`; the release counts signed-off merges to `main` that
change the firmware. **The next is `v0.2.21`.**
