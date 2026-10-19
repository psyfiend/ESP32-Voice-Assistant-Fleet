# TEST - 2.11: group cards, clusters and the arranger

Rounds in order, each with your results under it. The design is `docs/design/group-cards.md` §6-7
(DECISIONS K88-K107, D-34, D-35); the build order you agreed on 2026-10-19 (K106): round 1 clusters
on the page, round 2 the arranger, round 3 what the glass edits (with the keyboard), round 4 the
summary card, count slots, extending a source group and group pages.

# Round 1 (2026-10-19, morning, built unattended) - clusters on the page

Branch `feat/2.11-group-cards` (off `main` at `6bd834f`: `v0.2.18` and the design). **No pull
request** - the PR comes at sign-off, after the all-nine gate. Flashed to the **P4_5** (COM15) and
the **4B** (COM7), and **once to the 1060** (COM9) for D-34. Each board's settings file was saved
first (`bench/settings/<board>/20261019-*.json`); nothing in it was changed by this round.

## What changed

| Where | Before | Now |
|---|---|---|
| **The House page** | 18 cards | **Office, Kitchen and Outside are clusters** - group cards made on the device, shown as a block of their members (K88-K92): one border, a header, the members inside as mini tiles. Garage's doors, Living's Overhead, Front's Thermo, Eric's bedroom and the garage's temperature stay cards. The doors wait for the summary card (round 4, K98) |
| **A cluster's header** | - | **one line** (K100): the group's colour as a **swatch** beside the cluster's name on the left (K102), and the header's members on the right - **Office and Kitchen: temperature, then occupancy; Outside: the deck's temperature, then its lux**. A value is its icon in its tint and the number with its unit; where there is no room the icon goes first, then the last value (K101, K104: no fixed number of slots - the page header's planner decides). **Occupancy is its own icon for each state** - the figure while someone is there, the struck-through one while clear (K101). Dashes while a value has not arrived or its source says unavailable (D-15) |
| **Mini tiles** | - | each member that is not in the header: **no border**, its hero and its entity label under it (K89). A light or switch is **the lit disc** (K100): a solid disc in the active colour - or the light's own colour - behind a dark glyph while on, the faint disc while off; a disc too small for its glyph is not drawn and the glyph takes the colour. A sensor is its number and unit (Outside's Front) |
| **Touching a cluster** | - | **a tap on a tile** is that member's own tap (a light toggles); **a long press on a tile** opens that member's own window, as a card's (K104) - Desk's is Desk's, with its scenes. **A long press on the header** opens the cluster's window on its **Members**; **a tap on the header does nothing** (K107) |
| **The members are the same cards** | - | Desk, Overhead, Sink, Table, Porch, Front and the sensors keep **their card ids**, so everything you saved on them is still theirs (Desk's tap, the history spans). Desk's custom card label "Office" does not show: a mini tile has no card label; the cluster's header names the group once (K92 rule 2) |
| **Where a cluster goes** | - | placed by today's flow engine, **as many cells as its tiles need** (`Layout::clusterSpan()`: the fewest cells holding every tile at 7.5 mm or more, wider first) - two cells across for the House page's three. A cluster that cannot fit on a board is shown as **its one card** (K92 rule 6): its lights and switches behind one tile. The arranger replaces this in round 2 |
| **A cluster's top edge** | - | in line with the cards' beside it: it keeps the room a floating tag takes above a card, with no tag in it |
| **The cards** | each set its own look | **built from the kit's roles** (`KS_CARD_*`): the surface, body, tag, badge, disc, name. **Nothing on screen changed** - the harness compared every state before and after on the P4_5 and the 4B (below). It is what lets the mini tiles and clusters share the cards' looks |
| **A header slot's panel** (D-35) | square where it met the bar, the bar's line running into it | **it lies on the bar**: its top corners turn down from the line, and **the bar's line fades out over 2.5 mm** and stops short of the panel's walls - "so the lines don't contact the panel" |
| **The halo** (D-35) | Linen's shadow; nothing on the dark schemes | **a black rim 5 mm wide at full strength, fading to clear, round every header panel (and its tab) and every window** - the card window, About, UI Tweaks. On Linen it takes the place of the old shadow. No dim |
| **The 7" boards' header words** (D-34) | 14 px (2.1 mm) on the 7B and the 1060 | **16 px (2.4 mm)** - one step up; the P4_5's are 2.25 mm and the 4B's 2.19. The tags stay 14. The 7B gets it at the next flash |

## The pictures

The House page on the P4_5 (Midnight), before and now:

![Before](../followups/211_house_before_p45.png)
![Now](../followups/211_house_p45.png)

The 4B (Linen) - the Outside header drops the lux icon to fit, as planned:

![The 4B](../followups/211_house_4b.png)

The page panel lying on the bar, with its halo (P4_5, then the 4B on Linen):

![The panel on the bar](../followups/211_pagepanel_p45.png)
![On Linen](../followups/211_pagepanel_4b.png)

A window with its halo (Desk's, from its mini tile):

![Desk's window](../followups/211_deskwin_p45.png)

The 1060 with the 7" header words at 16 px:

![The 1060](../followups/211_house_1060.png)

## Tests for you

To try the cluster settings that round 3 will put on the glass, from the PC (each takes effect when
the page is next built - swipe away and back):

    curl "http://fleet-ws-p4-5/settings?card=office_261019_0500&present=card"
    curl "http://fleet-ws-p4-5/settings?card=office_261019_0500&present="
    curl "http://fleet-ws-p4-5/settings?page=house&cl_style=bar"
    curl "http://fleet-ws-p4-5/settings?page=house&cl_style="
    curl "http://fleet-ws-p4-5/settings?card=office_261008_0310b&hidden=no"
    curl "http://fleet-ws-p4-5/settings?card=office_261008_0310b&hidden="

| # | Do this | PASS if |
|---|---|---|
| T1 | Look at the House page on the P4_5 | Office, Kitchen and Outside are clusters two cells wide, their tops in line with the cards beside them; each header one line - swatch, name, values; the tiles borderless with their names under them |
| T2 | Turn a light on and off from a tile (Office's are safe, W9) | its disc fills with the active colour (or the light's own) behind a dark glyph, and empties again; nothing else in the cluster moves |
| T3 | Long-press a tile (Desk) | Desk's own window, exactly as its card's was - its controls, scenes, History, SETTINGS |
| T4 | Long-press a cluster's header | the cluster's window on its Members - every member listed with its state; a tap on one opens its view |
| T5 | Tap a cluster's header | nothing happens |
| T6 | Watch the headers for a while | the temperatures follow their sensors; occupancy shows the figure while someone is there and the struck-through one while clear |
| T7 | Swipe to Fleet and back; change scheme (UI Tweaks) | the clusters come back the same; on each scheme the swatch, rule and tiles read |
| T8 | Open each header panel (clock, temperature, House, status) | each panel's top corners turn down from the bar's line; the line outside fades out short of the panel; a black halo all round the panel and its tab, smooth - no steps or bands |
| T9 | Open a card's window; About; UI Tweaks | each window stands on the same halo; it opens as fast as before (the first frame complete) |
| T10 | The settings above from the PC: Office as a card, then back; the page's clusters as Bar, then back; Office's temperature visible as a tile, then back | Office becomes one card (its two lights behind one tile) and returns; the headers become bars in each group's colour with the words in the surface's colour; a temperature tile joins Office's tiles while it stays in the header |
| T11 | The 4B (Linen) | as T1-T9 at 720 px; the Outside header fits by dropping the lux icon |
| T12 | The 1060 | the header's words (clock, page) look as large as the P4_5's and the 4B's - not "notably smaller" |

## Choices that were mine

- **The halo is painted, not LVGL's shadow.** As LVGL's shadow (5 mm, spread 2.5 mm) it cost a full
  redraw with a window open 45 ms more on the P4_5 (129 to 175 ms) and banded into visible steps
  (black to Midnight's ground is a handful of 16-bit levels). Painted as dithered A8 images (four
  corners, four tiled edge strips, `UI/Kit/KitHalo.cpp`) it costs ~28 ms on the same frame and does
  not band - §15's own fallback (D-33's way). Numbers below.
- **The halo replaces Linen's lift** on panels and windows - one rim, not a rim and a shadow.
- **Mini tiles' labels are the TAG face** (the cards' small words), so two names fit side by side; a
  value tile's number is the small value face where it fits, else the name's.
- **Header values are the NAME face**, the name too; the icons in their tints (white on a Bar).
- **The swatch is the default** (the mock's recommendation; K102 left it open). Bar is one setting away.
- **A cluster has no state of its own**: its members show theirs - a STALE or FAILED badge and the
  long-stale diagonal on a tile, as on a card. The window's members list says which.
- **The cluster's window, opened from the PC (`/popup?card=`), starts on the controls** of its
  first member's kind - the light's, for Kitchen; on glass a long press opens Members. Round 3's
  management screen goes inside this window.
- **The line stops a quarter of the panel's corner radius short of its walls**, after its 2.5 mm fade.

## Questions for you

1. **The halo on Linen** (the 4B): black at full strength reads heavier on a light page than on
   Midnight. Keep it as chosen, or lighten it on light schemes? *Mine: look at T8 on the 4B first; if
   it is heavy, the same 5 mm at ~60% on Linen.*
2. **D-34 on the 1060**: is 16 px right, or one more step (18)? *Mine: 16.*
3. **The P4_5's tiles at 5 x 3** are about 9 mm tall under a 5 mm header - the touch minimum. Rows 2
   (UI Tweaks) gives them more; the arranger (round 2) will choose a 5 x 2 page for the House page if
   it keeps the playground's verdict. *Mine: leave it to round 2.*

## Results (mine, before you)

- **Harness A/B for the cards onto the kit** (`r1base` / `r1kit`, P4_5 all 62 states, 4B six): the
  pages' pixels all explained - **no pixel moved**; the only tree differences are fonts on labels in
  hidden status rows ("Text", never drawn). Desk's light changed between captures, so its window
  states said "capture again"; the windows' styles are untouched by this step.
- **Round 1's walk** (`r1d`, P4_5): all 58 states open; the four Deck (Outside) states are absent -
  the deck's temperature is now a value in Outside's header, not a tile.
- **LVGL's pool on the House page** (P4_5): 105.6 KB before, 102.8 KB with the cards' shared
  styles, **96.6 KB with the clusters** (435 objects, was 457).
- **Frame times**, `/bench what=full`, render ms (a full redraw - the worst case):

  | | P4_5 page | P4_5 window open | P4_5 clock panel open | 4B page | 4B window | 4B clock panel |
  |---|---|---|---|---|---|---|
  | `v0.2.18` | 89.8 | 129.3 | 105.7 | 88.1 | 127.4 | 106.3 |
  | round 1, LVGL's shadow as the halo | 71.8 | 174.6 | 143.1 | 70.2 | 145.1 | 119.4 |
  | **round 1, the painted halo** | **71.0** | **139.4** | **103.1** | **70.3** | **119.2** | **94.4** |

  The page itself got faster (fewer objects: the clusters). With a window or a panel open, the halo
  costs ~28 ms (window) and ~16 ms (panel) on a full redraw of the P4_5.
- **Host tests**: 43, all PASS - `test_cluster`'s nine new (the tile grid, the cluster's span).
- **Structure check**: two files over 1,000 lines (`GUIManager.cpp` 1,490, `Card.cpp` 1,448 - down
  from 1,468); `lv_obj_set_style_*` outside the kit **399 in 26 files** (438 at `v0.2.18`) with the
  two new files counted; colour literals 10 in 2 files, unchanged.

## Your results

(to come)
