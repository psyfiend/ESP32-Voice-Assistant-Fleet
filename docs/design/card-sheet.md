# The card sheet — long press, milestone 2.10 (#65) — PROPOSED

**Status: design proposal, 2026-09-27, for the owner to react to. Nothing built.** Sources:
ROADMAP 2.10 (#65) and §4.3; `docs/design/context-panels.md` (the A/B split this builds on);
`cards.md` §4 and §13; the owner's ideas of 2026-09-27 (colour lights, an HA-style "more info" with
details and history, deck panels that fit what is on screen). Decisions for the owner are marked
**CHOICE** and collected in §9, each with a recommendation.

---

## 1. The shape, in one paragraph

A long press on any card opens **one sheet** - a single reusable object, rebuilt for whichever card
opened it - that rises from the bottom edge over a dimmed page, HA's "more info" dialog in our own
clothes. Its content is chosen by the entity's **kind**, the same way a card's type is: a light gets
brightness and colour, a sensor gets its history, everything gets its details and the card's own
settings. Tap stays a one-touch toggle, as the owner decided (ROADMAP 2.10). Tap outside or swipe
down to close. This is `context-panels.md`'s mechanism **A**: the binding comes from the card that
was pressed, so there is no "which card does it pull from" problem.

The owner's **deck-as-context-panel** idea is that document's **B**, and it is a separate, later
piece (§7) - but the sheet's building blocks are made so B reuses them.

## 2. What is already there

- Cards already tell a tap from a long press: `Card.cpp` listens for `LV_EVENT_SHORT_CLICKED` and
  `LV_EVENT_LONG_PRESSED`. Today a long press toggles **pause**; per ROADMAP 2.10, pause moves into
  the sheet.
- Lights already carry `brightness` (0-255) and `rgb_color` from HA (`EntityAttrs`, `HaValue.h`).
- LVGL has what the controls need, all enabled: `lv_slider`, `lv_arc`, `lv_chart`, `lv_tabview`.
- `test_lamp_1..4` are local (virtual) entities that work with no HA - **the sheet can be built and
  tested on the two laptop boards this weekend**, except for what has to talk to HA (§6).

## 3. Layout — **CHOICE A**

| Option | Looks like | For | Against |
|---|---|---|---|
| **A1 Bottom sheet** (recommended) | rises from the bottom edge, ~60% of the height; full width on the CYD's portrait screen, ~70% width centred on landscape | thumb-reachable; the same motion as the deck, so it reads as "the context area"; swipe down to close is natural | covers the bottom rows of cards |
| A2 Centred dialog | HA's own more-info look | familiar | a far reach on the 7"; a second visual language beside the deck |
| A3 Into the deck | the deck panel itself expands with the card's controls | unifies A and B visually | the deck is hidden by default and is two panels wide; a sheet needs one surface |

**Rules that come from LESSONS, whichever is chosen:**
- **The dim is a fill, not a fade.** A full-screen scrim with `bg_opa` draws a translucent fill; an
  object `opa` below COVER forces a full-screen LAYER - the allocation that froze `WS_P4_5`. The
  sheet slides (`translate_y`), it never fades.
- **Release the touch before rebuilding** (`lv_indev_wait_release`), or the finger that long-pressed
  lands on whatever the sheet puts under it.
- **The scrim switches by `CLICKABLE`, never `HIDDEN`** (the deck scrim's lesson: unhiding a
  screen-sized object redraws the whole screen in the first animation frame).
- On `lv_layer_top()`, which no longer scrolls (#68).

## 4. What is inside — tabs chosen by kind, built only when opened

HA's dialog has controls, history and settings. The same three, shown only where they apply, and
**built lazily** - a tab's widgets exist only while it is showing, because `lv_mem` is 128 KB and
`WS_P4_5` has the least of it free (~36 KB):

| Tab | light | switch / input_boolean | sensor (numeric) | binary_sensor | virtual / system |
|---|---|---|---|---|---|
| **Controls** | yes (§5) | a large on/off | - | - | if writable |
| **History** | on/off timeline | on/off timeline | line chart | on/off timeline | - |
| **Details** | yes | yes | yes | yes | yes |

The sheet opens on Controls where there is one, otherwise History, otherwise Details.

**Details** is the same for every kind: name, state, *last changed* ("4 min ago"), entity id, where
it comes from (HA, MQTT, virtual), stale/available state - and the **card's settings**: **Pause**
(moved here from long press), and later the per-card overrides the build sheet will allow.

## 5. Light controls — **CHOICE B** for colour

- **Brightness**: one large slider, the full width of the sheet, 0-100%. The card underneath
  follows live (the registry already does optimistic writes), so the owner sees the effect on the
  card and the lamp together.
- **Colour temperature**: a slider warm-to-cool, only if the light supports it, its range from the
  light's own `min/max_color_temp_kelvin`.
- **Colour** - LVGL 9 dropped its colour wheel, so it is ours to choose:

| Option | | |
|---|---|---|
| **B1 Hue strip + swatches** (recommended) | a rainbow slider for hue, and a row of 6-8 preset colours (HA's own "favourite colours" idea) | cheap, every size of screen, a finger can hit it |
| B2 A real colour wheel | a drawn hue/saturation disc | beautiful; a canvas image per size, and fiddly on the 3.5" |
| B3 Swatches only | presets, no free choice | simplest; too limiting on its own |

- **Sending**: while a slider is being dragged, at most one update every ~250 ms, plus one on
  release - never one per touch event. Colour-blind rule (HANDOFF): the current value is marked by
  position and a label, never by colour alone.

## 6. What needs new plumbing

| Piece | Where | Testable this weekend? |
|---|---|---|
| The sheet, scrim, gestures, tabs, Details, Pause | `src/UI/CardSheet.{h,cpp}` (new) | **yes** |
| Light controls driving `test_lamp_*` (virtual) | same | **yes** |
| `EntityAttrs` gains `supported_color_modes`, `min/max_color_temp_kelvin`, `color_temp_kelvin`, `last_changed` | `Entity.h`, `HaValue.h` | parse only; real values at home |
| Outbound `light.turn_on` with data (`brightness_pct`, `color_temp_kelvin`, `hs_color`) on the websocket - #44's HA leg | `Fleet_HA` | at home (HA unreachable here) |
| History: HA's `history/history_during_period` over the websocket on opening the tab (24 h, `minimal_response`, `no_attributes`), kept in PSRAM, drawn with `lv_chart` | `Fleet_HA` + `CardSheet` | at home |

All HA traffic stays on the loop task, and replies arrive on the websocket task, which must not
touch LVGL (HANDOFF) - the history reply is parked and the sheet picks it up on the next `tick()`.

## 7. The deck as a context panel (the owner's idea) — later, but shaped now

The owner: deck panels that fit what is on the screen. That is mechanism **B** - not inferred from
the cards on show (the objection in `context-panels.md` stands: on a page of mixed cards, nothing
can guess), but **declared by the page**: `PageSpec` gains its own deck - House might carry
"Lights" (all off, scenes) and "Climate"; Fleet keeps System and Display. Swipe to a page, its deck
comes with it.

So 2.10 builds A, and builds its interior from **reusable rows** - a slider row, a toggle row, a
swatch row, a value row, a chart row - which is exactly the vocabulary a declared deck panel would be
assembled from later (and which the build sheet, 3.1, will describe). One set of parts, two
mechanisms, no second grammar - the trap `context-panels.md` warned about.

## 8. The plan, in steps each with its own test sheet

1. **Frame**: long press opens the sheet over a dimmed page, swipe down / tap outside closes;
   Details tab; Pause moved into it. Measured with `/bench` like the deck swap (a sheet slide is the
   same kind of work) and checked with `/bench?what=verify`.
2. **Light controls** on the virtual lamps: brightness, temperature, hue + swatches, card follows live.
3. **HA** (at home): attributes, outbound `light.turn_on`, on a real bulb.
4. **History** (at home): the timeline and the chart.

Steps 1 and 2 fit on the two laptop boards.

## 9. Choices for the owner

| | Choice | Recommendation |
|---|---|---|
| **A** | Where the sheet appears | **A1 bottom sheet** |
| **B** | How colour is chosen | **B1 hue strip + swatches**; a wheel later if wanted |
| **C** | Pause moves off long press into the sheet now | **yes** (ROADMAP already says so) |
| **D** | History's default span | **24 h**, with 1 h / 7 d buttons if cheap |
| **E** | Close gestures | tap outside + swipe down; **no** close button needed on touch |
| **F** | Build step 1 on the 4B first (the veto board of these two is the P4_5 - least `lv_mem`) | **P4_5 first**, per LESSONS' "flash the veto board first" |
