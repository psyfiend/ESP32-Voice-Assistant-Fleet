# Handoff — 2026-10-06

**Start here.** `CLAUDE.md` is the stable how-it-works. This file is only: where we are, what to do
next, what will bite you, and how to work with the owner. It was rewritten from scratch on
2026-09-29; the previous one, with every session log since 2026-09-24, is
`docs/archive/HANDOFF_to_2026-09-29.md` - read it only when chasing the history of something.

**Keep it short.** A finished investigation goes to `docs/LESSONS.md` (general) or
`docs/display/history.md` (display) as a few lines; a test run goes to its test log; a number goes
to the doc that owns it. This file says where things are, not how they got there.

---

## Where the project is

**`v0.2.9` tagged 2026-10-06** at the 2.10b merge (`v0.2.8` was 2.10a); `CHANGELOG.md` matches
versions to milestones.

What a board does: boots into two pages (House = the owner's 18 HA entities over the websocket,
Fleet = MQTT/system/virtual cards), swiped with wrap-around; three colour schemes (Midnight
default, Fleet, Linen with real shadows); card types with state icons; an FPS/CPU overlay;
`/screenshot` and `/bench` over HTTP; **a long press opens a card's popup window**, and a light's
window has its controls (2.10b) - working on the virtual lamps; HA lights get on/off until 2.10c.

## The card popup: 2.10a DONE (`v0.2.8`), 2.10b DONE (`v0.2.9`)

**What the popup is now: `card-sheet.md` §14 (the frame, the deck) and §15 (the light controls,
sizes, groups, pause)** - read both before touching it; where they differ from §9-13, they win.
Every round, result and measurement: `docs/TEST_2.10a.md` (twelve rounds) and `docs/TEST_2.10b.md`
(seven). The rules that cost the most to learn:

- **Usable first** (owner): the window is complete in its first frame and closes in one; no grow,
  no dim (LESSONS, "Effects that cover the screen"). **Nothing moves** within one window (§15).
- **Sizes are shares of the P4_5's window** (`pm()`, "P4_5 millimetres"); touch targets and text
  stay real millimetres. Windows up to 2:1, at most 100 mm, with side gaps.
- **Light state is in the registry**; a light command is confirmed only by a matching report.
  **Groups act as HA's light group** (read from HA's `group/light.py`). A paused member is out of
  its group.
- **`GET /popup`** (DEBUG_POPUP only; parameters in `CardPopup.h`) drives a window from a PC: open,
  switch control, send values, pause, open a member, read LVGL's pool and every lamp's levels.
  Most of 2.10b was checked that way before the owner touched it. Touch itself it cannot test.

**Flashed with v0.2.9, 2026-10-06:** WS_P4_5, WS_P4_4B, CYD_P4_1060. Signed off on WS_P4_5; seen on
the 4B and the 1060. The 7B and the 4880 were not connected and run older builds. **WS_S3_4B is
still locked up**, left as found (#69). All nine compile.

**Memory: DECIDED 2026-10-06 (owner)** - LVGL's pool is in PSRAM at 512 KB on every P4
(`[P4-options]`: `FLEET_LV_MEM_PSRAM`, `FLEET_LV_MEM_KB=512`; WS_S3_4B keeps 128 KB in PSRAM).
Internal heap free at boot 233 -> 361 KB; a popup's biggest free block 13 -> 399 KB; full-screen
frames ~10% slower. LVGL's own allocator kept: NINA's `lv_mem_psram.c` header records why system
malloc for LVGL is a trap on this hardware. LVGL 9.6: after 2.10 (#88).

**Carried forward:**
- **Scaling**: answered at 2.10b (card-sheet §15, proportional to the P4_5). Still open: the
  sensor hero's disc grows with the window while its icon is a fixed face, so on a 7" it is large
  for its glyph; a bigger icon face costs flash.
- **#85**: card borders in colour, a custom scheme page, retiring the system drawer's Cards and Log
  pages (owner, round 6). **#84**: cards never say "no data yet" (filed from this testing).
- Linen may get its own kind of animated chrome some day (owner).
- **The debug flags are local-only:** `-D DEBUG_POPUP` (what building the window costs, printed
  300 ms later so the print is not timed; `dbgMem` for G1; a long press on the window's title cycles
  the sparks: now and then / once / off) and `-D DEBUG_FRAMES` (GUIManager.cpp: every frame of every
  burst of motion). Add them to `WS_P4_TOUCH_LCD_5` in `platformio.ini` when measuring; never commit
  them.

## What is next — the new session's job: 2.10c, HA light attributes and calls (#65)

**Not started; confirm with the owner first.** The build order (card-sheet §9): 2.10c HA, 2.10d
stable ids and saving, 2.10e history. Interview §3 onward (2.8 header slots, then 2.11 groups)
after 2.10. ROADMAP §7 has the order.

**2.10c's scope:** HaProvider reads a light's attributes into `EntityAttrs` (`supported_color_modes`
-> `lightCaps`, `color_mode`, `brightness`, `color_temp_kelvin` and its min/max, `hs_color`), so
HA lights get the slider the virtual lamps have; CommandRouter's light sink sends `light.turn_on`
with data (today it refuses HA light levels, loudly, and the registry reverts them). Scenes
(`hue_scenes` on `light.office`) as a selector target (§11.2) are in reach.

**Read first:** HomeTiles' Bridge and light popup (`REFERENCE_PROJECTS.md`, MIT) - the owner was
struck by its HA-side helpers; `docs/design/ha-websocket.md`; `card-sheet.md` §11-15;
`components/Fleet_Providers/HaProvider.cpp` and `CommandRouter.cpp`.

**Direction (owner, 2026-10-06)**: a companion web app with a true preview that sends the config to
the device (#87) - the build sheet becomes its output, not the user's interface; and presentation
(group/room cards) as how this project stands apart. Also filed: #86 (card control styles, from
HomeTiles), #88 (LVGL 9.6 evaluation, after 2.10). An HA-side companion is a separate, later
choice; HA's `recorder/statistics_during_period` already reduces history for 2.10e.

**How to run it:** one branch off `main` (e.g. `feat/65-ha-lights`). While the owner tests, build and
flash WS_P4_5 only (COM15) unless told otherwise; the owner had the 4B (COM7) and 1060 (COM9, which
is sometimes "busy" - retry after a few seconds) connected for 2.10b. WS_S3_4B is locked up (#69):
leave it until the owner says. Before merge: the all-nine compile gate and a look on glass, then
`--no-ff`, a tag (`v0.2.10`) and a CHANGELOG entry (Versioning, below).

## Where the interview stands — 2026-10-02

- **§1 look and feel: CLOSED**, merged. `look-and-feel.md`.
- **§2 card popups: CLOSED 2026-10-02.** `card-sheet.md` §11-12. Settings model: precedence device
  < page < area/group < card, inheritance per setting with the source named and a Reset, LOCKED
  keeps ROADMAP Q2's meaning, the coloured label type is called **"band"**.
- **Group taxonomy draft** for §4: `group-cards.md` (source x presentation x tap behaviour; Hue
  group members verified from `light.office`'s attributes).
- **Artifacts:** "Card Popup Mock" (the 2.10 design), "Layout Playground" (the arranger, #78),
  "Beyond the Grid", "Linen Fill and Header Glyphs" - links in the design docs.
- **Merged to `main` 2026-10-01 (`0de94d3`), all nine environments compiled (28 min gate):**
  - `feat/74-sntp` (#74): `TimeService`, SNTP + POSIX zone; the boot report waits up to 10 s for the
    first sync, and the Doctor's `Time:` line shows the time and source. Owner confirmed on WS_P4_5.
    Still open on #74: the RTC fallback, zone/servers as settings.
  - `feat/card-tag-float` (#80): floating tag as the default (rises half its height, sticks out a
    quarter to the side, paid for inside the cell); status pills float on the top edge and the area
    name shortens beside them; bar band round-top/straight-bottom. Owner signed off. File folder
    not built (#80).
  - `feat/linen-butter`: Linen ground C+ (`CFC4B2`), card a hair darker (`F3EEE4`), and Linen's own
    "on" colour - **olive** `9AA35A` (butter was rejected on glass). Owner's executive decision,
    to be judged when back; other choices for 4.1 in `UITokens.cpp`.
  - Flashed: WS_P4_5 and WS_P4_4B. WS_S3_4B not flashed (#69's watch). The 4880 runs an old tag build.
- **Merge discipline (owner, 2026-10-01):** nothing reaches `main` without the all-nine compile gate
  and a look on glass; no quick-fix branches during the interview (mocks instead); demos on one
  board.
- **Playground v4** (same link): half-side floating tag, folder tab offset, clusters never tagged
  and their header bar clear unless the page is in Bar mode, status as icons. Notes: `look-and-feel.md`.
- **New issues:** #75 night mode, #76 HA alerts, #77 media player, #78 page arranger (reopens Q3b),
  #79 transitions from pre-rendered pictures, #80 tag styles, #81 system panel should slide.
  Comments on #6, #20, #35, #52-54, #74.
- **Reading a board's boot log without a monitor:** `scratchpad`-style script with pyserial that
  pulses RTS with DTR low (resets into a normal boot) and filters lines - PowerShell 5.1 strips
  quotes from `python -c "..."`, so write the script to a file.

## Fires still burning — each has an owner doc or issue, none needs the next session

| What | State | Where |
|---|---|---|
| **Battery** | **prototype merged** (`a283fd0`, 2026-09-30). Everything current: **`docs/design/power-battery.md`**. Runs only with `-D HAS_BATTERY` (7B, 4880). Voltage good; the inferred power state works for charging / on battery (P1-P3 pass) but cannot see a missing cell while USB is present (P5 fail). **Owner's direction: do not perfect the inference; fuel gauges (MAX17043, ordered) on the units that carry a battery.** Also open: the AXP2101 on the S3_4B, a power-state entity for the header (#19) | #72 |
| **2.9 last boards** | `CYD_S3_8048`, `WS_S3_5B` to move to esp_lcd when on the desk; then #67 closes | #67, `docs/display/README.md` |
| **4B panel timing** | S3_4B clean after `C2 31 02`; P4_4B faint lines at the right edge on the log page. 7-day watch. **S3_4B found locked up 2026-10-05** after days untouched; left as found, to troubleshoot later (owner). What to check first, before resetting it: #69's latest comment | #69 |
| **S3 LVGL speed** | S3_4B feels heavier since LVGL's pool moved to PSRAM. Paused by the owner; resume soon | #70 (full reasoning in its comments) |
| Rotation setting / IMU | recorded | #71 |
| Fonts / type ladder | recorded, feeds the interview | #73 |

**Branch rule (owner, 2026-09-29): branch every piece of work off `main` and merge it when done.**
The display work became a four-deep stack and forced unfinished work into `main` with the finished.
Never start a branch on an unmerged one without asking.

## Boards on the desk

| Board | Port | Notes |
|---|---|---|
| `WS_P4_5` | COM15 (CH343) | dev board; **screwed into its enclosure** - no battery test |
| `WS_P4_4B` | COM7 (CH343) | |
| `WS_P4_7B` | COM6 (native USB) | **needs two USB cables** or it browns out on boot |
| `CYD_P4_1060` | COM9 | also needs two cables |
| `CYD_P4_4880` | COM19 (native USB) | LiPo fitted; browned out on the PC's USB alone |
| `WS_S3_4B` | COM8 (CH343) | the slow dev target; `Serial` is native USB, not the CH343. **Locked up 2026-10-05** - #69 |
| `CYD_S3_3248` | - | **USB broken** (2026-09-28); out of commission |
| `CYD_S3_8048`, `WS_S3_5B` | - | not plugged in |

Ports move when boards are re-plugged; identify a board by its USB serial:
`Get-CimInstance Win32_PnPEntity | ? Name -match 'COM\d+' | select Name, DeviceID`.
**Is a board alive?** Ask Home Assistant (`sensor.<device>_uptime`), not the cable.
Dev boards while developing: **`WS_P4_5` + `WS_S3_4B`**; all nine only before a merge.

**RTC backup cells:** the three Waveshare P4s need **rechargeable** cells (the 7B's holder is ML1220
size - not a CR1220). S3_5B's 927 holder is unconfirmed. Nothing reads an RTC yet. #32.

## Read in this order

1. `CLAUDE.md` — the HAL/BSP, the token and paint rules, **and the file-editing rule at the top**.
2. `docs/LESSONS.md` — before debugging anything.
3. For UI work: `docs/design/interview-phase2-3.md`, `card-sheet.md`, `cards.md` §13, `pages.md`,
   `card-layout.md` (before moving anything inside a card), `tokens.md`, `dashboard.md`,
   `ha-websocket.md`, `startup.md`.
4. For the display: `docs/display/README.md` only - it indexes the rest.
5. `docs/ROADMAP.md` §7.

## Environment state git cannot see

**The ESP32-P4 framework libraries are REBUILT, not stock.** `esp32p4_es` in
`~/.platformio/packages/framework-arduinoespressif32-libs/` carries `MEMPOOL_PREFER_SPIRAM` and a
64-byte L2 line - the fix for esp-hosted-mcu#243 (#49). The stock copy sits beside it as
`esp32p4_es.stock.55.03.311`; rollback is a rename. A `pio pkg update` or platform reinstall
silently puts the bug back. `docs/REBUILD_P4_LIBS.md`. The S3 libraries are stock.

**Do not use NINA's C6 updater** - it hung `WS_P4_5`.

`reference/datasheets/` (ST7701S, ST7703) is gitignored, like the rest of `reference/`.

## Things that will bite you

- **Build from PowerShell, not Git Bash** - pioarduino rejects MSYS shells.
- **`pio run` with no `-e` builds ONE environment** (the default, the 7B). The all-nine gate is
  `pio run -e X -e Y ...` and takes ~30 minutes: run it detached (`Start-Process`) so a tool
  timeout cannot kill it.
- **Clear `.pio/build_cache` after a BSP, struct-layout or `lv_conf.h` edit.** Stale objects
  against a changed struct corrupt memory rather than failing.
- **Do not edit source while a build runs** - it compiles the half-finished edit. Docs are safe.
- **Never set build flags through `PLATFORMIO_BUILD_FLAGS`** - it wiped `.pio/build` mid-build. Put
  them in `platformio.ini`.
- **`PYTHONIOENCODING=utf-8` before any `pio` whose output you pipe**, or it dies silently.
- **Native-USB boards and a PC:** `Serial.setTxTimeoutMs(0)` in `main.cpp` is what stops them
  freezing when the port is held open unread. Do not remove it.
- **A serial monitor resets the board when it opens.** Attach with `--rts 0 --dtr 0`, or read the
  board through HA or `/screenshot`. PlatformIO's own Python has pyserial:
  `~/.platformio/penv/Scripts/python.exe`.
- **Prefer ESP-IDF facilities to Arduino-only ones** (owner). HTTP handlers run on their own task:
  never touch LVGL from one. HaProvider receives on the websocket task: never touch LVGL there.
- **`Edit` on a CRLF file**: deleting a line by matching a leading newline joins two lines.
- **`gh ... --body $text` in PowerShell 5.1 drops double quotes** inside the text (native argument
  passing), silently. Pipe it instead: `$text | gh issue create ... --body-file -`, or write a file.
  `git commit -m` has the same problem: write the message to a file and use `git commit -F`.
- **LVGL clips children to their parent** - use `UI::unclipShadows()`. **Never toggle HIDDEN on a
  screen-sized object when an animation starts.** Anything drawn stays ASCII, except `°`.
- **Verify from outside the device** - the router, the broker, HA, `/bench?what=verify`. The
  project's oldest rule.

## How to work with this owner

A hobbyist and ESP32 enthusiast, not a professional developer, and explicit about that - but reads
code, spots real bugs, and has caught several that were not obvious. **Treat the owner's instincts
as data.** A little colour-blind: never distinguish two states by colour alone.

**What works:**
- **Show, don't spec.** Build something to react to; the owner flashes fast and tests
  thoroughly, test by test against a sheet with PASS/FAIL criteria (`TEST_2.10a.md` is the model:
  a numbered table per round, then the owner's results under it).
- **Snappy and responsive first, eye candy second** (owner). 2.10a dropped its grow and its dim for
  it. Measure a frame before and after anything visual (`DEBUG_FRAMES`, `/bench`).
- **When the owner is testing, give ONE report when they say they are done** - not a message per
  serial line.
- **Plain language, not metaphor** ("sometimes I get a little lost in the slang").
- **Never assume. Give the top 2-3 options with a recommendation, and ask** before building on an
  assumption; discuss a structural choice a turn before implementing it.
- **Read the tool, not the name.** Check the source or the datasheet before asserting behaviour.
- **Own mistakes plainly.** Report negative results as clearly as wins, and say what has NOT been
  tested - the owner acts on it immediately.
- **Say what to expect** before anything that changes the screen - a benchmark freezes it for
  seconds; over a minute is a problem.
- **"I'm wondering whether..." is usually a design instinct** - engage with it.
- **Say plainly what is unexplained**; the owner often finds the answer (#49 ended that way).
- **Commit at the end of each step**, and push.
- **Avoid compacting context**: the owner would rather end a session with a clean handoff.

**What to avoid:**
- Committing straight to `main`. Feature branch off `main`, `--no-ff` merge once signed off.
- Guessing a fourth time. Instrument it or ask the far end.
- Claiming a script worked because it printed something. Assert the anchor, then check the file.

**Versioning (changed 2026-10-05, owner):** `A.B.C.D` - B is the roadmap PHASE, C counts RELEASES:
one more each time a signed-off merge to `main` is tagged, whatever milestones it holds. Add a
`CHANGELOG.md` entry with the tag, naming the milestones and issues in it. Never tag during
development. D is commits since the tag; a dirty tree appends `+dirty`. **`v0.2.9` was tagged at
the 2.10b merge (2026-10-06); the next signed-off merge in Phase 2 is `v0.2.10`**, whatever
milestones it holds. ROADMAP 3.3.

**The issue tracker is yours to manage**, and keeping it, HANDOFF and ROADMAP current is part of the
work, not a follow-up.
