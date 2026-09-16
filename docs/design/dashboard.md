# The dashboard, as data — milestone 2.5

**What this document is for.** `cards.md` says what a card contains and `card-layout.md` says how
one is drawn. This says how a *page* of them is described, placed, and degraded when it does not
fit — and why the description is a struct rather than a sequence of calls.

Read `card-layout.md` before moving anything inside a card. This document is about the grid the
cards sit in.

---

## 1. Why a page is data

Everything on screen through milestone 2.4 was constructed imperatively in `CardDemo.cpp`:
`place("deck_temp", "Deck", "Outdoor", ...)`, fifteen times. That is a page, but it is a page
written in the one form that cannot be loaded from a file, sent over a wire, or edited without a
rebuild — and milestone 3.3's whole job is to do exactly those things.

So 2.5's page is a `PageSpec` holding an array of `CardSpec`, in `include/Cards/PageSpec.h`, and
the first one is `include/Dashboards/Dashboard_Fleet.h`. The build sheet's JSON loader fills in
these same structs, which makes 3.1 "write down what this table already is" and 3.3 "a parser"
rather than a second design.

The imperative path still exists — the comparison bench needs it — but both paths now finish
through `CardPage::commit()`. There is one placement implementation.

### What a spec carries, and the one thing it does not

`HANDOFF.md`'s list of what 2.4 established a sheet entry must express, all present: a header
mode (or inherit), spans and priority, a variant override, staleness override, area, label.

**There is no domain field**, and that is deliberate. A card's type comes from its primary
entity's `EntityKind` through `cardForEntity()`, which is how every card in the project is already
built. A domain in the spec would let a sheet disagree with the registry about what a thing is,
and there is no good answer to that disagreement. When the build sheet learns to *declare*
entities as well as place them, the domain belongs on the declaration, next to the topic.

### One definition, whole fleet

`Dashboard_Fleet.h` renders on every board. Nothing in it is board-specific because the three
things that vary are all derived rather than declared:

- the **grid** comes from the panel — `WS_P4_7B` gets 7 cells across, `CYD_S3_3248` portrait gets 2
- cards that do not fit are dropped **by priority**, so a small board keeps what is worth keeping
- a spec whose entity is missing from this board's registry is skipped and reported

So the 3248 renders a true *subset* of the 7B rather than a different page, and flashing it is a
free check on the whole degradation scheme. Per-board pages are wanted later (the owner's call,
2026-09-15: "per-board is definitely important later"); when they arrive they want to be another
`PageSpec` selected by the board identity macro every BSP header already defines — the mechanism
`ConnectivityDefaults.h` uses, and no new machinery.

---

## 2. Sub-grid units

ROADMAP Q3b, decided 2026-09-03 and implemented here:

> Author the page as N×M **cells**, but allocate N·sub × M·sub **units** underneath. Cards span in
> units. A quarter-page card on a 3×3 page is 3×3 units — exactly expressible. A normal card is
> 2×2 units.

`subdivision` is a per-page property defaulting to 2. **Every span, minimum and coordinate in
`CardPlacement` is in units**, which is the one place 2.5 changed the meaning of a 2.4 struct — the
fields are the same, the unit is not. Defaults moved from `1` to `2` accordingly, so an ordinary
card is unchanged.

### The subdivision is dimensionally transparent

Worth stating because it is the reason this was safe to do late. Columns are `LV_GRID_FR(1)` per
unit track with a gap between every track. For `c` cells of width `W`:

```
one cell, undivided:   W/c - g + g/c
sub units, spanning 2: 2·(W - (2c-1)g)/(2c) + g   =   W/c - g + g/c
```

Identical. The gaps absorb the subdivision exactly, so **no card changes size when a page
subdivides**. Row heights are fixed pixels rather than fractions, and there integer division
loses up to `sub-1` px per cell — which is why `commit()` hands each card the height it *actually*
got rather than the one the token asked for (see §4).

---

## 3. Placement, and the validator

Q3b chose "placement at unit granularity, plus a validator" over a constraint solver, explicitly so
that layouts stay predictable and debuggable. Both halves are implemented.

**Flow** is the default: the first free position wins, and at that position the widest span that
fits wins. That ordering preserves 2.4's rule — a card that fits at its minimum beside its
neighbour beats a card sitting alone on a new row at full width.

**Explicit placement** sets `CardPlacement::col` / `.row` in units (`-1` = flow). Occupancy is
tracked in a bitmap, so pinned and flowed cards coexist without either having to know about the
other.

**An invalid pin is reported and then flowed, not dropped.** Out-of-bounds or already-occupied
prints through `DBG_CARDS` and marks the card `[pin rejected]` in the System Doctor. The card is
still the one the author asked for; only its coordinate was wrong, and silently losing it would be
a worse answer than putting it somewhere visible and saying so.

Validation is at **runtime**, not compile time as issue #16 originally suggested. The structs are
`const`, not `constexpr`, so they are not usable in constant expressions — the same reason
`CLAUDE.md` already records for preferring runtime checks to `static_assert` on BSP values.

---

## 4. Priority degradation

Carried on every card since 2.4 and read by nothing until now.

A page does not scroll — the owner, unprompted: *"There should never be any scrolling of cards on
any dashboard pages... Only when swiping or navigating to a new page entirely should new entities
be on the screen."* So an over-subscribed page has to drop something, and **which** is what
`priority` answers.

The algorithm, in `CardPage::plan()`: place everything in declaration order; if any card cannot be
placed, **drop the lowest-priority card still alive and re-plan the whole page**.

Dropping the lowest-priority card rather than the one that failed is the part worth defending.
They are different cards whenever a low-priority card was declared early, and dropping the failure
would make the outcome depend on declaration order — which is precisely what `priority` exists to
stop. Ties go to the later declaration, so an author listing equals keeps the reading order they
wrote.

`Dashboard_Fleet.h` uses four bands — `PRI_CRITICAL` 220, `PRI_NORMAL` 160, `PRI_NICE` 100,
`PRI_DEBUG` 40 — and puts the panel's own telemetry in the last one. A two-column portrait board
showing its free heap instead of the temperature outside would be exactly backwards, and that is
now a property of the data rather than of luck.

### The cell height a card is told

`commit()` calls `Card::setCellHeightPx()` before `build()`. Previously `resolveVariant()` and
`midHeight()` derived the height from `UI::grid()` and the card's own row span. That is wrong once
a span is in units, and it was already fragile: `UI::grid()` is global state that any other page
can move out from under a card that is mid-build. The page knows the answer exactly.

---

## 5. Where the dashboard lives on screen

`GUIManager` owns a host container between the header and the deck, and the page fills it. The
grid derives from **that container**, never from `bsp_display.WIDTH/HEIGHT` — which would be wrong
on every rotated board and wrong here regardless, since the cards do not get the whole screen.

Z-order is `touch overlay / dashboard / deck / system panel / header`. The dashboard sits *below*
the deck deliberately: an expanded accordion panel covers cards rather than pushing them, which is
what the owner asked to see.

**The deck costs a row.** A collapsed panel is `sc(85)` and the deck pads itself by `sc(10)`, so on
`WS_P4_7B` it is ~105 px of 600 — enough to take the page from four rows to three. That is a real
trade, not a preference, which is why the deck is one of the knobs below.

---

## 6. The two grid knobs

They are two, not one, and confusing them cost a round of explanation:

| Token | Decides | Notes |
|---|---|---|
| `TARGET_CARD_W` | **columns** | as many whole target-width cards as fit the width |
| `ASPECT_PCT` | **rows** | a hint: `cellW × pct` suggests a height, and the row count follows |

**Neither sizes a card.** Both counts are then stretched to fill the viewport exactly, so the ratio
a card ends up with is an *output*. Nothing constrains a card to a ratio, and a grid of 6×3 at
156×131 is as reachable as 7×3 — on `WS_P4_7B` the six-column case misses its third row by four
hundredths of a row, and the fix is in `ASPECT_PCT`, not in the width.

Both are exposed on the System panel (`Col -/+`, `Row -/+`, `Deck`) because that drawer opens over
the **real** dashboard. The bench derives its grid from a different host — a screen minus a button
bar — so a number tuned there would be right for the bench and wrong for the page that boots.

Each knob **rebuilds** rather than re-lays-out, because a card decides compact-vs-full from its
cell height when it is built; re-placing the same cards would show nothing.

### The number to aim at

`Card::resolveVariant()` computes what a full card needs from the type scale, so the threshold is
calculable rather than a matter of taste. On `WS_P4_7B` at 170 PPI — VALUE montserrat_40 (line
height 44), NAME 18 (21), status band 20, padding 10, margin 8:

| Header mode | Row height needed for a full card |
|---|---|
| `HDR_NONE` | 109 px |
| `HDR_BAR` / `HDR_TAG` | 129 px |

That also explains the open item 2.4 left: on the bench the row height was 116 px, and 129 − 116 =
**13** — the "~13 px short" recorded as unexplained in `HANDOFF.md`.

With the deck shown, the 7B's row height is quantised to 202 px at 2 rows, 131 at 3 and 95 at 4, so
3 rows is the practical maximum and it clears the threshold by **two pixels**. Hiding the deck
moves 3 rows to 166 px. `DEBUG_CARDS` is enabled on that environment so every card prints
`cell N need N -> variant` and the knob-turning is instrumented rather than judged by eye.

---

## 7. Units on a card

The owner always wants Fahrenheit, and both must be supported and overridable — his Home Assistant
reports 34 of 35 temperature sensors in °F and exactly one in °C, so a fleet preference *without*
conversion would render that one card wrong and it would look identical to a working card.

Resolution is fleet default → page → card, via `TempUnit` in `CardTypes.h` and `cardSetTempUnit()`.

**Conversion happens at format time and nowhere else.** The registry keeps whatever the source
said, because that value is what an echo is compared against, what an optimistic write reverts to,
and what our own HA discovery would publish. Converting on the way in would corrupt all three, and
the damage would be invisible.

Two consequences worth knowing: the unit *label* comes from `cardDisplayUnit()` rather than
`desc.unit`, because printing the source's unit beside a converted number is a caption that lies;
and a converted integer is promoted to one decimal, because 22 °C is 71.6 °F and printing `71`
would be a rounding the source never made.
