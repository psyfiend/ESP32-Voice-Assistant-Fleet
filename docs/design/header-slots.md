# The page header and its slots

How the bar across the top of every page works: what stands in it, how it decides what fits, what a
tap on it opens, and how to add to it. The kit pieces it is drawn with are described in
`components.md` ("The page header", "A drawer", "The halo"); its sizes and status colours in
`tokens.md` §8.

| Code | Owns |
|---|---|
| `include/UI/HeaderSlots.h`, `src/UI/HeaderSlots.cpp` | the slots as data: `SlotKind`, `SlotSpec`, each kind's keeps, `HEADER_DEFAULT`, `GlyphStyle` |
| `include/Layout/HeaderPlan.h`, `src/Layout/HeaderPlan.cpp` | the planner: where each slot stands and in which form (pure C++, `host-tests/test/test_header_plan`) |
| `include/UI/Panel_Header.h`, `src/UI/Panel_Header.cpp` | the bar: each kind's build and update, measuring, planning, the open tab |
| `include/UI/SlotPanels.h`, `src/UI/SlotPanels.cpp` | what each slot's panel holds (all but the status slot's) |
| `src/UI/Panel_System.cpp` | the status slot's panel, the system panel |
| `src/GUIManagerHeader.cpp` | opening and closing panels, a page's header shown / hidden, the glyph style setting, `/popup`'s header keys |
| `src/UI/Kit/KitHeader.cpp`, `KitDrawer.cpp`, `KitGlyph.cpp`, `KitHalo.cpp` | the bar, a slot, a glyph's frame; the drawer; stroke glyphs; the halo |
| `include/UI/GlyphShapes.h`, `src/UI/GlyphShapes.cpp` | the bar's glyphs as strokes on a 24-unit grid (`host-tests/test/test_glyph_shapes`) |
| `src/Cards/ClusterCard.cpp`, `src/Cards/Popup/PopupGroup.cpp` | a group's header, planned by the same planner, and its sensors' panel in the group view (§8) |

## 1. The header is optional

A page may hide its header (§9), so **nothing critical lives only in the header** (D-11). Anything
that matters also has a home on a card or, once alerts exist, in the notice queue (#76). The owner:

> "we should try to avoid placing anything that is ultra important or critical info in the header
> bar where it could be ignored or overlooked."

The clock is a card first; the clock in the header is a convenience beside it. The device's name is
not in the bar - "About this board" shows it.

## 2. Three places

The bar has three **places** (D-16):

| Place | Stands | Holds by default |
|---|---|---|
| left | pinned to the bar's left end | the clock (with its bell, §5) and the outdoor temperature |
| middle | on the bar's centre, whatever the sides hold | the page: its name and dots |
| right | pinned to the bar's right end | the alarm's notice, the access point's mast, the status glyphs |

A **slot** is one thing standing in a place. In each place, slots stand left to right in list
order. A slot is as wide as what it shows now, measured on the board.

## 3. Slots are data

```cpp
struct SlotSpec { SlotKind kind; HeaderPlace place; const char *entity; };
```

`SlotKind` is `SLOT_CLOCK`, `SLOT_PAGE`, `SLOT_STATUS`, `SLOT_SENSOR`, `SLOT_ALARM`, `SLOT_AP`; only
`SLOT_SENSOR` uses `entity` (an `EntityDescriptor::id`). A page's header is
`PageSpec::headerSlots` / `headerSlotCount`, or `HEADER_DEFAULT` when that is null - which every
page uses today:

| # | Kind | Place | Entity |
|---|---|---|---|
| 1 | clock | left | |
| 2 | sensor | left | `outdoor_temp` - HA's `sensor.outdoor_temperature` (K127), on no card |
| 3 | page | middle | |
| 4 | alarm | right | |
| 5 | ap | right | |
| 6 | status | right | |

At most `HEADER_SLOTS_MAX` (8) slots. `Panel_Header::setPage()` rebuilds the slots only when the
page's list is a different one; otherwise it updates the page slot alone.

## 4. The planner: claims and keeps

The owner's model of the header (D-19), which the planner implements:

> "Each slot would have default or baseline residents, and other entities would be more or less
> 'permanently' attached to certain slots but only show under certain circumstances."

**Each slot makes a claim on its place's room.** A claim has up to three **forms**, fullest first;
each step down sheds one **extra** (the clock's bell, the sensor's icon, the page's name). Each form
holds its room with a **keep**, a priority: `keep[k]` is how hard form k resists stepping down, and
the last form's keep is how hard the slot resists leaving the bar. `KEEP_ALWAYS` (255) never gives
way - a **resident**. A claim whose condition does not hold (`wanted` false: no alarm due, no access
point up) takes no room at all.

The keeps, fleet-wide (`HeaderSlots.cpp`):

| Kind | Form 0 | keep | Form 1 | keep |
|---|---|---|---|---|
| clock | time and bell | 15 | time | resident |
| page | name and dots | 30 | dots | resident |
| status | link and Wi-Fi glyphs | resident | | |
| sensor | icon and value | 20 | value | 60, then it leaves |
| alarm | ringing bell | 210, then it leaves | | |
| ap | mast | 220, then it leaves | | |

**How `Layout::planHeader()` decides** (`HeaderBar`: the bar's width, `EDGE` from each end, `SPACE`
between slots in a place, at least `CLEAR` between a side and the middle):

1. Every wanted claim starts in form 0.
2. If a side reaches within `CLEAR` of the middle (or, with no middle, of the other side), the
   claim with the lowest keep **among the overflowing side's and the middle's** steps down one form;
   from its last form it leaves the bar. Ties go to the later claim in the list. Residents never
   step.
3. Repeat until everything fits. If a side still meets the middle with nothing left that may give
   way, the bar is drawn as planned, `planHeader()` returns false and the board logs `[UI] header: a
   side meets the middle with nothing left to give way` once.

The middle never moves; shedding the page's name only narrows it. **A narrow board sheds by the same
rule**, measured on the board - there is no per-board header layout. With every claim present, a
narrowing bar loses the bell first, then the sensor's icon, the page's name, the temperature, the
alarm's notice, the mast - but only claims on the side that overflows, and the middle, are ever
asked.

`Panel_Header::claimOf()` measures each form from the slot's live objects (its pads, the children it
always shows, the extras that form keeps, `SLOT_GAP` between). An extra that is switched off
(`extraOn` false - the bell with no alarm this week) takes no room in any form.

**Cheap to keep.** `Panel_Header::tick()` asks every slot every 250 ms (a sensor at most once a
second). Each update keeps a `key` of what it shows and redraws only when the key changes; a redraw
marks the bar for planning, and the bar is planned again only then. The clock redraws once a
minute, a sensor when its value moves; the status glyphs throttle themselves and never change width.
The System Doctor's `[UI STATE]` prints each slot's redraws and form, how often the bar was planned
and whether it fitted.

## 5. The slot kinds

| Kind | Shows | Glyph and colour | Its panel |
|---|---|---|---|
| clock | the time, 12-hour, am / pm small beside it, no seconds ("6:56am"); `--:--` until the time is known. The bell beside it while an alarm is due within a week | bell: `TEXT` | the clock's |
| page | the page's name and a dot per page (this one larger and filled); no dots for one page; past 12 pages, "3 / 15" in place of dots | | the page's |
| status | the HA / MQTT link glyph, then Wi-Fi at the bar's edge - each its own widget (`Widget_MqttStatus`, `Widget_ConnStatus`), each failing independently | `SIG_*` by state; a strike means switched off on purpose, never broken | the system panel |
| sensor | its entity's icon and value as its card draws them, a temperature to the whole degree (`72°`) | the card's tint while live, `TEXT_DIM` otherwise | the sensor's |
| alarm | a ringing bell while the next alarm is within the warning time and not dismissed (D-21) | `SIG_WEAK` (amber: a notice, not a fault) | the alarm's |
| ap | the mast while the board hosts its setup access point (D-24) | `SIG_WEAK` | the system panel |

**No live value shows dashes, never an old one** (D-15) - the owner: "For sensor data showing an old
value is worse than no value". The sensor slot shows `--°` while its entity has never reported this
boot, and `--°` with the error mark ("!" on an `ST_WARN` disc, `KS_HDR_MARK`) when its source says
unavailable or the value has gone stale. An entity the board does not know shows `--°` and a
thermometer.

**The alarm is two claims from one alarm** (D-21): the bell inside the clock's slot while the next
alarm is within a week (`Alarms::bell()`), and the separate notice among the status icons within the
warning time (`Alarms::warning()`; 12 hours unless set). An unset alarm is not missing data, so with
none set neither shows (D-20). The board has no real alarms yet (K84): only a pretend one set from a
PC (`/popup?alarm=`, §9) brings either onto the bar.

## 6. Panels

**A tap on a slot slides down that slot's own panel from its place** (D-13). The panels are for
settings, shortcuts to settings, or windows that manage what the slot shows (D-17); the owner on the
system panel's buttons:

> "That panel will ultimately provide settings and/or shortcuts to settings or popups to manage the
> various statuses. Same for all of the header slots."

| Slot | Panel holds |
|---|---|
| clock | the time large and the date; **NEXT ALARM** - its time and how long until it, or "None set"; "Warn me before" 1 h / 3 h / 6 h / 12 h / Off (`system.alarm.warn`) |
| page | **PAGES** - every page, this one marked, a tap goes there; **HEADER** - Show / Hide for this page, with a note how to bring it back while hidden; **CARDS** - Edit cards and New group (K148), each opening its window over the page; the page's hidden cards are listed and restored in Edit cards |
| sensor | its name, its reading or dashes, and why: when it last reported, "No reading yet", "Unavailable at its source", "No report for ..." |
| alarm | **ALARM SOON** - its time and how long until it; Dismiss (until the next alarm comes due); Settings, which opens the clock's panel |
| status, ap | the system panel: **CONNECTIVITY** (Wi-Fi, Home Assistant, MQTT - a glyph and its state in words, refreshed each second while open), then **SYSTEM - DIAGNOSTICS** (Log, UI Tweaks, About; Scheme; the page's Grid) - development tools for now (D-17) |

The dim helper notes in the panels (`Kit::drawerNote()`) hide when UI Tweaks' **Show helper text**
is off (`system.ui.helper`).

**The mechanism** (`Kit::Drawer`). The slots other than status share one drawer, `_slotDrawer`,
emptied and rebuilt by `SlotPanels::build()` each time one opens; `GUIManager::openSlotPanel()`
fills the `SlotPanels::Context` with what the panel needs and what its choices do. The system panel
is its own drawer, at its own width rule (half a wide screen, three quarters of a middling one, all
of a narrow one); a slot's drawer takes the same rule capped at `DRAWER_W`. A drawer:

- is placed centred under its slot, kept `EDGE` from the screen's sides; at an edge it shares that
  side with its slot's tab, and that top corner is square;
- grows to its contents (at most the screen below the bar) in 350 ms ease-out, its face riding the
  growing foot; a line inside it can appear or go with the same slide (`Kit::drawerReveal()`);
- **lies on the bar** (D-35): its top edge is the bar's accent line, every corner rounded, and the
  bar's line fades out over `LINE_FADE` (2.5 mm) just short of its walls;
- **is one shape with its slot** (D-23): the open slot (`LV_STATE_CHECKED`, `KS_HDR_SLOT_OPEN`) is
  drawn in the panel's face with an accent edge on three sides, covering the bar's line, with a cove
  (`JOIN_R`) each side where the panel reaches past it. The slot stays open until its panel has
  finished closing (D-30, `Drawer::onClosed`);
- is shaded only where it meets the bar - the halo of the panel's top, its tab and coves, drawn by
  the bar (K108). Windows have the halo all round. Its strength is UI Tweaks' per scheme
  (`schemes.<name>.halo`), else 25%. On a scheme whose bar casts a shadow (Linen), the bar paints
  each open panel's face back over that shadow (D-32).

The bar is drawn over the panels: `Kit::drawerOverBar()` runs in its `DRAW_MAIN_END` (the line's
fade, the pane's top edge, the shadow cover) and its `DRAW_POST` (the shading).

**One panel at a time.** Opening a slot's panel closes the system panel and any deck panel; opening
another slot's replaces it. A panel closes on:

- a second tap on its slot;
- a tap anywhere outside it;
- a swipe up from anywhere, or a sideways swipe (which then does not turn the page);
- the window timeout (`Kit::windowCloseMs()`, `system.window.close`: 60 s unless set, or never),
  counted from the last touch anywhere on the screen (D-22);
- its slot's claim leaving the bar (an alarm passing);
- a choice in it that opens something else: a page, Edit cards, New group, the clock's panel.

## 7. Glyph styles

Every glyph in the bar - the status glyphs, both bells, the mast, the sensor's icon - is framed in
one **glyph style**, device-wide (`GlyphStyle`); words and numbers never are:

| Style | Look |
|---|---|
| `GLYPH_BARE` | the glyph alone |
| `GLYPH_SQUARE` (default) | a rounded square behind it: `FRAME` 28, `FRAME_R` 7, `FRAME_MIX` 14% of `TEXT` over the bar |
| `GLYPH_CIRCLE` | a circle in the glyph's state colour, the glyph drawn in the bar's colour (`SURFACE_ALT`) |

**A frame never shrinks a glyph** (D-24): the glyph is `GLYPH` (26) bare or framed, the frame round
it. The style is chosen in UI Tweaks' **Glyphs** row (D-25) and saved as `system.header.glyphs`;
group headers take it too (§8). Changing it rebuilds the slots.

The bar's glyphs are numbers (D-27): `Glyphs::Shape` is `WIFI`, `LINK`, `AP_MAST`, `BELL`,
`BELL_RING`, each stroke an arc, a line or a dot on a 24-unit grid, drawn by `Kit::strokeGlyph()`
with LVGL's arcs and lines, one object per stroke so each part takes its own colour. The sensor's
icon is its card's MDI glyph. The state colours are `UIPalette`'s `SIG_*` (`tokens.md` §8). The bar's
words are `UIToolkit::Font_Header`, sized per board in `gen_type_scale.py`.

## 8. Group headers reuse the planner

A **solo card's header has no slots** (D-14): its card label and STALE keep their places. A group
shown on the page has a one-line header planned by `Layout::planHeader()`
(`ClusterCard::layoutHeader()`):

- **the group's name** (with its colour swatch in Swatch style) is a left resident at its full
  width, so values give way before the name is cut; the name is then ellipsised to what the values
  leave;
- **up to four sensor values** (`CLUSTER_SLOTS_MAX`, chosen in the group's Header options, K129;
  on the page only its pinned header members, K160) stand on the right in one order everywhere: from
  the right, temperature, occupancy, light level, then the rest (K115). Each value is a claim - its
  icon an extra (keep `60 - 10r`), then the value itself (keep `120 - 10r`), `r` its rank from the
  right - so the leftmost gives way first;
- a binary sensor (occupancy) shows its state's own icon and no words (K101); "Show sensor icons"
  off drops the other icons;
- a value with no live reading shows `--` (D-15), with no error mark;
- in Swatch style the icons take the page header's glyph style (K115), using the
  `KS_CLUSTER_GLYPH_*` roles; in Band style they stand bare on the group's colour.

**On the page** the header's values are not tappable on their own: a tap on a group is its tap
action (its group view by default, K154), a long press its window. **In the group view** (K163) a
tap on the header's sensors drops a panel under the header, on the right: a row per sensor - its
glyph, HA's full name, its state - and a row opens that sensor's History. It slides as the drawers
do (350 ms); a second tap on the sensors, or a tap anywhere else in the window, folds it first. A tap
on the group's name opens Members.

Its sizes are `UIShape`'s `C_` millimetres (`C_PAD` edge, `C_VALUE_GAP` between and clear). The
group's design and its settings: `group-cards.md` and `ha-cards.md`.

## 9. Showing and hiding, saved settings, HTTP

**A page's header shown or hidden** is a page setting (D-14): `PageSpec::headerShown` is the
compiled default (true on every page today), and the owner's choice from the page panel is saved as
`pages.<slug>.header` - `shown` or `hidden`, stored only when it differs from the default. Hiding
or showing it rebuilds the page and keeps its arrangement (K167): the cards keep their size and the
height the header frees goes round them. **The header stays while any header panel is open**
(D-30): a Show / Hide chosen in the page panel takes effect once the panel has closed.

**Bringing a hidden header back for a moment** (the peek): a swipe down from the top edge on the
screen's right half shows the bar for 3 s, drawn over the page without a rebuild, so the cards do not
move; a second swipe down while it shows opens the system panel. A tap, a swipe up or a sideways
swipe puts it away; while a header panel is open the peek waits. A swipe down from the top edge on
the left half opens UI Tweaks, header or not.

| Setting | Values | Unset means |
|---|---|---|
| `pages.<slug>.header` | `shown`, `hidden` | the page's `headerShown` |
| `system.header.glyphs` | `bare`, `circle` | square |
| `system.alarm.warn` | hours (1-48), `off` | 12 |
| `system.window.close` | seconds, `off` | 60 |
| `system.ui.helper` | `off` | helper notes shown |
| `schemes.<name>.halo` | 0-100 | 25 |
| a group card's `slots`, `cl_style`, `cl_icons`; a page's `cl_style` | see `HTTP_REFERENCE.md` `/settings` | |

**From a PC** (`docs/HTTP_REFERENCE.md` has the full list). `/settings` sets the group header keys.
`/popup` (only in `-D DEBUG_POPUP` builds) drives the header:

| Address | Does |
|---|---|
| `/popup?slot=<kind>` / `slot=-1` | a tap on that kind's slot (`clock`, `page`, `sensor`, `alarm`, `status`, `ap`) / closes any panel |
| `/popup?drawer=1\|0` | opens / closes the system panel |
| `/popup?pagecard=1` | the page panel's Edit cards (`slot=page` first) |
| `/popup?glyphs=0\|1\|2` / `glyphs=3` | bare / square / circle, not saved / the saved style again |
| `/popup?pagehdr=0\|1` | this page's header hidden / shown - saved |
| `/popup?alarm=HHMM` / `alarm=-1`, `alarmin=N` | a pretend next alarm (RAM only, nothing rings) / cleared; N minutes from now |
| `/popup?sensorerr=1\|0` | the sensor slot shown failed (RAM only) / as its entity says |
| `/popup?drop=1` | a tap on the group view's header sensors: their panel drops or folds |

## 10. Adding a slot kind

1. Add `SLOT_<NAME>` to `SlotKind` before `SLOT_KIND_COUNT` - prefixed, since Arduino's macros eat
   single ALL-CAPS words.
2. Add its row to `KINDS` in `HeaderSlots.cpp`: its name (what `/popup?slot=` and the System Doctor
   use), its forms and a keep per form. Choose keeps against the table in §4.
3. Write `build<Name>()` and `update<Name>()` in `Panel_Header.cpp`, declare them in
   `Panel_Header.h`, and add the pair to `ops()`'s table. Build its objects in `s.box` from kit roles;
   list what it may shed in `s.extra[]` / `s.nExtra`, in shedding order. In the update, compute a
   `key` of what it shows, return if unchanged, else call `redrawn(s)` and redraw; set `s.wanted`
   for a claim with a condition and `s.extraOn[]` for an extra that is sometimes absent.
4. Give it a panel: a case in `SlotPanels::build()`, with any new `Context` fields filled in
   `GUIManager::openSlotPanel()` - or route its tap to the system panel in `GUIManager::onSlotTap()`.
5. Put it in `HEADER_DEFAULT` or in a page's `headerSlots`.

Nothing else changes: no layout code in the bar, and the planner already handles any claim. A change
to the planner itself gets a test in `host-tests/test/test_header_plan`.

## 11. Not built yet

- **Alerts as header claims** (#76, milestone 2.14; D-19, K138): a detailed form in the middle for
  a while, then a minimal "!" in status until acknowledged or cleared; a place that grows for a
  high-priority claim; a window taking the middle while it is open; claims bringing their own rows
  to their panel. The notice queue itself exists for info notices (K142, `UIToolkit::show_notice()`)
  and shows them as toasts, apart from the header.
- **Count and alert slots** from HA sensors - lights on, doors open (K119), each opening to what it
  counts (K134).
- **Real alarms** (K84, milestone 2.15): setting them in the clock's panel; timers.
- **Choosing the sensor slot's entity** on the board; a forecast in its panel; its panel opening the
  sensor's History.
- **Deep links** from panels to settings pages (D-13; 4.1, #23).
- **The clock's 24-hour form and seconds.**
- **A page with its header hidden by default** - the mechanism exists (`headerShown` false); no
  page uses it.
- **Per-page header slots** - the mechanism exists (`PageSpec::headerSlots`); no page sets them.

## 12. Why not

- **Four evenly spaced slots** (clock, page, status, climate - each a quarter of the bar). Built and
  seen on glass; the owner preferred the status icons on the right and the page in the true middle,
  and slots sized to what they show leave the planner to decide what fits.
- **The alarm's bell among the status icons.** The alarm is a clock function, so its bell sits in
  the clock's slot; only the near-term notice joins the status icons (D-21).
- **A tap on a slot opening its entity's window.** A slot opens its own panel, which can hold more
  than one entity's window would (D-13).
- **A Status bar toggle on the system panel.** The page panel's Show / Hide is the one way to hide
  the header (D-30).
- **The panel joined to the bar with rounded joins or flares.** The owner disliked the corners where
  the panel met the bar's line; the panel lies on the bar instead (D-35).
- **A halo below the bar on header panels.** It made the slide look chunky at any strength; the
  shading where a panel meets the bar is enough (K108).
- **LVGL's own shadow for the halo.** It banded on these 16-bit panels and cost far more per frame
  than painted, dithered images (`components.md`, "The halo").
- **A peek that rebuilds the dashboard.** The grid would re-plan and the cards jump every time
  someone glanced at the clock; the peeked bar is drawn over the page instead.
- **Slots in a solo card's header.** A solo card's header stays clean: each place on it already
  means one thing (D-14).
