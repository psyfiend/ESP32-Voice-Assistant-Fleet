# Card layout - the model, and why it is this one

The model the code implements. If you are about to move something on a card, read §1 and §2 first:
between them they explain the layout rules. Companion to `cards.md`, which says what a card
*contains*; this says how it is *arranged*. The header modes are `CardHeaderStyle`
(`HDR_NONE`, `HDR_BAND`, `HDR_TAG`, `HDR_TAG_FLOAT`; on screen: the label type None, Band, Tag,
Float).

---

## 1. Three rules

### 1.1 Never position a widget from another widget's measurements inside `render()`

`lv_obj_align_to()` resolves against the reference object's position **at the moment it is
called**, not at layout time. `render()` runs before LVGL has laid anything out, so every
measurement it takes is of a stale or default geometry. The symptoms are a unit beside the wrong
label, a percentage behind the value, a label hanging off the card's edge.

**Use a flex container instead.** Flex measures after layout. Both card bodies are vertical stacks
for this reason.

The same applies to `lv_obj_update_layout()`: in a render path it walks the entire screen, once per
card, per repaint, and a variant resolved from it reads LVGL's default object size instead of a
real cell.

**Derive from tokens, not from widgets.** `UIGrid::cellH` and the card's own span are known before
anything is laid out. `Card::midHeight()` computes the hero's band that way.

### 1.2 A container clips its children, and some styles cost a whole render layer

`lv_obj` clips its children by default. `LV_OBJ_FLAG_OVERFLOW_VISIBLE` lifts that **for one level
only**: the parent above still clips, and the page's container is scrollable, so it clips hard.
Anything hung outside its cell is cut away.

**A tag lives inside its cell**, as a row above the surface. The card is shorter by the tag's
height, every card in that mode takes that height, and the gap above a tag is the page's ordinary
row gap.

Two styles make LVGL render an object to an **intermediate layer buffer**:

| Style | Why it allocates |
|---|---|
| `clip_corner` | masks rounded corners by rendering to a layer and masking it |
| `opa` below `LV_OPA_COVER` | composites the whole subtree |

Both are per object, per frame, and the layer is sized by the object's width. On `WS_P4_5` a
two-cell card asked for a layer larger than LVGL's free memory and the board froze retrying it. So
no card uses `clip_corner`, not even the header band, and a paused card dims by **mixing its colors
toward the ground** (`Card::tone()`), never by setting an opacity.

### 1.3 LVGL's draw walk recurses once per level of nesting

`loopTask` is where LVGL runs, and arduino-esp32 gives it 8192 bytes by default. A card is nested
deep: screen, column, host, page, cell, surface, body, band, row, label. A tree deeper than the
stack allows ends in `Stack canary watchpoint triggered (loopTask)` with a backtrace of one
alternating pair of addresses repeated about a dozen times; that pair is LVGL walking down the
tree. `main.cpp` sets `SET_LOOP_TASK_STACK_SIZE(16 * 1024)`.

**The System Doctor reports the stack's high-water mark.** If it trends toward zero as cards gain
nesting, the answer is a flatter tree rather than a bigger stack.

---

## 2. The layout model

Both layouts are the **same vertical stack**: a value card and a state card side by side put their
names at the same height.

```
  +-----------------------------+
  | [tag row]   HDR_TAG only, outside the surface
  +-----------------------------+
  | [header band] HDR_BAND only, inside, costs the body a strip
  |                             |
  |  icon        <- out of flow, top-left corner
  |                             |
  |      MID      grows         |  the hero: a value+unit, or an icon disc
  |                             |
  |      NAME                   |  the location, or what the thing is
  |  batt            seen       |  STATUS, reserved whether or not it has content
  +-----------------------------+
```

Three numbers, shared by both layouts, all in `Card`:

| | |
|---|---|
| `topBandHeight()` | the corner icon's size. **Not reserved in the stack** |
| `statusBandHeight()` | the taller of the icon face and the text face |
| `midGap()` | between the hero and the name |

### The corner icon is out of the flow

It sits in a corner the centred hero never uses, so reserving a row for it would pay for a
collision that cannot happen; on a small cell (`CYD_S3_3248`) that row is the difference between a
status line fitting and not. A card with a hero icon has no corner icon (K172).

### The status band is reserved even when empty

A card whose sensor reports no battery must not sit its name lower than the one beside it that
does. `statusBandHeight()` takes the **taller** of the two faces that can appear there (the
battery glyph from the icon face, the percentage from the text face).

### The hero always charges for a header

`midHeight()` subtracts a header's height **whatever the card's mode is**, so a card's hero is the
same size in all four modes.

### A hero is clamped rather than scaled from its font

An icon disc sized at a multiple of its font's line height overflows a small cell and is clipped by
its own parent. The disc takes the smaller of the font-derived size and the band it has.

---

## 3. What is derived, and from what

Nothing about size is declared twice. One physical fact per board produces everything:

```
  BSP_<BOARD>.h  WIDTH, HEIGHT, DIAGONAL_IN
        |
   bspPixelDensity()  ->  real PPI
        |
        +-- bspUiScale()  =  PPI / 170   ->  UI::sc()  ->  every padding, radius, gap
        +-- UI::minTouch()  =  9 mm in real pixels
        +-- recomputeGrid()  ->  columns, rows, cell size
        +-- gen_type_scale.py  ->  UITypeScale.h   <- BUILD TIME
        +-- gen_icon_font.py   ->  UIIcons.h       <- BUILD TIME
```

The last two resolve at build time because **LVGL compiles fixed bitmap fonts**: a face has to be
chosen before the binary exists. `lv_display_set_dpi()` does not help with type: every DPI-sensitive
path in LVGL goes through `LV_DPX_CALC`, and no DPI path touches fonts (the callers are `lv_obj`'s
default size, `lv_obj_scroll`'s minimum scrollbar, `lv_slider`'s click area, `lv_arc`'s touch
tolerance and `lv_theme_default`'s radius, border, padding and shadow).

`UI::sc()` and LVGL's `lv_dpx()` are the same arithmetic with different reference constants (170 and
160). That duplication is logged in `FUTURE_IMPROVEMENTS.md`.

---

## 4. Variants

`VAR_FULL` and `VAR_COMPACT` are **derived from the cell, not declared**. A card measures whether
its cell can seat a hero, a name and a status line at the type scale's sizes, and draws **less**
when it cannot, never the same thing smaller, which would undo the work the generated type scale
exists to do.

- `ValueCard` drops the status corners.
- `StateCard` drops the **name** and keeps the icon, because state *is* the icon and its color
  (`cards.md` §4).

**The test keeps a margin.** A font's line box is taller than its ink, labels round up, and the
body's padding is approximate at that point. Being slightly too eager to go compact costs a status
line; being slightly too reluctant breaks the card.

`-D DEBUG_CARDS` prints the decision per card: `[Cards:debug] sensor: cell 121 need 119 -> full`.

---

## 5. State, and where its verdict lives

A command's outcome is a property of the **entity** (`Entity::cmdFailed`), not of the card that
sent it, so a parent card and a child card bound to the same switch cannot disagree. A parent's
state derives from where its children are:

- every commandable child failed: `ST_REFUSED`
- some: `ST_PARTIAL`, and the body keeps showing what the children are actually doing

- **A successful echo produces no notification.** It carries the value the optimistic write already
  applied, so `setValue()` finds nothing changed and does not dirty the entity. Resolution is
  therefore **polled** from `pollState()`, never awaited from a snapshot.
- **The verdict compares against what was commanded**, not against `prevValue`, which a second
  command inside the same window deliberately does not overwrite.
