# TEST - 2.8: the header's slots

# Round 2 - three places, on claims (2026-10-15, evening)

Branch `feat/2.8-header-slots`, `main` (`v0.2.17`) merged in, built with you at the desk from your
verdicts on round 1 and the round-2 mock (DECISIONS D-16 to D-20, `docs/design/header-slots.md`
§9-10). **No pull request** - for your test first.

**On the desk now**: the P4_5 and the 4B run round 2 (with your local lines), their settings files
as found; the 1060 runs the `v0.2.17` release.

## What changed since round 1

| Where | Round 1 | Round 2 |
|---|---|---|
| **The header** | four evenly spaced slots | **three places** (D-16, D-18): **left** the clock and the deck's temperature; **middle** the page's name and dots, on the bar's centre; **right** the alarm (while one is set) and the link and Wi-Fi |
| **Where things stand** | each slot a quarter of the bar | **claims** (D-19, your idea): each slot claims the room it needs, measured on the board; when a side would run into the middle, the claim of the lowest priority gives way first - the alarm's time, then the temperature's icon, then the page's name; the clock, the dots and the status glyphs never leave. The middle never moves |
| **The alarm** | "Next alarm: none set" in the clock's panel | **its own slot among the status glyphs** (D-18): a bell and "6:30am"; **on the bar only while an alarm is set** (D-20). Its panel: NEXT ALARM, the time, "in 12 hrs 53 min". Nothing sets an alarm until K84, so for now **a PC sets a pretend one** (RAM only, nothing rings) |
| **The glyph style** | bare by default, a setting | **square by default** (Q1, D-20); the Glyphs button still cycles square, circle, bare (a development tool, D-17) |
| **The system panel** | the old panel, centred under its slot | **the kit's drawer**, the same component as every other slot's panel (Q7, D-20): the same frame, slide and joined look; its buttons and width as before |
| **Under the hood** | slots laid out by flex | a pure planner, `Layout/HeaderPlan` (ten tests on the PC), and the slots as data with a place each (`HeaderSlots.h`) - a notice (#76) will be one more claim, no new layout code |

## The pictures

The P4_5: `v0.2.17`, then round 2 (square), bare, circle, and with an alarm set:

![v0.2.17](followups/28r2_hdr_before_p45.png)
![Round 2, square](followups/28r2_hdr_square_p45.png)
![Bare](followups/28r2_hdr_bare_p45.png)
![Circle](followups/28r2_hdr_circle_p45.png)
![An alarm set](followups/28r2_hdr_alarm_p45.png)

The 4B (720 px), and with an alarm set - everything still in full:

![4B](followups/28r2_hdr_square_4b.png)
![4B, an alarm set](followups/28r2_hdr_alarm_4b.png)

The panels, each joined to its slot - the alarm's (P4_5, then the 4B), the system panel, the
clock's, the temperature's, the page's:

![Alarm](followups/28r2_slot_alarm_p45.png)
![Alarm, 4B](followups/28r2_slot_alarm_4b.png)
![System panel](followups/28r2_status_p45.png)
![Clock](followups/28r2_slot_clock_p45.png)
![Temperature](followups/28r2_slot_sensor_p45.png)
![Page](followups/28r2_slot_page_p45.png)

## Tests for you

To set a pretend alarm from the PC (and clear it):

    curl "http://fleet-ws-p4-5/popup?alarm=0630"
    curl "http://fleet-ws-p4-5/popup?alarm=-1"

| # | Do this | PASS if |
|---|---|---|
| R1 | Look at the House page | the clock and the deck's temperature on the left; "House" and its dots in the middle of the bar; the link and Wi-Fi on the right, each glyph in a rounded square; no bell |
| R2 | Watch the clock across a minute | it turns over on the minute; nothing else in the bar flickers (when the time gains a digit, 9:59 to 10:00, the temperature moves over by that digit - expected) |
| R3 | Tap each slot: clock, temperature, House, the status glyphs | each panel slides down from under its slot, the slot drawn in the panel's colour so the panel pours out of it; the status glyphs' is the system panel |
| R4 | With a panel open: tap its slot again; tap a card; swipe up; swipe sideways | each puts it away (the tap on a card does nothing else) |
| R5 | Set the pretend alarm (above) | a bell and "6:30am" appear left of the status glyphs; the middle does not move |
| R6 | Tap the bell; then clear the alarm from the PC | NEXT ALARM, its time and how long until it; cleared, the slot and its panel leave together |
| R7 | The page panel: Fleet; Header Hidden then Shown | as round 1: the Fleet page; this page's cards grow into the bar's room and come back |
| R8 | The system panel (status glyphs) | the same buttons in the same place; its slide and look match the other panels |
| R9 | Glyphs (system panel), three times | circle, bare, square; the choice survives a reboot (square is stored as nothing) |
| R10 | The schemes (Midnight, Fleet, Linen) | the squares read on each; an open slot still reads as part of its panel |
| R11 | The 4B | as R1-R8 at 720 px |

## Choices that were mine

1. **The keeps** (who gives way first): the alarm's time 10, the temperature's icon 20, the page's
   name 30, the temperature itself 60, a set alarm's bell 200; the clock, the dots and the status
   glyphs never. The order is your mock's; the numbers leave room for #76's notices between them.
   On the P4_5 and the 4B everything fits in full; the planner's tests cover the 320 px board.
2. **Slots are as wide as what they show**, plus a pad each side - no 9 mm minimum (Q9 below). The
   status glyphs stay one slot, one tap target, as in round 1.
3. **The alarm's look**: the outline bell (`bell-outline`, added to the icon font) and its time in
   the clock's form; its panel says how long until it, in TimeFormat's words.
4. **"Next alarm" left the clock's panel** - it is the alarm slot's now.
5. **A pretend alarm** is the only way to see the slot until K84: RAM only, not saved, nothing
   rings; at its time it leaves the bar and its panel goes with it.
6. **The system panel kept its width** (half the P4_5) **and its buttons**; only its frame is new.

## Questions for you

Carried from round 1, with their recommendations: **Q2** the clock's form (keep 12-hour; 24 h and
seconds as settings with 4.1), **Q3** how a hidden header comes back (keep the edge swipe; try a
drag from a card later), **Q4** the error mark (keep it), **Q6** what each panel holds next (the
temperature's "This slot shows" first). New:

- **Q8. When the alarm shows.** Today: whenever one is set. Your message also said "an alarm about
  to go off within a few hours". **Recommended: whenever set for now; "only within N hours" as a
  setting with K84**, when there are real alarms to judge it by.
- **Q9. Tap targets.** Slots are as wide as what they show: on the P4_5 the clock is 94 px (~8 mm),
  the temperature ~10 mm, the status pair ~10.5 mm, and the bar is ~5 mm tall. A 9 mm minimum
  width (`UI::minTouch()`) would space the sides out unevenly. **Recommended: keep, and see how
  they feel to tap.**

## Results

- **The harness**, P4_5, `v0.2.17` against round 2, 55 states each, minutes apart (`r2b`, `r2c`):
  **every window's contents unchanged** - its tree identical but for `desk_temp`'s temperature
  mark, 15 px lower, which is #104 (open, seen before); the only unexplained pixels in a window
  state are the new header above it. The header differs in every page state, as it should. **The
  system panel**: the same place and width (623,35, 640 px); its first row has five buttons where
  `v0.2.17` had four (round 1's Glyphs). LVGL's pool: **+3.4 to +4.8 KB** in every state (the
  header's slots, the alarm's built and hidden, the frames, the slot drawer). The first window
  after the flash took 111 ms, the face painting every boot pays (HANDOFF); the rest within 1 ms.
- **The 4B**: its header states captured (`r2h`) and looked at (pictures above). Its full walk
  stops at the Desk window: the 4B's own setting closes a window after 10 s (`system.window.close`),
  shorter than the walk - left as you set it.
- **Drawing cost**, P4_5, a full redraw of the House page, 20 frames, three runs each:
  `v0.2.17` **103.9 ms** (render 89.4) -> round 2 **104.8 ms** (render 90.5), under 1%.
- **Host tests**: 27, all PASS (the planner's ten new).
- **The structure check** (W13): files over 1,000 lines - `GUIManager.cpp` 1,615 (round 1 1,605:
  the [UI STATE] line and the panel guard), `Card.cpp` 1,462; style calls outside the kit **492**
  (round 1 501: the system panel's frame is the kit's now; `Panel_Header.cpp` 5, the sensor's tint
  and the alarm's circle); colour literals 46 (unchanged); no shape numbers; no LVGL in the core.
- **The gate**: all nine SUCCESS on `418ed0f` (17 min; P4_5 41.3% flash, the S3 boards 34.7-36.8%).

---

# Round 1 (2026-10-15, morning) - superseded by round 2

Branch `feat/2.8-header-slots` (off `main` at `v0.2.16`), built unattended on 2026-10-15 from your
interview answers (`docs/design/header-slots.md`, DECISIONS D-11 to D-15 - both on PR #107, still
open) and the mock's round 2 (the "2.8 Header Slots" canvas: the four slots in the three glyph
styles, at every board's width, and each slot's panel). **No pull request** - for your look first.

**To see it**: the boards are back on `main`'s build. Flash the branch to the P4_5 (and the 4B):
`git switch feat/2.8-header-slots`, then the usual upload. Your settings file is not touched by it
except by what you change on the device (the glyph style, a page's header).

## What changed

| Where | Before | Now |
|---|---|---|
| **The header** | the device's name left, the page's name and dots in the middle, the link and Wi-Fi glyphs right | **four slots, each a quarter of the bar, no true middle** (D-12): the clock, the page (name and dots), status (the link and Wi-Fi, as before), the outdoor temperature (`outside_deck_temp`: its icon in its tint and a whole degree, "51°"). The device's name has left the bar (About this board shows it) |
| **The clock** | - | "6:56am" as you wrote it: 12-hour, am / pm small beside it, no seconds; `--:--` until the time is known; redrawn when the minute changes and only then |
| **No live value** | - | **dashes, never an old value** (D-15): the sensor slot shows `--°` until its sensor reports this boot, and `--°` with a small **"!"** on an amber disc when its source says unavailable or it has gone stale; its panel says which |
| **A tap on a slot** | the status icons opened the system panel from the right edge | **slides down that slot's own panel from its place** (D-13); the slot is drawn open, in the panel's colour, like a folder's tab. Another tap, a tap outside, a swipe up or sideways puts it away. One panel at a time |
| **The status slot's panel** | the system panel, against the right edge | **the same system panel, centred under its slot**; a new **Glyphs** button on its first row |
| **The clock slot's panel** | - | the time large, the date, "Next alarm: none set" (the alarm's place, K84) |
| **The page slot's panel** | - | **PAGES**: House and Fleet, this one marked - a tap goes there; **Header: Shown / Hidden** for this page |
| **The sensor slot's panel** | - | its name, its reading ("51.0°F"), "reported 48s ago" - or dashes and what went wrong |
| **The glyph style** (#19's open question) | bare | **a setting with all three**: bare (the default), a rounded square, a circle in the glyph's own colour with the glyph in the bar's. The **Glyphs** button cycles them, saved (`system.header.glyphs`). Every glyph in the bar takes it - the status glyphs and the sensor's icon; words and numbers never do |
| **Header shown / hidden** | Hide Bar, for every page, not saved | **a page setting** (D-14), saved by the page's slug (`pages.<slug>.header`): the page panel's Shown / Hidden, and **Hide Bar now hides this page's header**. Hidden, a swipe down from the top edge brings it back for three seconds, as before |
| **Narrow boards** | - | the same four slots everywhere; where a slot is too narrow for the page's name and its dots, the name gives way and the dots stay (measured, not by board) |

How it is built (the "bones" you asked about): a slot is **data** - its kind and, where it has one,
its entity (`include/UI/HeaderSlots.h`); a page's header is `PageSpec::headerSlots` (none: the
default four) and `headerShown`; each kind is a build and an update in one table in
`Panel_Header.cpp`, and a panel in `SlotPanels.cpp`. **A new kind of slot is a new row**, never
layout code. The bar, a slot, a glyph's frame and the slot drawer are kit components with their
own roles, their sizes a new token group (`UIHeader`, `UI::hdr()`): `components.md`, `tokens.md` §8.

## The pictures

The header on the P4_5: `v0.2.16`, then bare, square, circle:

![Before](followups/28_hdr_before_p45.png)
![Bare](followups/28_hdr_bare_p45.png)
![Square](followups/28_hdr_square_p45.png)
![Circle](followups/28_hdr_circle_p45.png)

The 4B (720 px): before, bare, square, circle:

![Before, 4B](followups/28_hdr_before_4b.png)
![Bare, 4B](followups/28_hdr_bare_4b.png)
![Square, 4B](followups/28_hdr_square_4b.png)
![Circle, 4B](followups/28_hdr_circle_4b.png)

The status slot's panel - the system panel - before (right edge) and now (under its slot):

![Before](followups/28_status_before_p45.png)
![Now](followups/28_status_after_p45.png)

The clock's, the page's (P4_5, then the 4B) and the sensor's panels:

![Clock](followups/28_slot_clock_p45.png)
![Page](followups/28_slot_page_p45.png)
![Page, 4B](followups/28_slot_page_4b.png)
![Sensor](followups/28_slot_sensor_p45.png)

## How it was checked

- **The harness**, P4_5, `v0.2.16` against the branch (53 states; five new: `header_square`,
  `header_circle`, `slot_clock`, `slot_page`, `slot_sensor`): the header changed in every state, as
  it should - the clock where the device's name was; the system panel moved under its slot and has
  the Glyphs button; **every window's contents unchanged** (the only unexplained pixels in a window
  state are the header's left slot above it). LVGL's pool: about **+2.7 KB** in every state (the
  header's extra objects). The 4B: its header states captured and looked at (pictures above).
- **A page's header hidden** (P4_5, from the PC): the cards grew into the room, the setting was
  saved as `pages.house.header = hidden`; shown again, the key was removed - the file is as it was.
- **The glyph styles** on both boards, all three, and back to bare (shown from the PC, never saved).
- **Drawing cost**, P4_5, a full redraw of the House page (`/bench what=full`, 20 frames): **102.9
  ms** on the branch (render 88.3); `main` - see Results. The clock redraws once a minute and the
  sensor only when its whole degree changes: [UI STATE] counts each slot's redraws (`Header: clock
  N | page N | status 0 | sensor N`).
- **Compiles**: all nine (see Results). **Not on glass**: the S3 boards (none can be flashed), the
  narrow forms (the 3248's USB is broken, the 4880 was not flashed), the Linen and Fleet schemes
  beyond the first look, and the alarm (nothing sets one yet).

## Tests for you

| # | Do this | PASS if |
|---|---|---|
| T1 | Look at the House page | four slots, evenly spaced: "h:mmam", "House" and its dots, the link and Wi-Fi, a thermometer and the deck's temperature in whole degrees; the device's name gone |
| T2 | Watch the clock across a minute | it turns over on the minute; nothing else in the bar flickers |
| T3 | Tap each slot in turn | each slides down its own panel from under it, the slot drawn open; the status slot's is the system panel, centred under it |
| T4 | With a panel open: tap the same slot; tap a card; swipe up; swipe sideways | each puts it away (the tap on a card does nothing else) |
| T5 | Page panel: tap Fleet | the Fleet page, the panel gone |
| T6 | Page panel: Header > Hidden, on one page | that page's cards grow into the bar's room; the other page keeps its header; a reboot keeps both; Shown brings it back |
| T7 | Hide Bar (system panel) | hides this page's header, as T6 |
| T8 | Glyphs (system panel), three times | square, circle, bare - all four glyphs framed each time; the choice survives a reboot |
| T9 | The schemes (Midnight, Fleet, Linen) with each glyph style | the frames and circles read on each; a circle's glyph is the bar's colour |
| T10 | Unplug the deck's sensor, or wait for a stale reading, if convenient | `--°` and the "!" mark; its panel says what went wrong |
| T11 | The 4B | as T1-T8 at 720 px |

## Choices that were mine (the documents left them open)

1. **Bare is the default glyph style** - the header as it was - and the other two are a button press
   away. The style is one setting for every glyph in the bar.
2. **The sensor slot shows `outside_deck_temp`** (the House page's "Deck") as a whole degree with
   its card's icon and tint. A board without that entity shows `--°`.
3. **Stale counts as no live value**: a reading past its entity's stale time shows dashes and the
   mark, as unavailable does (your "an old value is worse than no value").
4. **The error mark**: "!" on a small amber disc beside the dashes - a shape and a colour.
5. **The open slot is drawn in the panel's colour**, so slot and panel read as one piece (the
   folder-tab idea from the card window). The status slot's panel kept its width; the others are
   narrower (at most 260 logical px).
6. **Hide Bar is now this page's setting**, saved, matching D-14 (until now it was every page's and
   forgotten at a reboot).
7. **The Glyphs button** sits on the system panel's first row (Log / Tokens / Cards / About /
   Glyphs) until the settings pages (4.1).
8. **The page panel's rows are a finger tall** (9 mm), so two pages already make it tall.
9. **The clock** is 12-hour with a small am / pm and no seconds; 24 hours and seconds are not built.
10. **Narrow boards** keep all four slots; only the page's name gives way, measured. The weather
    icon from the mock is not built: the slot shows its sensor's own icon.

## Questions for you

- **Q1. The glyph style** (#19): bare, square or circle, now that all three are on glass?
  **Recommended: square** - it groups the status glyphs and the sensor's icon without the circle's
  loudness, and a state change still shows in the glyph's own colour. The circle makes a fault
  hardest to miss; bare is quietest.
- **Q2. The clock's form**: "6:56am" as built - or 24 hours, or seconds, as settings? **Recommended:
  keep 12-hour; 24 h and seconds as settings with 4.1.**
- **Q3. How a hidden header comes back**: a swipe down from the top edge, as Hide Bar's always did -
  or also a drag down that starts on a card (header-slots.md §4)? **Recommended: keep the edge
  swipe; try the card drag on glass in round 2** - it starts as a press on a card, so the two need
  feeling together.
- **Q4. The error mark** (D-15's "a way to communicate an error"): "!" on amber beside the dashes.
  Enough, or too loud for a slot you may glance at all day? **Recommended: keep it.**
- **Q5. The open slot's look** - the slot in the panel's colour. Does it read as "this panel came
  from here", or would you rather the slot stay as it is? **Recommended: keep.**
- **Q6. What the slots' panels should hold next** (each panel's last line says what it could):
  the clock - alarms and timers with the Clock page (K84); the page - its cards' size and layout
  (2.11); the sensor - the forecast and choosing the sensor. **Recommended: the sensor's "choose
  which one" first** - it is the slot's only setting today, and it is in the code as data already.
- **Q7. The system panel's own look**: it is still the old panel (not the kit), so it differs a
  little from the three new panels (its title row, its buttons). **Recommended: move it onto
  `Kit::Drawer` in round 2**, so all four panels are one component.

## Results

- **The gate**: all nine SUCCESS on `3260eb0` (15:49) and again on the final commit `a04711b`
  (10:28; P4_5 41.0% flash, the S3 boards 34.5-36.6%).
- **Drawing cost**, P4_5, a full redraw of the House page, 20 frames: `main` **101.8 ms** (render
  87.0) -> the branch **102.9 ms** (render 88.3), about 1%. LVGL's pool on the House page: 4.4 KB
  more in use (the four slots, the frames and the slot drawer, built once).
- **The structure check** (W13): files over 1,000 lines - `GUIManager.cpp` 1,551 -> 1,605 (2.8's
  slot code is its own file, `GUIManagerHeader.cpp`, 141), `Card.cpp` 1,462; style calls outside
  the kit 523 -> 501 (the header moved onto the kit; `Panel_Header.cpp` keeps 3, the sensor's tint);
  colour literals 44 -> 46 (the two status glyphs' "off" colour, now also used for their strike in
  the bare and square styles); no shape numbers; no LVGL in the core layers.
- **The boards are back on `main`** (`v0.2.16` with your local lines): the P4_5 and the 4B, each with
  its settings file byte-identical to its copy from this morning (05:42); the 1060 was never
  flashed.

## Your results

**Round 1 (owner, 2026-10-15)** - seen on glass, not tested against the table above:

> "I caught some of the screens when there were 4 header slots, it was an idea I kinda wanted to see
> but I'm not really crazy about it. For the main system header I'd probably leave the status icons
> on the right, page info in the middle, and put the clock/alarm/temp on the left side."

> "The 'knobs'/buttons in the 'SYSTEM - DIAGNOSTICS' panel are there basically so we can test
> various UI looks/functions while developing and is not intended to be there in the final product.
> Or at least not in its current incarnation. That panel will ultimately provide settings and/or
> shortcuts to settings or popups to manage the various statuses. Same for all of the header slots."

Recorded as DECISIONS D-16 (three places: clock, alarm and temperature left; the page middle; the
status icons right - D-12 superseded) and D-17 (the panels are for settings; the knobs are
development tools); `header-slots.md` §9. Q1-Q7 stay open for round 2.

## Round 2

Built 2026-10-15 evening - at the top of this sheet.
