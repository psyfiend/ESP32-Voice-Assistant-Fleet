# Fleet Dashboard — Roadmap

**The plan: what we are building, in what order, and what is done.** One line per milestone and a
pointer to where its detail lives. Rewritten lean on 2026-10-06; the roadmap as it grew - the
2026-09-03 blueprint, every milestone's history, the Q1-Q11 reasoning - is
`docs/archive/ROADMAP_to_2026-10-06.md`, unchanged.

**Quoted section numbers** - "ROADMAP 4.1", "ROADMAP §4.2", "ROADMAP Q9", "section 3.3" in code
comments and older documents - refer to that archived copy, where they still find their text.
`docs/DECISIONS.md` indexes the Q1-Q11 decisions.

## Where things go

So that no document has to be reconciled with five others:

| A piece of information | Goes in |
|---|---|
| Where we are, what is next, what will bite you | `docs/HANDOFF.md` |
| The plan and each milestone's status (one line) | here |
| A decision (the owner's, or an accepted recommendation) | `docs/DECISIONS.md` - one line, pointing at its reasoning |
| The reasoning and design behind a feature | `docs/design/<area>.md` |
| An open question, or a note for a future milestone | that milestone's **GitHub issue** (or the design interview, if the owner must answer it) |
| A test round, its results, its measurements | `docs/TEST_<milestone>.md`, archived when the milestone is done |
| A mistake worth not repeating | `docs/LESSONS.md` |
| What a version contains | `CHANGELOG.md` |
| How the code works today (stable) | `CLAUDE.md` and the design docs |

A milestone's row here changes when its status changes - nothing else is written in this file.

## Phase 0 — Repo hygiene: DONE 2026-09-03

Submodules fixed (LVGL v9.5.0, the Arduino_GFX fork with `upstream`), bb_captouch vendored, the
platform pinned, version plumbing, the clone test, the issue tracker. Left: **#3** review `lv_conf.h`
against 9.5's template (with #88's 9.6 evaluation).

## Phase 1 — Connectivity: DONE 2026-09-08

Hostnames, the connectivity state machine, APSTA, the AP, the header glyph, MQTT, the entity
registry, HA discovery - verified on every board. Left behind on purpose, each with its issue: **#6**
captive portal (needs Phase 4's server), **#7** on-device connectivity screen, **#39** `_proven`
fingerprint, **#42** AP_ACTIVE after idle, **#44** outbound commands (MQTT and HA on/off work; HA light
levels arrive with 2.10c), **#47** discovery payload size, **#48** reason 36.

## Phase 2 — UI foundation: IN PROGRESS

| # | Milestone | Status |
|---|---|---|
| 2.1 | Startup reorganisation | DONE 2026-09-09 - `design/startup.md` |
| 2.2 | Design system (tokens) | DONE 2026-09-10 - `design/tokens.md` |
| 2.3 | Memory budget spike | DONE 2026-09-10; its "no PSRAM pool needed" superseded by DECISIONS A11 |
| 2.4 | Card base class | DONE 2026-09-15 - `design/card-layout.md` |
| 2.5 | Page + grid engine | DONE 2026-09-18, `v0.2.5` - `design/dashboard.md` |
| 2.6 | Pages and swipes | DONE 2026-09-24 - `design/pages.md` |
| 2.7 | First card types | DONE 2026-09-24, `v0.2.7` - `design/cards.md` §13 |
| 2.8 | Header bar v2 (**#19**) | **DONE 2026-10-17, `v0.2.18`** - `design/header-slots.md`, D-11 to D-33, `archive/TEST_2.8.md` (seven rounds). Three places on claims (the clock and alarm left, the page middle, status right), each slot with its own panel, one shape with its tab; glyphs drawn from numbers; a page's header shown / hidden; the system panel's new layout; **UI Tweaks** and a card's **Card UI** as chip tables; the cards' **active frames** - Corner cut the default - and lit glass; Linen's "on" terracotta `D47F45` |
| 2.9 | Display stack: esp_lcd (**#67**) | six of nine boards, merged 2026-09-29; left: CYD_S3_8048, WS_S3_5B - `display/README.md` |
| 2.10 | Card popup (**#65**) | **DONE 2026-10-12.** 2.10a `v0.2.8`; 2.10b `v0.2.9`; 2.10c `v0.2.10` (HA light levels, members of HA groups, scenes, the window redesigned - `design/card-sheet.md` §14-16). **2.10d + 2.10e `v0.2.11`** (merged together, K51): stable ids and saving (§17), History from HA - charts and activity (§18). Sheets in `archive/`. Left over, filed: #90, #91, #92 (K17 done, `v0.2.12`) |
| 2.11 | Group cards (**#66**) | **DONE 2026-10-30, `v0.2.19`** - sixteen rounds, 54 builds, signed off by the owner (`archive/TEST_2.11.md`; design `design/group-cards.md` §6-7; K88-K129). Clusters and solo cards on the page, **the arranger by the owner's rules in units** (Playground v9, #78, K109, K120, K121; `design/layout-engines.md`), tiles as the mock's E, near square, one height a page (K116, K122, K124), cluster headers in one order (K114-K119), what the glass edits (Visibility, Copy / Move, hidden cards and Restore, the keyboard), **a cluster's window is its Members** with the CLUSTER panel (K129), the House page on HA's new names (K127). The group view and Add cards from HA are 2.13 |
| 2.13 | HA integration: Add cards from HA (**#66** follow-on) | **designing**, branch `feat/2.13-ha-cards` - designed with 2.14 and with Home Assistant's side (K130-K134; running order 6 below) |
| 2.14 | Alerts on the claim model (**#76**) | designed with 2.13 (K130), **built after 2.15**: its night form needs the night face |
| 2.15 | Bedroom alarm clock (**#35**, #100, #74, #75) | after 2.13 (K131) - the clock page and its alarms, night dimming, weather and temperature cards, and **OTA (5.1, 5.2) pulled forward** |
| 2.12 | UI foundation - the refactor | **DONE 2026-10-14, `v0.2.13`** - the card window rebuilt on a foundation (safety net, the split, `UIShape`, the component kit, the Sheet, `Layout::FlowEngine` with host tests, the structure check), nothing changed on screen. How it is built: `design/ui-architecture.md`; the record: `design/ui-foundation.md`. Follow-ups #96, #97 and more: **DONE, `v0.2.15`** (`archive/TEST_UI_FOLLOWUPS.md`) |

## Phase 3 — Build sheet

Reframed by DECISIONS D-8: the build sheet is what the companion web app (**#87**) produces and the
device reads, and the backup format - not something a user edits. **Shrunk and moved after 2.14 and
4.3 (K131)**: the settings file is already the runtime schema, so Phase 3 becomes exporting and
importing it, scoped with #87; 3.2 and 3.3 are re-scoped then.

| # | Milestone | Status |
|---|---|---|
| 3.1 | Schema v1 (**#20**) | not started - expresses the settings model (D-7) and stable card ids (2.10d) |
| 3.2 | Compile-time loader (**#21**) | not started |
| 3.3 | Runtime JSON loader (**#22**) | not started |

## Phase 4 — Settings, navigation, web

| # | Milestone | Status |
|---|---|---|
| 4.1 | Standard settings pages (**#23**) | not started; screen dimming pulled forward when it is needed daily; the settings that matter also as HA entities (FUTURE_IMPROVEMENTS, owner 2026-10-07); the windows' timeout (60 / 30 / 10 s / off) and countdown bar, device-wide - already system settings, set from a PC until this page (K79); perhaps a "low / no eye candy" setting for a plainer look (owner, 2026-10-14) |
| 4.2 | Live layout settings (**#24**) | not started |
| 4.3 | Navbar (**#25**) | not started |
| 4.4 | Context panels (**#26**) | not started - `design/context-panels.md` |
| 4.5 | Web config page (**#27**) | not started - now part of the companion app's scope (#87) |

## Phase 5 — OTA

| # | Milestone | Status |
|---|---|---|
| 5.1 | Partition layout (**#28**) | not started - **in 2.15** (K131). Two app slots mean a new partition table: one full flash per board, the settings carried across. Images boot into D-6's grids |
| 5.2 | OTA with rollback (**#29**) | not started - **in 2.15** (K131): the bedroom clock is the first board off the desk |
| 5.3 | Fleet build script (**#30**) | not started |

## Phase 6 — Expansion

**#31** card library (with #86 control styles, #77 media), **#32** WS_S3_4B onboard hardware, **#33**
RS485/Modbus, **#34** weather station (its cards come with 2.15), **#35** alarm clock (now 2.15).

## Running order — 2026-10-06 (items 1-5 done; the UI follow-ups inserted 2026-10-14; 2.11 before #76, 2026-10-17; 2.13 / 2.14 named 2026-10-30; rewritten after 2.11, 2026-10-30, K131)

The numbers above are identity, not sequence. Agreed with the owner:

1. ~~**2.10d + 2.10e, one merge**~~ - DONE, `v0.2.11`; ~~**K17**~~ (a group's member as a card of
   its own) - DONE, `v0.2.12`.
2. ~~**2.12 UI foundation - the refactor**~~ - DONE, `v0.2.13`.
3. ~~**The UI follow-ups from 2.12's test**~~ - DONE, `v0.2.15` (#96, #97, #23 and #92 in part, #98;
   `archive/TEST_UI_FOLLOWUPS.md`); #93 `v0.2.14`; ~~#99 satin~~ DONE, `v0.2.16` (K77, K85-K87;
   `archive/TEST_99.md`). #100 NINA's clock is **tabled** - three clocks agreed (K81-K84, #100),
   prototype on `feat/100-nina-clock`. #104 (a light's temperature mark) waits.
4. ~~**2.8 header slots** (#19)~~ - DONE, `v0.2.18` (`archive/TEST_2.8.md`, seven rounds; D-11 to
   D-33): three places on **claims** (D-19, a pure planner, host-tested), a panel per slot, the
   system panel's new layout, UI Tweaks and Card UI, the cards' active frames (Corner cut the
   default), lit glass, Linen's terracotta. ~~**A17**~~ DONE, `v0.2.17`
   (`archive/TEST_A17.md`).
5. ~~**2.11 group cards with the page arranger**~~ - DONE, `v0.2.19` (#66, #78;
   `archive/TEST_2.11.md`, sixteen rounds; design `design/group-cards.md` §6, K88-K129; the
   arranger's spec: artifact "Layout Playground"; engines: `design/layout-engines.md`). A cluster's
   long press opens its Members (K129).
5b. **The 2.11 follow-ups** (the owner's notes on `v0.2.19`, 2026-10-31; branch
   `feat/2.11-followups`, `TEST_2.11_FOLLOWUPS.md`), before 2.13 is built. Round 1 is on the dev
   boards: a swipe in a sheet never taps (signed off), one name for a member in Members and Header
   options, and the first Move this card. **Round 2 is being designed** (artifact "Card moves on the
   glass"): card management on the glass, all-in - a tile's Hide from the cluster / Add to dashboard
   / Change cluster, a solo card's Hide from this page / Change group / Add to cluster, a group
   picker with Custom for every card, and **Restore**, so every moved, copied or regrouped card has
   one way back to how it was built. The header glyph beside a member's row in Members. **No new
   words on a panel until the owner has agreed them.**
6. **2.13 - Add cards from HA, HA's organisation offered** (owner, 2026-10-29; HA's
   `FLEET_REFERENCE.md` and `HOUSE_RULES.md`), **designed together with 2.14 and with improvements
   on HA's side** (K130, K132-K136; `HANDOFF.md`, "Home Assistant"). **HA's side so far**: the
   milestone's log is the HA folder's `projects/2026-10_fleet-2.13/`, tagged `fleet-2.13-start`
   (K135); its first HA job is turning the generated logical and status YAML into hand-written
   files organised by function (K136). Open: per-room status entities for cluster headers, and the
   anatomy of an alert so HA's own Security dashboard works as fully as the boards (artifact "HA
   Side of 2.13").
   the group view (K118); Add cards from the entities labelled `fleet`, the linked area's first
   (K119, K123); a cluster linked to the HA area closest to its intent - or none - its header able
   to adopt that area's logical entities as members; an outdoor average from HA. The dashboard
   never has to follow HA's organisation, and needs no particular HA layout to work: the owner
   chooses when to take what HA has (K133). With it, from 2.11's final pass
   (`archive/TEST_2.11.md`, 2026-10-29): a header panel's temperature opening that sensor's
   History; a cluster's own card state for its members; **UI Tweaks' "Show helper text"**, hiding
   every helper note. 2.13 also decides what an entity with no card type yet becomes (the
   thermostat).
7. **2.15 - the bedroom alarm clock** (#35, #100 and K81-K84's clocks, #74 RTC fallback, #75 night
   mode): alarms that ring without HA, night dimming (from 4.1), the weather-station and
   temperature cards (#34), and **OTA - 5.1 and 5.2 pulled forward**, since this is the first board
   off the desk. The new partition table means one full flash per board with the settings carried
   across.
8. **2.14 - #76, HA alerts on the claim model** (the notice queue from NINA's toasts; mock first),
   in the system header, the cluster headers and the night face - designed with 2.13 (K130), built
   once 2.15's night face exists.
9. **4.3 navbar** (#25).
10. **Phase 3, shrunk** - exporting and importing the settings file, scoped with **the companion
    web app** (#87); 3.1's schema is what that file already is (K131).

Alongside, when the hardware is on the desk: #67's last two boards; #72 battery (fuel gauges
ordered); #69 4B panel timing (WS_S3_4B locked up, waiting to be troubleshot); #70 S3 LVGL speed;
#88 LVGL 9.6, after 2.10.

## Other open work, by theme

- **Looks**: #73 type ladder, #80 tag styles, #82 per-panel colour, #83 custom icon packs, #85 colour
  borders and a custom scheme page, #79 page transitions; History's activity rows in satin once an S3
  is measured (K86).
- **Side projects**: #100 clocks - small, NINA medium and NINA full (K81), the full one in 2.11's hero
  region; tabled 2026-10-14.
- **Behaviour**: #74 clock (RTC fallback), #75 night mode, #76 HA alerts, #84 "no data yet", #71
  rotation.
- **Layout**: #52 auto-sort, #53 auto-fill, #54 auto-adjust (all weighed against #78's arranger).
- **Fleet**: #55 peer discovery, #36 distribution `platformio.ini`, #37 `library.json` manifests, #40
  GFX fork diff.
