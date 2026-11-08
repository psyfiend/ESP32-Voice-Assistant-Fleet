# UI foundation - the layers and the rules behind them

Why the UI is built as tokens, a component kit, layout engines and Sheets, and what each layer owns.
**How the UI is built, where things live and how to change it** is `ui-architecture.md`; this file
is the reasoning behind that structure. The owner's goal: modular, portable, data in and the engine
draws, so that "change the corner radius once and every window, card and button follows".

## 1. What is a system, and what is still local

**A system (build on it):**
- **Tokens** (`include/UI/UITokens.h`, `tokens.md`): colors (live scheme switching through
  `UI::paint()`), the type scale, icon faces, `UI::minTouch()`, per-board scale from real pixel
  density, and the card window's shape and spacing in millimetres (`UIShape`). Nothing outside
  `UITokens.cpp` names a color.
- **Pages are data**: `PageSpec` / `CardSpec` (`include/Cards/PageSpec.h`) plus the cards made on
  the board (`CardAdded`), placed by `CardPage` through a `Layout::Engine`.
- **Panel rows are data**: SETTINGS, CHART, ACTIVITY and SCENES are each a table of `DeckRowSpec`
  (label, kind, choices, selected, live, callback); one folder builder builds any of them.
- **Settings** are a layered file of named choices (`include/Settings.h`).
- **Pure modules with no LVGL** (`Layout`, `TimeFormat`, `HeaderPlan`, `HaHistory`'s parsing) are
  tested off the board in `host-tests/`.

**Still local** (`ui-architecture.md` §6): the cards, the system panel's pages and the status widgets
set their looks with `lv_obj_set_style_*` at the call; `scripts/structure_check.py` counts them.

## 2. The layers

Four layers, each with one job and a rule at each boundary that `grep` can check:

| Layer | Owns | Lives in | The rule |
|---|---|---|---|
| **Tokens** | values: color, type, icon faces, radius and spacing scales, control heights | `UITokens` | no color, size or radius literal outside it |
| **Components** | things: Button (by role), Chip / ChipRow, Selector (the sliding switch), Checkbox, Dropdown, Row, Panel (folder) from a row table, Dialog from a spec, Label roles, the card's parts | `include/UI/Kit.h`, `src/UI/Kit/` | only components set `lv_obj_set_style_*`; above them, code asks for a component **by role**, never by look; one shared `lv_style_t` per role, refilled on a scheme change like `UI::paint()` |
| **Layouts** | arrangement: an engine turns items into placements | `include/Layout/`, `src/Layout/` (pure C++, no LVGL) | items are `{id, min/pref span, priority, pin}`; engines know nothing about cards |
| **Content** | what is shown: page specs, card specs, view specs, row tables, dialog specs | specs and builders per feature | content code never styles; it fills specs |

Above them, **features** (the card window, history, groups, Edit cards) compose content from
components.

The card window is a **Sheet** (`include/UI/Sheet.h`): the frame (header with title and link, corner
buttons, the two panels with their drag, the countdown bar, the tap catcher, the "usable first"
build) takes a list of views and panels from a `SheetSpec`. The card window, "About this board",
UI Tweaks, Edit cards and New group are clients. A **Dialog** is `{title, paragraphs[],
actions[{label, role, callback}]}`: a confirmation is one call.

The test of the structure: *the same content data, plugged into another layout engine, just works.*
Cards size themselves from the cell they are given (compact and full variants, stepped icon faces,
the name rule in K69), and that property must be kept.

## 3. What must not change

These were earned over many rounds on glass:

- **Usable first**: the window is complete in its first frame; the build times printed by
  `DEBUG_POPUP` must not get worse.
- **Nothing moves** inside one window (`card-sheet.md` §14-15).
- The LVGL traps in LESSONS: a child overflowing its parent forces a layer (and froze the P4_5); an
  opacity below cover on a container makes a layer; LVGL draws in strips and skips a child whose
  parent is outside the strip.
- The settings file's keys and the card ids (K33, K34).
- Everything in CLAUDE.md.

## 4. Proving a refactor changed nothing

`GET /popup?tree=1|page` (needs `-D DEBUG_POPUP`) prints every object of the open window or the page:
class, place, size, flags, state, events, text and the style as LVGL resolves it. `scripts/ui_golden.py`
walks a fixed list of states through `/popup`, saving each tree and a screenshot, and `compare`
reports every difference, telling the house apart: digits and times are masked, a light switched
between captures marks its window "capture again", and a card on a page whose words changed is data.
A/B on one board: capture the old firmware, flash the new, capture again minutes apart. Never run
two harness commands at one board (each turns the sparks off for its run and back on at its end).
Details: `ui-architecture.md` §5.

A change to window build time or LVGL's pool per window is measured with `measure` and `mem=1`
before it is believed.

## 5. The structure check

`scripts/structure_check.py` (W13) runs with every all-nine gate and never fails a build. It reports
files over 1,000 lines, `lv_obj_set_style_*` outside the kit, color literals outside the tokens,
numbers where a shape token belongs, and LVGL included in `SystemCore` / `SystemReport`, skipping
the generated font faces and headers, `third_party/` and `lv_conf.h`. The merge report quotes its
output, so drift is seen at 1,000 lines.

## 6. Known quirks

- `rowLive()` in `src/Cards/Popup/PopupSettingsRows.cpp` is unused.
- A card span of 0 counts as one whole cell when the page counts the rows it wants
  (`Layout::rowsWanted()`) and as one unit when it places (`FlowEngine`). No card sets 0
  (`CardPlacement` defaults to 2), so it shows nowhere.
- The kit's styles follow a scheme change live; a window's own local colors do not. A window open
  across a scheme change would show both, a path the device cannot take (the window is modal).
- The kit's button roles are CANCEL and DESTRUCTIVE; PRIMARY and QUIET arrive with their first user.
- `host-tests/` runs with `wsl bash host-tests/run_in_wsl.sh`; `pio test -d host-tests` needs a
  Windows C++ compiler (MSYS2 or WinLibs, `host-tests/README.md`).
- A board in the ROM bootloader after an interrupted flash keeps a dark screen until it is started
  with `--after watchdog-reset` (LESSONS).

## 7. Designed for, not built

The owner's ideas that shape the structure; see `layout-engines.md` for the engines and
`group-cards.md` for groups:

- many windows in one family (settings pages, Visibility, moving a card), built as Sheet clients;
- groups, group cards and clusters (built), group pages (K118, not built);
- bento layouts and an arranger (built, `layout-engines.md`); a domain weight map, a page with a
  primary region and orbital pages (proposals);
- special multi-cell cards: weather station, media player, thermostat controls, clock, camera
  (K81-K83);
- the sliding selector reused for History's span chips (built, K75).
