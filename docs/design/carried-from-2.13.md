# What 2.13 set aside, and why

Written 2026-11-07, at 2.13's merge gate. 2.13 set out to add cards from Home Assistant (#116) and
grew into retiring the compiled dashboard (K171), the group's solo card (K188-K191) and much of how
groups behave. On 11-06 the owner closed its scope (K192) so it could merge. This file gathers
everything that was intended for 2.13, or raised while building it, and did not ship. For each: what
was intended, what was said on both sides, what was decided, what made it wait, and what is still
open.

`PLAN_2.13.md` and `TEST_2.13.md` are archived unchanged at the merge and keep the full trail. This
file is the live one. The K-numbers are `DECISIONS.md`. The owner's five brainstorm ideas of 11-07
are in `FUTURE_IMPROVEMENTS.md`, "Brainstorm, 2026-11-07", and are linked here where they touch.

**The owner's framing, kept because it steers all of this:**
- "A group is a group is a group": one object, shown three ways (solo card, group view, group on the
  page; K88, K90).
- **On 11-07 he asked to stop designing the boards around kinds of card that do not exist yet.**
  Summaries and alerts need not fit inside the group card just because they hold several entities
  (K189). He was feeling bogged down, and closing the scope was part of the answer.
- **A predecessor's line he quotes:** nobody will stand in the garage tapping for 20 minutes to
  rearrange a dashboard; they will do it in the companion web app in 20 seconds. **So not every
  complex card needs a full editor on the glass** (K182). Editing groups on the glass already went
  far past what he expected.

---

## 1. HA group entities as board groups (K170), and the mirror (K184)

**Intended.** K170 (11-04): every HA group entity (Desk `light.office_lights_hue`, Bedroom
`light.eric_bedroom_lights_hue`) becomes a group on the board, through a group made from the HA
group's members. It shows as a Solo card by default and gets the GROUP panel, Show as and Edit
cards. One command goes to the HA group entity and to any card added to it.

**Why it waited.** Claude's report of 11-06 found more to it than the decision assumed:

- **"Every HA group entity" needs a definition first.** 20 HA entities carry a member list
  (`attributes.entity_id`), and only 3 are the intended kind:
  - the Hue rooms and zones: Office/Desk, Bedroom, and Indoors (all five lamps);
  - 7 one-member aliases from the logical layer (Back Door, Front Door, Garage North / South / House
    Door, Attic Hatch, 3D Printer);
  - groups of groups three deep (Perimeter Doors, Leak Detected, which contains Kitchen Leak);
  - totals and averages (House Temperature and Humidity, power and energy). These are summaries,
    not groups.
- **Today the board has two separate ideas. K170 makes Desk both at once:**
  - **a card on an entity:** Desk on `light.office_lights_hue`, its lamps learnt from the member
    list, with no cards of their own;
  - **a group of cards:** pins, a header, Edit cards, moves.

  Every place that asks "group or entity card?" needs a third answer. What that moves:
  1. **Membership has two owners.** The Hue app owns Desk's members, and the board would keep a
     copy. A lamp added in the Hue app leaves that copy stale. Worse, removing a lamp on the board
     cannot remove it from the Hue group, so "all on" still switches it.
  2. **The face has two sources:** HA's group state, and the board's own reading of its members.
  3. **Scenes belong only to the Hue group.** A lamp added on the board takes no part in a scene.
  4. **A group inside a group in the settings file.**
     - `readMember()` refuses a member with no entity.
     - `nested()` (`Popup/PopupMove.cpp`) still enforces one level; K147 allows two.
     - Moves and Restore record a member as one card.
     - Edit cards, the group view and the back path ("Office > Desk > lamp") each gain a level.
  5. **One lamp in two groups.** Indoors holds Office's and Bedroom's lamps, so each lamp would get
     two cards with two sets of settings.
  6. **The 128-card limit.** The three Hue groups would cost 13 cards per board. Counting the door
     aliases, about 30 more. The dev boards hold 30-37 cards now.
  7. **K133.** A group made automatically at boot would be the board writing its own dashboard
     without a tap.
  8. **Converting Desk** in every board's file, its saved choices moved under the same id.
  9. **A name clash.** HA calls the Desk group "Office", which would arrive as a second Office.

**What the owner said (11-06).**
- **The Hue groups will never be edited** (K179).
  - Desk becomes a native HA light group. It is his job to rebuild it, with the few scenes he wants
    recreated in HA.
  - Eric Bedroom stays a Hue group for its dynamic scenes, and is the only Hue group left.
  - Indoors is legacy and is not offered. "All indoor lights", if ever wanted, would be an HA script
    or template.
  - A read-only member list is acceptable for a Hue group, and he still wants to open it and see its
    members.
- **The mirror** (K184): adding to an HA group (Desk plus Office Overhead, say) makes a group on
  the board that looks and behaves as the original and holds the longer member list.
  - On and off reach every member.
  - A Hue scene reaches only the Hue group's members.
  - HA's own members stay read-only.
- **Controls on a mixed group** (K185): every control any member has. His example is the kitchen's
  Sink and Table, only one of which dims. Toggling both is the usual case. The slider still belongs
  on the group, and he can turn the Sink off in its own window first.
- **Temperature and door groups open as groups** (K186), so each member's History is a tap away. A
  group's own History shows its members together. Being discrete HA groups is what gives them a
  group History.

**Decided.** K170, narrowed by K179; K184; K185 (built since 2.6, `PopupLight.cpp`); K186. **K192
moved all of it out of 2.13.** Claude had proposed a short "2.13b" right after the merge; that is not
scheduled.

**Still open.**
- **Which entities count.** Claude's proposal: any HA entity listing 2 or more members, its members
  read-only and its face HA's own value; one-member aliases stay plain cards.
- **When the board group is made:** at boot, or when its card is made. Claude's proposal is when its
  card is made, which keeps K133.
- **When HA's member list changes** (a lamp added in the Hue app): it joins unpinned with a notice,
  joins pinned, or the board asks.
- **Which domains:** switch groups, old-style `group.*`.
- **Whether Bedroom is a page card on every board**, or only on the P4_5.
- **Once Desk is an HA light group**, check that its HA scenes are offered.
- **Waiting on this:** TEST_2.13 open item 8, K191's "Face greyed for an HA group entity" (no HA
  group entity is a board group yet).

## 2. Boards writing to HA: mirroring board groups into HA, Spook (K180)

**The owner's ideas** (11-03 to 11-06):
- Each board group as an HA group entity, so HA shows every board's groups. Its header members would
  be in its attributes, or a companion helper would hold its settings.
- From there: manage the fleet from HA, sync a group across boards, deploy the same groups to several
  boards (Phases 3 and 4).
- He considered labels such as `Fleet-<device>-<group>` and found them messy: each entity would
  carry a label per board.
- **Mirroring whole board groups as HA groups does not work:** an HA group holds one domain only, and
  the old-style YAML groups look like more trouble than they are worth.

**Spook.**
- `group.add_members`, `group.remove_members` and `group.set_members` work only on groups made in
  HA's interface (group helpers), never on Hue groups.
- The owner reads the reference's "while Home Assistant is running" as "takes effect at once,
  permanently", with no YAML reload needed. Not yet verified.
- What it would cost:
  - the firmware depends on a custom integration;
  - the board's HA user may need permission for these actions;
  - a tap on one board changes a group that the phone and the automations use too.
- **The owner's reading of K133, kept for then:** K133 is about his changes in HA not appearing on
  the boards by themselves. Objects a board creates in HA do not break its spirit.

**Decided.** K180: the boards keep their own configuration. Nothing is mirrored into HA, and no
board edits HA's groups, for now. To revisit with the web app (#87) and ESPHome: remote management,
fleet sync. **See also** brainstorm ideas 2 (live member reports) and 5 (synchronizing boards).

## 3. Summary cards (K182, K189) and roll-ups

**Intended.** PLAN 2.4: Garage - Doors becomes an ordinary group, read from roll-up entities in HA.
Deferred to 2.14 on 11-05. Discussed at length on 11-06.

**What exists.**
- **On the board:** the compiled Garage - Doors is a `SummaryCard` (`CardCatalog.cpp`). Its face
  rule:
  - one member active: its short name (North);
  - all of two active: "Both"; all of more than two: "All";
  - some active: a count ("2 open");
  - none active: the card's name.
- **In HA:** `binary_sensor.garage_doors` has an `open` attribute (none / North / South / Both), and
  `sensor.doors_open` has `names` and `entities`. The boards read neither.

**Claude's options (11-06).** The face could be computed in three places:
- **A, an HA template entity:** shared by every board, the phone and 2.14's alerts, but written in
  YAML.
- **B, a board framework:** you choose a rule on the glass, but it lives on one board until Phase 3.
- **C, the web app:** an editor for B, not a separate system.

Claude recommended B, with A as one of its sources:
- **"Summary" as a third Show as**, with rules: Names, Count, Any, and Average / Lowest / Highest /
  Total.
- **An HA entity with members as a summary from HA.** This is the same mechanism as §1's mirror.
- **Words from an attribute** (`open` -> North) as an option.

**What the owner said.**
- **A summary need not resemble a group card** (K189). Keep what a group has for its members: the
  Members list, a tap into any member, perhaps opening as a group view. **Its face follows a rule
  chosen for its purpose.**
- **One framework, shared by every board.** Ideally a summary is defined in HA and added from Edit
  cards. Perhaps HA could even hold YAML files as plain storage the boards read, valid HA or not.
- **What is built in HA should be usable in HA too.** Fleet-only entities, templates and scripts are
  allowed where that cannot be. Nothing in `house_rooms.yaml` / `house_status.yaml` is set in stone:
  names, groupings, templates and alerts can all change.
- **The summaries he expects to use:**
  - **Garage doors open:** a solo card with members; the face shows one door's name or "Both";
    Details holds open and close buttons; History for both doors, or each one from its own window.
  - **Lights on, per room:** a header sensor in each group, shown only above 0. Its header panel
    row taps through to the lights, and each light works like any other card.
  - **Garage door night alert:** a door open more than 5 minutes at night, as a system header alert
    with history; Members lists the doors.
  - **Perimeter door left open:** open longer than 10 minutes; an alert with history, and Members.
  - **Temperature roll-ups:** the face shows an average or a chosen member. History shows every
    member, each one shown or hidden. **Ideally built on the fly.** `sensor.house_temperature`
    serves the thermostat and leaves out sensors that read high on purpose.
  - Later: irrigation alerts (over-watering, climate).
- **Resizable cards that show more on their face** are Phase 6 territory.
- **Seen on the glass (11-06):** a group of temperatures with Face "All members" shows its first
  member's value. He wants an average or a chosen member, and the History described above.

**Decided.** K182, K189: summaries are their own kind of card; a group of only sensors waits for that
design; **2.14 builds them with alerts** (K192). Garage - Doors stays compiled until then (K171).

**Still open.**
- Whose words a summary shows: the board's (the Names rule, card names), HA's (attributes), or both.
- Which rules: Names, Count, Any, Average / Lowest / Highest / Total, or one missing.
- With three of four doors open: "3 open", or their names.
- **Pitfalls Claude noted:**
  - summaries of summaries run into K147's nesting limit;
  - `unknown` after an HA restart counts as a state (#84);
  - an average over members in different units;
  - state must never be shown by color alone;
  - long words need the text fitter.
- **See also** brainstorm idea 1 (roll-up entities per HA area that keep up when a light changes
  room).

## 4. Backup and Restore (K181, K187) - the next milestone

**How it arose.**
- The owner (11-06) had hardly ever used Reset. Since the dashboard is now quick to rearrange and to
  fill from HA, losing a setup worries him less.
- **His idea:** save a layout on the board and restore it, with several saved, to switch between
  page layouts quickly.
- **Why Reset had to change anyway:** since K171 it deleted the whole House page.

**Decided.**
- **K181:**
  - a full backup (pages, groups, cards and settings), restored after a flash, before Phase 3;
  - 3 to 5 kept, named `<device_name>-<timestamp>`; a rough UI is fine;
  - backward compatible, and a backup from one board restores on another;
  - choosing pages or groups, or objects without settings, is Phases 3-5.
- **K187:**
  - Backup and Restore take Reset's place in the system panel;
  - a backup holds the whole settings file, including cards made from other boards' MQTT entities
    (the deck sensors);
  - the board's own MQTT discovery (Panel) stays compiled;
  - a "start over" Reset that wipes the page may come back later.
- **K192:** it is its own milestone, right after 2.13.

**Claude's design notes (11-06), not yet agreed.**
- Files beside the settings file on LittleFS, about 10-15 KB each.
- Restore first saves the current state automatically ("Before restore"). It reads the backup through
  before swapping it in, and refuses one that will not read, leaving the live file untouched (A17).
- The arranged layout is already saved per page with the screen's fingerprint. On the same board it
  is used as is; on another board the page arranges again.
- Every file write runs on the settings task, never an HTTP handler (LESSONS).
- The P4_5 serves 7 of its 8 HTTP routes, so PC access goes through query parameters on existing
  routes.
- Phase 3's export and import (K131) should use the same format.

**Still open.**
- **`system.ha.known`** (what HA has already offered). Claude: restoring it would make "N new" offer
  old entities again. K187 says a backup holds the whole file. To settle.
- Where Backup and Restore sit in the system panel, and every word on screen.
- **The owner (11-06):** could a backup keep the page's arrangement, so a restore need not re-arrange?
  On the same board it already does (above).
- **Hardware will matter later.** A board with a battery or attached sensors may make a backup less
  portable.

**Reset's leftovers.**
- The button is gone (K192), but `/popup?dash=2` still wipes the House page without asking. It was
  kept on purpose for development; it is one stray URL away.
- `CardAdded::resetToCompiled()`'s `RESET_IDS` is 64 against 128 made cards, so a reset of a full
  board would be partial.
- Both go when Backup and Restore replace them.

## 5. Restore, a card's history, and deleting groups (K197, K198)

**What the board does today.**
- **Restore** follows K137's rules (a copy deleted, a moved card sent home, a regrouped card's group
  given back), fixed for made cards in 2.13.
- **A group emptied by a move** hides itself (the owner's 15: "hidden automatically but can be used
  again").
- **A group emptied in Edit cards** is not drawn, and is not listed under Hidden.
- **Nothing on the board deletes a group** (K197).

**What the owner said (11-07).**
- Restore matters less now that cards are easy to add, move and group.
- **If kept, it goes one level back**, from a history of where each card lived and with what
  settings. **One move of history is fair.** He cannot find a good reason for more: a group is quick
  to rebuild, header sensors included, and his HA is small. Detailed histories belong with backup
  and restore.
- **Delete belongs to the web app**, where undo is easier.
- **His earlier "keep groups even when empty or removed"** dates from when compiled groups were all
  there was. He asked on 11-07 whether it still holds.

**The case, as of 2.13** (Claude, 11-07, answering that question):

*For keeping an empty group:*
- Its name, color, HA area link, header choices and Card UI choices survive, ready to refill.
- Cards whose label is that group keep it.
- An accidental "remove the last member" is undone by adding a card back.

*Against:*
- **It is hard to refill.** An emptied New group is not on the page or under Hidden. Not yet checked:
  whether Add to group still offers it as a target. It does show up in the label choices, as one more
  "New group".
- **It costs a made-card slot for good** (1 of 128; K176), and there is no delete on the board.
- **It takes a place in the group list,** which is capped at 32 entries (page areas, HA areas and
  saved groups). Colors for new groups repeat sooner, because the board picks the least-used color.
- **Identical "New group" entries** in the label choices cannot be told apart.
- **Its saved face and tap can point at members that no longer exist.**

The 1060 shows it: four empty "New group" groups from the 11-07 tests (100-200 bytes each in
`cards`, about 70 more in `groups`, each holding a slot), and a label group "Zuggin" that no card uses.

*Claude's recommendation:*
- A group emptied in Edit cards is deleted, entry and all, along with its label group if no card
  wears it, as `dropIfEmptySolo` already does for solo labels.
- A group emptied by a move does the same once Restore is reconsidered.
- **This changes K197's "an empty group stays hidden".** Not agreed; nothing built.

**Still open (with Backup and Restore, or the web app).**
- Restore's history of one: what it records (place, group, label, card settings), and what Restore
  does with it.
- TEST_2.13 open item 5: a card moved out of a group onto the page has no way back.
- Item 7: a hidden group's card restored makes a copy.
- R6: Restore offers the place before last.
- Whether an emptied group is deleted (above), and cleaning up the 1060's leftovers.

## 6. Pieces planned for 2.13 that K192 left out without naming

K192 named what was in 2.13 (K188-K191, Reset removed, Restore fixed). It did not say where these
went, so they are listed here until each is given a home.

| Piece | Where it stood | What is needed |
|---|---|---|
| **The thermostat's controls** (K144, K153; mock boards 14-15) | Not built. The window is read-only (`PopupClimate.cpp`); built only with the owner present, since it commands the house | Settle first: the step (1 °F / 0.5 °C), limits from HA's `min_temp` / `max_temp`, the wait after the last tap (Claude: 3 s), the setpoint dimmed until HA confirms, and the old value plus a toast if HA refuses (words his) |
| **Units and precision** (K153, K177: UI Tweaks, Inherit / F / C) | Not built. The device-wide unit is fixed at °F (`CardIcons.cpp`), the climate unit hard-coded (`HaValue.h`) | Claude's D3: card -> UI Tweaks -> Inherit, the page's `tempUnit` retired, precision read per card from HA's entity registry in the background. Open: does anyone need the page's unit; for an MQTT sensor, does Inherit mean its own unit or HA's; does the card's Units row stay |
| **The tap cycle for a group's existing members** (K168, D4) | Only cards being added cycle through the header step | Claude's ring: unpinned -> pinned -> header (sensors) -> removed (made cards) -> back. Open: where remove sits; a header member's first tap |
| **Panel's wide values** (K171, D5) | Tiles in a group are one size | Claude: shorter values plus the text fitter now ("312 KB", "3d 4h"), a per-member span when another group needs it. Open: the IP address at tile width; shortened formats everywhere or only in Panel. Mocks owed |
| **The arranger when the header comes back** (K167, D6) | A page arranged with the header hidden shrinks its cards when the header returns, against K167. To confirm on the glass | Claude: always arrange for the room with the header shown. Open: does any page keep its header hidden for good |
| **The text fitter beyond Edit cards** (K152, D7) | Edit cards only | Claude: one face per group, the largest that fits every tile name. Open: one long name shrinks the group, or is cut with "..." |
| **Two levels of nesting** (K147) | `nested()` still enforces one | Needed by §1 |
| **New group from the page's own cards** (PLAN 3.6, K91) | New group offers HA's areas only | - |
| **The Active state row in a member's own view** (K166) | It starts from the held card's value, not the member's | A fix |
| **`Popup/PopupCluster.cpp`** | 1,010 lines by the structure check (over its 1,000), with `GUIManager.cpp` 1,546 and `Card.cpp` 1,489 | Move Edit cards' save and dialogs into `PopupEditCards.cpp` |
| **The golden harness** (`scripts/ui_golden.py`) | Its states predate K171; not re-run since | Re-run on the three dev boards, fix what it trips on |
| **Checks with HA running** (PLAN 3.7) | Not done; each changes HA, so the owner approves each | "1 new" (an HA label added and removed); a group's "all off" with no HA light group for the room; `unknown` after an HA restart (#84) |
| The card limit's toast (K176) | A later ordinary toast replaces it | Accepted until 2.14's alerts |
| All Lamps (four entities on one card) | Cannot be a made card; left out of `test_cards.json` | - |
| A converted group's member order | Follows the settings file, which may differ from the compiled order | - |

## 7. Home Assistant's side (PLAN §4, D9)

**Nothing was built in HA in 2.13.** Only `FLEET_REFERENCE.md` rule 3 and `HOUSE_RULES.md` §7 were
reworded (K133, HA commit `d8293c5`).

**Claude's D9 recommendation, never answered as such:** move these to 2.14, because alerts depend on
them.
- the YAML conversion (K136: the logical and status layers as hand-written files by function, like
  `packages/sprinklers/v4_*`; `house_logical.py` retires; file layout to agree);
- doors that appear twice (rename leftovers);
- `delay_on` and `last_changed` resetting on an HA restart (a trigger-based template, or Spook's time
  in state);
- reading what HA 2026.11 and Spook now offer.

**Per-room status entities** (HA choice 3) were "probably not needed", since the boards count from a
group's members. Brainstorm idea 1 brings them back as roll-ups.

**Owed at the merge:** HA's milestone log (`projects/2026-10_fleet-2.13/`) says what changed (little),
and the tag `fleet-2.13-done`.

**The owner's own jobs:**
- rebuild Desk as an HA light group (K179);
- three lights read unavailable in HA on 11-05 (Sink, Table, Porch), and the Tuya switches were
  offline on 11-07.

## 8. Words and looks never confirmed

- "Built in": the mark Edit cards shows on Panel's row (K171).
- Card margins' steps, Small / Medium / Large / X-large, as 50-200% of today's gap.
- Where Card UI's Card state row sits.
- Whether UI Tweaks' "Group view" row becomes a section, as Card UI's did (K183).
- Re-reading every on-screen "group" against K146 / K158 (PLAN 2.7).
- Round 1's knowns: a humidity's decimal, the 4B's °F, long names in tiles; and the 4B's slow HTTP
  after a restore. To recheck on the glass.
- Issues: #116 to bring up to date from this file and close; #52 (auto-sort) to close if superseded.
