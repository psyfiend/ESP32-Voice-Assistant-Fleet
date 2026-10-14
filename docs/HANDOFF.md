# Handoff — 2026-10-13 (2.12 UI foundation BUILT on `refactor/ui-foundation`, PR open for the owner's test and merge)

**Start here.** `CLAUDE.md` is the stable how-it-works. This file is only: where we are, what to do
next, what will bite you, and how to work with the owner. It was rewritten from scratch on
2026-09-29; the previous one, with every session log since 2026-09-24, is
`docs/archive/HANDOFF_to_2026-09-29.md` - read it only when chasing the history of something.

**Keep it short.** A finished investigation goes to `docs/LESSONS.md` (general) or
`docs/display/history.md` (display) as a few lines; a test run goes to its test log; a number goes
to the doc that owns it. This file says where things are, not how they got there.

---

## Where the project is

**`v0.2.12` tagged 2026-10-13** at the K17 merge (a group's member as a card of its own);
**`v0.2.11`** 2026-10-12 at the merge of 2.10d and 2.10e - **milestone 2.10, the card popup, is
done** (`v0.2.10` was 2.10c, `v0.2.9` 2.10b, `v0.2.8` 2.10a); `CHANGELOG.md` matches versions to
milestones.

What a board does: boots into two pages (House = the owner's 18 HA entities over the websocket,
Fleet = MQTT/system/virtual cards), swiped with wrap-around; three colour schemes (Midnight
default, Fleet, Linen with real shadows); card types with state icons; an FPS/CPU overlay;
`/screenshot`, `/bench` and `/panel?resend=1` over HTTP; **a long press opens a card's popup
window**; a light's window has its controls, on the virtual lamps and on HA's lights, with the
members of an HA group and the light's HA scenes; **every card's History** - a number's chart, or
anything on/off as its activity - from HA; the owner's choices saved by card id on LittleFS.

## What is next

0. **2.12 UI foundation: BUILT 2026-10-13, all seven phases, on `refactor/ui-foundation`; the pull
   request is open and waits for the owner.** Nothing should look or behave differently - the
   harness compared every window, view, panel and dialog at every phase (`docs/design/ui-foundation.md`
   §6, phase by phase). **Owner's run 1 (2026-10-14, `docs/TEST_2.12.md`): all PASS but T9**, the
   4B dialog's overlapping buttons, which `v0.2.12` already had; Q2-Q7 answered (W13 accepted, the
   gate fixed); Q1 re-asked; U1-U5 still the owner's to accept. On sign-off: the CHANGELOG entry
   (`v0.2.13`) and the sheet to `archive/`, on the branch; the owner merges; then `git pull`, tag
   `v0.2.13`, flash. **After the merge, in this order:** T9's dialog buttons (they share the row,
   24 mm at most); the owner's run-1 asks - About this board as a button on the system panel's
   Log | Tokens | Cards row, Entity properties / About with both columns left aligned and the gap
   halved, the countdown bar thinner (1-2 px) and on the window's bottom edge; then the ideas in
   the owner's 2026-10-14 notes (the selector for the span buttons, #92; a quieter "glass" for
   things that can be tapped; the dialog's own countdown, with #23) - discussed before building.
1. **What 2.12 leaves for the next work:** a window is a `Kit::Sheet` built from the component kit
   (`include/UI/Kit.h`, `Sheet.h`; `docs/design/components.md`, with pictures); the window's sizes
   are `UIShape` millimetres (`tokens.md` §7); the dashboard's placement is `Layout::FlowEngine`
   (`ui-foundation.md` §8 designs 2.11's engines on it); pure modules have tests on the PC
   (`host-tests/`); `scripts/structure_check.py` reports drift. #92 (the span chips as the
   control deck's selector) can now reuse `Kit::selector()`.
2. **The design interview §3 onward** (below), then 2.8 header slots (#19), then 2.11 group cards
   with the page arranger (#66, #78). `ROADMAP.md`'s running order.

**Guards in place (2026-10-13, W10-W11)** - read CLAUDE.md "Guards" and "Subagents" before your
first command: a hook refuses destructive git/delete commands (also in subagents), the pre-commit
hook refuses the local debug lines and corrupted bytes, release builds go through
`scripts/release/gate.ps1` and `flash_release.ps1`, and read-only work goes to the `explorer` /
`auditor` agents. **`main` takes changes only through a pull request the owner merges (W12)**:
push the branch, `gh pr create`, wait for the owner's Merge, then `git pull`, tag, flash.

**K17 DONE** (`v0.2.12`, K65-K70): `CardAdded` (`include/Cards/CardAdded.h`) keeps cards made on the
device in the settings file and merges them into their page after the card they came from; Remove
asks in a dialog; removed cards are the file's pool. `/popup?addcard=1|removecard=1|2`. Open from
it: a group's member shows Area in HA / Device "None" - HA's area template runs before the members
are learnt (#93). Sheet: `docs/archive/TEST_K17.md`.
Owner's ideas parked for after 2.10 has settled: the light controls' sliding selector for the
chart's span chips (#92); a window timeout and the countdown bar, device-wide (#23).

## The card popup: 2.10 DONE (`v0.2.8` - `v0.2.11`)

**What the popup is now: `card-sheet.md` §14 (the frame, the panels), §15 (the light controls,
sizes, groups, pause), §16 (HA lights, members, scenes, and the window as redesigned in 2.10c),
§17 (saving, 2.10d) and §18 (History, 2.10e)** - read them before touching it; a later section
wins where they differ; the window's parts have names ("Words", K22). Every round, result and
measurement: `docs/archive/TEST_2.10a.md` (twelve rounds), `TEST_2.10b.md` (seven),
`TEST_2.10c.md` (nine), `TEST_2.10d.md`, `TEST_2.10e.md` (six). The rules that cost the most to learn:

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

- **Hand-drawn faces are cached, and the cache must give slots back** (it filled after a few page
  swipes once - LESSONS). **Never read from a DSI panel that shows a picture** - it blanked all
  three P4s (LESSONS). **Keep moving objects tight**: a transparent holder as wide as the window made
  each frame of a panel's slide redraw everything under it.

**Flashed with `v0.2.11`, 2026-10-12 (release flags, a clean tree):** WS_P4_5, WS_P4_4B,
CYD_P4_1060 - the three on the desk. WS_P4_7B and CYD_P4_4880 still run `v0.2.10`. **WS_S3_4B is
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
  them. **`-D DEBUG_HANG`** (2.10d): HangWatch, `GET /hang` - where `loop()` stopped, the last PPA
  jobs and the DMA2D registers; `/hang?test=1` makes a 4 s stall to check it. `DEBUG_POPUP` also brings `GET /popup` (above).
- **Reading a board's boot log without a monitor:** a pyserial script that pulses RTS with DTR low
  (a normal boot) and filters lines; PlatformIO's Python has pyserial. PowerShell 5.1 strips quotes
  from `python -c "..."`, so write the script to a file.

## 2.10d and 2.10e - DONE (`v0.2.11`, 2026-10-12)

**2.10d, saving:** `card-sheet.md` §17, CLAUDE.md "Saved settings", K33-K50, A14, A15.
**2.10e, History:** `card-sheet.md` §18 (rounds 1-6 at its end), K52-K64. `HaHistory`
(`components/Fleet_HA`) asks HA's websocket on the loop task and parses on the websocket task;
HaClient's buffer is 48 KB in PSRAM; `TimeFormat` (`include/`) says every time and duration.
`/popup?hist=1|span=|more=|row=|x=|dur=|chips=` drive History from a PC (DEBUG_POPUP).
Both signed off on the P4_5, 4B and 1060; the all-nine release gate passed at `f1810ea` (34 min,
the local debug lines stashed and restored - the way to run it again).

**Carried forward from 2.10d and 2.10e:**

- **The freezes (A15).** The P4_5 froze three times on 2026-10-09 in card windows; HangWatch
  (`include/HangWatch.h`, `-D DEBUG_HANG`, `GET /hang`) caught `loop()` blocked in
  `ppa_do_operation()` - **IDF issue #19023**, the PPA never finishing a 90/270-degree strip
  rotation. Fixed on IDF master, not v5.5; the P4 libraries were rebuilt with that one patch
  (`scripts/idf-patches/`, REBUILD_P4_LIBS "third rebuild"). **No freeze since**, through all of
  2.10e's testing on three boards. If one comes back: `/hang` on the P4_5 before a reset (it lists
  the last 16 PPA jobs and the DMA2D registers). The 2.10c "hang, once" (below) was probably the same.
- **The P4_5's white flash (#91)** - the P4_5 alone (the 4B and 1060 never showed it), with
  sustained drawing - dragging panels, dropdowns - and no save near it; `/panel` is blind to the
  first 413 lines of a frame. Instrumenting it may need a library rebuild: ask the owner first.
- **The Card label row is greyed** (K48 - its redesign, Current group / None / Custom with a name
  window, comes after). The owner's ideas for the window's panels and a group's own window:
  `interview-phase2-3.md` §12. #90: the window title shows the current name, after HA's names are
  tidied. #92: chart options (fixed range, bars, compare, humidity, the sliding span selector).
- **The laptop:** when the owner tests away from the desk, the P4_5 is reflashed from the laptop
  (COM6) to join that Wi-Fi (credentials are compiled in) and is at **`http://10.0.0.2`** - that
  router resolves neither `fleet-ws-p4-5` nor `.local`.
- **The S3 RGB boards** (S3_4B, S3_5B, 8048) probably flash on writes too
  (`CONFIG_LCD_RGB_ISR_IRAM_SAFE` off, stock libraries) - untested, none on the desk working.
- **HangWatch stays** (committed, gated by its flag).

**Owner's notes for later** (K48): moving a card to another group gets its own row and
window (a list of groups, each openable to its members; "change the label" or "move into a group
panel"), and the three things now called "group" need names - the shared label, a group card like
Desk, a group panel on the dashboard. Also: NINA-style toasts, the grid arranger with
2.11 (#78), scene secondary info (#89), the web UI for names and groups. The Selector (K31) is a
device-wide choice that belongs on the device's own settings page (4.1).

**Still open from 2.10c, none blocking:**
- **The 4B washed out, twice ever**: the picture near-white and lurid while `/screenshot` was right -
  the panel, not the frame buffer. The next time, before resetting: `http://fleet-ws-p4-4b/panel?resend=1`
  (`src/PanelDebug.cpp`). If that cures it, the fix is to re-send now and then, blind - **reading the
  panel to detect it is not an option** (it blanks the picture - LESSONS).
- Switching the control live while the selector is dragged (measure with `DEBUG_FRAMES` first).
- Linen's panel slides are a little choppier than the dark schemes' on the P4_5 ("totally
  passable"); each frame measured 8-40 ms.
- The 4880 showed the window in portrait for the first time; nothing was tuned for it.
- **The panel's own settings as HA entities** (owner's idea; FUTURE_IMPROVEMENTS, with 4.1).

**Debugging the window from the PC:** `/popup` (DEBUG_POPUP) also takes `show=0|1|2` (which scenes),
`deck=2` (CHART open, on History), `look=0..4` (the selector for the scheme showing; 4 = its own),
`scheme=0|1|2`, and `ctl=` prints where the stage's children landed; its listing says how many faces
came out flat.

**Real lights (W9):** any Office light may be commanded while testing, and is put back as found
(100%, 2710 K when last left); **ask before the Kitchen.** PC scripts that drive them are easy to
rebuild from `ha-websocket.md` §9; the long-lived token is in `ConnectivityLocalSecrets.h`.

**Direction (owner, 2026-10-06)**: a companion web app with a true preview that sends the config to
the device (#87) - the build sheet becomes its output, not the user's interface; and presentation
(group/room cards) as how this project stands apart. Also filed: #86 (card control styles, from
HomeTiles), #88 (LVGL 9.6 evaluation, after 2.10). An HA-side companion is a separate, later
choice; stock HA served everything 2.10e needed (`card-sheet.md` §18).

**How to run it:** one branch off `main` (e.g. `feat/17-member-card`). While the owner tests, build
and flash WS_P4_5 only (COM15) unless told otherwise; through 2.10 the owner also had the 4B (COM7) and
1060 (COM9, which is sometimes "busy" or "not functioning" - a replug cures it) connected and
tested on all three. WS_S3_4B is locked up (#69): leave it until the owner says. Before merge: the
all-nine compile gate without the local debug lines (stash `platformio.ini`, build, pop) and a look
on glass, then `--no-ff`, a tag (`v0.2.12`) and a CHANGELOG entry (Versioning, below).

## Where the design interview stands

`docs/design/interview-phase2-3.md` is the blueprint for 2.8, 2.11 and Phase 3; Claude facilitates.
**§1 look and feel CLOSED 2026-10-01** (`look-and-feel.md`); **§2 card popups CLOSED 2026-10-02**
(`card-sheet.md` §11-12, built as §14-15). **§3 onward is next, after K17**, and #87 (the companion
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
| `WS_P4_5` | COM15 (CH343 `5B90154141`) | dev board; **screwed into its enclosure** - no battery test |
| `WS_P4_4B` | COM7 (CH343 `5AAF289419`) | P4 rev v1.0 (`esptool read-mac`), the only one on a CH343 |
| `WS_P4_7B` | COM6 (native USB) | **needs two USB cables** or it browns out on boot |
| `CYD_P4_1060` | COM9 (native USB, serial `30:ED:A0:E3:2C:7B`) | also needs two cables; its log names it. **Flash it at 460800 and end with `--after watchdog-reset`**: on 2026-10-13 two 1.5 Mbaud uploads lost its USB, the second leaving it dark; BOOT held while plugging in, then a watchdog reset, brought it back (LESSONS) |
| `CYD_P4_4880` | COM19 (native USB) | LiPo fitted; browned out on the PC's USB alone |
| `WS_S3_4B` | COM8 (CH343 `5A67166678`, not probed) | the slow dev target; `Serial` is native USB, not the CH343. **Locked up 2026-10-05** - #69 |
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
   `card-layout.md` (before moving anything inside a card), `tokens.md`, `components.md` (the
   kit and the Sheet), `ui-foundation.md` (§8: the layout engines), `dashboard.md`,
   `ha-websocket.md`, `startup.md`.
5. For the display: `docs/display/README.md` only - it indexes the rest.

## Environment state git cannot see

**The ESP32-P4 framework libraries are REBUILT, not stock.** `esp32p4_es` in
`~/.platformio/packages/framework-arduinoespressif32-libs/` carries `MEMPOOL_PREFER_SPIRAM` and a
64-byte L2 line - the fix for esp-hosted-mcu#243 (#49) - and, since 2026-10-08,
`LCD_DSI_ISR_CACHE_SAFE` (flash writes flashed the panels blue; A14), and since 2026-10-09 IDF's
**PPA rotation fix** (the freezes; A15, a source patch in `scripts/idf-patches/`). Beside it:
`esp32p4_es.cache_safe` (before A15), `esp32p4_es.hosted_fix` (#49 only), `esp32p4_es.stock.55.03.311`;
rollback is a rename. A `pio pkg update` or platform reinstall silently puts the bugs back.
`docs/REBUILD_P4_LIBS.md`; the build tree is in WSL on the desktop PC (`~/esp32-arduino-lib-builder`,
`esp-idf` carries the patch, so its version reads `-dirty`). The S3 libraries are stock.

**This is per machine.** A build on a machine with the stock or older libraries brings back the
freezes, the flash-write flash and #49. **The folder is published as a GitHub pre-release**:
<https://github.com/psyfiend/ESP32-Voice-Assistant-Fleet/releases/tag/p4-libs-2026-10-09>
(`esp32p4_es-2026-10-09.zip`, 97 MB, SHA-256 `E30C92D7...2043307` in the notes; install steps there).
Check after a build: the firmware's IDF version string ends `-dirty`
(`grep -a -o "v5.5.5-832-g2553c5ad432[-a-z]*" .pio/build/<env>/firmware.bin`). A new rebuild gets a
new pre-release, never git (the folder is 367 MB; one library is over 50 MB).
**The laptop** has the `p4-libs-2026-10-09` folder too (installed 2026-10-10, SHA-256 checked;
`WS_P4_TOUCH_LCD_5` built and its firmware reads `-dirty`). Its older folders have different names
from the desktop's: `esp32p4_es.a10-2026-09-22` (#49 only) and `esp32p4_es.stock.55.03.311`.
The local debug flags in `platformio.ini` are per machine too: the laptop's (skip-worktree, its own
path prefix) has `DEBUG_POPUP` and `DEBUG_FRAMES` on `WS_P4_TOUCH_LCD_5`.

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
- **`PYTHONIOENCODING=utf-8` before any `pio` whose output you pipe**, or it dies silently - and
  an unattended upload gets a `timeout` (one sat for two hours, LESSONS).
- **The UI harness** (`scripts/ui_golden.py`, `ui-foundation.md` §6): never two runs at one board
  (each turns the sparks off and on); compare A and B captured minutes apart, and capture a state
  again both ways when the house changed under it. A window closes itself after a minute, so a
  board whose screenshots are slow fails its walk - the 1060 took ~40 s a screenshot on the
  afternoon it went down.
- **Host tests: `wsl bash host-tests/run_in_wsl.sh`** - Windows has no compiler for
  `pio test -d host-tests` (`host-tests/README.md`).
- **Never run `/panel?flash=N` without `&nvs=1`**: it erases the start of the `spiffs` partition,
  which has held LittleFS and the settings file since 2.10d - very likely every saved setting
  lost at the next boot. Found 2026-10-13 writing `docs/HTTP_REFERENCE.md` (every route, what it
  reads and changes); the fix is the owner's to choose.
- **Windows PowerShell 5.1 (the only one here) makes a native program's stderr fatal under
  `$ErrorActionPreference = "Stop"`.** It stopped `scripts/release/gate.ps1` at the first S3 (its
  linker warns on stderr) on 2026-10-13; fixed 2026-10-14 with the owner's OK (TEST_2.12 Q7) -
  `"Continue"` around the build, its exit code the result. Any new script that runs `pio` needs the
  same.
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
development. D is commits since the tag; a dirty tree appends `+dirty`. **`v0.2.12` was tagged at
the K17 merge (2026-10-13); the next signed-off merge in Phase 2 is `v0.2.13`**, whatever
milestones it holds. `docs/DECISIONS.md` W3.

**The issue tracker is yours to manage**, and keeping it, HANDOFF and ROADMAP current is part of the
work, not a follow-up.
