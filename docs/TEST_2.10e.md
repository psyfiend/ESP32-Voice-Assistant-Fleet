# TEST 2.10e - History (#65)

Branch `feat/65-history` (off `feat/65-saving`; the two merge together - K51). Boards: **WS_P4_5**
(COM15), **WS_P4_4B** (COM7), **CYD_P4_1060** (COM9), all flashed 2026-10-11 overnight with
`DEBUG_POPUP` (and `DEBUG_HANG` on the P4_5), the p4-libs-2026-10-09 libraries. What was built and
why: `docs/design/card-sheet.md` section 18; decisions K52-K55.

Mark each line PASS or FAIL, with a note for anything that looks wrong even if it passes. About 30
minutes. Nothing here commands a light; T-lines from 2.10d are not repeated.

**Useful addresses** (P4_5; the 4B is `fleet-ws-p4-4b`, the 1060 `fleet-cyd-p4-1060`):
- `http://fleet-ws-p4-5/popup?hist=1` - while a History view is open: what the board asked HA,
  how big the answer was and how long it took
- `http://fleet-ws-p4-5/panel` - underruns (for any flash, #91)

## Round 0 - leftovers from 2.10d, round D (P4_5)

The P4_5 adds 4 greyed test rows to SETTINGS until its next restart; to put them back after a
restart: `http://fleet-ws-p4-5/popup?rows=4`.

| # | Do this | PASS if |
|---|---|---|
| D3 | Desk -> SETTINGS, drag up hard and fast, past the end | Stops dead with the last row on screen: never past it, nothing settling back, no gap under the panel |
| D8 | Scenes -> SCENES: drag it down a little and then up, without lifting | It never goes above its resting place; no gap under it |
| D10 | The carets | Rows below the screen: a caret at the bottom pointing **up**. The panel's top off the screen: a caret at the top pointing **down** |
| D9 | `http://fleet-ws-p4-5/popup?rows=0`, reopen Desk's SETTINGS | Normal, no carets |

## Round 1 - a number's chart

| # | Do this | PASS if |
|---|---|---|
| H1 | **Thermo** (House page): long press, then the chart icon | "Thermo > History": span chips 24 h (lit) / 12 h / 6 h / 1 h, "Now 71.0°F  Min ..  Max ..", a line over the last day, times along the bottom ending "Now" |
| H2 | Tap 12 h, 6 h, 1 h | Each redraws in about a second ("Loading..." between); min and max follow the span |
| H3 | Open the **CHART** panel | Span shows the chip you chose; change it there - the chips follow |
| H4 | Untick Show min and max | The readout says only "Now ..". Tick it again |
| H5 | **Kitchen temperature** and an Outside temperature | Each its own line. Does the shape match HA's own graph for that sensor? |
| H6 | Close and reopen Thermo's History | Back on 24 h (choices are not kept yet - K55) |

## Round 2 - activity

| # | Do this | PASS if |
|---|---|---|
| A1 | **Desk** -> History | A list, newest first: On / Off, the time, and under it what did it ("By psyki", "Automation: ..."); an **ACTIVITY** panel |
| A2 | Compare with HA's logbook for Office (light.office) | Same changes, same times, same causes |
| A3 | Tap a row | Its details: the time to the second, "lasted .." (or "so far" on the newest), HA's own words. Tap again: folds |
| A4 | ACTIVITY -> Period: Last 7 days | Many more rows: 10, then "Show more (N)". Tap it: up to 60, then "N older not shown" if there are more |
| A5 | Untick Show causes | The "By .." lines go; a row's details still say it |
| A6 | **Office occupancy** (or Kitchen occupancy) -> History | Detected / Clear, with causes where HA has them |
| A7 | **Garage North** -> History | "No changes in the last 24 hours" (unless it moved) |
| A8 | Desk -> Members -> a lamp -> its chart icon | That lamp's own activity ("Office lamp > History") |
| A9 | **All Lamps** (Fleet page) -> History | Says a group made here has no history of its own, and to open Members |
| A10 | A Fleet-page card that is not from HA (a virtual lamp, Uptime) -> History | Says only Home Assistant's history is shown so far |

## Round 3 - the other boards, and things to watch

| # | Do this | PASS if |
|---|---|---|
| B1 | H1 and A1 on the **4B** and the **1060** | Fits their windows: chips, readout and times readable, nothing cut off |
| B2 | Drag a tall panel on the 4B and the 1060 (`http://fleet-ws-p4-4b/popup?rows=4` first, the same for the 1060) | Same as D3/D8/D10. **And: any flashes?** (#91 - so far only the P4_5 flashes) |
| B3 | Through the whole session: the **card labels' colours** | Every card label keeps its group's colour, right after a restart too (a 2.10d bug fixed tonight: the P4_5's Kitchen cards came up in Garage's green once) |
| B4 | Any freeze | `/hang` on the P4_5 before a reset |

## Your results

### Run 1 - 2026-10-11 morning, all three boards

| Round | Result |
|---|---|
| 0 | D3 D8 D10 D9 PASS - 2.10d round D is done |
| 1 | H1 H2 H4 H5 H6 PASS. **H3 PARTIAL**: Span and the chips follow each other, but a reopened window was back on 24 h - the owner expected it kept (K55 had it unsaved). **Now kept per card** (K56) |
| 2 | A1-A10 PASS |
| 3 | **B1**: on the 4B the span chips ran over "Now .. Min .. Max .." - min and max are now in the chart (below). B2: the P4_5 flashed once or twice while dragging; **the 4B and 1060 never did** (#91). B3 PASS so far. B4: watching |

**Owner's additions, built the same morning** (round 2 below): SETTINGS gets **Units** (°C / °F,
where the card shows a temperature) and an **Entity properties** button (a view of its own, no
corner icon - no room on the 4B); a card that toggles nothing taps to **History** by default;
CHART gets **Legend** (X axis / Y axis / Both) and **Show span buttons**; ACTIVITY's Period is
renamed **Span**, gets **24 h / 7 d** chips and its own **Show span buttons**; activity rows sit
in from the window's sides; **a finger on the chart** puts a dotted line at that time, its time and
value at the top (slide to move it); **min and max** are faint dotted lines where they happened,
their values at the top of the chart, and "Now" stays beside the chips. Every History choice is
kept per card.

## Round 2 - the additions (all three boards, flashed 2026-10-11 ~12:40)

| # | Do this | PASS if |
|---|---|---|
| R1 | Thermo -> History: 6 h; CHART: Legend -> X axis, untick Show span buttons. Close the window, reopen History (and once after a restart) | Still 6 h, X axis, no buttons. Put them back after |
| R2 | Thermo's chart: put a finger on it and slide | A dotted line follows, "time  value" at the top, never under the finger; it stays when you lift |
| R3 | Min and max | Faint dotted lines where each happened, "Min .." / "Max .." above them; untick Show min and max: gone |
| R4 | Legend: X axis / Y axis / Both | Only the times / only the top and bottom values / both |
| R5 | Show span buttons off (CHART), and on ACTIVITY too | The chips go, the chart (or list) gets the room. Back on after |
| R6 | Desk -> History | 24 h / 7 d chips over the list; the rows in from the sides; ACTIVITY's first row says **Span** |
| R7 | Thermo -> SETTINGS -> Units -> °C, close | The card and the window read °C, and the chart too. Restart: still °C. Back to °F |
| R8 | Desk -> SETTINGS -> Entity properties -> View | "Desk > Properties": HA's name, light.office, HA's area, Home Assistant, 3 members, the card's group and id. Back arrow: the controls |
| R9 | A sensor card you never set a Tap action on (Thermo, an occupancy): tap it | Its window opens on History. A light still toggles |
| R10 | The 4B: a chart | The chips and "Now" side by side, nothing running under them |

**Round 2 results (owner, 2026-10-11 afternoon): R1-R10 all PASS** (R8: "Love this!" - fleet and
local entities too). R4 note: Legend was meant as **regular intervals** on the axes - hours along
the bottom, values up the side - not two or three labels. Also asked: the span in words where the
buttons were when they are hidden, Now staying right; and a **fixed range** option (open, below).

## Round 3 - axes in round steps (all three boards, flashed 2026-10-11 ~15:30)

| # | Do this | PASS if |
|---|---|---|
| X1 | Thermo, 24 h | Values up the left in round steps (here every 2°F), a faint line at each; hours along the bottom on the clock (every 3 h on the P4_5), a faint line at each; the unit on the top value only |
| X2 | 12 h, 6 h, 1 h | The time steps shorten (hours, then 10-15 minutes), the values re-step to the range |
| X3 | The same chart on the 4B and the 1060 | Fewer ticks on the 4B, more on the 1060; labels never on top of each other |
| X4 | CHART: untick Show span buttons | "24 hours" (or the span) at the left where the buttons were, Now still at the right; the chart taller, maybe a line more |
| X5 | ACTIVITY: untick Show span buttons | "Last 24 hours" / "Last 7 days" where the buttons were |
| X6 | Legend X axis / Y axis / Both | The grid stays; only the time labels / only the value labels / both |

**Round 3 results (owner, 2026-10-11 evening):** "it looks so good" - asked for the round 4 changes
below before the last round of testing.

## Round 4 - the last round (all three boards, flashed 2026-10-11 ~21:45)

| # | Do this | PASS if |
|---|---|---|
| Y1 | CHART: the rows | Span, Legend (None / X axis / Y axis / Both), Show min/max, Span buttons - in that order |
| Y2 | Legend: None | No labels on either axis; the grid stays |
| Y3 | Span buttons off | "Showing: last 24 hours" (or the span) in the middle, larger; Now still at the right. The same on ACTIVITY ("Showing: last 7 days") |
| Y4 | Min and max | Their values inside the chart, beside their dotted lines - the max above the line, the min below, moved to the other side where there is no room; never on the line |
| Y5 | The chart | A soft fill in the accent under the line, fading toward the bottom |
| Y6 | ACTIVITY: the rows | Span, Details (Basic / Extra), Show triggers, Span buttons |
| Y7 | Details: Basic | The state on the left; on the right "14 minutes ago" over "9:16 PM" ("Yesterday" over the time; "3 days ago" over "Oct 8, 5:27 PM"). Tap a row: it reads "Off for 10 min" |
| Y8 | Details: Extra | Days as headings ("Today, October 11, 2026", "Yesterday, October 10, 2026"); each row "Closed for 2 hrs 38 min" (the newest "... so far"), the time on the right. Tap a row: "At 9:05:34 PM" and HA's own words for the trigger |
| Y9 | Close and reopen | Details and the rest kept |
| Y10 | Entity properties on Thermo, Desk, a lamp | A **Device** line: "THERMOSTAT", "Office", "Office lamp"; "This board" on the board's own entities |
