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
  command jumps straight to the loud state (`cards.md` §3). Item 8 asks only what annoys the owner now.
- **Icons** (item 6): MDI is live fleet-wide (2.4). Item 6 asks only for dislikes.

## 2. The owner's two standing rules (2026-10-01)

Stated by the owner while answering batch 1, to be held against everything in the interview:

1. **"Most if not all of the cards will be coming from HA and I'd rather not sacrifice any
   functionality HA can offer."**
2. **"I'm a sucker for eye candy. I will be sad if the best we can do for a dashboard is a simple
   grid of uniformly sized squares."**

**Rule 1, as agreed 2026-10-01:** every HA function of an entity we show is *reachable* on the
board - on the card's face, in its popup, or as a service call - in domain priority order (lights,
switches, covers, climate, sensors, weather, media, scenes/scripts). Anything ruled out is measured
first, never ruled out by assumption.

- **Ruled out, owner agrees:** live camera video (snapshots on events are acceptable; no cameras are
  installed yet) and maps.
- **Album art is IN - Claude retracted it.** Claude had listed it as unlikely; that was wrong. Art
  comes from HA's media proxy over plain HTTP (not the websocket, so the 8 KB buffer is not the
  limit), is decoded into PSRAM (the P4 has a hardware JPEG decoder; the S3s decode in software),
  and a 300x300 picture is ~180 KB of PSRAM, not `lv_mem`. To be measured when the media card is
  built.
- **A media player is wanted** (owner): through Music Assistant, Sendspin, or one-offs for Spotify
  (NINA has a Spotify page - `reference/`) or SoundCloud. New card / page kind; see §12 of the
  interview.

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
Three words proposed: **clear, alive, crafted** (not objected to, 2026-10-01).

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

**Owner, 2026-10-01: "your mockup is KILLING IT".** Answers:

- **11a - B and C are the base.** D has interesting ideas; the specific layouts these responsive
  cards could have are to be discussed separately before any is built.
- **11b - who sets a card's size:** see §3.10 (authority) and §3.8 (auto layout). The owner reads
  it as "who has the final say".
- **11c - option 1: a slider on a card face takes the drag only when the finger starts on it**;
  swipes anywhere else change the page. Horizontal is preferred over vertical for most card faces.
  **Popups may use vertical sliders**, as HA's details dialog does for brightness and colour
  temperature (`card-sheet.md` §3 already puts a big vertical slider on the light's Control tab).
- **11d - rectangles.** "I honestly can't conceive of any reasons why an L shaped cluster would be
  REQUIRED. Chosen, perhaps." L-shapes are deferred, not rejected. The real question the owner sees
  is how members are arranged inside a rectangle - §3.8.

### 3.8 Auto layout and dense clusters (owner, 2026-10-01) - design direction, OPEN items below

The owner's thinking, close to the owner's own words:

1. **A cluster does not cost the cells its members would cost alone.** Inside a shared border the
   members lose their own borders and sit closer together, on the cluster's own **internal grid**:
   4 standalone cards fill 2x2 cells, but a cluster in the same 2x2 cells might hold a 3x3 grid of
   9 members. The tightest fit is to be found by test (perhaps 4 members across 3 cells, not 3
   across 2). The cluster's **header** holds secondary entities (temperature, occupancy, lux) that
   would otherwise each take a whole 1x1 cell - so clusters can be both more logical *and* denser.
2. **Or more spacious:** a page for one room with fewer entities than cells keeps them in one
   shared border, centred, with big margins.
3. **Empty cells, when a page has too few cards:** cards grow, but only to a **predefined maximum
   size**; past that they spread evenly over the grid with spacing and margins (as the HA swipe-view
   mods the owner has used do). Four cards stretched to quarters of a 7" screen "will probably look
   ridiculous". Which kinds grow, and in what priority - perhaps user-settable.
4. **Auto-arrangement prefers proportional shapes.** In landscape, a 3x2 cluster over a 6x1; two
   2x3 clusters look better than two 6x1. An odd count (5 members) raises: is 5x1 the only shape?
   Where does the empty slot go, and why there?
5. **The user can still demand a shape** (a 6x1 cluster), and the dashboard then rearranges the
   other clusters and their shapes to fit.
6. **Drag-and-drop of entities and of clusters** - "would absolutely love to see".

**Claude, on what this means:**

- **It reopens ROADMAP Q3b.** Q3b chose explicit placement plus a validator and rejected "a
  constraint solver" because a layout that moves things by itself is hard to predict ("why did that
  card move?"). Points 3-5 ask for exactly that kind of automatic arrangement. Proposed resolution:
  **auto layout is the default; any card or cluster can be pinned** to a position and shape, and
  pins are what Q3b's validator checks. The packer is **deterministic** - the same cards in the same
  order on the same screen always give the same layout - so "why did it move?" always has the answer
  "because something on the page changed". Drag-and-drop then means "change the order or set a pin
  and re-run the packer". OPEN.
- **The search is small.** A page holds roughly 3-12 blocks, each with a handful of candidate shapes,
  so the packer can try every combination and score each (closest to square, fewest empty units,
  reading order kept). To be prototyped in the browser first.
- **The internal grid is derived, never declared** - the tokens rule. The smallest member tile is
  `UI::minTouch()` (9 mm) plus its label, so the same cluster holds more members on the 7" (170 PPI,
  60 px minimum) than on the P4_5 (294 PPI, 104 px).
- **Odd member counts - Claude's recommendation:** no hole. The cluster's primary member (the first,
  or one marked primary) takes two slots, so 5 members fill a 3x2 with one wide tile. OPEN.
- **Growing cards** needs a **max span** per card kind beside ROADMAP Q4's `preferred_span` /
  `min_span` / `priority`; a light can grow to its 2x2 face, a garage door perhaps to 2x1.
  *Superseded by Q-L3 below: single cards do not change span.*

**Answers, 2026-10-01 (batch 3, Q-L1 to Q-L3):**

- **Q-L1 - the owner's scenario, and the direction it sets.** First boot; the user opens the web
  UI, connects HA, sees the recommended grid (6x3 on the 7B), picks 18 entities, groups them into
  clusters by hand or turns on **"organize clusters by area"**, and saves. The owner's questions:
  do we know how many cells that takes before saving, and do we show it? Can the best shape for
  each cluster on its own stop the others fitting - so that the most entities per page needs an
  odd shape like 6x1? And: Q3b's "why did that card move?" was really about **page overflow**
  pushing cards onto a next page nobody can predict. The owner's priority: **no dead space** - "less
  'oh no I put too many cards on the page' and more 'I need to find an entity to fill this
  space!'" - so a fill-the-space arranger beats meticulous hand curation that any change breaks.
  **Direction agreed:** an arranger fills the page; it searches the whole page at once, so shapes
  are chosen together, not per cluster (yes, an odd shape sometimes wins - the playground shows
  when); the **web UI previews the result before saving**; **overflow is reported, never spilled to
  another page** (Q3b's "reports, never resolves" survives); pins remain for the user who wants a
  6x1. Prototype: artifact **Layout Playground** (https://claude.ai/artifact/SBSanCnwviNbKzQmkwiJXp).
- **Q-L2 - odd member counts: DECIDED.** The short row's members stretch to share the row (5
  members in a 3-wide cluster: the 2 in the short row take 1.5 slots each). The user picks whether
  the short row is at the **top or bottom**. Drag-and-drop within a cluster: a dropped member
  pushes the others along, wrapping to the next row. (Built that way in the playground.)
- **Q-L3 - DECIDED: single cards never auto-span or change size class.** "EVERY card and cluster
  should scale evenly to fill the space", as cards do today when rows and columns change - within
  reason. So the arranger fills a page by choosing the **grid size** (bigger cells), within a
  maximum cell size. OPEN: the playground also lets a *cluster* absorb leftover neighbouring cells
  (cards never do) - the owner to say whether that reads as "filling" or as a cluster changing
  shape on its own.

### 3.9 Page transitions from pre-rendered pictures (owner, 2026-10-01) - feasible, to measure

**Owner's idea:** while the board is idle (no taps for a while), build each page out of sight and
take its picture, on a schedule, so a fresh picture of every page is always ready for a transition.
Transitions wanted: **slide, dissolve / fade, shrink / grow, wipe**.

**Claude:** this fits `pages.md` §7 and §6 (the overview needs the same pictures). What it costs:

- **Time:** a card builds in ~7.5 ms plus ~8 ms of layout (`display-stack.md`), so a 15-card page is
  ~250 ms during which `loop()` is busy. Done only after an idle period, and abandoned if a touch
  arrives, so a tap is never kept waiting more than one card's build.
- **`lv_mem`:** the hidden page and the visible one exist at the same time. Two numbers on record
  disagree - ~715 B a card (2.3, measured on the CYD) and ~2.8 KB (`pages.md` §7) - and P4_5 has
  ~36 KB free. One measurement settles whether a whole second page fits or must be built in parts.
- **PSRAM:** a full-screen picture is 1.8 MB on `WS_P4_5` (1280x720, 16-bit); a set of pages is a
  few MB. Comfortable on the P4s; to be checked on the S3s.
- **The picture is a little old** when shown; the slide lasts ~250 ms and the live page replaces it.
- All four effects work on pictures without the full-screen layer that froze P4_5: slide and wipe
  move or clip the picture, fade sets the picture's opacity, shrink / grow scales it (the P4's PPA
  accelerates that; on the S3s it is the effect to cut first).

### 3.10 Navigation, authority, setup (owner, 2026-10-01) - recorded for §5, §6, §9

- **Fewer cards per page makes navigation matter more:** a navbar with hot-links to pages or rooms;
  a card of any size that **links to a room page** or a **function page** (a weather page, all the
  house lights). Goes to interview §5 (the "go to" address) and §6 (navbar).
- **Authority:** the build sheet "sets the stage for the first boot after flashing". After that,
  the user's own configuration is what every ordinary restart returns to. A **hard reset** wipes
  customizations back to factory settings. Consistent with ROADMAP Q2 (defaults < sheet < runtime
  edits; factory reset = the sheet).
- **What a build sheet IS (owner, 2026-10-01, answering Q-L4) - this reframes interview §8-§9.**
  "A build_sheet is nothing special, it's just a template with entity data and settings." Applying
  one at compile time has exactly the same effect as importing an exported one. It holds page
  layouts, settings, anything customizable; it is applied automatically on first boot, or at any
  time by **import**. The device and the web UI both **export and import**, perhaps at levels -
  "save all" (personal settings, layouts, entities) or "settings only" (Wi-Fi credentials, scheme).
  A setting **"Restore previous settings when flashed"** keeps the user's configuration across a
  firmware update; with OTA, a small UI can offer "save" or "hard reset" when updating. A user
  could even build a whole configuration in a web UI before flashing - the public would flash stock
  firmware and then upload it. The owner finds the name "build sheet" misleading for this.
  **Claude's notes:** (1) the current configuration belongs in a LittleFS file, not NVS - the same
  JSON shape as an exported template, so export is a file copy (interview §9 item 1's
  recommendation, confirmed by this). (2) A normal `pio run -t upload` or OTA writes only the app
  partition, so a LittleFS configuration **survives an ordinary firmware update by itself**; only a
  full erase or a partition-table change loses it - that is when an off-device backup is needed.
  (3) A changed template arriving with new firmware is then just "an import the user did not ask
  for", and "Restore previous settings when flashed" decides it. OPEN: a better name than "build
  sheet" (candidates: *profile*, *dashboard config*, *template*).
- **Web UI parity:** "I don't see any reason why the web UI shouldn't be able to configure as much
  as the build sheet." Import / export of configurations goes through the web UI, after v1.0.
- **First-boot setup wizard** like NINA's: joins Wi-Fi via a QR code (`reference/` -
  `wifi_qr_code.h`, `REFERENCE_PROJECTS.md`), sets the basics, then the web UI manages the device.

### 3.11 The card tag (owner, 2026-10-01) - DECIDED, three styles

The owner liked that the mocks' area tag partly overlaps its card. Decisions:

- **The default card header becomes the TAG** ("until we fix the card header bar - can't stand how
  it sticks out in the corners!"). **Floating** preferred.
- **A new knob, Tag:**
  - **Attached** - as today: the pill sits on the card's top edge, flush with its left edge.
  - **Floating** - the pill hangs over the top-left **corner**, sticking out by the same amount
    over the top and over the left, so it reads as lying on top of the card. With Linen's shadows,
    the tag should cast a shadow on the card.
  - **File folder** - like the tab of a manila folder: the card's left edge runs straight up to the
    top of the tab, and a rounded inside corner joins the tab to the card's top edge.
- **Shadow cost (Claude):** in LVGL the tag is its own object, so its shadow is a style on that
  object, drawn onto whatever lies under it - no extra layer. Shadow cost grows with the shadow's
  area, and a tag is small, so it should be cheap; to be measured with `/bench` on Linen.
- **The header bar's corners** sticking out past the card's rounded corners is a bug to fix, not a
  style - issue opened.
- The mocks with all three: Layout Playground (§3.8), Tag switch.
