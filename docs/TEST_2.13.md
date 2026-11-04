# Test sheet - 2.13, cards from Home Assistant (rounds 1 to 3)

# Round 3

**Branch** `feat/2.13-add-from-ha` at `1edb552`. **Boards** WS_P4_5 and WS_P4_4B run it (app only,
each board's settings saved first in `bench/settings/<board>/20261103-182*.json`, 0 differences
after). **CYD_P4_1060 is not flashed**: its USB link dropped as the flash began (esptool could not
open COM9 at any rate) and it stopped answering - replug it holding BOOT (LESSONS, "A dark screen
after a good flash"); its app was not written, so it still has round 2. CYD_P4_4880 runs round 2;
WS_S3_4B compiles (#69; RAM 47.0%, flash 37.3%). **Decisions** K154-K158.

The P4_5 tests marked *(run)* were driven by Claude over `/popup` with screenshots, then the P4_5's
settings put back (0 differences). **Taps and long presses on the glass are untried** - `/popup` has
no way to tap a tile.

## What changed on screen

- **The group view is the cluster** (K154): Office drawn as on the page - its header and its tiles,
  the hidden ones last, dimmed, "Hidden" - with no title bar and no X, sized to fit its tiles at the
  page's tile size, a gap above the panels. A tap outside closes it.
- **Its tiles act as on the page**: a light's tap switches it, a sensor's opens its History; a long
  press opens that member - in this window. A tap on its header opens Members.
- **A group keeps its GROUP panel in every presentation** (K155): Rename group, Header options,
  Edit cards, Show as, Face, Area - Header options greyed as a solo card, Face greyed as a cluster.
  SETTINGS no longer has Show as or Face.
- **Tap action and Long press per presentation** (K156): a solo card's choices and the cluster's are
  kept apart. A cluster ignores a tap it does not offer: P4_5's Office, which had kept a solo card's
  Toggle, opens its group view again.
- **Members of a solo group card list every member**, the header's sensors with their glyphs (K157).
- **Desk in Office** (K157): Members > Office Desk opens "Office > Desk" with Desk's Members tab;
  its lamps open "Desk > lamp". The back arrow steps back one level at a time.
- **Members past the sixth** open: the rows come from the group, not the window's six entities.
- **Edit cards**: a row ticked to add or restore looks like a lit chip; the list showing in the
  column is glass, not lit.
- **Label words** (K158): adding cards to the page asks "Select label for these cards:" (Area, the
  page's labels, New label, None); its dialog says "The cards will be labeled Garage."; Change label
  says "This card's label will change from A to B."; Properties says Card label.

## Tests

**T1 - The group view.** Tap Office. *PASS:* a window that is Office's cluster - swatch, Office,
occupancy and temperature, Desk and Overhead tiles the size they are on the page - centred, no title
bar, no X, a gap above the GROUP and SETTINGS tabs. Tap outside: it closes. *(run, opened by
/popup)*

**T2 - Its tiles.** In the group view tap Overhead: it switches, as on the page (Office lights only;
switch it back). Long-press Desk: "Office > Desk". Back arrow: the group view again.

**T3 - Its header.** In the group view tap Office's header: Members. Tap Office in the title: the
group view.

**T4 - Hidden members.** Hide one of Kitchen's tiles (Edit cards, Hide). Tap Kitchen. *PASS:* the
hidden one is a tile, last, dimmed, "Hidden"; the window is wide enough for both. Restore it. *(run:
Sink hidden through /settings)*

**T5 - Desk in Office.** Office > Members > Office Desk. *PASS:* "Office > Desk", with the Members
tab beside History. Tap it: Desk's three lamps. Tap one: "Desk > " and the lamp. Back arrow twice:
Desk, then Office's Members. *(run: the first step)*

**T6 - Solo card keeps GROUP.** Office > GROUP > Show as > Solo card, close the window. Long-press
the Office card. *PASS:* the GROUP tab is there; Rename group, Header options greyed, Edit cards,
Show as says Solo card, Face, Area. SETTINGS has Tap action and Long press opens, no Show as or Face.
*(run)*

**T7 - Solo Members.** On the solo Office card, tap the Members tab. *PASS:* Office Desk, Office
Overhead, Office Occupancy and Office Temperature, the last two with their header glyphs. *(run)*

**T8 - Edit cards from a solo card.** GROUP > Edit cards. *PASS:* In this group lists the four, glass
in the column. *(run)*

**T9 - Tap per presentation.** As a solo card, SETTINGS > Tap action > Toggle. GROUP > Show as >
Cluster, close. *PASS:* a tap on Office opens its group view. Back to Solo card: a tap switches the
lights.

**T10 - Chosen rows.** Edit cards from the page, tick a card in an area. *PASS:* it looks like a lit
chip (tinted, an accent edge), not lit glass; red for a row ticked to hide is unchanged.

**T11 - Label words.** Edit cards from the page, add two cards from Garage. *PASS:* "Select label for
these cards:", New label, None, and the note; Next: "The cards will be labeled Garage." On a page
card: Visibility > Change label, pick another: "This card's label will change from ... to ...".

## Not built yet

Units and precision from HA; Removed for built-in cards; the text fitter outside Edit cards; the
two-level nesting rules; New group from the page's own cards; the thermostat's controls (with you
there).

# Round 2

**Branch** `feat/2.13-add-from-ha` at `d8d3d26`. **Boards** WS_P4_5, WS_P4_4B, CYD_P4_1060 and
CYD_P4_4880 run it (flashed 2026-11-03 04:50-05:00, app only; each board's settings saved first in
`bench/settings/<board>/20261103-04*.json` and unchanged after - the 4880 only learnt HA's list, as round
1 did). WS_P4_7B was not plugged in; WS_S3_4B compiled only (#69) - it builds (RAM 46.9%, flash
37.3%). **Decisions** K146-K153; the look:
artifact "2.13 Round 2 Mocks" (version 17).

Built unattended overnight with your permission. The P4_5 tests below marked *(run)* were driven by
Claude over `/popup` with screenshots, then the P4_5's settings put back (0 differences). **Nothing has
been tried by hand on the glass.**

## What changed on screen

- **The group view keeps its panels** (GROUP, SETTINGS); its title row has no buttons; the group's
  name in the title goes between the group view and Members (Members' own tab lit).
- **CLUSTER is GROUP** (K146): Rename group, Edit cards, Show as, Header options, Area. A tile's
  Visibility says Hide from the group, Change group; a page card's says Change label, Add to group.
  The move dialogs say group.
- **Rename group renames the group**: every card wearing it follows; its color stays.
- **Edit cards** (K148) from a group's GROUP panel or the page panel's CARDS: one window over the
  page, the lists in a column (In this group / On the page / Hidden, then HA's areas), the result
  shown as you tick - dashed rows being added, red rows going.
- **The page panel**: CARDS is Edit cards and New group; HIDDEN is gone (hidden cards are in Edit
  cards).
- **Show as** (Cluster, Solo card) in GROUP; a group shown as one card has **Show as** and **Face**
  in its SETTINGS. **Long press opens** is in every card's SETTINGS.
- **Names fitted to their room** (K152) in Edit cards: a name too long for its row steps down to the
  tag's face before it is cut; rows keep one height.

## Tests

**R1 - The group view.** Tap Office. *PASS:* the window fills the page under the header, GROUP and
SETTINGS tabs under it, no buttons beside the title, which says Office. Tap Office in the title:
Members, its bulbs tab lit. Tap Office again: the group view. Open GROUP from the group view. *(run)*

**R2 - Edit cards from a group.** Office > GROUP > Edit cards. *PASS:* the window fills the page;
the column has In this group (4), On the page, then House's and Outside's areas; In this group lists
Desk, Overhead, then Occupancy and Temperature flat, "In the header". Tick Desk: red, a tick, the foot
says Hide 1 (no Remove: Desk is built in). *(run)*

**R3 - One kind of tick at a time.** With Desk ticked, open On the page and tap Table. *PASS:* "Cards
cannot be selected to add while cards are selected to hide." and the rows there are flat and dim.
Untick Desk: they come back. *(run)*

**R4 - The result as you tick.** Open Office (the area), tick Office Motion PIR Occupancy; On the
page, tick Table; back to In this group. *PASS:* In this group says "4 +2", On the page "1", Office
"1 / 11"; two dashed rows at the foot of the members, "From Office" and "From the page"; the foot
has Copy / Move and Add 2. Add 2: "Add cards to group?" in the dialogs' sentences, Cancel and
Confirm. Confirm: both are tiles in Office. *(run)*

**R5 - Remove.** Office > Edit cards, tick the two cards R4 added. *PASS:* both red; the foot has
Remove 2 and Hide 2. Remove 2: "Remove these cards?"; Remove: both gone. *(run)*

**R6 - Hide and restore a member.** Edit cards, tick Overhead, Hide 1, Hide. *PASS:* Overhead is no
tile on the page, still in the group view (dimmed, Hidden). Edit cards again: Overhead last, "Hidden";
tick it: Restore 1 - back as a tile.

**R7 - Edit cards from the page.** Tap House. *PASS:* PAGES, HEADER, CARDS (Edit cards, New group,
"Hidden cards are viewed and restored in Edit cards"), no HIDDEN. Edit cards: On the page lists
every card, groups marked Group; Hidden; the areas. *(run)*

**R8 - Add to the page, the group step.** Garage area, tick Garage Light Attic and Garage Overhead
Lights, Add 2. *PASS:* "House > Add to dashboard", "Select group for these cards:", Garage * lit, the
page's groups, New group, None, the note, Next. Next: "Add cards to dashboard?", "...placed onto the
House page." "The cards will become members of the group Garage." Add: two cards on the page. Then
tick both under On the page: Remove 2 (made here) - gone. *(run)*

**R9 - Cards from two areas.** Tick one from Garage and one from Kitchen, Add 2. *PASS:* the step
starts with Area, lit; Next says "Each card will become a member of its Home Assistant area's group."

**R10 - Hide and restore from the page.** Tick Eric under On the page, Hide 1, Hide. *PASS:* gone
from the page; Edit cards > Hidden lists it, tick it, Restore 1: back. *(run)*

**R11 - Rename group.** Office > GROUP > Rename group, type Study, Save. *PASS:* the window, the
cluster's header and every card wearing Office say Study, in Office's purple. Rename again with the
field empty: Office. *(run)*

**R12 - Show as and Face.** Kitchen > GROUP > Show as: Solo card, close. *PASS:* Kitchen is one card
named Kitchen. Its SETTINGS has Long press opens, Show as (Solo card), Face (All lights, then each
member). Face: one member - the card shows that one. Show as: Cluster, close: the cluster is back.
*(run, without Face)*

**R13 - Long press opens.** Any card's SETTINGS > Long press opens: History view; close; long press
it. *PASS:* its window opens on History. A cluster's choices are Members view, Group view, History
view. Toggle is never offered.

**R14 - The other boards.** On the 4B, the 1060 and the 4880: R1, R2 and R7. *PASS:* everything
fits; on the 4B the rows are one column.

**R15 - Names fitted.** On the 4B, Edit cards from the page, the Garage area. *PASS:* "On the page",
"Eric Bedroom" and "Garage Motion Occupancy" are whole, in a smaller face than the names that fit
already; every row the same height; only a name too long for either face ("Garage Overhead ...") is
cut. *(run)*

## To confirm (words and choices Claude made)

- Show as's **Cluster / Solo card**, **Face** with **All lights** (All members when none switch),
  **Long press opens** and its **Details view / Members view / Group view / History view**.
- The step's **Area** (cards from several areas); its dialog line "Each card will become a member of
  its Home Assistant area's group."
- Toasts after an action: "Added N to Office", "Hid N in Office", "Removed N from Office", "Restored
  N to Office", and on the page "Added N", "Hid N", "Removed N", "Restored N". Restore asks nothing.
- The page's Hide dialog: "Hidden cards can always be restored from Edit cards." A hidden group's
  Visibility note: "Hidden groups can be restored from the page's Edit cards." A member's Hide
  dialog: "This card can be restored from the group's Edit cards".
- Groups in the page's On the page marked **Group**; "In another group"; "From Home Assistant" for an
  entity with no area.
- Restore N is blue (Confirm); Remove N is a plain button beside a red Hide N.
- Show as is in GROUP for a cluster, but in SETTINGS for a group shown as one card (it has no GROUP
  panel).

## Not built this round

- The group view's header sensors in its title row, and their drop-down (mocks 2 and 3); a header
  panel row tapping as its member (K150's second half).
- **Removed** for a built-in card (K148): Remove is offered only for cards made on the board.
- Units and precision from HA (K153): HA's display precision is not in what the board reads.
- The text fitter anywhere but Edit cards (Members, the group step, the cards themselves).
- Two levels of nesting (K147) and "one block per group": nothing new allows or stops them yet.
- New group from the page's cards (mock 11), and the thermostat's controls (with you there).

## Known

- The group view and Edit cards end where the panels' tabs begin; the page shows in the strip beside
  the tabs.
- Edit cards from a group opens on In this group; its HA area is not brought to the top of the column.
- A cluster renamed before tonight keeps its own words until its next rename.
- The P4_5 once failed to join WiFi after a flash (handshake timeout, then a 45-minute wait); a reset
  fixed it. Not from this round's code.

---

# Round 1

**Branch** `feat/2.13-add-from-ha`. **Boards** WS_P4_5, WS_P4_4B and CYD_P4_1060 run the round-1 build
(flashed 2026-11-02 08:40; the P4_5's settings put back as they were at 05:52 - the one difference is
the new `system.ha.known` list). **Decisions** K138-K145; design `docs/design/ha-cards.md`; the look:
artifact "2.13 Add Cards Mocks".

Built unattended on 2026-11-02 with the owner's permission (flash the three dev boards, Office lights,
HA's docs). Every test below was run once by Claude over `/popup` and screenshots; **none has been
tried by hand on the glass.** Results go under each test.

## What changed on screen

- A **tap on a cluster opens its group view** (it opened Members before); a long press still opens
  Members. A cluster with "Details view" saved as its Tap action opens the group view too.
- The page panel (tap the page name) has a **CARDS** section: **Add card** and **New cluster**.
- A cluster's **Add cards** has two sources, **On this board** and **From Home Assistant**.
- The CLUSTER panel (on Members) has an **Area** row; **Add cards** says "N new" when its area has
  something new.
- UI Tweaks ends with **Show helper text** (a checkbox); a cluster's Card UI has **Card state**.
- A **thermostat** can be added as a card.

## Tests

**T1 - Add cards from HA, in a cluster.** Long press Office > CLUSTER > Add cards. *PASS:* From Home
Assistant is lit; Office's entities are listed, each with its state; Desk's lamps say "In Desk", the
overhead "In this cluster", occupancy and temperature "In the header"; Other areas are chips below.
Tick 3D Printer and Office Humidity: each row lights with a tick, the button says "Add 2". Add: the
window closes, "Added 2 to Office", both are tiles. Reboot: both still there, with HA's names.

**T2 - Other areas.** In T1's list, tap the Hallway chip. *PASS:* Hallway's three entities replace
Office's, Office among the chips. Tick THERMOSTAT and Add: a tile with the room's temperature.

**T3 - The thermostat's window.** Long press that tile. *PASS:* the temperature large with what it is
doing under it ("Idle"); Mode, Heat to, Doing, Humidity, Fan beside it, each matching HA; History
works. On a solo card: "Heat to 62°" where a sensor says "Seen:".

**T4 - Add card to the page.** Tap House > CARDS > Add card. *PASS:* HA's areas in a column under
House and Outside, No area last; Garage shows its eleven, Garage Doors' two doors "In Doors". Tick two
and Add: two cards at the end of the page, in Garage's color.

**T5 - New cluster.** House > CARDS > New cluster. *PASS:* the areas as chips with their counts. Tap
Garage: its entities, lights ticked, one occupancy and the temperature ticked. Create: a Garage
cluster appears with the lights as tiles, occupancy and temperature in its header. Rename cluster
renames it.

**T6 - The group view.** Tap Office. *PASS:* the window covers the page under the header, no panels;
every member a tile, the ones hidden from the cluster last, dimmed and saying Hidden. Tap a light's
tile: it switches. Tap a sensor's tile, or long press any: that member's own view. The tune icon goes
to Members; on Members the bulbs icon comes back. X closes. Check on the 4B and the 1060 too.

**T7 - Area.** On Office's Members open CLUSTER. *PASS:* Area says Office. Choose Kitchen, close, open
Add cards from HA: Kitchen's entities come first. Put it back to Office. Outside's says Outside (HA's
floor) and lists Deck first.

**T8 - Something new.** In HA, add the `fleet` label to an entity in Office that does not have it.
*PASS:* within a few seconds a notice "New in Office: <its name>" with a bar running down; Office's
CLUSTER > Add cards says "1 new"; the entity's row says New. Open the list, close it: "Open" again.
Remove the label afterwards. (Not tested: it needs a change in HA.)

**T9 - Notices queue.** *PASS:* two notices in a row show one after the other, never one over the
other. (Not tested on its own.)

**T10 - Show helper text.** UI Tweaks, untick Show helper text. *PASS:* the notes under Members ("Tap a
member for details"), Visibility, a cluster's views, the page panel and the hidden cards' sheet are
gone; tick it again and they are back.

**T11 - A cluster's card state.** Office's Card UI > Card state: Bracket. *PASS:* once the window has
gone, Office's lit tiles show the bracket while solo cards keep yours; Inherit goes back.

**T12 - Many entities.** *PASS:* with more than ~45 HA entities on a board every one updates (the
subscription goes in parts now). Add ten from HA to one board and toggle a few in HA.

## To confirm (words and looks Claude chose)

- "Group view" in Tap action; the tune and bulbs icons for the two views (grid and list need the icon
  font made again - your call).
- "Added N" (the page's Add card), "Waiting for Home Assistant", the thermostat window's Mode / Heat to
  / Doing / Humidity / Fan.
- New cluster shows one list, not the mock's Tiles and Header sections; the occupancy ticked first is
  the first one HA lists (Garage's motion sensor rather than Garage Occupancy).
- Card state's place at the foot of a cluster's Card UI.
- A thermostat's unit is taken as °F.

## Known, not fixed

- The card window is one size, so a cluster's Add cards shows four rows at a time and scrolls.
- On the 4B the group view's °F sits over a sensor's number.
- A tile from HA keeps HA's full name ("Office Humidity" wraps in a tile); rename it in Card UI.
- A humidity added from HA shows a decimal (43.0 %).
- An area's "1 new" counts the area's own entities, not a linked floor's areas one by one.
