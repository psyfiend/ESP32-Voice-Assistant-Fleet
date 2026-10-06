# TEST 2.10b - the light controls (#65)

**DONE: signed off 2026-10-06 after round 7 (and one last speck), merged and tagged `v0.2.9`.**
What was built is `docs/design/card-sheet.md` section 15.

Branch `feat/65-light-controls`. Board: **WS_P4_5** (flashed 2026-10-05 with `-D DEBUG_POPUP -D
DEBUG_FRAMES`, local only). Design: `docs/design/card-sheet.md` sections 11.1 and 15, the artifact
"Card Popup Mock" (v3).

Mark each line PASS or FAIL, with a note for anything that looks wrong even if it passes.

**The Fleet page's lamps are four different kinds of light now**, so a window can be seen hiding
what a light cannot do:

| Card | What it can do | Its selector |
|---|---|---|
| Lamp 1 (Kitchen) | dim | Power \| Brightness |
| Lamp 2 (Kitchen) | dim, white range 2200-6500 K | Power \| Brightness, Temperature |
| Lamp 3 (Lounge) | dim, white range, colour | Power \| Brightness, Temperature, Colour |
| Lamp 4 (no card of its own) | on/off only | - |
| **All Lamps** | a group of all four | everything any member can do |

Each lamp answers 0.8 s after the LAST command it was sent, as a slow bulb would, and remembers its
brightness and colour while off.

## Round 1

### Size (Z)

| # | Do this | PASS if |
|---|---|---|
| Z1 | Open the Switch's window | Same size as in v0.2.8; the toggle is still ~30 mm tall |
| Z2 | Hide the system header, then open a window | The window is taller, NOT wider; the toggle the same size as in Z1 |

The 7" boards and the 4B are not flashed with this build - see "Not tested" below.

### A light's window (L)

| # | Do this | PASS if |
|---|---|---|
| L1 | Open **Lamp 3** (off) | A tall slider on the left; beside it "Brightness", "Off", "Changed ...", and the selector: Power \| Brightness, Temperature, Colour, with Brightness highlighted |
| L2 | Open **Lamp 1**, then **Lamp 2** | Lamp 1: Power \| Brightness only. Lamp 2: Power \| Brightness, Temperature |
| L3 | Lamp 3: tap the slider halfway up | The lamp turns on at about 50%: the fill jumps there, the value says ~50%, the card under or beside the window fills to the same level |
| L4 | Drag slowly from the bottom to the top over about two seconds | The fill and the percentage follow the finger the whole way. The card follows in steps (about three a second). After you let go, nothing jumps back |
| L5 | Drag all the way down | 1%, not Off; the fill is a small rounded end, not a line |
| L6 | Tap Power; then tap it again | Off: value "Off", fill empty, power icon white. On again at the brightness it had |
| L7 | Tap the Temperature button (sun with thermometer) | The slider becomes a strip, warm orange at the bottom to white at the top, with a white ring; the value is in K |
| L8 | Tap near the top of the strip | The ring jumps there, the value reads near 6500 K, the card's disc goes white |
| L9 | Tap the Colour button (palette) | A rainbow strip and eight swatches; no value line |
| L10 | Tap the blue swatch | A ring round the blue swatch; the strip's ring on blue; the card's disc blue |
| L11 | Drag along the colour strip | The ring follows; the card's colour follows in steps |
| L12 | Start a drag on the slider and carry the finger off it, past the window's edge | The slider keeps following (stopping at its ends). The window does NOT close, the page does NOT change, no drawer |
| L13 | Flick quickly up and down on the slider | No close, no drawer, no deck. A drag down on the title row still closes |
| L14 | The **Switch** and **Stuck** windows | The toggle as before: tap, drag, Stuck slides back |

### All Lamps - the group (G)

Open **All Lamps**, then its members view (the bulbs icon, top right) to check each step.

| # | Do this | PASS if |
|---|---|---|
| G1 | All four off, open All Lamps | Power \| Brightness, Temperature, Colour - everything any member can do |
| G2 | Drag brightness to the bottom (1%) | Members: Lamps 1-3 "On, 1%"; **Lamp 4 "On"** (it can only switch, so it turns on) |
| G3 | Temperature, tap near the bottom (~3000 K or less) | Lamps 2 and 3 show the K; Lamp 1 only its % |
| G4 | Colour, tap a swatch | Only Lamp 3 changes to a colour; Lamp 2 keeps its white |
| G5 | Power off, then Power on | Every lamp comes back at its own brightness and colour (what you saw HA do) |
| G6 | Turn Lamp 1 off from its own card. Open All Lamps' SETTINGS deck | A row "On when: Any is on / All are on", "Any is on" chosen; All Lamps reads on |
| G7 | Choose "All are on" | The All Lamps card goes to off (3/4 shown on it); the window says Off |
| G8 | Close, tap the All Lamps card | All four turn ON (a group that is off turns everything on) |
| G9 | Back to "Any is on"; with some lamps on, tap the card | All four turn OFF |

G6-G9's choice is kept in RAM only: a reboot or a page rebuild puts it back to "Any" (saving is 2.10d).

### The deck (D)

| # | Do this | PASS if |
|---|---|---|
| D1 | Open a lamp's deck, close it, open it again | The rows are there both times. Say whether the FIRST opening hesitates - its rows are built then (to save memory) |
| D2 | All Lamps' deck | Five rows (the extra "On when") and the note fit without scrolling |

### Looks (K)

| # | Check | PASS if |
|---|---|---|
| K1 | Linen: a lamp's window in each control | The slider's track, the fill, the strips' ring and the selector are all clear |
| K2 | Text | Nothing renders as an empty box |

### Round 1 results (owner, 2026-10-05)

Z1-Z2, L1-L6, L8, L10-L14, G1-G9, D1-D2, K1-K2 PASS - "looks amazing". D1: no hesitation on the
first opening of the deck. G2: Lamp 4 has no card of its own but shows under Members (by design).
**L7 FAIL / L9 PARTIAL**: a stray vertical line beside the strip's left edge (the hidden ring's
outline - a 0-wide object still drew it), and sometimes no ring in Temperature (the lamp was showing
a colour, which reports no kelvin). **Slider grip** opened off to the right on All Lamps and Lamp 3
and moved to the middle a second later (placed from a width LVGL had not laid out yet). All three
fixed in `6a629e7`. **Raised**: a paused member makes All Lamps PAUSED, and a paused card's own
window should say so more strongly - discussion in HANDOFF, not built.

## Round 2 - the fixes, and the other sizes

Flashed 2026-10-05: **WS_P4_5** (with the debug flags) and **WS_P4_4B**. **CYD_P4_1060** is built
but was not connected.

| # | Board | Do this | PASS if |
|---|---|---|---|
| R1 | P4_5 | Open All Lamps and Lamp 3 a few times, lamps on | The grip is in the middle from the first moment |
| R2 | P4_5 | Lamp 3: Temperature, then Colour, a few times each | No line beside the strip |
| R3 | P4_5 | Lamp 3 showing a colour (tap a swatch), then Temperature | No ring; the value reads "A colour". Tap the strip: the ring appears, the K shows |
| R4 | 4B | Open the Switch, then a lamp | The toggle and the slider are smaller than before (~29 mm, about the P4_5's); the window still screen-wide |
| R5 | 4B | Lamp 3: Colour | The swatches and selector fit beside the strip |
| R6 | 1060 (when connected) | Open the Switch, then a lamp | The window is wider than tall (~108 x 75 mm); the toggle ~48 mm - the same share of the window as on the P4_5 |

**Round 2 results (owner, 2026-10-05):** R1-R5 PASS. R6 (1060) not yet - it would not flash (COM9 busy).

## Round 3 - paused members, group brightness, a member's own controls

Asked for in round 1 (owner, 2026-10-05). Flashed: **WS_P4_5** and **WS_P4_4B** (`229f0bf`).
The 1060 has the build but could not be flashed (COM9 busy).

| # | Do this | PASS if |
|---|---|---|
| P1 | Pause Lamp 3 (its window's SETTINGS) | Lamp 3's own window: a PAUSED pill above "Brightness", the slider and selector greyed |
| P2 | In that window, tap the slider, Power, a swatch | Nothing changes; a toast says "Paused - turn it off in SETTINGS" |
| P3 | The All Lamps card on the page | NOT paused - no PAUSED badge, normal colours |
| P4 | Open All Lamps | No Colour button (only Lamp 3 had colour); the status line ends ", 1 paused" |
| P5 | Turn All Lamps on and off | Lamp 3 does not change; the others do |
| P6 | All Lamps' members view | Lamp 3 listed, reading "Paused" |
| P7 | Pause all four (All Lamps' SETTINGS, Paused On) | The All Lamps card now shows PAUSED; its window greyed with the pill |
| P8 | Resume (Paused Off) while the window is open | The pill goes and the controls come back to colour within a second |
| B1 | Lamp 1 at 100%, Lamp 2 at 50% (their own windows), Lamps 3 and 4 off | The All Lamps CARD fills to 75%; its window says 75% |
| M1 | All Lamps, members view, tap Lamp 2 | "All Lamps > Lamp 2", back arrow, Lamp 2's own controls (Power \| Brightness, Temperature) |
| M2 | Change Lamp 2's brightness there, then the back arrow | Back on Members, Lamp 2's row shows the new level |
| M3 | Tap Lamp 4 | "All Lamps > Lamp 4", the big toggle (on/off only); it works though Lamp 4 has no card |
| M4 | In a member's view, open SETTINGS and set Paused On | Only that member pauses (check Members) |

**Round 3 results (owner, 2026-10-05):** P1-P3, P5-P8, B1, M1-M4 PASS. **P4 FAIL**: with every
member paused, Colour came back (the "all paused shows them frozen" rule counted Lamp 3 again).
**Raised**: the selector moved down when the PAUSED pill appeared, and the whole row slid sideways
between lights with different controls - "all of the text locations should be fixed". Both fixed
(round 4).

## Round 4 - nothing moves

Flashed 2026-10-05: **WS_P4_5** (debug flags), **WS_P4_4B**, **CYD_P4_1060** (R6 from round 2 is
on this build too).

| # | Do this | PASS if |
|---|---|---|
| F1 | Pause Lamp 3, then pause the other three one by one, watching All Lamps' window | Colour never comes back. With all four paused: a greyed slider and a greyed Power, no other buttons |
| F2 | Open Lamp 1, Lamp 2, Lamp 3, All Lamps, the Switch, one after another | The slider (or toggle), "Brightness"/"Power", the value and "Changed..." are in the same place in every window |
| F3 | Lamp 3: Brightness, Temperature, Colour | The selector stays put; in Colour the swatches fit between the label and the selector |
| F4 | Pause and resume Lamp 3 with its window open | PAUSED appears BESIDE "Brightness"; nothing else moves |
| F5 | 4B and 1060: repeat F2 and F3 | The same on each board (the 1060 window is wider than tall) |

**Round 4 results (owner, 2026-10-06):** F1, F3-F5 PASS. **F2 PASS, but** the slider sat hard
against the left edge on lights with few controls, and even four controls were not centred. Asked
for: centre by real width (not the rearranged layout), the window up to 2:1 with notable side gaps,
the chart always in the corner with Members inside it, and Paused as a real switch.

## Round 5 - balance, width, the Paused switch

Flashed 2026-10-06: **WS_P4_5** (debug flags), **WS_P4_4B**, **CYD_P4_1060** (`514b86e`).

| # | Do this | PASS if |
|---|---|---|
| W1 | Open any window on each board | P4_5 ~93 mm wide (was 68), the title no longer ellipsised; 1060 ~129 mm; 4B ~60 mm. A clear gap to the screen's sides on every board |
| W2 | The Switch, then Lamp 1, then Lamp 3, then All Lamps | The toggle and its words centred; Lamp 1's slider well in from the left edge; Lamp 3 and All Lamps centred as a whole |
| W3 | Lamp 3: Brightness, Temperature, Colour; pause and resume it | Nothing moves within that one window |
| W4 | All Lamps' window, top right | The chart in the corner, the members icon beside it towards the middle |
| W5 | Open any SETTINGS deck | Paused is a switch: knob left, grey = live; tap it - knob right, accent = paused, the card pauses. Tap again to resume |
| W6 | Linen, one window | The switch and the layout read clearly |

**Round 5 results (owner, 2026-10-06):** all PASS except: with All Lamps paused, the whole group
shifted (its hidden buttons narrowed the column); and the 7" window at ~129 mm made every reach
longer - the X especially - and left less room to tap outside.

## Round 6 - the last before the merge gate

Flashed 2026-10-06: **WS_P4_5** (debug flags), **WS_P4_4B**, **CYD_P4_1060**.

| # | Do this | PASS if |
|---|---|---|
| Y1 | All Lamps: pause it, resume it, pause Lamp 3 alone | The slider, the words and Power never move (the column is sized for everything the members can do) |
| Y2 | 1060: open any window | ~100 mm wide (round 5: ~129); the X within an easy reach |
| Y3 | P4_5 and 4B: open any window | Unchanged from round 5 (93 mm and ~60 mm) |

**Round 6 results (owner, 2026-10-06):** passed; asked for two more before the merge: a small grey
crescent at the foot of the SETTINGS tab's inner curve (seen on every board, in screenshots too),
and the system drawer's contents standing still while its edge moves (#81).

## Round 7 - before the merge gate

Flashed 2026-10-06: **WS_P4_5** (debug flags), **WS_P4_4B**, **CYD_P4_1060**. **New in this build:
LVGL's pool is in PSRAM at 512 KB on every P4** (#88) - the internal pool was 128 KB.

| # | Do this | PASS if |
|---|---|---|
| X1 | Linen: open any window's SETTINGS deck, look at the foot of the inner curve | No grey crescent below the pane's edge |
| X2 | Midnight: the same | Nothing there either (Midnight casts no shadow; say if you saw it there before) |
| X3 | Open and close the system drawer (swipe down, right half) | The contents slide down WITH the bottom edge, like the deck's panels, and slide back up with it |
| X4 | Use the board normally for a few minutes: swipe pages, open windows, scheme changes | Feels as before. Page swipes and scheme changes may be a little slower (full-screen frames ~10%); say if you notice |
| X5 | HA free-heap sensor over the next hours | Internal free heap ~128 KB higher than before |

Measured on WS_P4_5 (Fleet page): internal heap free at boot 361 KB (was 233). LVGL's pool 500 KB
usable; with All Lamps' window and deck open 99 KB used, biggest free block 399 KB (was 13 KB).

## Not tested (by Claude)

Everything below was driven over `/popup` with no finger on the glass, so **nothing about touch has
been tried**: dragging smoothness, flicks, a finger leaving the slider, the gestures in L12-L13.
Also not seen: Linen (K1); the House page's HA lights, which open on the big toggle now (until 2.10c
tells us what they can do) - their toggle switches the real light; and the size change on any board
but the P4_5. The 7B, 1060 and 4B windows were worked out on paper (below), not seen.

## Measured (Claude, WS_P4_5, 2026-10-05)

**Window sizes, worked out from the code** (P4_5 measured on the device):

| Board | Window | Hero (toggle or slider) | Was |
|---|---|---|---|
| WS_P4_5 | 787 x 545 px, 68 x 47 mm | 30 mm tall (349 px) | unchanged |
| CYD_P4_1060, WS_P4_7B | ~108 x 75 mm | ~48 mm | 68 mm wide, 58 mm toggle |
| WS_P4_4B | ~66 x 57 mm (screen-limited) | ~29 mm | ~40 mm toggle |

**LVGL's pool** (`/popup`, Fleet page, internal pool of 116 KB usable):

| State | Used | Biggest free block |
|---|---|---|
| Idle, fresh boot | 83 KB | 31 KB |
| A switch window, v0.2.8 (deck rows built with it) | 99 KB | 15 KB |
| A lamp window, Brightness (deck rows not built) | 91 KB | 24 KB |
| ... Colour (strip of 6 pieces, 8 swatches) | 93-94 KB | 21-22 KB |
| ... with the deck opened | 100 KB | 15 KB |
| After closing, once several windows have come and gone | 83-85 KB | 15-21 KB (fragmented) |

Building the deck's rows on first open saves ~7 KB while folded and ~10 ms of build time (26-27 ms
to 16-17 ms). The biggest free block never returns to 31 KB after the first window: closing leaves
the pool fragmented (26-54%). It settles; it does not keep falling.

**The pool in PSRAM, tried and reverted** (`-D FLEET_LV_MEM_PSRAM`, same 128 KB, same build):

| | Internal pool | PSRAM pool |
|---|---|---|
| Internal heap free at boot | 233 KB | 361 KB |
| Full-screen frame (`/bench`) | 91-97 ms | 100-108 ms (+10%) |
| One card frame | 6.8-7.7 ms | 7.3-8.1 ms (~same) |
| Window's opening frame | 44-45 ms | 46-47 ms |
| Window's closing frame | 51-56 ms | 56-63 ms |

**Frames** (`DEBUG_FRAMES`): a lamp window's opening frame 49 ms, long press to glass 69 ms; each
slider update 5-23 ms to draw (the slider, the value and the card under the window).
