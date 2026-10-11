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
