# Design tokens — Phase 2.2 (issue #13)

**Status: implemented and hardware-verified 2026-09-10.** Design was agreed before the code, per
ROADMAP §2.2. Everything below held up on glass except the grid target width, corrected in §6.

Companion to `docs/design/cards.md` (what a card contains) and `docs/design/startup.md` (where the
UI layer sits). This file covers **how things look** and nothing else — see §1 for why that
boundary matters.

---

## 1. What tokens are, and what they are not

Three separate layers, deliberately not merged:

| Layer | Answers | Lives in | Milestone |
|---|---|---|---|
| **Design tokens** | What colour is "active"? How round? Which font size? | `include/UITokens.h` | **2.2** |
| **Card type rules** | A light uses state colour; a sensor uses a quantity tint | `Card` subclasses | 2.4 |
| **Card instance config** | *This* card shows `deck_temp`, spans 2, sits at row 1 col 3 | Build sheet | Phase 3, #20 |

If tokens carried entity bindings, changing a colour would mean editing card definitions —
inverting ROADMAP §4's layering, where the build sheet is the top layer and tokens sit near the
bottom. The token header answers *"what does an active light look like"*; the build sheet answers
*"there is a light card here, bound to `light.porch`"*.

---

## 2. DPI and UI scale — the numbers, measured

The current scheme is `-D HIGH_DPI_DISPLAY` on three environments, which sets `UI_SCALE 1.5f` and
`lv_display_set_dpi(disp, 150)`. Both numbers were picked during `WS_P4_4B` bring-up to make a
720×720 board look like the 480×480 board beside it, and have been applied unchanged since.

**Computing the real pixel density from resolution and panel diagonal:**

| Board | Resolution | Diagonal | **PPI** | Current scale |
|---|---|---|---|---|
| `CYD_S3_3248` | 320×480 | 3.5" | **165** | 1.0× |
| `CYD_P4_1060` | 1024×600 | 7" | **170** | 1.0× |
| `WS_P4_7B` | 1024×600 | 7" | **170** | 1.0× |
| `WS_S3_4B` | 480×480 | 4" | **170** | 1.0× |
| `CYD_S3_8048` | 800×480 | 5" | **187** | 1.0× |
| `WS_S3_5B` | 1024×600 | 5" | **237** | 1.5× |
| `WS_P4_4B` | 720×720 | 4" | **255** | 1.5× |
| `WS_P4_5` | 720×1280 | 5" | **294** | 1.5× |

**The first finding is reassuring: the 1.0/1.5 split is not arbitrary.** Sorted by density the
fleet falls into two clean clusters — 165–187 PPI and 237–294 PPI — with a 50-PPI gap between
them and no board anywhere near the boundary. Whoever drew that line during bring-up drew it in
the right place, by eye, and it has held for five boards added since.

**The second finding is the real problem: the high-DPI cluster spans 24%.** `WS_P4_5` at 294 PPI
and `WS_S3_5B` at 237 PPI both get exactly 1.5×, so identical token values render **24% physically
smaller** on the P4-5. A 16 px label is 1.7 mm tall there and 2.1 mm on the 5B. That is a real
difference in legibility, and it is invisible in the browser bench because the bench models pixels,
not millimetres.

### Decision: derive the scale, stop declaring it

`UI_SCALE` becomes computed rather than a build flag:

```
scale = PPI / PPI_REFERENCE          (PPI_REFERENCE = 170, the low cluster's centre)
```

| Board | Derived scale | vs today |
|---|---|---|
| `CYD_S3_3248` | 0.97 | ~same |
| `CYD_P4_1060`, `WS_P4_7B`, `WS_S3_4B` | 1.00 | same |
| `CYD_S3_8048` | 1.10 | **+10% — currently under-scaled** |
| `WS_S3_5B` | 1.40 | −7% |
| `WS_P4_4B` | 1.50 | same |
| `WS_P4_5` | 1.73 | **+15% — currently under-scaled** |

Two boards are visibly wrong today and one of them is a dev target. `CYD_S3_8048` sits between the
clusters and got rounded down; `WS_P4_5` is the densest panel in the fleet and is being treated as
though it were the 5B.

**This needs one new BSP field: `DisplayConfig.DIAGONAL_IN`** (tenths of an inch as a `uint8_t`, so
`35` = 3.5", avoiding a float in a `const` struct). Eight headers, one line each. In exchange
`UI_SCALE` stops being a per-environment flag anyone has to remember, and `-D HIGH_DPI_DISPLAY`
can be retired entirely.

`lv_display_set_dpi()` should then receive the board's **real** PPI rather than a blanket 150 —
LVGL uses it for `lv_dpx()` and its own default sizing, so feeding it a number that is wrong by
up to 96 PPI has been quietly distorting anything that relies on it.

### Caveat worth deciding separately: distance, not just density

Pure PPI scaling makes everything the same *physical* size on every board. That is exactly right
for **touch targets** — a fingertip is about 9 mm regardless of which panel it lands on — and
arguably wrong for **text**, because a 3.5" panel held at arm's length and a 7" panel on a wall
across the room want different apparent sizes.

So the scale may eventually need two factors: a density term (derived, above) and a per-board
viewing-distance nudge (declared, small, defaulting to 1.0). **Not building that yet** — get the
density term right first and see whether anything still looks wrong on glass.

### A token that falls out of this for free

**Minimum touch target.** 9 mm is 60 px at 170 PPI and 104 px at 294 PPI. Once PPI is known this
becomes a derived token rather than a guess, and card and control sizes can be validated against
it instead of being eyeballed per board.

---

## 3. Structure

Flat structs with `const` instances and designated initialisers — the same idiom as `Fleet_BSP`,
because the project already knows its shape and its traps are documented (declaration order,
macro collisions). One idiom beats two.

Four groups, deliberately not nested:

- **`UIPalette`** — ground, surface, text, accent, the **semantic state palette** (active / idle /
  ok / warn / bad) and the sensor quantity tints. State colours are separate from `ACCENT` on
  purpose: accent is decoration, state carries meaning (`cards.md` §0).
- **`UIMetrics`** — radius, padding, border width and opacity, shadow, card header height. Per
  scheme, because the owner's light and dark schemes differ only here (dark takes a `1px lighten
  @40%` border, light takes none).
- **`UIGrid`** — target card width, aspect, gap, inset. **Columns are always derived from target
  width, never declared.** Defaults computed from resolution and DPI, overridable per board later
  if a board disagrees.
- **`UIType`** — the font shortlist and icon size. Fleet-wide.

### Font cost — measured 2026-09-10, and it corrects an earlier claim here

An earlier note in this file said `lv_conf.h` enabling every Montserrat size from 8 to 48 meant
21 faces were "compiled into every board". **That was wrong.** Enabling a size in `lv_conf.h` puts
it in the archive; the linker only pulls in the object files something actually *references*, and
discards the rest. Trimming `lv_conf.h` would therefore save nothing.

What costs flash is **referencing another distinct size**, and the price is high. Measured on
`WS_P4_5` by pointing `UIType::HERO` at a face nothing else used and then away again:

| Change | Flash |
|---|---|
| Before the reference page | 1,478,486 |
| + page, with `HERO` = `montserrat_48` | 1,662,840 |
| + page, with `HERO` = `montserrat_40` | **1,566,104** |

**One font face = 96,736 bytes.** The reference page's own code is only a few KB; essentially the
entire 184 KB jump was two new font faces being linked.

Three consequences:

1. **The type shortlist is a flash budget, not a style choice.** Every distinct size in `UIType`
   costs roughly 85–97 KB. Four sizes is ~350 KB; the fleet currently references nine across
   `UIToolkit` and `UITokens`.
2. **`HERO` shares `VALUE`'s face** until a fullscreen card genuinely needs a larger one. Paying
   96 KB for something nothing draws was not a trade worth making.
3. **This is the number #14 needed for the icon question.** An MDI subset is a font too. A
   card-sized icon set will be far smaller than a 48 px full-ASCII face — fewer glyphs, smaller
   glyphs — but it is the same order of magnitude, and it has to be budgeted against the type
   scale rather than considered separately.

Access goes through one namespace rather than globals, so runtime scheme switching has a single
choke point:

```cpp
namespace UI {
    const UIPalette &pal();
    const UIMetrics &met();
    const UIGrid    &grid();
    const UIType    &type();
    void setScheme(const UIPalette *p, const UIMetrics *m);   // runtime, repaints
    int32_t sc(int32_t v);                                    // absorbs UIToolkit::sc()
    lv_color_t c(uint32_t hex);
    lv_style_selector_t part(lv_part_t p, lv_state_t s);      // kills the #13 deprecation warning
}
```

**Runtime scheme switching is in.** Cards must therefore read `UI::pal()` when they build or
restyle, never cache a colour. Cheap to honour now, invasive to retrofit later.

---

## 4. Agreed starting values

From the bench, 2026-09-10. Shared across all four of the owner's signed-off configs:

```
radius 10 (dark) / 12 (light) · pad 5 · gap 12 · inset 14 · shadow 8
montserrat_40 value · montserrat_16 name · icon 26
state shows as: icon disc      actor name: below
card header: area colour       sensor icon tints: on      status row: on
system header: clock left · board name centre · charge/batt/mqtt/wifi right
```

Per-scheme:

| | Dark (Slate) | Light (Paper) |
|---|---|---|
| Ground | `#1A1F27` | `#E8E9EC` |
| Surface | `#212429` | `#FFFFFF` |
| Border | 1px lighten @40% | none |
| Accent | `#9B7BD4` violet (3248) | `#E0A53C` amber (big screens) |

Per grid density (the only genuinely per-board values):

| Layout | Target card | Aspect |
|---|---|---|
| 6×3 large high-DPI | 110 px | 1.00 |
| 5×3 large / 3×2 landscape 3248 | 135 px | 0.80 |
| 2×3 portrait 3248 | 135 px | 0.90 |

**`pad 5` is deliberately tiny** — card padding is doing almost nothing and the grid gap carries
the visual separation. That is what makes the dense grids read well, but it means cards are
effectively edge-to-edge content, so text clipping at small sizes is the thing to watch on the
3248.

**These values are expected to change once they are on glass.** Every one of them is a one-line
edit in a header, which is the entire point of putting them here instead of in card code.

---

## 5. What this milestone touches

The existing panels currently hold their colours as `lv_color_hex()` literals scattered through
`UIToolkit.cpp`, `Panel_Header.cpp`, `Panel_System.cpp` and `Panel_Display.cpp`, and their fonts in
`UIToolkit::Font_*`. Those all move into tokens.

**Consequence: 2.2 is not a pure-addition milestone.** The existing, known-good UI will change
appearance slightly as it starts reading tokens. That is the owner's stated preference — *"I would
prefer all visual customization and specification not live in the actual UI code"* — and it is the
right call, but it means the Phase 2.1 regression baseline ("does it look identical") no longer
applies from here on.

---

## 6. What hardware changed — measured 2026-09-10

Flashed to `WS_P4_5` (294 PPI, 1.73×) and `CYD_S3_3248` (165 PPI, 0.97×, portrait).

**The grid derivation works.** One token set, two boards, no per-board layout code:

```
[UI] Scheme "Fleet" | 5x3 grid of 230x210 px | scale 1.73x (294 PPI)   WS_P4_5
[UI] Scheme "Fleet" | 2x3 grid of 141x144 px | scale 0.97x (165 PPI)   CYD_S3_3248
```

**`TARGET_CARD_W` is 130, not the 135 the bench used.** The browser bench modelled `WS_P4_5` at a
hardcoded 1.5× because that is what `HIGH_DPI_DISPLAY` gave it; the board is really 1.73×. Every
value tuned in the bench therefore rendered ~15% larger on real glass, and at 135 that cost a
whole column and a whole row. Corrected in both the firmware and the bench.

**The measured card cost.** `CYD_S3_3248`, 8 sample cards, repeated across all three schemes:

```
713-723 bytes per card | pool 35% used, 29% frag (stable)
```

The 10-byte spread is allocator noise — block headers, alignment, and which free hole each of the
24 objects lands in. The schemes build identical widget trees and differ only in style property
values, so scheme choice costs nothing.

**Font range is ASCII plus a small symbol set.** `·` (U+00B7) and `—` (U+2014) rendered as tofu
boxes. The degree sign is in range and works. Rule for anything drawn on these panels: **stay
ASCII, except `°`**, unless the font range is deliberately extended — which costs flash at the
rate in §3.

**Scrolling needed taming.** `UI::tameScroll()` — vertical-only, no elastic rubber-banding, no
scrollbar. The springback at the end of a scroll looks poor at the refresh rates the S3 boards
manage, and a slightly-too-wide row must never be able to start a sideways drag. Call it on every
scrollable container.

---

## 7. Shape: the card window's millimetres (2.12 P2)

The card window sizes what a finger touches and what an eye reads in real millimetres on glass,
so a target or a line is the same physical size on every board (its `mm()`, the derivation
`UI::minTouch()` uses). Until 2.12 those millimetres were ~150 literals across the window's code
(`mm(1.6f)`). They are now **`UIShape`**, read through **`UI::shape()`**: declared in
`include/UI/UITokens.h` §2b, with a comment on each saying what it draws, and valued in
`src/UI/UITokens.cpp`.

**Using them.** `mm(UI::shape().S5)`, or `pm(...)` for the `P_` group, which is in "P4_5
millimetres" - the same share of every window as on the P4_5 (`card-sheet.md` §15). Never through
`UI::sc()`: these are not logical pixels. No number goes into a popup file: a new size is a new
token with its comment. An effect's own dice may stay named constants beside the effect (the
sparks' `SPARK_*_MM` in `src/UI/Kit/KitSparks.cpp`), since nothing else draws one.

**Its own struct, not `UIMetrics`** (the 2.12 brief said `UIMetrics`; this is the zero-risk
reading of it). `UIMetrics` is per scheme and in logical pixels; these are fleet-wide and physical.
Fifty-six floats in `UIMetrics` would give every scheme a copy to keep in step, and changing that
struct's layout is the build-cache trap (CLAUDE.md). One `const` instance, in flash, no RAM. If a
scheme ever wants its own corners, `UI::shape()` can return a per-scheme copy and no caller changes.

**The scales** (as merged on `feat/ui-followups`, #97 - below). Until then every value was the one
the window already had - the harness found no pixel changed on three boards.

| Group | Tokens, in millimetres |
|---|---|
| Radius | `R_CONTROL` 1.6, `R_PANEL` 1.6, `R_WINDOW` 2.4, `R_CHIP` 3.0 |
| Spacing | `S1` 0.6, `S2` 0.8, `S3` 1.0, `S4` 1.2, `S5` 1.6, `S6` 2.0, `S7` 2.4, `S8` 3.0, `S9` 4.0, `S10` 5.0 |
| Lines | `LINE_HAIR` 0.2, `LINE_MID` 0.3, `LINE_HEAVY` 0.4, `LINE_PLOT` 0.45, `DASH` 0.6, `RING_W` 0.4, `RING_PAD` 0.3; letter spacing `TRACK_PILL` 0.2, `TRACK_TAB` 0.4 |
| Heights, widths | `H_CONTROL` 6.0, `H_CHIP` 6.0, `H_ROW` 7.2, `H_BAR` 0.17 (0.5 until #97: 2 px at 294 and 255 PPI, 1 px at 170), `H_TABS` 6.0, `W_SIDE_MIN` 6.0, `W_WINDOW_MAX` 100, `W_ROWS_MAX` 70, `W_DIALOG_MAX` 90, `W_DIALOG_BTN` 24, `SWATCH` 6.5, `CARET_W` 2.4, `CARET_H` 1.0 |
| Finger travel | `SLOP_SELECTOR` 1.0, `SLOP_KNOB` 1.2, `SLOP_HOLD` 4.0 |
| The hero (`pm()`) | `P_GAP` 4.0, `P_STACK_GAP` 3.0, `P_GRIP_INSET` 1.4, `P_GRIP_H` 0.5, `P_GRIP_MIN` 5.0, `P_MARK_H` 2.6, `P_MARK_LINE` 0.35, `P_SCENE_H` 9.0, `P_SCENE_GAP` 1.4, `P_SCENE_PAD` 2.4 |
| A cluster (2.11, K89-K103; the "2.11 Clusters" mock's) | `C_HDR_H` 5.2 its header row, `C_PAD` 1.1 inside its frame, `C_TILE_GAP` 0.8 between mini tiles, `C_SWATCH_W` 1.1 / `C_SWATCH_H` 2.4 the group's swatch, `C_VALUE_GAP` 1.3 between header values, `C_TILE_MIN` 7.5 the least a tile is each way ("tight", K103) |
| Floating (D-35, "Panel Edges") | `HALO` 5.0 the halo round a header slot's panel and every window, `LINE_FADE` 2.5 the bar's line fading out before an open panel's walls |

The spacing literals clustered into ten steps from 0.6 to 5.0 mm. Several tokens share a value
(`R_CONTROL` and `R_PANEL`; `H_CONTROL`, `H_CHIP`, `H_TABS` and `W_SIDE_MIN`): they are different
roles, and stay separate so one can change alone.

**The near-equal tokens, merged** (owner, 2026-10-14, TEST_2.12 Q1: "I doubt I'd even notice the
difference"; built on `feat/ui-followups`, #97, with the harness's before/after pictures in
`docs/archive/TEST_UI_FOLLOWUPS.md`; `v0.2.15`). Pixels before line minimums, on WS_P4_5 / WS_P4_4B / CYD_P4_1060
(294 / 255 / 170 PPI):

| Merge | Was | Now | |
|---|---|---|---|
| `S_GUTTER` 1.5 into `S5` 1.6 | 17 / 15 / 10 | 19 / 16 / 11 | the header row and the chart's gutters |
| `S_ROW_MIN` 0.55 into `S1` 0.6 | 6 / 6 / 4 | 7 / 6 / 4 | only the P4_5 moves, by 1 px |
| `S_CHIP_SIDE` 2.2 into `S7` 2.4 | 25 / 22 / 15 | 28 / 24 / 16 | the span chips' sides |
| `LINE_FINE` 0.25 into `LINE_MID` 0.3 | 3 / 3 / 2 | 3 / 3 / 2 | no change on these three; +1 px on WS_S3_5B and CYD_P4_4880 |
| `H_MORE` 7.0 into `H_ROW` 7.2 | 81 / 70 / 47 | 83 / 72 / 48 | "Show more" as tall as a panel row |
| `S_MEMBERS` 1.4 into `S5` 1.6 (or `S4` 1.2) | 16 / 14 / 9 | 19 / 16 / 11 (or 14 / 12 / 8) | **the owner's choice** - both pictured; built: `S5` |
| `R_CONTROL` 1.2 and `R_BUTTON` 1.6 as one `R_CONTROL`, 1.6 (or 1.2) | 14 / 12 / 8 and 19 / 16 / 11 | 19 / 16 / 11 (or 14 / 12 / 8) | **the owner's choice** - both pictured; built: 1.6, the corners the owner set on glass for the dialog's buttons and the squared selector (K27) |

## 8. The page header's slots, in logical pixels (2.8)

The header is `sc(SYSTEM_HEADER_FULL_H)` - 35 logical px - tall on every board, so what sits in it
is sized in the same logical pixels: **`UIHeader`**, read through **`UI::hdr()`**
(`include/UI/UITokens.h` §2c), passed to `UI::sc()`. One `const` set, fleet-wide. `GLYPH`, the
dots, `SLOT_GAP` and `LINE` are the values the bar had before it had slots; the frames are the
"2+" chip of #19's mock; the drawer's are the system panel's shape, named.

| Token | Value | What uses it |
|---|---|---|
| `GLYPH` | 26 | a status glyph's drawing box - **the same bare or framed** (round 3, D-24: a frame never shrinks it; round 2's 28 bare / 19 framed are gone with `GLYPH_FRAMED`) |
| `FRAME` | 28 | a glyph's frame: the rounded square, the circle - round the glyph, not inside its box |
| `FRAME_R` | 7 | the rounded square's corners |
| `FRAME_MIX` | 14 | the rounded square's shade: % of `TEXT` over the bar |
| `SLOT_GAP` | 8 | between the things in one slot |
| `SLOT_PAD` | 6 | a slot's sides - how far its tap area and its open tab reach past what it shows (4 in round 1, when slots were quarters) |
| `EDGE` | 8 | from each end of the bar to its side's first slot (round 2); with `SLOT_PAD`, the first thing sits where round 2's mock drew it (24 px from the P4_5's edge) |
| `SPACE` | 2 | between two slots in one place; their pads make the rest |
| `CLEAR` | 12 | at least this between a side and the middle - closer, the lowest keep gives way (`Layout/HeaderPlan.h`) |
| `DOT_ON`, `DOT_OFF`, `DOT_RING`, `DOT_GAP` | 11, 8, 2, 5 | the page's dots: this page's, another's ring and its line, between them |
| `MARK` | 12 | the error mark beside a slot's dashes |
| `LINE` | 2 | the bar's accent line - and the outline of an open slot and its panel, in the accent (round 3, D-23) |
| `JOIN_R` | 6 | a cove beside an open tab, in the bar: the rounded corner where the tab's side meets the bar's line (round 3, D-23; the panel's own flares went in round 4, D-30) |
| `DRAWER_W` | 260 | a slot's panel at most |
| `DRAWER_R`, `DRAWER_PAD`, `DRAWER_GAP`, `DRAWER_LINE` | 15, 10, 8, 2 | a slot's panel: its bottom corners, inside, between rows, its edge |

Logical rather than millimetres (`UIShape`) because the bar they sit in is logical; the two agree
to within the scale's clamps (1 logical px is ~0.15 mm wherever `sc()` is not clamped).

**The status glyphs' colours (round 3, D-24 / D-27)** are palette tokens, `UIPalette`'s `SIG_*`,
one set per scheme family - a glyph's colour is its link's state, so it lives with the scheme:

| Token | Dark schemes (Fleet, Midnight) | Linen | What it means |
|---|---|---|---|
| `SIG_STRONG` | `34D399` | `1B7A3A` | Wi-Fi strong |
| `SIG_GOOD` | `4ADE80` | `2F8F4E` | Wi-Fi good; a link that is up |
| `SIG_WEAK` | `FBBF24` | `9A6412` | weak, or a link that is quiet |
| `SIG_BAD` | `F87171` | `B4321F` | down |
| `SIG_BUSY` | `60A5FA` | `1D5FD6` | joining, connecting |
| `SIG_OFF` | `64748B` | `8A8076` | switched off |
| `SIG_LINK` | `A78BFA` | `6B4FC8` | the MQTT link's arrows |

The dark greens are the mocks' softer ones; Linen's are darker so they read on its light bar (T9's
"Wi-Fi's green all but vanishes on Linen"). The glyphs themselves are numbers, not tokens:
`UI/GlyphShapes.cpp` holds each stroke on a 24-unit grid, as the mocks drew them.

**The header's words have their own type role, `HEADER`** (round 4, D-30): TAG's millimetre
target and TAG's scale on every board, except the P4_5 (`gen_type_scale.py`, `HEADER` 1.18): 26 px,
2.25 mm, matching the 4B's 22 px within 2%. `UIToolkit::Font_Header` is the larger of `HEADER` and
`Font_PanelHeader`.

**The active frames** (rounds 4-6, D-30 to D-32) are `UIMetrics` tokens in logical px, drawn in
`ST_ACTIVE` (`Cards/CardFrame.cpp`): `BRACKET_W` 6 (the bracket's top and bottom bars),
`ACTIVE_EDGE` 2 (its sides), `ACTIVE_RING` 3 (the corner cut's frame and the edge of each cut),
`GLOW_DEPTH` 18 and `GLOW_OPA` (how far the light reaches in from a lit edge, and how strong it is
there: 115 of 255 on the dark schemes, 150 on Linen), `CUT_PCT` 24 (the corner cut's chamfer, % of
the card's height), and `ACTIVE_INK` (round 6: how far the frame's edge goes from the active
colour toward the scheme's text - 0 on the dark schemes, where the frames glow, 35 on Linen, where
they go darker to stand out; the light goes half as far). The corner cut's shadow is the card's
own (`SHADOW`, `SHADOW_Y`, `SHADOW_OPA`), measured from LVGL and painted around the cut (round 7,
D-33). `UI::darkest()` (the scheme's darkest, 60% of the way to black) inks lit glass, warmed.
**Linen's `ST_ACTIVE` is terracotta `D47F45`**, the owner's pick on glass (round 6; olive
`9AA35A` before) - `/popup?active=RRGGBB` tries another without a build.
