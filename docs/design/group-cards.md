# Groups, areas and clusters - what is a "group card"? (milestone 2.11, #66)

**Status: interview §4 under way, 2026-10-19** - the names, mini tiles and what the glass edits are
answered (§6.1); Claude's model of how the pieces fit is accepted (§6.3), with the owner's notes
(§6.4); the mocks are next ("2.11 Clusters", Layout Playground v5). §1-5 are the draft taxonomy of 2026-10-01 and the owner's model of 2026-10-12; where they
disagree with §6, §6 wins. Nothing here is built. Related:
`card-sheet.md` §6 (aggregate vs area, draft 2), `look-and-feel.md` §3.8 (clusters, the arranger),
`ha-websocket.md` (area resolution, measured).

---

## 1. The owner's three kinds (2026-10-01)

1. **A group that already exists outside the board.** The owner's "Desk" (`light.office`) is a group
   of three Hue lights defined in the Philips Hue app and brought in by HA's Hue integration. HA's
   own "group" helper (light group, switch group, sensor group with mean / median / max...) behaves
   the same way: **one entity, its own entity id; a service call to it controls every member.**
   HA's details dialog for it shows the usual light controls, a palette, and **each member at the
   bottom** - tap toggles it, its slider sets its own brightness, a long press opens its own dialog.
   The gear icon holds name, icon, entity id, enabled, visible. (Screenshot: the owner's Office
   group dialog, 2026-10-01.) The owner has no HA-helper groups defined; Hue groups only.
2. **An HA area.** Entities and devices carry an `area` in HA, and actions can target an area by
   `area_id` ("turn on all Living room lights"). An area card is "essentially a 1x1 cluster with a
   single hero": a tap could toggle the area's lights, or open a page / popup showing the area's
   entities as one cluster (border, tag / bar / header). The user can hide entities from it, choose
   which feed the header, choose the hero, and choose what tap and long press do. **The owner's
   warning:** his Kitchen area holds two light switches, a motion / temperature sensor and a
   TCL/Roku TV - a Kitchen tap must turn on the lights, NOT the TV, or an area tap is useless.
3. **A group made on the board.** Any entities the user picks - "Bedroom" holding some, not all, of
   the Bedroom area. A chosen hero; a tap acts on the hero, or toggles every member; a long press
   opens a popup or page of the members. The owner asks how the board can act on many entities that
   share no HA id. And, brainstorming: a group of all the outdoor temperature sensors where **a tap
   cycles to the next member**.

And separately: **a cluster** (several entities shown together in one border, on the page) should
get the same kind of details popup - where the user picks which entities show or hide, which feed
the header, and **what a single tap on each entity does** (toggle by default; open details, e.g.
for sensors).

## 2. Claude's reading: two separate questions, not one

Every one of these is a *set of entities*. Two things vary independently, and mixing them up is
what makes "group card" confusing:

**A. Where the set comes from (its SOURCE):**

| Source | Who defines it | How the board learns the members | How one tap acts on all of them |
|---|---|---|---|
| **Group entity** (Hue group, HA group helper) | Hue / HA | The group entity lists its members in its attributes - to be confirmed for the owner's Hue groups by looking at `light.office` in Developer Tools > States | **One call to the group entity** - HA and Hue do the rest. Nothing to compute |
| **HA area** | HA | One `render_template` of `area_entities('kitchen')` (to verify) - avoids the 1.3 MB entity registry (`ha-websocket.md`) | **One call targeting the area, scoped by domain**: `light.toggle` with `area_id: kitchen` touches only lights - so **the TV is safe by construction**. If the user excludes a light, the call switches to an explicit entity list |
| **Board group** | The user, on the board / web UI / build sheet | It IS the list | **One HA call with a list of entity ids** - `light.turn_on` for all-lights, `homeassistant.turn_on` / `turn_off` across domains. Only non-HA members (MQTT, virtual) need the board to loop. HA accepts lists; no per-entity loop for HA entities |

**B. How the set is SHOWN (its PRESENTATION)** - any source, any of these:

| Presentation | What it is |
|---|---|
| **One card** | a hero (a chosen member or the group itself), the set's state summarised (mixed indicator), long press for the members |
| **Cluster** | every visible member as a tile inside one border on the page, header with promoted values (`look-and-feel.md` §3.8) |
| **Its own page** | a room / area page holding the set as one large cluster - the "linked page" of `pages.md` |

So "Desk as a cluster", "Kitchen area as one card", "my Bedroom group as a page" are all valid
combinations, and each needs only: a source, a presentation, and the tap / long-press behaviour.

**C. Behaviour (per card, per tile):** tap = toggle (default for lights / switches) / toggle all /
open details / **cycle the shown member** (the outdoor-temperature idea - entirely on the board,
no HA call; it could also rotate on a timer) / nothing. Long press = the popup.

**Toggle-all with a mixed set:** decide on the board - if any member is on, turn all off; else turn
all on - and send `turn_on` / `turn_off`, never `toggle` (a toggle of a mixed set flips each member
and stays mixed). `cards.md` §4 already chose "tap toggles all; mixed state has its own indicator".

## 3. Small-screen consequences (Claude)

- A members list is the HA way and works in a popup (D4 - members inside the window, `card-sheet.md`).
- Area and board-group settings (which members, hero, header feeds, tap action) are configuration -
  the popup's settings side / web UI / build sheet, not something to do through many small taps.
- Cycling and toggle-all are cheap; listing an area's members needs one template call per area, at
  load, not per frame.

## 4. OPEN - for interview §4 (2.11)

- ~~Confirm the source x presentation split, and the names~~ **ANSWERED 2026-10-18** -> §6.1.
- ~~Area tap default~~ **ANSWERED by K67** (2026-10-12): HA areas are never commanded.
- ~~Does a board group live in the build sheet only (3.1), or also on the glass?~~ **ANSWERED
  2026-10-18** -> §6.1: made and edited on the glass too, at a minimum; the intricate rest in the web
  app.
- ~~Verify: Hue group member attributes on `light.office`~~ **VERIFIED 2026-10-02 (owner):**
  `entity_id: [light.office_right, light.office_lamp, light.office_left]`, plus `is_hue_group`,
  `hue_type: room` and `hue_scenes` - see `card-sheet.md` §11.1.
- ~~Verify: `render_template` for `area_entities()`~~ - not needed: areas are never commanded (K67),
  and members' areas are learnt since #93.

## 5. The owner's model, 2026-10-12 (before K17; DECISIONS K65-K67)

**Three ways a set of entities shows up**, each a place the user may want "make this a card":

1. **A group card for a group defined at the source** (Desk = `light.office`, a Hue room): one
   entity; a command to it acts on every member. The only kind on an HA page today.
2. **A group card made on the device or in the web app**: one 1x1 card for several entities;
   commands go to the list of entity ids (HA members), one by one otherwise. Exists compiled -
   the Fleet page's All Lamps (virtual lamps); HA members not yet.
3. **A cluster** (group panel): one bordered card larger than 1x1 holding members that would
   otherwise be cards of their own, **without their own borders** - clear logical groupings,
   denser than a cell each. (`look-and-feel.md` §3.8.)

In §2's terms 1 and 2 are two SOURCES shown as one card, and 3 is a PRESENTATION of any source.

**"Open"** (a group card's tap action): (a) as a cluster; (b) a bordered window of the members as
ordinary cards; (c) a page holding only the members; (d) the Members view. A user may keep the
members off the dashboard entirely and reach them only this way.

**Groups** (the card label's relationship, K46): device-native; an HA area becomes a group. A card
is in exactly one group, every card is in one (a group may have one card). A card label has
words, colour and type; the colour and type follow the group, the words may be the card's own
while it stays in (e.g. Desk's three members as cards, Office's blue, each worded "Desk": grouped
by colour truthfully, by words visually). All members of a group card share its group - a member
made into a card (K17) takes it. Groups honour inheritance (device / page / group / card, each
layer may opt out - D-7) and may be LOCKED (D-7, Q2). **HA areas are never commanded**: an area
may hold entities that are not on the dashboard. "Kitchen lights at one tap" is a group card of
exactly those lights.

**Actions**: add a card to another card (a group card is made); make a card from a member - a
copy for a source group (its membership is the source's, and its scenes stay with it); two
cards on one entity are separate cards with their own settings (K33) - except Paused, which is
the entity's (#60): pausing the member's card pauses it in the group card too. Remove any card
(to the page's pool, K35/K66). Move a card to another group or found a new one: 2.11.

**Visibility (owner, 2026-10-12, K68)** - the greyed SETTINGS row is about groups, not every card,
and shows only where it means something: a card in a group that is shown as a cluster, a group
panel, or a page of the group's members. A cluster shows every card of its group by default; the
row then says where this one appears: **in the cluster only**, **available for the dashboard**
(it still takes a deliberate "make a card" - K17's copy - to put one on the main grid), or
**hidden**. K17's "Add as a card" is that deliberate step, built before the row exists. Later, the
device should recognise and manage the cards that are also members of an existing group card.

**Placement of added cards (owner, K65 Q1):** the first card made from a group card stands next to
it; later ones follow, the newest last. Live layout settings (4.2) and the arranger (#78) replace
this.

**Open for 2.11 (Claude):**
- "Every card is in a group" and "a card's colour and type are its own only outside a group" meet
  in K46's solo group: a card that wants its own colour founds a group of one. So SETTINGS' Card
  label "None" (K44, K48) would go - or "None" means no label shown, while the group stays.
- The card label **type** follows the group: today it is the page's (`headerDefault`) or the
  card's. A group would need a type of its own, inherited from the page.
- Layout: there is no arranger yet (#78); cards flow in the page's order and the lowest priority
  drops first. K17 places a new card after its parent, with the parent's priority.
- In HA a group entity's members may sit in other areas than the group (areas are per entity);
  irrelevant to K17 - a member made a card takes the group card's group.

## 6. Interview §4, first sitting (2026-10-18)

### 6.1 Answered (DECISIONS K88-K91)

- **The names (§4 Q1)** - the owner accepted Claude's recommendation:
  - **group**: the relationship behind the card label - a name and a colour (K46);
  - **group card**: one card standing for a set of entities;
  - **cluster**: a group card shown as a block of its members, on the page;
  - **group page**: a group card's members as a page of their own.

  A group card has a **source** - a group defined in Hue or HA (Desk = `light.office`), or a list
  made on the device (All Lamps; the owner's "Laundry") - and a **presentation**. Its members are
  cards of their own: an id, their own settings. "Area card" is dropped as a term.
- **"Area" is HA's word, never the dashboard's** (owner):

  > "As far as how our dashboard engine handles card relationships the word 'area' is not part of
  > the conversation, our entire card management and grouping systems use the term 'group'. The
  > word 'area' has meaning only when an entity is brought onto the dashboard upon which the 'area'
  > becomes 'group'. We maintain a table of area names in HA and the group that each area is mapped
  > to so that if we rename a group but then bring an entity in that belongs to the original area
  > name, it will be assigned to the correct renamed group."

  That table exists: a linked group's id is `ha_<area_id>` (`CardGroups.h`), and HA keeps an area's
  id when the area is renamed in HA, so the link survives a rename at either end.
- **Mini tiles (§4 Q5)** (owner): a cluster's members have no border of their own - the icon, with
  its entity label underneath. The cluster's border is the only one.
- **Any presentation can become another** (owner): a group card can be shown as a cluster, and back;
  and its tap can open a **group page** holding only its members - Desk's page holds its three
  lamps, not every card in Office.
- **What 2.11 edits on the glass** (owner's minimum; everything more intricate is the web app's,
  #87 - Claude's earlier advice, which the owner found grounding):
  - a card's group - which includes moving a card into a cluster, or a mini tile out to the page;
  - which members fill the cluster header's slots (temperature, occupancy);
  - members shown or hidden in a cluster (hidden is not removed);
  - **making a cluster**: choose a group to base it on, then a list of cards with that group's
    preselected - leave any out, add any others - and a name of its own if wanted, which its members
    then wear;
  - adding cards to a cluster, moving them out, swapping the cards in the header's slots.

  "Just because a cluster is made of the 'Office' group doesn't mean there can't be solo cards on
  the dashboard that are still members of 'Office'." The same cards can be in a cluster and on the
  page "without conflict".

### 6.2 Questions dropped - already answered

- §4 Q2 (a tap on a group whose members disagree): 2.10b, #66's comment - as HA's light group, "On
  when: any / all", on turns all off, otherwise all on.
- §4 Q9 (area colour): K46, K47 - the colour is the group's, device-wide.
- §4 Q4 in part: D-14 and #66's first comment - a cluster has a bar header whenever it has content;
  status as icons, told apart by shape and fill. Left: temperature and lux as a number or an icon;
  one line or two.

### 6.3 Claude's model of how the pieces fit - ACCEPTED 2026-10-19 (K92), with §6.4's notes

The owner asked: must groups be exclusive? Are Desk's lamps in Desk or in Office? Which group does a
member copied to the page join? Can a cluster of two lights and a TV, turned into one card, toggle
only the lights? And for every combination of entities, cards, groups, group cards and clusters,
with how each can go wrong. The owner's own reasoning got most of the way: "once an entity becomes a
card, the card becomes its own unique thing detached from the entity itself".

**The objects:**

| Thing | What it is | Its group | Where it sits |
|---|---|---|---|
| Entity | a light, sensor or switch in HA, MQTT or virtual; in HA at most one area | none - an HA area at most | the registry |
| Card | the dashboard's object for one entity (a solo card) or a set (a group card); an id (K33) and its own settings | exactly one | on one page's grid, or inside one group card |
| Group | a name and a colour, device-wide; remembers the HA areas linked to it | - | nowhere: groups place nothing (K46) |
| Group card | a card for a set: a source and a presentation | exactly one | a page |
| Member | a card inside a group card | its group card's | inside that group card, on no page |

**The rules (proposal):**

1. **Groups stay exclusive: a card is in exactly one.** A grouping by function ("Temp Sensors",
   "Indoors") is a group card in a group of its own, whose members are copies of cards that live
   elsewhere; copies are independent (K33). The price: the same sensor can wear two colours on one
   dashboard - colour then says "the set you put it in", not "where it is". The alternative, a card
   in several groups, needs a rule for which colour it wears, and whichever it wore would mislead.
2. **A member has no group of its own: it wears its group card's** (refines K67). Desk's lamps are
   Desk's members and wear Office; "Desk" is the card's own words (K46: own words keep the group).
   Desk as a cluster: a header saying "Desk" in Office's colour. Desk moved to another group: its
   lamps follow. Mini tiles show no card label - the cluster's header shows the name and the colour
   once.
3. **One level of nesting.** A group card made on the device may hold solo cards and group cards
   defined at the source - Desk as one mini tile in an Office cluster, its tap toggling all three
   lamps, its long press opening Desk's own window. It never holds another group card made on the
   device: no loops, no cluster inside a cluster.
4. **A source group's members are the source's** (K65): the board may hide one, never add or remove
   one. To put Desk beside a sensor, put Desk in a group card made on the device (rule 3).
5. **The tap set.** A group card's tap acts on the members that switch together - lights and
   switches by default; anything else (a TV, a fan, a cover, a lock) only when ticked; sensors never.
   "On when: any / all" counts the same members. The Kitchen's TV is safe unless someone ticks it.
   Paused cannot do this job: it belongs to the entity, everywhere (#60).
6. **Presentation is the group card's setting**: Card (1x1) or Cluster; its tap may open the group
   page, a window, or the Members view. Switching keeps the members and their settings. **A cluster
   that cannot fit** (a small screen) is shown as its 1x1 card, and the arranger says so - which
   answers §4 Q7.
7. **The header's slots are filled from members**; a member in a slot is not also a tile.
8. **Visibility inside a group card is Shown / Hidden.** K68's third choice, "available for the
   dashboard", goes: any member can be copied to a page at any time ("Add card to dashboard"), so it
   says nothing. *K68 was the owner's, so this is his to confirm.*
9. **A member copied to a page asks which group, in one tap**: the group card's group (chosen
   already) / the entity's own HA-area group / Other group. The owner's Laundry: a group card in
   Living holding Table (Kitchen in HA) and Overhead (Living in HA); Table's "Add card to dashboard"
   offers "Living (Laundry's group)", "Kitchen (Table's area in HA)", "Other...". Replaces K65's
   fixed rule.
10. **Making a cluster from cards on the page copies them; one checkbox, "Take them off the page",
    moves them** - ticked to begin with, as a cluster is usually made to tidy a page.
11. **The card list** (making a cluster, adding a member): the cards on the device, by group, and
    **the HA entities the owner has labelled** in HA - labels exist since HA 2024.4, on entities,
    devices and areas - say "Fleet", read in one template call (`label_entities('fleet')`, and
    `label_devices()` with `device_entities()` for a labelled device). To verify on the owner's HA.
    Searching all of HA is the web app's job.

**How it can go wrong, and what stops it:**

| Case | What goes wrong | What stops it |
|---|---|---|
| One entity on three cards | renamed on one, the others keep the old name: "I changed it and it didn't change" | by design (K33); Entity properties lists the entity's other cards ("Also on: ...") - K68's "recognise and manage the cards that are also members" |
| Pausing a member | pauses it in every card | Paused is the entity's (#60); its row says so |
| A mixed group card's tap | the TV turns on | rule 5 |
| The Hue app adds a lamp to Desk | the board's list is out of date | members follow the source at every load (A13); a vanished member's settings are kept and listed (K35) |
| Adding a sensor to Desk | Hue owns that list | rules 3 and 4 |
| A group card inside itself | a loop | rule 3 |
| A function group's colour | one sensor in two colours | rule 1's price, accepted |
| Two groups with one name | a new "Outside" beside HA's Outside area | warn and offer the existing one, as K46 does for a colour |
| A group renamed on the board, or the area renamed in HA | the next entity from that area lands in a new group | the `ha_<area_id>` id (exists) |
| HA renames an entity id | the card loses its entity | G4: an "unknown entity" card, a System Doctor line |
| A group with cards deleted | cards with no group | a group is deleted only once empty; a solo group goes with its last card (K46) |
| Members hidden or shown | the cluster changes size, the page re-arranges | the arranger is deterministic (#78): the same blocks always give the same page |
| More members than a screen holds | the cluster does not fit | rule 6 |
| A device-made group card's History | no HA entity, so no history | the members' History through Members (K54's rule, extended) |
| A device-made group card's scenes | none - scenes are the source's | Laundry has none; Desk keeps its own |
| A cluster based on a group, later | a card added to that group afterwards does not join | a cluster's members are a list made once; the web app may offer "new in this group" later |
| A cluster on an S3 | 9 tiles and a header on a 128 KB pool | mini tiles are lighter than cards; measured with `lv_mem_monitor()` before committing |

**What this buys, and what it costs:**

- **Buys:** one colour rule that never lies about what it claims; any entity in any arrangement,
  through copies; one object - the group card - for the 1x1 card, the cluster and the group page,
  so a small screen's fallback, the owner's "transform" and the group page are one setting; commands
  right by construction (a source group: one call; a list: one call with the entity ids; an area:
  never); groups never move cards.
- **Costs:** copies multiply and drift - more entries in the settings file, and "which card is
  which?"; a sensor can wear two colours; a source group is extended only through one more level;
  members become cards with ids - more to save, and a source group's members get ids the board
  makes (A13's, with the group card's id).

**Still open beside the model:** naming on the glass - a new cluster or group needs words, and K37
put the keyboard later; until it exists, a new cluster takes its base group's name, renamed from the
PC. §4 Q3 (sizes) goes to the Layout Playground; Q6 (long press inside a cluster) and Q8 (group
pages and "back") come next; §5's "Open for 2.11" (Card label "None", the label type per group)
after.

### 6.4 The owner's notes on the model (2026-10-19)

"The proposed model in §6.3 looks legit to me" - with these:

- **Extending a group defined at the source (K93).** The owner's idea: a member added to Desk keeps
  the card called "Desk", and behind the scenes the board makes it a group card holding
  [Desk's Hue group + the new member]. Claude: manageable - it is rule 3's nesting, done for the
  user. The card keeps its id, words, group, settings and place; its source changes from "the Hue
  group" to "a list holding the Hue group and the extras". The tap is one HA call with both ids (HA
  expands the Hue group). The Members view lists Desk's lamps (from Hue: they can be hidden, not
  removed) and then the extras (removable). Desk's Hue scenes stay, acting on the Hue lamps only,
  and the view says so. Removing the last extra makes it the plain Hue card again.
- **History for a group made on the device (K94):** scenes do not matter much; history does, and a
  **local history recorder is already in scope** - for MQTT cards, sensors wired to the board, and
  the board's own counters (boots, firmware updates, others to come). A device-made group card's
  History is then its members', from HA or from that recorder.
- **"A cluster based on a group is a snapshot"** - the owner asked what this means. Making a
  cluster from the Kitchen group copies the cards ticked at that moment into it. A Kitchen entity
  brought onto the board later (a new Kitchen light from HA) becomes a solo card in the Kitchen
  group, on the page; it does not appear in the Kitchen cluster by itself. **OPEN:** keep the
  snapshot and add it by hand (Claude's recommendation: a cluster that changes by itself moves the
  page), or offer "Also add to the Kitchen cluster?" when a card joins a group that has a cluster.
- **The on-screen keyboard comes in 2.11 (K95)** - "Should we just take the plunge?" Claude: yes.
  It has been put off three times (K37), and 2.11 is the first place it is needed: group and
  cluster names. A kit component, ASCII only (the fonts carry nothing else), used by the custom card
  name (K37) as well.
- **A card's options by where it is (K96)** - the SETTINGS "Visibility" row, in flux until now,
  becomes a button opening a view whose choices depend on where the card is (names not settled):
  - **A card on the page:** *Hide from dashboard* - with a way to see the hidden cards and restore
    them (K66's pool); *Add to group* - a screen to add the card to an existing group card, to make
    a new group card (choosing its sibling members), or to add it to an existing cluster, as a copy
    or as a move.
  - **A member of a group card:** *Add card to dashboard*; when that member is already on the
    dashboard, *Remove from dashboard*.
  - **A member in a cluster:** *Hide from cluster view* - with a way to see the hidden ones and
    restore them; *Add card to dashboard*.
  - **The cluster itself:** a "card / tile management" screen - members shown or hidden, cards
    added, which members fill the header's slots. ("This may be delving too deeply into 'do that
    from webapp' territory", the owner says; Claude: it is K91's minimum, a list of rows on the kit,
    and belongs on the glass.)

  Claude's notes: *Add to group* in these words means a group card or a cluster; choosing the
  card's **group** (its label's name and colour, K88) is a separate choice on the same screen, and
  the two need different words. *Hide* is K66's remove: the card goes to the page's pool with its
  settings, and the pool's list restores it (or deletes it, on the owner's word).
- **A cluster can be 1x1 (K97)** - the owner, from the House page's two garage doors: as a group card
  they would go "on" if either door is open, but better still if one cell showed which door. "Maybe
  a cluster with few enough mini tiles could actually fit a 1x1 cell?" On the 7" boards at 6x3 a
  cell is about 24 x 25 mm - room for 2 x 2 mini tiles with labels a size down; on the P4_5 at 5x3
  (about 22 x 19 mm), two side by side. The arranger may give a cluster one cell when its tiles fit;
  a 1x1 cluster has no header, and wears its group's card label as a solo card does. To confirm on
  the mock.
- **The arranger, the owner's verdict on the playground (v4):** the first look "blew me away"; over
  time, "rather unhappy with the way the auto-layout feature works": it could not find layouts the
  owner was sure existed, made "very unbalanced clusters where a cluster becomes enormous with just
  2 mini tiles", or stretched them into long rows and columns. Claude, from v4's code: its packing
  is greedy (first fit in reading order, so a tiling that needs another order is never found), it
  tries only five shapes per cluster and stops after 4,000, and the spare cells are handed out
  AFTER packing - each cluster in turn grabbing the empty cells beside it, so the first one grabs
  most. Playground v5 replaces it (#78).

### 6.5 The mocks (2026-10-19, built unattended) - for the owner to react to

- **[2.11 Clusters](https://claude.ai/artifact/9mRzE8nb7U8nYSfxAxQhrV)** - the House page with
  clusters on the P4_5 and the 4B (switches for the open looks), a cluster in one cell on every
  board at true size, Desk as a card / a cluster / a group page, and a card's options by where it is
  (K96). Its section E asks **C1-C8**, each with Claude's recommendation: the mini tile's "on" (lit
  disc), header values (number), one header line, the group's colour on a cluster (a swatch), where
  one-cell clusters are allowed, one Tile / Header / Hidden choice per member, Copy / Move when adding
  to a cluster, and the snapshot (§6.4).
- **What the mock measured** (estimates from the BSP diagonals until 2.11 runs on glass):
  - **One-cell clusters with every tile at 9 mm** fit only on the 7" boards (7B, 1060: a 23 x 25 mm
    cell holds the two doors at 20 x 10 mm, Desk's three lamps at 10 x 10 mm) and the CYD_S3_8048.
    On the P4_5, the S3_5B, the 4880 and the 3248, two tiles are tight (8-9 mm) and three too
    small; the 4B's narrow cells (15.5 mm) hold neither. So K97 is a 7"-and-up feature unless the
    owner accepts tight tiles.
  - **The P4_5's cells are short** (16.4 mm): a one-row cluster's tiles are about 8.8 mm tall under
    a 5 mm header - at the touch minimum. Two-line headers would push them under it.
- **[Panel Edges](https://claude.ai/artifact/U8wGGgPEWHpgKNvZ2miYMu)** - the owner's requests 2 and 3
  from 2026-10-19 (`interview-phase2-3.md` §12): the header panel lying on the bar (top corners
  rounded toward its middle, the bar's line fading out before the walls) with a halo, and a window
  over the page with no backdrop / a halo / the dim / both; sliders for the halo's width and
  strength and the line's fade; the cost of each, from 2.10's measurements.
- **[Layout Playground](https://claude.ai/artifact/SBSanCnwviNbKzQmkwiJXp) version 5** - the new
  arranger, v4 kept beside it for comparison (`layout-engines.md`, "As prototyped"). On the House
  page it gives the P4_5 a 5 x 2 page with no empty cell and four even 2 x 1 clusters (v4: 4 x 3,
  clusters 1 x 2 and 1 x 3); with a five-member cluster added, a full 6 x 4. **On the 7B the House
  page has 6 blocks for 18 cells**: v5 leaves 9 cells empty rather than make one cluster huge (v4
  made two 3 x 2 clusters). Spreading that spare room as even margins (`look-and-feel.md` §3.8 item
  3) is the question it raises. Its weights are starting guesses for the owner to tune.
