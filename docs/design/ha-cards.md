# Cards from Home Assistant, groups and Edit cards

**How the dashboard gets its cards from Home Assistant, and what the glass does with them.**
Decisions: DECISIONS K130-K200 (the Cards and the popup table). Issue #116. What is not built, and
the open questions: ROADMAP's "Phase 2 summary" and `carried-from-2.13.md`. The group model behind
this is `group-cards.md`; the window is `card-sheet.md`.

Nothing from HA is added by itself (K133): HA offers entities, the owner picks them.

## 1. The foundation

### 1.1 Subscribing to more than ~45 entities

`HaProvider::sendSubscribe()` builds one `subscribe_trigger` per 1,536-byte frame (about 45 typical
entity ids) and sends as many frames as the list needs, each starting where the last stopped; HA
keeps every one. The scene-and-area template's reply is copied into a PSRAM buffer sized to it, and
its request is built in a larger one.

### 1.2 Room

| Limit | Value | Notes |
|---|---|---|
| `ENTITY_MAX` (registry slots, PSRAM, ~1 KB each) | 192 | the index type allows 255 |
| `HaPublisher`'s per-slot arrays | PSRAM | they grow with the registry |
| `CardAdded::MAX` (cards made on the board) | 128, PSRAM | K176; hidden page cards do not count |
| HA catalogue | 48 areas, 160 entities | `HaCatalog.h` |
| Websocket receive buffer | 48 KB in PSRAM (8 KB internal fallback) | `HaDefaults.h` |

### 1.3 An entity made at runtime

An entity HA has and the compiled table does not is made the way learnt group members are (A13):
its id from HA's (`switch.office_3d_printer` -> `ha_switch_office_3d_printer`), its kind from the
domain (light; switch / input_boolean / fan -> switch; binary_sensor; sensor; climate; scene ->
button). **It is kept on the card that uses it**: the card entry has `ha` (HA's entity id) and, from
the catalogue at the moment it was added, `ha_dc` (device class) and `ha_unit`. At boot,
right after the compiled HA table, `SystemCore` makes the entity of every card entry with `ha`
(`adoptHaCard`, `Settings::forEachCardWith("ha", ...)`), so the page finds it when it is built;
HaProvider subscribes it and HaRest fetches its value as for any other. An entity the board already
has (compiled or learnt) is used as it is, with no `ha` key. An HA light a card was made from is
`Entity::adopted`, so the scene query asks it.

### 1.4 The catalogue: what HA offers

One `render_template` per request, answered once and cancelled (the scene query's pattern):
`label_entities('fleet')` with each entity's area id and name, its area's floor, its name, device
class, unit and state, and the floors' and areas' order. Only entities labelled `fleet` are offered
(K140). It is kept in PSRAM, read by the pickers under its own lock, and asked again when a picker
opens and when HA says its entity or area registry changed (`subscribe_events` for
`entity_registry_updated` and `area_registry_updated`, debounced).

**New in an area** (K142): the ids the board has seen are kept in the settings file
(`system.ha.known`). An id in the catalogue and not known, in an area a group is linked to, is new:
a notice once ("New in Office: 3D Printer"), an "N new" mark on that group's Edit cards, New on its
row, until Edit cards has shown it. The first catalogue a board reads knows everything.

### 1.5 The notice queue (K142)

`UIToolkit::show_toast()` queues notices instead of replacing the one showing: each is shown for its
time with a bar that runs down, the next after it (NINA's toast idea). Only info notices exist;
`UIToolkit::show_sticky` is a notice that stays until tapped. Alert levels will use the same queue.

## 2. The dashboard is the settings file (K171)

The compiled cards are retired. The House page compiles only Garage - Doors; the Fleet page only the
**Panel** group (Free Heap, Uptime, IP Address, Signal), `CardSpec::builtIn`: never hidden or
removed, its HA discovery unchanged. Every other card is made on the board (`CardAdded`) and kept in
the settings file with its id. A board with no file shows Panel and Doors. The compiled entity table
(`ExternalEntities_HA.h`) stays, because made cards name its entities. `CardGroups` reads the made
cards' areas (`CardAdded::specAt`), so area groups keep their colors and card labels.
`scripts/release/retire_compiled.py` converts an older settings file;
`scripts/release/test_cards.json` holds the test cards; the dev boards have an MQTT group (the deck
sensor) in their files.

## 3. What the glass does

### 3.1 Group and label (K146, K158)

A **group** is the bordered block - shown as a group on the page, or as one card - and its members
share its name and color; renaming a group renames it, one block per group. A **label** is a page
card's colored name (Card label, Change label, "Select label for these cards:"): its HA area's by
default; it may match any group's name and color, or be None; a group's rename or recolor changes
every label that matches it. The window's group panel is **GROUP**. On screen the word is group; the
code type is `ClusterCard`.

### 3.2 The group view (K154, K159, K162, K163)

- A **tap on a group** runs its Tap action, by default Group view (`TAP_GROUP`); a **long press**
  runs Long press opens, by default Members. A group keeps both separately as a group on the page
  (`tap`, `long`) and as a solo card (`card_tap`, `card_long`) (K156).
- The view is **the group's own cluster** - `CardPage::makeGroup(id, true)`, a real `ClusterCard` -
  in a **bare sheet** (`Kit::sheetBare()`: no title row, no X, the window's corners, its shadow let
  out by `UI::shadowReachPx()`). It closes on a tap outside, its countdown, or a drag down from its
  header. The header's name opens Members; its sensors drop a panel listing each (glyph, full name,
  state), a row opening that member as its tile would (`ClusterCard::setSlotsTap`). The panel
  slides; a tap elsewhere folds it first. A long press on a tile presses it in as on the page. A
  History opened from the panel (or by Long press opens) goes back to where it was opened from
  (K169, `markDirectView()`).
- **Every member alike, the pinned first.** Tiles have a maximum size, `C_TILE_MAX_W` x
  `C_TILE_MAX_H` (25 x 20 mm), never wider than `C_TILE_WIDE` (2) times their height, every tile one
  size, the spare room spread evenly between and around them (`Layout::cappedGrid` /
  `cappedTileAt`, host-tested). The window keeps the normal card window's size and place unless its
  tiles would be smaller there than the page's; then it grows, up to the page area.
- **Tiles or Cards**: UI Tweaks' Group view (`system.ui.group_view`) for the device, a group's own
  in its Card UI's Group view section as **Card appearance** (Inherit / Tiles / Cards, `group_view`,
  greyed while the group is a solo card). Cards are members drawn as solo cards without card labels,
  on the same grid, **on the page's ground** with the page's card color (K173), in the group's own
  Card state. Tiles keep the cluster's surface.
- **Card margins** (K167): UI Tweaks' (`system.ui.margins`: small / medium / large / xlarge, 50 /
  100 / 150 / 200 % of the gap) for the page and every group view; a group's own (`margins`) in the
  same section, for its group view only.
- The GROUP and SETTINGS panels stay under it (K155, K175).
- **A member opened here** has its own SETTINGS live - Tap action, Long press opens, Card UI, Units,
  Active state - saved on its own card (K166, `settingsSubject()`).
- Code: `Cards/Popup/PopupGroup.cpp` (`groupCap()`, `fitted()`, `place()`, the swipe, the drop-down).

### 3.3 Pinned members (K160, K164)

The page shows a group's **pinned** members - its tiles and its header sensors. Saved: a member's
`hidden`: `yes` means unpinned; header members are the group's `slots`. **New members join
unpinned**, so adding never rearranges the page (New group pins what it adds, K190). Members and
Edit cards end a member's row in a square: its header glyph, or a pin (`MDI_PIN`, the accent, K178)
for a tile, nothing when unpinned. A tile's Visibility says **Pin this card** / **Unpin this card**,
each asking first (K165) and closing the window (K196). "Pin" means on the page; pinning a card to a
grid cell is shelved (K164, `FUTURE_IMPROVEMENTS.md`).

### 3.4 Edit cards (K148, K149, K152, K161, K165)

One window over the page, from the page panel's **CARDS** (Edit cards, New group) or a group's
**GROUP > Edit cards**. Its column: In this group (from a group), On the page, Hidden, then HA's
areas by floor; a group's own HA area lists first. **The lists show the result as you tick** -
dashed rows coming, red rows going; each row says what is already held (In this group, In the
header, On the page).

- **From the page**: one kind of tick at a time, with a sentence saying why when another is tapped;
  Add N, Hide N, Remove N (cards made on the board), Restore N. Every choice is made before a
  go / no-go dialog in the dialogs' sentences (K149); adding from HA goes through Add to dashboard's
  label step, with **Area** (each card in its own HA area's group).
- **From a group**: the tap cycle (K161) - a member's pin flips, a second tap marks a made card for
  removal (red), a third puts it back (built-in members, Garage - Doors' and Panel's, only flip); a
  card not in the group is added, then added pinned, then - **a sensor only** - into the header
  (its glyph in the square), then not. Adding from HA has no Copy or Move; page cards offer both.
  One **Save N**, always asked (a line per kind of change), then the toast "Saved N changes to
  Kitchen" (`editAct()` / `doEditSave()` in `Popup/PopupCluster.cpp`). A page card that came out of
  this group goes home (`CardAdded::moveBack`). No panels under it. An HA area's rows for cards
  already here take their own list's square and cycle.
- Hidden and removed differ: a hidden member still shows in the group view, a removed card nowhere.
  Delete stays the web app's (K117).
- **No check mark** on a chosen row - the lit chip says it, and nothing in a row moves.
- **The limit** (K176): within 25% of 128 made cards, Edit cards opens with a toast that stays until
  tapped; at the limit no row can be chosen to add.
- Names are fitted to their row (K152, `Kit::fitLabel`): the largest linked face that fits, else cut.
- Code: `UI/EditCards.cpp` (the list, `Spec::cycle`, `saveWord`, `haGlyph`), `UI/EditCardsSheet.cpp`
  (from the page), `UI/HaOffer` (what HA offers, "N new"), `UI/HaPicker`,
  `UI/NewClusterSheet.cpp`.

### 3.5 New group (K168, K190, K191)

Full size: "Model this group after an existing HA area?" (HA's areas, Create new); then Edit cards with
the new group's own list first ("Office (new)" or "New group"), where the first tap on a card pins
it and a sensor's next tap puts it in the header; **Create group N**; its name ("The group will be
named '...'. Do you want to enter a different name?", the name or Change name - the keyboard,
"New group name" - and Next; "New group" when the area's name is taken); and "Create new group?".
With fewer than two pinned the group is created shown as a solo card and asks its face and tap the
same way as Show as. A group modelled after an area is linked to it (K141): the area's own group
(`ha_<area>`) unless that is taken, else a group of its own (`CardGroups::make`). A group made on
the board never names an HA area (K199).

### 3.6 A group's options (K155, K157, K174, K175, K188, K191, K193)

**GROUP holds what concerns its members**: Header options, Edit cards, Face, Area. **Visibility
holds how it stands on the page**: Show as... (Solo card or Group), Rename group, Hide from this
page. GROUP is on every view of a group except History and Scenes, and not under its Edit cards. A
row that does not apply is greyed (`ChipRowSpec::greyed`); an option never moves with the card's
state (K155). Long press opens: Group view, Members, Details or History - never Toggle.

A group shown as its solo card is marked on the page with a small group glyph after its label's
words (K193). Fewer than two pinned members shows the group as its solo card; its Members lists
every member, header sensors too, and a member that is itself a group (Desk) opens as its tile
does, "Office > Desk" with its own Members (K157). A solo card's tap defaults to Toggle when its
members are lights and switches, Members when mixed (K188).

### 3.7 The thermostat, read-only (K144)

An entity with no card type of its own is a read-only card. The thermostat's window
(`Popup/PopupClimate.cpp`) shows the room's temperature large with what it is doing under it, and
beside them its mode, setpoint ("Heat to 68°"), what it is doing, humidity and fan, each only when
HA says it, with History as its tab. No setpoint or mode is sent; controls wait for the card
library (`carried-from-2.13.md` §6).

## 4. Saved keys

`ha` / `ha_dc` / `ha_unit` on a card made from HA; `system.ha.known`; a group's HA area link
`groups.<id>.ha_area`. A group's own keys: `present`, `slots`, `face`, `tap` / `long`,
`card_tap` / `card_long`, `card_state`, `group_view`, `margins`. A card made on the board: `page`,
`after`, `entity`, `area`, `pri`, `name`, `label`; a group made here has `kind`: `cluster`; its
members `in`. The full picture of the file is `card-sheet.md` §17 and `Settings.h`.
