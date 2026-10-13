# TEST K17 - a member as a card of its own

Branch `feat/17-member-card` (off `main` at `v0.2.11`). Decisions K65-K67; the model:
`docs/design/group-cards.md` §5. Flashed to the P4_5 2026-10-12 (with the local debug lines).

Checked from the PC before you (`/popup?addcard=1`, `?removecard=1`): Desk > Members > Office Left
made a card - right after Desk, in Office's colour, named "Office Left" - kept over a reboot, then
removed and kept in the file as the page's pool. Nothing tested by touch.

| # | Do this | PASS if |
|---|---|---|
| A1 | Desk > Members > a lamp; open SETTINGS and drag it up | Last row: **Add as a card**, a button "Add" |
| A2 | Tap Add | The button reads "Added", greyed; nothing else changes while the window is open |
| A3 | Close the window | The lamp's card is right after Desk: HA's name ("Office Left"), Office's card label colour, lit like the lamp |
| A4 | Tap it; long press it | Tap toggles that lamp alone (Desk follows HA); its window is the lamp's own: controls, History, SETTINGS |
| A5 | Reboot (unplug) | The card comes back, after Desk - a moment after the page, once HA names Desk's members |
| A6 | Desk > Members > the same lamp > SETTINGS | "Added", greyed - one card per member from here |
| A7 | Add the other two lamps | All three after Desk, in the order you added them (Q1); on a full page the lowest priority drops off the end |
| A8 | On an added card: SETTINGS, **Remove card** | First tap: "Sure?"; second: the window closes and the card is gone; the others close up |
| A9 | Change a setting on an added card (Tap action, Entity label), close, reboot | Kept, like any card's |
| A10 | Pause an added lamp | Paused in Desk's Members too (Paused is the entity's, #60) |

**Q1 (Claude's call, say if you want it otherwise):** several cards added from one group card
stand after it in the order they were added - the first one added is next to Desk.

## Your results

**Run 1 (owner, 2026-10-12):** "Add as a card" never showed for Desk's members and came and went
for All Lamps' Lamp 4 (added, removed, then not offered, then offered again). **Cause:** SETTINGS
was sized for its rows when the window opened and filled the first time it opened - so it held
the rows of whichever view it first opened in; a member's own view has one row more. **Fixed:**
SETTINGS is made again, in place, whenever its rows would change (`deckSync()`: another view,
"Added", "Sure?"). In a member's own view on the P4_5 the new row is under the caret - drag up.
Lamps 1-3 have compiled cards of their own on the Fleet page (Lamp 4's was removed 2026-09-19):
opened from those cards, they are not in a member's own view, so no "Add as a card"; and a
compiled card cannot be removed until page editing (2.11, K66). From All Lamps > Members > Lamp 1
it is offered - a second card for Lamp 1 (allowed, K33). Retest A1-A10.
