# TEST 2.10b - the light controls (#65)

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
