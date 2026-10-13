# UI foundation - the refactor before 2.8 (milestone 2.12, proposed 2026-10-13)

**Status: PROPOSED, the owner's to approve phase by phase.** Written by Claude at the end of K17,
from the owner's questions about how portable and data-driven the UI code is. This file is the
brief for the session that does the work: read all of it before touching code.

---

## 1. Why: what we found (measured 2026-10-13, at `v0.2.12`)

The owner looked at the "Remove this card?" dialog and asked whether it came from a reusable
"window with buttons", and whether changing, say, the corner radius once would change it
everywhere. The honest answer was: partly.

**What is already a system (keep it, build on it):**
- **Tokens** (`include/UI/UITokens.h`, `docs/design/tokens.md`): colours (live scheme switching
  through `UI::paint()`), the type scale, icon faces, `UI::minTouch()`, per-board scale from real
  pixel density. Nothing outside `UITokens.cpp` names a colour.
- **Pages are data**: `PageSpec` / `CardSpec` (`include/Cards/PageSpec.h`), placed by
  `CardPage` (units, spans, priority, flow). K17's added cards are more data merged into the same
  list (`CardAdded`).
- **Panel rows are data**: SETTINGS, CHART, ACTIVITY and SCENES are each a table of `DeckRowSpec`
  (label, kind, choices, selected, live, callback); one `buildFolder()` builds any of them;
  `deckSync()` rebuilds when the rows change. This is the model to grow.
- **Settings** are a layered file of named choices (`include/Settings.h`), already in the build
  sheet's shape.
- Pure modules with no LVGL (`TimeFormat`, `HaHistory`'s parsing) - testable off the board.

**What is not:**
- **`src/Cards/CardPopup.cpp` is 6,325 lines in one file** - the window frame, the hold and tap
  catcher, five views (controls, members, history, scenes, properties), the control deck and its
  hand-drawn faces, the panels and their drag, the history chart and activity, the Remove dialog
  and the `/popup` debug driver - all sharing one file-static state struct `s` (and `h`, `s_hv`,
  `s_chart`...). Its good builders (`buildFolder()`, `deckButton()`, `histChips()`, the dialog)
  are private to the file and tied to that state.
- **Shape and spacing are half tokenised**: 132 literal `mm(...)` values in that file; of its 28
  `set_style_radius` calls, 13 use the radius token and 15 are local numbers (chips 3 mm, deck
  buttons 1.2 mm, dialog buttons 1.6 mm...). Change the radius token today and cards and the
  window follow; chips, deck buttons and the dialog do not.
- **Styles are local** (`lv_obj_set_style_*` per object) almost everywhere in the popup; every
  object carries its own style properties in LVGL's pool, and a role cannot be restyled at once.
- `Card.cpp` (1,461 lines) and `GUIManager.cpp` (1,533) are large but cohesive; they are second.
- There is one layout engine (CardPage's flow), welded to the page.

Colour and type are a system; shape and components are halfway; layout is data with one engine.
The owner's goal - modular, portable, data in and the engine draws - is the target. Nobody caught
the drift because each 2.10 round added a little to the one file; the owner has asked that we stay
on top of structure from here on.

## 2. Where the UI is going (the owner's ideas - design for them, do not build them)

Everything below shapes the structure; none of it is in scope for this milestone.

- **Many windows**: configuration windows for clusters, group cards and group pages (2.11), a
  device settings page (4.1: popup timeout and countdown bar, device-wide), Visibility (K68),
  moving a card to another group (K48, K67). They must look like one family by construction.
- **Groups** (K46, K67): device-native groups under the card label; group cards (Desk, All
  Lamps); **clusters** - one bordered card larger than 1x1 holding member cards *without their own
  borders*, denser than a cell each; "open" a group card as a cluster, a window of ordinary cards,
  a page of the members, or the Members view (`docs/design/group-cards.md` §5).
- **Bento layouts**: tiles of different sizes on one grid. The page already works in sub-grid
  units (`PageSpec.subdivision`). Clusters get their own sub-grid; a **domain weight map** gives
  each kind of entity its importance and size (media player 2x2, thermostat 2x1, light 1x1...),
  overridable per card through the usual layers (D-7). Packing without holes: the owner found
  "tetris" / greedy packing; our grids are tiny (at most ~12x8 units), so an exhaustive or
  backtracking search is affordable inside a cluster - **but stability beats optimality**:
  "nothing moves" (W6, card-sheet §15) - a deterministic engine that prefers the previous
  arrangement, so a small change never reshuffles a room.
- **Special multi-cell cards**: weather station, media player, thermostat controls, clock, camera.
- **A page with a primary region** (~1/3 of the screen) for one large element - a media player,
  a weather station, a cluster of hotlinks, or the room the board stands in - and the rest filled
  with clusters and cards.
- **Alternative arrangements**: even hub-and-spoke / orbital instead of a grid. The test of the
  structure: *the same content data, plugged into another layout engine, just works.* Cards
  already size themselves from the cell they are given (compact / full variants, stepped icon
  faces, the K69 name rule) - that property is what makes it possible, and must be kept.
- **The sliding selector** (the light controls' switch) reused for the chart's span chips (#92).
- Consistency: "change the corner radius once and every popup, card and button follows."

## 3. The architecture to aim for

Four layers, each with one job, and a rule at each boundary that can be checked by `grep`:

| Layer | Owns | Lives in (proposed) | The rule |
|---|---|---|---|
| **Tokens** | values: colour, type, icon faces, **radius scale, spacing scale, control heights**, motion durations | `UITokens` (extended) | no colour, size or radius literal outside it (today: colour only) |
| **Components** | things: Button (by role), Chip / ChipRow, Selector (the sliding switch), Checkbox, Dropdown, Row, Panel (folder) from a row table, Dialog from a spec, Label roles (title / body / caption), card frame, disc | `src/UI/Kit/` + `include/UI/Kit/` | only components set `lv_obj_set_style_*`; above them, code asks for a component **by role** (primary, quiet, destructive), never by look; shared `lv_style_t` per role, repainted on scheme change like `UI::paint()` |
| **Layouts** | arrangement: an engine turns items into placements | `src/Layout/` (pure C++, no LVGL) | items are `{id, min/pref/max span, weight, priority, pin}`; engines know nothing about cards; today's flow is the first engine |
| **Content** | what is shown: page specs, card specs, view specs, row tables, dialog specs | specs and builders per feature | content code never styles; it fills specs |

Above them, **features** (the card window, history, groups) compose content from components.

The card window becomes a **Sheet**: the frame (header with title and link, corner buttons, the
two panels with their drag, auto-close bar, tap catcher, "usable first" build) taking a list of
views and panels. The card popup is its first client; a cluster's configuration window is the
next. A **Dialog** is `{title, paragraphs[], actions[{label, role, callback}]}` - the Remove
dialog becomes one call.

**What must not change** (they are the owner's, earned over 2.10's rounds): every pixel the owner
signed off; "usable first" (the window complete in its first frame; build-time numbers printed by
`DEBUG_POPUP` must not get worse); "nothing moves"; the LVGL traps in LESSONS (a child overflowing
its parent forces a layer and froze the P4_5; an opacity below cover on a container makes a layer;
LVGL draws in strips and skips a child whose parent is outside the strip - K64); the settings file's
keys and card ids (K33, K34); everything in CLAUDE.md.

## 4. The roadmap - phases, each its own commit(s), each verified before the next

**P0 - The safety net (first, before any code moves).**
- A **tree dump**: `GET /popup?tree=1` (DEBUG_POPUP) prints the open window's LVGL object tree -
  per object: type, depth, coordinates, size, visible, and text (label text, dropdown options) -
  as one line each. Geometry is deterministic where screenshots are not (temperatures, times and
  history data change minute to minute). Also `GET /popup?tree=page` for the dashboard page.
- A **PC harness** (`scripts/ui_golden.py`, using the PlatformIO Python): drives a fixed list of
  states through `/popup` on a board - dashboard (both pages), each card type's window, each view
  (controls, members, a member's own, history chart and activity, scenes, properties), each panel
  open, the Remove dialog (`removecard=2` on an added card - add one with `addcard=1` and remove
  it after) - and saves for each state the tree dump and a `/screenshot` PNG under
  `test/golden/<board>/<state>.{txt,png}` (gitignored unless the owner wants them committed).
  `--compare` diffs the trees (text masked: digits and times) and reports every object whose
  type or geometry changed; the PNGs are for looking at (an agent can read them).
- **Capture the baseline from `v0.2.12`** on the P4_5, the 4B and the 1060 before P1 starts.
  Also record: `lv_mem` used after opening a window (the `/popup` output prints it), the window
  build time, and the boot log's free heap.
- Exit: the harness runs end to end on all three boards twice with no differences between runs
  (fix the masking until it is quiet).

**P1 - Split `CardPopup.cpp` by concern. Mechanical: no logic changes.**
- One internal header for the shared window state (`src/Cards/Popup/PopupState.h`, not public).
- Files of ~300-900 lines each, along the seams the section headers already mark: frame and
  open/close; hold and tap catcher; controls (light slider, toggle, sensor hero, the column);
  the control deck and the hand-drawn faces; scenes view; members and a member's own view;
  history (chart; activity; the top line); properties; the panels (folder builder, drag, carets);
  SETTINGS rows and callbacks; CHART / ACTIVITY / SCENES rows; the dialog; the `/popup` debug
  driver.
- `CardPopup.h` stays the public interface, unchanged.
- Exit: P4_5 and WS_S3_4B build; the harness reports **no tree differences** on all three boards;
  build time and `lv_mem` unchanged.

**P2 - Shape and spacing tokens. Zero visual change.**
- Add to `UIMetrics`: a radius scale (e.g. chip, control, surface, window), a spacing scale in
  millimetres (S1-S6, whatever the existing literals cluster into - measure them first), named
  control heights (row, control, chip), and any repeated durations.
- Replace the literals with tokens whose values reproduce today's look exactly. Where two
  literals are almost the same (1.2 vs 1.6 mm), keep both values for now and list them in the
  hand-off as candidates to merge - **the owner decides**, not the refactor.
- Document the scales in `docs/design/tokens.md`; add the rule to CLAUDE.md.
- Exit: no tree differences; `grep -c 'mm([0-9.]*f)'` in the popup files near zero (what remains
  is commented with why).

**P3 - The component kit.**
- Extract, generalise and document (`docs/design/components.md`: each component, its roles, the
  tokens it reads, a screenshot): Button (roles: primary, quiet, destructive, cancel), Chip and
  ChipRow (`histChips()`), Selector (the control deck's switch - so #92 can reuse it), Checkbox,
  Dropdown, Row, Panel (folder) built from a row table and **not tied to the popup's state**,
  Dialog from a spec, Label roles.
- Shared `lv_style_t` per role instead of local properties, kept in step with the scheme (as
  `UI::paint()` does). Measure `lv_mem` before and after - it should drop.
- Move the popup to the kit. Exit: no tree differences (or differences the hand-off lists and
  explains, each one sub-pixel or a deliberate merge the owner must confirm); `lv_mem` and build
  time no worse.

**P4 - The Sheet.**
- The window frame as a component taking view and panel specs; the card popup as its client.
- Prove it with a second, small client that ships (candidate: an "About this board" sheet from the
  system panel, or the Remove dialog's bigger sibling) - the owner chooses; if no answer, build
  it behind `DEBUG_POPUP` only.
- Exit: as P3, and the popup's first-frame build time no worse.

**P5 - The layout engine interface (design + the one engine we have).**
- Pull CardPage's placement out into `Layout` (pure C++, no LVGL): item model, container, result;
  `FlowEngine` = today's behaviour, byte for byte (the harness proves it on the dashboards).
- Add a PlatformIO `native` environment with unit tests for the pure modules (`Layout`,
  `TimeFormat`) - fast, off the board, run before every board build.
- **Design only** (a section in this file or `group-cards.md`): the cluster container (an engine
  run on a cluster's members in its own sub-grid), the bento / packing engine (stable,
  deterministic, prefers the previous arrangement), the domain weight map, the primary-region
  page, orbital. These are 2.11's to build.

**P6 - Hand-off, and a structure gate so this cannot drift back.** Propose (for the owner's
sign-off, as W10) a check run with the all-nine compile gate before every merge - a small script,
`scripts/structure_check.py`, that reports: any source file over ~1,000 lines; any
`lv_obj_set_style_*` call outside the component layer; any colour, radius or `mm(...)` literal
outside the tokens; any new LVGL include in `SystemCore`/`SystemReport`. It reports, it does not
fail the build; the merge report quotes its output. That is how the 6,000-line file would have
been caught at 1,000.

CLAUDE.md (the layer rules, where things live), `components.md`, `tokens.md`,
HANDOFF, ROADMAP, DECISIONS (new decisions written as **proposals for the owner**, not as
decided), and a short `docs/TEST_2.12.md` for the owner: what to look at on glass (everything
should look exactly as at `v0.2.12`), with the harness results attached.

Not in this milestone: new features, look changes, an LVGL upgrade (#88), any library rebuild,
ESP-IDF migration, the S3 boards' esp_lcd move (#67).

## 5. How to run it (the session's working rules)

- **Read first**: `CLAUDE.md` (all of it - the file-editing rule above all), `docs/HANDOFF.md`,
  `docs/DECISIONS.md` (W1-W9, K-series for the window), `docs/LESSONS.md` (the LVGL traps),
  `docs/design/card-sheet.md` §14-18 (what the window is), `docs/design/tokens.md`, this file.
- **Branch**: `refactor/ui-foundation` off `main` at `v0.2.12`. Commit and push at the end of each
  phase (and mid-phase when a step is done and verified). Never merge to `main` - the owner signs
  off on glass first (W2).
- **Boards you may build and flash without asking**: WS_P4_5 (COM15), WS_P4_4B (COM7), CYD_P4_1060
  (COM9) - check the ports by USB serial number first (HANDOFF). Compile WS_S3_4B as the S3 check;
  do not flash it (locked up, #69). All nine compile only at the end of P6.
- **The local debug lines** in `platformio.ini` (`-D DEBUG_POPUP` on the three P4s, `-D DEBUG_HANG`
  on the P4_5) are the owner's and uncommitted: keep them, **never commit them**; stash them for a
  release build and pop them after.
- **Do not**: rebuild libraries, change the settings file's keys or card ids, change what anything
  looks like or does, command any light except Office ones (W9 - none should be needed).
- **When a decision would change the look or the behaviour**: take the zero-change option, and
  write the question in HANDOFF for the owner with your recommendation and the alternative.
- **When stuck**: commit what works on the branch, write where and why in HANDOFF, and move to the
  next independent piece rather than forcing it.
- **Measure, do not assume** (the project's habit): `lv_mem`, build time, frame times
  (`DEBUG_FRAMES`) for panel slides, before and after each phase.
- **Subagents**: useful for **read-only** work in parallel - the literal/radius/style inventory
  for P2, a map of every caller of each builder for P3, writing the PC harness script while the
  main session adds the tree dump. A cheaper model is fine for inventories. **Never two agents
  editing the same files**; the main session does every edit to the popup and every build/flash.
- US English on screen; ASCII except the degree sign; comments may stay British.

## 6. For the owner

Each phase leaves the board looking and behaving exactly as at `v0.2.12`; the proof is the
harness's tree comparison and the screenshots. After P6 there is one short test sheet: look at
everything, it should be unchanged. What you get: a dialog is one call, a config window is a list
of views, a radius is one token, and 2.11's clusters and bento packing have a place to live.
