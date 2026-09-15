# Card layout — the model, and why it is this one

**Written 2026-09-15, at the end of milestone 2.4, after the same class of bug came
back four times in four different places.** Each time it was fixed by adjusting a number
and reflashing. That worked, slowly, and it is not how the next change should go.

This document is the model the code implements. If you are about to move something on a
card, read §1 and §2 first — between them they explain every layout defect this milestone
produced.

Companion to `cards.md`, which says what a card *contains*. This says how it is *arranged*.

---

## 1. The three rules that were learned the hard way

### 1.1 Never position a widget from another widget's measurements inside `render()`

`lv_obj_align_to()` resolves against the reference object's position **at the moment it is
called** — not at layout time. `render()` runs before LVGL has laid anything out, so every
measurement it takes is of a stale or default geometry.

This produced, in order: a unit rendered beside the card's *name* instead of its number; a
battery percentage drawn *behind* the value, intermittently, depending on whether a layout
pass happened to have run; and a percentage hanging off the bottom edge of a 141 px card.
Three different symptoms, one cause.

**Use a flex container instead.** Flex measures after layout. Both card bodies are vertical
stacks for this reason and no other.

The same applies to `lv_obj_update_layout()` — calling it in a render path walks the entire
screen, once per card, per repaint. It was also how `resolveVariant()` came to read LVGL's
default object size (~92 px) instead of a real cell (264 px), which put every card on the
fleet's largest panel into COMPACT.

**Derive from tokens, not from widgets.** `UIGrid::cellH` and the card's own span are known
before anything is laid out. `Card::midHeight()` computes the hero's band that way.

### 1.2 A container clips its children, and some styles cost a whole render layer

`lv_obj` clips its children by default. `LV_OBJ_FLAG_OVERFLOW_VISIBLE` lifts that **for one
level only** — the parent above still clips, and the grid container is scrollable, so it
clips hard. That is why an area tag hung outside its cell was invisible on every board: it
was drawn and immediately cut away, and the only part that ever showed was the two-pixel
overlap, visible only through a translucent paused card.

**A tag lives inside its cell**, as a row above the surface. The card is shorter by the
tag's height, every card in that mode takes that height, and the gap above a tag is the
page's ordinary row gap. No overflow, no widened gaps, no cell arithmetic.

Two styles make LVGL render an object to an **intermediate layer buffer**:

| Style | Why it allocates |
|---|---|
| `clip_corner` | masks rounded corners by rendering to a layer and masking it |
| `opa` below `LV_OPA_COVER` | composites the whole subtree |

Both are per object, per frame. With a page rebuilt while the old one is still alive that
was thirty-six allocations, and the P4 emitted `lv_draw_layer_alloc_buf: Allocating layer
buffer failed` until it was reset.

So: `clip_corner` **only** in `HDR_BAR`, the one mode with an edge-to-edge child that
needs it. And a paused card dims by **mixing its colours toward the ground**
(`Card::tone()`), never by setting an opacity.

### 1.3 LVGL's draw walk recurses once per level of nesting

`loopTask` is where LVGL runs, and arduino-esp32 gives it `ARDUINO_LOOP_STACK_SIZE` —
**8192 bytes** by default. A card is nested deep: screen → column → host → page → cell →
surface → body → band → row → label.

**Every crash in milestone 2.4 was this**, and they all reported identically: `Stack canary
watchpoint triggered (loopTask)`, with a backtrace containing one alternating pair of
addresses repeated about a dozen times. That pair is LVGL walking down the tree, and the
repeat count is the tree's depth.

It was misread three times as a memory problem. The fixes that appeared to help — deleting
the old page before building the new one, dropping `clip_corner`, dropping the opa — all
helped by making the tree *smaller*, never by addressing its *depth*. Adding one band per
card put it straight back over the line.

`main.cpp` sets `SET_LOOP_TASK_STACK_SIZE(16 * 1024)`. **Measured headroom: 7556 bytes free
on `CYD_S3_3248`**, so peak use is ~8.8 KB — above the old limit, which is the whole story.

**The System Doctor reports the high-water mark.** If it trends toward zero as cards gain
nesting, that is the warning, and the answer is a flatter tree rather than a bigger stack.

---

## 2. The layout model

Both layouts are the **same vertical stack**, and that is the point — a value card and a
state card sitting side by side must put their names at the same height.

```
  ┌─────────────────────────────┐
  │ [tag row]   HDR_TAG only, outside the surface
  ├─────────────────────────────┤
  │ [header bar] HDR_BAR only, inside, costs the body a strip
  │                             │
  │  icon        ← out of flow, top-left corner
  │                             │
  │      MID      grows         │  the hero: a value+unit, or an icon disc
  │                             │
  │      NAME                   │  the location, or what the thing is
  │  batt            seen       │  STATUS, reserved whether or not it has content
  └─────────────────────────────┘
```

Three numbers, shared by both layouts, all in `Card`:

| | |
|---|---|
| `topBandHeight()` | the corner icon's size. **Not reserved in the stack** |
| `statusBandHeight()` | the taller of the icon face and the text face |
| `midGap()` | between the hero and the name |

### Why the corner icon is out of the flow

It sits in a corner the centred hero never uses. Reserving a row for it was paying for a
collision that cannot happen — and that row is exactly what broke `CYD_S3_3248`, where 22 px
is the difference between a status line fitting and not. Moving the name under the value
turned three rows into four, and the icon's row was the one that did not need to exist.

### Why the status band is reserved even when empty

A card whose sensor reports no battery must not sit its name lower than the one beside it
that does. `statusBandHeight()` takes the **taller** of the two faces that can appear there,
because sizing it to one of them made a card with a battery glyph sit its status line lower
than a card carrying only an age.

### Why the hero always charges for a header

`midHeight()` subtracts a header's height **whatever the card's mode is**. A hero sized from
the real band grew in `HDR_NONE`, where there is no header to pay for — so the same card's
disc was one size with a header and a larger one without. Sizing to the most constrained
mode makes a card's hero identical in all three, which is what a reader expects of the same
card wearing different chrome.

### Why a hero is clamped rather than scaled from its font

An icon disc sized at a multiple of its font's line height overflows a small cell. On
`CYD_S3_3248` the band is ~55 px and twice the line height wants 68. It was clipped top and
bottom **by its own parent**, and making the multiplier larger made it worse. The disc takes
the smaller of the font-derived size and the band it has.

---

## 3. What is derived, and from what

Nothing about size is declared twice. One physical fact per board produces everything:

```
  BSP_<BOARD>.h  WIDTH · HEIGHT · DIAGONAL_IN
        │
   bspPixelDensity()  →  real PPI
        │
        ├── bspUiScale()  =  PPI / 170   →  UI::sc()  →  every padding, radius, gap
        ├── UI::minTouch()  =  9 mm in real pixels
        ├── recomputeGrid()  →  columns, rows, cell size
        ├── gen_type_scale.py  →  UITypeScale.h   ← BUILD TIME
        └── gen_icon_font.py   →  UIIcons.h       ← BUILD TIME
```

The last two resolve at build time because **LVGL compiles fixed bitmap fonts** — a face has
to be chosen before the binary exists. That is also why `lv_display_set_dpi()` does not help
with type: every DPI-sensitive path in LVGL goes through `LV_DPX_CALC`, and **no DPI path
touches fonts**. Verified: the callers are `lv_obj` (default size of an unsized object),
`lv_obj_scroll` (minimum scrollbar), `lv_slider` (click area), `lv_arc` (touch tolerance)
and `lv_theme_default` (radius, border, padding, shadow). That is the complete list.

`UI::sc()` and LVGL's `lv_dpx()` are the same arithmetic with different reference constants
(170 vs 160). That duplication is known and logged in `FUTURE_IMPROVEMENTS.md`.

---

## 4. Variants

`VAR_FULL` and `VAR_COMPACT` are **derived from the cell, not declared**. A card measures
whether its cell can seat a hero, a name and a status line at the type scale's sizes, and
draws **less** when it cannot — never the same thing smaller, which would undo the work the
generated type scale exists to do.

- `ValueCard` drops the status corners. `cards.md` §1 already treats that row as absent when
  there is nothing to put in it; a cell too small to seat it is the same situation arriving
  from the other direction.
- `StateCard` drops the **name** and keeps the icon, because `cards.md` §4 says state *is*
  the icon and its colour.

**The test keeps a margin.** A font's line box is taller than its ink, labels round up, and
the body's padding is approximate at that point. Landing within a pixel of the cell meant a
121 px card claimed a full layout and then overflowed. Being slightly too eager to go
compact costs a status line; being slightly too reluctant breaks the card.

`-D DEBUG_CARDS` prints the decision per card: `[Cards:debug] sensor: cell 121 need 119 ->
full`. This threshold was guessed at twice and both guesses were wrong — the numbers end the
argument.

---

## 5. State, and where its verdict lives

A command's outcome is a property of the **entity** (`Entity::cmdFailed`), not of the card
that sent it. Cards used to track their own commands in bitmasks, which meant a parent card
and a child card bound to the same switch could disagree — and which answer you got depended
on which one you had tapped.

Reading the flag off the entity makes a parent's state derive from **where its children are**
rather than the path they took:

- every commandable child failed → `ST_REFUSED`
- some → `ST_PARTIAL`, and the body keeps showing what the children are actually doing

Two details that are not obvious and cost a round each:

- **A successful echo produces no notification.** It carries the value the optimistic write
  already applied, so `setValue()` finds nothing changed and declines to dirty the entity.
  Resolution is therefore **polled** from `pollState()`, never awaited from a snapshot.
- **The verdict compares against what was commanded**, not against `prevValue`. `prevValue`
  is deliberately not overwritten by a second command inside the same window, so it is the
  wrong baseline — tapping a switch twice quickly returns the value to it and a perfectly
  successful command was being called a failure.
