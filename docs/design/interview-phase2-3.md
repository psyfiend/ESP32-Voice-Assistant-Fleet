# Design interview — the rest of Phase 2 and all of Phase 3

**Purpose.** A structured interview whose answers become the blueprint for everything left in
Phase 2 (2.8, 2.10, 2.11, the rest of 2.9, the deck's future) and Phase 3 (the build sheet), and for
how the build sheet, the on-device UI and the web UI live together. Written 2026-09-27 after the
owner's brain dump on card popups (`card-sheet.md`). **It does not reopen decisions already taken**
- those are listed in §0.2 so nobody asks them twice.

---

## 0. How to run it

### 0.1 Format

- **Facilitated by Claude**: one section per sitting, in the order below. Claude asks a few
  questions at a time in chat, shows a mock or screenshot where the question is about looks, pushes
  back where it sees a problem, and writes the answers into the named design doc. The owner answers
  in any form - a sentence, a sketch, a screenshot of something he likes, "your call". Answers
  jotted straight into this file between sessions are picked up too.
- **Every question has a recommendation** where Claude has one. "Go with the recommendation" is a
  valid answer, and a fast one.
- Questions marked **[BLOCKING x.y]** must be answered before milestone x.y starts; the rest can wait
  until their milestone is next.
- **Output of each session:** the answers are written into the design doc named at the top of the
  section (created if needed), decisions dated, and the ROADMAP row updated. The answers ARE the
  blueprint; this file stays as the index of what was asked.
- **Show, don't spec** (HANDOFF): where a question is about looks, Claude builds a throwaway mock on
  the glass or a screenshot before the owner is asked to choose, rather than asking in the abstract.

### 0.1a For the Claude facilitating it - the protocol

1. **Before a session**: read `CLAUDE.md`, `HANDOFF.md`, this file, and the design doc the section
   feeds (named in its heading; create it if missing). Check the section's questions against what
   has changed since - drop any the code or a later decision has already answered, and say so.
2. **Open the session** by naming the section, its purpose in one sentence, and roughly how many
   questions. Then ask **3-4 questions at a time**, in the order written, each with its context
   and recommendation in plain words (the owner dislikes jargon and metaphor - HANDOFF). Never
   paste the whole section at once.
3. **Looks questions: show first.** If a question is about how something looks or moves, build a
   throwaway mock on a board (or a `/screenshot`, or a quick HTML sketch for layout-only questions)
   and ask him to react to it. Say what the mock is NOT (e.g. "colours are placeholders").
4. **Engage, don't transcribe.** Push back when an answer conflicts with a decision in §0.2, a
   measurement, or a gotcha in §10 - name the conflict and the evidence, then let him decide. His
   "I'm wondering whether..." is usually a design instinct worth following up (HANDOFF).
5. **Record as you go**: after each batch, write the answers into the section's design doc, dated,
   in his words where they matter, with the decision and its reason. Mark anything left open as
   OPEN with what would settle it. Tick the question here (`- [x]` or "ANSWERED -> doc §n").
6. **Close the session** with a short summary: what was decided, what is open, what it changes in
   the ROADMAP; update the ROADMAP row and HANDOFF; commit (docs only) and push.
7. **Do not build** anything a session decides until the owner says go - the interview produces the
   blueprint, not code (throwaway mocks excepted). Keep a running "new questions" list at the end
   of this file for anything the answers raise.

### 0.2 Already decided - not asked again

| Decision | Where |
|---|---|
| Build sheet precedence: defaults < build sheet < runtime edits, unless locked | ROADMAP Q2 |
| Authority locks: tri-state INHERIT / LOCKED / UNLOCKED, inherited down Dashboard > Page > Grid > Card > Entity; **lock configuration, never interaction**; the UI names the ancestor that locked something | ROADMAP Q2 |
| The model is nested C++ structs; JSON is the interchange format; two loaders (compile-time, runtime LittleFS) | ROADMAP Q3 |
| Every `*Defaults` struct is a schema; a build sheet is overrides against it | ROADMAP Q3c |
| Unit-granular placement with a validator that reports, never resolves | ROADMAP Q3b |
| Public path works on first flash with local-only demo cards; personal path per room; cards responsive (preferred / minimum span, priority) | ROADMAP Q4 |
| Tap on a card is one-touch toggle; long press opens the popup | ROADMAP 2.10 |
| Popup: centred window, dim fill, tabs across the top, primary control in the header (pending D1-D7) | `card-sheet.md` draft 2 |

---

## 1. Look and feel - the whole product  → `docs/design/look-and-feel.md`

The answers here steer every later section, so they come first.

1. **Who looks at these screens, from where, for how long?** (Walking past at 2 m? Standing at arm's
   length? Guests?) *Sets text sizes, contrast, how much a card may say.* **[BLOCKING 2.8]**
   **ANSWERED 2026-10-01 -> `look-and-feel.md` §3.1**
2. **Three words for the feel** (e.g. "calm, legible, quick" vs "rich, alive, playful"). Claude will
   test every later choice against them. **ANSWERED 2026-10-01 -> `look-and-feel.md` §3.2** (the
   rule, not three words)
3. **Motion budget.** Everything slides/grows (HA-like), or motion only where it explains something
   (a panel opening), or almost none? *Motion costs frames on the S3s; the P4s can afford more.*
   **Recommendation:** motion only where it explains a change of place, 150-250 ms, never decorative.
   **ANSWERED 2026-10-01 -> `look-and-feel.md` §3.3** (design for P4, cut for S3)
4. **Night.** Should screens dim, switch to a dark/red-shifted scheme, or turn off at night; by
   clock, by room light (a lux sensor), or by HA's sun? *Screen dimming was pulled forward from 4.1
   as a daily-use problem.* **ANSWERED 2026-10-01 -> `look-and-feel.md` §3.4**
5. **Sound and touch feedback.** Boards have speakers. A soft click on tap, nothing, or only on
   errors? **Recommendation:** off by default, a setting.
6. **Icons.** MDI everywhere (HA's own set, today) - any you dislike, want bigger, or want replaced?
7. **Dense vs airy.** On the big panels, more cards per page or larger cards? (`TARGET_CARD_W` today.)
8. **What does "broken" look like?** Unavailable / stale / offline entities: greyed, struck, a badge,
   hidden? *Today: STALE tag, greyed.* Anything that annoys you now?
9. **Reference images.** Dashboards you like the look of (HA themes, Tile cards, Mushroom, a
   product). Screenshots welcome - worth more than adjectives.
10. **Owner's notes, 2026-09-29 - bring these into the section, do not re-ask what they state.**
    - **Linen:** find a better card fill colour. **Wi-Fi header icon:** more visible (a box or a
      background behind it?).
    - **Chrome text too small:** header device name and page title, deck panel headers, the system
      panel, the card's area tag. **Audited 2026-09-29:** the chrome faces (`UIToolkit::Font_Caption/
      Label/Button/PanelHeader/Hero`) are NOT on the type scale - `UIToolkit.cpp:40` picks one of two
      fixed sets by `bspUiScale() >= 1.3` (16/20/22/22/34 above, 10/12/14/14/24 below). The 4880 is
      1.28, so it gets the small set at 217 PPI: panel headers 14 px = 1.6 mm, captions 10 px =
      1.2 mm. The comment there says the sizes were to "move into the token header in the next
      commit"; they never did. **Proposal:** give the chrome roles mm targets in
      `gen_type_scale.py`, like the card roles, so every board derives them. The card area TAG is
      derived already, but its 1.90 mm target is small everywhere - one number to agree.
    - **Hero value size vs card size** (owner, on glass): **4B** - 4x4 a bit too big, 3x3 could be
      larger, 3x4/4x3 good. **7" boards** - 6x3 "naked" (bump it up), 7x3 fine, 8x3->8x4 fits.
      **4880 landscape** (screenshots `screenshots/fleet-cyd-p4-4880_20260929-01*.png`): 4x3 excellent,
      4x3 with deck good; 5x3 too big in places; 4x4 with header and tag not feasible.
    - **Found in those screenshots (Claude):** cards side by side pick DIFFERENT value faces on a
      5-column grid - height chooses VALUE vs VALUE_SM (`Card.cpp:199`), then width can force
      VALUE_SM (`ValueCard.cpp:151`), and "57.6 F" fits where "74.0 F" misses by pixels. And the
      height check ignores the corner icon, so a big value can collide with it. Candidate answers:
      one value face per page (the largest every card can take); a third, larger face for roomy
      cells; the icon in the fit.
    - **Rotation as a setting** (manual on esp_lcd boards is feasible; auto needs an IMU the 4880
      does not have) - belongs to §5 settings.
11. **Beyond the grid (added 2026-09-30, owner).** Is a page always a grid of equal cards, or can
    it hold clusters with their own shapes - a room, a function, a group - drawn as something other
    than cards? *Touches ROADMAP Q3b (unit placement, free positioning rejected) and 2.11.* Claude
    sketches two or three alternatives before asking. **[BLOCKING 2.11]**

## 2. Card popups (2.10)  → `docs/design/card-sheet.md`

The blueprint exists (draft 2). Open decisions D1-D7 are there; these are the rest.

1. **D1-D7** from `card-sheet.md` §10 - answer or "recommended". **[BLOCKING 2.10]**
2. **Which kinds get a popup in 2.10a/b?** Lights and switches first, sensors after the HA history
   work (2.10e)? **Recommendation:** yes, that order.
3. **Lights without colour** (dimmable only): Control tab shows only brightness - agreed?
4. **Colour presets**: fixed eight like HA, per-light favourites saved on the board, or taken from HA?
5. **Effects** (for lights that have them): a list in the Control tab, or skip? **Rec:** list, later.
6. **History spans**: 24 h default; also 1 h / 7 d? How many Activity rows before "show more"?
7. **Popup for virtual/system cards** (uptime, heap, RSSI): worth it, or long press does nothing?
8. **Closing**: tap outside, X, swipe down - which of these? (Rec in `card-sheet.md` D6: all three
   plus 60 s auto-close.)
9. **Custom name**: edited on the board with an on-screen keyboard, or only from the web UI?
   *An LVGL keyboard on a 3.5" screen is poor.* **Rec:** web UI only; the board shows the field
   read-only with "edit in the web UI".
10. **Settings scope in the popup**: which of variant / area / label / name / header colour / area
    colour / visible / pause are per-card, and which should be per-page or per-area instead?

## 3. Header bar v2 - the slot mechanism (2.8)  → `docs/design/header-slots.md`

2.8 builds ONE slot mechanism used three times: the page header, a group card's header, a normal
card's header (`cards.md` §8).

1. **Page header content, per screen size.** Today: device name, page title + dots, status glyphs.
   What should the left / centre / right hold on the 7" vs the 3.5"? **[BLOCKING 2.8]**
2. **Slot types wanted**: clock, date, weather now, outdoor temp, WiFi, MQTT/HA link, a chosen
   sensor, alarm state, "next calendar event", notifications count? Rank them.
3. **Clock**: 12/24 h, seconds, date format. *Needs SNTP + time zone - see §8 G6.*
4. **Tapping a slot**: does nothing, opens that entity's popup, or goes to a page?
   **Rec:** opens the popup, same as a card's long press - one rule.
5. **Header hidden / peek** (exists): keep as a per-page setting?
6. **Card header slots** (area left, STALE right today): should a card's header also be able to show
   a promoted value (e.g. battery %)?
7. **When a slot's entity is stale/unavailable**: hide the slot, grey it, or show a dash?

## 4. Group and area cards (2.11)  → `docs/design/group-cards.md`

1. **Confirm the two kinds** (`card-sheet.md` §6): *aggregate* (one card, several same-kind
   entities acting as one, e.g. the 3 office bulbs) and *area* (a multi-cell card holding several
   different entities with a header). Any third kind? **[BLOCKING 2.11]**
2. **Aggregate tap** when members disagree (some on): all off, or all on? *`cards.md` §4 row 4
   chose "tap toggles all; mixed state has its own indicator".* Still right?
3. **Area card sizes**: smallest useful size (2x1? 2x2?), and the most entities it may hold per size.
4. **Area header**: which promoted values (temperature, humidity, occupancy, light level)? One line
   or two? What if the area has none?
5. **Inside an area card**: entities as mini tiles (icon + state), as rows (icon, name, state), or
   chosen per entity? **Show-don't-spec: Claude mocks both.**
6. **Long press inside an area card**: on a member -> that member's popup; on the header -> the
   area's popup. Agreed?
7. **An area on a small screen** that cannot fit its members: collapse to an aggregate-style summary
   card, or drop members by priority?
8. **Rooms as linked pages** (`pages.md`): should an area card's header tap open the room's full
   page? How does "back" work - a header back button, a swipe, or both?
9. **Area colour**: per area, set once, used by every card in it (`card-sheet.md` §5.4) - agreed?

## 5. Edge gestures, the system panel, and what becomes of the deck  → `docs/design/context-panels.md`

**Owner's direction, 2026-09-27 - the working assumption until decided otherwise:**
- **The deck is NOT part of the default dashboard view.** It is a throwback to the owner's first
  design, a year old; it stays in code because it may be worth repurposing - inside popups or detail
  views, or on targeted "room" pages reached by a specific route rather than by swiping pages.
- **Device-wide settings and anything affecting the whole dashboard come from a system panel at the
  top**, in keeping with the traditional layout (today's System drawer).
- **Edge swipes become shortcuts**: at least **two swipe-up** targets and **swipe-down** targets,
  one of which stays fixed to the system panel / settings. (2.6 already moved the edge gestures into
  a target table, so targets are data, not code.)
- **Owner, 2026-09-27 (later): all three swipe zones should be user-customizable**, and a target can
  be **a page, a card popup, the top level of settings, a deep link to a specific settings view or
  tab (the log, for one), or anything else**. The log gets its own home "at a location that makes
  sense". So a *target* is a general "go to" address - the same idea as linked pages and card links;
  it wants ONE addressing scheme used by swipes, cards, slots and the navbar alike.

Questions:
1. **The three zones**: which edges/corners exactly (up-left, up-right, down?), their defaults, and
   whether the system-panel zone is customizable too or only re-pointable to another settings view.
2. **The "go to" address** (one scheme for swipes, card taps, header slots, navbar): what it must be
   able to name - page (incl. hidden room pages), card popup (and which tab), settings top level,
   settings deep link (view/tab, e.g. the log), the overview, a scene/script, a URL? What happens
   when the target no longer exists (deleted page, renamed entity)?
3. **Per page or per device?** Can a room page have its own swipe-up targets?
4. **The deck's future**: which of its three possible homes is worth pursuing - inside popups
   (e.g. a light's extra controls), on room pages (a strip of room controls), or retire it once
   Audio/Display move into Settings (4.1)?
5. **Audio and Display panels** (today's deck contents, developer tools): into the system panel /
   Settings now? **Rec:** yes, when 4.1 starts.
6. **Scenes and scripts**: runnable from a shortcut or a card? That is a new outbound call type.
7. **The system panel itself**: what belongs on it (brightness, scheme, page settings, device
   info, the knobs), and does it grow into Settings pages (4.1) or stay one drawer?

## 6. Pages and navigation (rest of 2.6's scope, 4.2/4.3)  → `docs/design/pages.md`

1. **Home page per device** and **return-to-home after idle** (how long)? **Rec:** yes, 2 min, a
   per-device setting.
2. **Page transitions** once drawing is fast (`pages.md` §7): slide via snapshot, fade via snapshot,
   or instant? *Measured: a page change is ~300-400 ms of rebuild before any animation could start
   (`display-stack.md` §8.9).* **Rec:** instant until the rebuild is faster; then slide.
3. **The overview (alt-tab)**: wanted soon, or later? Gesture to open it?
4. **Navbar (4.3)**: an edge strip of page buttons - wanted at all, given swipes and dots?
5. **Linked pages** (from a card or area): how deep may they nest (today: one-deep back)?
6. **Kiosk mode**: a board locked to one page, no drawer, no swipes (e.g. a guest room)? That is an
   authority-lock use case.

## 7. Finishing 2.9 (other boards)

1. **Order**: 7B (EK79007) and CYD_P4_1060 (JD9165) next, when each is on the desk - agreed?
2. **The S3s** (RGB panels, step 4) may need an S3 library rebuild (`display-stack.md` §6.2). OK to
   do that rebuild if the numbers call for it?
3. **CYD_S3_3248** (QSPI, step 5): its partial-update question is unanswered (survey §5). Worth a
   spike, or accept full-frame sends?
4. **The 16 ms refresh** (`FLEET_LV_REFR_PERIOD`) for boards that can afford it: on after a look?
5. **Packaging the flush** as a shareable component (owner, 2026-09-27): now, after step 6, or never?
   Name?

## 8. The build sheet (Phase 3)  → `docs/design/build-sheet.md`

### 8.1 What it describes **[BLOCKING 3.1]**

1. **Scope**: dashboards, pages, cards, entities, areas, decks, header slots, themes, device
   settings (brightness, dimming, WiFi mode), `*Defaults` overrides. Anything else? Anything out?
2. **One sheet per device, or one fleet sheet with per-device sections?** (Q4 says both paths exist;
   this is about the FILE.) **Rec:** one fleet file + per-device files that include/override it.
3. **Card identity**: every card gets a stable `id` in the sheet (needed for runtime overrides and
   saved settings - `card-sheet.md` §5.1). Human-chosen (`office_lights`) or generated?
   **Rec:** human-chosen, required, validated unique.
4. **Entity references**: by HA `entity_id` (renames in HA break them) or by HA's unique/registry id
   (stable, unreadable)? **Rec:** entity_id, plus a board-side "unknown entity" card that says so.
5. **Areas**: taken from HA's own areas, or declared in the sheet, or both?
6. **Themes/colours in the sheet**: can a sheet define its own scheme, or only pick one of ours?

### 8.2 Authoring

1. **Who writes it and with what?** A text editor + a PC-side validator; the web UI; or a visual
   editor later? (JSON is the interchange format either way - Q3.)
2. **Validation**: on the PC before flashing, on the device at load, both? **Rec:** both - the same
   validator code compiled for both.
3. **What the device shows when a sheet is bad**: fall back to the compiled sheet with a banner,
   refuse to start, or load what it can? **Rec:** load the last good sheet, banner naming the
   error line.
4. **Previewing** a sheet before it goes to a board: a screenshot from a board running it
   (`/screenshot` exists), or a PC-side render (much more work)?

### 8.3 Delivery and versioning

1. **How a new sheet reaches a board**: compiled in (3.2), uploaded to LittleFS over HTTP (3.3),
   pulled by the board from a URL/HA, pushed over MQTT?
2. **`schema_version` and migration**: when the schema changes, does the board convert an old sheet,
   or refuse it with a message? **Rec:** refuse newer-than-me; migrate older with a logged note.
3. **Fleet updates**: change the fleet file once, all boards pick it up - wanted? *That is a pull
   model plus a notification.*

## 9. Build sheet vs on-device UI vs web UI - living together  → `docs/design/build-sheet.md` §layers

The precedence is decided (§0.2). What is not:

1. **Where runtime edits are stored**: NVS keys, or a runtime overrides JSON on LittleFS mirroring
   the sheet's shape? **Rec:** an overrides JSON - one format, exportable, diffable.
   **[BLOCKING 2.10d]** (saved card settings are the first runtime edits)
2. **Web UI and on-device edits**: the same layer (last write wins), or separate layers?
   **Rec:** same layer; both are "the user", both respect locks.
3. **Export**: "save my board's tweaks back into a build sheet" - a button that produces the merged
   JSON? *Otherwise tweaks made on the glass can never become the new baseline.* **Rec:** yes.
4. **When a new build sheet arrives and the board has runtime overrides**: keep overrides that still
   point at existing ids, drop orphans with a log, or reset all? **Rec:** keep by id, report orphans.
5. **Factory reset**: back to the build sheet (not the built-in defaults) - Q2 says so. Also a
   "reset this card / this page" in the popup and settings?
6. **Locks in the UI**: a locked setting shows greyed with "locked by <ancestor>" (Q2). Can the owner
   unlock from the board with a PIN, or only by a new sheet? **Rec:** only by a new sheet.
7. **Web UI access**: open on the LAN (like `/screenshot` today), a PIN, or HA-authenticated?
8. **Multi-device**: a change made on one board's web UI - that board only, or offered to others
   (e.g. "apply to all boards showing this page")?
9. **What the web UI is** (4.5): served by each board (synchronous server, decided), one page per
   board; or a single fleet page served from somewhere else? *Each board serving its own is the
   decided baseline; a fleet view would need a host.*

## 10. Gotchas Claude already sees (for the owner to weigh, not answer)

| | Gotcha | Consequence |
|---|---|---|
| G1 | **`lv_mem` is 128 KB and P4_5 has ~36 KB free.** Popups, area cards, a second page for transitions and the overview all compete for it | each feature budgets its widgets; lazily built tabs; maybe `LV_MEM` in PSRAM one day (measured unnecessary so far) |
| G2 | **Flash writes (NVS/LittleFS) pause the flash cache**; the esp_lcd panel interrupt is not IRAM-safe | saving settings may glitch the panel - test before relying on it; batch writes |
| G3 | **Stable ids** for cards, pages, areas are the foundation of overrides, saved settings, locks, links and export | must be in schema v1, never retrofitted |
| G4 | **HA renames entities**; a sheet pointing at `light.office_left` breaks silently | an "unknown entity" card, and a report in the System Doctor |
| G5 | **Websocket receive buffer is 8 KB**; history and big attribute sets exceed it | use statistics API; raise/stream the buffer before 2.10e |
| G6 | **Wall-clock time** (SNTP + time zone) is needed by the clock slot, "8 min ago", history axes, night mode | confirm what exists; one time service for all |
| G7 | **The S3s draw 3-5x slower than the P4s** | every visual decision needs an S3 check; motion that is smooth on P4 may stutter on the CYD |
| G8 | **Two authoring surfaces (sheet + UI) invite drift** | export (§9.3) is what keeps them one thing |
| G9 | **Colour-blind owner** | every state distinction by shape/label as well as colour - a review item on every mock |
| G10 | **Board count grows** (JC4880P433 waiting) | every per-board number stays in the BSP or is derived; the sheet never names a board's pixels |

## 11. After the interview

Each section's answers go into its named design doc; ROADMAP Phase 2/3 rows are rewritten from
them with acceptance criteria; new issues are opened for anything the answers add. Claude then
proposes the running order for the rest of Phase 2 and Phase 3 in one table for sign-off.

## 12. New questions raised by the answers (running list)

| Raised | Question | Goes to |
|---|---|---|
| §1, 2026-10-01 | **Card sizes**: a card at 1x1 / 2x1 / 2x2 shows more on its face (a light's slider); who picks the size - sheet, glass, both? | §1 item 11 |
| §1, 2026-10-01 | **Drag-and-drop layout editing on the glass** (owner): an edit mode on the board, or web UI only? How it meets locks and the build sheet | §6 / §9 |
| §1, 2026-10-01 | **Per-device role and viewing distance** (nightstand, kitchen, garage): a setting that scales hero text | §1 batch 2 |
| §1, 2026-10-01 | **HA alerts on the devices**, customizable, also on the night screen: what is an alert, where it shows, how it is dismissed | new issue; §3 / §5 |
| §1, 2026-10-01 | **Clock card** with several faces, flipping as time changes; **weather card** with small animations | card library (Phase 2/4) |
| §1, 2026-10-01 | **Night mode** pulls #74 (SNTP) ahead of 2.10? | ROADMAP order |
| §1, 2026-10-01 | **Irrigation interface** (next summer): fleet firmware with an irrigation page set, or its own project? | later |
