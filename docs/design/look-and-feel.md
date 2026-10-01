# Look and feel - the whole product

**Status: interview §1 in progress, opened 2026-09-30.** Answers from `interview-phase2-3.md` §1
land here, dated, with the decision and its reason. Anything still open is marked **OPEN** with what
would settle it. Every later design choice (popups, header, group cards, the build sheet's theme
section) is tested against this file.

---

## 1. Already on record - brought in, not re-asked

### 1.1 Owner's notes from glass, 2026-09-29 (interview §1 item 10)

- **Linen:** find a better card fill colour.
- **Wi-Fi header icon:** more visible - a box or a background behind it?
- **Chrome text too small:** header device name and page title, deck panel headers, the system
  panel, the card's area tag. Cause audited: the chrome faces are two fixed sets chosen by
  `bspUiScale() >= 1.3` (`UIToolkit.cpp:40`), not derived from PPI. Proposal: mm targets in
  `gen_type_scale.py`. Tracked as **#73**.
- **Hero value size vs card size:** 4B - 4x4 a bit too big, 3x3 could be larger, 3x4 / 4x3 good.
  7" - 6x3 "naked", 7x3 fine, 8x3 -> 8x4 fits. 4880 landscape - 4x3 excellent, 5x3 too big in
  places, 4x4 with header and tag not feasible. Plus: side-by-side cards pick different value faces,
  and the corner icon is not in the fit. **#73**.
- **Rotation as a setting** -> interview §5 / **#71**.

### 1.2 Owner, 2026-09-30: the grid is not the goal

In the owner's words, paraphrased from the session prompt: the owner goes back and forth on whether
the project is getting too locked into a rigid grid, and not using more dynamic, fluid shapes and
structures to show data from HA, and is open to new ideas for presenting and interacting with
**clusters** of information - rooms, functions, other logical smart-home groups. The current "simple
grid of cards" is **not** to be treated as the final or only goal.

Claude's note on what this touches: ROADMAP Q3b (unit-granular placement, free positioning
rejected) and the derived grid in `tokens.md` are both grid-shaped; 2.11's area cards are the first
cluster. Raised as a new §1 question (item 11) so it is settled before 2.11, not discovered during it.

### 1.3 Decided elsewhere, so §1 does not ask

- **Stale / failed look** (item 8, in part): dimming rejected; a STALE tag, escalating; a failed
  command jumps straight to the loud state (`cards.md` §3). Item 8 asks only what annoys him now.
- **Icons** (item 6): MDI is live fleet-wide (2.4). Item 6 asks only for dislikes.

## 2. The owner's two standing rules (2026-10-01)

Stated by the owner while answering batch 1, to be held against everything in the interview:

1. **"Most if not all of the cards will be coming from HA and I'd rather not sacrifice any
   functionality HA can offer."**
2. **"I'm a sucker for eye candy. I will be sad if the best we can do for a dashboard is a simple
   grid of uniformly sized squares."**

**Claude's reading of rule 1, proposed - OPEN until the owner confirms:** every HA function of an
entity we show is *reachable* on the board - on the card's face, in its popup, or as a service
call - in domain priority order (lights, switches, covers, climate, sensors, weather, media,
scenes/scripts). Some HA features cannot work at all inside 128 KB of `lv_mem` and an 8 KB
websocket buffer (G1, G5): live camera video, maps, media artwork are the likely ones. Each is
measured before being ruled out, never ruled out by assumption.

## 3. Answers

### 3.1 Who looks, from where (item 1) - ANSWERED 2026-10-01

**Mostly the owner, at arm's length: glance, tap, done.** Devices in common areas must read from
2-3 m. The planned boards and their jobs:

| Where | Job | Distance |
|---|---|---|
| Owner's nightstand | clock / alarm first (#35), dashboard second | arm's length, in the dark |
| Kitchen | a prominent weather station, plus other cards | 2-3 m |
| Garage | garage cards | 2-3 m |
| Dad's desk | light use | arm's length |
| Irrigation (next summer) | a dedicated irrigation interface - "a whole other subject" | arm's length |

**What follows (Claude):** a board has a **role**, and the role decides more than which cards it
shows. Proposed: a per-device **viewing distance** setting (arm's length / across the room) that
scales the hero value and state on top of the PPI-derived type scale (#73). No board-specific
numbers; the setting picks a millimetre target. OPEN - asked in batch 2.

### 3.2 The feel (item 2) - ANSWERED 2026-10-01, three words OPEN

The owner declined to pick three words ("both", "dynamic", "optional") and gave the rule instead:

- **At its smallest, a card does one thing extremely well: it advertises its state or its
  intended function.**
- Above that, **small touches that make it feel premium and thought through**: small weather
  animations on a weather card; a dedicated **clock card** ("an obvious oversight thus far") that
  can *flip* as the time changes, with several clock faces; transitions; **drag-and-drop layout
  editing**. "Not excess animations just for the sake of it."

**Claude's test for every later choice, proposed:** (1) at minimum size, state or function is
unmistakable; (2) an effect is allowed when it is tied to something real - time passing, weather,
an action the user took - never idle decoration; (3) every effect can be turned off per device.
Three words proposed: **clear, alive, crafted.**

### 3.3 Motion (item 3) - ANSWERED 2026-10-01

**Design for the P4s; cut effects later where they lag the S3s.** Wanted: motion tied to things
happening - menus opening, **page swipes**, long press opening card details. Not wanted: gratuitous
animation, "fluid dynamics".

**What it changes:** `pages.md` §7 already chose **slide via snapshot** as the page transition (two
live pages do not fit `lv_mem`), and the snapshot capability now exists (#58). Interview §6 item 2's
recommendation moves from "instant" to "slide on the P4s". Known cost: a page rebuild is ~300-400
ms; a swipe that follows the finger needs a cached picture of the neighbouring page (also what the
overview needs, `pages.md` §6). Popup grow-from-card (`card-sheet.md` D1) is in line with this answer.

### 3.4 Night (item 4) - ANSWERED 2026-10-01

**An absolute must-have for any bedroom screen.**

- **Light:** the owner will fit light sensors in some enclosures / stands, or place one elsewhere in
  the room and tell the board to **follow it** - so the board reads light from a local sensor *or*
  from any HA lux entity.
- **Minimum:** customizable **night hours**. The screen goes black and shows **a clock, or
  nothing**; **a tap brings the dashboard back** (and does not also act on what is under the
  finger - Claude's recommendation, consistent with the answer).
- **Alerts from HA**, customizable, optionally shown on the night screen. The owner has asked for
  HA alerts on the devices before; this is the first place they are designed.

**What follows (Claude):** night hours need wall-clock time, so **#74 (SNTP) becomes a prerequisite
of night mode** - recommend pulling it ahead of 2.10. HA alerts have no issue yet - to be opened
at the end of the section.

### 3.5 Reference images (item 9, in part) - 2026-10-01

The owner sent four screens of the **irrigation dashboard in HA** (Zones & Scheduling; Cycle
Settings and Zone Settings dialogs; Irrigation System with Activity and System Event Logs). What
they show the owner likes:

- **Grouped sections**, each a rounded panel with a title, a subtitle, and a status pill top right
  ("System Online").
- **Tables inside a card**: rows of zones with AM / PM / status columns, and a **day-of-week row**
  with today highlighted - a dense, glanceable schedule.
- **Dialogs as tall, sectioned sheets**: centred section headings with an italic one-line
  explanation, big stepper time pickers, sliders with the value on the right, an info callout.
- **An event log as a timeline**: a dot per event on a vertical line, the entity name bold, an
  `area > device` breadcrumb under it, state and time on the right. `card-sheet.md`'s breadcrumb and
  Activity tab already match this.

### 3.6 Card sizes (raised by the owner, 2026-10-01) - folded into item 11

**Owner:** resizing individual cards has not been discussed directly. Cards default to compact
because it is neater, but a 1x1 card that becomes **2x1 or 2x2** (HA's own cards are mostly
horizontal) spends a much smaller share of itself on the secondary line, and **a light card could
carry a dimmer slider or other data right on its face**. Units already exist, so an irregular
shape can simply reserve the cells / units it needs.

Claude agrees, and notes that `cards.md` ("compact and full are derived, not declared") already
makes a card draw *more* when its cell has room - so a bigger span unlocking more content is the
existing rule extended, not a new one. To be mocked with item 11.

### 3.7 Beyond the grid (item 11) - mock shown 2026-10-01, OPEN

Mock: artifact **Beyond the Grid** (https://claude.ai/artifact/WuUmbnVWwBbxn4eCghrnXz) - the
House page on `WS_P4_5` four ways: **A** today's 5x3; **B** mixed sizes on the unit grid (2x2 clock
+ weather, 2x1 lights with a slider on the face, 2x1 temperature with a 24 h line); **C** room
clusters (a header with promoted values, members as tiles or rows; a plain card as a cluster of
one); **D** fluid shapes (a sky panel driven by `sun.sun`, a temperature dial with glowing light
orbs, an L-shaped kitchen, garage doors drawn as doors). Plus one light and one sensor at 1x1 /
2x1 / 2x2. Claude's framing: these are rungs of one ladder that can mix on a page, not rivals.
