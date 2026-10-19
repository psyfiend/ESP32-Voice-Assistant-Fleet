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

**2026-10-19, afternoon** (rounds 1-3 tried together, not test by test; HA had crashed overnight, so
the morning's readings were dashes). "I like it!" The notes, in short:

- **The halo (D-35)**: "the effect itself is outstanding", but not calibrated - **too strong
  everywhere** (about 50% or lower), and on **Linen** it stops reading as a shadow and looks like a
  black border. **Its corners need work**: where a panel's top corners meet the bar (the page panel
  and the system panel on Linen, pictured in the session) the halo leaves square marks, and the
  tab's coves show dark specks.
- **Icon sizes must match**: a cluster's tile icons and a solo card's must be the same size (or very
  close), on every grid. If that is too hard, drop the disc in a tile so its icon can be a solo
  card's size, and show a tile that is on with the page's card state round the tile (Bracket, Corner
  cut or Fill) - borderless until active.
- **Cluster headers** take a cell's height and leave neighbours' icons out of proportion - bigger
  or smaller depending on the grid. **Clusters should fill their whole cells**: Float and Tag take
  room from a solo card for its label, and a cluster has no such label - let only solo cards wear
  that room. (An idea set aside by the owner: the sensors in tags of their own on the right.)
- **The grid**: clusters need more room than a solo card's cells give - more height above all.
  Use cells and sub-units to even it out (a solo card ~0.75 of the room a cluster's cell gets;
  double the grid so a card is 2x2 sub-units); aim for **tiles and cards of the same shape**, so
  clusters may need to be bigger. A thought for the arranger's logic.

# Round 2 (2026-10-19, morning, built unattended) - the arranger

The same branch, on round 1. Flashed to the P4_5 and the 4B (settings saved first); the 1060 keeps
round 1 (flashed once, for D-34). Design: `layout-engines.md` "As prototyped" and "As built"; K105.

## What changed

| Where | Before | Now |
|---|---|---|
| **Where blocks go** | the flow engine: each card or cluster at the first free place, reading order | **the arranger - Layout Playground v5, as prototyped** (K105): every tiling of the board's grid and its neighbours (a row or column either way), the cheapest page by the playground's weights - **an empty cell costs 60**, a cluster with too much room, a long thin cluster, a block out of reading order, leaving the board's grid, single cards drifting from today's size |
| **The P4_5's House page** | 5 x 3 with three cells empty at the bottom right (round 1) | **5 x 3 with none empty**: the three clusters three cells wide, the doors and the four cards around them |
| **The 4B's** | 4 x 3, full | the same 4 x 3 - it was already the cheapest |
| **When it runs** | - | **when a page's blocks change, and the answer kept** (K105) - in the settings file, `pages.<slug>.layout`, with a fingerprint of what it was arranged for (the blocks, the page's room, the weights). A build that matches uses it at once; one that does not is built as before while the arranger runs **on a task of its own**, and the page is built again when its answer is kept - so the screen never waits. Your next boot uses the kept answer straight away |
| **How long** | - | **P4_5: 150,031 steps, 414 ms** (one neighbouring grid hits the playground's 150,000-step cap); **4B: 57 steps, 1 ms**. No S3 to time (the S3_4B is locked, #69); it compiles |
| **The Fleet page** | flowed | still flowed: the arranger finds no page for its 16 cards on the board's grid or its neighbours at the playground's cell sizes (`/popup` says so). A smaller `mincell` would let it try |
| **Your Col / Row** (UI Tweaks) | obeyed | still obeyed: the arranger uses that grid (the other count the board's) |

![The P4_5, arranged](../followups/211_r2_house_p45.png)

## Tuning from the PC

Each is kept in the settings file (`system.arranger`); the page is arranged again when next built -
swipe away and back. Empty `v` puts the playground's default back.

    curl "http://fleet-ws-p4-5/settings?arranger=hole&v=40"
    curl "http://fleet-ws-p4-5/settings?arranger=hole&v="
    curl "http://fleet-ws-p4-5/settings?arranger=off&v=1"
    curl "http://fleet-ws-p4-5/settings?arranger=off&v="
    curl "http://fleet-ws-p4-5/popup"

The weights, in plain words: **hole** (60) what an empty cell costs - lower lets the arranger leave
cells empty rather than stretch a cluster; **comfy** (1.6) how roomy a cluster's tiles may be before
it pays for spare room; **loose** (25) what that spare room costs, by its square - so two clusters a
cell bigger each cost less than one two cells bigger; **long** (40) what a long thin cluster (beyond
1.8 : 1) costs; **order** (4) what moving a block ahead of an earlier one costs; **away** (6) what
each row or column away from the board's own grid costs. The sizes (mm): **maxcell** 34 and
**mincell** 14 a grid cell's size; **tilew** 12 x **tileh** 9 the smallest tile the arranger plans
for; **hdr** 5 a cluster's header. `/popup`'s second line says what the last run did and how this
page was placed. **off** = 1 lets every page flow as in round 1, to compare.

## Tests for you

| # | Do this | PASS if |
|---|---|---|
| A1 | Look at the House page on the P4_5 | no empty cell; the three clusters three cells wide; nothing overlaps |
| A2 | Reboot the P4_5 (the reset button) | the House page comes up arranged at once - no flowed page first |
| A3 | `arranger=off&v=1`, swipe away and back; then `off&v=` | round 1's flowed page; then the arranged page again, at once (its answer is kept) |
| A4 | `arranger=hole&v=40`, swipe away and back; then `hole&v=` | the page may change once the new answer is ready (a moment after the swipe back); then back |
| A5 | Office as a card (round 1's T10), swipe away and back | the page is arranged again for the new blocks, a moment after it is built |
| A6 | The 4B | its House page as before, full |

## Choices that were mine

- **A cluster always counts one member in its header** for the arranger, so it never gets a single
  headerless cell - one-cell clusters (K103, sensors only) are round 4's, and not drawn yet.
- **The page is built first and arranged after** when there is no kept answer, rather than waiting
  for the arranger: a first boot, or a change, shows the flowed page for under half a second.
- **Every page of up to 24 blocks** is arranged; a page the arranger finds no page for flows, and
  it is not tried again until its blocks or the weights change.
- **The arranger's own weights stay the playground's** - nothing tuned yet; that is yours on glass.

## Results (mine, before you)

- **Host tests**: 52, all PASS - `test_arranger`'s nine: the playground's answers for its House
  blocks on its seven boards (P4_5 5 x 2, 7B 5 x 3, both 4Bs 4 x 3, the 4880 5 x 2 and 2 x 5, none
  for the 3248), every grid, place and score to 1e-5; the same answer twice; a fixed grid obeyed.
- On the boards: as above - the kept answer is used after a reboot on both.

## Your results

(to come)

# Round 3 (2026-10-19, morning, built unattended) - what the glass edits

The same branch, on round 2: `bd88809` (the cluster's window), `751d1f5` (the keyboard), `29a93a2`
(Visibility and hidden cards), `3816fe6` (Copy / Move), `6a5bde9` (removing a copy), `96b54ba` (the
cluster's Add cards), `2275545` ("which group?"). Flashed to the P4_5 and the 4B, each board's settings saved first. **Every change
the tests made was put back** from those saved copies (`restore_settings.ps1`): the P4_5's file is
the same as at 09:39, the 4B's as at 09:28. Words on screen are the "2.11 Clusters" mock's (section
D) where it had them - proposals, K96 left the names open.

## What changed

| Where | Before | Now |
|---|---|---|
| **A cluster's window** (a long press on its header, K107) | its Members | **its management screen** (K104): HEADER - **Name** (Rename: the keyboard), **Style** (Page's / Swatch / Bar), **Sensor icons**, **Header slots** (Choose: the members, each ticked into the header or not, in the order ticked, four at most); MEMBERS - each member with **Visible** (a tile or not); **Add cards** at the foot. Changes show when the window closes |
| **The on-screen keyboard** (K95) | - | **a kit component** (`Kit::keyboard()`): over everything, the rest dimmed - a box with what the words are for, a one-line field and Cancel / **Save** (the accent), above LVGL's keys along the bottom (lower case, ABC, 1# symbols, the arrows, backspace, the hide key, the tick). **Plain ASCII only** - the keys offer nothing else and the field refuses anything else. Keys 7 mm tall: four rows take about half the P4_5's height. It goes with the window under it, unsaved |
| **Naming a cluster** | from the PC | **Rename** types its own words (`label_text` on the cluster - in its group's colour, K46); an empty field is its group's name again (shown faintly in the field). The window's title follows at once, the cluster when the window closes |
| **Card UI's Custom chips** | Entity label's Custom kept the words the card showed; Card label's Custom likewise | **a tap on Custom opens the keyboard** on the words the card shows now: Entity label - the card's name (`name`, K37, K40); Card label - its card label's words (`label_text`; none: the colour alone) |
| **SETTINGS** | Remove card, on cards added on the device | **Visibility**, on every card: a view whose choices depend on **where the card is** (K96) |
| **Visibility, a card on the page** | - | **Hide from this page** - K66's remove, now **for every card**: after the dialog ("Hide this card?"), to the page's pool with its settings (`removed`: `yes`), and the page is arranged again. **Add to a cluster** - below |
| **This page's hidden cards** | - | **listed in the page slot's panel** (tap the page's name in the header), each with **Restore**, which puts it back where it was |
| **Visibility, a mini tile** | - | **Hide from the cluster** - the window closes and the tile goes (its Visible, K104, brings it back); **Add card to dashboard** - a copy right after the cluster, in its group (K65); **Remove from the cluster** on a copy made on the glass, after a dialog - deleted, entry and all (the card it came from is untouched; the dashboard's own members only hide) |
| **Add to a cluster** (Visibility, on the page) | - | **the page's clusters as chips, "Copy - stays here" / Move** (K100: one screen, Copy first), and Add. A copy is a card of its own (`in`: the cluster's id - K106's key) **with the name the card shows** ("Thermo", not HA's "THERMOSTAT Temperature"), wearing the cluster's group (K92 rule 2). **Move** is a copy, and the card itself to the page's hidden cards. Only a card standing for one thing - not a cluster or a group card (one level, K92 rule 3) |
| **A cluster's Add cards** | greyed | **the page's cards that can join, each with a box**, then Copy / Move and Add - the same as above, several at once |
| **Add card to dashboard** (a member's own view; a tile's Visibility) | the card made at once, in its group card's group (K65) | **"Joins which group?" first** (K92 rule 9): its group card's group as the first chip, **chosen already** - Add is still one tap - its entity's HA-area group beside it when that differs, and every other group in a list. The card goes right after its group card, in the group chosen |

## The pictures

The cluster's window, its header slots (4B), and the keyboard over it (P4_5, then the 4B on Linen):

![The cluster's window](../followups/211_r3_cluster_window_p45.png)
![Header slots](../followups/211_r3_header_slots_4b.png)
![The keyboard](../followups/211_r3_keyboard_p45.png)
![The keyboard on Linen](../followups/211_r3_keyboard_4b.png)

Visibility on the page (4B), its dialog, and the page's hidden cards (the P4_5 with Thermo hidden;
the 4B's list - cards removed in October's K17 tests, already in its pool):

![Visibility](../followups/211_r3_visibility_4b.png)
![Hide this card?](../followups/211_r3_hide_dialog_p45.png)
![Hidden cards](../followups/211_r3_hidden_cards_p45.png)
![The 4B's](../followups/211_r3_hidden_cards_4b.png)

A tile's Visibility, and a copy's (with Remove):

![A tile](../followups/211_r3_visibility_tile_p45.png)
![A copy](../followups/211_r3_copy_visibility_p45.png)

Add to a cluster; Thermo moved into Kitchen (and copied into Office before it); copied into Office
on the 4B; the cluster's Add cards, and Office with two cards added:

![Add to](../followups/211_r3_add_to_cluster_p45.png)
![Moved](../followups/211_r3_moved_p45.png)
![Copied on the 4B](../followups/211_r3_copied_4b.png)
![Add cards](../followups/211_r3_add_cards_p45.png)
![Added](../followups/211_r3_added_p45.png)

"Which group?" for Kitchen's Sink (its area's group is its cluster's, so one chip), and its card
made, right after the Kitchen cluster:

![Which group](../followups/211_r3_which_group_p45.png)
![Made](../followups/211_r3_joined_p45.png)

## Tests for you

Office's lights are safe to use (W9). Each test puts back what it changed.

| # | Do this | PASS if |
|---|---|---|
| E1 | Long-press Office's header | its window: HEADER (Name, Style, Sensor icons, Header slots) and MEMBERS (each with Visible, Add cards) |
| E2 | Style: Bar, close; then Page's | Office's header is a bar in its colour; then a swatch again |
| E3 | Header slots: untick occupancy, back, close; then tick it again | the header shows the temperature only and occupancy comes back as a tile; then as before |
| E4 | Members: untick Overhead's Visible, close; then tick it | Overhead's tile goes; then comes back |
| E5 | Name: Rename, type "Work" on the keys, Save | the title says Work; after closing, so does the header. Rename again, empty the field (backspace), Save: Office again |
| E6 | The keyboard: ABC for capitals, 1# for symbols, the arrows, backspace held down; then the hide key, and a tap outside the box | it types what the keys say and nothing else; held backspace repeats; the hide key and a tap outside leave without saving |
| E7 | Desk's window - SETTINGS - Card UI: tap Entity label's Custom | the keyboard on "Desk"; change it, Save: the card's name changes. Tap Inherit: back |
| E8 | The same for Card label's Custom | the keyboard on the card label's words; Save changes them, in the group's colour. Inherit: back |
| E9 | Thermo's window - SETTINGS - Visibility - Hide from this page - Hide | the dialog asks; Thermo goes and the page is arranged again (a moment later) |
| E10 | Tap "House" in the header | HIDDEN CARDS lists Thermo with Restore; Restore puts it back |
| E11 | Long-press Porch's tile - SETTINGS - Visibility - Hide from the cluster | the window closes and Porch leaves Outside. Outside's window - Porch's Visible - close: back |
| E12 | Thermo - Visibility - Add to a cluster: Office, Copy, Add | a Thermo tile joins Office (named Thermo) and Thermo stays on the page |
| E13 | Long-press that new tile - SETTINGS - Visibility - Remove from the cluster - Remove | the copy goes; Office as before |
| E14 | Thermo - Add to a cluster: Kitchen, Move, Add | Thermo leaves the page (listed under HIDDEN CARDS) and joins Kitchen. Remove the copy (E13), Restore Thermo (E10) |
| E15 | Office's window - Add cards: tick two, Copy, Add | both join Office. Remove each copy (E13) |
| E16 | Long-press Sink's tile - SETTINGS - Visibility - Add card to dashboard | "Joins which group?" with Kitchen chosen; pick another from the list, then Kitchen again; Add: a Sink card right after the Kitchen cluster (the page arranged again). Hide it from the page (E9) |
| E17 | The 4B: E1, E5, E9-E10, E12-E13, E16 | the same at 720 px on Linen; the keyboard's keys about 5.6 mm wide |

## Choices that were mine

- **Visibility replaced SETTINGS' Remove card.** Hide from this page is K66's remove for every card,
  with the same dialog in K96's words; for a card added on the device it is what Remove was.
- **This page's hidden cards live in the page slot's panel** - the mock's "this page's list of hidden
  cards"; the page's name in the header is the page's own place. Each has Restore; **Delete** (K66:
  "deleted only by the owner") is not built.
- **Tapping Custom opens the keyboard**, every time, on the words the card shows - one choice, no
  separate Rename button. Cancel leaves the card Custom with the words it had.
- **A cluster's name is its card label's words** (`label_text`), as a card's own words (K46 rename
  a): in its group's colour, and empty is the group's name. A name the same as the group's is not
  kept, so a group renamed later still names it.
- **The keyboard** is LVGL's keys in the kit's roles; 7 mm keys (a phone's - four rows of the 9 mm
  touch minimum left the P4_5's field no room); a cursor that does not blink (a blink redraws the
  field twice a second); Save is a new button role, **CONFIRM**, in the accent.
- **"Add to a group card or cluster" became "Add to a cluster"**: group cards and a new group card
  as targets wait for the card list.
- **"Which group?" is a view of the window**, not a dialog: chips for the two suggestions and a list
  of the rest did not fit a dialog's box. The suggestion chosen already keeps Add one tap (the mock's
  "one tap: the first is chosen already").
- **A copy takes the name its card shows** (kept as its custom name), so Thermo's copy is "Thermo".
- **Move is Copy and Hide from this page**: the card goes to the page's hidden cards, restorable -
  K92 rule 10's "Take them off the page" read as the pool, not a deletion.
- **A copy made on the glass can be removed for good**; a dashboard member only hides.
- **The tests' changes were undone by restoring each board's saved settings file**, so your file is
  exactly as you left it.

## Questions for you

1. **The hidden-cards list in the page panel** - right home, or a window of its own? *Mine: the
   panel; a page with many hidden cards scrolls it.*
2. **Custom opening the keyboard** - or a Rename button beside the chips? *Mine: Custom.*
3. **The pools hold cards from the K17 tests** (October 14-18): the 4B's House page Office lamp and
   Office Right, twice each; the P4_5's Office Right four times (and a Kitchen lamp on its Home page).
   **Delete beside Restore**, with a dialog, in round 4? *Mine: yes.*
4. **Two members of one name** in a cluster (Office's own Overhead and a copy of Living's) read the
   same on their tiles. Show the card label's words under a copy from another group, or leave it to
   a rename? *Mine: leave it; the copy can be renamed (E7).*

## Not built (round 3's rest)

Group cards and a new group card as Add targets (K96); the card list (K91: HA entities not on the
page, making a cluster from a group); Delete from the pool; the keyboard for groups' names (nothing
names a group on the glass yet). A card made from a member keeps HA's name (K65), as before.

## Results (mine, before you)

- **Every piece verified on both boards** with `/popup` and screenshots (the pictures above), and
  undone: Thermo hidden and restored; Porch hidden from Outside and back; Thermo copied into Office
  (the arranger re-arranged the page in 76 ms on the P4_5, 1 ms on the 4B), moved into Kitchen (39
  ms), the copy removed; two cards added to Office at once (26 ms); Sink's card made in Kitchen
  through "which group?" (10 blocks, 221 ms).
- **The keyboard costs ~7 KB of LVGL's pool** while it shows (P4_5: 105 to 112 KB with Office's
  window under it).
- **Host tests**: 52, all PASS (no layout code changed this round).
- **`WS_S3_4B` compiles** with the keyboard (built once, not flashed - #69).
- **Structure check**: two files over 1,000 lines (`GUIManager.cpp` 1,489, `Card.cpp` 1,448 -
  neither grew); `lv_obj_set_style_*` outside the kit 402 in 28 files (399 in 26 after round 1: the
  two new views' list columns, as the cluster's window has); colour literals 10 in 2 files and shape
  numbers none, unchanged; no LVGL in the core layers.

## Your results

**2026-10-19, afternoon** (with round 1's notes): "lots of thoughts and notes about the sheets" to
come; **at a minimum, the new views should look like Card UI and UI Tweaks** - the same face, chip
styles, buttons, and the same columns and justification.

# Round 4 (2026-10-19, afternoon) - your notes, and the summary card

The same branch: `a61718a` (the summary card), `d15b0e6` (the halo, its corners, clusters filling
their cells, tiles' discs), `ea68c95` (the views as Card UI). Flashed to the P4_5 and the 4B, their
settings saved first; nothing in them was changed this round. Your answers before it (2026-10-19):
tiles keep the disc and match the solo cards in size and look, the grid giving clusters the room;
**clusters fill their cells**; **the grid goes to Layout Playground v6 first**; the halo's strength
**a slider in UI Tweaks**, 50% on Midnight and Fleet and 30% on Linen until moved.

## What changed

| Where | Before | Now |
|---|---|---|
| **The halo's strength** | full black at the edge, every scheme | **50% on Midnight and Fleet, 30% on Linen**, and **a Halo slider at the foot of UI Tweaks** (0-100%), kept per scheme - each scheme remembers its own. Reset all puts each scheme's own back. Painted at that strength, so it stays smooth (drawn faded, the steps came back) |
| **Where a panel meets the bar** | square marks beside the coves, dark specks in them, a light strip at the panel's top corners; on Linen a grey patch at the system panel's corner | **the tab, its coves and the panel's top are one shape with one halo**, painted for the shape: no marks, nothing darkened twice. The panel's top corners are whole arcs; **a panel flush with its tab has a square corner on that side** (the system panel's right); the bar's own drop shadow (Linen) is off while a panel is open - the halo is the panel's shadow |
| **Clusters** | kept a tag's room above them, so their tops lined up with the cards' | **fill their whole cells**; only solo cards wear Float's and Tag's room. A cluster's top stands a little above its neighbours' |
| **Tile icons** | sized from the tile alone - bigger or smaller than the solo cards' | **a solo card's disc and icon** wherever the tile has the room (measured from a 1x1 card on the page); smaller only where it does not. **On the 4B they now match; on the P4_5 at 5 x 3 the tiles are still smaller** - that is the room v6 is for. A tile's disc no longer overflows its room (it did once clusters grew taller) |
| **The garage doors** | two cards, North and South | **one summary card, "Doors"** (K98): open while either door is, its words naming the open one - **"North", "South", "Both"** (more than two: "All", or "2 open"); a **tap opens its Members** (never a door's own action); same ids, so North's and South's settings are theirs |
| **The views** (the cluster's window, Header slots, Add cards, Visibility, Add to, which group) | rows of words and checkboxes, buttons beside them | **as Card UI and UI Tweaks**: the kit's chip table - each option's name right-justified against its chips (stacked on a narrow window) - and the actions as buttons below. **Members: Shown / Hidden**; Header slots: **Yes / No**; **Add cards: each card No / Copy / Move** (one choice per card); which group: **every group a row with Join**, its group card's first and chosen |

## The pictures

The page panel and the system panel on Linen (the 4B, close up) - smooth halo, whole arcs, the
system panel's square corner - and the Halo slider:

![The page panel](../followups/211_r4_pagepanel_corners_4b.png)
![The system panel](../followups/211_r4_syspanel_corner_4b.png)
![Halo slider](../followups/211_r4_halo_slider_p45.png)

The House pages - clusters to their cells' edges; the 4B's tiles the solo cards' size, the P4_5's
smaller:

![The 4B](../followups/211_r4_house_4b.png)
![The P4_5](../followups/211_r4_house_p45.png)

The summary card's words on a test build with the doors pointed at two virtual lamps ("Both"; one
read "North"), not kept; its Members on the real doors:

![Both](../followups/211_r4_summary_both.png)
![Doors' members](../followups/211_r4_doors_members_p45.png)

The views as Card UI (P4_5, and the cluster's window on the 4B):

![The cluster's window](../followups/211_r4_cluster_window_p45.png)
![On the 4B](../followups/211_r4_cluster_window_4b.png)
![Add cards](../followups/211_r4_add_cards_p45.png)
![Visibility](../followups/211_r4_visibility_p45.png)
![Which group](../followups/211_r4_which_group_p45.png)

## Tests for you

| # | Do this | PASS if |
|---|---|---|
| F1 | Open each header panel on Midnight, then on Linen | the halo reads as a shadow, about half as strong (lighter on Linen); smooth, no steps; no marks beside the coves or at the panel's top corners |
| F2 | The system panel (the status icons) on Linen | its right side runs straight from the tab into the panel - no notch; no grey patch at the corner |
| F3 | UI Tweaks - scroll to the foot - move Halo | the number follows the knob; on release the window's halo changes; switch scheme: that scheme keeps its own; Reset all: 50 / 30 again |
| F4 | Look at the House page on each board | clusters reach their cells' edges; on the 4B the tiles' discs are the solo cards' size |
| F5 | Open a garage door (or wait for one) | Doors lights up and reads "North" or "South"; both open, "Both" |
| F6 | Tap Doors | its Members: North and South with their states |
| F7 | Long-press Office's header | the window as Card UI: Header style, Sensor icons, each member Shown / Hidden; Rename, Header slots, Add cards below |
| F8 | Add cards: one card Copy, another Move, Add | both join Office; the moved one leaves the page for its hidden cards. Remove the copies (a tile's Visibility) and Restore the moved one (House panel) |
| F9 | Thermo's Visibility; Add to a cluster; Sink's Add card to dashboard | buttons and chip rows as Card UI's; each still does what round 3's did |

## Choices that were mine

- **The halo's strength is a per-scheme setting** (`schemes.<name>.halo`), as the selector's look is:
  a slider on Linen does not move Midnight's.
- **The bar's drop shadow is off while any panel is open** - the simplest way to keep it off the
  halo's corners; it returns when the last panel has gone.
- **A tile matches the page's solo card measured from the first 1x1 card on the page** - every 1x1
  card's hero is the same size on a page, so any one will do.
- **"Doors"** for the summary's own name (in Garage); "All" past two members and "2 open" for some -
  your "North" / "South" / "Both" are the two-member case.
- **The summary's tap is Members** - K98 offered History, Members or a small window; Members says
  which door, and a tap never moves a door.
- **One choice per card in Add cards** (No / Copy / Move) in place of a box per card and one Copy /
  Move for all - it fits the chip table and lets one card copy while another moves.
- **"Which group?" lists every group as a row**, its suggestions first, in place of two chips and a
  list - one control, as the chip table has.

## Questions for you

1. **Halo on Linen at 30%** - right, or lower still? (The slider is there to find out.)
2. **The summary's words** past two members: "All" and "2 open" - right?
3. **v6** comes next: the solo-card scale and finer sub-units in the Layout Playground, for you to
   try before the arranger changes. Anything you want it to show besides the House page on the
   P4_5 and the 4B?

## Results (mine, before you)

- Each change seen on glass by screenshot on both boards (the pictures above). The summary card's
  words checked on a throwaway build with the doors pointed at virtual lamps; the doors themselves
  only read closed today.
- The slider row joins the kit's chip table: Card UI, UI Tweaks and anything else can use it.

## Your results

(to come)
