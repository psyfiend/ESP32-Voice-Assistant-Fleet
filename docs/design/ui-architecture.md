# UI architecture - how the screen is built, and how to change it safely

**Read this before any UI work.** It is the map as of `v0.2.18` (2026-10-17, milestone 2.8, the
header bar v2), built on the foundation milestone 2.12 laid at `v0.2.13` without changing a pixel
(`ui-foundation.md` is that milestone's record). The rules here are load-bearing; CLAUDE.md states each one briefly, this file says where
each lives and how to work with it. What things look like: `components.md` (the kit, with pictures)
and `tokens.md` (colours, sizes, type). What the card window is, view by view: `card-sheet.md`
§14-18.

## 1. The layers

Each layer uses the ones below it and never reaches up. Lower layers include no LVGL header.

| Layer | What it is | Where |
|---|---|---|
| Hardware and services | display, touch, Wi-Fi, MQTT, Home Assistant, the HTTP server, the saved settings; started in one documented order | `SystemCore`, `components/Fleet_*`, `Settings` - **no LVGL** |
| Entities | every light, sensor and switch the board knows, from HA, MQTT and virtual sources, behind one mutex; commands go out through it | `components/Fleet_Entities` (`EntityRegistry`), `components/Fleet_Providers`, `components/Fleet_HA` (`HaClient`, `HaHistory`) - **no LVGL** |
| The LVGL engine | buffers, `lv_init()`, the flush to the panel, touch input | `LVGL_Startup`, `LVGL_Flush_*` |
| The screen | the root screen, the header and its slots' panels (2.8), the pages, the system panel and its pages, UI Tweaks' choices (2.8 round 3) | `GUIManager` (+ `GUIManagerHeader.cpp`, `GUIManagerTweaks.cpp`), `src/UI/Panel_*`, `UI/HeaderSlots` + `UI/SlotPanels` + `UI/GlyphShapes`, `LogPage`, `ReferencePage` |
| Pages | a grid of cards built from data (`PageSpec`, the dashboards in `include/Dashboards/`); where each card goes is a **layout engine's** decision | `CardPage` + `Layout::` (`include/Layout/`) |
| Cards | a card per entity or group, its type from the entity's kind; bound to its entities so it redraws when they change; "on" shown by its card state (2.8) | `src/Cards/` (`Card`, `CardFrame` - the active frames, `CardCatalog`, `CardBinder`, `StateCard`, `ValueCard`) |
| Windows | everything that opens over the page is a **Sheet**: the card window, "About this board", "UI Tweaks" | `Kit::Sheet` (`include/UI/Sheet.h`); clients `CardPopup` + `src/Cards/Popup/`, `UI/AboutSheet`, `UI/TweaksSheet` |
| Components | buttons, dialogs, chip rows and chip tables, list items, panel rows, folder panels, drawers, the header's bar and slots, the selector, icon buttons, label roles; the finishes (satin, glass, lit glass) | `Kit::` (`include/UI/Kit.h`, `src/UI/Kit/`) |
| Tokens | every colour, size, font and shape | `UI::` (`include/UI/UITokens.h`, `src/UI/UITokens.cpp`) |

**Threads.** LVGL runs only on `loop()` (`LV_OS_NONE`). Providers, the HA websocket task and HTTP
handlers never touch LVGL; an HTTP handler asks the UI thread and waits (`UI/Screenshot.cpp` is the
pattern; `docs/HTTP_REFERENCE.md` lists every route). Anything that writes flash runs on a task
whose stack is in internal RAM - the settings task.

## 2. Where things live

| You want... | Look in |
|---|---|
| a colour, a size, a font, a radius | `include/UI/UITokens.h` (`UIPalette`, `UIMetrics`, `UIShape`, `UIGrid`, `UIType`); values in `src/UI/UITokens.cpp`; why: `tokens.md` |
| a component and its looks | `include/UI/Kit.h`; `src/UI/Kit/Kit<Name>.cpp`; the roles' styles in `KitStyles.cpp`; pictures in `components.md` |
| the window frame | `include/UI/Sheet.h`, `src/UI/Kit/KitSheet.cpp` (frame, auto-close, the tap catcher), `KitSheetPanels.cpp` (the two folder panels), `KitSparks.cpp` |
| the card window's contents | `src/Cards/CardPopup.cpp` (the client: tabs, views, open/close) and `src/Cards/Popup/` - one file per concern, sharing `PopupState.h`: controls and the light hero (`PopupControls`, `PopupLight`, `PopupDeck`), `PopupScenes`, `PopupMembers`, History (`PopupHistory`, `...Chart`, `...Activity`), `PopupProps`, `PopupCardUi` (Card UI, 2.8 round 3), a cluster's window `PopupCluster` (2.11, K104), Visibility, Add to a cluster and "which group?" `PopupVisibility` (2.11, K96), the panels' rows (`PopupSettingsRows`, `PopupSideRows`, `PopupPanels`), `PopupDialog` (Hide, Remove this copy), the hold (`PopupHold`), helpers (`PopupCommon`), and the PC's handles (`PopupDebug`, `PopupTree`) |
| typing words (a name) | `Kit::keyboard()`, `src/UI/Kit/KitKeyboard.cpp` (2.11, K95); `components.md` "Keyboard" |
| a card hidden from its page, a copy in a cluster | `Cards/CardAdded` (`hide()`, `restore()`, `pool()`; `addTo()`, `membersIn()`); the page's list in `src/UI/SlotPanels.cpp`, filled in `GUIManagerHeader.cpp` |
| where cards go on a page | `include/Layout/Layout.h` (`FlowEngine`), `src/Cards/CardPage.cpp`; the engines to come: `layout-engines.md` |
| a group card, a cluster and its mini tiles (2.11) | the spec: `CardSpec::members` / `present` / `slots` (`PageSpec.h`), the House page's in `Dashboard_HA.h`; built by `CardPage::makeGroupCard()`; drawn by `Cards/ClusterCard` and `Cards/MiniTile.cpp`; the arithmetic `include/Layout/Cluster.h` (host-tested); `group-cards.md` §6-7 |
| the arranger (#78, K105) | `include/Layout/Arranger.h` (pure, host-tested against the playground), `Cards/PageArranger` (kept answers, the background run), `CardPage::arrangePage()`; `layout-engines.md` "As built" |
| the halo round panels and windows (D-35) | `Kit::haloAttach()`, `src/UI/Kit/KitHalo.cpp`; the panel on the bar `Kit::drawerOverBar()` in `KitDrawer.cpp` |
| what the header shows and where | `include/UI/HeaderSlots.h` (the slots as data, each kind's keeps), `include/Layout/HeaderPlan.h` (the planner: three places, claims, keep), `src/UI/Panel_Header.cpp` (each kind's build and update), `src/UI/SlotPanels.cpp` (their panels), `include/UI/GlyphShapes.h` (the status glyphs as strokes, round 3); `header-slots.md` §10-12 |
| UI Tweaks and the system panel's choices | `src/GUIManagerTweaks.cpp` (each row: what it sets, where it is saved; the scheme; the grid's counts; the deck under the window), `src/UI/TweaksSheet.cpp` (the window), `src/UI/Panel_System.cpp` (CONNECTIVITY, SYSTEM - DIAGNOSTICS) |
| a card's "on" look | `src/Cards/StateCard.cpp` (`render()` picks the state: Fill, Icon glow, Bracket, Corner cut), `src/Cards/CardFrame.cpp` (`Card::activeFrame()`: the frames drawn in draw events; the corner cut's corners as cached images with LVGL's own shadow, measured - D-33) |
| which card a kind of entity gets | `src/Cards/CardCatalog.cpp` (`cardForKind()`) |
| what a dashboard contains | `include/Dashboards/Dashboard_HA.h`, `Dashboard_Fleet.h` (`PageSpec`, `CardSpec`; ids per K33) |
| the owner's saved choices | `include/Settings.h` (one JSON file), `CardGroups`, `CardAdded`; CLAUDE.md "Saved settings" |
| times and durations in words | `include/TimeFormat.h` (pure; tested in `host-tests/`) |

## 3. The rules that hold it together

- **No colour or size literal outside the tokens.** A colour is `UI::c(UI::pal().X)`; chrome uses
  `UI::paint()`, which follows a scheme change; never cache a colour. Logical-pixel tokens go through
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
  auto-close (60 s), the quiet build, the tap catcher and both folder panels are the Sheet's. One
  sheet shows at a time, and its tap catcher takes every touch outside it - which is also what
  keeps page swipes from happening under a window. `GUIManager`'s swipe handler has a second check
  for the card window only (`CardPopup::isOpen()`); `Kit::sheetShowing()` would make it cover any
  sheet.
- **Placement is an engine's.** A page never places cards itself: it gives `Layout::Item`s to a
  `Layout::Engine` and lays out the `Place`s that come back. A new arrangement is a new engine,
  pure, with host tests before it draws anything.
- **Usable first, nothing moves** (the window, `card-sheet.md` §14-15): complete in its first frame,
  closed in one; within one window nothing jumps. **Snappy first, eye candy second** (W6): measure
  a frame before and after anything visual (`DEBUG_FRAMES`, `/bench`).
- **Light state lives in the registry**; a command is confirmed only by a matching report. A group
  acts as HA's light group does; a paused member is out of its group.
- **Anything drawn stays ASCII, except the degree sign**; words on screen are US English.
- **LVGL traps that cost days** (LESSONS): a child overflowing its parent forces a layer (it froze
  the P4_5); an opacity below cover on a container makes one; LVGL draws in strips and skips a child
  whose parent is outside the strip; never read from a DSI panel that shows a picture (it blanks
  it); keep moving objects tight (a wide transparent holder redraws everything under a slide); the
  hand-drawn faces' cache must give its slots back. From 2.8: **an object's `DRAW_POST` is clipped
  to its own area** - draw outside it in `DRAW_MAIN_END`; anti-aliased triangles that share an
  edge leave a seam; an image meant to match LVGL's own drawing must blend as LVGL does, in 16 bits.

## 4. How to...

- **Add a look** - a role: a `KitStyle` value in `Kit.h`, its styles filled in `KitStyles.cpp`'s
  `fill()` from tokens, a row and a picture in `components.md`.
- **Add a component** - a function in `Kit::` (its own `src/UI/Kit/Kit<Name>.cpp` when it is more
  than a few lines) that builds exactly the objects, flags and events its callers need, keeps its
  state in the caller's struct (found through the object's user data), and uses roles.
- **Add a window** - a client of `Kit::Sheet`: a `SheetSpec`, a state struct that extends
  `Kit::Sheet` if it needs one, its views; `UI/AboutSheet.cpp` (~130 lines) is the smallest
  example, `CardPopup.cpp` the full one.
- **Add a view to the card window** - a `PopupView` value (`PopupState.h`), its builder in its own
  `Popup<Name>.cpp`, a case in `showView()`; a corner tab through the `SheetSpec` if it has one; a
  `/popup?view=` number and a state in `scripts/ui_golden.py` so the harness walks it.
- **Add a card type** - a `Card` subclass and a case in `cardForKind()`; the kinds without a card
  today are number, text, climate and weather (they are skipped and reported). Cards are the part
  of the UI not yet built from the kit (section 6): the first new type should start that.
- **Add a layout** - a class implementing `Layout::Engine` with a suite in
  `host-tests/test/test_<name>/` (`layout-engines.md`).
- **Add an HTTP route** - `http.addRoute()` from the owner of the feature (never touching LVGL in
  the handler), and a section in `docs/HTTP_REFERENCE.md` in the same commit.

## 5. How a change is proved

| Tool | What it proves | How |
|---|---|---|
| **The harness**, `scripts/ui_golden.py` | a window, view, panel or page is the same object tree and the same pixels as before | `capture <host> --board <B> --label <L>` walks 62 states (the system panel and About since #97; the header's slots, UI Tweaks, Card UI and the active frames since 2.8; fewer on the 4B, whose own 10 s window timeout cuts the walk - `--only`) through `GET /popup` and saves each tree and screenshot; `compare <B> <A> <B>` reports every difference, telling the house apart (times, values, a light switched); `measure` times each window's build; `crop` cuts a picture for the docs. Needs `-D DEBUG_POPUP` |
| **A/B on one board** | a refactor changed nothing | capture on the old firmware, flash the new, capture again minutes apart; a state the house changed under is captured again both ways. Keep each phase's image (`test/golden/<board>/fw_*`, gitignored) to flash back |
| **Host tests**, `host-tests/` | the pure modules' rules (`Layout`, the header's planner `HeaderPlan`, `TimeFormat`) | `wsl bash host-tests/run_in_wsl.sh` (or `pio test -d host-tests` with a Windows compiler) |
| **The structure check**, `scripts/structure_check.py` | the code is not drifting: big files, style calls outside the kit, colour literals, shape numbers, LVGL in the core layers | run it with the gate; quote it in the merge report (W13) |
| **The gate**, `scripts/release/gate.ps1` | all nine boards compile from the committed `platformio.ini` | ~15 minutes; it stashes and restores the owner's local debug lines. **Once, right before a merge** - never per test round (the owner) |
| **The owner's sheet**, `docs/TEST_<milestone>.md` | touch, feel, schemes, the auto-close, a reboot - what no script sees | numbered tests with PASS criteria, the owner's results under them |

The harness's rules: never two runs at one board (each turns the sparks off and back on); the 1060
is slow (6-8 s a state) and fails its walk if its screenshots slow further - capture it alone.

## 6. Not on the foundation yet

2.12 rebuilt the card window. The rest of the UI still sets its looks at the call site; the
structure check counts it (531 `lv_obj_set_style_*` calls outside the kit in 26 files at `v0.2.13`;
**438 in 24 at `v0.2.18`**). Bring each onto the kit and tokens when it is next changed, not in a
sweep:

- **Cards** - **on the kit since 2.11 round 1** for their parts' looks (`KS_CARD_*`: the wrapper,
  surface, body, tag, badge, disc, name; `components.md`), proved by the harness with no pixel moved;
  what follows a card's state stays local. `Card.cpp` is 1,448 lines and 43 style calls (was 1,468
  and 87), `StateCard.cpp` 30, `ValueCard.cpp` 13. **Clusters** (`Cards/ClusterCard`) and **mini
  tiles** (`Cards/MiniTile.cpp`) were built on the roles from the start. The active frames left
  `Card.cpp` for `CardFrame.cpp` in 2.8.
- **`GUIManager.cpp`** (1,490 lines) - the second file over the 1,000-line mark; 2.8 put its header
  and UI Tweaks work in `GUIManagerHeader.cpp` and `GUIManagerTweaks.cpp`.
- **The header is on the kit since 2.8** (`Kit::headerBar()`, `headerSlot()`, `glyphFrame()`,
  `Kit::Drawer` for every slot's panel - the system panel's frame too since round 2; `UI::hdr()`
  sizes); **where its slots stand is a pure planner**, `Layout/HeaderPlan.h` (claims and keeps,
  D-19; host-tested). Still to come: the system panel's rows of buttons and its pages
  (`Panel_System`, `Panel_Display`, `Panel_Audio`, `LogPage`, `ReferencePage`), `UIToolkit`, the
  status widgets (whose 34 colour literals are most of what the check reports).

## 7. What 2.11 will ask of this structure

The finishes (#99, `v0.2.16`) and the follow-ups (`v0.2.15`) are done, and 2.8 left no look
question open. **Group cards (2.11)** are the next pressure on it: a cluster is a container inside
an item, placed by an engine run on its own sub-grid (`layout-engines.md`); its header is meant to
be the page header's slots and planner again (`cards.md` §8, `Layout/HeaderPlan.h`); and its
members are cards without their own borders - which is where the cards should come onto the kit.
