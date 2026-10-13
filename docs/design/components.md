# The component kit (2.12 P3)

**Status: built and verified 2026-10-13** - the card window is built from it, and the harness found
no tree or pixel difference against 2.12 P2 (`ui-foundation.md` §6). Companion to `tokens.md` (the
values the kit reads) and `card-sheet.md` (what the window is).

The kit is `include/UI/Kit.h` and `src/UI/Kit/`. It is the layer between the tokens and a screen:
a screen asks for a dialog, a row of chips or a panel of rows, and the kit builds it in the house
style. The card window is its first client; the Sheet (P4) and #92's chart options are the next.

---

## 1. How it works

**A component takes a role; a role is a few shared styles.** Every role's look is one `lv_style_t`
per part and state it styles (`Kit::KitStyle`), filled from the tokens by `KitStyles.cpp`. The
styles are filled once at `GUIManager::begin()` (`Kit::begin()`) and again on every repaint - a
scheme or accent change - through `UI::onRepaint()`, after which LVGL restyles every object using
them. That is `UI::paint()`'s mechanism (#64), so a component follows the scheme without a restyle
of its own.

**The caller places it and owns what changes.** A component never sizes or positions itself
beyond its own insides; the caller does (`lv_obj_set_size`, flex, align). Anything that changes
while it shows - a value, a ring round the chosen one, a greyed state that comes and goes - stays
a LOCAL property set by the caller, which beats a shared style in LVGL. A paused window's quiet
colours are set that way too, over the role's.

**The same objects, the same events.** Each component builds exactly the objects its code built
before it was a component, in the same order, with the same flags and the same event codes and
user data - which is what let the harness prove the move changed nothing. Where a component needs
its own state (the selector), it keeps it in the caller's struct and finds it through the
object's user data, so the events carry what they always carried.

**What it costs.** An object using a shared style holds one 8-byte entry in its style list
instead of a copy of every property: a window costs ~180 B less of LVGL's pool, and an open panel
or History view 1-2.5 KB less (§4). The styles themselves take ~880 B of the pool once, at boot.

**Where styles may be set.** In the kit, and in the builders of what no component draws yet -
the window's hero (toggle, slider, disc), the chart, the sparks, the window frame (the Sheet,
P4). `scripts/structure_check.py` (P6) reports any other `lv_obj_set_style_*` call.

---

## 2. The components

### Basics - `plain()`, `label()`, label roles, `textW()`

`Kit::plain(parent)` is the bare object everything starts from: no theme styles, not scrollable,
not clickable. `Kit::label(parent, font, colour)` is a label with its own (local) look, for a
colour worked out at the call; `Kit::label(parent, role)` takes a shared look instead.

| Label role | Face | Colour | Used for |
|---|---|---|---|
| `KS_LABEL_NAME` | `NAME` | `TEXT` | a title, a row's words, a state |
| `KS_LABEL_NAME_DIM` | `NAME` | `TEXT_DIM` | a greyed row's words, a note, a day heading |
| `KS_LABEL_TAG` | `TAG` | `TEXT` | a value in a list, a chip, a chart reading |
| `KS_LABEL_TAG_DIM` | `TAG` | `TEXT_DIM` | "Changed 3m ago", a property's name, an axis |
| `KS_LABEL_TAG_ACCENT` | `TAG` | `ACCENT` | a lit chip, "Show more" |

### Button - `Kit::button(parent, role, text, w, h, cb)`

![The Remove dialog's two buttons](components/dialog.png)

A plain object with words, its look by role: **CANCEL** (the surface, a faint edge) and
**DESTRUCTIVE** (`ST_BAD`, the words in the surface colour). Radius `R_BUTTON`, edge `LINE_FINE`,
pressed: 20% of `TEXT` mixed in. The words are the role's ink (`NAME`).

The brief listed PRIMARY and QUIET too. No button in the house is either today - the row button is
a row control (below), a scene is a pill - so they wait for their first user rather than ship
untested.

### Dialog - `Kit::dialog(spec)`

A question over everything, on the top layer, the rest dimmed (`GROUND` at 60%): a title, up to
two centred paragraphs, and up to two buttons side by side; a tap outside the box calls the spec's
`outside`. Returns the dim; deleting it closes the dialog. One role so far, **DESTRUCTIVE** (the
box's edge in `ST_BAD`, `LINE_HEAVY`). The box: the scheme's card radius, `SURFACE_ALT`, padding
`S9`, rows `S7`. "Remove card" (K66, K69) is its first client.

### Chip row - `Kit::chipRow(parent, words, n, lit, room, cb)`

![History's span chips](components/chips.png)

Small round-ended buttons side by side, the chosen one lit: `SURFACE` unlit; lit, 25% of the
accent mixed in with a `LINE_FINE` accent edge and the words in the accent. Height `H_CHIP`, radius
`R_CHIP`, sides `S_CHIP_SIDE`, gaps `S4` - both shrinking to fit `room`, as far as half. Each
chip's tap carries its index. History's spans are its client; #92's chart options can be.

### List item - `Kit::item(parent)`

![Members' rows](components/items.png)

A tappable surface in a list - a member, an activity row, "Show more": the card's radius and
`SURFACE`, 15% of `TEXT` mixed in while pressed. The caller sizes it, lays out what it holds and
says what a tap does.

### Rows - `Kit::rows(body, specs, n)` and its parts

![SETTINGS' rows](components/rows.png)

A panel's rows from one list of `Kit::RowSpec` - words, kind, choices, the chosen one, live or
greyed, a callback, where to keep the control - so a panel's size (`Kit::rowsNeedW()`) and its
contents cannot disagree. A row is `H_ROW` tall, its words on the left (`KS_LABEL_NAME`, dim when
greyed), its control on the right, `H_CONTROL` tall:

- **Dropdown** (`ROW_DROP`): `SURFACE`, radius `R_CONTROL`, a `LINE_HAIR` accent edge (greyed: 25%
  of `TEXT` in `SURFACE_ALT`, and disabled); its list the same, every choice in sight (the screen
  limits it), the chosen line in the accent.
- **Checkbox** (`ROW_CHECK`): a box as tall as a dropdown, a `LINE_MID` accent edge, the accent
  when ticked.
- **Row button** (`ROW_BUTTON`): a dropdown's frame with words; greyed, no accent and nothing to tap.

### Folder panel - `Kit::folder(F, spec)`

![The SETTINGS panel, open](components/panel.png)

A pane of rows with a tab rising from its top edge, the join curving in like a file folder's
(owner, rounds 7-11), built below the bottom of the screen in a track it can be dragged in. The
spec says the window it belongs to, which side its tab is on, its title, its widest row, its row
count, the tab's width and the callbacks; `Folder` returns its objects, its height, the strip its
tab shows folded (`head`) and how much stays below the screen open (`hide`). Sliding it, dragging
it and what its carets mean are the caller's (`PopupPanels.cpp`). The face: `SURFACE_ALT`, the
scheme's border, radius `R_PANEL`, the scheme's lift.

### Selector - `Kit::selector(S, parent, spec)`

![The control deck](components/selector.png)

Choices on a ribbon and a metal selector that slides between them (owner, 2026-10-07, after HTC's
TouchFLO 3D): dragged, or glided to a tapped choice, snapping to the nearest; its icon a size up in
the accent. An ACTION (the window's Power) sits on a ribbon of its own before the choices, and the
selector never lands on it. The spec gives the choices' icons, the chosen one, the action, whether
it is quiet (a paused window), what to ask before anything moves (`blocked`) and what to call when
the selector arrives (`onChosen`). `Kit::Selector` is its state, kept by the caller while it shows.

Its **four looks** (Black or Silver, Square or Round) are per scheme and saved (K36, K45):
`Kit::selectorLook()`, `Kit::keepSelectorLook()`. Its face, and a lit icon button's, is a **face
drawn by hand** - a raised metal face or a soft dent, painted into PSRAM once (`KitFaces.cpp`).

### Icon button - `Kit::iconButton(parent, glyph, font, cb)`

![The window's corner icons](components/icons.png)

A round button at the minimum touch size with an icon: a soft dent in the surface, tinted while
pressed. `Kit::iconButtonLit(b, lit)` lights it as the selector is - the same metal, the icon a
size up in the accent: the window's corner icons work as tabs that way.

### Pill, swatch and badge (styles)

![Scene pills](components/scenes.png) ![Colour swatches](components/swatches.png)

`KS_PILL` / `KS_PILL_PRESSED`: a round button with words (a scene); room for the ring round the
loaded one. `KS_SWATCH`: a round colour swatch with a hairline edge (the colour is the caller's).
`KS_BADGE_PAUSED`: the card's own PAUSED badge.

The pictures are WS_P4_5 in Midnight, cut from the harness's screenshots
(`scripts/ui_golden.py crop`).

---

## 3. Not in the kit

What only the card window draws stays in `src/Cards/Popup/`: the hero (the toggle and its knob,
the light's slider, a sensor's disc), the history chart and the activity list's layout, the
sparks. The window's frame - its shape, header row, views and panels together - becomes the
**Sheet** in P4.

## 4. Measured

LVGL's pool per window opened, WS_P4_5, median of 7 (`scripts/ui_golden.py measure`):

| Window | 2.12 P2 | With the kit |
|---|---|---|
| Desk (HA light group) | 10,176 B | 10,004 B |
| Deck (temperature) | 7,188 B | 6,992 B |
| North (garage door) | 7,200 B | 7,004 B |
| Overhead (switch) | 7,460 B | 7,280 B |
| All Lamps (group here) | 10,928 B | 10,756 B |
| Lamp 1 (virtual light) | 8,800 B | 8,628 B |

And the whole pool in use, P2 against the kit, from the same house state (the harness's A/B):

| State | P2 | With the kit | |
|---|---|---|---|
| The dashboard, no window | 86,088 B | 86,964 B | +876: the kit's styles, once |
| Desk, SETTINGS open | 105,488 B | 105,212 B | -276 |
| Desk, History | 117,672 B | 116,504 B | -1,168 |
| Deck, its CHART panel open | 115,872 B | 114,256 B | -1,616 |
| North, its ACTIVITY panel open | 113,956 B | 113,076 B | -880 |

The window's build time did not move: the fastest of 7 opens within 0.3 ms of `v0.2.12` on every
window.
