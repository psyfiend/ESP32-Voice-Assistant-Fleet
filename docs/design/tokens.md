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
