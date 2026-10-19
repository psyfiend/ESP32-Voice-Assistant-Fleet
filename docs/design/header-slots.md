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
  turning yellow is boring. Obvious, but boring." -> #106, **built in 2.8 rounds 4-7** as the
  cards' active frames (Corner cut the default; D-30 to D-33) and closed.
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

Round 1 was built unattended on `feat/2.8-header-slots` (`archive/TEST_2.8.md` there; the mock's round 2 on
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
- **The round-2 mock** (the "2.8 Header Slots" canvas, three places at the P4_5 in the three glyph
  styles, the narrowest board, the 4B, a slot's panel open). The owner, on it:

  > "I like how the header panel opens looking like it is 'connected' to the header, as if it pours
  > downward out into the panel. I'd say move the alarm icon to the status icons (even though it is
  > a clock function) and leave the weather with the clock."

  So (D-18): **left - the clock and the weather**; **middle - the page**; **right - the status
  icons, the alarm among them**. **Keep the open slot joined to its panel** - the slot drawn in the
  panel's face, the panel square where it meets the bar - so a panel pours down out of its slot.
- **Still open** from round 1's sheet: the glyph style (Q1 - Claude's recommendation: square), the
  clock's form (Q2), how a hidden header comes back (Q3), the error mark (Q4), the open slot's look
  (Q5), what each panel holds next (Q6), the system panel onto `Kit::Drawer` (Q7).

## 10. Claims: what shows is decided by priority (2026-10-15, before round 2's build)

Asked how a page's header should name where its slots go, the owner answered with a model of the
whole header (DECISIONS D-19):

> "Some kind of priority system where multiple entities or events can make simultaneous claims to a
> given slot and whether they are displayed or not is decided by their priority. For example the
> 'alarm' icon could have a static presence in the 'status icons' slot but could be elevated to a
> priority where it would appear if certain conditions are met. Or a popup window could temporarily
> bump the middle slot to show relevant info while the popup is on screen. For alerts there could be
> a 'detailed' and 'minimal' way to represent information, so that a 'detailed' version such as
> 'ALERT - Front door open for 2 hours' could temporarily occupy the middle slot for 30 seconds and
> draw the users attention, then the 'minimal' version could be reduced to an exclamation mark icon
> and reside in the status bar slot with a higher priority until dismissed or resolved. Perhaps the
> status icons slot could be designed to have a certain number of low-pri icons always present
> (wifi/mqtt for example) but enable a 3rd slot if it is a high-pri status like an exclamation mark,
> or an alarm about to go off within a few hours."

> "Each slot would have default or baseline residents, and other entities would be more or less
> 'permanently' attached to certain slots but only show under certain circumstances. And of course
> additional entities or events down the road could make a claim to any slot as needed."

> "The NINA project has an advanced 'toast' system with a buffer and priority levels ... I'd like to
> import/port over the framework of how that toast system works, perhaps it could function in
> parallel with a dynamic header system like I've described to temporarily put text or data into
> the header. Such alerts or dynamic events could be actionable such that if an event added an
> alert/notification icon to a header slot it could also add some kind of interactive
> button/text/info into the panel that opens when the slot is tapped."

**Words.** The owner's "slot" in these quotes is what this document calls a **place** - left, middle,
right. A **slot** is one thing standing in a place (the clock, the alarm, the status glyphs); each
slot makes a **claim** on its place.

- **Residents and claims.** A place has residents that are always there (the clock; the page; the
  link and Wi-Fi) and claims that show only while their condition holds (the alarm, while one is
  set). Both are data - `HeaderSlots.h`, one list of `{kind, place, entity}`.
- **Priority decides, and room.** Each claim holds its room with a priority. When a side would run
  into the middle, the lowest priority gives way first - first a slot's extras (the alarm's time, the
  temperature's icon, the page's name: round 2's mock's order), then a whole claim. Residents never
  leave. **This is also how a narrow board sheds**: one rule, measured on the board.
- **The middle never moves**: it stands on the bar's centre; the sides are pinned to the edges.
- **A pure planner** (`include/Layout/HeaderPlan.h`, like `Layout::`): given the claims, each form's
  measured width and its priority, it returns which show, in which form, and where. No LVGL - tested
  on the PC (`host-tests/test_header_plan`).
- **Each kind keeps its build, its update and its panel**; a tap on a slot opens that slot's panel
  joined to it (D-18).

**With #76, later**: the notice queue (NINA's `nina_toast`: a queue any task can post to, drained on
the UI thread, a few visible and a backlog, severity setting look and lifetime, repeats counted,
tap to dismiss - its mechanism ported, our tokens for its looks, non-commercial with credit);
**detailed and minimal forms** (a notice's sentence in the middle for a while, then its "!" in
status); **a place that grows** for a high-priority claim; **a window taking the middle** while it
is open; **claims bringing rows to their panel** (acknowledge, snooze). Each is a new claim source;
none is new layout code. NINA's `nina_nav_arbiter` (sources submit claims, one resolve per cycle) is
the same shape for page changes (`REFERENCE_PROJECTS.md`).

**Round 2's answers** (D-20): **square glyphs** (Q1); **the alarm leaves until one is set** - an unset
alarm is not missing data - and is shown from the PC until K84 builds alarms; **the system panel
moves onto `Kit::Drawer`** (Q7). Still open, carried on the sheet with their recommendations: the
clock's form (Q2), how a hidden header comes back (Q3), the error mark (Q4), what each panel holds
next (Q6).

## 11. Round 2 on glass (2026-10-16)

Every round-2 test passed; the owner's results and words are in `archive/TEST_2.8.md`. What changes next
(DECISIONS D-21 to D-24):

- **The alarm by the clock** (D-21): the bell beside the clock while an alarm is set (within about
  7 days); the clock's panel is where alarms are set and seen; **within 12 hours a notice glyph
  joins the status icons**, dismissable each time or turned off, its 12 hours the owner's setting.
  Two claims from one alarm - the claim model at work.
- **The system panel and UI Tweaks** (D-22): the panel becomes CONNECTIVITY then SYSTEM -
  DIAGNOSTICS; the knobs move to a UI Tweaks window; every header panel follows the window timeout;
  the left-half swipe opens UI Tweaks. The page panel's Shown / Hidden becomes a selector bar.
- **One shape** (D-23): an open slot and its panel inside one border and one colour, the panel
  breaking through the bar's accent line, every corner rounded.
- **The glyph overhaul** (D-24): the mocks' glyphs exactly, never shrunk to fit a frame, centred,
  readable on Linen; an AP mode its own glyph. The frame style (Q1) waits for it.
- **The header's type** one step up - between today's face and the cards' entity label.
- **Kept**: Q2 the clock as built, Q3 the edge swipe, Q4 the error mark, Q9 slots as wide as what
  they show; T4's swipe up to close ("exactly what I would have wanted").

## 12. Round 3, as built (2026-10-16 night)

Settled on the canvas first (D-25 to D-28), then built unattended; `archive/TEST_2.8.md` round 3 has the
pictures, the tests and the choices made along the way (D-29).

- **The glyphs are numbers** (`UI/GlyphShapes`): each stroke of the mocks' glyphs on a 24-unit
  grid, drawn with LVGL's arcs and lines (`Kit::strokeGlyph`), one size bare or framed, coloured
  from the scheme's `SIG_*`. The AP mast is its own claim (`SLOT_AP`, keep 220) beside the status
  glyphs and opens the system panel as they do.
- **One shape**: the open tab (`KS_HDR_SLOT_OPEN`, an accent border left, top and right), the
  panel's flares and the tab's coves, `JOIN_R` round (`components.md`, "One shape").
- **The alarm is two claims from one alarm**: the clock's slot carries a bell as an extra
  (`extraOn`: switched off, it takes no room) within about 7 days; the notice (`SLOT_ALARM`, keep
  210) within `Alarms::warnHours()` - 12 by default, saved (`system.alarm.warn`), set from the
  clock's panel, dismissable (`Alarms::dismissWarning()`).
- **The system panel**: CONNECTIVITY (status rows: a glyph, a name, the state in words, refreshed
  once a second while open) and SYSTEM - DIAGNOSTICS. **The knobs are UI Tweaks** - a Sheet
  client; its choices device-wide and saved; the deck rises under it (`Kit::sheetBeside()`).
- **Every header panel closes on the window timeout** (`Kit::windowCloseMs()`), counted from the
  last touch anywhere.

## 13. Round 4, from round 3's test (2026-10-16 afternoon)

Every round-3 test passed but the deck's colour under UI Tweaks; the owner's list is in
`archive/TEST_2.8.md` (D-30). For the header: **a panel's side walls rise straight to the blue line** (the
flares are gone; the coves at the slot stay), and **the slot stays open until its panel has
finished closing** (`Kit::Drawer::onClosed`). **The header is always visible while a header panel
is open**: the page panel's Show / Hide waits for the panels to close (`GUIManager::
headerPanelsClosed()`); a peek re-arms while one is open. The system panel lost its Status bar
(one way to hide the header). The P4_5's words are 26 (a `HEADER` type role).

## 14. Rounds 5-7, and the merge (2026-10-17)

Round 6 kept **the bar's drop shadow off an open panel**: the bar is drawn over the panels (it
covers their top edge), so on Linen its shadow lay across the top of an open one, under its slot;
the bar now paints each open panel's face back over it (`Kit::drawerCoverBarShadow()`, in the bar's
`DRAW_MAIN_END` - LVGL 9.5 clips `DRAW_POST` to the object). The rest of rounds 5-7 was the cards
(D-31 to D-33: chip tables, the active frames, Linen's terracotta). Signed off 2026-10-17 and merged
as `v0.2.18`; the sheet is `archive/TEST_2.8.md`.

## 15. After 2.8: the panel lies on the bar (2026-10-19, D-35; built in 2.11 round 1)

The owner, on the joined look: the rounded corners where the panel rejoined the bar's line did not
please him, and his answer was the other way round -

> "Place the rounded corners on the inner corners of the panel at the top ... and then ... insert a
> very small horizontal 'fade-to-black' section so that the header bar accent lines outside the
> panel don't actually contact the panel ... This will drive home the illusion that the panel has
> opened up on top of the header bar. Actually ... can we extend this border all the way around the
> opened panel? So that it appears to be floating disconnected above whatever is underneath it?"

Mocked on the "Panel Edges" artifact (`https://claude.ai/artifact/U8wGGgPEWHpgKNvZ2miYMu`), and
chosen there ("loved it"): **Edges + halo** - the slot and its panel stay one outline (D-23), the
panel's top corners rounded toward its middle; the bar's accent line **fades to clear over 2.5 mm**
and stops short of the walls; a **halo 5 mm wide at full strength** all round the slot and panel,
black against them fading out. **Every window gets the same halo** (the card window, About, UI
Tweaks); the dim stays out - its cost was measured in 2.10a (one full-screen redraw to open and one
to close: P4_5 ~85 ms, 4B ~52 ms). To build: LVGL's own shadow (spread 0) on the panel and on the
Sheet, measured with `/bench` against today; if a window-sized shadow costs too much (Linen's
shadows cost +35%, `display-stack.md` §8.2), the ring becomes painted images, as the corner cut's
shadow did (D-33).
