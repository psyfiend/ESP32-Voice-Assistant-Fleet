# Decisions

**Every decision that shapes later work, one line each, with its date and where the reasoning lives.**
Started 2026-10-06 from the roadmap's Q1-Q11, the design interview and the milestones' sign-offs, so
a decision is found in one place instead of being reconstructed from five.

How to use it:
- **Before building anything a decision touches**, read its line and the document it points to.
- **A new decision** (the owner's, or a recommendation the owner accepted) gets a line here in the
  same session, under its area. The reasoning stays in the design doc, test sheet or issue it came
  from; this file only points.
- **A changed decision** keeps its line, struck through or marked SUPERSEDED, with the new one
  beside it. The history is the point.
- **An open question is not a decision.** It lives in the issue for the milestone it blocks, or in
  `docs/design/interview-phase2-3.md` if it needs the owner's interview.

"Archive §n" means `docs/archive/ROADMAP_to_2026-10-06.md` section n.

## Working together

| # | Decision | Date | Where |
|---|---|---|---|
| W1 | Branch every piece of work off `main`, merge it when done; never stack branches | 2026-09-29 | HANDOFF |
| W2 | Nothing reaches `main` without the all-nine compile gate and a look on glass; no quick-fix branches during the interview (mocks instead); demos on one board | 2026-10-01 | HANDOFF |
| W3 | Versions `v0.<phase>.<release>`: the release number counts signed-off, tagged merges to `main`, not milestones | 2026-10-05 | CHANGELOG, HANDOFF |
| W4 | Prefer ESP-IDF facilities to Arduino-only ones; an IDF migration is expected | 2026-09-24 | CLAUDE.md |
| W5 | Dev boards while building: WS_P4_5 and WS_S3_4B; WS_P4_5 alone while the owner tests; all nine before a merge | 2026-09-28 / 10-04 | HANDOFF |
| W6 | Snappy and responsive first, eye candy second | 2026-10-04 | `card-sheet.md` §14, LESSONS |
| W7 | Recommend one option, ask before building on an assumption, discuss a structural choice a turn before building it; plain language | standing | HANDOFF |
| W8 | Files are written with Edit/Write, never piped through a shell | 2026-09-18 | CLAUDE.md |
| W9 | Real lights while testing: any Office light may be commanded and is put back as found; ask before anything in the Kitchen | 2026-10-06 | HANDOFF |

## Platform and architecture

| # | Decision | Date | Where |
|---|---|---|---|
| A1 | One codebase, nine boards, the board picked by `-D BSP_HEADER`; seven flat BSP structs | 2026-09-03 | CLAUDE.md |
| A2 | LVGL a submodule at a release tag (v9.5.0), no local changes; Arduino_GFX a fork with an `upstream` remote; bb_captouch vendored | 2026-09-03 | Archive §7 Phase 0 |
| A3 | The entity registry is the keystone: providers write it on their own tasks and never touch LVGL; the LVGL thread drains it | 2026-09-03 | Archive §4.1-4.2, `EntityRegistry.h` |
| A4 | Optimistic commands: applied at once, confirmed only by a matching echo, reverted and FAILED after 3 s (#63); a light's levels the same way | 2026-09-07 / 10-05 | `EntityRegistry.h`, `card-sheet.md` §15 |
| A5 | Startup is a five-way split; `SystemCore` and `SystemReport` include no LVGL; UI registers with lower layers, never the reverse | 2026-09-09 | `docs/design/startup.md` |
| A6 | Libraries are zero-dependency where they can be; Ethernet belongs inside `Fleet_Connectivity` (Q9) | 2026-09-03 | Archive §8 Q9 |
| A7 | One HTTP server, `esp_http_server`, owned by `SystemCore`; handlers never touch LVGL | 2026-09-24 | CLAUDE.md |
| A8 | HA entities over HA's websocket, not MQTT (#43); MQTT for our own entities and Zigbee2MQTT | 2026-09-22 | `docs/design/ha-websocket.md` |
| A9 | Display stack: raw `esp_lcd` replacing Arduino_GFX board by board (2.9, #67); one present mode per bus | 2026-09-25 | `docs/display/README.md` |
| A10 | The P4 framework libraries are rebuilt for esp-hosted-mcu#243 (#49) | 2026-09-22 | `docs/REBUILD_P4_LIBS.md` |
| A11 | **LVGL's pool: 512 KB in PSRAM on every P4** (`FLEET_LV_MEM_PSRAM`); LVGL's own allocator kept, never the system `malloc`. Supersedes 2.3's "PSRAM pool not needed" | 2026-10-06 | #88, CLAUDE.md, LESSONS |
| A12 | Voice assistant parked, not abandoned: audio code is protected during the UI work (Q8) | 2026-09-03 | Archive §8 Q8 |
| A13 | The entity table learns entities while running: the members a source names for a group (HA's `entity_id`), with ids made from the source's own (`ha_light_office_lamp`), so pauses and settings find them after a reboot | 2026-10-06 | `card-sheet.md` §16, `EntityRegistry.h` |

## Connectivity and Home Assistant

| # | Decision | Date | Where |
|---|---|---|---|
| C1 | Four connectivity modes, `STA_WITH_AP_FALLBACK` the default (Q1) | 2026-09-03 | Archive §8 Q1 |
| C2 | HA naming: `device_id = fleet_<board>_<last6 of MAC>`, `unique_id = <device_id>_<object_id>` (Q5) | 2026-09-07 | Archive §8 Q5 |
| C3 | One HA device per board, no fleet parent; device-based discovery (Q6) | 2026-09-07 | Archive §8 Q6 |
| C4 | Works with stock HA is the baseline; an HA-side companion (as HomeTiles' Bridge) is a separate, later choice. HA's `recorder/statistics_during_period` serves history (2.10e) | 2026-10-06 | #87, HANDOFF |
| C5 | HA light levels go as `light.turn_on` with `brightness` (0-255), `color_temp_kelvin` or `hs_color`; HA's `success: false` fails a command at once, `success: true` still waits for the report | 2026-10-06 | `ha-websocket.md` §9 |

## Dashboards, pages and the build sheet

| # | Decision | Date | Where |
|---|---|---|---|
| D-1 | Dashboard > Page > Grid > Card > Entity; "page", not "screen" (Q7) | 2026-09-03 | Archive §8 Q7 |
| D-2 | The build sheet: nested plain structs as the model; authored by hand AND from a UI, three-layer precedence plus hierarchical locks (Q2, Q3) | 2026-09-03/04 | Archive §8 Q2-Q3 |
| D-3 | Every tunable default is build-sheet overridable: each `*Defaults` struct is a schema, a sheet is overrides (Q3c) | 2026-09-04 | Archive §8 Q3c |
| D-4 | Sub-cell grid units; the validator reports, never resolves; nothing spills to another page (Q3b). **Reopened**: an arranger fills the page by default, explicit placement becomes a pin (#78) | 2026-09-03 / 10-01 | Archive §8 Q3b, `look-and-feel.md` §3.8 |
| D-5 | Works on first flash with no build sheet; per-device sheets for the owner (Q4) | 2026-09-03 | Archive §8 Q4 |
| D-6 | Per-board default grids: CYD_S3_3248 2x4; the two 4Bs 3x4; 7B and 1060 6x3; P4_5 5x3. Both 4Bs need a smaller type scale | 2026-09-17/18 | Archive §7 Phase 5, `dashboard.md` §6 |
| D-7 | The settings model: precedence device < page < area/group < card; inheritance per setting, the source named, a Reset; "Detach"; LOCKED keeps Q2's meaning; label types tag / float / band / folder / none | 2026-10-02 | `card-sheet.md` §11.3 |
| D-8 | **A companion web app with a true preview sends the config to the device**; the build sheet becomes its output and backup format, not the user's interface. Presentation (group/room cards) is how the project stands apart | 2026-10-06 | #87 |
| D-9 | Pages are rebuilt on arrival (one page's cards in memory at a time); swipes wrap around | 2026-09-24 | `docs/design/pages.md` |

## Cards and the popup

| # | Decision | Date | Where |
|---|---|---|---|
| K1 | Card types by HA domain; layouts (Value/State) are not types | 2026-09-22 | `cards.md`, LESSONS |
| K2 | Design tokens: no colour or size literals in UI code; UI scale from real PPI | 2026-09-10 / 09-22 | CLAUDE.md, `tokens.md` |
| K3 | Tap stays a one-touch toggle; long press opens the popup on every card - uniform navigation | 2026-09-22 / 10-02 | `card-sheet.md` §11.2 |
| K4 | ~~D1: the popup grows from the card~~ SUPERSEDED: it appears complete at once and closes at once - no grow, no dim, no leap | 2026-10-02 → 10-04 | `card-sheet.md` §14, LESSONS |
| K5 | D2: tabs for the entity's own features, deck panels for dashboard settings; SETTINGS always in the right half | 2026-10-02 / 10-05 | `card-sheet.md` §11.1, §14 |
| K6 | D3: a window opens on its control; Power in the selector, no header toggle | 2026-10-02 | `card-sheet.md` §11.1 |
| K7 | D4: members behind their own icon; a member row opens that member's controls in the same window | 2026-10-02 / 10-05 | `card-sheet.md` §11.1, §15 |
| K8 | D5: card settings saved by stable card id, written on close (2.10d) | 2026-10-01 | `card-sheet.md` §11 |
| K9 | D6: closes four ways - X, tap outside, drag down on the header row, 60 s | 2026-10-02 | `card-sheet.md` §13 |
| K10 | D7: colour by hue strip and eight swatches (LVGL has no colour wheel) | 2026-10-02 | `card-sheet.md` §11.1 |
| K11 | The popup is up to 2:1 and at most 100 mm wide with side gaps; its contents are the same share of it as on the P4_5; nothing moves within one window | 2026-10-05/06 | `card-sheet.md` §15 |
| K12 | A light's commands go every 300 ms while dragging and once on release; kept for HA lights after measuring the owner's Hue bridge | 2026-10-05 / 10-06 | `card-sheet.md` §15, `ha-websocket.md` §9 |
| K13 | Groups act as HA's light group: union of capabilities, mean levels of members that are on, "On when: any / all" (`GroupOn`), HA's tap rule | 2026-10-05 | `card-sheet.md` §15 |
| K14 | A paused member is out of its group; a paused window is greyed with a PAUSED pill; Paused is a switch | 2026-10-05/06 | `card-sheet.md` §15 |
| K15 | Scenes: a selector target plus a per-card "load / cycle scene" tap behaviour (recommended, not objected to). **Confirmed 2026-10-06: the selector target is built as 2.10c's last step**; a scene is only ever loaded (`scene.turn_on`), never shown as on | 2026-10-02 / 10-06 | `card-sheet.md` §11.2, #65 |
| K16 | Card control styles (Automatic / Dimmer / Switch / Button) are card settings and variants, for the card-library work | 2026-10-06 | #86 |
| K17 | A group defined in HA or Hue (`light.office`) gets the Members view like a group defined on the device: each member can be opened and controlled. Adding a member as a card of its own: later | 2026-10-06 | #65 |
| K18 | Such a group is commanded as itself (the bridge keeps its bulbs in step) unless a member is paused - then through its other members one by one, so K14 holds. Pausing the group pauses its members. "On when" is the source's to decide | 2026-10-06 | `card-sheet.md` §16 |
| K19 | Scenes: HA's scene entities on the light's own device, learnt by one template per session (stock HA); ~~the selector's fifth button; their buttons over the slider's place; a narrow window stacks its selector~~ SUPERSEDED after round 3: a view of its own, its icon under the chart, the brightness slider kept and the buttons in the column. A tap loads one and rings it while the light stays as the scene left it; never "on" | 2026-10-06 | `card-sheet.md` §16 |
| K20 | The window's corner icons are tabs: History, Members and Scenes are views, shown on every view they belong to, lit when showing, a tap on a lit one returns to the controls. History is the group's from its views, a member's from its own. ~~The X closes from every view~~ Back is hard-linked: the X on the controls only; the arrow elsewhere, always to one fixed view (History/Members/Scenes -> controls, a member -> Members, a member's History -> the member); the title's first part links up | 2026-10-06 | `card-sheet.md` §16 |
| K21 | Scenes hidden in HA's own UI (entity registry `hidden_by`, read by HA's `is_hidden_entity`) are hidden on the panel too; a card setting "Scenes: Visible / All / Off", Visible by default | 2026-10-07 | `card-sheet.md` §16 |
| K22 | The window's parts have names: window, title (breadcrumbs), chips, hero, control deck, switch, selector, SETTINGS panel | 2026-10-07 | `card-sheet.md` "Words" |
| K23 | The control deck sits under the hero where the window has the height (4B, 7"), beside it on the P4_5; ~~Scenes, and Colour under a tall window, push the slider aside~~ (K26). The slider may move between controls; the control deck never does (refines K11's "nothing moves") | 2026-10-07 | `card-sheet.md` §16 |
| K24 | The control deck after TouchFLO 3D: a short ribbon, a taller selector that can be dragged (its icon switches past halfway, snaps on release) and glides on a tap; the chosen icon a size up in the accent; Power a plain button; colours from the scheme; lit corner chips match the selector | 2026-10-07 | `card-sheet.md` §16, #25 |
| K25 | The SETTINGS panel: as wide as its longest row (right edge on the window's, never narrower than its tab), never taller than the window (scrolls), dropdowns for choices, a checkbox for Paused; the owner's row list, the unready rows greyed until 2.10d | 2026-10-07 | `card-sheet.md` §16 |
| K26 | In Scenes, and Color under a tall window, the slider stays where the controls put it; the content sits beside it. Only what does not fit moves anything: too wide, the slider goes left by the difference; under the clapperboard, the content drops to the slider's bottom first, then the slider goes left, then the scenes take fewer columns and scroll | 2026-10-07 | `card-sheet.md` §16 |
| K27 | Four control-deck looks, kept: Black - Square, Black - Round, Silver - Square, Silver - Round; Black - Square on Midnight and Fleet, Silver - Round on Linen (owner, round 9, settling rounds 5 and 6). The selector and lit chips are hand-drawn metal (a light/dark line curving up in the middle, brushed, dithered for the 16-bit panels), unlit chips soft dents; the selector's and lit chips' icons larger | 2026-10-07 | `card-sheet.md` §16 |
| K28 | A second panel, CHART: SETTINGS' mirror image (the same folder builder), its tab in the left half a millimetre from SETTINGS', up only while History shows (demo rows until the chart has settings); either panel opening folds the other; it leaves with the window as SETTINGS does (folded: slides down; open: at once). The panels open and fold in 260 ms (from 220) | 2026-10-07 | `card-sheet.md` §16 |
| K29 | The title is centred when "Area > Name" fits; else it takes the room beside the X, left of centre; else the name alone (the back arrow still goes up) | 2026-10-07 | `card-sheet.md` §16 |
| K30 | Text on the panels is US English ("Color"); code and docs keep their own spelling | 2026-10-07 | - |
| K31 | The selector's look follows the scheme: chosen per scheme (SETTINGS' row is "Selector"), each scheme's default until one is chosen; schemes are per page, so the look is too | 2026-10-07 | `card-sheet.md` §16 |
| K32 | A group defined at the source is paused exactly when all its members are - the registry keeps the group's own flag in step both ways (K18 is the other direction) | 2026-10-07 | `card-sheet.md` §16 |
