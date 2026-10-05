# Handoff — 2026-10-02

**Start here.** `CLAUDE.md` is the stable how-it-works. This file is only: where we are, what to do
next, what will bite you, and how to work with the owner. It was rewritten from scratch on
2026-09-29; the previous one, with every session log since 2026-09-24, is
`docs/archive/HANDOFF_to_2026-09-29.md` - read it only when chasing the history of something.

**Keep it short.** A finished investigation goes to `docs/LESSONS.md` (general) or
`docs/display/history.md` (display) as a few lines; a test run goes to its test log; a number goes
to the doc that owns it. This file says where things are, not how they got there.

---

## Where the project is

**`v0.2.7` tagged 2026-09-24** (through milestone 2.7). Since then, all on `main`: #58 screenshots,
and **2.9's display stack on six of nine boards** (merge `75a43c8`, 2026-09-29).

What a board does: boots into two pages (House = the owner's 18 HA entities over the websocket,
Fleet = MQTT/system/virtual cards), swiped with wrap-around; three colour schemes (Midnight
default, Fleet, Linen with real shadows); card types with state icons; an FPS/CPU overlay;
`/screenshot` and `/bench` over HTTP.

## 2.10a status - 2026-10-04 evening (home: WS_P4_5 on COM15, WiFi)

**Branch `feat/65-popup-frame`** (off `main`). Decisions taken before building: `card-sheet.md`
section 13. **While testing, build and flash WS_P4_5 only** (owner, 2026-10-04), until told otherwise.

**THE GROW IS GONE - "usable first" (owner, round 4).** Every version of the grow was smooth in
motion and slow at its ends: the frame the window landed in (dim + window + contents) took ~120 ms
on Midnight, ~200 ms on Linen, after ~240 ms of growing; closing took 2-3 stages. "Function before
form." The window now appears complete in the first frame after the long press (250 ms) and closes
in one frame. It sits on the SCREEN as its topmost object, so LVGL draws only the window where it
covers the page; a transparent catcher on `lv_layer_top()` (made once, switched by CLICKABLE) takes
every touch outside it. **What was learned is in LESSONS, "Effects that cover the screen".**

**No dim - DECIDED (owner, round 5):** tried it, "the delay seems like an eternity and adds very
little". Removed; nothing is drawn behind the window. Blur was never built (LESSONS).

**Built (rounds 6-7):** the held card is pressed in (0.4 mm) while its border fades to the ACCENT
over the 250 ms hold - a tap shows the start and fades back; the card STAYS pressed in with the
accent while its window is open; the press moves the card's edge-attached parts too (band, tag
pills, badge, corner icon - `Card::pressLook()`; the floating tag stays, owner). **No leap**
(owner, round 6: it made the popup feel slower). The switch's knob slides on a tap and can be
DRAGGED (round 8; let go, it goes to whichever half it is in). **Interference, round 8**: one spark
at a time travels a short stretch of the window's edge (core, glow, fading tail, the odd fleck),
often running again further on; first 1.5-3.5 s after opening, then every 5-12 s, never within 2 s
of a touch; electric whites/blues on dark schemes, deep blues/violet on light (CardPopup.cpp,
"Interference"). Round 7's version - scattered one-frame pieces - read as a display fault (owner).
A long press on the title cycles now and then / once / off (`DEBUG_POPUP`). All of it
`transform_width/height`, translate, border and small rects - never a scale or a layer.
Round 8 PASSED except S1 (sparks): the owner's last tweaks (each run its own dice, faster, more
overlap) are in round 9 - "then we'll leave it alone for now either way".

**Built, round 9 (2026-10-05), flashed on WS_P4_5, not yet seen:**
- **The settings deck** (pathway 1, Card Popup Mock v3): a SETTINGS tab the window's width peeks up
  under the window; tap to open over the window's lower part; tap in the window or the tab to fold;
  tap outside closes both. Rows: **Paused (works, kept on the device - #60)**, Label, Custom name,
  On the dashboard (shown quieter and inactive: they need a card id that survives a rebuild to be
  kept - 2.10d - and a hidden card needs a way back - #78). CardPopup.cpp, "The settings deck".
- **The chart icon** (`mdi:chart-bar`, font regenerated) on the history button.
- **The window's frame**: it was redrawing from the screen's corner (LESSONS, the (0,0) bullet);
  now built quiet and only its rectangle invalidated. Expected ~45 ms off every open; the log says.
- **G1 (`lv_mem` with the window open)**: logged at every open and close (`DEBUG_POPUP`).

**Crash fixed (round 6, 2-3 on glass):** a swipe started on a card rebuilt the page and the hold
restyled the deleted card - LESSONS, "Never keep a pointer to an LVGL object you do not own".

**Open with the owner:**
- **Interference**: settled after round 9 unless the owner asks again. When and how often become a
  setting later. Linen may get its own kind of animated chrome some day (owner).
- **The deck's inactive rows**: Claude built them inactive; the owner may want Label working now (in
  RAM, keyed by page and slot, lost at reboot - card-sheet 13 allows that). Asked.
- **Versioning** (owner, 2026-10-05): tying the tag's third number to milestones has frozen the
  build at v0.2.7.x while milestones finish out of order. Claude recommended counting RELEASES
  (one tag per signed-off merge, the tag and a CHANGELOG line naming the milestones in it) - no
  milestone renumbering. Not decided.
- **Card borders in colour, a custom scheme, and the system drawer's old pages** (owner, round 6):
  #85, not for 2.10a.
- **Scaling** (owner): the toggle is "too big" on the 1060 and the 4B (it is sized from the window's
  height, so ~57 mm tall on the 1060 vs ~30 mm on the P4_5); the occupancy hero's circle is huge for
  its icon, by a different ratio per board. Placeholder visuals; the owner: not this step's focus.
  Claude's suggestion: cap heroes at the P4_5's physical size when 2.10b designs the controls.

**Built and seen on glass:** long press (now **250 ms**) opens a window on every card type; header X /
"Area > name" / history (chart icon since round 9) / members (multi-entity cards); closes by X, tap outside, drag down on the
header row, 60 s idle with a shrinking bar; modal; a switch gets its real toggle, everything else a
read-only value + "Changed N ago"; history and members are placeholder views with a back arrow.
Owner's round 1 results are in `docs/TEST_2.10a.md` terms (O/H/C/B/V/L); C3, V3, B1, H1 fixed.

The grow's history (filled box, four walls, page picture, rings - each measured) is in git
(`f21ece6`..`4f220ad`) and its conclusions in LESSONS.

`-D DEBUG_FRAMES` (GUIManager.cpp): every frame of every burst of motion over serial. `-D DEBUG_POPUP`:
what building and tearing down the window costs, printed 300 ms later so the print is not timed.

**Display-stack bug fixed (all esp_lcd DSI boards):** the repair fallback raced the PPA and left
lines behind - `docs/display/history.md`. Verified on WS_P4_5; WS_P4_4B and CYD_P4_1060 flashed with
it 2026-10-04, nothing reported; the 7B and 4880 not flashed.

**Next, in order:** (1) round 9 on glass (deck with Pause, chart icon, the faster window, sparks -
`TEST_2.10a.md`); read G1 (`lv_mem`) and the window's new frame time from the log; (2) the owner's
call on the deck's inactive rows and on versioning; (3) all-nine compile gate, look on glass, merge
with `--no-ff` - then 2.10a is done.

**The debug flags are local:** `-D DEBUG_POPUP` and `-D DEBUG_FRAMES` are in `WS_P4_TOUCH_LCD_5` in
this PC's working copy of `platformio.ini`, uncommitted. Never commit them on.

#84 (cards never say "no data yet") was filed from this testing.

## What is next — the new session's job: BUILD 2.10a (#65)

The design interview's §1 and §2 are closed; the owner chose to start building the card popup.
**Read, in this order:** `CLAUDE.md` (the file-editing rule at the top), this file,
`docs/design/card-sheet.md` **§11-12** (the decided popup - §1-10 are draft 2, superseded wherever
§11 differs), `docs/design/look-and-feel.md` §2 and §3.14 (the owner's standing rules and §1's
decisions), `docs/LESSONS.md` (layers, `lv_mem`, events not bubbling, HIDDEN on screen-sized
objects). Then **open the artifact "Card Popup Mock"**
(https://claude.ai/artifact/Lt6NmxMYLc7xrWzaomEqDm, v3) - it is the owner's approved design;
build what it shows.

**2.10a's scope (card-sheet §9, as revised by §11):** long press opens a centred window over a dim
fill (`bg_opa` on a scrim, never an object `opa`); the frame **grows** from the card as an empty
rectangle, then the contents appear; header = X top left (back arrow on inner views), "Area >
name" centred, history / members icons top right; modal (no page swipes, no edge gestures);
closing by X, tap outside, drag down, auto-close 60 s; the **settings deck peeks up from the
bottom of the screen** when the window opens and retracts with it (pathway 1); the window holds
the entity, not the card. The light / switch controls themselves are 2.10b, HA attributes and
calls 2.10c, stable ids and saving 2.10d, history 2.10e.

**How to run it:** one feature branch off `main` (e.g. `feat/65-popup-frame`). Develop on
**WS_P4_5 (COM15)** - the owner is away for the weekend with the laptop and that one board, so
expect short sessions and keep each one ending in a commit. Before merge: the **all-nine compile
gate** plus a look on glass (merge discipline below). WS_S3_4B stays unflashed until #69's watch
ends. Measure `lv_mem` with the window open (G1 in the interview) and frame time during the grow
(`/bench`).

**After 2.10:** interview §3 (header slots, 2.8), §4 (groups - `group-cards.md` has the draft),
§5-§9. ROADMAP §7 has the order.

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
| **4B panel timing** | S3_4B clean after `C2 31 02`; P4_4B faint lines at the right edge on the log page. 7-day watch | #69 |
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
| `WS_S3_4B` | COM8 (CH343) | the slow dev target; `Serial` is native USB, not the CH343 |
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
  thoroughly, test by test against a sheet with PASS/FAIL criteria.
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
- **Avoid compacting context**: the owner would rather end a session with a clean handoff.

**What to avoid:**
- Committing straight to `main`. Feature branch off `main`, `--no-ff` merge once signed off.
- Guessing a fourth time. Instrument it or ask the far end.
- Claiming a script worked because it printed something. Assert the anchor, then check the file.

**Versioning:** `A.B.C.D`, where C is the milestone within the phase. Tag on `main` at merge when a
milestone completes, never during development. D is commits since the tag; a dirty tree appends
`+dirty`.

**The issue tracker is yours to manage**, and keeping it, HANDOFF and ROADMAP current is part of the
work, not a follow-up.
