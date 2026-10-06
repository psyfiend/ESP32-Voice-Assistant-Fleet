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
