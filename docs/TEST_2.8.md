# TEST - 2.8: the header's slots (round 1)

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

*(yours)*
