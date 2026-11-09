# Design interview - the rest of Phase 2 and all of Phase 3

**Purpose.** A structured interview whose answers are the blueprint for what is left of Phase 2
(2.8, 2.10, 2.11, 2.13, the rest of 2.9, the deck's future) and Phase 3 (the build sheet), and for
how the build sheet, the on-device UI and the web UI live together. **It does not reopen decisions
already taken**; those are listed in §0.2 so nobody asks them twice.

Each question is marked **ANSWERED** with the decision that settled it (`docs/DECISIONS.md`) and the
document that holds the design, **DROPPED** when a later decision made it moot, or **OPEN**. A
question with no mark is OPEN. Decisions now enter DECISIONS.md at a milestone's sign-off (W14).

---

## 0. How to run it

### 0.1 Format

- **Facilitated by Claude**: one section per sitting. Claude asks a few questions at a time in chat,
  shows a mock or screenshot where the question is about looks, pushes back where it sees a problem,
  and writes the answers into the named design doc. The owner answers in any form - a sentence, a
  sketch, a screenshot of something he likes, "your call".
- **Every question has a recommendation** where Claude has one. "Go with the recommendation" is a
  valid answer.
- Questions marked **[BLOCKING x.y]** must be answered before milestone x.y starts; the rest can wait
  until their milestone is next.
- **Output of each session:** the answers are written into the design doc named at the top of the
  section, and the ROADMAP row is updated.
- **Show, don't spec** (HANDOFF): for a question about looks, Claude builds a throwaway mock on the
  glass or a screenshot before the owner is asked to choose.

### 0.1a For the Claude facilitating it - the protocol

1. **Before a session**: read `CLAUDE.md`, `HANDOFF.md`, this file, and the design doc the section
   feeds. Check the section's questions against what has changed since - drop any the code or a
   later decision has already answered, and say so.
2. **Open the session** by naming the section, its purpose in one sentence, and roughly how many
   questions. Ask **3-4 questions at a time**, each with its context and recommendation in plain
   words (the owner dislikes jargon and metaphor). Never paste the whole section at once.
3. **Looks questions: show first.** Build a throwaway mock on a board (or a `/screenshot`, or a quick
   HTML sketch) and ask the owner to react to it. Say what the mock is not (e.g. "colors are
   placeholders").
4. **Engage, don't transcribe.** Push back when an answer conflicts with a decision in §0.2, a
   measurement, or a gotcha in §10 - name the conflict and the evidence, then let the owner decide.
   His "I'm wondering whether..." is usually a design instinct worth following up.
5. **Record as you go**: write the answers into the section's design doc in his words where they
   matter, with the decision and its reason. Mark anything left open as OPEN with what would settle
   it, and mark the question here.
6. **Close the session** with a short summary: what was decided, what is open, what it changes in
   the ROADMAP; update the ROADMAP row and HANDOFF.
7. **Do not build** anything a session decides until the owner says go - the interview produces the
   blueprint, not code (throwaway mocks excepted). Keep the running "new questions" list (§12) for
   anything the answers raise.

### 0.2 Already decided - not asked again

| Decision | Where |
|---|---|
| Build sheet precedence: defaults < build sheet < runtime edits, unless locked | ROADMAP Q2, D-7 |
| Authority locks: tri-state INHERIT / LOCKED / UNLOCKED, inherited down Dashboard > Page > Grid > Card > Entity; **lock configuration, never interaction**; the UI names the ancestor that locked something | ROADMAP Q2 |
| The model is nested C++ structs; JSON is the interchange format; two loaders (compile-time, runtime LittleFS) | ROADMAP Q3, D-2 |
| Every `*Defaults` struct is a schema; a build sheet is overrides against it | ROADMAP Q3c, D-3 |
| Unit-granular placement with a validator that reports, never resolves | ROADMAP Q3b, D-4 |
| Public path works on first flash with local-only demo cards; personal path per room; cards responsive (preferred / minimum span, priority) | ROADMAP Q4, D-5 |
| Tap on a card is a one-touch toggle; long press opens the window | K3 |
| The window appears complete at once and closes at once: no grow, no dim | K4, `card-sheet.md` §14 |

---

## 1. Look and feel - the whole product  → `docs/design/look-and-feel.md`

**CLOSED** - summary and what stays open: `look-and-feel.md` §14.

1. **Who looks at these screens, from where, for how long?** *Sets text sizes, contrast, how much a
   card may say.* **ANSWERED** -> `look-and-feel.md` §2
2. **Three words for the feel.** **ANSWERED** -> §3 (a rule, and "clear, alive, crafted")
3. **Motion budget.** **ANSWERED** -> §3: design for the P4s, cut where the S3s lag; popups appear at
   once (K4)
4. **Night.** Dim, switch scheme or turn off; by clock, by room light or by HA's sun?
   **ANSWERED** -> §4 (night hours, black with a clock or nothing, a tap wakes it); built with the
   bedroom clock, 2.15 (K131)
5. **Sound and touch feedback.** **ANSWERED**: off by default, a per-device setting (#23)
6. **Icons.** **ANSWERED**: MDI everywhere; custom HA icon packs wanted, #83 -> §12
7. **Dense vs airy.** **ANSWERED** by the arranger (K105, K109, K121) -> §5
8. **What does "broken" look like?** **ANSWERED**: status pills follow the label type, inside the
   card's borders -> §9
9. **Reference images.** **IN PART**: the irrigation dashboard -> §13; more welcome any time
10. **Chrome text size, Linen's fill and "on" color, the Wi-Fi header icon, hero value sizes.**
    **ANSWERED** -> §10, §11 (Linen: D-32, D-33; header glyphs: D-24, D-27). Chrome text's millimetre
    targets and the close / far viewing distance setting (#73): OPEN
11. **Beyond the grid.** Is a page always a grid of equal cards, or can it hold groups with their
    own shapes? **[BLOCKING 2.11]** **ANSWERED** -> §5: mixed sizes on the unit grid and groups are
    the base, an arranger with pins (K105, K109)

## 2. Card popups (2.10)  → `docs/design/card-sheet.md`

**CLOSED** - answers in `card-sheet.md` §11-12; the group question it raised is in `group-cards.md`.

1. **D1-D7** (the window's design decisions). **ANSWERED**: K4 (no grow, no dim), K5 (tabs for the
   entity's own features, panels for settings), K6 (opens on its control), K7 (members behind their
   own icon), K8 (settings saved by card id, written on close), K9 (four ways to close), K10 (hue
   strip and eight swatches). **[BLOCKING 2.10]**
2. **Which kinds get a window?** **ANSWERED**: lights and switches first, sensors with History
   (K52, K57).
3. **Lights without color** (dimmable only): the controls show only brightness. **ANSWERED** (the
   window shows what the light reports, `card-sheet.md` §15).
4. **Color presets.** **ANSWERED** (K10): a hue strip and eight swatches.
5. **Effects** (for lights that have them): a list, later. OPEN
6. **History spans**: **ANSWERED** (K52, K63): 24 / 12 / 6 / 1 h for a number, 24 h or 7 days for
   activity; ten rows, then Show more.
7. **A window for virtual / system cards** (uptime, heap, RSSI). OPEN
8. **Closing**: **ANSWERED** (K9, K79): X, a tap outside, a drag down on the header row, and an
   auto-close that is a system setting.
9. **Custom name**: **ANSWERED** by the on-screen keyboard (K95), in place of the recommendation
   "web UI only".
10. **Settings scope in the window**: which are per card and which per page or group. **ANSWERED**
    (D-7, K40-K42, K44-K46, K67; the window's own rows: `card-sheet.md` §17).

## 3. Header bar v2 - the slot mechanism (2.8)  → `docs/design/header-slots.md`

**ANSWERED and BUILT**, from the mock "2.8 Header Slots" (D-11 to D-35). The slot mechanism serves
two places: the page header and group headers; a solo card's header keeps its formula (D-14).

- [x] 1. **Page header content, per screen size** - the header is optional, so nothing critical
  lives only there; the clock is a card first (D-11) -> §1.
- [x] 2. **Slot types** - three places, not four slots: the clock, alarm and temperature on the left,
  the page's name and dots in the middle, the status icons on the right (D-16; D-12's four slots
  replaced) -> §2. The glyph style is the owner's choice in UI Tweaks (D-25) -> §7.
- [ ] 3. **Clock**: 12/24 h, seconds, date format. OPEN (`include/TimeFormat.h` holds the wording,
  K59).
- [x] 4. **Tapping a slot** - each slot slides down its own panel from its place, which can
  deep-link to settings (D-13) -> §6.
- [x] 5. **Header hidden / peek** - a page setting; full-screen pages hidden by default (D-14) ->
  §9. It comes back by a swipe down from the top (`pages.md` §3).
- [x] 6. **Card header slots** - none on a solo card; slots for groups (D-14) -> §8.
- [x] 7. **A stale slot** - dashes until live data, and a way to show an error (D-15).

## 4. Group and area cards (2.11)  → `docs/design/group-cards.md`

**ANSWERED and BUILT** (K88-K129, K146-K196).

1. **Confirm the kinds.** **ANSWERED** (K88, K146): a **group** is the bordered block - shown as a
   group on the page or as one card - and its members share its name and color; a **label** is the
   colored name on a solo card. -> `group-cards.md` §1
2. **Aggregate tap** when members disagree. **DROPPED**: answered in 2.10b, as HA's light group
   (K13).
3. **Group sizes**: smallest useful size and most entities per size. **ANSWERED** by limits the
   arranger never crosses (K109: a tile no smaller than one of a 3 x 1 group on the 4B at 4 x 4).
4. **Group header**: which values, one line or two. **ANSWERED** (K101, K115, K128): from the right,
   temperature, occupancy, light level, then count / alert; one line; four sensors at most.
5. **Inside a group**: mini tiles, rows, or per entity. **ANSWERED** (K89, K116): mini tiles, each a
   faint panel, the icon over its label.
6. **Long press inside a group**: on a member, that member's window (K104); a group's own window
   opens Members (K129), and a tap opens the group view (K154). **ANSWERED**
7. **A group on a small screen** that cannot fit its members: shown as its one-cell solo card
   (K92 rule 6). **ANSWERED**
8. **Rooms as linked pages** (`pages.md`): a group page, and how "back" works. **ANSWERED** in part:
   the group view is built (K154, K169); group pages (K99, K118) are not built.
9. **Group color**: per group, device-wide. **ANSWERED** (K46, K47).

## 5. Edge gestures, the system panel, and what becomes of the deck  → `docs/design/context-panels.md`

**Owner's direction - the working assumption until decided otherwise:**
- **The deck is not part of the default dashboard view.** It stays in code because it may be worth
  repurposing: inside windows or detail views, or on targeted room pages reached by a specific route.
- **Device-wide settings and anything affecting the whole dashboard come from a system panel at the
  top** (today's system drawer, D-22).
- **Edge swipes become shortcuts**: at least two swipe-up and swipe-down targets, one fixed to the
  system panel / settings.
- **All the swipe zones should be user-customizable**, and a target can be a page, a card window,
  the top level of settings, a deep link to a specific settings view, or anything else. A *target*
  is a general "go to" address, the same idea as linked pages and card links, wanting one addressing
  scheme used by swipes, cards, slots and the navbar alike.

The gestures as built are in `pages.md` §3 and are code, not a table of targets.

Questions (all OPEN unless marked):
1. **The three zones**: which edges and corners exactly, their defaults, and whether the
   system-panel zone is customizable too.
2. **The "go to" address**: what it must be able to name (a page including hidden room pages, a card
   window and its tab, settings top level, a settings deep link, the overview, a scene or script, a
   URL), and what happens when the target no longer exists.
3. **Per page or per device?**
4. **The deck's future**: inside windows, on room pages, or retired once Audio and Display move into
   Settings (4.1).
5. **Audio and Display panels** (today's deck contents, developer tools): into Settings when 4.1
   starts (recommendation).
6. **Scenes and scripts**: runnable from a shortcut or a card? A scene is loaded from a card (K15,
   K70); a script is a new outbound call type.
7. **The system panel**: **ANSWERED in part** (D-17, D-22): CONNECTIVITY, then SYSTEM - DIAGNOSTICS
   (Log, UI Tweaks, About, the scheme, the grid). Whether it grows into Settings pages (4.1): OPEN.

## 6. Pages and navigation (4.2/4.3)  → `docs/design/pages.md`

1. **Home page per device** and **return-to-home after idle**. The board boots on House
   (`pages.md` §1); the idle return is OPEN (recommendation: 2 min, a per-device setting).
2. **Page transitions.** **ANSWERED**: instant until the rebuild is faster (`pages.md` §4). A page
   change is a rebuild of about 300-400 ms before any animation could start.
3. **The overview (alt-tab)**: wanted soon, or later? OPEN
4. **Navbar (4.3)**: an edge strip of page buttons. OPEN
5. **Linked pages** (from a card or group): how deep may they nest? OPEN
6. **Kiosk mode**: a board locked to one page, no drawer, no swipes (an authority-lock use case).
   OPEN

## 7. Finishing 2.9 (other boards)  → `docs/display/README.md`

The display stack's status per board and its open questions are in `docs/display/README.md` (A9).

1. **Order**: the 7B and CYD_P4_1060 next. **ANSWERED**: all five P4s and `WS_S3_4B` have moved.
2. **The S3s** (RGB panels) may need an S3 library rebuild if the numbers call for it. OPEN
3. **CYD_S3_3248** (QSPI): its partial-update question; a spike, or accept full-frame sends? OPEN
4. **The 16 ms refresh** (`FLEET_LV_REFR_PERIOD`) for boards that can afford it. OPEN
5. **Packaging the flush** as a shareable component. OPEN

## 8. The build sheet (Phase 3)

The settings file `/cfg/settings.json` is the runtime schema (K34), so Phase 3 is shrunk to exporting
and importing it, together with the companion web app (D-8, K131).

### 8.1 What it describes **[BLOCKING 3.1]**

1. **Scope**: dashboards, pages, cards, entities, areas, header slots, themes, device settings,
   `*Defaults` overrides. Anything else? Anything out? OPEN
2. **One sheet per device, or one fleet sheet with per-device sections?** Recommendation: one fleet
   file plus per-device files that include and override it. OPEN
3. **Card identity.** **ANSWERED** (K33): a stable id made when the card is created, from its area,
   label and a stamp; unique on the device, never changed or reused. This replaced the
   recommendation of human-chosen ids.
4. **Entity references**: by HA `entity_id` (renames in HA break them). **ANSWERED** in part (K35):
   a saved setting whose card or entity is gone is kept and listed, never deleted automatically.
5. **Areas**: taken from HA, declared, or both? **ANSWERED** (K46, K88): HA areas become groups when
   an entity from one comes onto the board.
6. **Themes in the sheet**: can a sheet define its own scheme, or only pick one of ours? OPEN

### 8.2 Authoring

1. **Who writes it and with what?** A text editor and a PC-side validator, the web UI, or a visual
   editor later. **ANSWERED** in direction (D-8): a companion web app with a true preview sends the
   configuration to the device; the sheet is its output and backup format.
2. **Validation**: on the PC, on the device, both? Recommendation: both, the same code. OPEN
3. **What the device shows when a sheet is bad.** **ANSWERED** in part (A17): a file that exists but
   will not read makes the boot read-only and is never overwritten.
4. **Previewing** a sheet before it goes to a board. **ANSWERED** (D-8): the companion app's preview.

### 8.3 Delivery and versioning

1. **How a new sheet reaches a board**: compiled in, uploaded over HTTP (`POST /settings`, A17),
   pulled from a URL or HA, pushed over MQTT. OPEN
2. **`schema_version` and migration**: refuse newer-than-me, migrate older with a logged note
   (recommendation). The file has `schema` 1; additions are additive (K106). OPEN
3. **Fleet updates**: change the fleet file once, all boards pick it up. K180: boards keep their own
   configuration until the web app and remote management. OPEN

## 9. Build sheet vs on-device UI vs web UI - living together

The precedence is decided (§0.2). The owner's description of what a build sheet is: "just a template
with entity data and settings", the same thing as an exported configuration, applied on first boot
or imported any time, with export and import at levels, "Restore previous settings when flashed", a
hard reset to factory, and a web UI that configures everything the sheet can
(`look-and-feel.md` §8).

1. **Where runtime edits are stored.** **ANSWERED** (K34): one JSON file on LittleFS in the sheet's
   shape, storing only what was changed, credentials staying in NVS. **[BLOCKING 2.10d]**
2. **Web UI and on-device edits**: the same layer, last write wins (recommendation); both respect
   locks. OPEN
3. **Export**: a button that produces the merged JSON. **ANSWERED** in direction (K181): Backup and
   Restore is a milestone of its own before Phase 3 (2.16).
4. **A new sheet over runtime overrides**: keep by id and report orphans (recommendation), consistent
   with K35. OPEN
5. **Factory reset**: back to the sheet. The system panel has no Reset until Backup and Restore
   replace it (K192); `/popup?dash=2` resets a development board.
6. **Locks in the UI**: a locked setting shows greyed with "locked by <ancestor>". Whether the owner
   can unlock from the board with a PIN, or only by a new sheet (recommendation): OPEN
7. **Web UI access**: open on the LAN, a PIN, or HA-authenticated. OPEN
8. **Multi-device**: a change on one board's web UI applies to that board only, or is offered to
   others. OPEN
9. **What the web UI is** (4.5): served by each board (decided baseline) or a single fleet page served
   elsewhere. OPEN

## 10. Gotchas Claude already sees (for the owner to weigh, not answer)

| | Gotcha | Consequence |
|---|---|---|
| G1 | **`lv_mem` is small.** LVGL's pool is 512 KB in PSRAM on every P4 and 128 KB in PSRAM on every S3 (A11) | each feature still budgets its widgets on the S3s; lazily built panels stay the habit |
| G2 | **Flash writes (NVS, LittleFS) pause the flash cache**; on the P4 the per-frame DSI restart waits for it | fixed with `CONFIG_LCD_DSI_ISR_CACHE_SAFE` in the library rebuild (A14); settings write on a task whose stack is in internal RAM, and only when something changed (K34) |
| G3 | **Stable ids** for cards, pages and groups are the foundation of overrides, saved settings, locks, links and export | card ids K33; page ids and slugs `pages.md` §1 |
| G4 | **HA renames entities**; a setting pointing at the old name breaks silently | saved settings whose entity is gone are kept and listed (K35) |
| G5 | **Websocket receive buffer** limits history and big attribute sets | 48 KB in PSRAM where there is PSRAM (K54); history through the statistics and history APIs |
| G6 | **Wall-clock time** is needed by the clock slot, "8 min ago", history axes and night mode | `TimeService` (SNTP); the boot report waits for the first sync |
| G7 | **The S3s draw 3-5x slower than the P4s** | every visual decision needs an S3 check; motion that is smooth on a P4 may stutter on a CYD |
| G8 | **Two authoring surfaces (sheet and UI) invite drift** | export (§9 item 3) is what keeps them one thing |
| G9 | **Color-blind owner** | every state distinction by shape or label as well as color - a review item on every mock |
| G10 | **The board count grows** | every per-board number stays in the BSP or is derived; the sheet never names a board's pixels |

## 11. After the interview

Each section's answers go into its named design doc; ROADMAP rows are rewritten from them with
acceptance criteria; new issues are opened for anything the answers add. Claude then proposes the
running order in one table for sign-off.

## 12. New questions raised by the answers (running list)

| Question | Goes to |
|---|---|
| **Card sizes**: a card at 1x1 / 2x1 / 2x2 shows more on its face; who picks the size - the settings file, the glass, both? | `look-and-feel.md` §6 |
| **Drag-and-drop layout editing on the glass**: an edit mode on the board, or web UI only? How it meets locks | §6 / §9 |
| **Per-device role and viewing distance** (nightstand, kitchen, garage): a setting that scales hero text | `look-and-feel.md` §2 |
| **HA alerts on the devices**, customizable, also on the night screen. **ANSWERED**: K138, K139 (2.14); notices are a queue modelled on NINA's toasts (D-19) | `ROADMAP.md`, #76 |
| **Clock card** with several faces; **weather card** with small animations | K81-K83, 2.15 |
| **Irrigation interface**: fleet firmware with an irrigation page set, or its own project? | later |
| **Media player**: Music Assistant, Sendspin, Spotify, SoundCloud; album art | card library; `look-and-feel.md` §1 |
| **Auto layout** with pins, reopening ROADMAP Q3b. **ANSWERED**: the arranger (D-4, K105, K109) | `layout-engines.md` |
| **Idle pre-rendered page pictures** for transitions | `look-and-feel.md` §7 |
| **Link cards and function pages**; navbar hot-links | §5 item 2, §6 item 4 |
| **First-boot setup wizard** with a Wi-Fi QR code; import / export after v1.0 | §8.2, §9 |
| **A changed build sheet over a board with user edits**: keep, replace, or ask? | §9 item 4 |
| **A notification queue**: notices from HA shown in order, opened at any time, some kept until dismissed. **ANSWERED**: D-19, K138 | `header-slots.md`, #76 |
| **An active card's border** (a thicker contrasting border glowing inward). **BUILT**: the active frames (D-30 to D-33) | `tokens.md` §8 |
| **A header slot's panel lies on the bar, with a halo**, and **windows get the same halo**. **ANSWERED**: D-35, K108 | `header-slots.md` |
| **A group made from a group: a snapshot, or following its group?** **ANSWERED**: a snapshot (K100) | `group-cards.md` §2 |
