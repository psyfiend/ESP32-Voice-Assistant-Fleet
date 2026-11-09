# Groups, group cards and clusters

**The group model: what a group, a group card and a member are, how a set of entities is commanded,
and how it is shown.** Built (the compiled group cards are retired; groups are cards made on the
board, K171). Decisions: K46, K47, K67, K88-K92, K114-K116, K129, K146-K200 in `DECISIONS.md`.
What the glass does with groups, step by step: `ha-cards.md` §3. Placement of the blocks on a page:
`layout-engines.md`; the cluster header and tiles as components: `components.md`. Related:
`card-sheet.md` (the window), `ha-websocket.md` (area resolution).

## 1. Words

On screen the word is **group** (K191); the code's type names stay `ClusterCard` and "cluster".

| Word | Means |
|---|---|
| **Entity** | a light, sensor or switch in HA, MQTT or virtual; in HA it has at most one area |
| **Card** | the dashboard's object for one entity (a solo card) or a set (a group card); it has an id (K33) and its own settings |
| **Group** | a name and a color, device-wide (`CardGroups`), that cards share; it may be linked to HA areas. A card is in exactly one group |
| **Label** | the colored name on a solo card (K42's card label): its group's name or its own words, in its group's color (K146, K158) |
| **Group card** | one card standing for a set of entities, with a **source** and a **presentation** |
| **Member** | a card inside a group card, on no page of its own; it wears its group card's group |
| **Pinned** | a member shown on the page (a tile or a header sensor); an unpinned member is in the group view and Members only (K160) |

"Area" is HA's word only (K88): an entity's HA area becomes a group when the entity comes onto the
board. The group's id is `ha_<area_id>` (`CardGroups.h`), and HA keeps an area's id when the area
is renamed, so the link survives a rename at either end.

## 2. Source: where the set comes from

| Source | Defined by | Members | One tap acts on all of them by |
|---|---|---|---|
| **A group entity** (a Hue room, an HA group helper) | Hue / HA | the entity's `entity_id` attribute; the registry learns each as an entity of its own (A13) | one call to the group entity; HA and Hue do the rest |
| **A list made on the board** | the owner, on the glass | the list itself | one HA call with the entity ids (`light.turn_on`; `homeassistant.turn_on` / `turn_off` across domains); only non-HA members (MQTT, virtual) are commanded one by one |

- **HA areas are never commanded** (K67): an area may hold entities that are not on the dashboard
  (a Kitchen area with a TV). "The Kitchen lights at one tap" is a group card of exactly those
  lights.
- **A group defined at the source is read-only on the board** (K179): a member can be pinned or
  unpinned, never added or removed, and a group made in the Hue app is never edited from a board. A
  member can be opened and tapped. Extending such a group, and editing an HA group through a mirror
  on the board, are not built (`carried-from-2.13.md` §1).
- A group entity is paused when all its members are, in step both ways (K32); while one member is
  paused the group is commanded through its other members one by one (`card-sheet.md` §15, §16).

## 3. Presentation: how the set is shown

A group card is shown as one of two things, chosen in Visibility's **Show as...** (K191):

- **Group** (the cluster): a bordered block on the page holding the pinned members as mini tiles
  under a header; at least two members must be pinned.
- **Solo card**: one card with a face (a member's icon and state, or the group's own for a group
  made here) and a tap action. **Fewer than two pinned members shows the group as its solo card**
  (K188). It is marked with a small group glyph after its label (K193).

Switching keeps the members, pins and settings; each presentation keeps its own Tap action and
Long press opens (`tap`, `long` / `card_tap`, `card_long`, K156). A cluster that cannot fit a small
screen is drawn as its 1x1 card by the arranger.

A **summary card** (one icon for members of one kind, its label naming the active ones) is its own
kind of card, not a presentation (K189); only the compiled Garage - Doors is built
(`PRESENT_SUMMARY`; `carried-from-2.13.md` §3). **Group pages** (a group as a whole page) are not
built; the group view is the window version (K118, K154).

### 3.1 The cluster

- **Mini tiles** (K89, K116): a member drawn without its own border on a faint panel inside the
  cluster; a tile that is on is framed as a solo card is (Corner cut by default); a state's "on" is
  the lit disc. Tiles are drawn near square, the hero and label centred, one height per page; a
  solo card beside tiles is a tile's height.
- **The header** (K101, K115): one line - the group's swatch and name on the left, header sensors on
  the right, in one order everywhere: temperature, occupancy, light level, then count or alert;
  icons with text where there is room, text with units where not; occupancy has its own icon per
  state. **Four sensors at most** (`CLUSTER_SLOTS_MAX`), chosen in Header options; only sensors can
  be chosen; a member in a slot is not also a tile. Style **Swatch** or **Band** (K114): Swatch
  sits on a faint band, Band fills the row with the group's color. The page's label type applies to
  solo cards only; a cluster has its own page-wide style and each cluster may inherit it (K102).
  Placement uses the page header's planner of claims (`Layout/HeaderPlan.h`).
- **Taps** (K107, K110, K129, K156): a tap runs the cluster's Tap action, by default Group view; a
  long press runs Long press opens, by default Members. A tap on a tile acts on that member; a long
  press on a tile opens the member's own window. The cluster's window is its Members, with the GROUP
  panel (Header options, Edit cards, Area) beside SETTINGS.
- **One-cell clusters** (K103) are for sensors only, the whole cell one tap target; never two or
  three tap targets in one cell. Count sensors (lights on, doors open) and alerts come from HA
  template sensors and belong in header slots (K119, K134).
- **Size** is `Layout::clusterSpan()` cells (`include/Layout/Cluster.h`, host-tested); tiles stand
  where `Layout::tileGrid()` puts them.

## 4. The rules

1. **Groups are exclusive: a card is in exactly one** (K92). A grouping by function ("Temp Sensors")
   is a group card whose members are copies of cards that live elsewhere; copies are independent
   (K33). The price is that one sensor can wear two colors on a dashboard: the color says "the set
   you put it in". A card in several groups would need a rule for which color it wears, and
   whichever it wore would mislead.
2. **A member wears its group card's group** (K92): Desk's lamps are Desk's members and wear
   Office; "Desk" is the card's own words. Moving the group card moves its lamps with it. Mini tiles
   show no label; the header shows the name and color once.
3. **One level of nesting**: a group card made on the board may hold a solo card or a group card
   defined at the source (Desk as one tile in an Office group), never another group card made on
   the board. Two levels (a group shown as one card inside one other group) are not built (K147).
4. **A solo card's Toggle switches the whole group** (K195): with a toggleable face, a tap turns
   every light and switch among the members on or off, as the face shows. Toggle is offered only
   when every member is a light or switch; a group that mixes in anything that cannot be toggled
   (a pinned or unpinned sensor, a TV) taps to Members (K188, K194). A sensor in the header does
   not count. The tap sends `turn_on` or `turn_off`, never `toggle`: a toggle of a mixed set flips
   each member and leaves it mixed. "On when: any / all" counts the same members (K13).
5. **Pausing is the entity's** (#60): pausing a member's card pauses it in every card it is on.
6. **A member copied to the page asks which group** (K92, K149): the group card's own, the entity's
   HA-area group, or another, chosen before the Copy or Move dialog.
7. **History** of a group defined in HA is its own entity's; members' histories are reached through
   Members (K54). A group made on the board has none of its own.

## 5. What the glass edits

Groups and cards are edited on the glass at a minimum; the intricate rest belongs to the web app
(#87). All of it asks first, in the owner's sentences (K149, K165):

- a card's **group and label**: a page card's Add to group, Change label; a tile's Change group;
- **membership and pins**: Edit cards from a group (the tap cycle), Pin / Unpin this card;
- **making a group**: New group, from an HA area or from nothing, named with the keyboard (K95, K168);
- **moving a card** between page and group: each move picks a target, then Copy or Move, then a
  dialog; a card made on the board that moves again relocates and never copies a copy
  (`home_name`, `home_group`, `moved_to`, `orig_group`). A group card's member is Copy only;
- **Restore** (small, red, only when there is something to restore) is the one way back: a copy is
  deleted, a moved card goes home and its original returns, a regrouped card gets its group back.
  It stays as built and is not developed further (K198);
- **Hide** from the page (the page's pool, listed under Hidden in Edit cards) and Remove for cards
  made on the board; deleting for good is the web app's (K117);
- a group made on the board is **deleted when no card is in it**, with its label group if no other
  card wears it (K200, `CardAdded::emptied()`); a group a member was moved out of is hidden until
  a card returns; a compiled group is never deleted.

Card and group ids, groups' colors and where each setting lives: `card-sheet.md` §17. The page
header's slots and the cluster header share one planner: `header-slots.md` §4, §8.

## 6. Saved keys

On a group card: `present` (`card` / `cluster` / `summary`), `slots` (member ids, highest priority
first), `face`, `cl_style` (`swatch` / `bar`), `cl_icons`, `tap` / `long`, `card_tap` / `card_long`,
`card_state`, `group_view`, `margins`. On a member: `in` (its group card's id), `hidden` (`yes`
unpinned, `moved` moved out to the page), `from`. On any card: `removed` (`yes` hidden from its
page, `moved` moved into a group). On a page: `cl_style`, `layout` (the arranger's kept answer).
`system.arranger.*` holds the arranger's weights and budget. A group's HA link is
`groups.<id>.ha_area`.

## 7. Why these shapes

- **Copies, not references.** Once an entity is a card it is its own object with its own id and
  settings; renaming one card does not rename the entity's other cards. The cost is more entries in
  the settings file, and "which card is which?", which Entity properties answers by listing the
  entity's other cards.
- **A cluster is a snapshot.** Making a group from a set of cards copies the ones ticked then. An
  entity that later joins the same HA area becomes a card on the page, not a member of the group;
  the owner adds it from Edit cards, which lists what the area holds that the group does not
  (K133, K142). A group that changed by itself would move the page.
- **A small screen's constraint decides one-cell clusters.** Two or three tap targets in one cell
  are too close even on a 7" board, so a one-cell cluster holds sensors.
