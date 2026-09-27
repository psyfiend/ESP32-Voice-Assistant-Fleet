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
| 4.4 | Deck panels declared per page (mechanism B), built from the same rows | - |

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
