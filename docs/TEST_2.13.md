# Test sheet - 2.13, cards from Home Assistant (round 1)

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
