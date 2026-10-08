# The card popup — long press, milestones 2.10 / 2.11

**Status, 2026-10-06: 2.10a (the frame) and 2.10b (the light controls) are BUILT and merged -
§14 and §15 say what they are, and win over everything above them.** §1-13 are the design and
the interview answers that led there, kept for the reasoning; where they disagree with §14-15
(the grow, the dim, the P4_5-shaped window), they are history. Decisions, one line each:
`docs/DECISIONS.md` K3-K16. Next: 2.10c-e (§9).

The original status - **design, 2026-09-27, owner and Claude**: draft 1 (a bottom sheet) was
replaced the same morning by the owner's brain dump, with HA's own dialogs as the reference
(light, switch, switch history, temperature graph). Sources: ROADMAP 2.10 (#65), 2.11 (#66), 2.8;
`context-panels.md` (mechanism A = invoked by a card, B = declared by a page); `pages.md` (linked
pages, one-deep back); `cards.md`. Open decisions are marked **D1..D7** and collected in §10, each
with a recommendation.

### Words for the window's parts (owner, 2026-10-07)

| Word | Means |
|---|---|
| **window** | the popup a long press opens |
| **title** | the text at the top centre: what the window or view is ("Desk"), with the view it came from in front ("Desk > Members") - the **breadcrumbs**; its first part is a link up |
| **chips** | the round navigation buttons in the corners: X, back, chart, members, clapperboard |
| **hero** | the view's main content: on the controls, the slider (or toggle) with its label, value and swatches; on Members, the list |
| **control deck** | the bar of **switches** under or beside the hero - Power \| Brightness, Temperature, Colour. It says which modes the light supports and which one the slider is set to |
| **switch** | one of the control deck's options |
| **selector** | how the chosen switch is shown - a metal face over the chosen switch, its icon in the accent |
| **SETTINGS panel** | the folder-tab panel that rises from the bottom of the screen, in the right half |
| **CHART panel** | its neighbour in the left half, up only while History shows (a demo for now) |

Older sections and the code say "deck" for the SETTINGS panel (`s.deck`, `deckFill()`, "The settings
deck") and "chips" for its choice buttons (`deckChip()`); from 2.10c, "deck" in conversation means the
control deck. The code's `s.hero` is the slider or toggle alone. Code names change when the code is
next reworked, not before.

---

## 1. Presentation: a centred window over a dimmed page (owner's instinct, agreed)

- **Centred, bordered, rounded**, like HA. Width by screen: ~70% on the landscape panels (P4_5,
  7B, 4B square), almost full-screen on the CYD's 320x480 portrait, always with a margin so the
  page underneath still reads as "underneath".
- **The dim is a translucent FILL** (`bg_opa` on a full-screen scrim), never an object `opa`: an
  `opa` below COVER forces a full-screen LAYER out of `lv_mem` - the allocation that froze P4_5
  (LESSONS). Showing the dim costs one full-screen redraw (P4_5 ~85 ms, 4B ~52 ms), once.
- **"Grow out of the card" - yes, but grow the FRAME, not the contents (D1).** Scaling a live widget
  tree per frame needs a transform layer the size of the window: out of `lv_mem`, same freeze. What
  looks the same and is cheap: an empty rounded rectangle animates from the card's bounds to the
  window's bounds (~200 ms), then the contents appear in it. Cost per frame is about a deck-panel
  swap's (measured: P4_5 ~32 ms, 4B ~17 ms). If it looks mean on the P4_5, it becomes a pop.
- **Draft 1's bottom sheet is dropped.** The owner is right that it competes with the deck, and on
  landscape screens a centred window reads better.

## 2. The window's anatomy - one frame for every kind

```
 +---------------------------------------------------------------+
 |  Office > Motion                                        [X]   |   breadcrumb (area > device)
 |  Office Left                                      [ (o) ]     |   name  +  PRIMARY CONTROL
 |  [ Control ]  [ History ]  [ Settings ]                       |   tabs: big segmented row
 |---------------------------------------------------------------|
 |                                                               |
 |                    body of the selected tab                   |
 |                                                               |
 +---------------------------------------------------------------+
```

- **Tabs are a segmented row across the top, every segment >= `UI::minTouch()` (9 mm).** The
  owner's objection to HA's corner icons is right for a wall screen; three segments fit even on the
  CYD (3 x ~100 px against a 58 px minimum there).
- **Settings is a TAB, not a deck panel (D2).** The deck belongs to the page, which is dimmed and
  behind the window; putting part of the window's UI there splits one thing across two layers,
  and the deck is hidden by default on most boards. The Settings tab reuses the drawer's
  knob-row look, so it still *feels* like the knobs the owner has in mind.
- **The primary control lives in the header, for every controllable kind.** A light or switch
  shows its toggle at the top right on every tab. That is the consistency rule, and it answers the
  owner's switch question (§3): no giant toggle needed, and the control is always in the same place.

## 3. What each kind lands on

| Kind | Tabs | Lands on | Body of the landing tab |
|---|---|---|---|
| Light (dimmable / colour / temp) | Control, History, Settings | **Control** | HA's layout, turned sideways on landscape: the big vertical slider on the left; on the right the mode row (brightness / colour / temperature), swatches, and effect if any. Portrait (CYD): stacked like HA |
| Switch, input_boolean | Activity, Settings | **Activity** | the timeline + recent changes (§4). Toggle in the header (D3) |
| Numeric sensor (temp, lux, power) | Graph, Settings | **Graph** | 24 h line chart, plus now / min / max. An Activity list for a number is noise, so there is none |
| Binary sensor (motion, door) | Activity, Settings | **Activity** | timeline + recent changes |
| Virtual / system (uptime, heap, RSSI) | Graph, Settings | **Graph** | from the local sample ring (§4) |
| Aggregate light (one card, several bulbs) | Control, Members, Settings | **Control** | group brightness like HA's light group; **Members** is a grid of the bulbs (§6) |

## 4. History and activity - where the data comes from (the part easy to overlook)

- **Numeric sensors: HA's long-term statistics** (`recorder/statistics_during_period`, 5-minute
  means) - exactly what HA's own graph uses ("5-minute aggregated" in the owner's screenshot): 288
  points a day, a small reply. Raw history for a chatty sensor can be thousands of rows - tens to
  hundreds of KB of JSON - against an 8 KB websocket receive buffer today.
- **On/off entities: `history/history_during_period`** with `minimal_response` and
  `no_attributes`: few rows, since they change rarely. **The Activity list is built from the same
  reply** - no second request.
- **Entities HA does not own (virtual, MQTT, system)** have no HA history: the board keeps its own
  ring of samples in PSRAM (e.g. every 5 min for 24 h), **lost at reboot** - to be said on screen.
- **Clock.** "8 minutes ago" and axis times need wall-clock time and a time zone (SNTP + TZ). To be
  confirmed whether the fleet already keeps real time.
- Replies arrive on the websocket task, which must never touch LVGL (HANDOFF): parked, then drawn on
  the next `tick()`. Fetched when the tab opens, not before.
- Colour-blind rule: timeline segments carry their labels ("On"/"Off", as HA does), never colour alone.

## 5. Per-card settings (Settings tab) - and the complications behind them

The owner's list: variant (full / compact / auto), area shown, label (name / state / none), custom
name, header colour, and area colour for every card in the area. All feasible on the screen; the
work is underneath:

1. **A card needs a stable identity.** Today a card is "the Nth entry of this page's spec". A
   setting saved against "card 7" re-points silently the day the spec changes - exactly what
   HANDOFF forbids. Cards need a stable key (the spec's own, or its primary entity id). **Must
   exist before the first setting is saved.**
2. **Persistence means flash writes.** NVS writes briefly stop the flash cache; the esp_lcd panel's
   interrupt is not built IRAM-safe (`display-stack.md` §6.2), so a save might glitch the panel.
   Save on closing the window, not per tap, and test it on the glass.
3. **Who wins: the board or the build sheet?** Until 3.1 the board's saved settings are the only
   truth. Afterwards they are overrides on the sheet - ROADMAP's authority locks. Settings saved now
   must be designed to become overrides then, not a rival store.
4. **Area colour** is per area, not per card: an area table (name -> colour) that any card in the
   area edits, repainting every card in it (`restyleAll()` exists).
5. **"Visible"** (HA has it): a hidden card needs a way back - a "hidden cards" list in the drawer.
   Otherwise a card can be lost to a mis-tap.

## 6. Group and area cards - two different things, as the owner says

**Aggregate card (one card, several bulbs - e.g. the Office card).** Its window has **Members**: a
grid of the bulbs as borderless tiles, each behaving like a card - tap toggles, long press shows that
bulb's own controls **in the same window** with a back arrow (the content swaps; windows do not
stack - one window's worth of `lv_mem` at a time). This keeps everything in one place (D4). The
owner's alternative, a hidden page, already has a home: `pages.md` designed **linked pages** with
one-deep back and the title in the header. It fits a **room** better than a light group.

**Area card (ROADMAP 2.11, #66): a multi-cell card holding several entities.** The owner's
description is 2.11's. Its header of promoted values (the area's temperature and occupancy) is
**2.8's header slots** - which is why 2.8 comes first. Complications:
- Each entity inside needs its own touch target >= 9 mm, so a 2x1 area card holds perhaps 4-6.
- Taps must reach the entity tile, not the card (LESSONS: events do not bubble through a clickable
  child).
- **Defining** an area card - which entities, which feed the header - is a build-sheet (3.1) /
  web-UI (Phase 4) job, not an on-glass one. On the board: long press on the area card's header opens
  the area's own window (members, area colour, area settings).

> **Owner, 2026-09-27: the deck is not part of the default dashboard view** (a throwback to his
> first design). Where it may reappear - inside popups and detail views, or on room pages reached by
> a route rather than a swipe - is an open question (`interview-phase2-3.md` §5). Nothing in this
> blueprint depends on the deck; device-wide settings live in the system panel at the top.

## 7. Behaviour while the window is open (easy to forget)

- **Modal**: page swipes, edge gestures and the header peek are off; the deck scrim rule applies
  (CLICKABLE, never HIDDEN).
- **Live**: it follows the entity (registry dirty set), shows "unavailable" if HA drops it, and holds
  the **entity**, not the card, since a card can be rebuilt underneath.
- **Auto-close** after some idle time (D6), so a wall screen does not sit on a dialog all day.
- **Long press vs swipe**: a finger that moves cancels the long press (it must not start a page
  swipe *and* open a window); the touch is released before the window is built (LESSONS).
- **Cost of the page underneath**: cards keep updating beneath the dim, each redraw blending the
  scrim. Possible later: freeze the dimmed page as a picture while the window is open.

## 8. How it looks - notes, not rules

- The window surface uses the scheme's raised surface (Midnight / Linen), not white.
- **Keep the window's own shadow small or none**: Linen's shadows were measured +35% drawing time
  (`display-stack.md` §8.2), and a window-sized one is the most expensive shadow on screen. The dim
  already separates it.
- **No `clip_corner`** on the window (a layer, LESSONS); inner objects stay inside the radius.
- Charts: one line in the accent colour, faint grid, large axis text at our scale; ASCII and `°` only.

## 9. The plan

| Step | What | Where it can be tested |
|---|---|---|
| **2.10a** | Window frame: long press, dim, grow-the-frame animation, header + tabs, modal behaviour, Settings tab working in RAM (not saved), pause moved into it | laptop boards, virtual lamps |
| **2.10b** | Light Control tab (brightness / colour / temperature, swatches) and the header toggle, on the virtual lamps | laptop boards |
| **2.10c** | HA: light attributes, outbound `light.turn_on` with data | home |
| **2.10d** | Stable card ids + saving settings (with the flash-glitch test) | anywhere |
| **2.10e** | Graph / Activity: HA statistics and history, the local ring, clock | home for HA data |
| 2.8 -> 2.11 | Header slots, then area and aggregate cards with their windows | - |
| 4.4 | Context panels (mechanism B) - wherever the deck is repurposed, if at all (`interview-phase2-3.md` §5) - built from the same rows | - |

## 10. Decisions

As drafted on 2026-09-27. How each was finally decided - D1 was reversed on glass - is in
`docs/DECISIONS.md` K4-K10.

| | Question | Recommendation |
|---|---|---|
| **D1** | Grow from the card? | Yes, the frame only; fall back to a pop if it looks poor on P4_5 |
| **D2** | Settings: tab or deck? | **Tab** (§2) |
| **D3** | Switch landing | **Activity**, with the toggle in the header |
| **D4** | Aggregate card: members in the window, or a hidden page? | **In the window**; linked pages for rooms |
| **D5** | Card settings before the build sheet exists | Saved on the board, keyed by a stable card id, designed to become overrides at 3.1 |
| **D6** | Auto-close after idle | **60 s** |
| **D7** | Colour choice | Hue + swatches first (LVGL 9 has no colour wheel); a drawn wheel later if wanted |

## 11. Interview §2 answers (owner, 2026-10-01)

**Reference:** the owner's HA details dialog for the Office light group ("Desk", a Hue group) -
header with area and name, close / history / settings (gear) / menu icons; "100%" and "22 minutes
ago"; a big vertical brightness slider; a mode row (power, brightness, colour wheel, colour
temperature); eight swatches; then each member light as a row with its own slider. This is the
layout the popup mock follows, turned sideways for landscape.

- **D2 - tabs AND the deck, for different things (refines the recommendation).** The deck "isn't
  part of the default dashboard" meant there is no reason for it on a standard page; inside a
  popup, or on a special page (an area page holding one cluster of the room), deck panels can be
  useful. **Tabs** hold the entity's HA-style features - controls, history, colour, show / hide.
  **Deck panels** hold settings about the device or dashboard: style / scheme, header colour, an
  alternate cluster name, renaming entities, which entities feed the card header (temperature
  etc.) - the things behind HA's gear icon. Claude's notes: the deck then becomes a component a
  popup or page can carry (interview §5 item 4 - "inside popups" - answered by this); on a 3.5-4"
  screen a deck panel opened inside a popup covers most of its body, which is acceptable for
  settings; `lv_mem` for a deck inside a popup is to be measured with the rest of the window (G1).
- **D5 - RECOMMENDED, accepted.** Saved into the configuration file (LittleFS, the build sheet's
  shape - `look-and-feel.md` §3.10), keyed by a stable card id, written when the popup closes.
- **Which kinds first (item 2) - accepted:** lights and switches (2.10a-c), sensors after HA
  history (2.10e), media / climate / the rest with their card types.
- **Custom name (item 9) - an on-device keyboard IS wanted, as an option.** "Less than ideal but
  should be available for those that want it." The web UI remains the comfortable way. Plus a
  **label choice: HA device name / custom name / status (state) / no label.** (Today's label modes
  are name / state / none - this adds "custom".) Claude's notes on the keyboard: LVGL's keyboard
  widget costs some `lv_mem` only while it is open; the panels draw ASCII plus the degree sign
  only, so a name typed with other characters would show boxes - the keyboard should offer ASCII.
- **Mock for the next batch:** artifact **Card Popup Mock** (https://claude.ai/artifact/Lt6NmxMYLc7xrWzaomEqDm)
  - grow vs pop (D1), a switch landing on Activity (D3), group members in the window with
  long-press-to-drill-in (D4), X / tap outside / drag down plus an auto-close timer (D6), hue strip
  plus swatches and a colour-temperature strip (D7), the gear opening a settings deck inside the
  window (D2). **Found while building it (Claude):** the P4_5 is ~110 x 62 mm, so at touch-safe
  sizes HA's dialog does not fit in one view - in landscape the tabs move into the header row and
  the members get their own column; on the 4B the members scroll under the swatches.
### 11.1 Batch 2 answers (owner, 2026-10-02) - the popup layout

"Card Popup Mockup is terrific! Form and function is excellent!" Decisions:

- **D1 - GROW.** "Looks amazing in the mockup!"
- **D3 - open directly where actions can be taken**, with **no toggle in the header**. A switch's
  window opens on its control: the hero IS a big toggle (HA's own switch dialog: "On, 2 seconds
  ago", a tall toggle). Activity is behind the history icon.
- **D4 - members NOT on the first view** (crowded). A card representing several members gets a
  **members icon** (the multi-bulb hero icon) that navigates to a members view.
- **D6 - all four**: X, tap outside, drag down, auto-close.
- **D7 - hue control yes; the swatches move OFF the first view** into the colour (RGB) mode.
- **Layout (owner's proposal, built in mock v2):**
  - **Top left: X** to close. Any view reached from the first one (history, members, a member's
    own controls) shows a **back arrow** there instead.
  - **Top middle: "Area > entity"**, or "Group > entity" for a member.
  - **Top right: navigation icons** like HA's - the graph icon for history; the multi-bulb icon
    when the card has members.
  - **The hero is the control** - the slider, or the toggle. **Tapping anywhere along the slider
    moves it there**; optionally a setting "tap the slider to toggle on/off" - feasible, because a
    press that does not move is told apart from a grab-and-drag (mock v2 has both).
  - **Beside the hero:** the label of what it controls ("Brightness", "Colour", "Temperature") or
    the state; under it the value (percent / state / kelvin); under that the **selector:
    Power | Brightness, Colour, Temperature** with a small vertical divider after Power, as in HA.
    Power makes a header toggle redundant. Modes the light does not support are hidden (from HA's
    `supported_color_modes`) - a dimmable-only light shows Power | Brightness (answers §2 item 3).
  - Everything justified against the hero, the whole group centred in the window.
- **The deck in a popup - pathway 1 (the owner's preference), built in mock v2:** when the window
  springs up, the deck panel animates up from the bottom of the screen to show only its header
  ("SETTINGS"), as the hidden system header peeks down when swiped. Tap the header to open it; tap
  the header or anywhere inside the window to fold it back; a tap outside the window closes the
  panel and the window together. On close the header retracts so it is out of sight by the time the
  window has gone - quickly if the panel was open, more slowly if only the header showed.
  (Pathway 2, a gear icon with the deck fully hidden, is the alternative not chosen.)
- **Sensor cards get a second deck panel header - CHART:** span (24 h / 12 h / 6 h / 1 h), min / max
  on the card, legend, chart behind the card, units.
- **Hue groups verified (owner, Developer Tools):** `light.office` carries `is_hue_group: true`,
  `hue_type: room`, `lights:` (names), **`entity_id:` (the member entity ids)**, and
  **`hue_scenes:`** (Nightlight, Energize, Bright, Honolulu, Relax, Concentrate, Read), plus
  `supported_color_modes: [color_temp, xy]`. Members come straight from attributes; no registry
  lookup. Mock v2 shows the scenes as chips in the colour mode - OPEN whether they belong there.

### 11.2 Batch 3 answers (owner, 2026-10-02 night)

- **Mock bug the owner caught:** Desk's settings offered "Card header shows (temperature /
  occupancy)" - because the mock's rule was "has members". The setting belongs only to clusters
  and areas that have such a member. Fixed in mock v3.
- **Hue scenes:** the owner wonders whether they belong on the first view, for one-tap scene
  loading, and suggests a tap or long press could be configured to **load a specific scene or
  cycle through scenes** (as with the cycling temperature group). Second choice: **a target in the
  selector - Power | Brightness / Colour / Temp / Scenes**. Claude's recommendation, built in mock
  v3: **the selector target** (one tap away, the same place as every mode - uniform navigation, the
  owner's own rule below) **plus** the per-card tap behaviour "load scene X" / "cycle scenes"
  (`group-cards.md` §2C) for true one-tap loading from the dashboard. OPEN for confirmation.
- **Colour presets (item 4) - agreed:** eight defaults like HA's; long-press a swatch to save the
  current colour into it, per light, in the configuration file.
- **History (item 6) - agreed:** 24 h default with 1 / 6 / 12 h; 10 activity rows then "Show
  more"; fetched from HA when the view opens.
- **Virtual / system cards (item 7) - agreed**, and the reason generalises: **"I want all cards to
  behave as uniformly as possible so you don't need to remember navigation for every type of
  card."** Long press always opens a window; the same header, the same back arrow, the same deck.
- **Light effects (item 5): later.**

### 11.3 Settings scope (item 10) - the owner's model, and Claude's review

**The owner's proposal (2026-10-02):**

- **Separate the tag from the area.** The tag's shape is the **card label type** (tag / float /
  band / folder / none); its words are the **card label text**, defaulting to the card's area. A
  card with no area still shows its label type, with blank text.
- **Inheritance as per-card checkboxes:** "Disable page inheritance", "Disable area or group
  inheritance", "LOCKED - disable all inheritance".
- **Per page (every card on it):** variant (default compact), label type (default float), label
  text on/off (default on = area), entity label (name / state / none; default name), colour
  scheme, fill (default fill).
- **Per area or group:** label type, label text, label colour. Not changeable on a card unless it
  has disabled inheritance; re-enabling restores the area's settings.
- **Per card within a group:** variant, entity label, visibility.
- **Per card that has blocked inheritance:** variant, label type, label text (area / custom),
  label colour (ideally not one already used by another area or card), entity label (name /
  state / none / custom), visibility.
- "Much of this is a moot point as soon as clusters enter the picture" - tiles inside a cluster
  wear no per-card decoration.

**Claude's review - viable, and mostly a good idea; four corrections:**

1. **Precedence must be stated.** Page and area both set label type. Proposed: **device defaults <
   page < area / group < card** - the more specific wins - which is ROADMAP Q2's order with the
   area added.
2. **Inherit per SETTING, not per card.** Instead of checkboxes that must be ticked before a value
   can change, every setting shows where its value comes from - "Float (from page)" or "Band (from
   Kitchen)" - and changing it on the card simply overrides that one setting, with a **Reset**
   that returns it to inheriting. Same safety (the source is always named; reset restores), one
   fewer step, and no hidden state where a control refuses to change. It is how the build sheet
   already works (`*Defaults` < sheet < runtime overrides, stored sparse). The owner's "disable
   all inheritance" becomes a "Detach" button that copies the current values onto the card.
3. **"LOCKED" is already taken.** ROADMAP Q2's lock means the opposite: a PARENT forbids children
   to override (a kiosk page, a guest board). Keep that word for that.
4. **Unique label colours: warn, do not forbid.** A palette has only so many colours the owner can
   tell apart (G9); forbidding a used colour runs out fast. Show "Kitchen uses this colour" and let
   the user choose.

**DECIDED (owner, 2026-10-02):** "I'll go with your suggestions regarding the corrections."
So: precedence **device defaults < page < area / group < card**; inheritance **per setting**, with
the source named and a **Reset**; "Detach" instead of a per-card "disable all inheritance";
**LOCKED** keeps ROADMAP Q2's meaning; label colours **warn** rather than forbid. A card with no
area **hides its pill and keeps its space** (today's behaviour - cards stay lined up). The coloured
label type is called **"band"** - "bar" is confusing beside the system header bar. Label types:
**tag / float / band / folder / none**. (The code still says `HDR_BAR`; rename when it is next
touched.)

## 12. Section 2 CLOSED - 2026-10-02

**Decided:** D1 grow; D2 tabs for the entity's features, deck panels for dashboard settings; D3
open on the control, no header toggle; D4 members behind their own icon; D5 settings in the
configuration file by stable card id, written on close; D6 X / tap outside / drag down /
auto-close; D7 hue control, swatches in the colour mode. The window layout is the owner's (§11.1),
the deck peeks up from the bottom (pathway 1), sensors get a CHART panel, scenes are a selector
target plus a per-card tap action (§11.2 - recommended, not objected to), presets / history /
uniform popups (§11.2), the settings model (§11.3).

**Still open, none blocking 2.10a:** the group taxonomy's confirmations (`group-cards.md` §4, for
interview §4); `render_template` for area members (verify); light effects (later).

**The build order is unchanged** (§9): 2.10a frame, 2.10b light controls, 2.10c HA attributes
and calls, 2.10d stable ids + saving, 2.10e history - with this section's answers replacing draft
2's guesses wherever they differ.

- **D4 - opened a larger question: what kinds of "group" exist.** Recorded and answered as a draft
  taxonomy in `group-cards.md` (source x presentation x behaviour). The members-inside-the-popup
  part of D4 matches the HA reference above.

## 13. 2.10a build decisions (owner, 2026-10-02)

Asked before building; the owner took every recommendation. These replace §9's 2.10a line where
they differ (it still says "Settings tab" and "pause moved into it" without saying where).

- **Body:** a switch gets its real big toggle (simple enough to build now); every other kind shows a
  read-only value / state with "N minutes ago". Lights' sliders are 2.10b.
- **Pause** moves from long press to a **"Paused: Off / On" row in the SETTINGS deck**.
- **Drag down to close starts on the header row only**, so it never fights 2.10b's tall vertical
  slider.
- **The window's deck is a new, small part** owned by the popup. Reshaping the page deck
  (`GUIManager`'s Audio / Display accordion) waits until interview §5 decides where the deck lives.
- **History and members icons** are shown and lead to an empty placeholder view with a working back
  arrow, so the navigation is testable before 2.10e and the group work fill them.
- **Deck contents in 2.10a:** label choice (HA name / custom / state / none), Shown / Hidden, Paused.
  Custom name shows the option; the keyboard is later.
- **The window sits on LVGL's top layer**, above the header and the system drawer. The page under
  the dim keeps updating live.
- **The dim appears at once, not faded**: each frame of a fade redraws the whole screen (~85 ms a
  frame on P4_5); appearing costs one redraw. The grow runs over it. If it looks abrupt on glass, a
  fade is the thing to try.
- **Long press opens the window on every card type** (§11.2's uniform rule); tap is unchanged.
- **Deck settings live in RAM** and are lost at reboot - except Paused, which already persists
  through #60's pause store. Saving by stable card id is 2.10d.

## 14. The popup's deck, as built and tested (owner, 2026-10-05)

What 2.10a ended up with, after rounds 4-12 on glass (`docs/archive/TEST_2.10a.md`); signed off and merged
2026-10-05 (`v0.2.8`). Where it differs from the sections above, this wins.

- **The window appears complete at once and closes at once** - no grow, no dim (owner: speed first).
  The held card is pressed in with an accent border, and stays so while its window is open.
- **Each deck panel is the window's full width when open; only its TAB is offset.** A folder-tab
  shape: the pane has its own top edge, the tab rises from it with a curved inner corner. Folded,
  only the tab shows at the bottom of the screen, at the page deck's header height; the corner and
  the pane's edge sit just below the screen. Open, the pane's bottom edge stays below the screen.
- **SETTINGS always takes the right half.** A card that needs a second panel (a sensor's CHART,
  11.1) puts it in the left half - so SETTINGS is never pushed around.
- **With two panels (not built yet):** both tabs sit at the bottom when closed; each panel has its
  own open and close animation; opening one hides the other's tab; only one can be open at a time.
- **While a panel is open, the window is out of reach**: a tap anywhere in the window only folds
  the panel. A tap outside both closes everything.
- **Closing**: an open panel vanishes with the window; a tab that was only peeking slides back down.
- **Speed**: the deck is quicker than the page deck's panels (220 ms vs 300 ms, over a longer
  travel) and that is kept - settings should be quick to reach. Other expanding menus are judged as
  they come (no one-size rule).
- **Contents in 2.10a**: Paused works (kept on the device); Label, Custom name and On the dashboard
  are shown, quieter, and inactive - "fine for now", which settings exist is not settled.
- **Memory**: a window with its deck costs ~11-12 KB of LVGL's pool; with the pool at 128 KB in
  internal RAM, ~16 KB was left with one open. Answered at 2.10b: the pool is in PSRAM (§15).

## 15. 2.10b - the light controls, as built and signed off (2026-10-06, `v0.2.9`)

Signed off on WS_P4_5 after seven rounds, and seen on WS_P4_4B and CYD_P4_1060; every round and
measurement is in `docs/archive/TEST_2.10b.md`. Where this differs from sections 9-14, it wins.

**Size.** The P4_5's hero (the toggle or slider, 30 mm on calipers, "the perfect size") is the
reference: on every board the hero is the same share of the window as on the P4_5 (`pm()`, "P4_5
millimetres", from the height of the biggest P4_5-shaped window that fits). Touch targets and text
stay real millimetres. **The window is up to 2:1, never wider than 100 mm, with a side gap of at
least 6 mm or 8% of the screen**: P4_5 93 x 47 mm, 7" 100 x 75 mm, 4B ~60 mm. (Round 1 gave every
board the P4_5 window's 787:545 shape; the owner then asked for width on the P4_5, and round 5's
~129 mm on the 7" made every reach long.) Hiding the system header makes the window taller, never
wider. The rearranged layout (the selector centred along the bottom) was declined: the P4_5 lacks
the height (62 mm tall at 294 PPI). Covering the system header: decide at 2.10e, with the charts.
Portrait boards (the 4880) are left for later (owner).

**Nothing moves within one window** (owner, rounds 3-5). The column beside the slider or toggle is
as wide as that light's selector - for a group, everything its members can do, paused or not - or
a fixed allowance for its words, whichever is wider, and as tall as the hero: the label line at the
top (the PAUSED pill beside the label, the same height), the selector level with the hero's bottom.
The row is centred, so a toggle and its words sit centred, four controls are centred as a whole,
and two move in from the edge.

**Light state lives in the registry (owner).** What a light can do (HA's `supported_color_modes`
folded into dim / temperature / colour) arrives from the source with every report, so nothing is
saved to flash; levels are null while off, as HA reports them. A brightness, temperature or colour
command is applied at once and confirmed only by a matching report, exactly as on/off is (#63);
three seconds without one, it reverts and the card says FAILED. That is the "trust but verify" the
owner asked for: after a reboot, or while the link is down, nothing is shown as confirmed that the
source has not said.

**Commands while dragging: every 300 ms, and once on release (owner: "300ms sounds about right").**
The slider and the value follow the finger every frame; toggles still command on release.

**The window (mock v3, owner):** the slider is the hero, a tap jumps to that point; beside it what it
controls, the value, when it changed, and Power | Brightness, Temperature, Colour, showing only what
the light can do. Colour shows eight default swatches in place of the value line (the column must
fit 34 mm on the P4_5). A switch, an on/off light and a light whose source has not said what it can
do get the big toggle.

**Groups behave as in HA (owner):** what is offered is everything any member can do; each member is
sent what it can take (a brightness turns an on/off member on - the owner's 1% test; a temperature
becomes the nearest hue for a member with colour but no temperature, as HA converts; a colour leaves
the others on). Shown levels are the mean of the members that are on. A lamp comes back on at its
remembered levels (the owner saw HA restore a member's brightness). **"On when: any / all"** (HA's
group helper option) is a card setting, `GroupOn`, in the deck of a card standing for several
things; a tap on a group that is on turns all off, otherwise all on (it was "the inverse of the
majority"). Native HA groups are one entity, and HA decides for them.

HA's own rules, read from its source (`group/light.py`): brightness and colour temperature are the
mean over the members that are ON, hue a circular mean, the mode the most common, the capabilities
the union. **The group card fills to that mean brightness** (one at 100%, one at 50%: 75%).

**Paused.** A paused member is out of its group: not counted for on or off, not offered (a mode
only it has leaves the selector, even when every member is paused), not commanded. The group is
PAUSED only when every member is - a fully paused group keeps its greyed slider and offers only
Power; its status line says "1 paused" otherwise; Members lists the member, reading "Paused". The
card follows the same rule (`Card::counts()`). For a group defined at the source the registry
keeps the group's own pause in step with its members both ways (2.10c round 9, K32): pausing every
member pauses the group, and resuming any member resumes it - before that, a group paused from
its own window stayed PAUSED after each member was resumed from its own. A paused window: controls greyed, a PAUSED pill
beside the label, a touch on a control explains itself. **Paused in the deck is a switch** ("Off /
On" read two ways).

**Members.** A tap on a member row opens that member's own controls in the same window ("Group >
member", the back arrow returns to Members) - section 11.1's member view, reached by a tap (owner)
rather than the mock's long press. It needs no card of its own.

**The header.** The chart button is always in the corner; the members icon sits inside it.

**Memory.** The deck's rows are built when it is first opened, and only the chosen control is built
(owner: every reasonable saving without side effects). **LVGL's pool is in PSRAM at 512 KB on every
P4** (owner, 2026-10-06, #88): internal heap free 233 -> 361 KB, a window's biggest free block
13 -> 399 KB, full-screen frames ~10% slower.

**Open, for 2.10c-e:** scenes (`hue_scenes` on `light.office`) as a selector target (11.2) need HA's
attributes - 2.10c; saving a swatch with a long press needs the settings store - 2.10d; covering the
system header - 2.10e. The sensor hero's disc grows with the window around a fixed-size icon face,
so on a 7" it is large for its glyph (a bigger face costs flash).

## 16. 2.10c - Home Assistant's lights, as built (signed off 2026-10-07, `archive/TEST_2.10c.md`)

Measured before building: `ha-websocket.md` section 9. Decisions: `DECISIONS.md` C5, K12, K15, K17.

**Step 1 - levels.** An HA light reports what it can do (`supported_color_modes`, folded into the
four capability bits of section 15), what it is showing (`color_mode`) and its levels; one reader
(`HaValue.h`) serves the boot fetch and the live feed. So an HA light gets exactly the window a
virtual lamp of the same kind gets, and a light HA reports as on/off only - or a `switch.*` - keeps
the big toggle. Commands are `light.turn_on` with `brightness` (0-255), `color_temp_kelvin` or
`hs_color`; confirmed by the light's own report as before. HA's reply to the call is read: a
refusal (`success: false`, which means HA sent nothing) shows FAILED at once instead of after 3 s.
On the owner's Hue bulbs a colour confirms on the first report, then the bridge may correct it to
what the bulb can really make (blue 240 -> 255) about 1.4 s later, and the window follows.

**Step 2 - groups defined in HA or Hue (K17).** `light.office` is one entity to HA. Its `entity_id`
attribute names its bulbs; the registry LEARNS them - registers each as an entity of its own while
the board runs (id `ha_light_office_lamp`, name from its own `friendly_name`), subscribes and
fetches it. So Desk's window has the members icon, Members lists the three bulbs, and a tap opens
one bulb's own controls - exactly as All Lamps does with Lamp 4, which has no card either.

- **With no member paused, the window and the card stand for the group itself**: HA's own report
  is shown, and a command goes to `light.office` once, so the Hue bridge changes the bulbs together.
- **While any member is paused, they stand for the members** (section 15's rule, K14): the paused
  bulb is out - not shown in the mean, not commanded - and the others are commanded one by one
  (they may change a moment apart). The card does the same (`Card::liveEntities()`).
- **Pausing Desk itself pauses the group and every member**; resuming resumes them all.
- **"On when: any / all" is not offered** for such a group: HA or Hue decides when it is on.
- A member's pause is saved like any other and applied once the member is learnt after a reboot.
- Adding a member as a card of its own: later (owner) - it needs a place on the page (#78) and
  saving (2.10d).

**Step 3 - scenes (K15, K19).** A light's scenes are HA's scene entities on the light's own device -
for a Hue room, the scenes made in the Hue app - found by one `render_template` per session
(`ha-websocket.md` section 7a's pattern, cancelled once answered) and learnt like members, each a
BUTTON entity named as HA names it ("Relax"). Stock HA, no helper; scenes kept in HA's own
`scenes.yaml` belong to no device and are not offered.

- **Scenes is a view of its own** (owner, after round 3 - it was first built as a fifth selector
  button, and the off-centre selector looked awkward). Its icon (`mdi:movie-open`, a clapperboard,
  already in the icon faces; HA's own scene icon, the palette, is Colour's here) sits **under the
  chart** in the window's corner, on the light's controls and its scenes only, and only for a light
  with scenes - not while the window works through a group's members, as a scene would reach the
  paused one.
- **The brightness slider stays where it is; the scene buttons take the column** - the label, the
  value and the selector give way. Sized as a share of the window (9 "P4_5 millimetres", never under
  the swatches' 6.5 mm), three rows in the slider's height; when there are more, half a fourth row
  shows so it is plain there is more to scroll to. Centred, sorted by name.
- **A tap loads the scene** (`scene.turn_on`) and rings that button. The ring means "as the scene
  left it", so it goes with any command from the window, with a change from elsewhere once the
  scene has held still for 3 s (its own fade reports come within ~1.5 s), and with the window. HA
  records only when a scene was last activated, so no scene is ever shown as on. A refusal by HA
  marks it FAILED.
- A stacked layout for windows too narrow for a fifth selector button was built for the 4B and
  removed with the button.
- **The control deck under the hero where the window has the height** (owner, 2026-10-07, K23): the
  4B and the 7" panels; the P4_5, the one board too short (its screen is the widest shape), keeps it
  beside the slider. Worked out from the window's height. The slider, its words and the deck are
  centred. ~~Scenes - and Colour under a tall window - push the slider to the stage's left edge~~
  (see "The slider's place", below, K26). The control deck never moves - in Scenes, where there is
  no deck, its room is kept so the slider stays at the same height.
- **The control deck after HTC's TouchFLO 3D** (owner, 2026-10-07, K24): a **ribbon** three quarters
  of a switch tall, and on it the **selector**, as tall as the deck, carrying the chosen switch's icon
  a size up (the LG icon face) in the accent. The selector can be grabbed and slid: its icon becomes
  a neighbour's once it is more than halfway over it, and on release it snaps to the nearest switch,
  which is chosen. A tap on a switch glides it there (140 ms); the view is rebuilt after the glide.
  Power is an action, never selected: it has **a short ribbon of its own, with a break before the
  modes' ribbon** (no divider line). **The selector is metal** (owner: the plain dark one "looks like
  a void"): a two-tone face with a sharp step a little below the middle - a polished bevel, from
  LVGL's two-stop gradient - and a fine lighter edge; gunmetal on the dark schemes, silver on Linen;
  rounded squares by default. Icons in the text colour, the chosen one in the accent. **Lit corner
  chips take the same metal.** Looks to compare on glass from a debug-only SETTINGS row, "Deck look":
  square / round, gunmetal / silver.
- **The SETTINGS panel, reworked** (owner, 2026-10-07, K25): one list of rows (`deckSpecs()`) gives
  both its size and its contents. **As wide as its longest row**, its right edge on the window's,
  never narrower than its tab (within a curve's width of the tab it is the tab's width, one straight
  edge, no inner curve). **Never taller than the window**: open, the tab's top stops at the window's
  top; more rows scroll. **Dropdowns** for choices, **a checkbox** for Paused. Rows: Paused; Active
  state (Any / All members are on - groups defined here only); Label (Default / From HA / State /
  Custom / None); Visibility (Show on dashboard / only in group / only as member / Hidden); Tap action
  (Toggle / Details / Members / History view / Cycle scenes / Nothing); Scenes (Visible scenes only /
  Show all / Disabled); and in debug builds, Deck look. Label, Visibility and Tap action are shown
  greyed until saving (2.10d); Custom name joins when Label can be Custom.
- ~~**Scenes and Colour (wide): the slider and the buttons or swatches are one group**, centred~~
  (superseded in round 7 by the slider's place, below). Scenes are **a stack**: one column of
  buttons as wide as the longest name, a second column only when one is full, then scrolling.
  Not yet: switching the control live while dragging (to be measured first), and the inverted-icon
  variant.
- **The slider's place** (owner, round 7, K26 - `placeWide()`): in Scenes, and Color under a tall
  window, the slider stays exactly where the controls put it and the buttons or swatches sit just to
  its right, centred top to bottom in its height. Only what does not fit moves anything, and only as
  far as it must: wider than the room (more scene columns), the slider goes left by the difference;
  running under the clapperboard chip, the content first drops to the bottom of the slider's height,
  then the slider goes left until it clears; and when neither is enough (the 4B's seven scenes: their
  rows fill the height, and two columns do not fit beside the chip even with the slider at the
  edge), the scenes take the columns that fit beside the chip and scroll. The row is moved with
  `translate_x`, so taps land where it is drawn.
- **Four looks, hand-drawn faces** (owner, round 6-7, K27): Black - Square, Black - Round, Silver -
  Square, Silver - Round, all kept; unchosen, Black - Square on Midnight and Fleet, Silver - Round on
  Linen (round 9). **Chosen per scheme** (K31, SETTINGS' "Selector" row, in every build): a choice
  holds for the scheme it was made in, so a Linen page and a Fleet page each keep their own. LVGL's gradient has two stops and runs straight, and the owner asked for the line between
  the selector's light and dark halves to **curve up in the middle** (in the upper two fifths, a
  raised look) and for unlit chips to be **soft dents**, so the faces are ARGB8888 images painted per
  pixel (`paintFace()`): the curved line, faint brushed row streaks, a fine edge, corners
  anti-aliased in the alpha; a dent is shaded at the top, gone by the middle, a whisper of light at
  the bottom, in the window's colour, no border. Painted the first time a size is needed and kept in
  PSRAM (12 slots, ~40-55 KB each on the P4_5), **taken back when full** (round 9): a slot painted in
  another scheme's colours first, else the one asked for longest ago by an earlier window - never
  one the open window may be showing. (Never giving them back filled the cache after a few swipes
  between pages of different schemes, and faces came out flat.) **Dithered to the 16-bit grid** (4x4 ordered): a
  smooth grey ramp truncated to RGB565 banded pink and green, because green steps twice as finely as
  red and blue. The selector's icon is scaled 1.4x (the largest icon face is not big enough); lit
  chips use the next face up. Pressed chips tint.
- **CHART, a second panel** (owner, rounds 7-8, K28 - a demo): **SETTINGS' mirror image**, both
  made by one builder (`buildFolder()`): its tab in the left half, a millimetre short of SETTINGS'
  tab, its pane growing rightwards from the window's left edge, the inner curve on the tab's right.
  Five demo rows (time range, chart style, shading, min/max, compare) that work as controls and
  change nothing, so the pane is wider than its tab on every board. It peeks up when History shows
  and slides down when the window goes to another view. Opening either panel brings it to the front
  and folds the other; a tap in the window folds whichever is open. It leaves with the window as
  SETTINGS does: folded, its tab slides down on its own; open, it goes at once. Both open and fold
  in 260 ms (from 220 - round 7 found them choppier now that SETTINGS opens further; the page deck's
  panels take 300). **Each panel's holder is only as wide as its tab and pane** (round 9, measured on
  the P4_5): a window-wide transparent holder made every frame of a slide redraw the cards and the
  window under it - 35-105 ms a frame; now 8-40.
- **The stacked control deck is centred against the stage itself** (L5, measured on the 1060): a
  content-sized holder was centred inside a flex "track" as wide as the widest child, itself
  centred in the stage, and Color's wider row moved the deck by a pixel of rounding. Its holder is
  now the stage's full width.
- **The title gives way in steps** (owner, round 7, K29): centred when "Area > Name" fits between
  ends of equal width; else it takes the room beside the X (a window with two chips on the right has
  a chip's width spare on the left), left of centre; else the name alone - the back arrow still goes
  up. Sized before the view is built: a DOT label laid out at the last view's width rewrites its own
  text ("Sce..."), and measuring that kept it.
- **No word over the swatches**: Color shows only the swatches (and the PAUSED pill when paused).
  Text on the panels is US English (K30).
- **Scenes hidden in HA's UI are hidden here too** (owner, 2026-10-07, K21): the scene query asks HA's
  `is_hidden_entity` per scene (the entity registry's `hidden_by`). A card setting in SETTINGS,
  "Scenes: Visible / All / Off", Visible by default; live, in RAM until 2.10d. Office: Bright,
  Concentrate and Relax visible, four hidden.

**The corner icons are tabs (owner, after round 3).** History (the chart), Members and Scenes are
views of their own; their icons stay on every view they belong to, the one showing is lit (as a
chosen selector button is), and a tap on a lit one goes back to the controls. Members on the
window's own views; Scenes on the light's controls and scenes; the chart everywhere - **the group's
from the group's views, a member's from that member's own** ("Office lamp > History").

**Back is hard-linked, not a history** (owner, round 4 - the first build had only the X, and the
way back to the controls was to close and reopen). The X is on the controls only; every other view
has the back arrow, and it always goes to the same place: History, Members, Scenes -> the controls;
a member -> Members; a member's History -> that member. **The title's first part is a link up**:
"Desk >" goes to Desk's controls from any view below it, "Office lamp >" to the bulb's. **Members'
rows** are as wide as the controls view's slider and words together, and centred both ways (rounds
2 and 4).

**When a command fails, the board records why** (round 2, G8 - Desk said FAILED once, not
reproduced): no matching report in 3 s, with what was asked and what the light last said, or HA's
refusal and its message. The last four are logged and listed by `/popup?view=3`. A refusal of an
earlier call while a newer one is in flight (a drag) no longer fails the newer one.

## 17. 2.10d - stable card ids and saved settings (in progress, from 2026-10-08)

Decided with the owner on 2026-10-08: `DECISIONS.md` K33-K38. What the problem is, in the owner's
words: how does a device stay customised instead of reverting to its compiled configuration at every
reset?

**How it works.** The board boots from the compiled dashboard, then applies the owner's saved
changes from one file on top. A value comes from the first layer that sets it: the file, the card's
spec, the page's spec, the built-in default (D-7's order). New firmware with a changed dashboard
keeps every saved change for cards that are still there; new cards simply appear. At 3.1 the build
sheet replaces the two spec layers and the file stays on top, unchanged - Q2's runtime layer.

**Ids (K33).** Every saved change says which card it belongs to. Before 2.10d a card was "the Nth
entry of its page", so inserting a card above Desk would have moved Desk's settings onto Overhead.
An id is a name for the card made once, at creation, never changed and never reused - as HA makes
an area's id from its name and keeps it through renames. It is made automatically, so a fresh build
sheet just makes new ones and nobody looks anything up. Format
`<area>_<label>_<yymmdd>_<hhmm>`, e.g. `office_desk_261008_0310`; the label is dropped where it
repeats the area (`kitchen_261008_0310`), a same-minute twin takes a letter (`..._0310b`). The 35
compiled cards were given theirs on 2026-10-08. `checkCardIds()` runs at boot: a card with no id, a
malformed one or a duplicate draws but is never given its id, so it saves nothing.

**A card's life (K35, the owner's page-pool idea).**

| State | Its saved settings |
|---|---|
| Shown | - |
| Dropped because the grid is too small | still on its page; nothing changes |
| Hidden (Visibility - greyed until 2.11) | on its page, in the page's pool, can be brought back |
| Removed from the page (page editing, later) | in the pool, kept |
| Deleted | only by the owner, from the pool; gone |
| No longer in the dashboard definition | kept, listed as "unclaimed", cleared only by the owner |
| Its entity gone from HA | kept, listed as "entity missing" - the board cannot tell "gone" from "HA restarting" or "renamed" |

**The file (K34).** JSON on LittleFS, in the unused data partition. Only what was changed is stored;
choices by name, never by enum number, so reordering an enum cannot change what a saved choice
means; keys this firmware does not know are kept when it rewrites the file. Credentials stay in NVS.
Written whole to a temporary file and renamed, once, when the window closes, only if something
changed - on a task of its own with its stack in internal RAM (LESSONS: a flash write from a task
with a PSRAM stack reboots the board).

```json
{ "schema": 1,
  "schemes":  { "linen": { "selector": "silver_round" } },
  "entities": { "ha_light_office_lamp": { "paused": true } },
  "cards":    { "office_desk_261008_0310": { "label": "custom", "name": "Desk lamp", "scenes": "all" } } }
```

**The flash-write test (step 1, `/panel?flash=`).** On WS_P4_5: NVS writes of 512 B took 2-5 ms,
38 ms when NVS erased a page; 4 KB erases 15-17 ms; 64 KB 32 ms; 256 KB 117-360 ms. The DSI bridge
latched no underrun in any of them - but it masks underruns in a frame's first 413 lines, which is
where a write's late frame start would land, so whether a write is visible takes the owner's eyes.
HomeTiles saw a blue flash on every P4 panel during writes and fixed it by rebuilding three IDF
objects cache-safe (`HomeTiles/tools/esp-idf-3.3.7-p4-cache-safe/README.md`); our libraries have
the same setting off (`CONFIG_LCD_DSI_ISR_CACHE_SAFE`). The owner had paused cards many times
without seeing a flash.

**The owner's white flash (P4_5 only, now and then, no action it follows).** Not yet explained.
`/panel` now lists any underrun the bridge does see, with its time; read it right after a flash.

**Built so far** (WS_P4_5, driven from the PC with `/popup`; nothing yet touched on the glass):

| Step | What | Measured |
|---|---|---|
| 1 | `/panel?flash=` and the underrun recorder (`PanelDebug.h`) | above |
| 2 | Card ids and `checkCardIds()` | "35 cards on 2 pages, 0 id problems"; the failure paths not yet tried |
| 3 | `Settings.h`: the file, `GET /settings` (`?stats=1`); Active state, Scenes and Selector kept | mount 52 ms the first time, 3-11 ms after; a save 7-66 ms (66 when LittleFS erased a block), once per close; Desk's Scenes and a Midnight selector back after a reset; internal heap -6.6 KB |
| 3b | Paused in the file (`EntityRegistry::PauseStore`), #60's NVS list imported once | Garage North paused, PAUSED after a reset, resumed and the entry gone |
| 4 | Label: Inherit / From HA / State / Custom / None (K40); `Entity::sourceName`; custom names from a PC (`/settings?card=..&name=..`) | after a reset: Desk "From HA" -> "Office", Overhead "Ceiling lamp", Sink "State" -> "Off" |
| 5 | Tap action (K37, K39): Toggle / Details / Members / History / Cycle scenes / Load scene / Nothing, only what the card can do; a Tap scene row, greyed unless Load scene; `CardPopup::openOn()`; the toast "Scene: Desk - Bright" | Desk History -> History, Overhead Details -> window, North -> nothing; Cycle x4 = Bright, Concentrate, Relax, Bright (HA's last-activated times), hidden scenes untouched; Load Relax -> Relax |

Active state and Scenes are read every time a page is built, so they also survive a page swipe -
before 2.10d a swipe lost them.

**The flash writes flashed the panel** (owner, on glass): all three `/panel?flash=` tests, more with
the longer erases, light blue. Fixed by the cache-safe library rebuild (`REBUILD_P4_LIBS.md`, "The
second rebuild"), installed 2026-10-08; the owner's look at the same tests afterwards is pending.

**A lost setting, once.** One boot started with an empty store although the file was there, and the
next save replaced it (Desk's Scenes). Not explained; a file that will not read now makes the boot
read-only instead, and `/settings?stats=1` says what the boot saw.

Next: Area as a card setting (K41), then K17.
