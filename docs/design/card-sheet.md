# The card popup — long press, milestones 2.10 / 2.11 — BLUEPRINT (draft 2)

**Status: design, 2026-09-27, owner and Claude. Nothing built.** Draft 1 (a bottom sheet) was
replaced the same morning by the owner's brain dump, with HA's own dialogs as the reference
(light, switch, switch history, temperature graph). Sources: ROADMAP 2.10 (#65), 2.11 (#66), 2.8;
`context-panels.md` (mechanism A = invoked by a card, B = declared by a page); `pages.md` (linked
pages, one-deep back); `cards.md`. Open decisions are marked **D1..D7** and collected in §10, each
with a recommendation.

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
