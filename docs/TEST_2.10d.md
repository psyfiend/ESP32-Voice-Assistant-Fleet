# TEST 2.10d - stable card ids and saved settings (#65)

Branch `feat/65-saving`. Boards: **WS_P4_5** (COM15), **WS_P4_4B** (COM7) and **CYD_P4_1060** (COM9),
all flashed 2026-10-09 with `-D DEBUG_POPUP` (local only) and the **new cache-safe P4 libraries**.
Design and what each step measured: `docs/design/card-sheet.md` section 17; decisions:
`docs/DECISIONS.md` K33-K46, A14.

Mark each line PASS or FAIL, with a note for anything that looks wrong even if it passes. Rounds 1
and 2 matter most; the rest can be done in any order. About an hour for all of it.
**Which lights are safe**: anything in the Office. Nothing here needs the Kitchen.

**How it works, in three lines.** Each board keeps your choices in its own settings file - what you
choose on the P4_5 does not appear on the 4B. A choice made in a card's window is written when the
window closes. A choice made from a browser (the addresses below) shows on the card the next time its
page is built: swipe to the other page and back.

**Useful addresses** (P4_5 shown; the 4B is `fleet-ws-p4-4b`, the 1060 `fleet-cyd-p4-1060`):
- `http://fleet-ws-p4-5/settings` - the board's settings file, as it is
- `http://fleet-ws-p4-5/settings?stats=1` - how many saves, the files, anything unclaimed, and the
  **last ten changes**, each with its time and where it came from (`loopTask` = a card window,
  `httpd` = a browser)
- `http://fleet-ws-p4-5/panel` - any panel underrun the board has seen, with its time
- `http://fleet-ws-p4-5/screenshot` - what the screen shows

**Watch for, all through:**
- **Any flash** - light blue, white, or a blink - especially as a window closes (that is when it
  saves). Note the time and open `/panel`.
- **A change you did not make** in `/settings?stats=1`'s last changes. Desk's Tap action on the P4_5
  (Load scene, Bright) was set on 2026-10-08 with nobody at the panel, and it is still unexplained;
  leave it until T4 changes it, and tell me if anything like it appears.
- **A card label changing colour** when you did not ask for it.
- **A restart you did not cause** (the screen goes dark and comes back).
- **Anything slow**: a window that opens or closes late, a stutter as it saves.

**If something fails**, send me the line number, what you saw, and - if the board is still up - the
`/settings?stats=1` page and a `/screenshot`.

## Round 1 - the flash fix (A14)

Before the new libraries, every flash write made the P4 screens flash light blue (your test on
2026-10-08). Open each link while watching that board's screen.

| # | Do this | PASS if |
|---|---|---|
| F1 | P4_5: `http://fleet-ws-p4-5/panel?flash=20&nvs=1` | No flash. (20 writes like a Pause) |
| F2 | P4_5: `http://fleet-ws-p4-5/panel?flash=20` | No flash |
| F3 | P4_5: `http://fleet-ws-p4-5/panel?flash=8&kb=256` | No flash. (The worst case: up to a third of a second each) |
| F4 | The same three on the 4B (`fleet-ws-p4-4b`) and the 1060 (`fleet-cyd-p4-1060`) | No flash on either |
| F5 | Through the whole session: the P4_5's **random white flash** | Say if you see one, and open `http://fleet-ws-p4-5/panel` right after: does it list an underrun near that time? |

## Round 2 - things are kept (P4_5)

| # | Do this | PASS if |
|---|---|---|
| S1 | Pause **Garage North** (long press, SETTINGS, Paused). Close the window | No flash as it closes. `/settings?stats=1` says one more save |
| S2 | Swipe to Fleet and back; then restart the board (unplug, or the reset button) | North is still PAUSED after both. Resume it afterwards |
| S3 | Fleet page: **All Lamps**, SETTINGS, Active state -> All members are on. Close. Swipe away and back | Still "All". (Before 2.10d a swipe forgot it) |
| S4 | Restart. All Lamps' Active state | Still "All". Put it back to Any |
| S5 | Open a window, change nothing, close it | `/settings?stats=1`: the save count does not move (nothing changed, nothing written) |
| S6 | Open SETTINGS, scroll it if it can scroll, close the window | Nothing of the pane shows at the bottom of the screen afterwards (your bug from 2026-10-08) |
| S7 | System drawer: the new **Sel Own** button, next to the scheme | Each tap: Blk Sq -> Blk Rd -> Slv Sq -> Slv Rd -> Sel Own. Open Desk: the selector has that look |
| S8 | Choose Slv Rd on this page's scheme. Restart | Still Slv Rd. Back to Sel Own after |

## Round 3 - Entity label (K40, K45)

The words under the hero. The row is now called **Entity label**: Inherit / HA name / State / Custom / None.

| # | Do this | PASS if |
|---|---|---|
| L1 | **Overhead (Office)**: Entity label -> HA name | The card reads HA's own name for that light (not "Overhead") |
| L2 | -> State | "On" or "Off" |
| L3 | -> None, then -> Inherit | No words; then "Overhead" again |
| L4 | Restart with Overhead on HA name | Still HA's name |
| L5 | From a browser: `http://fleet-ws-p4-5/settings?card=office_overhead_261008_0310&name=Ceiling+lamp`, set Entity label to Custom, swipe away and back | "Ceiling lamp". Then `...&name=` (empty) and Inherit to put it back |

## Round 4 - Card label and groups (K46)

The coloured label at the card's top shows the card's **group**, and the group owns the colour. Your
dashboard's areas are now HA's areas under your shorter names (Living = "Living Room", Front = "Front
Room", Bedroom = "Eric Bedroom"). The row is **Card label**: every group, then **Own group**, then
**None**. **Groups now get colours of their own** (your call this morning): the House page's seven
are all different. With eight colours, the Fleet page's four test areas share with others.

| # | Do this | PASS if |
|---|---|---|
| G1 | Look at the House page | Every card label as in v0.2.10, except **Kitchen, now orange** (it shared Garage's green). Same words everywhere |
| G2 | **Thermo**: Card label -> **Own group** | Still "Front", in a colour no House group has |
| G3 | **Kitchen temperature**: open its Card label row | Two "Front" lines: one "Front (HA)", one "Front" (Thermo's). Choose the plain "Front" | 
| G4 | Look at the page | Thermo and Kitchen temperature: one label, one colour - a group of two, made without creating anything |
| G5 | From a browser: `http://fleet-ws-p4-5/settings?group=ha_office&name=Work`, swipe away and back | Desk, Overhead, Office occupancy and Office temperature all read "Work" - **still purple** |
| G6 | `...settings?card=office_261008_0310b&label_text=Study`, swipe away and back | Office temperature reads "Study", still purple (its own words, still in the group) |
| G7 | `...settings?group=ha_office&color=2E9E4F`, swipe away and back | All four Office cards turn green, "Study" included |
| G8 | **Garage North**: Card label -> None | No card label; the card keeps its place and size |
| G9 | Restart | G2-G8 all still as you left them |
| G10 | Put Thermo back on "Front (HA)" and Kitchen temperature on "Kitchen" | Thermo's group disappears from the rows (its last card left) |
| G11 | Put the rest back: `...?group=ha_office&name=` and `&color=` (both empty), `...?card=office_261008_0310b&label_text=` (empty), North -> Garage | Back to G1. `/settings?stats=1` shows what is left, and the last ten changes |

G5-G7 rename and recolour Office only to show it works; G11 undoes them. Skip them if you would
rather not touch the names.

## Round 5 - Tap action (K37, K39)

A long press always opens the window. The tap does what Tap action says; the list only offers what
that card can do.

| # | Do this | PASS if |
|---|---|---|
| T1 | **Overhead (Office)**: Tap action -> Details view. Close. Tap the card | Its window opens. The first tap inside it works (not swallowed) |
| T2 | -> History view. Tap | The window opens on History |
| T3 | -> Nothing. Tap | Nothing happens. Long press still opens the window |
| T4 | **Desk**: Tap action -> Cycle scenes. Tap the card three times, a second apart | Bright, Concentrate, Relax - each with a toast "Scene: Desk - ..."; only the scenes the window shows |
| T5 | Restart; tap Desk once | Bright again (the cycle starts from the first after each boot) |
| T6 | Desk -> Load scene. Go to Scenes; open the **SCENES** panel; Tap scene -> Relax. Tap the card | Relax, with its toast |
| T7 | **All Lamps** (Fleet): Tap action -> Members view. Tap | The window opens on Members |
| T8 | Put Desk and Overhead back to Toggle; the Office lights back as you like them | - |

T4 and T6 change the Office lights (scenes); I leave them at 100%, 2710 K after my own tests.

## Round 6 - the panels (K43, K45)

| # | Do this | PASS if |
|---|---|---|
| P1 | Desk's SETTINGS on the P4_5, the 4B and the 1060 | Paused, Entity label, Card label, Visibility (greyed), Tap action - and **no scrolling** on any of them |
| P2 | Desk -> Scenes | A **SCENES** panel peeks up on the left, as CHART does on History |
| P3 | Open it: Show hidden scenes, tick it | The four hidden scenes join the buttons. Untick: they go |
| P4 | Go from Scenes to History | SCENES goes down, CHART comes up |
| P5 | Restart with Show hidden ticked | Still ticked. Untick it after |

## Round 7 - left-over settings (K35)

| # | Do this | PASS if |
|---|---|---|
| U1 | `http://fleet-ws-p4-5/settings?card=old_card_261001_0900&label=ha`, then `/settings?stats=1` | It lists "unclaimed cards: old_card_261001_0900" |
| U2 | System drawer, Log: the System Doctor's **[SETTINGS]** section | The same entry (not checked by me: the PC cannot open the drawer) |
| U3 | `/settings?prune=cards` | "pruned 1"; the entry gone, nothing else touched |

## Your results

(Write them under each round, as in `archive/TEST_2.10c.md`.)
