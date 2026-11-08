# UI architecture - how the screen is built, and how to change it safely

**Read this before any UI work.** It is the map of the UI as built. The rules here are load-bearing;
CLAUDE.md states each one briefly, this file says where each lives and how to work with it. What
things look like: `components.md` (the kit, with pictures) and `tokens.md` (colors, sizes, type). What
the card window is, view by view: `card-sheet.md` §14-18. The foundation the kit and tokens stand on
is `ui-foundation.md`.

## 1. The layers

Each layer uses the ones below it and never reaches up. Lower layers include no LVGL header.

| Layer | What it is | Where |
|---|---|---|
| Hardware and services | display, touch, Wi-Fi, MQTT, Home Assistant, the HTTP server, the saved settings; started in one documented order | `SystemCore`, `components/Fleet_*`, `Settings` - **no LVGL** |
| Entities | every light, sensor and switch the board knows, from HA, MQTT and virtual sources, behind one mutex; commands go out through it | `components/Fleet_Entities` (`EntityRegistry`), `components/Fleet_Providers`, `components/Fleet_HA` (`HaClient`, `HaHistory`) - **no LVGL** |
| The LVGL engine | buffers, `lv_init()`, the flush to the panel, touch input | `LVGL_Startup`, `LVGL_Flush_*` |
| The screen | the root screen, the header and its slots' panels, the pages, the system panel and its pages, UI Tweaks' choices | `GUIManager` (+ `GUIManagerHeader.cpp`, `GUIManagerTweaks.cpp`), `src/UI/Panel_*`, `UI/HeaderSlots` + `UI/SlotPanels` + `UI/GlyphShapes`, `LogPage`, `ReferencePage` |
| Pages | a grid of cards built from data - the cards made on the board, kept in the settings file (`CardAdded`), merged with what `include/Dashboards/` compiles (Garage - Doors, the Panel group, K171); where each card goes is a **layout engine's** decision | `CardPage` + `Layout::` (`include/Layout/`) |
| Cards | a card per entity or group, its type from the entity's kind; bound to its entities so it redraws when they change; "on" shown by its card state | `src/Cards/` (`Card`, `CardFrame` - the active frames, `CardCatalog`, `CardBinder`, `StateCard`, `ValueCard`) |
| Windows | everything that opens over the page is a **Sheet**: the card window (and its group view, a *bare* sheet), "About this board", "UI Tweaks", Edit cards, New group, the hidden cards | `Kit::Sheet` (`include/UI/Sheet.h`); clients `CardPopup` + `src/Cards/Popup/`, `UI/AboutSheet`, `UI/TweaksSheet`, `UI/EditCardsSheet`, `UI/NewClusterSheet`, `UI/HiddenSheet` |
| Components | buttons, dialogs, chip rows and chip tables, list items, panel rows, folder panels, drawers, the header's bar and slots, the selector, icon buttons, label roles; the finishes (satin, glass, lit glass) | `Kit::` (`include/UI/Kit.h`, `src/UI/Kit/`) |
| Tokens | every color, size, font and shape | `UI::` (`include/UI/UITokens.h`, `src/UI/UITokens.cpp`) |

**Threads.** LVGL runs only on `loop()` (`LV_OS_NONE`). Providers, the HA websocket task and HTTP
handlers never touch LVGL; an HTTP handler asks the UI thread and waits (`UI/Screenshot.cpp` is the
pattern; `docs/HTTP_REFERENCE.md` lists every route). Anything that writes flash runs on a task
whose stack is in internal RAM - the settings task.

## 2. Where things live

| You want... | Look in |
|---|---|
| a color, a size, a font, a radius | `include/UI/UITokens.h` (`UIPalette`, `UIMetrics`, `UIShape`, `UIGrid`, `UIType`); values in `src/UI/UITokens.cpp`; why: `tokens.md` |
| a component and its looks | `include/UI/Kit.h`; `src/UI/Kit/Kit<Name>.cpp`; the roles' styles in `KitStyles.cpp`; pictures in `components.md` |
| the window frame | `include/UI/Sheet.h`, `src/UI/Kit/KitSheet.cpp` (frame, auto-close, the tap catcher), `KitSheetPanels.cpp` (the two folder panels), `KitSparks.cpp` |
| the card window's contents | `src/Cards/CardPopup.cpp` (the client: tabs, views, open/close) and `src/Cards/Popup/` - one file per concern, sharing `PopupState.h`: controls and the light hero (`PopupControls`, `PopupLight`, `PopupDeck`), `PopupScenes`, `PopupMembers`, History (`PopupHistory`, `...Chart`, `...Activity`), `PopupProps`, `PopupCardUi` (Card UI), a group's window and its GROUP panel - Header options, Edit cards, Face, Area; every change asked first (K165) - `PopupCluster`, the group view `PopupGroup` (K154: the group's own cluster in a bare sheet), the moves' views and dialogs `PopupMove`, Show as `PopupShowAs` (K191), the thermostat's read-only window `PopupClimate` (K144), Visibility `PopupVisibility` (Pin / Unpin this card, K160), the panels' rows (`PopupSettingsRows`, `PopupSideRows`, `PopupPanels`), `PopupDialog`, the hold (`PopupHold`), helpers (`PopupCommon`), and the PC's handles (`PopupDebug`, `PopupTree`) |
| typing words (a name) | `Kit::keyboard()`, `src/UI/Kit/KitKeyboard.cpp` (K95); `components.md` "Keyboard" |
| adding, hiding, pinning or removing cards | `UI/EditCards` (the one list: In this group, On the page, Hidden, HA's areas; the tap cycle - a sensor being added has a third tap into the header, K168; `Spec::saveWord`, `Spec::haGlyph`), `UI/EditCardsSheet` (from the page), `Popup/PopupCluster.cpp` (from a group), `UI/HaOffer` + `UI/HaPicker` (what HA offers, "N new"), `UI/NewClusterSheet` (New group, K168); a group header's glyph `cardHeaderGlyph()` in `Cards/CardIcons`; `ha-cards.md` §4 |
| a card hidden from its page, a copy or move into a group | `Cards/CardAdded` (`hide()`, `restore()`, `pool()`; `addTo()`, `membersIn()`, `moveBack()`, `emptied()`); the page's hidden cards are Edit cards' Hidden list; the limit `CardAdded::MAX` 128 and its warning `UIToolkit::show_sticky()` (K176); `CardAdded::specAt()` lets `CardGroups` read the made cards' areas; `CardAdded::resetToCompiled()` deletes every made card - the whole House page - and is reached only from the PC (`/popup?dash=`), the system panel having no Reset button (K192) |
| where cards go on a page | `include/Layout/Layout.h` (`FlowEngine`), `src/Cards/CardPage.cpp`; the arranger (below); the engines: `layout-engines.md` |
| a group card, a cluster and its mini tiles | the spec: `CardSpec::members` / `present` / `slots` (`PageSpec.h`) - compiled only for Garage - Doors and Panel (K171); a group made on the board is a `CardAdded` record (`kind`: `cluster`, its members `in` it); built by `CardPage::makeGroupCard()` (`src/Cards/CardPageGroups.cpp`); drawn by `Cards/ClusterCard` and `Cards/MiniTile.cpp`; the arithmetic `include/Layout/Cluster.h` (host-tested); `group-cards.md` §3 |
| the arranger (K105) | `include/Layout/Arranger.h` (pure, host-tested against the playground), `Cards/PageArranger` (kept answers, the background run), `CardPage::arrangePage()`; `layout-engines.md` "As built" |
| the halo round panels and windows (D-35) | `Kit::haloAttach()`, `src/UI/Kit/KitHalo.cpp`; the panel on the bar `Kit::drawerOverBar()` in `KitDrawer.cpp`; a bare sheet lets its client's shadow out (`Kit::sheetBare()`, `UI::shadowReachPx()`) |
| what the header shows and where | `include/UI/HeaderSlots.h` (the slots as data, each kind's keeps), `include/Layout/HeaderPlan.h` (the planner: three places, claims, keep), `src/UI/Panel_Header.cpp` (each kind's build and update), `src/UI/SlotPanels.cpp` (their panels), `include/UI/GlyphShapes.h` (the status glyphs as strokes); `header-slots.md` §3-4 |
| UI Tweaks and the system panel's choices | `src/GUIManagerTweaks.cpp` (each row: what it sets, where it is saved; the scheme; the grid's counts; the deck under the window), `src/UI/TweaksSheet.cpp` (the window), `src/UI/Panel_System.cpp` (CONNECTIVITY, SYSTEM - DIAGNOSTICS) |
| a card's "on" look | `src/Cards/StateCard.cpp` (`render()` picks the state: Fill, Icon glow, Bracket, Corner cut), `src/Cards/CardFrame.cpp` (`Card::activeFrame()`: the frames drawn in draw events; the corner cut's corners as cached images with LVGL's own shadow, measured to match it) |
| which card a kind of entity gets | `src/Cards/CardCatalog.cpp` (`cardForKind()`) |
| what a dashboard contains | **each board's settings file** (`CardAdded` records); compiled: `include/Dashboards/Dashboard_HA.h` (Garage - Doors), `Dashboard_Fleet.h` (the built-in Panel group, `CardSpec::builtIn`); the retired specs stay in those headers under `#if 0`; `scripts/release/retire_compiled.py`, `test_cards.json` |
| the owner's saved choices | `include/Settings.h` (one JSON file), `CardGroups`, `CardAdded`; CLAUDE.md "Saved settings" |
| times and durations in words | `include/TimeFormat.h` (pure; tested in `host-tests/`) |

## 3. The rules that hold it together

- **No color or size literal outside the tokens.** A color is `UI::c(UI::pal().X)`; chrome uses
  `UI::paint()`, which follows a scheme change; never cache a color. Logical-pixel tokens go through
  `UI::sc()`; **the card window's sizes are `UIShape` millimetres** - `mm(UI::shape().S5)`, or
  `pm()` for the `P_` group (P4_5 millimetres, a share of the window). A new size is a new token
  with a comment and a row in `tokens.md` §7.
- **No `lv_obj_set_style_*` for a look at the call site: a look is a role.** A component takes a
  role; a role is shared `lv_style_t`s in `KitStyles.cpp`, refilled on every repaint
  (`UI::onRepaint()`). The caller places the component and owns what changes - a value, a ring, a
  greyed state - as local properties, which beat the shared style. One style per part and state
  (`KS_X`, `KS_X_PRESSED`); two shared styles on one object are ordered by when they were added.
- **A window is a Sheet client.** It gives a `Kit::SheetSpec` - its tabs, its first view, a tick,
  its closing hooks, its panels' rows - and builds its views into `s.stage`; the frame, the header,
  auto-close, the quiet build, the tap catcher and both folder panels are the Sheet's. A **bare**
  sheet (`Kit::sheetBare(S, true, radius)`, the group view) has no header row and no padding, a clear
  window with the client's corners, its stage the window itself, and lets the client's shadow out;
  `Kit::sheetResized()` keeps the title and countdown bar in step when a client moves or resizes its
  window. One sheet shows at a time, and its tap catcher takes every touch outside it - which is also
  what keeps page swipes from happening under a window. `GUIManager`'s swipe handler stands down
  while the card window or any sheet is open (`CardPopup::isOpen() || Kit::sheetShowing()`).
- **Placement is an engine's.** A page never places cards itself: it gives `Layout::Item`s to a
  `Layout::Engine` and lays out the `Place`s that come back. A new arrangement is a new engine,
  pure, with host tests before it draws anything.
- **Usable first, nothing moves** (the window, `card-sheet.md` §14-15): complete in its first frame,
  closed in one; within one window nothing jumps. **Snappy first, eye candy second** (W6): measure
  a frame before and after anything visual (`DEBUG_FRAMES`, `/bench`).
- **An option never moves**: it stays in its panel in every state; one that does not apply is a
  greyed row (`ChipRowSpec::greyed`), never moved or removed (K155). Where an option lives is a
  design choice (K174).
- **Light state lives in the registry**; a command is confirmed only by a matching report. A group
  acts as HA's light group does; a paused member is out of its group.
- **Anything drawn stays ASCII, except the degree sign**; words on screen are US English. On screen
  the word is **group** (K191); `ClusterCard` and "cluster" are the names in code.
- **Text fits its room** through `Kit::fitLabel` (K152); "Show helper text" is the kit's
  (`Kit::helperShown()`).
- **LVGL traps that cost days** (LESSONS): a child overflowing its parent forces a layer (it froze
  the P4_5); an opacity below cover on a container makes one; LVGL draws in strips and skips a child
  whose parent is outside the strip; never read from a DSI panel that shows a picture (it blanks
  it); keep moving objects tight (a wide transparent holder redraws everything under a slide); the
  hand-drawn faces' cache must give its slots back; **an object's `DRAW_POST` is clipped to its own
  area** - draw outside it in `DRAW_MAIN_END`; anti-aliased triangles that share an edge leave a
  seam; an image meant to match LVGL's own drawing must blend as LVGL does, in 16 bits.

## 4. How to...

- **Add a look** - a role: a `KitStyle` value in `Kit.h`, its styles filled in `KitStyles.cpp`'s
  `fill()` from tokens, a row and a picture in `components.md`.
- **Add a component** - a function in `Kit::` (its own `src/UI/Kit/Kit<Name>.cpp` when it is more
  than a few lines) that builds exactly the objects, flags and events its callers need, keeps its
  state in the caller's struct (found through the object's user data), and uses roles.
- **Add a window** - a client of `Kit::Sheet`: a `SheetSpec`, a state struct that extends
  `Kit::Sheet` if it needs one, its views; `UI/AboutSheet.cpp` is the smallest example,
  `CardPopup.cpp` the full one.
- **Add a view to the card window** - a `PopupView` value (`PopupState.h`), its builder in its own
  `Popup<Name>.cpp`, a case in `showView()`; a corner tab through the `SheetSpec` if it has one; a
  `/popup?view=` number and a state in `scripts/ui_golden.py` so the harness walks it.
- **Add a card type** - a `Card` subclass and a case in `cardForKind()`; the kinds without a card
  today are number, text and weather (they are skipped and reported); a thermostat has a read-only
  card (K144). Cards are the part of the UI not yet built from the kit (section 6): the first new
  type should start that.
- **Add a layout** - a class implementing `Layout::Engine` with a suite in
  `host-tests/test/test_<name>/` (`layout-engines.md`).
- **Add an HTTP route** - `http.addRoute()` from the owner of the feature (never touching LVGL in
  the handler), and a section in `docs/HTTP_REFERENCE.md` in the same commit.

## 5. How a change is proved

| Tool | What it proves | How |
|---|---|---|
| **The harness**, `scripts/ui_golden.py` | a window, view, panel or page is the same object tree and the same pixels as before | `capture <host> --board <B> --label <L>` walks a fixed list of states (the system panel, About, the header's slots, UI Tweaks, Card UI, the active frames; fewer on the 4B, whose own 10 s window timeout cuts the walk - `--only`) through `GET /popup` and saves each tree and screenshot; `compare <B> <A> <B>` reports every difference, telling the house apart (times, values, a light switched); `measure` times each window's build; `crop` cuts a picture for the docs. Needs `-D DEBUG_POPUP` |
| **A/B on one board** | a refactor changed nothing | capture on the old firmware, flash the new, capture again minutes apart; a state the house changed under is captured again both ways. Keep each phase's image (`test/golden/<board>/fw_*`, gitignored) to flash back |
| **Host tests**, `host-tests/` | the pure modules' rules (`Layout`, the header's planner `HeaderPlan`, `TimeFormat`) | `wsl bash host-tests/run_in_wsl.sh` (or `pio test -d host-tests` with a Windows compiler) |
| **The structure check**, `scripts/structure_check.py` | the code is not drifting: big files, style calls outside the kit, color literals, shape numbers, LVGL in the core layers | run it with the gate; quote it in the merge report (W13) |
| **The gate**, `scripts/release/gate.ps1` | all nine boards compile from the committed `platformio.ini` | ~15 minutes; it stashes and restores the owner's local debug lines. **Once, right before a merge** (W5) |
| **The owner's sheet**, `docs/TEST_<milestone>.md` | touch, feel, schemes, the auto-close, a reboot - what no script sees | numbered tests with PASS criteria, the owner's results under them |

The harness's rules: never two runs at one board (each turns the sparks off and back on); the 1060
is slow (6-8 s a state) and fails its walk if its screenshots slow further - capture it alone.

## 6. Not on the foundation yet

The card window, the header, clusters and mini tiles are built on the kit and tokens. The rest of
the UI still sets its looks at the call site; `scripts/structure_check.py` counts it (files over
1,000 lines, `lv_obj_set_style_*` calls outside the kit and where, color literals). Bring each onto
the kit and tokens when it is next changed, not in a sweep:

- **Cards** - the parts' looks are on the kit (`KS_CARD_*`: the wrapper, surface, body, tag, badge,
  disc, name; `components.md`); what follows a card's state stays local. `Card.cpp` is the largest
  file and holds most of the remaining style calls, then `StateCard.cpp` and `ValueCard.cpp`.
  Clusters (`Cards/ClusterCard`) and mini tiles (`Cards/MiniTile.cpp`) are built on the roles. The
  active frames live in `CardFrame.cpp`.
- **`GUIManager.cpp`** - over the 1,000-line mark; the header and UI Tweaks work is in
  `GUIManagerHeader.cpp` and `GUIManagerTweaks.cpp`.
- **The header** is on the kit (`Kit::headerBar()`, `headerSlot()`, `glyphFrame()`, `Kit::Drawer` for
  every slot's panel and the system panel's frame; `UI::hdr()` sizes); where its slots stand is a
  pure planner, `Layout/HeaderPlan.h` (claims and keeps, D-19; host-tested). Still to come: the
  system panel's rows of buttons and its pages (`Panel_System`, `Panel_Display`, `Panel_Audio`,
  `LogPage`, `ReferencePage`), `UIToolkit`, the status widgets (whose color literals are most of what
  the check reports).

## 7. What the structure carries

A group card is a container inside a layout item, placed by the arranger on its own sub-grid
(`layout-engines.md`); its header is the page header's planner and claims again (`cards.md` §8,
`Layout/HeaderPlan.h`); its members are borderless mini tiles on roles; its window is a Sheet client
like the card's (K129). The group view is the group's own `ClusterCard` in a bare sheet, its tiles
capped by `Layout::cappedGrid` (host-tested). Edit cards is one list component (`UI/EditCards`) used
by three clients: the page, a group, New group. The arranger runs when the page's content changes,
not when its height does (K167; `PageArranger.cpp`, kept answers `v5`); a member's own view has its
own settings (`settingsSubject()`, K166).

Not built: an HA group entity as a group on the board (K170, K184), two-level nesting (K147), units
in UI Tweaks (K177), thermostat controls (K144 is read-only), summary cards beyond the compiled
Garage - Doors (K189). What is left open is in ROADMAP's "Phase 2 summary" and
`carried-from-2.13.md`. Alerts (2.14) will build on the header's claim model (K138).
