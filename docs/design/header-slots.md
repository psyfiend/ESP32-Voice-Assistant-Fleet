# Header slots - milestone 2.8 (#19)

The design interview's §3, answered by the owner on 2026-10-15 from the mock
[2.8 Header Slots](https://claude.ai/artifact/9dLvFUbWZePzgjLnnXZ5Sw) (`interview-phase2-3.md` §3).
This is what 2.8 builds, in the owner's words where they matter, and what is still open. Decisions:
DECISIONS D-11 to D-15.

## 1. The header is optional, so nothing critical lives only there (Q1)

> "In our grid based page view paradigm, I always imagined a clock type of card occupying some space
> on the grid, in our current incarnation perhaps simply a 2x1 in the top left corner. If we had a
> more stylized or specialized page ... then perhaps a clock would be centered? This isn't a
> cellphone, I don't think a clock *needs* to be in the header bar. In fact it may be that a user may
> wish to hide the header completely! For that reason we should try to avoid placing anything that is
> ultra important or critical info in the header bar where it could be ignored or overlooked."

- **The clock is a card first** - on the grid (a 2 x 1 top left today), centred on a specialised
  page. A clock in the header is a convenience beside it.
- **The header may be hidden** - by a page (§4), or everywhere by a user. Anything that matters has
  a home outside it: a card, or the notification queue (§7).

## 2. The page header: four evenly spaced slots (Q2)

> "I like the clock on the left, page title in the middle, status icons on the right. Maybe we could
> break with the status quo and have 4 evenly spaced slots without a bona fide middle? From left to
> right it could go '1:55am | House oO | status icons | climate/secondary'."

- **Four slots, each a quarter of the bar, no true middle**, left to right:
  1. **clock**
  2. **the page** (its name and dots)
  3. **status** (Wi-Fi, the HA / MQTT link - today's glyphs)
  4. **climate / secondary** (outdoor temperature, the weather now, a chosen sensor)
- The device's name leaves the bar: About this board and the system panel show it.
- **OPEN - the glyph style.** The owner: "Definitely want to see the header bar with all 3 glyph
  types (no bg, rounded square bg circle bg)" - #19's open question. Mock first: the four-slot
  header in each of the three, on the P4_5 and at every board's width.
- **OPEN - narrow boards.** At 320 px each quarter is 80 px. Claude's view: the same four slots, each
  in its compact form (the time; the dots alone; the status glyphs; the temperature alone). The mock
  shows it.

## 3. A slot's own panel (Q4)

> "Perhaps any given slot could have its own version of the system panel to slide down with
> additional settings or information. Right now the 'system panel' comes down from the upper right
> corner, but a version of that could come down from each slot location. 'Clock/alarm/time stuff |
> page/card/view settings or page navigation | status/connectivity/wifi settings/details | extra
> climate info, forecast, choose other sensors etc'. ... This sliding panel could contain buttons that
> deeplink straight to a specific settings page which would likely live inside a popup when we get to
> that point."

- **A tap on a slot slides down that slot's panel, from its place:**
  - the clock: time and date, the alarm (K84), timers later;
  - the page: the page's and its cards' view settings, page navigation;
  - status: connectivity, Wi-Fi settings and details - today's system panel becomes this one;
  - climate: more of the weather, the forecast, choosing other sensors.
- A panel can hold **buttons that deep-link to a settings page** (in a popup, when 4.1 #23 builds
  them).
- This replaces the earlier recommendation, "a tap opens the entity's window".

## 4. A hidden header (Q5)

> "Per page, as some pages may be designated for specific areas/groups where the 'regular' header
> bar contents may not apply or be relevant. Or special 'fullscreen' pages that won't have a header
> bar (by default at least)."

- **Shown / hidden is a page setting**; a full-screen page is hidden by default (the Clock page,
  K82). Hidden makes the cards notably larger.
- **OPEN - how a hidden header comes back.** Today: a swipe down from the top edge (Hide Bar's peek).
  The owner asked whether a drag down on a card could show it. Claude's view: pages do not scroll
  up and down, so a downward drag that starts on a card is free to mean "show the header" - but it
  starts as a press on that card, so the two have to be tried on glass together.

## 5. Card headers (Q6)

> "I'm somewhat opposed to the idea of adding another info bit to a card header, we've been so
> careful to leave the header clean and focused; different locations indicate different things, as
> do their colors. For solo 1x1 cards I don't think we should mess with the header formula. Card
> clusters? Yes we can embrace the slot concept for clusters."

- **A solo card's header keeps its formula** (its tag on the left, STALE on the right): no slots.
  `cards.md` §8's "one slot system, three consumers" becomes two: the page header and clusters.
- **A cluster's header takes slots** (2.11): the area or group's name on the left (or centred),
  occupancy and temperature on the right - and light counts, open doors and windows, from HA
  helpers the owner will make.
- Cards stay compact; the "full" style (battery and "last seen" along the bottom) is not coming back
  for now.

## 6. A slot with nothing live (Q7)

> "For sensor data showing an old value is worse than no value, I'd say dashes until live data comes
> in. Perhaps with a way to communicate an error in retrieving data."

- **Dashes until a live value arrives, never an old one** - the same as #84's "no data yet".
- **OPEN - showing an error.** Claude's view: a small mark beside the dashes, and the slot's panel
  saying what went wrong.

## 7. Raised by the answers, not 2.8

- **A notification queue** (owner): NINA queues its toasts so they show in order and can be
  dismissed or cancelled. The fleet wants a queue of notices HA can send (an automation), open at
  any time, some interactive, some kept until dismissed or not dismissable at all - an alarm coming
  up, a door open too long. -> #76.
- **An active card's border** (owner): in place of a card turning all yellow or green, a thicker
  border in a contrasting but complementary colour glowing inward toward the icon - "a slate card
  turning yellow is boring. Obvious, but boring." -> #106.
- **The clock's form** (Q3) was not answered. The owner's own example writes "1:55am". Claude's
  view: 12-hour with am / pm, no seconds in the bar; 24 h and seconds as settings.

## 8. What 2.8 builds, in order

1. **The mock** (show, don't spec): the four-slot header in the three glyph styles, on the P4_5 and
   at every board's width, and a slot's panel open - on the "2.8 Header Slots" canvas.
2. **The slot mechanism, data-driven**: a slot is a spec - its kind (clock, page, status, sensor),
   its entity where it has one, its panel - and a page's header is a list of them plus shown /
   hidden. One component draws it from the kit (`Kit::`, roles in `KitStyles.cpp`); a new kind is a
   new entry, never new layout code. The glyph style a setting with all three, so the owner can
   compare on glass.
3. **Each slot's panel**: the system panel's drawer generalised - it slides from the slot's place;
   today's system panel becomes the status slot's.
4. **The page setting**: header shown / hidden; the reveal from the top edge as today.
5. **Dashes** for no live value, and the error mark.
6. **Cheap to keep**: the clock redraws only when its minute changes, a slot only when its value
   does - one slot's area, never the bar or the screen (#19).

Cluster headers come with 2.11; the notification queue with #76.

## 9. Round 1 on glass, and the owner's reaction (2026-10-15)

Round 1 was built unattended on `feat/2.8-header-slots` (`TEST_2.8.md` there; the mock's round 2 on
the "2.8 Header Slots" canvas): four evenly spaced slots - clock, page, status, the outdoor
temperature - each tap sliding down that slot's panel, the glyph style a setting, a page's header
shown / hidden, dashes for no live value. The owner, having seen it:

> "I caught some of the screens when there were 4 header slots, it was an idea I kinda wanted to see
> but I'm not really crazy about it. For the main system header I'd probably leave the status icons
> on the right, page info in the middle, and put the clock/alarm/temp on the left side."

> "The 'knobs'/buttons in the 'SYSTEM - DIAGNOSTICS' panel are there basically so we can test
> various UI looks/functions while developing and is not intended to be there in the final product.
> Or at least not in its current incarnation. That panel will ultimately provide settings and/or
> shortcuts to settings or popups to manage the various statuses. Same for all of the header slots."

- **Three places** (D-16): **left** - the clock, the alarm, the temperature; **middle** - the page's
  name and dots, in the true middle again; **right** - the status icons. §2's four even slots are
  superseded. What round 1 built underneath stays: a slot is data (`HeaderSlots.h`), a kind is a
  build and an update, each slot has its panel, the glyph style and a page's header are settings.
  Round 2 changes where the slots stand - a place holds one or more slots - not what a slot is.
- **The panels are for settings** (D-17): the system panel's knobs are development tools. Each
  panel - the status slot's included - will hold settings, shortcuts to settings, or popups that
  manage what its slot shows. Round 1's small panels (the clock's time and date, the page's pages
  and Header Shown / Hidden, the sensor's reading) are a start in that direction.
- **Still open** from round 1's sheet: the glyph style (Q1 - Claude's recommendation: square), the
  clock's form (Q2), how a hidden header comes back (Q3), the error mark (Q4), the open slot's look
  (Q5), what each panel holds next (Q6), the system panel onto `Kit::Drawer` (Q7).
