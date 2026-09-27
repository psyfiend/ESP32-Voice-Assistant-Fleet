# Design interview — the rest of Phase 2 and all of Phase 3

**Purpose.** A structured interview whose answers become the blueprint for everything left in
Phase 2 (2.8, 2.10, 2.11, the rest of 2.9, the deck's future) and Phase 3 (the build sheet), and for
how the build sheet, the on-device UI and the web UI live together. Written 2026-09-27 after the
owner's brain dump on card popups (`card-sheet.md`). **It does not reopen decisions already taken**
- those are listed in §0.2 so nobody asks them twice.

---

## 0. How to run it

### 0.1 Format

- **Sessions by section**, in the order below; one sitting per section is plenty. The owner answers
  in any form - a sentence, a sketch, a screenshot of something he likes, "your call".
- **Every question has a recommendation** where Claude has one. "Go with the recommendation" is a
  valid answer, and a fast one.
- Questions marked **[BLOCKING x.y]** must be answered before milestone x.y starts; the rest can wait
  until their milestone is next.
- **Output of each session:** the answers are written into the design doc named at the top of the
  section (created if needed), decisions dated, and the ROADMAP row updated. The answers ARE the
  blueprint; this file stays as the index of what was asked.
- **Show, don't spec** (HANDOFF): where a question is about looks, Claude builds a throwaway mock on
  the glass or a screenshot before the owner is asked to choose, rather than asking in the abstract.

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
2. **Three words for the feel** (e.g. "calm, legible, quick" vs "rich, alive, playful"). Claude will
   test every later choice against them.
3. **Motion budget.** Everything slides/grows (HA-like), or motion only where it explains something
   (a panel opening), or almost none? *Motion costs frames on the S3s; the P4s can afford more.*
   **Recommendation:** motion only where it explains a change of place, 150-250 ms, never decorative.
4. **Night.** Should screens dim, switch to a dark/red-shifted scheme, or turn off at night; by
   clock, by room light (a lux sensor), or by HA's sun? *Screen dimming was pulled forward from 4.1
   as a daily-use problem.*
5. **Sound and touch feedback.** Boards have speakers. A soft click on tap, nothing, or only on
   errors? **Recommendation:** off by default, a setting.
6. **Icons.** MDI everywhere (HA's own set, today) - any you dislike, want bigger, or want replaced?
7. **Dense vs airy.** On the big panels, more cards per page or larger cards? (`TARGET_CARD_W` today.)
8. **What does "broken" look like?** Unavailable / stale / offline entities: greyed, struck, a badge,
   hidden? *Today: STALE tag, greyed.* Anything that annoys you now?
9. **Reference images.** Dashboards you like the look of (HA themes, Tile cards, Mushroom, a
   product). Screenshots welcome - worth more than adjectives.

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

## 5. The deck and context panels (towards 4.4)  → `docs/design/context-panels.md`

1. **Is the deck still wanted as a permanent strip**, or only on demand (swipe up)? On which boards?
2. **Per-page decks** (`card-sheet.md` §7): House -> "Lights" (all off, scenes) + "Climate"; Fleet ->
   System + Display. What would each of your real pages want there?
3. **Audio and Display panels** (today's deck): developer tools. Move them to Settings (4.1) when
   per-page decks arrive? **Rec:** yes.
4. **Scenes and scripts**: should the deck (or a card type) run HA scenes/scripts? That is a new
   outbound call type.
5. **Width**: two panels side by side today. More, fewer, or one full-width with tabs?

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
