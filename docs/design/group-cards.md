# Groups, areas and clusters - what is a "group card"? (milestone 2.11, #66)

**Status: draft taxonomy, 2026-10-01**, from the owner's notes during interview §2 (D4) and Claude's
reply. Interview §4 asks its questions against this file. Nothing here is built. Related:
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

- Confirm the source x presentation split, and the names: *group entity*, *area*, *board group*;
  *card*, *cluster*, *page*.
- Area tap default: the area's lights only (domain-scoped)? With per-area exclusions?
- Does a board group live in the build sheet only (3.1), or also on the glass?
- ~~Verify: Hue group member attributes on `light.office`~~ **VERIFIED 2026-10-02 (owner):**
  `entity_id: [light.office_right, light.office_lamp, light.office_left]`, plus `is_hue_group`,
  `hue_type: room` and `hue_scenes` - see `card-sheet.md` §11.1.
- Verify: `render_template` for `area_entities()`.

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
