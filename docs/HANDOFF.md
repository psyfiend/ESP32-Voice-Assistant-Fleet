# Handoff — 2026-10-30, night (2.11 signed off, `v0.2.19` at the owner's merge; next: 2.13 and 2.14, designed with Home Assistant)

**Start here.** This file is only: what to read, where we are, what to do next, what will bite you,
and how to work with the owner. The history of how things got here is in the test sheets
(`docs/archive/TEST_*.md`), `DECISIONS.md` and `CHANGELOG.md`; earlier handoffs are
`docs/archive/HANDOFF_to_2026-10-14.md` and `HANDOFF_to_2026-09-29.md`, and this file's own history
is in git.

## Read in this order - all of it, before starting

1. `CLAUDE.md` - the HAL/BSP, the rules, **and the file-editing rule at the top**.
2. `docs/ROADMAP.md` (the plan, and where each kind of note belongs) and `docs/DECISIONS.md` (every
   decision in one line - 2.11's are K88-K129).
3. **`docs/design/ui-architecture.md`**, then what it points at - `components.md` (the kit, the
   finishes), `tokens.md`, `header-slots.md` (the claim model 2.14 builds on), `card-sheet.md`
   §14-18, `layout-engines.md` (the arranger), `cards.md`, `pages.md`, `card-layout.md` (before
   moving anything inside a card), `group-cards.md` §6-7 (2.11 as built), `interview-phase2-3.md`.
4. **Home Assistant** (sibling folder `../Home Assistant/`, its own git repo): `HOUSE_RULES.md` (the
   three layers, naming, labels, the roll-up and alert contract), `FLEET_REFERENCE.md` (what the
   panels read), `docs/entity_reference.md` (every logical entity, roll-up and alert, with its
   members) and its `ROADMAP.md`. **2.13 and 2.14 are half HA work** - see "Home Assistant" below.
5. `docs/archive/TEST_2.11.md` - how 2.11 was tested and what the owner said, round by round.
6. `docs/LESSONS.md` - before debugging anything. `docs/HTTP_REFERENCE.md` - before calling a route.
   `host-tests/README.md` - before touching `src/Layout/`.
7. For the display: `docs/display/README.md` only - it indexes the rest.

**Keep it short.** A finished investigation goes to `LESSONS.md`; a test run to its test sheet; a
decision to `DECISIONS.md`; a number to the doc that owns it. This file is rewritten at a handoff.

---

## Where the project is

**2.11 is finished.** The owner signed it off on 2026-10-30 after sixteen rounds and 54 builds
(`docs/archive/TEST_2.11.md`). It is **`v0.2.19`**: [PR #113](https://github.com/psyfiend/ESP32-Voice-Assistant-Fleet/pull/113)
from `feat/2.11-group-cards` (all nine built, 32:36) is open for the owner to merge; the tag goes on the merge commit (below). The release before was `v0.2.18`
(2.8, the header bar v2). `CHANGELOG.md` matches every version to its milestones.

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
| `v0.2.19` | 2026-10-30 | **2.11, group cards** (16 rounds) | **63,694** | **259** |

Counted with `git grep -c '' <rev> -- src include components ':!components/lvgl' ...` (the
exclusions above, and `':!*font*' ':!*Font*'`) over `.c .cpp .h .hpp`; it gives 63,569 at round 14's
`0b6c978`, as recorded then.

What a board does: boots into two pages (House = the owner's HA entities over the websocket, Fleet =
MQTT/system/virtual cards), swiped with wrap-around; three colour schemes (Midnight default, Fleet,
Linen); **clusters** - a group as one block of tiles under a header - and solo cards, **placed by the
arranger**; a long press opens a card's window (controls, members, scenes, History, SETTINGS,
Visibility, Card UI) or a cluster's (its Members, with the CLUSTER panel); the header's slots open
panels (pages, hidden cards, the system panel); the owner's choices saved by card id on LittleFS;
HTTP routes for screenshots, benches, settings and debugging (`HTTP_REFERENCE.md`).

**The UI, in one paragraph** (details: `ui-architecture.md`): every window is a `Kit::Sheet` client;
buttons, dialogs, rows, panels and the selector are the component kit (`Kit::`), each look a *role*
- shared styles that follow the scheme - and a tappable surface has a finish (satin, glass, lit
glass); every colour and size is a token, a window's sizes millimetres (`UIShape`); where cards go
is the arranger's (`Layout::arrange()`), where the header's slots go `Layout::HeaderPlan`'s. Changes
are proved with the harness (`scripts/ui_golden.py`), host tests (`host-tests/`), the structure
check and the all-nine gate. `GUIManager` and the system panel's pages are not on the kit yet -
bring each over when it is next changed (`ui-architecture.md` §6).

**Where 2.11's pieces live**: a cluster `Cards/ClusterCard` + `Cards/MiniTile.cpp`, its span and tile
grid `Layout/Cluster.h`; the arranger `Layout/Arranger.h` (host-tested) and `Cards/PageArranger`
(its task at the idle priority; kept answers `pages.<slug>.layout`, `v4`), `CardPage::arrangePage()`;
the pins in `GUIManager` (`saveGrid()` in `GUIManagerTweaks.cpp`); the Reset
`CardAdded::resetToCompiled()`; the keyboard `UI/Kit/KitKeyboard.cpp`; **the cluster's window**
`Popup/PopupCluster.cpp` (the CLUSTER panel `clusterSpecs()`, Header options `buildClusterSlots()`;
the panel chosen in `PopupPanels.cpp`'s `sideFor()`; a cluster's `VIEW_MAIN` turned into Members at
the top of `showView()`, K129); Members' rows `memberRow()` / `memberState()` in `PopupMembers.cpp`;
Visibility, Add to, which group `Popup/PopupVisibility.cpp`; the hidden cards `UI/HiddenSheet` +
`GUIManagerHeader.cpp`; copies, moves and the pool `Cards/CardAdded`; the page panel
`UI/SlotPanels.cpp` (its line that unrolls: `Kit::drawerReveal()`, `KitDrawer.cpp`). **Changing the
arranger**: the playground first, then `host-tests/README.md`'s steps; the boards search with
`Layout::ARR_BUDGET_BOARD` (7,000), the host tests with 20,000; bump the kept-answer version in
`PageArranger.cpp` when the search changes.

## What is next

1. **The release, once the owner has merged** (he said he would on 2026-10-31): `git checkout main`,
   `git pull`, tag the merge commit `git tag -a v0.2.19 -m "v0.2.19 - 2.11 group cards"`, push the
   tag (`git push origin v0.2.19`). Then **build after the tag** (the version comes from
   `git describe`, so an image built before it says `0.2.18.N`) and flash, saving each board's
   settings first (`flash_release.ps1` does both for the three dev boards; it builds from the local
   `platformio.ini`, their `DEBUG_POPUP` lines included). **And the 4880** (owner, 2026-10-30): it
   still runs v0.2.9-era firmware, older than the settings file (`v0.2.11`), so there is nothing to
   save - check `GET /settings` anyway; its first boot formats the blank partition (A17). Read
   `HARDWARE_STATUS.md` and `docs/archive/TEST_CYD_P4_4880.md` before flashing it (COM19, native
   USB; it browns out on the PC's USB alone - the LiPo must be in). Confirm each board's version in
   `/settings?stats=1` or HA's `sensor.<device>_firmware`-style entity, not the cable.
2. **A short ROADMAP rewrite with the owner.** Settled with him 2026-10-30: **2.13 is "Add cards from
   HA" and 2.14 is alerts (#76)**, designed together (DECISIONS K130). Proposed and leaning yes, to
   settle then: **Phase 3 (the build sheet, 3.1-3.3) after 4.3 (the navbar) and the alerts**, since
   the settings file is already the runtime schema and what a schema most needs to say - the navbar,
   page navigation, alert slots, cluster-to-area links - is not designed yet; 3.x may shrink to the
   settings file's import / export plus the companion app (#87). Also to place: **the bedroom alarm
   clock** (the owner wants a board as his alarm clock: #100 NINA's clock and K81-K84, alarms that
   ring through the board's own speaker and do not need HA or Wi-Fi to ring, night dimming from
   4.1 - `schedule.night_hours` is HA's "night" - and **OTA with rollback, 5.2**, because a board on
   a nightstand is a board off the desk), and **weather-station and temperature cards** (HA's
   weather entity: forecasts come from the `weather.get_forecasts` service now, not attributes; the
   History the cards already fetch). The proposal made to him: 2.13, then the clock with OTA and
   dimming, then 2.14, then the navbar, then the schema.
3. **The 2.13 / 2.14 design interview** - like 2.11's §4 (`interview-phase2-3.md`), mocks first,
   with the Home Assistant side in the same conversation (next section). Then build 2.13.
4. Waiting: **#104** (a light's temperature mark moves between builds); **#84** "no data yet";
   **#99's S3 frame check** (W6) once an S3 is on the desk; the header clock's format (interview §3
   Q3); from 2.11's final pass, a header panel's temperature opening its History and a cluster's own
   card state (ROADMAP 5a).

**After any merge**: `git pull`, tag the merge commit (annotated, `vX.Y.Z - <milestones>`), push the
tag, flash the boards on the desk. The CHANGELOG entry and the archived test sheet go on the branch
before the owner merges. **The owner merges** (W12); a PR is opened only after the owner's sign-off.

## Home Assistant - the other half of 2.13 and 2.14

The owner reorganised HA on 2026-10-28 with a Claude session that did not know this project well,
steering it toward the dashboard (the `fleet` label, alert templates, logical entities). **He calls it
a first draft**: the next session is the one that knows what the dashboard needs, and its job is
both to build the dashboard side and to **propose improvements to HA's own structures and entities**
so the two fit - not only to consume what is there. Roll-ups, logical entities and alerts are to be
designed with 2.13 and 2.14 both in mind.

**What HA offers now** (`HOUSE_RULES.md` §1, §4-5, `FLEET_REFERENCE.md`): three layers - physical
(swappable), logical (`sensor.office_temperature`, permanent; a `source` attribute names the member
in use, `min` / `max` / `mean` with two or more) and status (roll-ups and alerts). Labels: `fleet`
(may be a card), `room_climate`, `perimeter`, `alert_high`, `alert_low`, `no_alerts`, `test`. The
stable contract: `sensor.alerts_active` (count; `level` high / low / none; names and entity ids per
level), `sensor.doors_open`, `sensor.lights_on` (with `by_area`), `sensor.house_temperature`,
`sensor.outdoor_temperature`, `binary_sensor.garage_doors` (`open`: North / South / Both / none),
`sensor.batteries_low`, `sensor.devices_unavailable`, `schedule.night_hours`. No room-wide light
groups: a room is controlled by targeting its area. A tested `render_template` "manifest" builds
clusters from areas and labels.

**Questions the interview has to settle** (my reading, 2026-10-30 - check them with the owner):

- **Offered, not imposed.** The owner was firm (ROADMAP 5a, K118-K123): the dashboard never has to
  follow HA's organisation and needs no particular HA layout - which is why there is no manifest
  dependency. `FLEET_REFERENCE.md` rule 3 ("Discover, don't hardcode", the manifest) reads the other
  way. Proposed: HA's areas, labels and the manifest feed **the Add cards picker and an area link's
  suggestions**; the board's settings file stays the truth. Ask the HA side to say so in its docs.
- **Entities at runtime** - the main firmware work of 2.13. HA entities are compiled today
  (`components/Fleet_Providers/ExternalEntities_HA.h`); a card added from HA needs an entity created
  at runtime, kept in the settings file, subscribed on the websocket, within the registry's room.
- **Per-area status for cluster headers**: `alerts_active` is house-wide; a cluster wants its own
  area's alerts (an attribute keyed by area, as `lights_on.by_area` is), doors open, lights on,
  occupancy. Which of these are HA roll-ups and which the board derives is a design choice to make
  once, for 2.13 and 2.14 together.
- **A cluster's "all off"** with no HA light groups: a service call targeting an area. Check what the
  firmware's command path can target today (entity ids).
- **`unknown` after an HA restart** should render as unavailable (`FLEET_REFERENCE.md`) - check that
  the firmware does.
- **The boards as HA devices**: they publish their own entities (`HaPublisher`, MQTT) and carry
  `no_alerts`; the dashboard's settings as HA entities (FUTURE_IMPROVEMENTS) would land in HA's
  structure too.

**How to work across the two projects - ask the owner first.** Until now this project only read HA
(`HOUSE_RULES.md` §7: HA sessions write HA's rules; this file said changes to HA are asked for
there). The owner now wants this project's Claude to help improve HA. **Ask him at the start**
whether a session here may change HA directly - through the `ha-mcp` tools (this environment has
them; the server's best-practices skill must be read before any HA change) and the HA folder's own
process (`logical/house_logical.py`, regenerate, `tools/config_snapshot.py`, a backup before
anything irreversible, `HOUSE_RULES.md` §6) - or writes its asks for an HA session. Until he says,
read only.

**HA's state** (its `ROADMAP.md`): the Tuya Wi-Fi switches are all unavailable - among them **Office
Light Overhead**, the garage overhead, the laundry and porch lights; the owner is fixing it. Test
with the Office's Desk, Left and Lamp meanwhile. Kitchen Leak Left is offline.

## Where the design interview stands

`docs/design/interview-phase2-3.md` is the blueprint for 2.8, 2.11 and Phase 3. §1 look and feel,
§2 card popups, §3 the header's slots (Q3, the clock's format, still open) and **§4 (2.11) are done**
(`group-cards.md` §6-7, K88-K129; Q8 - "back" from a group page - open, with the group view in 2.13).
The 2.13 / 2.14 interview is next. #87 (the companion web app, D-8) is to be scoped with Phase 3;
the owner would start it with dashboard arranging (FUTURE_IMPROVEMENTS). Artifacts on claude.ai:
[Layout Playground](https://claude.ai/artifact/SBSanCnwviNbKzQmkwiJXp) (page v12, the arranger's
spec; also `host-tests/tools/playground.html`), [2.11 Round 9 Mocks](https://claude.ai/artifact/SpSG1hhpbJsUmxBLv3cdfY)
(tile borders A-E, E chosen, and **the group view** for 2.13), "2.11 Clusters", "Panel Edges", "2.8
Header Slots", "Glass, Satin, Flat", "Card Popup Mock", "Beyond the Grid", "Linen Fill and Header
Glyphs", "Fleet Status Glyphs" - links in the design docs.

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
| `CYD_P4_1060` | COM9 (native USB, serial `30:ED:A0:E3:2C:7B`) | needs two cables; local `DEBUG_POPUP`. **Flash at 460800 with `--after watchdog-reset`** (LESSONS). **A software restart can boot-loop it** (its Wi-Fi co-processor): put a settings copy back with `-NoReboot`, then `esptool ... --after watchdog-reset read-mac` (LESSONS, round 9) |
| `WS_P4_7B` | COM6 (native USB) | needs two USB cables or it browns out on boot |
| `CYD_P4_4880` | COM19 (native USB) | LiPo fitted; browned out on the PC's USB alone. **Still on v0.2.9-era firmware**; the owner wants `v0.2.19` on it |
| `WS_S3_4B` | COM8 (CH343 `5A67166678`) | **locked up** (#69); compile it as the S3 check, never flash it until the owner says |
| `CYD_S3_3248` | - | USB broken (2026-09-28) |
| `CYD_S3_8048`, `WS_S3_5B` | - | not plugged in |

The P4_5, the 4B and the 1060 run 2.11 round 16 (the `v0.2.19` code, built before the tag); the rest
`v0.2.18` or older. Their dashboards are as the owner left them after his final pass: the P4_5's
House pinned 5 x 4, the 4B's 4 x 5 with Thermo hidden, the 1060's House 5 x 5 on Linen; window close
off on all three. Settings copies from every flash: `bench/settings/<board>/2026*.json`. Hosts:
`fleet-ws-p4-5`, `fleet-ws-p4-4b`, `fleet-cyd-p4-1060` (all in `HTTP_REFERENCE.md`). Environments:
`WS_P4_TOUCH_LCD_5`, `WS_P4_TOUCH_LCD_4B`, `CYD_P4_1060P470`, `CYD_P4_4880P443`. Ports move when
boards are re-plugged: `pio device list` and match the serial. **Is a board alive?** Ask Home
Assistant (`sensor.<device>_uptime`) or `GET /settings?stats=1`, not the cable. While developing,
build and flash the P4_5, the 4B and the 1060; all nine compile once, right before a merge. **Away
from the desk** the owner reflashes the P4_5 from the laptop (COM6); it is then at `http://10.0.0.2`.
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
- **The repo's PowerShell scripts run from the PowerShell tool** (`& .\scripts\release\save_settings.ps1
  <host> <env>`); `powershell -File` from Git Bash is refused by the execution policy.
- **Windows PowerShell 5.1 makes a native program's stderr fatal under `$ErrorActionPreference =
  "Stop"`** - `gate.ps1` handles it; a new script that runs `pio` must too.
- **`pio run` with no `-e` builds the 7B only.** The all-nine gate: `powershell -ExecutionPolicy
  Bypass -File scripts/release/gate.ps1` (~15 min, in the background); it refuses a dirty tree.
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
- **A kept arranger answer outlives a search change** unless its version moves (`v4` now): bump it
  whenever the search's shapes or costs change. **Time a search change on the 4B, unpinned** - it is
  the slowest board, and a pinned page does not run the unpinned search at all (LESSONS).
- **A task that never blocks runs at `tskIDLE_PRIORITY`** (or yields every second) - above it, the
  idle task starves and the task watchdog reboots the chip after 5 s. A boot loop: read the serial
  first (`task_wdt: ... CPU 0: <task>` names the culprit).
- **LVGL 9.5 clips an object's `DRAW_POST` to its own area** - anything drawn outside an object goes
  in `DRAW_MAIN_END` (LESSONS). An image meant to continue LVGL's drawing must blend in 16 bits.
- **Something that appears inside an open panel: animate it, not the panel** (`Kit::drawerReveal()`,
  LESSONS); **never add children to a kit component** - some find their parts by child index.
- **A screenshot is not the glass**: `/screenshot` renders the object tree afresh, so it can show
  what the panel does not. `/screenshot?fb=1` is the panel's own frame buffer (portrait on the P4_5).
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
  file and use `git commit -F` / `--body-file`. Commit and push in separate calls; with `main`
  checked out, push another branch as `git push origin <b>:<b>`.
- **Verify from outside the device** - the router, the broker, HA, `/bench?what=verify`, the harness.

## How to work with this owner

A hobbyist and ESP32 enthusiast, not a professional developer, and explicit about that - but reads
code, spots real bugs, and has caught several that were not obvious. **Treat the owner's instincts
as data** - #99's rail, 2.8's four slots, 2.11's arranger rules and its last change (a cluster's
window is its Members, K129) were the owner's. A little colour-blind: never distinguish two states
by colour alone.

**What works:**
- **Show, don't spec.** Build something to react to (a mock artifact, the Layout Playground); the
  owner flashes fast and tests thoroughly against a sheet - `docs/TEST_<topic>.md`, numbered tests
  with PASS criteria, the owner's results under them. Rounds go on one branch; the all-nine gate and
  the PR come once, at sign-off.
- **A PR only after the owner's sign-off**: push the branch, the owner tests and answers, then
  `gh pr create`; the owner merges.
- **Snappy and responsive first, eye candy second** (W6); **motion that jerks is a bug to him** -
  round 15b existed because a panel jumped.
- **While the owner is testing, give ONE report when he says the testing is done.**
- **Read the tool, not the name**; **make the case for any install first** - the owner decides.
- **Plain language, not metaphor.** State what things are; avoid "X, not Y" framings. Panel text in
  US English ("Color").
- **Never assume. Give the top 2-3 options with a recommendation, and ask** before building on an
  assumption; discuss a structural choice a turn before implementing it.
- **Own mistakes plainly**; **say plainly what is unexplained**; **say what to expect** before
  anything that changes the screen; say what was not tested on the glass.
- **Unattended work is welcome** when the brief is clear. Commit and push at the end of each step.
- **Docs when there is something to record**: DECISIONS after a settled discussion, design docs
  when something is decided, LESSONS as learned, ROADMAP as achievements are checked off, the test
  sheet each round. **HANDOFF only at a handoff** - rewritten then, with the lines of code recounted.
- **Recommend a handoff as context nears half** - before starting something big, or at a good
  stopping point mid-task - and prepare it (docs, this file, the next session's brief).

**What to avoid:** committing straight to `main` (the guard refuses it anyway); guessing a fourth
time - instrument it or ask the far end; claiming a script worked because it printed something.

**Real lights (W9):** any Office light may be commanded while testing, put back as found; ask
before the Kitchen. (The Office Overhead is unavailable in HA until the Tuya fix.)

**Versioning (W3):** `v0.<phase>.<release>`; the release counts signed-off merges to `main` that
change the firmware (a docs-only merge is not tagged). **`v0.2.19` is 2.11; the next is `v0.2.20`.**
