# 2.13 - what is left, in order (written 2026-11-04, after round 4)

**The short-term roadmap for the rest of 2.13**, so nothing discussed in the long 11-02 to 11-04
session is lost. Check items off here as they land; this file is archived beside `TEST_2.13.md`
when 2.13 merges. Decisions still go to `DECISIONS.md`, test rounds to `TEST_2.13.md`, the design
to `docs/design/ha-cards.md` - this file only orders the work and says where each piece stands.

**Where it stands (2026-11-05):** branch `feat/2.13-add-from-ha`, pushed; five rounds built
(K138-K178). Round 5 (`835b870`, plus `b588cab`'s tokens, which change nothing on the glass) runs on
the P4_5, the 4B and the 1060, their settings converted for K171. The CYD_P4_4880 runs round 2 with
its compiled cards: it needs `retire_compiled.py` before it takes round 5's firmware. **Round 5 is
untried on the glass** - `TEST_2.13.md` round 5, T1-T23 and K171's T24-T27. **What is not built yet
and what was discussed but not explored enough: §7.**

## 0. First: the owner's round-4 test - done 11-04

Recorded in `TEST_2.13.md` round 4; his further findings became 1.11-1.19 and 2.8-2.10.

## 1. Round 5 - the owner's visual list and small fixes

Agreed in direction; each is small. Where the cause is already known, it is given.

**Round 5 progress (unattended, 2026-11-05; the owner granted: build, flash the P4_5 / 4B / 1060 with
settings saved and setdiff after, convert their settings for K171, commit and push; no PR):**
- **Built, flashed to the three dev boards, round 5's sheet written** (`4966cb0`, `65d6f28`,
  `4c977d6`, `5c03eaa`): 1.1-1.7, 1.12, 1.13 (K166), 1.14 (K168), 1.17, 1.18 (K173), K165's
  dialogs, K167 (the arranger keeps its answer, v5; Card margins), K169, K172, K174, K175, K176,
  K178, the check mark (K168).
- **K171 built after the owner's answers (11-05, `835b870`)**: the compiled cards retired; Panel the
  Fleet page's one built-in group (with IP Address); Garage - Doors compiled until 2.14; the test
  cards in `scripts/release/test_cards.json`; the dev boards' MQTT group; the three boards' settings
  converted (`scripts/release/retire_compiled.py`) and put back before the flash, 0 differences.
  Pin/Unpin names in the accent; New group's dialog says 'Visibility'. **1.10 done** (`b588cab`).
- **Not built: K170** (§7.1), and the smaller pieces in §7.2.

| # | Item | What is known |
|---|---|---|
| 1.1 | **The pin is black** in Members and Edit cards | `pinGlyph()` (`Popup/PopupMembers.cpp:96`) and Edit cards' sign (`UI/EditCards.cpp:92`) set no color, so the pin takes the icon role's default. **The accent color (K178)** |
| 1.2 | **Corner cut on Linen in the group view** (Cards mode): the cut corners show the wrong color | `Cards/CardFrame.cpp:418` paints a cut corner in the page's `GROUND` unless the card is a mini tile; a group view card is a full card standing on the cluster's `SURFACE`. The corner wants the color actually behind the card |
| 1.3 | **The header sensors' drop-down animates** as the system header's slot panels do | `dropOpen()` in `Popup/PopupGroup.cpp` builds a plain panel that appears at once; the system header's panels are `Kit::Drawer` |
| 1.4 | **Long press on a group view tile or card** animates as it does on the page | the page's long-press feedback is not on the group view's members |
| 1.5 | **Edit cards, an HA area's rows for cards already here** (on the page or in the group) still use the old flat "In this group" style; they should show the square and take the tap cycle (K161) | `fillBody()` in `UI/EditCards.cpp` draws the cycle only for the In this group / On the page lists |
| 1.6 | **No panels under Edit cards opened from a group**, as from the page - more room | |
| 1.7 | Round 4's gaps: **Visibility's Add to group joins unpinned** (K160: only Edit cards does today); **a tap outside the header drop-down folds it** before anything else | `TEST_2.13.md` round 4, "Not built yet" |
| 1.8 | Round 1's knowns, to recheck on the glass | a humidity's decimal (43.0 %: 3.2 below), the 4B's °F over a number (likely gone with the capped tiles), HA's long names wrapping in tiles |
| 1.9 | The 4B stopped answering HTTP for minutes after a settings restore's reboot (11-04); opening its serial port reset it | watch for it; nothing to fix yet |
| 1.10 | **Drift the structure check found in 2.13's code** (11-04) | four millimetre numbers where a `UIShape` token belongs: `PopupGroup.cpp:168` `mm(100)`, `EditCards.cpp:307` `mm(80)` and `:560` `mm(100)`, `HaPicker.cpp:120` `mm(80)`; `UI/HiddenSheet` is no longer reachable on the glass (Edit cards replaced it) - **kept for now** (owner, 11-05: hiding and pinning may change as group views and pages grow). Files over 1,000 lines: `GUIManager.cpp` 1,529, `Card.cpp` 1,462 (unchanged in 2.13); **round 5 put `Popup/PopupCluster.cpp` at 1,005** - Edit cards' save and its dialogs want a file of their own (`PopupEditCards.cpp`). The mm numbers have moved: `PopupGroup.cpp:210`, `EditCards.cpp:351` and `:615` |
| 1.11 | **A dialog for Unpin / Pin this card** (round 4 T7, T8). The owner's words for Unpin: headline "Unpin this card?"; "This will hide the card <card> from the group when looking at the page."; "The card will still be a member of group <group> and will be visible on the 'Group view' and the 'Members view'."; Cancel, Confirm. Pin's words to agree | today no dialog (K160) |
| 1.12 | **No Scenes for a light added from HA** (Eric Bedroom, `light.eric_bedroom_lights_hue`; Desk has them) | the scene query asks only lights that are not `learnt` (`HaProvider.cpp:116`), to skip a group's members; `adoptHaCard` makes a made card's entity `learnt` too. Ask adopted cards as well (a flag of their own). Then check HA's `device_id()` gives the Hue group a device |
| 1.13 | **A member's SETTINGS greyed when opened from the group view or Members** (Tap action, Long press opens, Card UI; also Units, Active state); live from its page tile | greyed on purpose in a member view (`PopupSettingsRows.cpp:170`, `!inMember()`: "it is the card's"). The fix is a settings subject - the member's own card - for these rows and their callbacks, as Visibility already has (`visibilitySubject()`); changing the flag alone would save to the cluster |
| 1.14 | **New group redone as Edit cards** (owner's design, 11-04): full-size window, "New group", "Model this group after an existing HA area?" with HA's areas and "Create new"; then Edit cards' flow, its first list "New group" or "<area> (new)", taps add / add pinned; "Create group N"; then a dialog in the owner's words: headline "Create new group?"; "A new group will be created named <area name selected> (or new group). You can rename this group from the GROUP menu."; "The cards that will be added to the group are: <cards>" (K168); "From the GROUP menu you can add or remove cards, and from the 'Members view' you can change which cards are pinned."; Cancel, Create | `NewClusterSheet.cpp` uses chips and the old `HaPicker`, default window size, every member pinned. Edit cards needs a group context of {card id, area, group id} in place of a live `ClusterCard` (`doEditSave`, `fillMembers`); `CardAdded::addTo(id, area, gid, ...)` already takes strings. **Mock first** |
| 1.15 | **The arranger runs when nothing on the page moves** | (a) showing / hiding the system header rebuilds and re-searches: the kept answer's key includes the page's height (`PageArranger.cpp:81`); the kept arrangement could be re-fitted to the new height instead. (b) adding unpinned members re-searches: the key counts every member (`b.members`, `CardPage.cpp:556`), pinned or not, and any `CardAdded` change rebuilds the page |
| 1.16 | **Compiled members cannot be removed** (round 4 T9, T10): a tap only flips the pin | `canRemove` is only for made cards (`PopupCluster.cpp:396`). Answered by 3.3 if that comes first; otherwise a note on the row (owner: "This card was manually added and cannot be removed at this time."). **Moot: K171 retires them first in round 5** |
| 1.17 | **A group's own Card state is ignored in its group view**, which follows UI Tweaks (owner, 11-05) | the group view's cards should take the group's Card UI choice |
| 1.18 | **The group view's colors** (K173): its background the page's ground, its cards the page's card color | today its background is the card color |
| 1.19 | **Everything else settled 11-05**: Pin dialog and dialogs for membership changes (K165), member settings live (K166), no arrange for the header or unpinned members, spare height as spacing (K167), New group's tap cycle, no check mark, naming step (K168), HA groups as groups (K170), compiled cards retired (K171), corner icons (K172), Show as / Rename group to Visibility (K174), GROUP on every view (K175), 128 cards (K176) | see each decision |

## 2. To settle with the owner before building

| # | Question | Notes |
|---|---|---|
| 2.1 | **Corner icons on cards: still needed?** | **Settled, K172**: only on value sensors. The corner icon was the card's kind (cards.md); since 2.11 a tile and a solo card share one icon size, and value cards squeeze their value to clear it (#73's half) |
| 2.2 | **GROUP panel's Show as and Rename group move to Visibility?** | **Settled, K174**: they move to Visibility |
| 2.3 | **A GROUP panel on views that have none** (History, Scenes)? | **Settled, K175**: GROUP on every view without panels of its own; History and Scenes keep theirs |
| 2.4 | **"Garage - Doors" becomes an ordinary group** | still the bespoke card K98 left open. **Open (11-05)**: the owner wants roll-up entities in HA (`binary_sensor.garage_doors` with attribute `open`: North / South / Both / none; `sensor.doors_open` with `names`, `entities`) that a card shows while its members stay reachable. Claude's proposal (11-05): HA's binary sensor group helper where "any open" is the job, the board reading members from `entity_id`; a template carrying `entity_id` for anything more. **Deferred to 2.14** (owner): Garage Doors may be the only roll-up whose members are commanded; most will be informative (several temperatures, one History) or alerts |
| 2.5 | **How many made cards?** `CardAdded::MAX` is 64 | **Settled, K176**: 128, a warning within 25%. Hidden page cards do not count |
| 2.6 | **Which unit is shown** (°F / °C) - the owner: never assume, and let the user choose | **Settled, K177**: UI Tweaks, Inherit / F / C; precision HA's. Built with 3.2 |
| 2.7 | "Create N" or "Add N" for a new group; label and group words | judge on the glass; re-read every on-screen "group" against K146 / K158 once more |
| 2.8 | **Back from History reached directly** (round 4 T2): back to where you came from, or always to Details? | **Settled, K169**: back to where it was opened from. Back is hard-linked today (`CardPopup.cpp:216`); no view stack, only each member level's way out - the level needs to know it was entered straight onto History |
| 2.9 | **HA light groups as groups** (Desk `light.office_lights_hue`, Bedroom `light.eric_bedroom_lights_hue`): the GROUP panel, Show as, Edit cards. (a) a group made on the board from the HA group's members; (b) change the HA group's members through Spook | **Settled, K170**: (a). Today such a card has Members through its entity (`s.native`) and no link to a group card; Desk and Bedroom are Hue groups, defined in the Hue bridge |
| 2.10 | **Retire the compiled dashboard's cards now** (3.3 pulled forward; the owner: "as soon as possible") | **Settled, K171**: they retire in 2.13, as made cards with their ids. How a board with no settings file starts is still to settle |

## 3. The bigger pieces still in 2.13

1. **The thermostat's controls** (K144, K153; artifact "2.13 Round 2 Mocks", boards 14-15) - **built
   with the owner present**, since it commands the house:
   - its window like HA's thermostat: an arc for the setpoint, what it is doing on top (Heating,
     Idle), the setpoint large, the measured temperature under it;
   - the measured temperature (top left) and humidity (top right) open those sensors' History; the
     History icon keeps the thermostat's own state changes;
   - **Heat only**, one handle: a tap on the setpoint to change it, the command sent a few seconds
     after the last tap (the Honeywell API is slow; overshoot can be undone first);
   - Mode (the kit's selector: Off, Heat/Cool, Cool, Heat) later - it stays on Heat until spring;
     no preset or fan mode; a thermostat card on the page comes with the card library (#31).
2. **Units and precision from HA** (K153): the climate unit is hard-coded °F
   (`components/Fleet_HA/HaValue.h`, `haDescriptor`). Take HA's unit system (`get_config`) and each
   entity's display precision from the entity registry; never assume a unit.
3. **Removed for built-in cards** (K148) - or retire the compiled dashboard's cards in favour of
   cards made from HA, which the owner would prefer ("leave those behind sooner rather than later").
   **Decided 11-04 (K171): retire them**, pulled forward to round 5 or 6 - see 2.10.
4. **The text fitter beyond Edit cards** (K152): Members, the label step, tile names. `fitLabel` is in
   the kit.
5. **Two levels of nesting** (K147) enforced: a group shown as one card may join one other group,
   and then neither joins a third.
6. **New group from the page's own cards** (mock 11; K91) - a group with no HA area.
7. **Checks with HA running**: T8's "1 new" (needs an HA label change); a cluster's "all off" when
   HA has no light group for the room (what the command path can target); `unknown` after an HA
   restart (#84).

## 4. Home Assistant's side (K132-K138; log `../Home Assistant/projects/2026-10_fleet-2.13/`)

Nothing has been built in HA yet beyond rewording `FLEET_REFERENCE.md` rule 3 and `HOUSE_RULES.md`
§7 (K133, 11-02, HA commit `d8293c5`).

- **K136, the YAML conversion** - the logical and status layers as hand-written files by function,
  like `packages/sprinklers/v4_*`; `house_logical.py` retires. The file layout is still to agree.
- **Per-room status entities** (HA choice 3) - probably not needed: the boards count from a group's
  members. Decide, and close it in HA's log.
- Found and flagged, not fixed: some doors appear twice (rename leftovers); `delay_on` and
  `last_changed` reset when HA restarts (a trigger-based template or Spook's time-in-state); what HA
  2026.11 and Spook now offer, not yet read.
- At sign-off: HA's log says what changed, and the tag `fleet-2.13-done`.
- **Alerts' anatomy is decided** (K138) and is 2.14's to build, on both sides.

## 5. Sign-off and merge

The owner signs off -> `CHANGELOG.md` `v0.2.21` (`RECAP_2.13.md` is its source), archive
`TEST_2.13.md` and this file, `structure_check.py`, the control-byte scan, the all-nine gate
(`gate.ps1`, ~15 min, background) -> push, PR -> **the owner merges** -> tag `v0.2.21` on the merge
commit -> `flash_release.ps1`, the 4880 by hand -> HA's `fleet-2.13-done`.

## 6. Parked for later milestones, so it is not lost

- **2.14 alerts** (K138, K139): an Alerts view with every active alert and a history of resolved
  ones; acknowledge, "Quiet for 30 min" by severity; loud first, then a quiet glyph until resolved;
  "About this alert" (its rule and what it watches); actionable phone notifications calling the
  same scripts; a card whose entity is a cause framed in the alert's color.
- The owner's alert ideas: leak sensors only ever as alerts; temperature too hot / too cold; an
  air-quality meter with thresholds; the garage doors as cards that open and close once the Zigbee
  relay is back.
- **The web app** (#87): choosing the page in Add to dashboard; Edit cards beyond the House page;
  true delete; designing a page with a preview.
- Mirroring the boards' groups as HA group entities (owner's idea, 11-03, extended 11-04; deferred):
  each board group as a discrete HA group entity, so HA shows every board's groups - perhaps to
  manage the fleet from HA, sync a group across boards, or deploy the same groups to several boards
  at once. Also changing HA's own group entities' members through Spook (K170; never Hue groups).
  The owner's further thought (11-05): each board group as an HA group entity whose attributes say
  which members are in the header, or with a companion helper entity holding its settings - for
  Phases 3 and 4. He would recreate Desk and Bedroom as HA groups but for Hue scenes, which only
  target groups made in the Hue app.
- The old cell pinning, shelved (K164, `FUTURE_IMPROVEMENTS.md`).
- **Voice and the ESPHome API** (A18, Phase 7, `design/voice-esphome.md`).
- Look at NINA's repo now and then for ideas (`REFERENCE_PROJECTS.md`).
- **#73** stays open: chrome text still comes from two fixed sets chosen by `bspUiScale() >= 1.3`
  (`UI/UIToolkit.cpp:51`), and value cards still pick VALUE or VALUE_SM each on their own.

## 7. Not built yet, and not explored enough (written 2026-11-05, after round 5)

Gathered from this plan and the 11-04/05 session, so nothing agreed or floated is lost. Each item
says where it stands; settle the open ones with the owner before building.

### 7.1 K170 - an HA group entity is a group on the board (decided, not built)

The biggest piece left. Decided: every HA group entity (Desk `light.office_lights_hue`, Bedroom
`light.eric_bedroom_lights_hue`) is a group on the board, shown as a Solo card by default, with the
GROUP panel, Show as and Edit cards; a command goes in one call to the HA group entity and any card
added to it; the face decides what a tap can do. What building it needs, found on 11-04/05:

- **A group as a member of a group** (K147's two levels): a card made on the board that is a cluster
  cannot be a member yet - `CardAdded`'s member records need an entity (`readMember()`), and Desk
  sits inside Office.
- **Its members as cards**: today Desk's lamps are entities learnt from `attributes.entity_id`
  (`haLearn`, `EntityRegistry::learnMembers`), with no cards - so nothing can be saved on them (K166's
  Long press opens for Desk's lamps, shown as a cluster, has nowhere to go).
- **The command path**: one service call naming the group entity plus the extras; today a group
  card commands its members one by one.
- **Scenes stay the group entity's** (Hue scenes target the Hue group): the board group's Scenes
  come from its HA entity (1.12's query now asks a card's own lights).
- **Questions still open**: is it automatic for every HA group entity found (decided: yes) - and when
  is the board group made: at boot from the HA entity, or the first time its card is made? What
  happens when HA's member list changes (a lamp added in the Hue app)? Which domains beyond
  `light.*` (switch groups, `group.*`; the owner: "every HA group entity").

### 7.2 Smaller pieces not built

| Item | Where it stands |
|---|---|
| **The system panel's development Reset wipes the House page** (found at the handoff, 11-05) | `CardAdded::resetToCompiled()` deletes every card made on the board - since K171 that is nearly everything. Its id buffer (`RESET_IDS`) is also smaller than 128 made cards. Decide what Reset means now (a saved baseline file? remove it?) before anyone taps it |
| **Moving a card home by hand is no longer a Restore** (K171, found at the handoff) | `homeCluster()` (`Popup/PopupMove.cpp:96`) and `CardAdded::specOf()` search only compiled specs; the former compiled groups are made cards now. Look home up among the made cards too; test Restore of Kitchen's Overhead and Outside's Garage |
| The golden harness (`scripts/ui_golden.py`) | its 62 states and "telling the house apart" were set before K171; check it still walks after round 5 |
| The 4-tap cycle's header step for a group's **existing** members (K168) | only cards being added have it; a member's cycle is still pin, remove, back. Its order with remove is to settle |
| The Active state row in a member's own view | live since K166, but it starts from the held card's value, not the member's |
| `Popup/PopupCluster.cpp` is 1,005 lines (structure check, round 5) | move Edit cards' save and dialogs (`editAct`, `doEditSave` and the rest) into `PopupEditCards.cpp` |
| **Panel's tiles one size** (K171) | the owner wants Free Heap, Uptime and IP Address wide enough for their values; tiles in a group are one size - a tile span per member, or Panel's tile columns, is unbuilt. Not yet seen on the glass |
| **The Fleet page is unseen after K171** | `/popup` has no way to change page: add `?gopage=N` (DEBUG_POPUP) so Panel and MQTT can be checked from the PC |
| **The arranger with the header shown again** (K167) | a page arranged while the header was hidden, shown again, fits its cards to the shorter room (they shrink) - not re-arranged. To confirm |
| The card limit's toast (K176) | a later ordinary toast replaces it; the header alert waits for 2.14 |
| Units, device-wide (K177: Inherit / F / C in UI Tweaks) | decided, built with §3.2 |
| New group from the page's own cards (§3.6, K91) | New group offers HA's areas only |
| **The CYD_P4_4880** | runs round 2 and still has compiled cards; convert its settings (`retire_compiled.py --mqtt`?) before it takes round 5 - ask the owner whether it is a dev board for MQTT |
| WS_P4_7B, WS_S3_4B (and the other S3s) | K171: the S3_4B takes the P4_4B's settings file, the 7B the 1060's - at the release flash, not yet done; S3_4B stays unflashed until the owner says (#69) |
| All Lamps (four entities on one card) | cannot be a card made on the board; left out of `test_cards.json` |
| A converted group's member order | made members come in the settings file's order, which may differ from the compiled one |
| `UI/HiddenSheet` | kept on purpose (owner, 11-05), unreachable on the glass |

### 7.3 Discussed, not explored enough

- **Roll-up entities and Garage - Doors** (2.4, deferred to 2.14 by the owner): HA's binary sensor
  group helper where "any open" is the job, members read from `entity_id`; a template carrying
  `entity_id` for richer roll-ups (`sensor.doors_open`'s count). The owner: Garage Doors may be the
  only roll-up whose members are commanded; most will be informative (a group temperature card with
  several lines on one History graph) or alerts in the system or group headers.
- **Card margins' steps** (K167): 50 / 100 / 150 / 200 % of today's gap, Claude's numbers - judge on
  the glass. Whether UI Tweaks' "Group view" row should also become a section like Card UI's.
- **Card UI's Card state row** - its place in the list is still "to confirm" (round 3).
- **"Built in"** - the mark Edit cards shows on Panel's row instead of a Hide tick (K171): Claude's
  words, to confirm.
- **2.7**: "Create N" / "Add N" and re-reading every on-screen "group" against K146 / K158 - New
  group's "Create group N" is settled (K168); the rest to judge on the glass.
- **1.8 / 1.9**: round 1's knowns (a humidity's decimal, the 4B's °F, long names in tiles) and the
  4B's slow HTTP after a restore - recheck on the glass.
- **Three lights read unavailable in HA** on 11-05 (`light.kitchen_light_sink`,
  `light.kitchen_light_table`, `light.porch_light`) - the boards show N/A, which is correct; an HA-side
  matter for the owner.
- **Issues**: #52 (auto-sort) - close if the owner agrees it is superseded; #116 - bring it up to date
  from this plan.

## 8. Settled on 11-06, and what is still open (after the K170, summary and backup reports)

Settled (K179-K183): Hue groups read-only, Indoors not offered, Desk to become an HA light group
(the owner's job); the boards keep their own configuration - nothing mirrored into HA, no Spook for
now; a full backup and restore before Phase 3; summary cards are one shared framework, designed with
2.14's alerts; Active state only where a member can be on; the arranging toast; Card UI's one list.

Settled later the same day (K184-K187): the mirror is a board group with HA's members read-only
plus the owner's extras; a mixed light group shows every control any member has (built); any HA
entity with members opens as a group; Backup and Restore take Reset's place, and a backup holds the
whole settings file while the board's own MQTT discovery stays compiled.

**K170, open**
- Which members count: Claude's proposal is any HA entity whose `entity_id` lists 2 or more
  members, members read-only, the face HA's own value; one-member aliases stay plain cards.
- Desk as an HA light group: check after the owner makes it that its HA scenes are offered.

**K181 (backup), open**
- Where it lives: on the board (LittleFS, beside the settings file), and on the PC through
  `save_settings.ps1` as now.
- **The arranged layout is already in the settings file** (`pages.<slug>.layout`, fingerprinted with
  the screen): restored on the same board it is used as is; on another board the page is arranged
  again.

**A group shown as one card opens its group view - a hard requirement** (owner, 2026-11-06, after
Indoor temp on the glass). Five or six room groups can stand as single cards, each tap opening that
room's group view. "When viewed as a solo card, if there is a single member pinned then it should be
the face. Pinning additional members turns it into a cluster." Today a solo card cannot offer Group
view (`PopupSettingsRows.cpp` offers it to clusters only), its face is the Face row (K151), and
Visibility chooses cluster or solo card by hand (K174). Claude's proposal, to confirm:
- **Pins decide the presentation**: none pinned - one card with the group's face (its switching
  members, else all); one pinned - one card whose face is that member; two or more - a cluster.
  Summary stays a choice of its own.
- That makes the Face row and Visibility's cluster / solo chips the same thing as Pin - one of them
  goes, or shows what the pins chose, greyed (an option never moves).
- A solo group card's tap offers Group view; whether it is the default (Bedroom's tap toggling the
  lights today) is the owner's call.
- It replaces 11-06's "a cluster with nothing pinned shows its header alone" (`CardPageGroups.cpp`).

**From the glass**
- A group of temperatures on All members shows its first member; the owner wants an average, a
  chosen member, and a History of every member that can be shown or hidden - K182's work.
