# TEST 2.10c - Home Assistant lights get the controls (#65)

Branch `feat/65-ha-lights`. Board: **WS_P4_5** (COM15), flashed with `-D DEBUG_POPUP -D DEBUG_FRAMES`
(local only). Design: `docs/design/card-sheet.md` sections 15-16; the measurements behind it:
`docs/design/ha-websocket.md` section 9; decisions: `docs/DECISIONS.md` C5, K12, K15, K17, W9.

Mark each line PASS or FAIL, with a note for anything that looks wrong even if it passes.

**Which lights are safe**: anything in the Office. The Kitchen (Sink, Table) only when nobody is in
there - those lines are marked **(Kitchen)**.

## Round 1 - HA light levels (step 1)

Every light on the House page now reads what HA says it can do, and the window offers exactly that:

| Card | HA entity | What HA says it can do | Its selector |
|---|---|---|---|
| Desk (Office) | `light.office`, a Hue room of three bulbs | dim, white 2000-6535 K, colour | Power \| Brightness, Temperature, Colour |
| Table (Kitchen) | `light.dining_room_light` | dim | Power \| Brightness |
| Overhead (Office), Sink (Kitchen), Porch | `light.office_overhead`, ... | on/off | the big toggle |
| Overhead (Living) | `switch.tv_room_switch_1` | nothing (a switch) | the big toggle |

| # | Do this | PASS if |
|---|---|---|
| H1 | Open **Desk** | The slider, "Brightness", the room's level, and Power \| Brightness, Temperature, Colour |
| H2 | Drag Desk's slider slowly from the bottom to the top | The fill follows the finger. The bulbs follow about once a second (the Hue bridge's pace). After you let go nothing jumps back, and the card never says FAILED |
| H3 | Temperature, tap near the bottom, then near the top | The bulbs go warm, then cool white; the value reads in K (the top is 6535 K) |
| H4 | Colour, tap the **blue** swatch | The bulbs go blue. Within about 1.5 s the ring may slide a little towards violet: that is the blue the bulbs can really make, reported back. No FAILED |
| H5 | Colour, tap the **green** swatch | The same: green, possibly drifting a little towards blue-green |
| H6 | Power, then Power again | Off: the slider empties. On: back at the levels the bridge chooses (it came back at 40% after green once - say what you see) |
| H7 | With Desk's window open, change the room in the Hue app or HA | The window follows within about a second |
| H8 | Close the window; look at the Desk card | It fills to the room's brightness, its disc in the room's colour |
| H9 | Open **Overhead (Office)** | The big toggle, as before; it still switches the light |
| H10 | **(Kitchen)** Open **Table**, drag it | Power \| Brightness only. Say whether the card jumps around while the dimmer ramps - not measured |
| H11 | Fleet page: Lamp 3 and All Lamps | Unchanged from v0.2.9 |

When you are done, put the Office room back the way you like it - I restore it after my own tests,
but not after yours.

## Round 2 - the members of Desk (step 2)

Desk (`light.office`, a Hue room) now has a members icon. Its three bulbs are learnt from HA when
the room first reports, about 15 s after boot, so give the board that long before G1.

| # | Do this | PASS if |
|---|---|---|
| G1 | Open Desk | The members icon (the bulbs) is beside the chart, top right |
| G2 | Tap it | "Desk > Members": Office Right, Office lamp, Office Left (HA's own names), each "On, N%, NNNN K" |
| G3 | Tap Office lamp | "Desk > Office lamp", its own slider and Power \| Brightness, Temperature, Colour |
| G4 | Drag it to about 30% | Only that bulb dims. Back arrow: Members shows it at ~30% |
| G5 | In the lamp's view, SETTINGS, Paused On. Back, back | Members reads "Paused" for the lamp. Desk's main view says ", 1 paused" |
| G6 | Desk: drag to about 60% | Right and Left dim; the lamp stays where it was |
| G7 | Desk: Power, Power | Right and Left go off and on; the lamp stays on throughout |
| G8 | Close; look at the Desk card | Its fill is the mean of Right and Left only |
| G9 | Tap the Desk card | Right and Left switch; the lamp does not |
| G10 | Reboot the board (or wait for one), then open Desk | The lamp is still paused, with its levels showing in Members |
| G11 | Resume the lamp (its own view, SETTINGS). Back to Desk, drag | All three move together again |
| G12 | Desk's SETTINGS: Paused On | The whole window greys with PAUSED; Members lists all three as Paused. Paused Off brings it all back |
| G13 | Desk's SETTINGS | No "On when" row (HA decides that for its own groups) |
| G14 | Fleet page: All Lamps, pause Lamp 3, drag | Unchanged from v0.2.9 (the card code changed underneath) |

## Round 3 - scenes (step 3)

Desk's seven Hue scenes are found in HA when the board connects (about 15 s after boot). **WS_P4_4B
(COM7) is flashed with this build too**, for S8 - the only board where Desk's window stacks.

| # | Do this | PASS if |
|---|---|---|
| S1 | Open Desk | Five controls: Power \| Brightness, Temperature, Colour, and Scenes (a clapperboard) at the end |
| S2 | Tap Scenes | "Scenes" where "Brightness" was; seven buttons, Bright to Relax by name, in two rows; the selector has not moved |
| S3 | Tap Relax | The room goes to Relax; a ring round Relax |
| S4 | Tap Brightness | The slider is back, at the scene's level; nothing else moved |
| S5 | Close, reopen, Scenes | No ring: HA does not say which scene is showing |
| S6 | Pause Office lamp (Members > Office lamp > SETTINGS). Back to Desk | No Scenes button (a scene would reach the paused bulb); the slider and words have not moved. Resume it: Scenes is back |
| S7 | Office lamp's own view | No Scenes (scenes belong to the room) |
| S8 | **4B**: open Desk | The slider and its words centred on top; the five controls centred underneath. Scenes: two per row, the seventh reached by scrolling |
| S9 | Table (Kitchen), Overhead, the Fleet page's lamps | No Scenes button; laid out as before (Desk's own row is wider than in v0.2.9, for its fifth button) |
| S10 | The clapperboard | Say whether it reads as "scenes". Another glyph costs regenerating the icon font; HA's own scene icon (the palette) is Colour's here |

### Rounds 1-3 results (owner, 2026-10-06)

All PASS except: **H10** not run (the Kitchen). **G2** PASS, but the member rows need not be the
window's full width (awkward on the P4_5). **G8 FAIL, once, not reproduced**: one member paused, Desk
raised to 75%, the window closed - the Desk card said FAILED; a later command cleared it. **S5
PARTIAL**: the ring round a loaded scene stayed after the brightness was changed - it should last
only while the light is as the scene left it, and until the window closes. **On the layout**: the
selector off-centre beside the slider looked awkward with five buttons; asked for Scenes as an icon
under the chart and a view of its own, the slider kept, three rows of buttons on the P4_5, buttons
sized to the window (large on the 4B), the corner icons as tabs on every view (yes - History for the
group from its views, a member's from its own).

**G8, investigated (Claude):** not HA refusing calls - 40 calls to two bulbs at 300 ms and 60 at
150 ms, all accepted. But at 150 ms one bulb's last report came 2 s after its last command, close to
the 3 s window. Unproven; round 4 records the reason for every FAILED.

## Round 4 - Scenes as a view, the corner icons as tabs

Flashed: **WS_P4_5** and **WS_P4_4B**.

| # | Do this | PASS if |
|---|---|---|
| T1 | Open Desk | Power \| Brightness, Temperature, Colour (four again), the selector centred as in 2.10b. Corner: members, chart; **the clapperboard under the chart** |
| T2 | Tap the clapperboard | It lights; "Desk > Scenes"; the slider where it was, at the room's brightness; the scene buttons where the words and selector were. P4_5: three rows, half a fourth showing (Relax, scroll to it) |
| T3 | Tap Relax, then drag the slider | Relax rings, then the ring goes when you drag |
| T4 | Tap Relax; change the room in the Hue app after a few seconds | The ring goes within a second or so of the change |
| T5 | Tap the lit clapperboard | Back to the controls, on the control you had before |
| T6 | Tap the members icon, then the chart, then the chart again | Each lights on its own view; the lit chart returns to the controls. The X closes from any of them |
| T7 | Members | The rows are the width of the slider and its words, centred |
| T8 | Members > Office lamp, then the chart | "Office lamp > History"; the arrow returns to Members |
| T9 | Pause Office lamp; back to Desk | No clapperboard (as S6). From Scenes, pausing a member returns to the controls |
| T10 | **4B**: Desk | Side by side again, as in 2.10b. Scenes: smaller buttons than round 3, the slider kept, the grid centred, scrolling for the rest |
| T11 | If anything says FAILED | Note the time; the reason is now kept on the board (`/popup?view=3`, or ask me to read it) |

### Round 4 results (owner, 2026-10-06)

T1-T4, T6, T7, T9-T11 PASS (no FAILED seen). **Spotted at once**: the back arrow was gone from
almost every view - the only way back to the controls was to close and reopen. **T5 / T8**: the lit
icon taking you back works but is not discoverable, and "back" behaving differently per view is
confusing - back should be hard-linked (History, Members -> controls; a member -> Members; a
member's History -> the member), and the title could link up. **Asked for**: Members centred
vertically; and a rethink of the layout - the controls under the slider and words on every board but
the P4_5, Scenes and Colour pushing the slider aside for their buttons, with Scenes tried both ways
(the corner icon and a selector button). On the 4B, Colour's swatches pushed the selector down.

**Fixed straight away, flashed on both boards:** back hard-linked as asked, the title's first part a
link up, Members centred vertically.

| # | Do this | PASS if |
|---|---|---|
| N1 | Desk: History, Members, Scenes in turn | Each has the back arrow, and it returns to Desk's controls |
| N2 | Members > Office lamp; back | Members |
| N3 | Office lamp > its History; back; back | The lamp's controls, then Members |
| N4 | From the lamp's History, tap "Office lamp" in the title; from the lamp's controls, tap "Desk" | The lamp's controls; Desk's controls |
| N5 | Members | The list centred up and down |

**N1-N5 PASS** (owner, 2026-10-07). Then the owner's design pass: names for the window's parts, the
Scenes view without a control deck and with the slider pushed aside, the deck under the hero on tall
windows, a TouchFLO-3D-style deck, scenes hidden in HA hidden here, and a SETTINGS panel rework (next).

## Round 5 - the new layout and the control deck

Flashed: **WS_P4_5**, **WS_P4_4B**, **CYD_P4_1060**.

| # | Do this | PASS if |
|---|---|---|
| L1 | Desk on each board | 4B and 1060: the slider and its words centred, the control deck under them. P4_5: the deck beside the slider, as before |
| L2 | The deck | A ribbon shorter than the switches; the selector taller, darker (Midnight) or silver (Linen), the chosen icon a size up in the accent; Power a plain button |
| L3 | Tap Temperature, then Colour | The selector glides to each and the slider changes after it arrives. The deck itself never moves |
| L4 | Drag the selector slowly from Brightness to Colour | It follows the finger; its icon changes as it passes halfway over each switch; let go and it snaps to the nearest, which is chosen |
| L5 | Colour on the 4B and 1060 | The slider moves to the left; the swatches have the room; the deck stays put |
| L6 | Scenes on each board | The slider at the left, no deck, the buttons centred and clear of the clapperboard. Only Bright, Concentrate and Relax (the others are hidden in HA) |
| L7 | SETTINGS > Scenes: All, then Off, then Visible | All: seven. Off: no clapperboard (from Scenes, back to the controls). Visible: three |
| L8 | SETTINGS > Deck look: try all four | Round / Square, with and without the accent edge - say which you like; the lit corner chips follow |
| L9 | Linen | The ribbon, selector and lit chips read clearly |
| L10 | Members | "Tap a member for more details"; SETTINGS has no note at the bottom |

## Round 6 - after round 5's screenshots, and the SETTINGS panel

Not run as round 5 (the owner looked at screenshots only); this build replaces it. Flashed: **WS_P4_5,
WS_P4_4B, CYD_P4_1060**. Round 5's L1-L4, L7 and L10 still apply; these replace the rest.

| # | Do this | PASS if |
|---|---|---|
| M1 | Colour, then Scenes, on each board | The slider and the swatches (or scene buttons) are one group, centred left to right, the content centred between the chips and the deck |
| M2 | Scenes | One column of equal-width buttons, centred; a second column only if one would be full (SETTINGS > Scenes > Show all scenes: seven) |
| M3 | The control deck | No divider: Power on its own short ribbon, a break, then the modes. Rounded squares |
| M4 | The selector, and a lit corner chip | Metal, not a flat dark block: gunmetal with a lighter top and a sharp step below the middle; the chosen icon in the accent. SETTINGS > Deck look: Square, Round, Square silver, Round silver - which do you like? |
| M5 | Linen | The ribbon darker than the window, the selector silver, still clear |
| S1 | Open SETTINGS on Desk | Only as wide as it needs (here its tab's width on the P4_5), its right edge on the window's; open, it never reaches above the window's top |
| S2 | Paused | A checkbox; ticking it pauses Desk as the switch did |
| S3 | Scenes dropdown | Opens a list; picking "Show all scenes" or "Disabled" works as round 5's L7 |
| S4 | Label, Visibility, Tap action | Greyed and not tappable (they work after saving, 2.10d) |
| S5 | All Lamps (Fleet page) | One more row, Active state (Any / All members are on), and it works as G6-G9 did |
| S6 | A dropdown near the screen's bottom | Its list opens where it can be read |

### Rounds 5-6 results (owner, 2026-10-07)

L1-L4, L6-L10, M2, M3, M5, S1, S2, S4-S6 PASS. **L5 FAIL**: the control deck moves on choosing Color -
slightly left on the 1060, 1-2 px down on the P4_5. **L8 / M4**: all four looks liked; keep all four,
named "Black - Square / Black - Round / Silver - Square / Silver - Round"; default Black - Square on
Midnight and Fleet, Silver on Linen (round 6 said Silver - Square, round 5 Silver - Round). **S2**: the
checkbox could be larger. **S3**: "???" - not answered. **S6**: the list opens upwards.
**M1 and notes**: the slider should keep its own place where it can - in Scenes the buttons move to
it, and only more columns than fit move the slider; on the 4B the swatches could sit lower and to the
right instead of the slider moving; the 4B's second column of all seven scenes ran under the
clapperboard chip; the 1060's slider and deck both nudge left on Color. SETTINGS: a line under its
title on the 1060 and on Linen, and a break in its left border on the P4_5. **Asked for**: no "Colour"
label over the swatches, and US spelling ("Color"); a selector face whose light/dark line curves up
in the middle, for a raised look; inactive chips as soft dents; and a demo second panel that slides up
with the History view.

## Round 7 - faces, the slider's place, the CHART panel, the title

Flashed: **WS_P4_5, WS_P4_4B**. The 1060 is **not** flashed this round: COM9 was held by another
program all session (a serial monitor?), so its lines wait. Round 6's S1, S2, S4-S6 still apply.

| # | Do this | PASS if |
|---|---|---|
| N1 | Desk: the selector | Its light/dark line curves up in the middle; faint brushed streaks; no pink or green banding. Its icon larger, filling more of it |
| N2 | The corner chips | Unlit: soft dents in the window, darker at the top, no border. Lit (e.g. the chart on History): the selector's metal, the icon in the accent and a size larger |
| N3 | Press and hold a chip | It tints while pressed |
| N4 | SETTINGS > Deck look | Black - Square, Black - Round, Silver - Square, Silver - Round; Midnight opens on Black - Square, Linen on Silver - Square |
| N5 | Brightness, Color, Scenes on the P4_5 | The slider never moves; in Color no "Color" word above the swatches; the deck does not move (L5) |
| N6 | Scenes on the 4B | The slider where the controls put it, the three buttons just right of it |
| N7 | Color on the 4B | The slider stays; the swatches sit low, clear of the clapperboard |
| N8 | SETTINGS > Scenes > Show all scenes | P4_5: three columns, the slider moved left only as far as they need. 4B: one column that scrolls, nothing under the clapperboard, the slider unmoved |
| N9 | The chart chip (History) | A CHART tab slides up in the left half, a small gap from SETTINGS. Tap it: it opens with a demo line; tap again or in the window: it folds |
| N10 | From History go back, or to Scenes | CHART slides back down |
| N11 | History with CHART showing (open or not), then close the window | CHART goes at once with the window |
| N12 | Open SETTINGS while CHART is open, and the other way round | The one opened comes to the front; the other folds to its tab |
| N13 | SETTINGS on the P4_5, 1060 and Linen | One shape: no line under the title, no break in the left border |
| N14 | The title on the 4B: Desk, its Scenes, its History, a member | The whole "Area > Name" shows, left of centre where it needs the room. If it could not fit even so, the name alone |
| N15 | The checkbox (Paused) | Larger than round 6's |

**Asked**: Linen's default - Silver - Square (round 6) or Silver - Round (round 5)? Built as
Silver - Square; not answered yet.

### Round 7 results (owner, 2026-10-07)

N1-N8, N10, N12, N14, N15 PASS; N13 PASS on the P4_5 and 4B (1060 to do). S3 (round 6) PASS - the
"???" was not understanding it at first. **N9 PARTIAL**: the first time History showed, CHART flew
down from the top of the screen to its tab (P4_5 and 4B); on the 4B the pane did not grow to fit its
words; the owner wants it to open further, with dummy rows, to see two folder tabs side by side.
**N11 FAIL**: folded, CHART should slide down off the screen with the window, as SETTINGS does; only
open should it vanish at once. **Noted**: SETTINGS opens and folds choppier than before on both
boards, the P4_5 a little worse - try it slower, between its old speed and the page deck's.

## Round 8 - CHART as a real folder, the panels' speed, the 1060

Flashed: **WS_P4_5, WS_P4_4B, CYD_P4_1060** (COM9 free again).

| # | Do this | PASS if |
|---|---|---|
| P1 | History, the first time after opening a window | CHART rises from below the screen to its tab, as SETTINGS does - nothing comes down from the top |
| P2 | Tap CHART | It opens as SETTINGS does: a folder tab on the left, its pane wider than the tab with the curve where they join (a mirror image of SETTINGS), five demo rows that work and change nothing |
| P3 | Close the window with CHART folded | Its tab slides down off the screen after the window, as SETTINGS' does |
| P4 | Close the window with CHART open | It goes at once with the window |
| P5 | Open and fold SETTINGS, and CHART, on the P4_5 and 4B | Smoother than round 7 (260 ms, from 220; the page deck's panels take 300) - say if it is still choppy, and the next step is 300 |
| P6 | The 1060: Brightness, Temperature, Color | The control deck does not move at all (L5); the slider does not move either |
| P7 | The 1060: round 7's N1-N15 | As on the other two |

### Round 8 results (owner, 2026-10-07)

P1-P4 and P6 PASS. **P5**: both panels slide smoothly on the 1060 and 4B in every scheme; the P4_5
a little better than round 7 but still notably slower. **N1 on the 1060, Linen: FAIL, intermittent**
- Black - Square and Silver - Round drew flat (a plain grey selector, lit chips a white disc with the
accent icon); after a reset they drew, but the lit chips did not. Round 6's lines otherwise PASS on
the 1060. **Seen once on the 4B** (and once before, in 2.10a): the whole screen suddenly washed out,
almost white, no depth, the colours lurid - a `/screenshot` at the time looked normal, so a photo was
taken; a reset cured it.

## Round 9 - faces that run out, the P4_5's panels, a hang

Flashed: **WS_P4_5, WS_P4_4B, CYD_P4_1060**. All three carry a TEMPORARY loop watchdog (armed 60 s
after boot, not committed): if the hang below comes back, the board reboots instead of freezing, and
the P4_5 prints where it was stuck to a log on the PC.

| # | Do this | PASS if |
|---|---|---|
| Q1 | On the 1060: Linen, all four looks, then swipe to a page in another scheme and back, several times, opening Desk each time | Every look's selector and the lit chips are metal every time - never a flat disc |
| Q2 | SETTINGS and CHART on the P4_5 | Clearly smoother than round 8 (each frame measured at 8-40 ms, from 35-105) |
| Q3 | The same on the 4B and 1060, Linen included | As smooth as round 8, and the panels' shadows on Linen look as they did |
| Q4 | Use the boards as usual | No freeze. A reboot out of nowhere is the watchdog catching one - say when |
| Q5 | Linen, no look chosen (or `/popup?look=4` on a debug board) | Silver - Round (owner's choice, round 9); Midnight and Fleet still Black - Square |
| Q0 | After round 9's last flash | All three screens show a picture again (the build before blanked them all - see below) |
| Q6 | Once, on the 4B, while watching it: open `http://fleet-ws-p4-4b/panel?resend=1` in a browser | The page says "resent 19 init commands"; the picture stays as it was. If it goes black or changes, say so, and reset the board |
| Q7 | **If the 4B washes out again - before resetting it:** open `http://fleet-ws-p4-4b/panel?resend=1` | Say whether the picture came back. If it does, the panel had lost its settings |

### Round 9 results (owner, 2026-10-07)

Q0, Q1, Q3, Q5, Q6 PASS. **Q2 PASS** - the P4_5's panels clearly smoother; Linen still a little
choppier than the dark schemes, "totally passable". Q4: no reboots reported; no watchdog panic in
the P4_5's serial log. **Found**: (1) a group paused, then each member resumed from its own view -
the group card stayed PAUSED; (2) a selector look chosen in SETTINGS applied on every page, whatever
its scheme. **Asked**: rename "Deck look" to "Selector"; the look should follow the scheme; and the
panel's settings that matter as HA entities, for automations (recorded in FUTURE_IMPROVEMENTS).

Fixed and checked over `/popup` on the P4_5 (Desk): a group pauses with its last member and
resumes with its first, either way round, including paused from its own window and resumed member
by member; a look chosen on Midnight stays on Midnight, and Linen still opens on Silver - Round.

**The merge gate (2026-10-07):** the group-pause fix confirmed by the owner on glass; all nine
environments build (release flags: no `DEBUG_POPUP`, no watchdog); flashed and seen on glass on
WS_P4_5, WS_P4_4B, CYD_P4_1060, WS_P4_7B ("running like a champ") and CYD_P4_4880 - the window seen
in portrait for the first time, "totally usable". The Selector row moved out of the debug build at
the owner's request. **2.10c signed off.**

**The black screens (round 9, owner):** the build with `/panel`'s register read also done at the
end of boot left all three P4s with the backlight on and a black screen, the boards otherwise
answering. Reading a DSI panel while it shows a picture stops the picture; `/panel` reads nothing now
(LESSONS).

## Not tested (by Claude) - round 8

The animations' smoothness (screenshots cannot show it; not measured with `DEBUG_FRAMES`), CHART by
finger, its dropdowns opening, and closing the window with CHART folded (N11's fix: driven only by
reading the code - `/popup?close` closes as a tap outside does, but nothing was watched on glass).
Measured on the 1060 over `/popup`: the deck at x 373..649 on all three controls (it was 372..648 on
Color), the slider at 326.

## Measured, and not tested (by Claude) - round 9

- **The flat faces were the face cache running out**: twelve slots, never freed, one per scheme x
  look x size - two schemes' worth (schemes are per page, so a swipe changes it) filled them, and
  every face after that was drawn flat. Slots are now taken back (another scheme's first, then the
  oldest not used by the open window). Driven on the P4_5 over `/popup` (new: `scheme=0|1|2`), every
  scheme x every look x controls, Scenes and History, five times over: no face drawn flat (the count
  is in `/popup`'s listing), Linen's Black - Square and Silver - Round correct on screenshots. Not run
  on the 1060 itself (COM9 kept refusing), nor by finger.
- **The P4_5's panels**, timed per frame over serial (`DEBUG_FRAMES`): opening and folding SETTINGS
  drew 35-105 ms frames, the same over the nearly empty History view - so not the faces. The
  panel's holder was as wide as the window and transparent, so every frame redrew the cards, the
  window and the panel together. Sized to the tab and pane: **8-40 ms a frame**, about nine frames a
  slide instead of five. Not measured on the 4B or 1060 (already smooth), not seen on glass.
- **A hang, twice, not reproduced since.** During the first run of that test after flashing, the
  P4_5 froze for good (its HTTP server answered "busy" - the UI loop never came back; the owner
  reset it) and the 1060 took over 10 s over one request at the same step (opening Scenes on the
  second pass) but recovered. Three later runs on the P4_5 - nine passes, one straight after a
  reset - ran clean. Cause unknown; the face cache's reuse is the new code on that path,
  and the first suspect. Hence the watchdog.
- **The 4B washed out**: not reproduced, not investigated beyond the owner's photos (see the report).

## Not tested (by Claude) - round 7

Everything on the 1060 (not flashed), including whether its deck still shifts left on Color (L5 -
the P4_5's cause was the "Colour" line, now gone; the 1060's was not found). Pressing a chip, the
CHART panel by finger, the two panels opening over each other (N12), Linen, the dropdown lists, and
a member's title (N14). The faces were checked on screenshots only, which read the framebuffer, so
they are what the panel draws - but not how the glass shows it.

## Not tested (by Claude) - round 6

The dropdown lists (`/popup` cannot open them), scrolling a panel taller than the window (no card has
that many rows yet), All Lamps' Active state, Linen, and everything by finger.

## Not tested (by Claude) - round 5

The drag and the glide (L3, L4) by finger - `/popup` cannot drag; the looks were screenshotted on
Midnight only, not Linen or Fleet; the 1060 was checked by screenshot only.

## Not tested (by Claude) - round 3

Driven over `/popup`: no finger has scrolled the 4B's scene grid or tapped a scene button. Not tried:
HA refusing a scene; scenes kept in HA's `scenes.yaml` (they belong to no device, so they are not
offered); a light with more than 12 scenes (the rest are left out); the 7" panels.

## Not tested (by Claude) - round 2

Driven over `/popup` (G2-G13's logic, against the real bulbs, compared with HA) - no finger on the
glass, and nothing on the Fleet page: All Lamps (G14) could not be reached from the PC. Not tried: a
group whose members differ in what they can do (all three Office bulbs are alike), a group with more
than 8 members (the rest are left out), a member that is also a group, a bulb removed from the room
in Hue (it should leave Desk's Members, but stays in the entity table until a reboot), and anything learnt while a window is open.

## Not tested (by Claude) - round 1

Everything was driven over `/popup` from the PC, so **no finger has touched these windows**: the drag
feel on a real light, tapping the swatches, the timing as seen by an eye. The 300 ms sends were
simulated by sending one value every 300 ms. Also not tried: the Kitchen lights (not mine to
command), a light that is not Hue, a light that goes `unavailable` while its window is open, and HA
**refusing** a call (the code path that marks it FAILED at once has never run: nothing the panel
sends makes HA refuse it).

## Measured (Claude, WS_P4_5, 2026-10-06)

Driven over `/popup` on Desk, with HA's reports recorded from the PC at the same time; the room was
put back at 100%, 2710 K.

| Command from the window | HA reported | The board showed |
|---|---|---|
| Brightness 50% / 40% / 100% | 128 / 102 / 255 | the same, confirmed, no FAILED |
| 4000 K, 2200 K, 2710 K | 4000, 2202, 2710 | the same |
| Colour 225 / 120 (green) / 240 (blue) | 224; 120 then 129, 132, 136 over 4 s; 243 then 249, 255 | confirmed, then followed the bridge's later reports |
| Power off, Power on | off; on at 103 (40%) and hue 137 | the same |
| A drag 10% -> 100%, one value every 300 ms | the room in order, about once a second, ending at 255 | 255 |

Once (not reproduced): after a drag to 100% the board showed 248 for several seconds. In the run
recorded from the PC, HA reported the room's means in rising order (238, 247, 255) and the board
ended at 255. The 248 is unexplained.

Opening Desk's window: built in 21 ms, LVGL's pool 87 -> 95 KB used of 500.

**Round 2 (members), against the Office bulbs, compared with HA after each step:**

| Step | HA | The window |
|---|---|---|
| The lamp to 30% from its own view | lamp 77, the others 255 | the same |
| Lamp paused, Desk opened | - | "through its members", the lamp PAUSED |
| Desk to 60% | right and left 153, lamp 77 | the same |
| Power, Power | right and left off then on (at 153), lamp on at 77 throughout | the same |
| Lamp resumed | - | "as one" again, showing HA's room (128) |
| Desk paused, resumed | - | the room and all three paused, then all resumed |
| Reboot with the lamp paused | - | learnt at 15.0 s, its pause re-applied, its levels fetched |

The boot log: `subscribe_trigger id 2 for 3 entities, learnt since`, then `initial values: 24
fetched, 0 failed` (21 declared + 3 learnt). The first build kept a paused member's levels out at
boot (its on/off came through, its brightness did not); fixed - a paused entity now takes its first
reading whole. All three bulbs put back at 100%, 2710 K, none paused.

**Round 3 (scenes):** the boot log - `render_template id 2: the scenes of 5 lights (389 B)`, `scenes:
7 for 1 lights`, then their subscription, and `initial values: 31 fetched, 0 failed` (21 declared,
7 scenes, 3 members). Relax loaded from the window twice: HA's room went to it (the bulbs 2240-2440
K, 56-76%). Desk's window on Scenes: built in 9.6 ms, LVGL's pool 96 -> 99 KB. On the 4B the window is
600 x 568 px; Desk stacks with the slider at its full 267 px (the window had the height). One flaw
fixed on the way: the grid's edge clipped the ring round a bottom-row scene. The room put back at
100%, 2710 K.
