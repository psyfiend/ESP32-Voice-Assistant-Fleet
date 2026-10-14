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
- **Branch**: `refactor/ui-foundation` off the latest `main` (`v0.2.12` plus the guard merges).
  Commit and push the branch at the end of each phase (and mid-phase when a step is done and
  verified). Do not open the pull request until P6; the owner merges it, in the browser, after
  signing off on glass (W2, W12) - the guard refuses any other way into `main`.
- **Boards you may build and flash without asking**: WS_P4_5 (COM15), WS_P4_4B (COM7), CYD_P4_1060
  (COM9) - check the ports by USB serial number first (HANDOFF). Compile WS_S3_4B as the S3 check;
  do not flash it (locked up, #69). All nine compile only at the end of P6.
- **The local debug lines** in `platformio.ini` (`-D DEBUG_POPUP` on the three P4s, `-D DEBUG_HANG`
  on the P4_5) are the owner's and uncommitted: keep them, **never commit them** (the pre-commit
  hook refuses it). For the all-nine gate use `scripts/release/gate.ps1`, which backs them up and
  restores them; do not hand-roll a stash.
- **The guards** (CLAUDE.md, "Guards"): a hook refuses destructive git and delete commands. If one
  blocks something you believe is needed, stop that line of work, write it in HANDOFF for the owner,
  and carry on with the next independent piece - never route around it.
- **Do not**: rebuild libraries, change the settings file's keys or card ids, change what anything
  looks like or does, command any light except Office ones (W9 - none should be needed).
- **When a decision would change the look or the behaviour**: take the zero-change option, and
  write the question in HANDOFF for the owner with your recommendation and the alternative.
- **When stuck**: commit what works on the branch, write where and why in HANDOFF, and move to the
  next independent piece rather than forcing it.
- **Measure, do not assume** (the project's habit): `lv_mem`, build time, frame times
  (`DEBUG_FRAMES`) for panel slides, before and after each phase.
- **Subagents** (CLAUDE.md, "Subagents"; W11): the `auditor` agent for the P2 literal / radius /
  style inventories, `explorer` agents for the map of every caller of each builder (P3), in
  parallel. Writing the PC harness script may go to a writing subagent in a worktree, since no one
  else touches `scripts/ui_golden.py`. **Never two agents editing the same files**; the main
  session does every edit to the firmware and every build/flash.
- US English on screen; ASCII except the degree sign; comments may stay British.

## 6. Progress (the session's own record, phase by phase)

**P0 - DONE (2026-10-13, `6475370`, `34fd56a`).** `GET /popup?tree=1|page` prints every object -
class, place, size, flags, state, the events it listens for, text, and its style as LVGL resolves it
(theme, shared and local styles together, plus what each pressed / checked / disabled state
changes); also `mem=1`, `sparks=N` (off for a capture) and `removecard=3` (the Remove dialog over
any card, nothing removed - the settings file is never written by the harness). `scripts/ui_golden.py`
walks 46 states (43 on the 4B, whose grid drops the Panel cards) and saves each tree and screenshot
under `test/golden/<board>/<label>/`; `compare --pixels` reports what changed, telling the house
apart: digits and times masked, a text that changed size excuses only what sits with it, the
chart's insides follow the clock, a card on a page whose words changed is data, and a light that
changed between captures marks its window "capture again". A window closes itself after 60 s
without a touch and /popup is no touch, so the walks are short groups (the 1060 takes ~6 s a state).

Exit: two baseline captures per board, **no differences in trees or pixels** - WS_P4_5 `p0a`/`p0b`,
WS_P4_4B `p0a`/`p0b`, CYD_P4_1060 `p0c`/`p0d`. `p0c` (the walk as it now is) is the baseline on all
three. Window build times, median of 7 opens (minimum), sparks off - `measure`, label `p0c`:

| Window | WS_P4_5 | WS_P4_4B | CYD_P4_1060 |
|---|---|---|---|
| Desk (HA light group) | 36.5 ms (36.4) | 37.6 (37.5) | 36.8 (36.4) |
| Deck (temperature) | 31.1 (31.0) | 32.0 (32.0) | 31.3 (31.2) |
| North (garage door) | 30.1 (30.0) | 31.0 (30.7) | 30.4 (30.4) |
| Overhead (switch) | 32.2 (32.1) | 34.7 (32.9) | 32.1 (32.0) |
| All Lamps (group here) | 37.4 (37.1) | 37.1 (36.9) | 36.8 (36.7) |
| Lamp 1 (virtual light) | 33.9 (33.9) | 34.9 (34.8) | 33.7 (33.4) |

LVGL's pool per open window: Desk ~10.2 KB, a sensor ~7.2 KB, All Lamps ~11.0 KB (the 4B ~0.8 KB
more); closing gives it all back. The P0 firmware is kept per board in `test/golden/<board>/fw_p0/`
(app at 0x10000) for A/B re-flashing.

**P1 - DONE (2026-10-13).** `CardPopup.cpp` is now the frame and the public interface (721 lines);
the rest is 18 files in `src/Cards/Popup/` (86-838 lines) sharing `PopupState.h` (572 lines: the
window's types and state, and 141 declarations by file) in `namespace popup`. Made by a script that
copies every line of the old file once, verbatim (`test/golden/split_popup_p1.py`, gitignored); the
only edits were the namespace lines, `extern`s for the shared state and default arguments moving
to the declarations. `CardPopup.h` is unchanged. Checks: WS_P4_5 and WS_S3_4B build (P4_5 RAM
identical, flash 1.9 KB smaller); every one of the old object's 260 symbols is in the new ones (127
more: functions P0 had inlined away); **no tree or pixel differences against `p0c` on all three
boards**; build times within 0.1 ms on every minimum (medians within noise), pool cost per window
within +-60 B.

Findings for the owner, none changed (zero-change rule):
- `rowLive()` (SETTINGS rows) has been unused since K70.
- **WS_P4_4B: All Lamps' Members view titles "All Lamp..." when opened straight from a fresh
  window**, and "All Lamps >" when reached through Properties first - present at `v0.2.12` (seen in
  the baselines). The DOT-label trap of 2.10c round 7 (LESSONS), on another path. **Withdrawn
  2026-10-14** (owner, TEST_2.12 Q6): on the 4B that label is hidden in Members for lack of room,
  and the window shows "Members", which is right - the truncated text is never seen.
- The auditor's inventory also found two stale comments (the sparks' run time, SETTINGS' most rows)
  and `setFace()` declared twice; fixed in P2. The round selector is drawn two ways (the control
  deck's and SETTINGS'); that is P3's Selector.

**P2 - DONE (2026-10-13).** `UIShape` (`include/UI/UITokens.h` §2b, values in
`src/UI/UITokens.cpp`, read through `UI::shape()`): 56 tokens in millimetres - five radii, a
ten-step spacing scale and four values between its steps, ten line and letter-spacing weights,
fourteen sizes, three finger-travel distances, and ten hero values in P4_5 millimetres. 157
literals replaced; no `mm(<number>)` or `pm(<number>)` is left in the window's code - the sparks'
dice are named constants beside the effect, and the one remaining number (a 1 px outline) says why.
Its own struct rather than `UIMetrics` as the brief said: `tokens.md` §7 gives the reasons, and
DECISIONS U2 proposes it. "Any repeated durations": the window's are already named constants, each
used in one place, so none moved.

Checks: WS_P4_5 and WS_S3_4B build; **no tree or pixel differences against `p1` or `p0c` on all
three boards** (label `p2`); every window's fastest build within 0.5 ms of `p0c` (the 1060's Deck
read 32.0 in the first run and 31.3 in a second); medians move 0-2.5 ms between runs of the same
firmware (P1 showed the same - the boards' own HA traffic during a run); LVGL's pool per window
within 70 B of `p0c` (a label's text length - the trees are identical). **Never run two harness
commands at one board**: each turns the sparks off for its run and back on at its end, so the
other's numbers then include the sparks (+500 B and ~0.7 ms a window, seen once here).

| Window, fastest of 7 (ms) | WS_P4_5 p0c / p2 | WS_P4_4B p0c / p2 | CYD_P4_1060 p0c / p2 |
|---|---|---|---|
| Desk | 36.4 / 36.4 | 37.5 / 37.5 | 36.4 / 36.3 |
| Deck | 31.0 / 31.0 | 32.0 / 31.9 | 31.2 / 31.3 |
| North | 30.0 / 29.9 | 30.7 / 30.8 | 30.4 / 30.4 |
| Overhead | 32.1 / 32.0 | 32.9 / 32.8 | 32.0 / 31.9 |
| All Lamps | 37.1 / 37.1 | 36.9 / 37.0 | 36.7 / 36.7 |
| Lamp 1 | 33.9 / 33.6 | 34.8 / 35.0 | 33.4 / 33.4 |

The candidates to merge (`S_GUTTER` into `S5`, and six more) are in `tokens.md` §7 with each
one's pixels on the three boards and a recommendation. **The owner decides; nothing is merged.**

**P3 - DONE (2026-10-13).** The component kit: `include/UI/Kit.h`, `src/UI/Kit/` (Basics, Styles,
Button, Dialog, Chips, Rows, Folder, Faces, Selector), described with pictures in
`docs/design/components.md`. Label roles, Button (cancel, destructive), Dialog from a spec, Chip
row, List item, Rows (dropdown, checkbox, row button) from a row table, the Folder panel (not tied
to the window's state: the window it belongs to and its callbacks are its spec), the Selector (the
control deck's switch, its four looks and the hand-drawn faces - ready for #92), Icon button, and
pill / swatch / badge styles. Each role is shared `lv_style_t`s refilled on every repaint
(`UI::onRepaint()`, new, beside `UI::paint()`), filled at boot by `Kit::begin()`. The window is
built from it: the window's files lost ~1,000 lines net, and the kit is ~1,500 with its comments.

Checks, by A/B - each board flashed with its P2 image (`test/golden/<board>/fw_p2/`, gitignored)
and captured, then flashed with P3 and captured, minutes apart, so the house is the same in both:
**no tree or pixel differences on WS_P4_5, WS_P4_4B and CYD_P4_1060** (labels `p2d`/`p3d`). On the
1060 the Living room's Overhead and the Office desk light were switched on between its two
captures; those five states were captured again both ways (`p2e`/`p3e`) and matched. (A page
state cannot tell the house changed - only a window's entities are snapshotted - so a card that
changed colour on the page reads as a difference: capture again.) WS_S3_4B builds. LVGL's pool: ~180 B less per window, 1-2.5 KB less with a panel or History open,
~880 B more at boot for the styles themselves (components.md §4); the window's fastest build within
0.3 ms of `v0.2.12`.

Harness changes on the way: `crop` (the pictures), `--exact`; output that survives Windows' code
page; a label showing digits no longer counts as a pixel difference when the clock ticked between
its tree and its screenshot (the tree still checks it). **One run per board at a time**: a stray
second chain flashed the P4_5 under a capture once (both discarded, the A/B redone).

For the owner (P3), none changed (zero-change rule):
- **Buttons have two roles, CANCEL and DESTRUCTIVE**; the brief's PRIMARY and QUIET have no user in
  the house yet (the row button is a row control, a scene a pill), so they wait for their first.
- **The kit's styles follow a scheme change live, as `UI::paint()` does.** The window's own
  local colours still do not, so a window open across a scheme change would show both - a path the
  device cannot take (the window is modal; the scheme knob and page swipes are behind it).

**P4 - DONE (2026-10-13).** The Sheet: `include/UI/Sheet.h`, with `src/UI/Kit/KitSheet.cpp` (the
frame: placement, the window, header row, stage, auto-close, the quiet build, the modal tap
catcher, closing), `KitSheetPanels.cpp` (both folder panels: built, slid, dragged, synced, gone)
and `KitSparks.cpp` - moved from the card window (`CardPopup.cpp`, `PopupPanels.cpp`, the catcher
from `PopupHold.cpp`; `PopupSparks.cpp` is gone). A `Kit::SheetSpec` says what is the client's -
its tabs, its first view, a tick, what to do on closing, SETTINGS' rows, the left panel per view -
and the card window's state extends the sheet's (`struct Popup : Kit::Sheet`), so its code reads
`s.win`, `s.stage`, `s.right` as before. `components.md` §3.

The second client, **"About this board"** (`src/UI/AboutSheet.cpp`, ~130 lines with no frame code;
debug builds only: `GET /popup?about=1`, `about=0`): one view of fact rows - the board, its screen
and chip, the firmware, how long it has run, free memory and LVGL's pool, updated each second - on
the same frame (picture in `components.md` §3). Properties' row became the kit's `Kit::factRow()`
for it. Closing it leaves the page and LVGL's pool as they were.

Checks: A/B against each board's P2 image, minutes apart (`p2f`/`p4a` on WS_P4_5, `p2g`/`p4a` on
WS_P4_4B and CYD_P4_1060): **no tree or pixel differences on all three boards** (46 states; 43 on
the 4B). WS_S3_4B builds. **The window builds ~3.5 ms faster on every window** (WS_P4_5, fastest of 9,
P2 image and this build back to back: Desk 36.3 -> 32.4 ms, Deck 31.0 -> 27.2, North 29.9 -> 26.2,
Overhead 32.0 -> 28.1, All Lamps 36.9 -> 32.7, Lamp 1 33.7 -> 29.8). Explained before it was
believed: SETTINGS' row list was computed twice per window - once for its signature, once for its
width, and its Card label row walks the dashboard's groups - and the sheet reads it once; the same
rows. The board's own `DEBUG_POPUP` timings agree (the quiet build 28.8 -> 25.5 ms) and show the
first frame unchanged: 58-62 ms, the window's rectangle only, both ways.

For the owner (P4):
- **Where should "About this board" live?** It is behind `DEBUG_POPUP` until you say - the system
  panel, or a long press on the header's board name, would both be one line.

**P5 - DONE (2026-10-13).** The layout engine interface: `include/Layout/Layout.h` and
`src/Layout/Layout.cpp` - `Item`, `Place`, `Container`, `Events`, the `Engine` interface, and
`FlowEngine`, the dashboard's placement moved out of `CardPage.cpp` unchanged (pins first, the
widest span that fits at the first free spot, the lowest priority dropped until the rest fit; the
occupancy bitmap with it), and `rowsWanted()`. `CardPage` turns its cards into items each time it
plans, hands them to its `FlowEngine` and keeps the places it gets back as its slots
(`using Slot = Layout::Place`); its `DBG_CARDS` lines come back through `Events`, word for word.
Memory: the items are on the stack while a page plans (~400 B; the loop task has 16 KB), and a page
object is 4 B bigger (the engine's vtable pointer; the bitmap moved, not copied). The design for
2.11's engines is §8.

**Host tests** (`host-tests/`, its own PlatformIO project: `pio test -d host-tests`): 9 tests of the
flow engine's rules and 8 of `TimeFormat`'s wording. This PC has no Windows compiler, so
`pio test` fetches Unity and stops at "'g++' is not recognized"; `wsl bash host-tests/run_in_wsl.sh`
compiles the same suites with WSL's g++ 13.3 and the same flags: **17 of 17 pass, no warnings**.
Three deliberate breakages of a copy of the engine - ties dropping the earlier item, pins not placed
first, the narrowest span tried first - each failed exactly the test written for that rule.

Checks, by A/B - the P4 firmware captured, then this build flashed and captured, minutes apart:
**no tree or pixel differences on WS_P4_5** (46 states, `p4b`/`p5a`) **and WS_P4_4B** (43 states;
`house_page` and `desk_temp` showed the house changing between the captures - Office's lights on,
Kitchen's off, a temperature reading, and Desk's color temperature, which moved the marker on its
strip - and were captured again both ways, the P2 image against this build, `p2h`/`p5b`: no
differences). Every card on both pages sits where it did, which is what this
phase could have moved. Window build times on WS_P4_5, fastest of 9, within 0.3 ms of P4 on every
window (Desk 32.7, Deck 27.2, North 26.2, Overhead 28.1, All Lamps 33.0, Lamp 1 29.8 ms); LVGL's
pool per window unchanged. WS_S3_4B builds - with `DEBUG_CARDS` on, so the `Events` callbacks
compile there; RAM identical, flash +424 B.

**CYD_P4_1060: no differences either** (46 states, the restored P2 image against this build,
`p2i`/`p5c`; each window opened ~4 ms faster, P4's gain). Getting there cost an hour: its walks
failed twice part-way ("the window closed before its screenshot" - its screenshots, ~6 s a state
earlier, took ~40 s), then two 1.5 Mbaud flashes lost its USB link, the second after erasing the
app, which left it dark and its port gone. The owner held BOOT while plugging it in; the P2 image
then flashed at 460800 but the screen stayed dark - the chip was still in the ROM bootloader, and
only `--after watchdog-reset` started the app (LESSONS). After that power cycle its walks ran at
their usual pace, and P5 went on the same way: the app at 460800, a watchdog reset.

For the owner (P5):
- **A compiler on the PC** for `pio test` is an install, so it is yours (MSYS2 or WinLibs, README in
  `host-tests/`); the WSL route works without one.
- Found on the way, unchanged: a card span of 0 counts as a whole cell when the page counts the rows
  it wants, but as one unit when it places (`rowsWanted()` and `placeOne()`, both as they were). No
  card sets 0 (`CardPlacement` defaults to 2), so it shows nowhere today.

**P6 - DONE (2026-10-13).** The hand-off. `scripts/structure_check.py` (W13, accepted 2026-10-14, to run
with every all-nine gate; it reports and never fails): files over 1,000 lines, `lv_obj_set_style_*`
outside the kit, colour literals outside the tokens, numbers where a shape token belongs, LVGL in
`SystemCore` / `SystemReport` - skipping the generated font faces and headers, `third_party/` and
`lv_conf.h`, and saying how many it skipped. Its first report, at the end of 2.12: **two files over
1,000 lines** (`GUIManager.cpp` 1,538, `Card.cpp` 1,462 - neither the window's); 531 style calls
outside the kit in 26 files (the cards, the panels, the header - the window's own files have 1-36
each, its hero and chart); 44 colour literals in 4 files (the two status widgets, the card icons,
and the window's colour-picker mark, white and black on every scheme); **no shape numbers; no LVGL
in the core layers.** The docs: HANDOFF, ROADMAP, DECISIONS (U1-U5, W13 proposed), CLAUDE.md, this
section, and the owner's sheet `docs/TEST_2.12.md`.

**All nine environments compile** from the committed configuration. `scripts/release/gate.ps1`
built the five P4s (7B 37.1% of flash, P4_5 40.8%, 4B 39.1%, 1060 36.9%, 4880 37.5%) and then
stopped at the first S3: under Windows PowerShell 5.1, with `$ErrorActionPreference = "Stop"`, the
S3 linker's warning on stderr (`missing .note.GNU-stack section`) is a terminating error. Its
`finally` restored `platformio.ini` as designed. The four S3s were compiled directly - the local
debug lines touch only the three P4 environments, so their config is the committed one: WS_S3_4B
34.3%, CYD_S3_3248 35.1%, WS_S3_5B 36.4%, CYD_S3_8048 35.3%. The script is unchanged; its one-line
fix is the owner's to approve (TEST_2.12 Q7, HANDOFF).

## 7. For the owner

Each phase leaves the board looking and behaving exactly as at `v0.2.12`; the proof is the
harness's tree comparison and the screenshots. After P6 there is one short test sheet,
`docs/TEST_2.12.md`: look at everything, it should be unchanged; its questions Q1-Q7 gather what
each phase left for you. What you get: a dialog is one call, a config window is a list
of views, a radius is one token, and 2.11's clusters and bento packing have a place to live.

## 8. The layout engines - design for 2.11 (P5)

P5 built the interface and the one engine the dashboard has always had; what follows is the design
2.11 builds on it. Nothing here is decided: each is a proposal for the interview.

**The interface** (`include/Layout/Layout.h`, pure C++, tested off the board in `host-tests/`): a
`Container` is a grid of units; an `Item` has the span it would like and the least it will accept, an
optional pinned spot and a priority; an `Engine` takes the container and the items and writes a
`Place` for each - where it went, or that it did not fit. `FlowEngine` is today's: pins first, then
reading order, the widest span that fits at the first free spot, the lowest priority dropped until
the rest fit. CardPage turns its cards into items, asks the engine, and lays out what comes back.
New engines are new classes; a page chooses one.

**A cluster is a container inside an item.** On the page, a cluster (a group panel, `group-cards.md`
§5.3) is one item, its span from its member count (or declared). Inside it, an engine runs again on
the cluster's own sub-grid - its span in units less its frame - with the members as items, each a
unit or two (a member drawn without its own border, compact). The page places the cluster as it
places a card; the cluster places its members as the page places cards. Same interface, a smaller
container; the flow engine works there unchanged.

**The packing ("bento") engine.** What it must be: DENSE (no holes a smaller card could fill),
DETERMINISTIC (the same items give the same layout, every boot) and STABLE (one card more or less
moves as few others as possible - a layout that reshuffles itself is worse than one with a hole).
How: items sorted by area, largest first, ties by declaration order; each placed where it leaves the
least unusable space (a maximal-rectangles packer on the unit grid), trying spans from preferred
down to least; and stability from the LAST layout - each item first tries its previous place
(`Place` kept per card id, K33, in the settings file), and only those that no longer fit are packed
afresh. Priority still decides who leaves when nothing fits. Testable off the board: the same input
twice gives the same output; one item added moves at most the items after it.

**The domain weight map.** Which cards earn space: a weight per card - its domain's (climate and
media heavier than a lone sensor), times how much it is used (taps counted in the settings file) -
turned into a target area, spans chosen within each card's least and preferred. The weights are
data (a table beside the tokens, a page able to override them), never code.

**The primary-region page.** One hero region - a room's main light, its climate - takes a fixed share
(a 2x2 at the top left, or a column), and the rest of the page is flowed or packed around it: the
container split into regions, each region its own engine run. A card is put in the region by a
`primary` flag in its spec.

**Orbital.** A page arranged around a centre - the room as one large card, its devices around it,
heaviest nearest: positions by distance from the centre cell, ring by ring, clockwise from the top.
For the large panels; on a grid too small for a ring it falls back to flow.

Each is a class implementing `Layout::Engine`, pure, with host tests for its properties before it
draws anything.
