# 2.13 - Cards from Home Assistant

**What 2.13 builds, and how.** Decisions: DECISIONS K130-K164 (the owner's, 2026-10-30 to 11-04).
The look: artifact "2.13 Add Cards Mocks" (six screens, round 1), then "2.13 Round 2 Mocks"; the
alerts it was designed with (2.14): artifact "2.13 Alert Flow Mocks", K138-K139. Issue #116.

**§1-3 are round 1 as designed and built (2026-11-02). Rounds 2-4 revised screens 1-5 heavily -
read §4 for what the glass does now**, and `docs/PLAN_2.13.md` for what is left.

## 1. What the owner sees

| Screen | What | Decision |
|---|---|---|
| 1 | A cluster's **Add cards** with a source choice, On this board / From Home Assistant: its area's `fleet` entities first, each row saying what is already held, then Other areas | K140 |
| 2 | The page panel's **Add card**: every area (HA's floors as sections, No area last) beside its entities | K140 |
| 3 | The page panel's **New cluster**: K91's flow from an area - Tiles and Header preselected, a name, Create | K140, K91 |
| 4 | The CLUSTER panel's **Area** row (the group's link); "1 new" on Add cards; the notice "New in Office: 3D Printer" | K141, K142 |
| 5 | **The group view**: a tap on a cluster; every member, hidden ones last and marked; fills the page area; list and grid buttons between it and Members | K143 |
| 6 | **The thermostat**, read-only: card and window. **Still read-only** - no setpoint is sent (`Popup/PopupClimate.cpp`); its controls are `PLAN_2.13.md` §3.1 | K144, K153 |

Words: K145. Nothing from HA is added by itself (K133).

## 2. The foundation (Claude's design, from the code as of `v0.2.20`)

Read from the code by an explorer, 2026-11-02; the numbers are the code's.

### 2.1 Subscribing to more than ~45 entities

`HaProvider::sendSubscribe()` builds one `subscribe_trigger` in a 1,536-byte frame and refuses the
whole subscription when the list does not fit - about 45 typical entity ids. The owner labels 70
entities `fleet`. **The subscription is sent in chunks**: one `subscribe_trigger` per frame-full, the
next starting where the last stopped (HA keeps every one), so any number of entities is subscribed.
The scene-and-area template's 4 KB reply buffer and 3 KB request get the same treatment: the
reply is copied into a buffer sized to it (PSRAM), the request built in a larger one.

### 2.2 Room

| Limit | Was | Now | Why |
|---|---|---|---|
| `ENTITY_MAX` (registry slots, PSRAM, ~1 KB each) | 128 | 192 | 38 at boot + 70 `fleet` + learnt members and scenes; the index type allows 255 |
| `HaPublisher`'s per-slot arrays | internal RAM, ~62 B x `ENTITY_MAX` | PSRAM | they grew with the registry on boards with 20 KB to spare |
| `CardAdded::MAX` (cards made on the board) | 24, internal RAM | 64, PSRAM; **128 since round 5 (K176)** | Add card from HA makes cards by the handful; since K171 every card is one |

### 2.3 An entity made at runtime

An entity HA has and the compiled table does not is made the way 2.10c's learnt members are
(A13): its id from HA's (`switch.office_3d_printer` -> `ha_switch_office_3d_printer`), its kind from
the domain (light, switch / input_boolean / fan -> switch, binary_sensor, sensor, climate, scene ->
button). **It is kept on the card that uses it**: the card entry gets `ha` (HA's entity id) and,
from the catalogue at the moment it was added, `ha_dc` (device class) and `ha_unit`. At boot,
right after the compiled HA table, every card entry with `ha` makes its entity, so the page finds it
when it is built; HaProvider subscribes it and HaRest fetches its value as for any other. An entity
the board already has (compiled or learnt) is used as it is, with no `ha` key.

### 2.4 The catalogue: what HA offers

One `render_template` per request, answered once and cancelled (the scene query's pattern):
`label_entities('fleet')` with each entity's area id and name, its area's floor, its name, device
class, unit and state, and the floors' and areas' order. Kept in PSRAM, read by the pickers under its
own lock; asked again when a picker opens and when HA says its entity or area registry changed
(`subscribe_events` for `entity_registry_updated` and `area_registry_updated`, debounced).

**New** (K142): the ids the board has seen are kept in the settings file (`system.ha.known`). An id
in the catalogue and not known, in an area a cluster's group is linked to, is new: a notice once,
"1 new" on that cluster's Add cards, New on its row - known once Add cards has shown it. The first
catalogue after this build knows everything (nothing is new on day one).

### 2.5 The notice queue (K142)

`UIToolkit::show_toast()` replaces whatever is showing. Notices are queued instead, each shown for
its time with a bar that runs down, the next one after it - NINA's toast idea, info notices only in
2.13; 2.14 adds the alert levels on the same queue.

## 3. Build progress

Branch `feat/2.13-add-from-ha`, off `main` at `fd3d59e`. Unattended run from 2026-11-02 05:30, with
the owner's permission to flash the three dev boards (settings saved and restored), toggle Office
lights, and reword HA's docs for K133.

- [x] 2.1 subscription in chunks; the template buffers (`94cb6df`)
- [x] 2.2 room: registry 192, HaPublisher to PSRAM, CardAdded 64 in PSRAM (`94cb6df`)
- [x] 2.3 runtime entities from card entries (`94cb6df`)
- [x] 2.4 the catalogue, registry events, known ids, the notice queue (`61a088b`; `/popup?catalog=1`
  checked on the P4_5 - 70 entities, the Outside group linked to HA's Outside floor)
- [x] Screen 1 - a cluster's Add cards from HA (`161cf8d`; checked on the P4_5, survives a reboot)
- [x] Screen 2 - Add card (`16e497a`; checked on the P4_5)
- [x] Screen 6 - the thermostat (`4e98c4e`; checked on the P4_5)
- [x] Screen 4 - the Area row and "1 new" (`55d114c`; the row checked on the P4_5, "1 new" needs a
  change in HA - TEST T8)
- [x] Screen 3 - New cluster (`dcbd271`; checked on the P4_5)
- [x] Leftovers: Show helper text (`9f7ec11`), a cluster's card state (`a3eef7a`), HA's docs for K133
  (HA repo `d8293c5`)
- [x] The P4_5, 4B and 1060 flashed with round 1 and checked; the P4_5's settings put back (one
  difference: `system.ha.known`)
- [x] `docs/TEST_2.13.md` written
- [x] Screen 5 - the group view, tap and long press (`17b4dbe`; checked on the P4_5)
- [x] Round 2 (`c453a92` to `d8d3d26`, K146-K153), round 3 (`1edb552` to `ab4f396`, K154-K159),
  round 4 (`5fbc9ee`, K160-K164) - §4. What is left: `docs/PLAN_2.13.md`.

## 4. As built after round 5 (2026-11-05, `835b870`)

What the glass does now; the decisions hold the reasons, `TEST_2.13.md` the rounds.

### 4.0 The dashboard is the settings file (K171)

The compiled cards are retired. The House page compiles only Garage - Doors (its summary is a
compiled card's, until 2.14); the Fleet page only the **Panel** group (Free Heap, Uptime, IP
Address, Signal), `CardSpec::builtIn` - no Hide anywhere. Every other card is made on the board
(`CardAdded`), with the id it had when compiled. The compiled entity table (`ExternalEntities_HA.h`)
stays: made cards name its entities. `scripts/release/retire_compiled.py` converts a settings file
from before round 5; `scripts/release/test_cards.json` holds the retired test cards; the dev boards
have an MQTT group (the deck sensor). `CardGroups` reads the made cards' areas (`CardAdded::specAt`),
so area groups keep their colors and card labels.

### 4.1 Group and label (K146, K158)

A **group** is the bordered block - a cluster, or a group shown as one card - and its members share
its name and color; renaming a group renames it. A **label** is a page card's colored name (Card
label, Change label, "Select label for these cards:"). The window's group panel is **GROUP** (it was
CLUSTER).

### 4.2 The group view (K154, K159, K162, K163)

- A **tap on a cluster** runs its Tap action, by default Group view (`TAP_GROUP`); a **long press**
  runs Long press opens, by default Members (K151). A group card keeps both separately as a cluster
  (`tap`, `long`) and as a solo card (`card_tap`, `card_long`) (K156).
- It is **the group's own cluster** - `CardPage::makeGroup(id, true)`, a real `ClusterCard` - in a
  **bare sheet** (`Kit::sheetBare()`: no title row, no X, the window's corners, its shadow let out by
  `UI::shadowReachPx()`). It closes on a tap outside or a drag down from its header. The header's
  name opens Members; its sensors drop a panel listing each (glyph, full name, state), a row opening
  that member as its tile would (`ClusterCard::setSlotsTap`). The panel slides; a tap elsewhere folds
  it first. A long press on a tile presses it in as on the page. A History opened from the panel (or
  by Long press opens) goes back to where it was opened from (K169, `markDirectView()`).
- **Every member alike, the pinned first.** Tiles have a maximum size, `C_TILE_MAX_W` x
  `C_TILE_MAX_H` (25 x 20 mm), never wider than `C_TILE_WIDE` (2) times their height, every tile one
  size, the spare room spread evenly between and around them (`Layout::cappedGrid` /
  `cappedTileAt`, host-tested). The window keeps the normal card window's size and place unless its
  tiles would be smaller there than the page's, and then grows, up to the page area.
- **Tiles or Cards**: UI Tweaks' Group view (`system.ui.group_view`), a group's own in its Card UI's
  **Group view** section as **Card appearance** (Inherit / Tiles / Cards, `group_view`, greyed while
  it is a solo card). Cards are members drawn as solo cards without card labels, on the same grid,
  **on the page's ground** with the page's card color (K173), in the group's own Card state.
- **Card margins** (K167): UI Tweaks' (`system.ui.margins`: small / medium / large / xlarge, 50 /
  100 / 150 / 200 % of the gap) for the page and every group view; a group's own (`margins`) in the
  same Group view section, for its group view only.
- The GROUP and SETTINGS panels stay under it (K150, K155, K175).
- **A member opened here** has its own SETTINGS live - Tap action, Long press opens, Card UI, Units,
  Active state - saved on its own card (K166, `settingsSubject()`).
- Code: `Cards/Popup/PopupGroup.cpp` (`groupCap()`, `fitted()`, `place()`, the swipe, the drop-down).

### 4.3 Pinned members (K160, K164)

The page shows a group's **pinned** members - its tiles and its header sensors. Saved: a member's
`hidden`: `yes` means unpinned; header members are the group's `slots`. **New members join
unpinned**, so adding never rearranges the page. Members and Edit cards end a member's row in a
square: its header glyph, or a pin (`MDI_PIN`, the accent, K178) for a tile, nothing when unpinned.
A tile's Visibility says **Pin this card** / **Unpin this card**, each asking first in the owner's
words (K165); Visibility's Add to group joins unpinned too. "Pin" means on the page; the old
pinning to a grid cell is shelved (K164, `FUTURE_IMPROVEMENTS.md`).

### 4.4 Edit cards (K148, K149, K152, K161)

One window over the page, from the page panel's **CARDS** (Edit cards, New group) or a group's
**GROUP > Edit cards**; it replaced round 1's Add card, a cluster's Add cards and the page panel's
HIDDEN section. Its column: In this group, On the page, Hidden, then HA's areas by floor. **The
lists show the result as you tick** - dashed rows coming, red rows going.

- **From the page**: one kind of tick at a time; Add N, Hide N, Remove N (cards made on the board),
  Restore N; every choice made before a go / no-go dialog in the dialogs' sentences (K149) -
  Restore too (K165); adding from HA goes through the label step. Panel's row says "Built in".
- **From a group**: the tap cycle - a member's pin flips, a second tap marks a made card for removal
  (red), a third puts it back (compiled members only flip - since K171 only Garage - Doors' and
  Panel's); a card not in the group is added, then added pinned, then - **a sensor only** - into the
  header (its glyph in the square, K168), then not. Copy / Move for page cards. One **Save N**,
  **always asked** (K165: a line per kind of change), then the toast "Saved N changes to Kitchen"
  (`editAct()` / `doEditSave()` in `Popup/PopupCluster.cpp`). A page card that came out of this
  group goes home (`CardAdded::moveBack`). No panels under it; it reaches the screen's foot (1.6).
  An HA area's rows for cards already here take their own list's square and cycle (1.5).
- **No check mark** on a chosen row - the lit chip says it, and nothing in a row moves (K168).
- **New group** (K168, `UI/NewClusterSheet.cpp`): full size; "Model this group after an existing HA
  area?" (HA's areas, Create new); Edit cards with the new group's own list first ("Office (new)" or
  "New group"); **Create group N**; its name ("The group will be named '...'. Do you want to enter a
  different name?", the name or Change name - the keyboard "New group name" - and Next); "Create new
  group?". A group modelled after an area is linked to it (K141): the area's own group (`ha_<area>`)
  unless that is taken, else a group of its own (`CardGroups::make`).
- **The limit** (K176): 128 cards made on a board; within 25% of it Edit cards opens with a toast
  that stays until tapped (`UIToolkit::show_sticky`); at the limit no row can be chosen to add.
- Names fitted to their row (K152, `Kit::fitLabel`): the largest linked face that fits, else cut.
- Code: `UI/EditCards.cpp` (the list, `Spec::cycle`, `saveWord`, `haGlyph`), `UI/EditCardsSheet.cpp`
  (from the page), `UI/HaOffer` (what HA offers, "N new"), `UI/HaPicker`, `UI/NewClusterSheet.cpp`.

### 4.5 A group's options (K151, K155, K157, K174, K175)

**GROUP holds what concerns its members**: Header options, Edit cards, Face, Area. **Visibility
holds how it stands on the page**: Show as (Cluster, Solo card), Rename group, Hide from this page
(K174). GROUP is on every view of a group except History and Scenes, and not under its Edit cards
(K175). A row that does not apply is greyed (`ChipRowSpec::greyed`); an option never moves with the
card's state. Long press opens: Group view, Members, Details or History - never Toggle. A solo group
card's Members lists every member, its header sensors too; a member that is itself a group card
(Desk) opens as its tile does, "Office > Desk" with its own Members.

### 4.6 Limits and saved keys

| What | Number | Where |
|---|---|---|
| Entities in the registry | 192 | `EntityRegistry.h:57` (`ENTITY_MAX`) |
| Cards made on the board | 128 (K176) | `CardAdded.h` (`CardAdded::MAX`); hidden page cards do not count |
| HA catalogue | 48 areas, 160 entities | `HaCatalog.h:40-41` |
| Ids per subscribe frame | ~45 (1,536-byte frame), sent in parts | `HaProvider.cpp:44` |
| Websocket receive buffer | 48 KB in PSRAM (8 KB internal fallback) | `HaDefaults.h:119-121` |

A card made from HA carries `ha` (the entity id), `ha_dc` (its device class) and `ha_unit`; at boot
`SystemCore` creates its entity from those (`adoptHaCard`, `Settings::forEachCardWith("ha", ...)`).
The ids HA has offered are `system.ha.known`; a group's HA area link is `groups.<id>.ha_area`. A
group card's own keys: `present`, `slots`, `face`, `tap` / `long`, `card_tap` / `card_long`,
`card_state`, `group_view`, `margins`. A card made on the board: `page`, `after`, `entity`, `area`,
`pri`, `name`, `label`; a group made here `kind`: `cluster`; its members `in` (K171's conversion
writes these, `retire_compiled.py`). An HA light a card was made from is `Entity::adopted`, so the
scene query asks it (1.12).
