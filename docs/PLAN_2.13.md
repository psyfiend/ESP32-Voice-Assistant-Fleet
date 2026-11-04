# 2.13 - what is left, in order (written 2026-11-04, after round 4)

**The short-term roadmap for the rest of 2.13**, so nothing discussed in the long 11-02 to 11-04
session is lost. Check items off here as they land; this file is archived beside `TEST_2.13.md`
when 2.13 merges. Decisions still go to `DECISIONS.md`, test rounds to `TEST_2.13.md`, the design
to `docs/design/ha-cards.md` - this file only orders the work and says where each piece stands.

**Where it stands:** branch `feat/2.13-add-from-ha`, four rounds built (K138-K164), round 4 at
`5fbc9ee` on the P4_5, the 4B and the 1060. The CYD_P4_4880 runs round 2. **Round 4 is untried on
the glass** (taps, swipes, Save N). Nothing pushed.

## 0. First: the owner's round-4 test

`TEST_2.13.md` round 4, T1-T13, plus the "couple of minor bugs" the owner mentioned on 11-04 but has
not listed yet - ask for them before building round 5.

## 1. Round 5 - the owner's visual list and small fixes

Agreed in direction; each is small. Where the cause is already known, it is given.

| # | Item | What is known |
|---|---|---|
| 1.1 | **The pin is black** in Members and Edit cards | `pinGlyph()` (`Popup/PopupMembers.cpp:96`) and Edit cards' sign (`UI/EditCards.cpp:92`) set no color, so the pin takes the icon role's default. Pick its color with the owner (the header glyphs carry their own tint) |
| 1.2 | **Corner cut on Linen in the group view** (Cards mode): the cut corners show the wrong color | `Cards/CardFrame.cpp:418` paints a cut corner in the page's `GROUND` unless the card is a mini tile; a group view card is a full card standing on the cluster's `SURFACE`. The corner wants the color actually behind the card |
| 1.3 | **The header sensors' drop-down animates** as the system header's slot panels do | `dropOpen()` in `Popup/PopupGroup.cpp` builds a plain panel that appears at once; the system header's panels are `Kit::Drawer` |
| 1.4 | **Long press on a group view tile or card** animates as it does on the page | the page's long-press feedback is not on the group view's members |
| 1.5 | **Edit cards, an HA area's rows for cards already here** (on the page or in the group) still use the old flat "In this group" style; they should show the square and take the tap cycle (K161) | `fillBody()` in `UI/EditCards.cpp` draws the cycle only for the In this group / On the page lists |
| 1.6 | **No panels under Edit cards opened from a group**, as from the page - more room | |
| 1.7 | Round 4's gaps: **Visibility's Add to group joins unpinned** (K160: only Edit cards does today); **a tap outside the header drop-down folds it** before anything else | `TEST_2.13.md` round 4, "Not built yet" |
| 1.8 | Round 1's knowns, to recheck on the glass | a humidity's decimal (43.0 %: 3.2 below), the 4B's °F over a number (likely gone with the capped tiles), HA's long names wrapping in tiles |
| 1.9 | The 4B stopped answering HTTP for minutes after a settings restore's reboot (11-04); opening its serial port reset it | watch for it; nothing to fix yet |
| 1.10 | **Drift the structure check found in 2.13's code** (11-04) | four millimetre numbers where a `UIShape` token belongs: `PopupGroup.cpp:168` `mm(100)`, `EditCards.cpp:307` `mm(80)` and `:560` `mm(100)`, `HaPicker.cpp:120` `mm(80)`; `UI/HiddenSheet` is no longer reachable on the glass (Edit cards replaced it) - remove it or keep it on purpose. Files over 1,000 lines: `GUIManager.cpp` 1,529, `Card.cpp` 1,462 (unchanged in 2.13) |

## 2. To settle with the owner before building

| # | Question | Notes |
|---|---|---|
| 2.1 | **Corner icons on cards: still needed?** | the corner icon was the card's kind (cards.md); since 2.11 a tile and a solo card share one icon size, and value cards squeeze their value to clear it (#73's half) |
| 2.2 | **GROUP panel's Show as and Rename group move to Visibility?** | K155 says a group's options never move between presentations; this would be a deliberate one-time move, so the GROUP panel and Visibility each need one clear job |
| 2.3 | **A GROUP panel on views that have none** (History, Scenes)? | today those views show no panels |
| 2.4 | **"Garage - Doors" becomes an ordinary group** | still the bespoke card K98 left open |
| 2.5 | **How many made cards?** `CardAdded::MAX` is 64 | the owner asked on 11-03; 128 costs about 40 KB of PSRAM. Hidden page cards do not count |
| 2.6 | **Which unit is shown** (°F / °C) - the owner: never assume, and let the user choose | with 3.2; a device-wide choice in UI Tweaks is the obvious home |
| 2.7 | "Create N" or "Add N" for a new group; label and group words | judge on the glass; re-read every on-screen "group" against K146 / K158 once more |

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
   Needs a decision first.
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
- Mirroring the boards' groups as HA group entities (owner's idea, 11-03; deferred).
- The old cell pinning, shelved (K164, `FUTURE_IMPROVEMENTS.md`).
- **Voice and the ESPHome API** (A18, Phase 7, `design/voice-esphome.md`).
- Look at NINA's repo now and then for ideas (`REFERENCE_PROJECTS.md`).
- **#73** stays open: chrome text still comes from two fixed sets chosen by `bspUiScale() >= 1.3`
  (`UI/UIToolkit.cpp:51`), and value cards still pick VALUE or VALUE_SM each on their own.
