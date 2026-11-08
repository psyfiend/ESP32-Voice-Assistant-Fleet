# Open threads from 2.13

The detail behind the 2.13 items in ROADMAP's Phase 2 summary: for each, what was intended, what the
owner said, what is decided, and what is still open, so the thinking is not lost when the work is
picked up. Decision numbers are `DECISIONS.md`. The full trail is in `archive/PLAN_2.13.md` and
`archive/TEST_2.13.md`.

**The owner's framing, which steers all of this:**
- "A group is a group is a group": one object, shown three ways - a solo card, the group view, a
  group on the page (K88, K90).
- Do not design the boards around kinds of card that do not exist yet. Summaries and alerts need not
  fit inside the group card because they hold several entities (K189).
- Nobody will stand in the garage tapping for 20 minutes to rearrange a dashboard; they will do it in
  the companion web app in 20 seconds. **Not every complex card needs a full editor on the glass**
  (K182).

---

## 1. HA group entities as board groups (K170), and the mirror (K184)

**Intended.** Every HA group entity (Desk, Eric Bedroom) becomes a group on the board, made from the
HA group's members: a Solo card by default, with the GROUP panel, Show as and Edit cards; one command
reaches the HA group entity and any card added to it.

**What makes it larger than it looks.**
- **"Every HA group entity" needs a definition.** About 20 HA entities carry a member list
  (`attributes.entity_id`), and only the Hue rooms and zones are the intended kind. The others are
  one-member aliases from the logical layer (the doors, Attic Hatch, 3D Printer), groups of groups
  three deep (Perimeter Doors, Leak Detected), and totals and averages (House Temperature and
  Humidity, power) - which are summaries (§3).
- **The board has two separate ideas today, and this makes Desk both**: a card on one entity (Desk
  on `light.office_lights_hue`, its lamps learnt from the member list) and a group of cards (pins, a
  header, Edit cards, moves). Every place that asks "group or entity card?" needs a third answer:
  1. Membership has two owners: the Hue app owns Desk's members and the board would keep a copy,
     which goes stale; removing a lamp on the board cannot remove it from the Hue group, so "all on"
     still switches it.
  2. The face has two sources: HA's group state and the board's reading of its members.
  3. Scenes belong only to the Hue group; a lamp added on the board takes no part.
  4. A group inside a group in the settings file: `readMember()` refuses a member with no entity,
     `nested()` allows one level (K147 wants two), moves and Restore record a member as one card,
     and Edit cards, the group view and the back path each gain a level.
  5. One lamp in two groups (a zone holding two rooms' lamps) gives a lamp two cards and two sets of
     settings.
  6. The 128-card limit: the Hue groups would cost about 13 cards a board, the door aliases about 30
     more.
  7. K133: a group made automatically at boot would be the board writing its own dashboard.
  8. Desk would have to be converted in every board's file, its saved choices kept under its id.
  9. HA calls the Desk group "Office", which would arrive as a second Office.

**What the owner said.**
- **Hue groups are never edited from a board** (K179). Desk becomes a native HA light group (his job,
  with the scenes he wants recreated in HA); Eric Bedroom stays a Hue group for its dynamic scenes;
  Indoors is not offered ("all indoor lights", if wanted, would be an HA script or template). A
  read-only member list is fine; he still wants to open it and see the members.
- **The mirror** (K184): adding to an HA group makes a group on the board that looks and behaves as
  the original and holds the longer list. On and off reach every member; a scene only the HA group's
  members; HA's own members stay read-only.
- **A mixed group shows every control any member has** (K185) - his example is the kitchen's Sink and
  Table, one of which dims.
- **Temperature and door groups open as groups** (K186), so each member's History is a tap away, and
  the group's own History shows them together.

**Open.**
- Which entities count. Claude's proposal: any HA entity listing two or more members, its members
  read-only and its face HA's own value; one-member aliases stay plain cards.
- When the board group is made: at boot, or when its card is made (Claude: when its card is made,
  which keeps K133).
- When HA's member list changes: the new member joins unpinned with a notice, joins pinned, or the
  board asks.
- Which domains: switch groups, old-style `group.*`.
- Whether Bedroom is a page card on every board or the P4_5 only.
- Once Desk is an HA light group, check its HA scenes are offered.
- K191's "Face greyed for an HA group entity" cannot be seen until an HA group entity is a board
  group.

## 2. Boards writing to HA (K180)

**The owner's ideas.** Each board group as an HA group entity, so HA shows every board's groups (its
header members in attributes, or a companion helper holding its settings); from there, managing the
fleet from HA, syncing a group across boards, deploying the same groups to several boards. Labels
such as `Fleet-<device>-<group>` were considered and found messy (a label per board on each entity).

**Why not, as things stand.**
- An HA group holds one domain only, so a mixed board group cannot be one; old-style YAML groups are
  more trouble than they are worth.
- Spook's `group.add_members` / `remove_members` / `set_members` work only on groups made in HA's
  interface, never on Hue groups. Using them would make the firmware depend on a custom integration,
  may need permissions for the board's HA user, and a tap on one board would change a group the phone
  and automations use too. Whether a change takes effect permanently without a reload is unverified.

**The owner's reading of K133, for when this returns:** K133 is about his changes in HA not appearing
on the boards by themselves; objects a board creates in HA do not break its spirit.

**Decided.** K180: the boards keep their own configuration until the web app (#87) and the ESPHome
API (Phase 7). See also the brainstorm's live member reports and synchronizing boards
(FUTURE_IMPROVEMENTS).

## 3. Summary cards (K182, K189)

**What exists.**
- On the board: Garage - Doors is a compiled `SummaryCard` (`CardCatalog.cpp`). Its face: one member
  active, its short name (North); all of two active, "Both"; all of more, "All"; some, a count ("2
  open"); none, the card's name.
- In HA: `binary_sensor.garage_doors` has an `open` attribute (none / North / South / Both), and
  `sensor.doors_open` has `names` and `entities`. The boards read neither.

**Where the face could be computed.**
- **A - an HA template entity**: shared by every board, the phone and 2.14's alerts; written in YAML.
- **B - a framework on the board**: a rule chosen on the glass, living on one board until Phase 3.
- **C - the web app**: an editor for B.

Claude recommended B with A as one of its sources: rules Names, Count, Any, Average / Lowest /
Highest / Total; an HA entity with members as a summary from HA (the same mechanism as §1's mirror);
words from an attribute (`open` -> North) as an option.

**What the owner said.**
- A summary need not resemble a group card (K189). It keeps what a group has for its members - the
  Members list, a tap into any member, perhaps opening as a group view - and its face follows a rule
  chosen for its purpose.
- One framework, shared by every board; ideally defined in HA and added from Edit cards (perhaps HA
  holding YAML files the boards read). What is built in HA should be usable in HA too; fleet-only
  entities, templates and scripts are allowed where that cannot be. Nothing in HA's
  `house_rooms.yaml` / `house_status.yaml` is set in stone.
- **The summaries he expects to use:**
  - **Garage doors open**: a solo card with members; the face shows one door's name or "Both";
    Details holds open and close buttons; History for both doors, or each from its own window.
  - **Lights on, per room**: a header sensor in each group, shown only above 0; its header panel row
    taps through to the lights.
  - **Garage door night alert**: a door open more than 5 minutes at night, a system header alert with
    history; Members lists the doors.
  - **Perimeter door left open**: open longer than 10 minutes; an alert with history and Members.
  - **Temperature roll-ups**: the face an average or a chosen member; History shows every member,
    each shown or hidden; ideally built on the fly. `sensor.house_temperature` serves the thermostat
    and leaves out sensors that read high on purpose.
  - Later: irrigation alerts.
- Resizable cards that show more on their face belong to Phase 6.
- On the glass: a group of temperatures with Face "All members" shows its first member's value; he
  wants an average or a chosen member.

**Open.**
- Whose words a summary shows: the board's (card names), HA's (attributes), or both.
- Which rules, and with three of four doors open, "3 open" or their names.
- Pitfalls: summaries of summaries hit K147's nesting limit; `unknown` after an HA restart counts as
  a state (#84); an average over members in different units; state never shown by color alone; long
  words need the text fitter.
- See also the brainstorm's per-area roll-up entities.

## 4. Backup and Restore (K181, K187) - milestone 2.16

**Why.** Reset deleted the whole House page once the compiled cards were retired (K171), so it was
removed. The owner had hardly used it, and with the dashboard quick to fill from HA a lost setup
worries him less; his idea was several saved layouts on the board to switch between.

**Decided.** A full backup (pages, groups, cards, settings) restored after a flash; 3 to 5 kept,
named `<device_name>-<timestamp>`; a rough UI; backward compatible, and a backup from one board
restores on another (K181). Backup and Restore take Reset's place in the system panel; a backup holds
the whole settings file, cards made from other boards' MQTT entities included; Panel's MQTT discovery
stays compiled (K187). Choosing pages or groups is Phase 3-5.

**Claude's design notes, not yet agreed.**
- Files beside the settings file on LittleFS, about 10-15 KB each.
- Restore first saves the current state ("Before restore"), reads the backup through before swapping
  it in, and refuses one that will not read, leaving the live file untouched (A17).
- The arranged layout is saved per page with the screen's fingerprint: used as is on the same board,
  arranged again on another.
- Every file write on the settings task, never an HTTP handler (LESSONS).
- PC access through query parameters on existing routes (the P4_5 serves 7 of its 8 HTTP routes).
- Phase 3's export and import should use the same format.

**Open.**
- `system.ha.known` (what HA has already offered): restoring it would make "N new" offer old
  entities again, while K187 says a backup holds the whole file.
- Where Backup and Restore sit in the system panel, and every word on screen.
- A board with a battery or attached sensors may make a backup less portable.

**Reset's leftovers.** `/popup?dash=2` still wipes the House page without asking (kept for
development), and `CardAdded::resetToCompiled()`'s `RESET_IDS` holds 64 against 128 made cards. Both
go when Backup and Restore replace them.

## 5. Restore and a card's history (K198, #118)

**As built.** Restore follows K137's rules - a copy deleted, a moved card sent home, a regrouped card's
group given back. A group made on the board is deleted when no card is in it, with its label group
(K200); a group a member was moved out of keeps that member and is hidden until a card returns.

**What the owner said.** Restore matters less now that cards are easy to add, move and group. If it
is developed, it goes one move back, from a record of where each card lived and with what settings;
longer histories belong with Backup and Restore. Deleting anything else belongs to the web app, where
undo is easier (K117).

**Known gaps, for #118.** A card moved out of a group onto the page has no way back; restoring a
hidden group's card makes a copy; Restore offers the place before last.

## 6. Smaller pieces left out of 2.13

| Piece | Where it stands | What is needed |
|---|---|---|
| **Units and precision** (K177) | The device-wide unit is °F (`CardIcons.cpp`), the climate unit fixed in `HaValue.h` | Claude's proposal: card -> UI Tweaks -> Inherit, the page's `tempUnit` retired, precision read per card from HA's entity registry. Open: does anyone need the page's unit; for an MQTT sensor, does Inherit mean its own unit or HA's; does the card's Units row stay |
| **The thermostat's controls** (K144) | The window is read-only (`PopupClimate.cpp`); built only with the owner present, since it commands the house. Phase 6, or a one-off build | Settle first: the step (1 °F / 0.5 °C), limits from HA's `min_temp` / `max_temp`, the wait after the last tap (Claude: 3 s), the setpoint dimmed until HA confirms, the old value and a toast if HA refuses |
| **The tap cycle for a group's existing members** | Only cards being added reach the header step | Claude's ring: unpinned -> pinned -> header (sensors) -> removed (made cards) -> back. Open: where remove sits; a header member's first tap |
| **Panel's wide values** | Tiles in a group are one size | Shorter values with the text fitter ("312 KB", "3d 4h"), a per-member span when another group needs one. Open: the IP address at tile width |
| **The arranger when the header comes back** (K167) | A page arranged with the header hidden may shrink its cards when the header returns. To confirm on the glass | Claude: always arrange for the room with the header shown |
| **The text fitter beyond Edit cards** (K152) | Edit cards only | One face per group, the largest that fits every tile name. Open: one long name shrinks the group, or is cut with "..." |
| **Two levels of nesting** (K147) | `nested()` allows one | Needed by §1 |
| **New group from the page's own cards** | New group offers HA's areas | - |
| **Active state in a member's own view** (K166) | Starts from the held card's value, not the member's | A fix |
| **`Popup/PopupCluster.cpp`** | Over 1,000 lines by the structure check | Move Edit cards' save and dialogs into a file of their own |
| **The golden harness** (`scripts/ui_golden.py`) | Its states predate the made-card dashboard | Re-run on the dev boards, fix what it trips on |
| **Checks with HA running** | Not done; each changes HA, so the owner approves each | "N new" (an HA label added and removed); a group's all-off with no HA light group for the room; `unknown` after an HA restart (#84) |
| The card limit's toast (K176) | A later ordinary toast replaces it | Accepted until 2.14's alerts |
| All Lamps (four entities on one card) | Cannot be a made card; not in `test_cards.json` | - |
| A converted group's member order | Follows the settings file | - |

## 7. Home Assistant's side

Nothing was built in HA in 2.13; `FLEET_REFERENCE.md` rule 3 and `HOUSE_RULES.md` §7 were reworded
for K133.

**Proposed for 2.14, which depends on it:**
- the YAML conversion (K136): the logical and status layers as hand-written files by function, like
  `packages/sprinklers/v4_*`, and `house_logical.py` retired; the file layout to agree;
- doors that appear twice (rename leftovers);
- `delay_on` and `last_changed` resetting on an HA restart (a trigger-based template, or Spook's time
  in state);
- what HA 2026.11 and Spook now offer.

Per-room status entities were thought unnecessary, since the boards count from a group's members;
the brainstorm's roll-ups per area bring them back.

**Owed:** HA's milestone log (`projects/2026-10_fleet-2.13/`) saying what changed (little), and the
tag `fleet-2.13-done` (K135).

**The owner's own jobs:** rebuild Desk as an HA light group (K179); three lights (Sink, Table, Porch)
read unavailable in HA, and the Tuya switches were offline.

## 8. Words and looks to confirm

- "Built in": the mark Edit cards shows on Panel's row (K171).
- Card margins' steps, Small / Medium / Large / X-large, as 50-200% of the medium gap.
- Where Card UI's Card state row sits.
- Whether UI Tweaks' "Group view" row becomes a section, as Card UI's did (K183).
- Every on-screen "group" re-read against K146 / K158.
- A humidity's decimal, the 4B's °F, long names in tiles; the 4B's slow HTTP after a restore.
