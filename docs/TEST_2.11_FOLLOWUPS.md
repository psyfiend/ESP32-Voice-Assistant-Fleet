# TEST - the 2.11 follow-ups (your notes on v0.2.19)

Branch `feat/2.11-followups`, off `main` at `v0.2.19`. Four of your five notes from 2026-10-31; the
4880's portrait look and the rotation switcher wait (below).

## What changed

| # | Your note | What it does now |
|---|---|---|
| 1 | Moving a card from one cluster to another took two trips | A tile's Visibility says **Move this card**. Its view shows where the card is ("In cluster:", "Member of group:"), **Copy or move this card?**, then two buttons side by side: **Change group?** (to the page, in the group you pick - what Add card to dashboard did) and **Add to cluster?** (another cluster on this page). Each opens a grid, then **Next**. |
| 1a | A confirmation, showing where from and where to | **Next** opens a dialog: "Move this card?" (or Copy), *From: the Office cluster, in the group Office.* / *To: the Kitchen cluster, in the group Kitchen.* (or *To: the page, in the group ...*). Its edge is the accent; Hide's and Remove's stay red. |
| 1b | The window stayed open after Add card to dashboard | **Every way out closes the window**, copy or move, and the page re-arranges - as Remove does. A group card's member (Desk's lamp) keeps its Add card to dashboard, now with the dialog too. |
| 2 | Which members are in a header slot | Members marks each one with **HEADER** after its name. |
| 3 | Header options said "Eric", Members said "Eric Bedroom Temperature" | **Both lists show the same name**: one you typed on the glass, else HA's full name. The tile keeps its short label. Two members with one name still say which is which. |
| 4 | A swipe on a sheet with nothing to scroll counted as a tap | **In any open sheet, a finger that moves more than 4 mm is never a tap**, whether or not there was anything to scroll. Sliders, the selector and the chart still drag as before. |

### How a move between clusters is kept

- **A card made on this board** (a copy, or one moved in earlier) simply changes cluster. It wears
  the new cluster's group.
- **A dashboard's own member** cannot leave its cluster's definition, so it is marked moved out
  (no tile, not in the hidden cards). A copy goes into the other cluster and remembers it. In the
  first cluster's Members its row says **In Kitchen**. The copy's Visibility offers **Move back to its
  cluster**, which puts the original tile back and deletes the copy.
- A moved member now records **which card it became** (`moved_to` in the settings file). Before
  this, a member that had been copied once and moved once could be "moved back" by the wrong card.
  Moves saved before this build keep working as they did.
- The development reset clears `moved_to` with the rest.

## Try it

On the P4_5 (or the 4B, or the 1060):

1. Long-press a tile in one cluster > SETTINGS > Visibility > **Move this card**. Check the view's
   words. **Add to cluster?** > pick another cluster > **Next**. Check the dialog's From and To.
   Choose **Move**. The window closes; the tile is in the other cluster, gone from the first.
2. Open the first cluster's Members: the moved member says **In <cluster>**.
3. Long-press the moved tile in its new cluster > Visibility > **Move back to its cluster**. It is
   back where it started.
4. Do 1 again with **Copy**: both clusters show it. Its copy's Visibility says Remove from the
   cluster, which deletes only the copy.
5. **Move this card** > **Change group?** > a group > **Next** > **Move**: on the page, in that
   group; the window closes.
6. Desk's lamp (a group card's member) > Add card to dashboard > Add: the dialog, then the window
   closes.
7. A cluster with sensors in its header: Members shows **HEADER** on those rows. Header options and
   Members name each member alike.
8. In Members (all rows on screen), start a swipe on a row and lift off it: nothing opens. A plain
   tap still opens the member.
9. The back arrow inside a grid goes to the view's start, then to Visibility.

## Questions

- **The tag's words.** "HEADER" in the dim tag face, after the name. A small header glyph instead?
- **Page cards.** A card on the page still has "Add to a cluster" (one step already, with Copy or
  Move, and it closed already). It has no confirmation dialog. Should it get one, and the same
  "Move this card" view?
- **Other pages.** Add to cluster? lists the clusters on this page only.

## Later, from the same notes

- The 4880 in portrait: font sizes and fit, far down the list.
- A rotation switcher: rotate-and-restart from UI Tweaks is small (the board already works out its
  frame path, layout and touch mapping from the rotation at boot); switching live, without a
  restart, is a larger job. With #71.
