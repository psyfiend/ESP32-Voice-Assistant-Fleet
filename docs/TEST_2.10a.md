# TEST 2.10a - the card popup window (#65)

Branch `feat/65-popup-frame`. Board: **WS_P4_5**. Design: `docs/design/card-sheet.md` sections 11-13
and the artifact "Card Popup Mock" (v3).

Mark each line PASS or FAIL, with a note for anything that looks wrong even if it passes.

**Step 1 (this build): the window.** The settings deck that peeks up from the bottom, and Pause
inside it, come in step 2. **Until then nothing on the board can pause a card** - long press used
to do that and now opens the window.

The Fleet page has the useful test cards: **Switch** (echoes its command), **Stuck** (ignores its
command, so it shows a refusal), **All Lamps** (four lamps on one card) and the sensor cards.

## Opening

| # | Do this | PASS if |
|---|---|---|
| O1 | Long press any card (hold ~half a second) | The page dims at once, an empty frame grows out of that card into a centred window, then the contents appear |
| O2 | Watch the grow on a card in a corner, then one in the middle | It starts on the card each time and ends in the same centred place. Note any stutter: smooth / slight / bad |
| O3 | Press a card and slowly drag your finger a little, keeping it down past half a second | No window opens |
| O4 | Tap a card (quick) | It toggles or does whatever it did before - no window |
| O5 | Swipe pages left and right after closing a window | Pages still change |

## The header

| # | Look at | PASS if |
|---|---|---|
| H1 | Top left | An X |
| H2 | Middle | "Area > Name", area dimmer than the name, e.g. "Office > Switch" |
| H3 | Top right | A clock icon (stands in for the mock's chart icon - see notes) |
| H4 | All Lamps' window | A second icon top right, the light-bulb group |
| H5 | Any single-entity card's window | No bulb-group icon |

## Closing - the four ways

| # | Do this | PASS if |
|---|---|---|
| C1 | Tap the X | The contents vanish, the empty frame shrinks back into the card, then the dim lifts |
| C2 | Tap anywhere on the dim, outside the window | Closes the same way. The card you tapped on does NOT toggle |
| C3 | Drag down starting on the header row (the title) | Closes |
| C4 | Drag down starting lower, in the body | Does NOT close |
| C5 | Open a window and leave it | A thin bar along the bottom of the window shrinks; at 60 s the window closes by itself |
| C6 | Open, wait ~30 s, touch inside the window | The bar jumps back to full |

## Modal

| # | Do this, with a window open | PASS if |
|---|---|---|
| M1 | Swipe left or right on the dim | Page does not change (the swipe's tap closes the window - that is fine) |
| M2 | Swipe down from the very top edge | No drawer, no header peek, no log page |
| M3 | Swipe up from the bottom edge | No deck, no FPS overlay |

## The body

| # | Open | PASS if |
|---|---|---|
| B1 | **Switch** | A tall toggle on the left with the knob at the TOP when on, BOTTOM when off. Beside it "Power", "On"/"Off" and "Changed N ago" |
| B2 | Tap the toggle | The knob moves and the word changes; the Switch card under the dim follows |
| B3 | **Stuck**, tap its toggle | The knob moves, then a few seconds later jumps back (the command was refused) |
| B4 | A temperature or other sensor card | An icon in a circle, the reading's kind ("Temperature"), the number with its unit, "Changed N ago" |
| B5 | Leave B4 open for a minute | "Changed N ago" counts up |
| B6 | A motion / door card | The state word ("Detected", "Clear", "Open") |
| B7 | A lamp | Read-only for now (its controls are 2.10b): icon, "State", On/Off |
| B8 | An HA card (no HA reachable from here) | "Unavailable" in the warning colour |

## Inner views

| # | Do this | PASS if |
|---|---|---|
| V1 | Tap the clock icon | Title reads "Name > History", a note says history arrives with 2.10e, the X becomes a back arrow, the right icons go away |
| V2 | Tap the back arrow | Back on the main view, X restored |
| V3 | All Lamps: tap the bulb-group icon | Title "All Lamps > Members", a row per lamp with its state |
| V4 | From V3, toggle a lamp's own card later and reopen | The members rows show the new states |

## Looks

| # | Check | PASS if |
|---|---|---|
| L1 | Midnight | Window a step lighter than the cards, thin border, no shadow |
| L2 | Linen (cycle the scheme first, then open a window) | Readable; the dim is dark, not foggy |
| L3 | Text | Nothing renders as an empty box |

## Measured (Claude, from the board) - pending

- `lv_mem` with the window open (interview G1).
- Frame time during the grow (`/bench`) - needs the board on a network the laptop can reach.

## Notes and known gaps

- **History icon:** the mock's chart glyph is not in the board's icon font, and the laptop cannot
  regenerate the font (no Node.js). `clock-outline` stands in until the next font regeneration at
  home.
- **Big words** ("On", "Unavailable") use the toolkit's existing hero face, smaller than the mock's
  6.5 mm; a bigger face costs flash (14-29 KB) and is a choice for later.
- Lights are read-only in this window until 2.10b.
