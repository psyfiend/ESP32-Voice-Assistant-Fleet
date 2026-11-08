# The dashboard as data

`cards.md` says what a card contains and `card-layout.md` says how one is drawn. This says how a
*page* of them is described, placed, and degraded when it does not fit - and why the description is
a struct rather than a sequence of calls. Where the arranger places a page, `layout-engines.md` is
the home; this document covers the grid, the spec and the flow fallback.

---

## 1. A page is data

A page is a `PageSpec` holding an array of `CardSpec`, in `include/Cards/PageSpec.h`. The cards made
on the board are kept in the settings file (`CardAdded`) and merged in; what stays compiled is
`include/Dashboards/Dashboard_Fleet.h` (the built-in Panel group) and `Dashboard_HA.h` (Garage -
Doors), K171. Both placement paths finish through `CardPage::commit()`: there is one placement
implementation.

A spec carries a header mode (or inherit), spans and priority, a variant override, a staleness
override, area and label. **There is no domain field**: a card's type comes from its primary
entity's `EntityKind` through `cardForEntity()`. A domain in the spec would let it disagree with the
registry about what a thing is.

### One definition, the whole fleet

A page renders on every board. Nothing in it is board-specific, because the three things that vary
are derived:

- the **grid** comes from the panel: `WS_P4_7B` gets 7 cells across, `CYD_S3_3248` portrait gets 2
- cards that do not fit are dropped **by priority**, so a small board keeps what is worth keeping
- a spec whose entity is missing from this board's registry is skipped and reported

A small board renders a true subset of a large one. A per-board page would be another `PageSpec`
selected by the board identity macro, as `ConnectivityDefaults.h` does.

---

## 2. Sub-grid units

A page is N x M **cells**, with N x `sub` by M x `sub` **units** underneath, and cards span in
units: a quarter-page card on a 3 x 3 page is 3 x 3 units, a normal card 2 x 2. `subdivision` is a
per-page property defaulting to 2 (`PAGE_SUBDIVISION_DEFAULT`). **Every span, minimum and coordinate
in `CardPlacement` is in units**; `U_CELL` and `U_HALF` name a whole cell and half of one.

### The subdivision is dimensionally transparent

Columns are `LV_GRID_FR(1)` per unit track with a gap between every track. For `c` cells of width
`W`:

```
one cell, undivided:   W/c - g + g/c
sub units, spanning 2: 2*(W - (2c-1)g)/(2c) + g   =   W/c - g + g/c
```

Identical: the gaps absorb the subdivision exactly, so **no card changes size when a page
subdivides**. Row heights are fixed pixels rather than fractions, and integer division loses up to
`sub-1` px per cell, which is why `commit()` hands each card the height it actually got
(`Card::setCellHeightPx()`) rather than the one the token asked for. The page knows the answer
exactly; `UI::grid()` is global state that another page can move.

---

## 3. Placement, and the validator

Placement is at unit granularity plus a validator, not a constraint solver, so layouts stay
predictable and debuggable.

**Flow** is the default: the first free position wins, and at that position the widest span that
fits wins, so a card that fits at its minimum beside its neighbour beats a card sitting alone on a
new row at full width.

**Explicit placement** sets `CardPlacement::col` / `.row` in units (`-1` = flow). Occupancy is
tracked in a bitmap, so pinned and flowed cards coexist. (Pinning predates the arranger and is
shelved: "Pin" on the glass means on the page, K164.)

**An invalid pin is reported and then flowed, not dropped.** Out-of-bounds or already-occupied
prints through `DBG_CARDS` and marks the card `[pin rejected]` in the System Doctor.

Validation is at **runtime**: the structs are `const`, not `constexpr`, so they are not usable in
constant expressions (the reason CLAUDE.md gives for preferring runtime checks to `static_assert`).

---

## 4. Priority degradation

A page does not scroll: new entities appear only by swiping or navigating to another page. An
over-subscribed page drops something, and `priority` says which.

`CardPage::plan()`: place everything in declaration order; if any card cannot be placed, **drop the
lowest-priority card still alive and re-plan the whole page**. Dropping the lowest-priority card
rather than the one that failed keeps the outcome independent of declaration order; ties go to the
later declaration. Cards dropped for room are listed in Edit cards as "No room" (K113).

`Dashboard_Fleet.h` uses four bands - `PRI_CRITICAL` 220, `PRI_NORMAL` 160, `PRI_NICE` 100,
`PRI_DEBUG` 40 - and puts the panel's own telemetry in the last one. Uptime is `PRI_DEBUG + 10`
so that it does not always lose a tie to Free Heap, and both are 3 units wide (one and a half cells)
because Uptime's `HH:MM:SS` does not fit the value face in one cell; `minSpanX = U_CELL` lets them
fall back on the narrowest board.

---

## 5. Where the dashboard lives on screen

`GUIManager` owns a host container between the header and the deck, and the page fills it. The grid
derives from **that container**, never from `bsp_display.WIDTH/HEIGHT`, which is wrong on rotated
boards and ignores the header. The dashboard sits below the deck in z-order, so an expanded deck
panel covers cards rather than pushing them. The touch-visualiser overlay lives on `lv_layer_top()`.
The deck's vertical cost is measured from the objects (`lv_obj_get_coords()`, absolute screen
coordinates) and not encoded as a constant.

---

## 6. Rows and columns

**Columns** come from `TARGET_CARD_W`: as many whole target-width cards as fit the width
(`recomputeGrid()`). **Rows** come from the cards, not the geometry. When the arranger has no answer
and the page flows:

1. `CardPage::rowsWanted()` counts the cells the specs ask for and divides by the columns.
2. `commit()` starts at that number, plans, and **adds a row only when placement fails**.
3. Cards are dropped by priority only after every available row has been tried.
4. `useRows()` divides the whole height among exactly that many rows.

Fewer rows means taller cards. A page the arranger has placed uses its own grid of units
(`CardPage::useUnits()`), every block pinned and drawn at the rectangle the finish gave it. A row
count held from the system panel (Col / Row, saved as `pages.<slug>.cols` / `rows`) is obeyed.
`UIGrid::rows` is a geometric estimate for callers with no cards to count.

| Token | Decides | |
|---|---|---|
| `TARGET_CARD_W` | **columns** | as many whole target-width cards as fit the width |
| `ASPECT_PCT` | **a ceiling** | the tallest a card may be as a percentage of its width (default 130): a page with three cards does not make each as tall as the screen |

Each knob **rebuilds** the page, because a card decides compact or full from its cell height when it
is built. `Card::fullCellNeedPx()` and `Card::compactCellNeedPx()` are shared statics so the page
asks the card layer the same question before deciding how many rows to carve.

### Per-board default grids

`GUIManager` carries an `#ifdef` on the board identity macro: `WS_P4_5` starts at 5 x 3, both 4B
boards at 3 x 4 (D-6), as exact counts through `COLS_OVERRIDE` and `setRowsOverride()`. Only the
boards the owner named are set. `GUIManager.h` includes `bsp_loader.h` itself, because a board macro
tested before `bsp_loader.h` is included is undefined and the `#if` takes the fallback silently.

---

## 7. Units on a card

The owner wants Fahrenheit, and both units are supported and overridable: his Home Assistant
reports most temperature sensors in degrees F and one in C, so a fleet preference without
conversion would render that one card wrong and look identical to a working card.

Resolution is fleet default, then page, then card, via `TempUnit` in `CardTypes.h` and
`cardSetTempUnit()`. **Conversion happens at format time and nowhere else.** The registry keeps
whatever the source said, because that value is what an echo is compared against, what an
optimistic write reverts to, and what the board's own HA discovery publishes. The unit *label* comes
from `cardDisplayUnit()` rather than `desc.unit`, and a converted integer is promoted to one decimal
(22 C is 71.6 F; printing `71` would be a rounding the source never made).

A device-wide units choice in UI Tweaks is not built (K177).

---

## 8. The system drawer

It opens over a page and its geometry is derived the same way.

**Width comes from the screen in logical pixels, with no board macro.** At or above 700 logical px
it takes half the width; at or above 400, three quarters; below that the whole screen
(`Panel_System.h`: `PANEL_WIDE_LOGICAL`, `PANEL_MID_LOGICAL`). Logical rather than physical:
`WS_P4_5` is 1280 px across and 740 logical, so a physical test would file it with the 7B when it
has less usable width than a five-inch board. The fleet's widths cluster at 330 / 480 / 727-740 /
1024 logical, so both thresholds sit in a gap.

**Height is measured.** `_ui_content` is `LV_SIZE_CONTENT` and the wrapper animates to what it
measures. The panel's `[SysPanel]` line at boot prints the width, the top and the face height.

**The header is show or hide**, 35 logical px (`UIToolkit::SYSTEM_HEADER_FULL_H`); a hidden header
means the panel starts at y = 0.

`lv_obj_set_pos()` is relative to the parent's content area, so a container that does not clear its
theme padding is shifted by that padding.
