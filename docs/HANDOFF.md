# Handoff

**Start here.** Where the project stands - its goals, the decisions and findings that shape the next
work, the state of the boards and the environment - and how to work with the owner. It is an
overview, not a log of the last session: the next session may pick up any milestone. History is in
git, `CHANGELOG.md` and `docs/archive/`.

## Read in this order

1. `CLAUDE.md` - the HAL/BSP, the rules, **and the file-editing rule at the top**.
2. `docs/ROADMAP.md` - the plan, how a milestone runs, and the Phase 2 summary of open work.
3. The milestone in progress, if there is one: `docs/PLAN_<milestone>.md`, then `docs/TEST_<milestone>.md`.
4. `docs/DECISIONS.md` - settled decisions, one line each.
5. `docs/design/ui-architecture.md` and what it points at (`components.md`, `tokens.md`,
   `header-slots.md`, `card-sheet.md`, `layout-engines.md`, `group-cards.md`, `ha-cards.md`).
6. **Home Assistant** (sibling folder `../Home Assistant/`, its own git repo): `HOUSE_RULES.md`,
   `FLEET_REFERENCE.md`, `docs/entity_reference.md`.
7. `docs/LESSONS.md` before debugging anything; `docs/HTTP_REFERENCE.md` before calling a route;
   `host-tests/README.md` before touching `src/Layout/`.

---

## Where the project stands

**Phase 2 (the UI foundation) is nearly done.** The latest release is `v0.2.21`, milestone 2.13:
cards come from Home Assistant on the glass, and the compiled dashboard is retired - **a board's
settings file is its dashboard** (K171). The Phase 2 summary in ROADMAP lists what its milestones
left open.

**What a board does:** two pages - House, from HA over the websocket, and Fleet with the built-in
Panel group (and, on the dev boards, an MQTT group). The arranger places groups and solo cards. Edit
cards adds from HA (entities labelled `fleet`), pins, hides, removes and restores; New group is Edit
cards' flow. A tap on a group opens its group view, a long press its Members. A group shows on the
page as a group or as one solo card (Show as...). A group made on the board is deleted when no card
is in it (K200). Every membership change asks first. The thermostat is a read-only card. The UI in
one paragraph and where everything lives: `ui-architecture.md`.

**Lines of code** - the project's own C/C++ (`src/`, `include/`, its own `components/`; generated
fonts, `lvgl`, `GFX_Library_for_Arduino`, `SensorLib`, `XPowersLib`, `bb_captouch_fork`,
`esp_websocket_client` and `include/third_party` left out; every line counted, comments too):

| Release | Milestone | Lines | Files |
|---|---|---|---|
| `v0.1.0` | Phase 1 | 9,039 | 44 |
| `v0.2.4` | 2.4 | 18,745 | 98 |
| `v0.2.7` | 2.7 | 28,207 | 117 |
| `v0.2.8`-`v0.2.11` | 2.10, the card popup | 50,115 | 183 |
| `v0.2.12` | K17 | 50,872 | 185 |
| `v0.2.13` | 2.12, the UI foundation | 52,957 | 222 |
| `v0.2.16` | #99, satin and glass | 53,449 | 222 |
| `v0.2.18` | 2.8, the header bar v2 | 57,224 | 244 |
| `v0.2.19` | 2.11, group cards | 63,694 | 259 |
| `v0.2.20` | 2.11 follow-ups | 64,629 | 260 |
| `v0.2.21` | 2.13, cards from HA | 70,945 | 279 |

Counted with `git grep -c -e '^' <rev> -- src include components ':!components/lvgl' ...` (the
exclusions above, and `':!*font*' ':!*Font*'`), summing `.c .cpp .h .hpp`. **In PowerShell use
`-e '^'`, not `''`**: Windows PowerShell 5.1 drops an empty argument to a native program.

## Goals and what is next

- **A session of its own to break down the remaining roadmap and issues** into milestones small
  enough to keep to, before the next milestone starts (the owner, at 2.13's sign-off). The running
  order after 2.11 (K131) - 2.15, 2.14, 4.3, Phase 3 - is the starting point; 2.14 depends on the HA
  work in the Phase 2 summary; Backup and Restore (2.16) has no place yet.
- **Every milestone now runs by ROADMAP's "How a milestone runs"** (W14): a plan first, with a
  parking lot for new ideas; the test sheet only when something is ready on the glass; decisions
  into DECISIONS at sign-off.
- Candidates the owner has named: the thermostat's controls as a one-off build; 2.14 alerts with the
  HA entities it needs; Backup and Restore.

**Issues:** **#116** is 2.13's (closed by its PR). **#118** is a card's place history (K198).
**#52** (auto-sort) - the owner thinks it is superseded; close with a comment if he agrees. **#73
stays open** (chrome text from two fixed sets, `UIToolkit.cpp`).

**After any merge**: pull `main`, tag the merge commit (annotated, `vX.Y.Z - <milestones>`), push the
tag, flash the boards on the desk. **The owner merges** (W12); a PR is opened only after his sign-off.

## Recent findings that shape later work

- **Internal RAM on the S3s is the limit.** 2.13's window buffers pushed three S3s past internal RAM
  at link time; every S3 now takes LVGL's pool in PSRAM, and large fixed buffers use `PsramArray`
  (A11, LESSONS). None of the S3s has run 2.13 on the glass.
- **Scope grew faster than it was tested in 2.13**: decisions were recorded before they were tried
  and then replaced. Hence W14 - decisions at sign-off, a parking lot for new ideas.
- **HA group entities are larger than they look** (`design/carried-from-2.13.md` §1): membership
  with two owners, nesting, the card limit.

## Home Assistant

**A session here may change HA directly** (K132) - through the `ha-mcp` tools (**read the server's
best-practices skill with `ha_get_skill_guide()` before any change**) and the HA folder's own
process (`HOUSE_RULES.md` §6: change, check config, reload, `tools/config_snapshot.py "message"`, a
full backup before anything irreversible). **The owner must be able to read and manage everything
built there without AI** - he knows YAML, not Python (K136). Nothing from HA is imposed on the boards
(K133). **Nothing is built on either side until it is agreed.** A milestone's HA work is logged in
the HA folder's `projects/YYYY-MM_fleet-<milestone>/` and bounded by tags (K135); 2.13's log and its
`fleet-2.13-done` tag are owed (Phase 2 summary).

**Where the HA repo is:** on the desktop it is the original; the laptop has a clone at
`C:\Users\eric\Documents\Projects\Home Assistant`. Push from the laptop to a `laptop` branch.
`private/` and `secrets.yaml` never leave the desktop; the ha-mcp URL is a secret. The owner signs
in to HA's web pages himself; never enter a password.

## Boards on the desk

| Board | Port | Runs |
|---|---|---|
| `WS_P4_5` | COM15 desktop / COM6 laptop (CH343 `5B90154141`) | 2.13; the main dev board; `DEBUG_POPUP`, `DEBUG_HANG` |
| `WS_P4_4B` | COM7 (CH343 `5AAF289419`) | 2.13; `DEBUG_POPUP`. Flash with `--after watchdog-reset`; restore settings with `-NoReboot` |
| `CYD_P4_1060` | COM9 (native USB, `30:ED:A0:E3:2C:7B`) | 2.13; `DEBUG_POPUP`. Flash at **460800 with `--after watchdog-reset`**; two cables; `/screenshot` needs `curl -m 90` |
| `CYD_P4_4880` | COM19 (native USB, `80:F1:B2:D3:CA:EC`) | 2.13, its settings converted with the MQTT group. Flash at 460800 with `--after watchdog-reset`; not covered by `flash_release.ps1` |
| `WS_P4_7B` | COM6 desktop (native USB) | `v0.2.19` - not plugged in since; two USB cables. Takes the 1060's settings file (K171) |
| `WS_S3_4B` | COM8 | **locked up** (#69); compiled as the S3 check, never flashed until the owner says. Takes the P4_4B's file (K171) |
| `CYD_S3_3248` | - | USB broken |
| `CYD_S3_8048`, `WS_S3_5B` | - | not plugged in; would start with Panel and Doors only |

Hosts: `fleet-ws-p4-5`, `fleet-ws-p4-4b`, `fleet-cyd-p4-1060`, `fleet-cyd-p4-4880` (without
`.local` from PowerShell). Ports move when boards are re-plugged: `pio device list` and match the
serial. **Is a board alive?** `GET /settings?stats=1`. Saved settings: `bench/settings/<board>/`.

**Flashing, app only:** `esptool --chip esp32p4 -p COMn -b 921600 write-flash 0x10000
.pio/build/<env>/firmware.bin` from the PowerShell tool (from Git Bash esptool's output fails on
cp1252). **Tell the owner before each flash**, save the board's `/settings` first
(`scripts/release/save_settings.ps1 <host> <Board>`), compare after with `setdiff.py`; put a copy
back with `restore_settings.ps1` (`-NoReboot` to write it before a flash). An app flash never touches
the settings partition.

## Environment state git cannot see

**The ESP32-P4 framework libraries are REBUILT, not stock** (`esp32p4_es` in
`~/.platformio/packages/framework-arduinoespressif32-libs/`): #49's PSRAM and cache-line changes,
`LCD_DSI_ISR_CACHE_SAFE` (A14), IDF's PPA rotation patch (A15). **A `pio pkg update` or platform
reinstall silently puts the bugs back.** How: `docs/REBUILD_P4_LIBS.md`; check a build with `grep
-a -o "v5.5.5-832-g2553c5ad432[-a-z]*" .pio/build/<env>/firmware.bin` (it must end `-dirty`).

**The owner's local debug lines** in `platformio.ini` (`DEBUG_POPUP` on the P4_5, 4B and 1060;
`DEBUG_HANG` on the P4_5) keep the file modified - **never commit them**; the pre-commit hook refuses
them, and `gate.ps1` / `flash_release.ps1` stash them. To commit another change to `platformio.ini`,
stage a copy without them (`git hash-object -w` + `git update-index --cacheinfo`).

**`docs/HANDOFF_LAPTOP_*.md` and the next-session prompts are untracked on purpose** - never commit
them.

**The laptop is different** (`C:\Users\eric\Documents\Projects\ESP32-Voice-Assistant-Fleet`): its
`platformio.ini` has its own `symlink://` prefix and is marked **skip-worktree**, so `gate.ps1` there
is a compile check only, a branch whose `platformio.ini` differs cannot be checked out without
un-skipping it first, and `core.hooksPath` is unset there (the owner sets it himself).

**WSL's Ubuntu has g++ 13.3** (host tests: `wsl bash host-tests/run_in_wsl.sh`); **node** is on the
desktop's PATH. **PIL is not installed**: `/screenshot`'s PNG reads fine with Read as it is. **Do not
use NINA's C6 updater** - it hung `WS_P4_5`. `reference/` is gitignored.

## Things that will bite you

- **A board's settings file is its dashboard (K171).** A wiped partition leaves Panel and Garage -
  Doors. Save before anything that could touch it.
- **`PYTHONIOENCODING=utf-8` before any `pio` or `esptool` whose output is piped**; give an
  unattended upload a `timeout`. pyserial and esptool are in PlatformIO's own Python
  (`%USERPROFILE%\.platformio\penv\Scripts\python.exe`).
- **The repo's PowerShell scripts run from the PowerShell tool**; `powershell -File` from Git Bash is
  refused by the execution policy.
- **`pio run` with no `-e` builds the 7B only.** The all-nine gate: `powershell -ExecutionPolicy
  Bypass -File scripts/release/gate.ps1` (~20 min, in the background); it refuses a dirty tree.
  **Do not edit source while a build runs.**
- **Clear `.pio/build_cache`** after a BSP, struct-layout or `lv_conf.h` edit.
- **Internal RAM on the S3s**: a large fixed array goes in `PsramArray`, never a static array; the
  gate is where an S3 overflow shows.
- **The arranger's kept answer records the height it was made for** (K167). Changing the fingerprint
  makes every page arrange once (a few seconds, in the background).
- **A task that never blocks runs at `tskIDLE_PRIORITY`** or the task watchdog reboots the chip.
- **LVGL 9.5 clips `DRAW_POST` to the object** - draw outside it in `DRAW_MAIN_END`. **A child's
  shadow escapes an overflow-visible parent only as far as the parent's ext draw size**. Never add
  children to a kit component.
- **A screenshot is not the glass.** **Never read from a DSI panel that is showing a picture.**
  `/popup` cannot change page.
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
the arranger's rules, the move design, pinning, Edit cards' tap cycle, New group's flow). **Treat
them as data.** A little colour-blind: never distinguish two states by colour alone.

- **Never assume** (W7): where you would normally assume, say what the assumption would have been and
  ask at least two questions that fill the gap.
- **Keep to the plan** (W14): a new idea goes to the plan's parking lot unless he moves it into scope;
  say "that's parking lot unless you want it in".
- **Show, don't spec.** Mock artifacts first; he tests thoroughly against a sheet
  (`docs/TEST_<milestone>.md`, numbered tests with PASS criteria, his results under them). Rounds go on
  one branch; the gate and the PR come once, at sign-off.
- **No on-screen words unless agreed with him first.** US English on screen ("Color"). When a case
  needs words he has not given, build the nearest and list them under "to confirm". **Label** and
  **group** are different words (K146, K158). Names in dialogs are in the accent.
- **Where an option lives is a design choice; it never moves with a card's state** (K155). One that
  does not apply is greyed.
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
  no stray literals; run `structure_check.py` and flag drift when it starts.
- **Real lights: Office only**, put back as found; ask before any other room. **The thermostat
  commands the house** - only with him there.
- **Docs are sources of truth, not timelines**: design docs and code comments say how things work
  now, with no dates or retellings; decisions go to DECISIONS at sign-off; LESSONS as learned.
  **HANDOFF is rewritten at a handoff**, with the lines recounted. **Recommend a handoff as context
  nears half.**

**Versioning (W3):** `v0.<phase>.<release>`; the release counts signed-off merges to `main` that
change the firmware.
