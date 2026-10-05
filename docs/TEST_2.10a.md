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
| O1 | Long press any card (hold a quarter second) | The page dims at once, an empty frame grows out of that card into a centred window, then the contents appear |
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

## Round 1 results (owner, 2026-10-03) and what changed

O1, O3-O5, H2, H5, C2, C4-C6, B2-B4, V1, V2, L1, L3 PASS. Fixed since: **C3** (drag down never
arrived - LVGL hands a gesture to the first object WITHOUT GESTURE_BUBBLE), **V3** (title clipped /
wrapped), **B1** (group shifted on the first tap), **H1/H3/H4** (bigger icons in discs, nearer the
corner), **L2** (Linen's shadow on the window). **O2/C1** (choppy grow): now a rounded outline that
grows from the card, the dim only while the window is open - smoother, but the owner wants a filled
window (see HANDOFF). **B5/B8** are #84, not 2.10a: the board had no network, so nothing ever had a
reading. Long press is 300 ms.

## Round 3 (owner, 2026-10-04)

| # | Do this | Result |
|---|---|---|
| R1 | Open and close popups from cards on the far left, far right, top and bottom rows | **PASS** - no lines left behind |
| R2 | The page-picture backdrop (filled window over a snapshot) | **Rejected**: slower (4 frames) and the background froze. Reverted |
| R3 | The ring-painted grow | **"Looks pretty good"**, no gaps seen. Wanted: a border and rounded corners while it moves |

## Round 4 (owner, 2026-10-04 evening, home)

The grow itself "isn't that bad", but opening as a whole is not smooth: a delay between the long
press registering and the motion starting, then "an untenable delay" between full size and a usable
window (the dim and the contents arrive together, late). Closing is as bad in reverse: contents
cleared, then the dim lifts, then the empty frame shrinks. **Owner's verdict: unless performance
improves drastically, this way of adding flair is not viable; snappy first.** Long press 300 -> **250
ms**. Border and rounded corners on the grow are on hold until the direction is chosen (HANDOFF).

## Round 5 - usable first (2026-10-04 evening, WS_P4_5)

No grow: the window appears complete and closes in one step. Backdrop NONE to start. A long press on
the window's title switches NONE / DIM for the next window (debug build only).

| # | Do this | PASS if |
|---|---|---|
| U1 | Long press a card (a quarter second) | The window appears complete - frame, title, X, contents - in one step |
| U2 | Tap the X | The window is gone in one step, page back as it was |
| U3 | Tap a card outside the window | Closes; that card does NOT toggle |
| U4 | Drag down on the title row; then open again and drag down in the body | The first closes; the second does not |
| U5 | With the window open: swipe left/right, down from the top edge, up from the bottom | No page change, no drawer, no deck (the tap may close the window - fine) |
| U6 | Open a card beside other live cards (sensor, Uptime) | Cards outside the window keep updating |
| U7 | Long press the window's title | Toast "Backdrop: DIM - from the next window" |
| U8 | Close, open another card (DIM) | The window first, then the page around it darkens a moment later. Say how the gap feels |
| U9 | DIM: look at the window's four rounded corners | Dim right into the corners - no bright specks |
| U10 | DIM: close | Window and dim go together |
| U11 | Linen: repeat U1 and U8 | Note how much slower, if at all |
| U12 | After a dozen opens and closes | No lines or leftovers on screen |

**Round 5 results (owner):** the instant window is "a much better experience" than any grow - "the
speed alone makes the entire experience vastly preferable". DIM: "the delay seems like an eternity
and adds very little" - **decided: no dim.** Measured from the owner's DIM trial (scheme not
recorded): the window's frame 136-143 ms, the dim's frame 98-102 ms, closing 147-149 ms. NONE:
building the window 5 ms, tearing it down 0.5 ms, one close frame seen at 46 ms.

## Round 6 - the hold, the leap, the knob (2026-10-04 night, WS_P4_5)

| # | Do this | PASS if |
|---|---|---|
| P1 | Tap a card quickly | Its border starts toward the accent colour and fades back; the card does what a tap does |
| P2 | Press and hold a card | The border fades to the accent over the quarter second and the card sinks slightly; then it jumps a little larger for a moment, then the window appears |
| P3 | With the window open | The card you held (if visible beside the window) keeps the accent border |
| P4 | Close the window | The card's border returns to normal with it |
| P5 | Start a swipe on a card | The border lets go as soon as the finger moves; the page changes as before |
| P6 | Long press the window's title | Toast "Leap: 1 frame"; again "Leap: none"; again "Leap: 2 frames". Compare how each feels |
| P7 | Switch's window: tap the toggle | The knob SLIDES to the other end |
| P8 | Stuck's window: tap the toggle | The knob slides, then a few seconds later slides back |
| P9 | Repeat P2 in Midnight and in Linen | Note any difference in how fast the window appears |

**Round 6 results (owner):** P1, P3, P4, P5, P8 PASS. **P2 partial**: the leap was clipped by the
card's wrapper (it only lets a shadow's width out). **P6**: the leap made the popup FEEL slower - the
jump, then a visible wait for the window; with no leap it felt faster. **Decided: no leap**; the
press-in alone "is by itself a really catchy visual". **P7**: passes, but a knob that slides when it
cannot be dragged is "jarring" - either make it draggable or drop the slide (open). **P9**: no
difference seen between Midnight and Linen (measured: the window's frame ~77 ms Midnight, ~140 ms
Linen; closing ~48 / ~92). **Bug: a teal screen and a reboot, 2-3 times** - found in the log and
fixed: a swipe started on a card rebuilt the page and the hold restyled the deleted card (LESSONS).
The owner also asked that the press move a card's edge-attached parts with it (band, tag pills,
badge, corner icon - not the floating tag).

## Round 7 - press-in only, interference (2026-10-04 night, WS_P4_5)

| # | Do this | PASS if |
|---|---|---|
| Q1 | Hold a card | It presses in and its border goes to the accent; the window appears; the card STAYS pressed in with the accent while the window is open, and comes back when it closes |
| Q2 | Repeat Q1 in each label type (cycle the header knob: Float, Tag, Band, None) | The band, the tag pills, the status badge and the corner icon move in with the card; the floating tag stays where it is |
| Q3 | Start swipes on cards, a dozen times; change scheme a few times | No teal screen, no reboot |
| Q4 | Open a window | For about a third of a second its edge crackles - sparks along the border, flecks jumping off it, short stretches of border dropping out - then it settles. Different each time |
| Q5 | Leave a window open without touching it | Now and then (every 6-15 s) a shorter, weaker flicker; never just after you touched it |
| Q6 | Long press the window's title | Toast cycles "Interference: on open only", "off", "open + now and then" |
| Q7 | Linen | The crackle reads on the light window too (it uses the accent toward the text colour there) |

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
