# Design tokens

How things look: `include/UI/UITokens.h` declares the tokens, `src/UI/UITokens.cpp` values them.
Companion to `docs/design/cards.md` (what a card contains) and `docs/design/startup.md` (where the
UI layer sits). The rules for using tokens are in CLAUDE.md ("Design tokens") and `ui-architecture.md`
§3.

---

## 1. What tokens are, and what they are not

Three separate layers:

| Layer | Answers | Lives in |
|---|---|---|
| **Design tokens** | What color is "active"? How round? Which font size? | `include/UI/UITokens.h` |
| **Card type rules** | A light uses state color; a sensor uses a quantity tint | `Card` subclasses |
| **Card instance config** | *This* card shows `sensor.x`, spans 2, is in group Kitchen | the settings file (`Settings.h`, `CardAdded`) |

If tokens carried entity bindings, changing a color would mean editing card definitions. The token
header answers "what does an active light look like"; the settings file answers "there is a light
card here, bound to `light.porch`".

---

## 2. DPI and UI scale

The scale is derived from the panel's real pixel density, not declared:

```
PPI   = diagonal pixels / DisplayConfig.DIAGONAL_IN       (bspPixelDensity())
scale = PPI / 170, clamped to 0.8 .. 2.5                   (bspUiScale(), 170 = BSP_PPI_REFERENCE)
```

`DisplayConfig.DIAGONAL_IN` is in tenths of an inch (`35` = 3.5"), so the `const` struct holds no
float. `lv_display_set_dpi()` receives the real PPI. 170 is the centre of the fleet's low-density
cluster, so a token written as 16 px is 16 physical px on a 170 PPI panel and the same physical size
on every board.

| Board | Resolution | Diagonal | PPI | Scale |
|---|---|---|---|---|
| `CYD_S3_3248` | 320x480 | 3.5" | 165 | 0.97 |
| `CYD_P4_1060`, `WS_P4_7B`, `WS_S3_4B` | 1024x600, 1024x600, 480x480 | 7", 7", 4" | 170 | 1.00 |
| `CYD_S3_8048` | 800x480 | 5" | 187 | 1.10 |
| `CYD_P4_4880` | 480x800 | 4.3" | 217 | 1.28 |
| `WS_S3_5B` | 1024x600 | 5" | 237 | 1.40 |
| `WS_P4_4B` | 720x720 | 4" | 255 | 1.50 |
| `WS_P4_5` | 720x1280 | 5" | 294 | 1.73 |

A scale of 1.0 for every low-density board and 1.5 for every high-density one (the arrangement
before the scale was derived) renders identical token values 24% physically smaller on the P4_5 than
on the 5B, which is why the scale follows density.

**The scale follows density only.** A per-board viewing-distance factor (text on a 3.5" panel at
arm's length and a 7" panel across the room want different apparent sizes) is not built.

**Minimum touch target**: `UI::minTouch()` is 9 mm in real pixels, 60 px at 170 PPI and 104 px at
294 PPI.

---

## 3. Structure

Flat structs with `const` instances and designated initializers, the idiom of `Fleet_BSP`. Six
groups, reached through one namespace:

- **`UIPalette`** - ground, surface, text, accent, the **semantic state palette** (active / idle /
  ok / warn / bad), the sensor quantity tints and the status glyphs' `SIG_*` colors. State colors
  are separate from `ACCENT` on purpose: accent is decoration, state carries meaning (`cards.md` §0).
- **`UIMetrics`** - radius, padding, border width and opacity, shadow, card header height, and the
  per-scheme looks: the active frames (`ACTIVE_RING`, `ACTIVE_EDGE`, `BRACKET_W`, `GLOW_DEPTH`,
  `GLOW_OPA`, `CUT_PCT`, `ACTIVE_INK`), the halo (`HALO_PCT`) and the shadow (`SHADOW`, `SHADOW_Y`,
  `SHADOW_OPA`). Per scheme.
- **`UIShape`** - the card window's sizes in millimetres, fleet-wide (§7).
- **`UIHeader`** - the page header's slots, in logical pixels (§8).
- **`UIGrid`** - target card width, aspect cap, gap, inset. **Columns are always derived from target
  width, never declared** (§5).
- **`UIType`** - the font shortlist and icon sizes. Fleet-wide.

```cpp
namespace UI {
    const UIPalette &pal();   const UIMetrics &met();   const UIShape &shape();
    const UIHeader  &hdr();   const UIGrid    &grid();  const UIType  &type();
    void setScheme(...);                  // runtime, repaints
    int32_t sc(int32_t v);                // logical px -> real px
    lv_color_t c(uint32_t hex);
    lv_style_selector_t part(lv_part_t p, lv_state_t s);
}
```

Runtime scheme switching is in: cards read `UI::pal()` when they build or restyle and never cache a
color. `UI::shadowReachPx()` is how far a card's shadow reaches past it, used to let shadows out of
their parents (`UI::unclipShadows()`, and a bare sheet).

### Font cost

A referenced font face costs 6-73 KB of flash by kind and size (CLAUDE.md, "Two costs worth
knowing"). Enabling a size in `lv_conf.h` costs nothing, because the linker discards unreferenced
faces; **referencing** another distinct size is what costs. The type shortlist in `UIType` is a
flash budget. `HERO` shares `VALUE`'s face until a fullscreen card needs a larger one. An MDI icon
subset is a font too and is budgeted with the type scale.

---

## 4. The schemes

Three schemes, one for every page (`system.ui.scheme`, saved; D-29). **Midnight is the default.**

| | Fleet | Midnight | Linen |
|---|---|---|---|
| Ground | `101010` | `1A1F27` | `CFC4B2` |
| Surface | `181818` | `212429` | `F3EEE4` |
| Accent | `00A8FF` cyan | `00A8FF` cyan | `A8561C` copper |
| Active ("on") | `F0A63C` orange | `F0A63C` orange | `D47F45` |
| Border | explicit `404040` | derived | derived |
| Metrics | dark | dark | light |

The state colors (idle, ok, warn, bad) and the sensor tints are the same on every scheme: a warning
is amber on a dark or a light scheme. Only "on" differs, because the orange of an active card reads
wrong on Linen's sand; it still means the same thing (a filled card with a lit disc), so state never
rests on color alone. `/popup?active=RRGGBB` tries another "on" without a build.

| | Dark metrics (Fleet, Midnight) | Light metrics (Linen) |
|---|---|---|
| Radius | 10 | 12 |
| Padding | 5 | 5 |
| Border | 1 px lightened 40% | 2 px, darker |
| Shadow | none | mild drop (blur 6, 3 down, opacity 140) |
| Active frames | glow, `GLOW_OPA` 115, `ACTIVE_INK` 0 | `GLOW_OPA` 150, `ACTIVE_INK` 35: the frame's edge goes 35% toward the text so it stands out, the light half that |

Dark schemes take a border because a dark card on a dark ground needs an edge; the light scheme
leans on its shadow. `pad 5` is small on purpose: the grid gap carries the separation, so text
clipping at small sizes is the thing to watch on the smallest board.

---

## 5. The grid

`TARGET_CARD_W` decides how many **columns** fit and is per board (`UITokens.cpp`): **130 on
`WS_P4_5`, 135 elsewhere**. A larger target means fewer columns, and one number cannot serve both
the P4_5 (740 logical px wide) and the 7B (1024): the P4_5 needs 132 or less for five columns.
Other defaults: `GAP` 12, `INSET` 14, `ASPECT_PCT` 130 (a ceiling on a card's height as a percentage
of its width, not a target shape). Rows come from the cards (`dashboard.md` §6); `UIGrid::rows` is
an estimate for callers with no cards.

```
[UI] Scheme "Fleet" | 5x3 grid of 230x210 px | scale 1.73x (294 PPI)   WS_P4_5
[UI] Scheme "Fleet" | 2x3 grid of 141x144 px | scale 0.97x (165 PPI)   CYD_S3_3248
```

**A card costs about 715 bytes of `lv_mem`**, the same on every scheme (they build identical widget
trees and differ only in style values).

**Font range is ASCII plus a small symbol set.** `·` (U+00B7) and `—` (U+2014) render as empty
boxes. The degree sign is in range. Anything drawn on the panels stays ASCII, except `°`, unless
the font range is deliberately extended, which costs flash at the rate in §3.

**Scrolling.** `UI::tameScroll()` makes a container vertical-only, with no elastic springback and no
scrollbar, and clickable so a swipe from a heading or gap scrolls it. Call it on every scrollable
container.

---

## 7. Shape: the card window's millimetres

The card window sizes what a finger touches and what an eye reads in real millimetres on glass, so
a target or a line is the same physical size on every board (`mm()`, the derivation `UI::minTouch()`
uses). They are **`UIShape`**, read through **`UI::shape()`**: declared in `include/UI/UITokens.h`
§2b with a comment on each saying what it draws, and valued in `src/UI/UITokens.cpp`.

**Using them.** `mm(UI::shape().S5)`, or `pm(...)` for the `P_` group, which is in "P4_5
millimetres" - the same share of every window as on the P4_5 (`card-sheet.md` §15). Never through
`UI::sc()`: these are not logical pixels. No number goes into a popup file: a new size is a new
token with its comment and a row here. An effect's own dice may stay named constants beside the
effect (the sparks' `SPARK_*_MM` in `src/UI/Kit/KitSparks.cpp`), since nothing else draws one.

**Its own struct, not `UIMetrics`.** `UIMetrics` is per scheme and in logical pixels; these are
fleet-wide and physical. Putting them in `UIMetrics` would give every scheme a copy to keep in
step, and changing that struct's layout is the build-cache trap (CLAUDE.md). One `const` instance,
in flash, no RAM. If a scheme ever wants its own corners, `UI::shape()` can return a per-scheme copy
and no caller changes.

| Group | Tokens, in millimetres |
|---|---|
| Radius | `R_CONTROL` 1.6, `R_PANEL` 1.6, `R_WINDOW` 2.4, `R_CHIP` 3.0 |
| Spacing | `S1` 0.6, `S2` 0.8, `S3` 1.0, `S4` 1.2, `S5` 1.6, `S6` 2.0, `S7` 2.4, `S8` 3.0, `S9` 4.0, `S10` 5.0 |
| Lines | `LINE_HAIR` 0.2, `LINE_MID` 0.3, `LINE_HEAVY` 0.4, `LINE_PLOT` 0.45, `DASH` 0.6, `RING_W` 0.4, `RING_PAD` 0.3; letter spacing `TRACK_PILL` 0.2, `TRACK_TAB` 0.4 |
| Heights, widths | `H_CONTROL` 6.0, `H_CHIP` 6.0, `H_ROW` 7.2, `H_BAR` 0.17 (the countdown bar: 2 px at 294 and 255 PPI, 1 px at 170), `H_TABS` 6.0, `W_SIDE_MIN` 6.0, `W_WINDOW_MAX` 100, `W_ROWS_MAX` 70, `W_DIALOG_MAX` 90, `W_NARROW` 100 (a narrow window: Edit cards' column and the group view's drop-down take a larger share), `W_TWO_COL` 80 (two columns of rows in Edit cards and the HA picker), `W_DIALOG_BTN` 24, `SWATCH` 6.5, `CARET_W` 2.4, `CARET_H` 1.0 |
| Finger travel | `SLOP_SELECTOR` 1.0, `SLOP_KNOB` 1.2, `SLOP_HOLD` 4.0 (in an open sheet a finger that moves more than this is never a tap, K137) |
| The hero (`pm()`) | `P_GAP` 4.0, `P_STACK_GAP` 3.0, `P_GRIP_INSET` 1.4, `P_GRIP_H` 0.5, `P_GRIP_MIN` 5.0, `P_MARK_H` 2.6, `P_MARK_LINE` 0.35, `P_SCENE_H` 9.0, `P_SCENE_GAP` 1.4, `P_SCENE_PAD` 2.4 |
| A group's cluster (K89-K103) | `C_HDR_H` 5.2 its header row, `C_PAD` 1.1 inside its frame, `C_TILE_GAP` 0.8 between mini tiles, `C_SWATCH_W` 1.1 / `C_SWATCH_H` 2.4 the group's swatch, `C_VALUE_GAP` 1.3 between header values, `C_TILE_MIN` 7.5 the least a tile is each way ("tight", K103), `C_TILE_FOOT` 1.5 a tile's hero and label kept off its top and foot, clear of its active frame, `C_TILE_FOOT_AT` 18.0 the height from which a tile keeps all of `C_TILE_FOOT` (a shorter one keeps less, K126), `C_TILE_TALL` 1.1 (a ratio) a tile drawn no taller than that times its width, centred in its slot; a group whose own is taller than the page's shortest draws halfway down to it (K124). In the group view (K159): `C_TILE_MAX_W` 25.0 / `C_TILE_MAX_H` 20.0 the biggest a tile is, and `C_TILE_WIDE` 2.0 (a ratio) no wider than that times its height |
| Floating (D-35) | `HALO` 5.0 the halo round a header slot's panel and every window, `LINE_FADE` 2.5 the bar's line fading out before an open panel's walls |
| The keyboard (K95) | `KEY_H` 7.0 a key's height (a phone's, under the touch minimum), `KEY_GAP` 1.0 between keys and round them |

The spacing values fall into ten steps from 0.6 to 5.0 mm. Several tokens share a value
(`R_CONTROL` and `R_PANEL`; `H_CONTROL`, `H_CHIP`, `H_TABS` and `W_SIDE_MIN`): they are different
roles, and stay separate so one can change alone. The near-equal pairs were merged into the ten
spacing steps and one control radius (owner, K72).

## 8. The page header's slots, in logical pixels

The header is `sc(SYSTEM_HEADER_FULL_H)` - 35 logical px - tall on every board, so what sits in it
is sized in the same logical pixels: **`UIHeader`**, read through **`UI::hdr()`**
(`include/UI/UITokens.h` §2c), passed to `UI::sc()`. One `const` set, fleet-wide.

| Token | Value | What uses it |
|---|---|---|
| `GLYPH` | 26 | a status glyph's drawing box - the same bare or framed: a frame never shrinks it (D-24) |
| `FRAME` | 28 | a glyph's frame: the rounded square, the circle - round the glyph, not inside its box |
| `FRAME_R` | 7 | the rounded square's corners |
| `FRAME_MIX` | 14 | the rounded square's shade: % of `TEXT` over the bar |
| `SLOT_GAP` | 8 | between the things in one slot |
| `SLOT_PAD` | 6 | a slot's sides - how far its tap area and its open tab reach past what it shows |
| `EDGE` | 8 | from each end of the bar to its side's first slot |
| `SPACE` | 2 | between two slots in one place; their pads make the rest |
| `CLEAR` | 12 | at least this between a side and the middle - closer, the lowest keep gives way (`Layout/HeaderPlan.h`) |
| `DOT_ON`, `DOT_OFF`, `DOT_RING`, `DOT_GAP` | 11, 8, 2, 5 | the page's dots: this page's, another's ring and its line, between them |
| `MARK` | 12 | the error mark beside a slot's dashes |
| `LINE` | 2 | the bar's accent line - and the outline of an open slot and its panel, in the accent (D-23) |
| `JOIN_R` | 6 | a cove beside an open tab, in the bar: the rounded corner where the tab's side meets the bar's line. The panel's side walls rise straight, with no flares (D-30) |
| `DRAWER_W` | 260 | a slot's panel at most |
| `DRAWER_R`, `DRAWER_PAD`, `DRAWER_GAP`, `DRAWER_LINE` | 15, 10, 8, 2 | a slot's panel: the radius of every corner (it lies on the bar, its top corners rounded toward its middle, D-35), inside, between rows, its edge |

Logical rather than millimetres because the bar they sit in is logical; the two agree to within the
scale's clamps (1 logical px is about 0.15 mm wherever `sc()` is not clamped).

**The status glyphs' colors** are palette tokens, `UIPalette`'s `SIG_*`, one set per scheme family -
a glyph's color is its link's state, so it lives with the scheme:

| Token | Dark schemes (Fleet, Midnight) | Linen | What it means |
|---|---|---|---|
| `SIG_STRONG` | `34D399` | `1B7A3A` | Wi-Fi strong |
| `SIG_GOOD` | `4ADE80` | `2F8F4E` | Wi-Fi good; a link that is up |
| `SIG_WEAK` | `FBBF24` | `9A6412` | weak, or a link that is quiet |
| `SIG_BAD` | `F87171` | `B4321F` | down |
| `SIG_BUSY` | `60A5FA` | `1D5FD6` | joining, connecting |
| `SIG_OFF` | `64748B` | `8A8076` | switched off |
| `SIG_LINK` | `A78BFA` | `6B4FC8` | the MQTT link's arrows |

Linen's are darker so they read on its light bar. The glyphs themselves are numbers, not tokens:
`UI/GlyphShapes.cpp` holds each stroke on a 24-unit grid.

**The header's words have their own type role, `HEADER`**: TAG's millimetre target and TAG's scale
on every board, except where `gen_type_scale.py`'s per-board table gives a larger one: `WS_P4_5`
1.18 (26 px, 2.25 mm, matching the 4B's 22 px within 2%), `WS_P4_7B` and `CYD_P4_1060` 1.26 (D-34).
`UIToolkit::Font_Header` is the larger of `HEADER` and `Font_PanelHeader`.

**The active frames** are `UIMetrics` tokens in logical px, drawn in `ST_ACTIVE`
(`Cards/CardFrame.cpp`): `BRACKET_W` 6 (the bracket's top and bottom bars), `ACTIVE_EDGE` 2 (its
sides), `ACTIVE_RING` 3 (the corner cut's frame and the edge of each cut), `GLOW_DEPTH` 18 and
`GLOW_OPA` (how far the light reaches in from a lit edge, and how strong it is there), `CUT_PCT` 24
(the corner cut's chamfer, % of the card's height), and `ACTIVE_INK` (how far the frame's edge goes
from the active color toward the scheme's text). The corner cut's shadow is the card's own
(`SHADOW`, `SHADOW_Y`, `SHADOW_OPA`), measured from LVGL and painted around the cut.
`UI::darkest()` (the scheme's darkest, 60% of the way to black) inks lit glass, warmed.
