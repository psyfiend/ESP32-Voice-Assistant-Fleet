# Fleet Dashboard — Roadmap

**The plan: what we are building, in what order, and what is done.** One row per milestone, saying
DONE or in progress and where its detail lives. Open work left behind by a signed-off milestone is in
its phase's summary at the end of the phase, tagged by the milestone it came from. The roadmap as it
grew before this form - the original blueprint and every early milestone's history - is
`docs/archive/ROADMAP_to_2026-10-06.md`; section numbers quoted in older code and documents
("ROADMAP 4.1", "ROADMAP Q9") refer to it.

## Where things go

| A piece of information | Goes in |
|---|---|
| Where the project stands: current goals, decisions, findings | `docs/HANDOFF.md` |
| The plan and each milestone's status | here |
| The milestone in progress: goal, deliverables, working decisions, parking lot, what is done | `docs/PLAN_<milestone>.md`, archived at sign-off |
| A test of something ready on the glass, and its results | `docs/TEST_<milestone>.md`, archived at sign-off |
| A settled decision | `docs/DECISIONS.md`, added at sign-off - one line, pointing at its reasoning |
| The reasoning and design behind a feature | `docs/design/<area>.md` |
| An open question or a note for a future milestone | that milestone's **GitHub issue** |
| Open work a signed-off milestone left | its phase's summary, here |
| A mistake worth not repeating | `docs/LESSONS.md` |
| What a version contains | `CHANGELOG.md` |
| How the code works today (stable) | `CLAUDE.md` and the design docs |

## How a milestone runs

1. **Plan first** (`PLAN_<milestone>.md`, the first two or three turns, before code): the goal in
   one sentence; at most five deliverables, each with what "done" means; what is out of scope; open
   questions; working decisions, which may change; and a **parking lot**. A new idea goes to the
   parking lot unless the owner moves it into scope, and then something else comes out.
2. **Build in rounds.** The test sheet starts when a deliverable is ready on the glass; a round with
   nothing to test adds nothing to it. The dev boards are built each round; all nine once, right
   before the merge (W5).
3. **Sign off**: the settled decisions go to DECISIONS; the parking lot is sorted into this file -
   the phase's summary, a milestone of its own, "Other open work", or an issue; the milestone's row
   says DONE; CHANGELOG gets its entry; the plan and test sheet are archived; docs and code are
   scrubbed of anything stale. DECISIONS W14.

## Phase 0 — Repo hygiene: DONE

Submodules fixed (LVGL v9.5.0, the Arduino_GFX fork with `upstream`), bb_captouch vendored, the
platform pinned, version plumbing, the clone test, the issue tracker. Open: **#3** review `lv_conf.h`
against 9.5's template (with #88's 9.6 evaluation).

## Phase 1 — Connectivity: DONE

Hostnames, the connectivity state machine, APSTA, the AP, the header glyph, MQTT, the entity
registry, HA discovery. Open, each with its issue: **#6** captive portal (needs Phase 4's server),
**#7** on-device connectivity screen, **#39** `_proven` fingerprint, **#42** AP_ACTIVE after idle,
**#47** discovery payload size, **#48** reason 36.

## Phase 2 — UI foundation: IN PROGRESS

| # | Milestone | Status |
|---|---|---|
| 2.1 | Startup reorganisation | DONE - `design/startup.md` |
| 2.2 | Design system (tokens) | DONE - `design/tokens.md` |
| 2.3 | Memory budget spike | DONE (its PSRAM finding replaced by DECISIONS A11) |
| 2.4 | Card base class | DONE - `design/card-layout.md` |
| 2.5 | Page + grid engine | DONE, `v0.2.5` - `design/dashboard.md` |
| 2.6 | Pages and swipes | DONE - `design/pages.md` |
| 2.7 | First card types | DONE, `v0.2.7` - `design/cards.md` |
| 2.8 | Header bar v2 (#19) | DONE, `v0.2.18` - `design/header-slots.md` |
| 2.9 | Display stack: esp_lcd (#67) | in progress - `display/README.md` |
| 2.10 | Card popup (#65) | DONE, `v0.2.8`-`v0.2.12` - `design/card-sheet.md` |
| 2.11 | Group cards (#66) | DONE, `v0.2.19`, follow-ups `v0.2.20` - `design/group-cards.md`, `design/layout-engines.md` |
| 2.12 | UI foundation - the refactor | DONE, `v0.2.13`, follow-ups `v0.2.15` - `design/ui-architecture.md` |
| 2.13 | Add cards from HA (#116) | DONE, `v0.2.21` - `design/ha-cards.md` |
| 2.14 | Alerts on the claim model (#76) | not started |
| 2.15 | Bedroom alarm clock (#35, #100, #74, #75) | not started |
| 2.16 | Backup and Restore (DECISIONS K181, K187) | not started |

### Phase 2 summary: open work from its milestones

Each item names the milestone it came from. Detail behind the 2.13 items - what was said, decided
and still open - is `design/carried-from-2.13.md`, by section.

**From 2.9**
- The last two boards onto esp_lcd: CYD_S3_8048 and WS_S3_5B, when the hardware is on the desk (#67).

**From 2.12**
- Merge the near-equal shape tokens, with before and after pictures (K72, `design/tokens.md` §7).

**From 2.13 - its Home Assistant side (the work 2.14 depends on)**
- HA's logical and status layers as hand-written YAML by function (K136), the duplicate door
  entities, `delay_on` and `last_changed` resetting on an HA restart; per-area roll-up entities.
  Carried §7.
- HA's milestone log `projects/2026-10_fleet-2.13/` brought up to date and the tag
  `fleet-2.13-done` (K135). Carried §7.
- HA group entities as groups on the board, and the mirror (K170, K184, K186); which entities
  count, when the board group is made, what happens when HA's member list changes. Carried §1.
- Summary cards as their own kind of card, and groups of sensors (K182, K189); Garage - Doors stays
  compiled until then. Carried §3.
- Checks that change HA, each with the owner: "N new" with an HA label added and removed; a group's
  all-off with no HA light group for the room; `unknown` after an HA restart (#84). Carried §6.

**From 2.13 - on the boards**
- Units as a UI Tweaks choice (K177) and precision from HA. Carried §6.
- The tap cycle for a group's existing members (header step, remove). Carried §6.
- Two levels of nesting (K147), needed by HA groups as board groups. Carried §6.
- New group from the page's own cards. Carried §6.
- The text fitter beyond Edit cards (K152). Carried §6.
- Panel's wide values at tile width. Carried §6.
- The arranger when the system header comes back (K167), to confirm on the glass. Carried §6.
- The Active state row in a member's own view starts from the held card's value (a bug). Carried §6.
- `/popup?dash=2` wipes the House page without asking, and `CardAdded::resetToCompiled()` resets
  at most 64 cards; both go when Backup and Restore replace Reset (2.16), with the Reset wiring
  `Panel_System` still carries (`askDashReset`, `confirmDashReset`, `dashAskCb`). Carried §4.
- Code tidy found by the sign-off scrub (strings, so not changed there): `Fleet_Display.cpp`'s
  `#error` text ("2.9 step 5 adds QSPI"), `AudioManager.cpp`'s "no longer matches" warnings,
  `DeviceIdentity.cpp`'s "fleet-dev" fallback message (the slug is "dev"); `HiddenSheet` and
  `SlotPanels`' `viewHidden` reachable only from `/popup`; the arranger test generator
  (`host-tests/tools/`) and `playground.html` keep their version notes.
- `docs/display/test-log.md` is a dated log of test sheets; decide which sheets still earn their
  place.
- `Popup/PopupCluster.cpp` is over 1,000 lines: move Edit cards' save and dialogs into a file of
  their own. Carried §6.
- The golden harness (`scripts/ui_golden.py`) re-run against the made-card dashboard. Carried §6.

**From 2.13 - words and looks to confirm on the glass**
- "Built in" on Panel's row in Edit cards; Card margins' four steps; where Card UI's Card state row
  sits; whether UI Tweaks' "Group view" becomes a section; every on-screen "group" re-read against
  K146 / K158; a humidity's decimal, the 4B's °F, long names in tiles; the 4B's slow HTTP after a
  restore. Carried §8.

## Phase 3 — Build sheet

The build sheet is what the companion web app (#87) produces and the device reads, and the backup
format - not something a user edits (D-8). The settings file is already the runtime schema, so
Phase 3 is exporting and importing it, scoped with #87 (K131).

| # | Milestone | Status |
|---|---|---|
| 3.1 | Schema v1 (#20) | not started |
| 3.2 | Compile-time loader (#21) | not started |
| 3.3 | Runtime JSON loader (#22) | not started |

## Phase 4 — Settings, navigation, web

| # | Milestone | Status |
|---|---|---|
| 4.1 | Standard settings pages (#23) | not started |
| 4.2 | Live layout settings (#24) | not started |
| 4.3 | Navbar (#25) | not started |
| 4.4 | Context panels (#26) | not started - `design/context-panels.md` |
| 4.5 | Web config page (#27) | not started - part of the companion app (#87) |

## Phase 5 — OTA

| # | Milestone | Status |
|---|---|---|
| 5.1 | Partition layout (#28) | not started - planned inside 2.15 (K131) |
| 5.2 | OTA with rollback (#29) | not started - planned inside 2.15 (K131) |
| 5.3 | Fleet build script (#30) | not started |

## Phase 6 — Expansion

**#31** card library (with #86 control styles, #77 media, and the thermostat's controls - K144; the
thermostat could also be a one-off build of its own), **#32** WS_S3_4B onboard hardware, **#33**
RS485/Modbus, **#34** weather station, **#35** alarm clock (in 2.15).

## Phase 7 — Voice and the ESPHome API

DECISIONS A18; design notes `design/voice-esphome.md`.

| # | Milestone | Status |
|---|---|---|
| 7.1 | ESPHome native API on the boards - HA adopts a board as an ESPHome device; optional, beside MQTT | not started |
| 7.2 | Assist satellite - wake word on the board, audio to HA's Assist pipeline | not started; needs 7.1 |

## Running order

The numbers above are identity, not sequence. The order agreed after 2.11 (K131) was 2.13, 2.15,
2.14, 4.3, then Phase 3. **It is to be reviewed in a session of its own** that breaks down the
remaining roadmap and issues into milestones small enough to keep to (owner, at 2.13's sign-off):
2.14 depends on the HA work listed in the Phase 2 summary, and 2.16 has no place yet.

Alongside, when the hardware is on the desk: #67's last two boards; #72 battery; #69 4B panel timing
(WS_S3_4B locked up); #70 S3 LVGL speed; #88 LVGL 9.6.

## Other open work, by theme

- **Looks**: #73 type ladder, #80 tag styles, #82 per-panel colour, #83 custom icon packs, #85 colour
  borders and a custom scheme page, #79 page transitions; History's activity rows in satin once an S3
  is measured (K86).
- **Behaviour**: #74 clock (RTC fallback), #75 night mode, #84 "no data yet", #71 rotation, #104 a
  light's temperature mark.
- **Cards**: a card's place history, so Restore can go one move back (K198, #118); #89 scene as
  secondary info; #90 window title; #91 P4_5 flash; #92 chart options.
- **Layout**: #52 auto-sort (largely answered by the arranger and groups from HA areas - a candidate
  to close); arranging by hand.
- **Fleet**: #55 peer discovery, #36 distribution `platformio.ini`, #37 `library.json` manifests, #40
  GFX fork diff; boards writing to HA - their groups mirrored, HA groups edited from a board (K180,
  with #87 and Phase 7). Carried §2.
- **Side projects**: #100 clocks (K81-K84).
- **Ideas not yet scheduled** (the owner's): HA roll-up entities per area, live member reports for
  board groups, card presentations and sizes, arranging by hand, synchronizing boards -
  `FUTURE_IMPROVEMENTS.md`, "Brainstorm".
