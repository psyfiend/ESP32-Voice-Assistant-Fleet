# Handoff — 2026-10-07 (2.10c round 7 built, before the owner's test)

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
window has its controls (2.10b) - on the virtual lamps in `v0.2.9`, and on HA's lights, with
members and scenes, on the 2.10c branch (below).

## The card popup: 2.10a DONE (`v0.2.8`), 2.10b DONE (`v0.2.9`)

**What the popup is now: `card-sheet.md` §14 (the frame, the deck) and §15 (the light controls,
sizes, groups, pause)** - read both before touching it; where they differ from §9-13, they win.
Every round, result and measurement: `docs/archive/TEST_2.10a.md` (twelve rounds) and `docs/archive/TEST_2.10b.md`
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
  **`view=3` lists without navigating**; any other `view=` from a member's own view first goes back
  to the group (a listing that navigated cost one 2.10c round - LESSONS). `ctl=3` is Scenes,
  `scene=N` loads one. `/bench?what=page` puts the page back afterwards, so the Fleet page cannot be
  reached from the PC.

**Flashed with v0.2.9, 2026-10-06:** WS_P4_5, WS_P4_4B, CYD_P4_1060 (the P4_5 and the 4B have since
been flashed with the 2.10c branch). Signed off on WS_P4_5; seen on
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
  them. `DEBUG_POPUP` also brings `GET /popup` (above).
- **Reading a board's boot log without a monitor:** a pyserial script that pulses RTS with DTR low
  (a normal boot) and filters lines; PlatformIO's Python has pyserial. PowerShell 5.1 strips quotes
  from `python -c "..."`, so write the script to a file.

## What is next: 2.10c is BUILT, waiting for the owner's test (#65)

**Branch `feat/65-ha-lights`:** (1) HA lights get their levels and `light.turn_on` with data; (2) the
members of a group defined in HA (`light.office`'s three bulbs) are learnt and get the Members view,
with a paused member taken out (K18); (3) scenes. **Rounds 1-3 tested by the owner** (all pass but
G8 once and S5); after them Scenes became a view of its own under the chart, the corner icons became
tabs (K19 superseded, K20), and every FAILED now records why (G8). What was built: **`card-sheet.md`
§16**. What HA does, measured from the PC: **`ha-websocket.md` §9**. Decisions: DECISIONS C5, K12,
K15, K17-K30, A13, W9.

**The owner's next job: `docs/TEST_2.10c.md` round 8** (P1-P7: CHART as a real folder, the panels'
speed, the 1060; round 7's results are recorded there). If G8's FAILED comes back, read the reason with `/popup?view=3` before theorising. **Built in
round 7** (2026-10-07): hand-drawn faces (curved metal selector, dent chips, dithered - K27), the four
named looks with per-scheme defaults, the slider's place (K26, `placeWide()`), the CHART demo panel on
History (K28), the title that gives way in steps (K29), flush SETTINGS, no "Color" word, US spelling
(K30). **Round 8**: CHART rebuilt as SETTINGS' mirror image (`buildFolder()`), leaving with the
window as SETTINGS does; panels at 260 ms; the 1060's L5 found and fixed (LESSONS, "A flex column
centres its children twice"). **Open with the owner**: Linen's default look (Silver - Square built;
round 5 said Silver - Round), and whether 260 ms is smooth enough (300 is the next step). **Still
open in 2.10c:** live switching while the selector is dragged (measure with `DEBUG_FRAMES` first),
then the merge gate. `/popup?ctl=` now also prints where the stage's children landed. Words for the window's parts: `card-sheet.md`
"Words" (K22) - "deck" means the control deck. `/popup` gained `show=0|1|2` (which scenes) and
`deck=2` (CHART open, on History); `look=4` returns to the scheme's own look.

**Boards: WS_P4_5 (COM15), WS_P4_4B (COM7) and CYD_P4_1060 (COM9) all carry round 8.** COM9 is
sometimes held by another program on the PC (most of 2026-10-07's afternoon); retry later. All with
local `DEBUG_POPUP` (P4_5 also `DEBUG_FRAMES`) in the uncommitted `platformio.ini` - put it back
before the merge gate. Everything was driven over `/popup` against the real Office lights; **no finger has
touched any of it**, and All Lamps (Fleet page) was not re-checked after the card code changed.

**Then:** fix what the rounds find, the all-nine gate, merge `--no-ff`, tag `v0.2.10`, CHANGELOG.
After 2.10c: 2.10d stable ids and saving (which also unlocks "add a member as a card", K17), 2.10e
history; then interview §3 onward. `docs/ROADMAP.md`'s running order has it.

**Real lights (W9):** any Office light may be commanded while testing, and is put back as found
(100%, 2710 K when last left); **ask before the Kitchen.** PC scripts that drive them are easy to
rebuild from `ha-websocket.md` §9; the long-lived token is in `ConnectivityLocalSecrets.h`.

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

## Where the design interview stands

`docs/design/interview-phase2-3.md` is the blueprint for 2.8, 2.11 and Phase 3; Claude facilitates.
**§1 look and feel CLOSED 2026-10-01** (`look-and-feel.md`); **§2 card popups CLOSED 2026-10-02**
(`card-sheet.md` §11-12, built as §14-15). **§3 onward is next after 2.10**, and #87 (the companion
web app) should be scoped there, before 3.1. The group taxonomy draft for §4 is `group-cards.md`.
Artifacts on claude.ai: "Card Popup Mock" (2.10), "Layout Playground" v4 (the arranger, #78),
"Beyond the Grid", "Linen Fill and Header Glyphs", "Fleet Status Glyphs" - links in the design docs.
Linen's "on" colour (olive `9AA35A`) was the owner's executive decision; alternatives for 4.1 are in
`UITokens.cpp`.

## Fires still burning — each has an owner doc or issue, none needs the next session

| What | State | Where |
|---|---|---|
| **Battery** | **prototype merged** (`a283fd0`, 2026-09-30). Everything current: **`docs/design/power-battery.md`**. Runs only with `-D HAS_BATTERY` (7B, 4880). Voltage good; the inferred power state works for charging / on battery (P1-P3 pass) but cannot see a missing cell while USB is present (P5 fail). **Owner's direction: do not perfect the inference; fuel gauges (MAX17043, ordered) on the units that carry a battery.** Also open: the AXP2101 on the S3_4B, a power-state entity for the header (#19) | #72 |
| **2.9 last boards** | `CYD_S3_8048`, `WS_S3_5B` to move to esp_lcd when on the desk; then #67 closes | #67, `docs/display/README.md` |
| **4B panel timing** | S3_4B clean after `C2 31 02`; P4_4B faint lines at the right edge on the log page. 7-day watch. **S3_4B found locked up 2026-10-05** after days untouched; left as found, to troubleshoot later (owner). What to check first, before resetting it: #69's latest comment | #69 |
| **S3 LVGL speed** | S3_4B feels heavier since LVGL's pool moved to PSRAM. Paused by the owner; resume soon | #70 (full reasoning in its comments) |
| Rotation setting / IMU | recorded | #71 |
| Fonts / type ladder | recorded, feeds the interview | #73 |

**Branch rule and merge gate: `docs/DECISIONS.md` W1-W2.** Never start a branch on an unmerged one
without asking.

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
2. `docs/ROADMAP.md` (the plan, and where each kind of note belongs) and `docs/DECISIONS.md` (every
   decision in one line) - both short.
3. `docs/LESSONS.md` — before debugging anything.
4. For UI work: `docs/design/interview-phase2-3.md`, `card-sheet.md`, `cards.md` §13, `pages.md`,
   `card-layout.md` (before moving anything inside a card), `tokens.md`, `dashboard.md`,
   `ha-websocket.md`, `startup.md`.
5. For the display: `docs/display/README.md` only - it indexes the rest.

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
  thoroughly, test by test against a sheet with PASS/FAIL criteria (`docs/archive/TEST_2.10b.md` is the model:
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
milestones it holds. `docs/DECISIONS.md` W3.

**The issue tracker is yours to manage**, and keeping it, HANDOFF and ROADMAP current is part of the
work, not a follow-up.
