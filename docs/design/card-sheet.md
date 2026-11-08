# The card window - a long press on any card

**What the card window is, how it behaves and how its settings are kept.** Built. Decisions in
`docs/DECISIONS.md`: K3-K29, K31-K43, K49-K50, K52-K64, K70, K79. The code is `src/Cards/CardPopup.cpp`
and `src/Cards/Popup/` (one file per concern, sharing `PopupState.h`); the frame is the Sheet
(`components.md` §3). What groups add to it: `group-cards.md` and `ha-cards.md`.

Section numbers are cited from `DECISIONS.md` and from code comments, so they are kept: §1-§13 are
the rules of the window, §14-§18 its parts in detail.

### Words for the window's parts

| Word | Means |
|---|---|
| **window** | the popup a long press opens |
| **title** | the text at the top centre: what the window or view is ("Desk"), with the view it came from in front ("Desk > Members") - the **breadcrumbs**; its first part is a link up |
| **chips** | the round navigation buttons in the corners: X, back, chart, members, clapperboard |
| **hero** | the view's main content: on the controls, the slider (or toggle) with its label, value and swatches; on Members, the list |
| **control deck** | the bar of **switches** under or beside the hero - Power \| Brightness, Temperature, Color. It says which modes the light supports and which one the slider is set to |
| **switch** | one of the control deck's options |
| **selector** | how the chosen switch is shown - a metal face over the chosen switch, its icon in the accent |
| **SETTINGS panel** | the folder-tab panel that rises from the bottom of the screen, in the right half |
| **left panel** | its neighbour in the left half, per view: CHART, ACTIVITY, SCENES, GROUP |
| **card label**, **entity label** | the colored name on a card, and the words under its hero (K42) |

The code's `s.hero` is the slider or toggle alone; `s.deck` and `deckFill()` are the SETTINGS panel,
`deckChip()` its choice buttons.

---

## 1. Presentation

- A **centered, bordered, rounded** window over the page, on LVGL's top layer above the header and
  the system drawer; the page under it keeps updating.
- **It appears complete at once and closes at once** (K4): no grow, no dim, no leap. A fade or a
  grow redraws the whole screen each frame (a dim as a translucent object forces a full-screen
  layer out of `lv_mem`, which froze the P4_5). The held card is pressed in with an accent border
  while its window is open. Windows carry a painted halo instead of a dim (`components.md`, halo).
- **Size** (K11): up to 2:1 and never wider than 100 mm (`W_WINDOW_MAX`), with a side gap of at least
  6 mm or 8% of the screen (P4_5 93 x 47 mm, 7" 100 x 75 mm, 4B ~60 mm). Hiding the system header
  makes the window taller, never wider. The hero (the toggle or slider, 30 mm on the P4_5) is the
  same share of the window on every board (`pm()`, "P4_5 millimeters"); touch targets and text are
  real millimeters. Portrait boards are not yet fitted.
- **Nothing moves within one window** (K11). The column beside the slider or toggle is as wide as
  that light's selector (for a group, everything its members can do, paused or not) or a fixed
  allowance for its words, whichever is wider, and as tall as the hero: the label line at the top
  (the PAUSED pill beside it), the selector level with the hero's bottom. The row is centered.

## 2. The window's anatomy

- **Header row**: X top left on the controls (any other view has the back arrow there); the title at
  the top center; the corner icons at the right (the chart, and Members and Scenes where the card
  has them).
- **The corner icons are tabs**: History, Members and Scenes are views of their own; their icons
  stay on every view they belong to, the one showing is lit as a chosen selector button is, and a
  tap on a lit one goes back to the controls. History is the group's from the group's views, a
  member's from that member's own ("Office lamp > History"). Scenes shows on the light's controls
  and its scenes only, and only for a light with scenes (not while the window works through a
  group's members, as a scene would reach the paused one).
- **Back is hard-linked** (K20, K169): the X is on the controls only; every other view has the back
  arrow, and it always goes to the same place: History, Members, Scenes -> the controls; a member
  -> Members; a member's History -> that member. A window opened straight onto History goes back to
  where it was opened from. The title's first part is a link up ("Desk >" goes to Desk's controls
  from any view below it).
- **The title gives way in steps** (K29): centered when "Area > Name" fits between ends of equal
  width; else it takes the room beside the X, left of center; else the name alone (the back arrow
  still goes up). It is sized before the view is built, because a dotted label laid out at the last
  view's width rewrites its own text.
- **Views**: the controls, History, Members, a member's own, Scenes, Entity properties, Card UI,
  Visibility, and the group views (`ha-cards.md` §3).
- **Closing** (K9): the X, a tap outside, a drag down that starts on the header row (so it never
  fights the tall slider), or the idle timeout (60 s unless set otherwise, K79).
- **Panels** (K5, K25, K28, K50): SETTINGS always takes the right half; the view's own left panel is
  its mirror image, built by the same folder builder, its tab a millimeter short of SETTINGS' tab.
  Each is the window's full width when open and only its tab offset: a folder-tab shape, the pane
  with its own top edge, the tab rising from it with a curved inner corner. Folded, only the tab
  shows at the bottom of the screen; open, the pane's bottom edge stays below it. Opening either
  panel folds the other; a tap in the window folds whichever is open and a tap outside both closes
  everything. A panel is **as wide as its longest row**, its right edge on the window's, never
  narrower than its tab and never taller than the window. **Panels are dragged whole, never scrolled
  inside**: up to show hidden rows (it stays where let go, never past its last row), down to fold
  (past a third of the way folds, less goes back up); carets at the screen's edges point the way. A
  folded tab opens by a tap only. A panel opens and folds in 260 ms; a closing window takes an open
  panel with it at once and slides a peeking tab down. Each panel's holder is only as wide as its tab
  and pane, so a slide does not redraw the cards under it.

## 3. What each kind lands on

| Kind | Lands on | Body |
|---|---|---|
| Light (dimmable / color / temperature) | the controls | the slider, the selector, swatches (§15, §16) |
| Switch, on/off light, a light whose source has not said what it can do | the controls | the big toggle |
| Numeric sensor (temperature, lux, power) | History | a chart (§18); tapping the card opens History |
| Binary sensor (motion, door) | History | its activity (§18) |
| Climate | its own view | a thermostat's facts, read-only (`ha-cards.md` §3.7) |
| Group card (a set of entities) | the controls or Members | `group-cards.md` |

The window opens on the control and carries no toggle in its header (K6): the hero is the control, and
Power is in the selector. A card that toggles nothing taps to History (K57).

## 4. Where the window's data comes from

Light state lives in the entity registry (§15); History is asked of HA when its view opens (§18);
scenes and group members are learnt from HA (§16). Replies arrive on the websocket task, which never
touches LVGL: they are parked and drawn on the next `tick()`. The window holds the entity, never the
card, since a card can be rebuilt under it.

## 5. Per-card settings

A card has its settings in SETTINGS (§11.3, §17), saved by its stable id (K8, K33). Flash writes
happen when the window closes, never per tap (K34).

## 6. Group cards

A group card's window has **Members** (a list of its members as rows: a tap opens that member's own
controls in the same window, `Group > member`, so windows never stack, K7) and its settings under the
GROUP panel. The group model: `group-cards.md`.

## 7. Behavior while the window is open

- **Modal**: page swipes, edge gestures and the header peek are off; a scrim catches taps outside
  (clickable, never hidden).
- **Live**: it follows the entity (registry dirty set) and shows "unavailable" if HA drops it.
- **Long press versus swipe**: a finger that moves cancels the long press, and the touch is released
  before the window is built. In an open sheet a finger that moves more than 4 mm is never a tap.
- The cards under it keep updating.

## 8. Look

- The window surface uses the scheme's raised surface; no `clip_corner` (a layer, LESSONS); inner
  objects stay inside the radius.
- Charts: one line in the accent color, faint grid, large axis text at our scale; ASCII and the
  degree sign only.

## 9. Memory

A window with its panels costs ~11-12 KB of LVGL's pool. The panels' rows are built when first
opened, and only the chosen control is built. LVGL's pool lives in PSRAM (CLAUDE.md), so a window
costs no internal RAM.

## 10. The decisions that shaped it

| | Question | Settled |
|---|---|---|
| D1 | Grow from the card? | No: appears complete at once (K4, W6) |
| D2 | Settings: tab or panel? | Tabs for the entity's own features, panels for settings; SETTINGS always on the right (K5) |
| D3 | Switch landing | open on the control, no header toggle (K6) |
| D4 | Members | behind their own icon; a member's controls open in the same window (K7) |
| D5 | Card settings | saved by stable card id, written on close (K8) |
| D6 | Close | four ways: X, tap outside, drag down on the header row, auto-close (K9) |
| D7 | Color | a hue strip plus eight swatches in the Color mode; LVGL has no color wheel (K10) |

All cards behave as uniformly as possible so navigation does not change by type (K3): long press
always opens a window, with the same header, back arrow and panels.

## 11. The layout, the taps and the settings model

### 11.1 Layout

- **Top left** X (or back); **top middle** "Area > entity" (a member: "Group > entity"); **top right**
  the corner icons.
- **The hero is the control**: a tap anywhere along the slider moves it there. Beside it: the label of
  what it controls, the value, and the selector Power | Brightness, Color, Temperature; modes the
  light does not support are hidden (from HA's `supported_color_modes`).
- **Color** has a hue control and eight swatches; a long press on a swatch saves the current color
  into it, per light.
- **A sensor's CHART panel** holds span, min/max, legend and shading (§18).

### 11.2 Taps

- Tap stays a one-touch toggle where the card has one (K3); a long press opens the window on every
  card and never toggles (K39).
- **Scenes** are a view of their own (§16) and a tap behavior: Tap action offers **Scenes...** and
  SCENES' **Scene action** says whether it loads the default scene or cycles (K70).

### 11.3 The settings model

- **Precedence** (D-7): the more specific wins - device, page, group, card - and every setting
  inherits on its own: a card's Card UI rows start at **Inherit**, which follows UI Tweaks.
- The tag's shape is the **card label type** (float / band / tag / none; UI Tweaks' "Label type",
  K114); its words are the **card label** (Inherit, Custom, None, in Card UI); the words under the
  hero are the **entity label** (Inherit, Name (HA), State, Custom, None).
- A card with no area hides its pill and keeps its space, so cards stay lined up.
- Group colors: a color used by another group is warned about, never forbidden (K46).
- D-7's **Detach** (copy the inherited values onto the card) and **LOCKED** (a parent forbids
  overrides) are not built.

## 12. Not built

Light effects; a brightness chart for lights (the default for a light stays its activity); a fixed
or custom range for charts, with a number wheel (`lv_roller`) for low and high - proposed as CHART's
"Scale: Auto / Fixed", kept per card; the local log for non-HA entities (their History is empty);
a full-screen chart; thermostat controls and units in UI Tweaks (`carried-from-2.13.md` §6);
Backup and Restore (`carried-from-2.13.md` §4).

## 13. The window's rules of build

- **Paused** is a switch in Visibility (K14, K111) and is the entity's: pausing a member's card
  pauses it in every card it is on.
- **The window's panels** (a window's own, not the page deck's) keep their settings in RAM until the
  window closes, then write once (§17).
- **A drag down to close starts on the header row only.**
- **Long press opens the window on every card type** (K3, K39).

---

## 14. The panels and the window's frame

- **Each panel is the window's full width when open; only its TAB is offset** - a folder-tab shape
  with its own top edge. Folded, only the tab shows, at the page deck's header height.
- **While a panel is open, the window is out of reach**: a tap anywhere in the window only folds
  the panel; a tap outside both closes everything.
- **Speed** (W6): the window's panels are quicker than the page deck's (260 ms against 300 ms) and
  that is kept - settings should be quick to reach.
- **Usable first**: the window is built in one quiet pass so its first frame draws only itself
  (`Kit::sheetBegin()`); a window with its panels costs ~11-12 KB of LVGL's pool.

## 15. The light controls

**Light state lives in the registry.** What a light can do (HA's `supported_color_modes` folded into
dim / temperature / color) arrives from the source with every report, so nothing is saved to flash;
levels are null while off, as HA reports them. A brightness, temperature or color command is applied
at once and confirmed only by a matching report, exactly as on/off is (A4); three seconds without
one, it reverts and the card says FAILED. After a reboot, or while the link is down, nothing is
shown as confirmed that the source has not said.

**Commands while dragging go every 300 ms and once on release** (K12). The slider and the value
follow the finger every frame; toggles command on release. A switch, an on/off light and a light
whose source has not said what it can do get the big toggle.

**Groups behave as in HA** (K13): what is offered is everything any member can do; each member is
sent what it can take (a brightness turns an on/off member on; a temperature becomes the nearest hue
for a member with color but no temperature; a color leaves the others on). HA's own rules (from its
`group/light.py`): brightness and color temperature are the mean over the members that are on, hue
a circular mean, the mode the most common, the capabilities the union. **The group card fills to
that mean brightness.** A mixed group shows every control any member has (K185). A lamp comes back
on at its remembered levels. **"On when: any / all"** (Active state, `GroupOn`) is a card setting
for a group made on the board that has a member that can be on; a tap on a group that is on turns
all off, otherwise all on. A native HA group is one entity and HA decides for it.

**Paused** (K14). A paused member is out of its group: not counted for on or off, not offered (a mode
only it has leaves the selector, even when every member is paused), not commanded. The group is
PAUSED only when every member is - a fully paused group keeps its greyed slider and offers only
Power; its status line says "1 paused" otherwise; Members lists the member, reading "Paused". The
card follows the same rule (`Card::counts()`). For a group defined at the source the registry keeps
the group's own pause in step with its members both ways (K32). A paused window has its controls
greyed, a PAUSED pill beside the label, and a touch on a control explains itself.

**Members.** A tap on a member row opens that member's own controls in the same window ("Group >
member"; the back arrow returns to Members). It needs no card of its own.

**Failed commands are recorded**: no matching report in 3 s, with what was asked and what the light
last said, or HA's refusal and its message. The last four are logged and listed by `/popup?view=3`.
A refusal of an earlier call while a newer one is in flight (a drag) does not fail the newer one.

## 16. Home Assistant's lights

Measured: `ha-websocket.md` §9. Decisions: C5, K12, K15, K17-K21.

**Levels.** An HA light reports what it can do, what it is showing (`color_mode`) and its levels; one
reader (`HaValue.h`) serves the boot fetch and the live feed. So an HA light gets exactly the
window a virtual lamp of the same kind gets. Commands are `light.turn_on` with `brightness`
(0-255), `color_temp_kelvin` or `hs_color`, confirmed by the light's own report (C5). HA's reply to
the call is read: a refusal (`success: false`, which means HA sent nothing) shows FAILED at once.
A Hue bulb may correct a color to what it can really make about 1.4 s after confirming, and the
window follows.

**Groups defined in HA or Hue** (K17, K18). `light.office` is one entity to HA. Its `entity_id`
attribute names its bulbs; the registry LEARNS them - registers each as an entity of its own while
the board runs (id `ha_light_office_lamp`, name from its own `friendly_name`, A13), subscribes and
fetches it. So Desk's window has Members, listing the bulbs, and a tap opens one bulb's controls.

- **With no member paused, the window and the card stand for the group itself**: HA's report is
  shown, and a command goes to `light.office` once, so the Hue bridge changes the bulbs together.
- **While any member is paused, they stand for the members** (K14): the paused bulb is out - not
  shown in the mean, not commanded - and the others are commanded one by one (`Card::liveEntities()`).
- **Pausing Desk itself pauses the group and every member**; resuming resumes them all (K18).
- **"On when: any / all" is not offered** for such a group: HA or Hue decides.
- A member's pause is saved like any other and applied once the member is learnt after a reboot.

**Scenes** (K15, K19, K21, K43). A light's scenes are HA's scene entities on the light's own device -
for a Hue room, the scenes made in the Hue app - found by one `render_template` per session
(`ha-websocket.md` §7a) and learnt like members, each a BUTTON entity named as HA names it. Scenes
kept in HA's `scenes.yaml` belong to no device and are not offered. Scenes hidden in HA's UI
(`hidden_by`, read through `is_hidden_entity`) are hidden here too, unless SCENES' "Show hidden
scenes" is ticked.

- **Scenes is a view of its own**, its icon (`mdi:movie-open`, a clapperboard) under the chart in the
  window's corner.
- **The brightness slider stays where it is; the scene buttons take the column** - the label, the
  value and the selector give way. Sized as a share of the window (9 P4_5 millimeters, never under
  the swatches' 6.5 mm), three rows in the slider's height, and half a fourth row shows when there
  are more. Centered, sorted by name.
- **A tap loads the scene** (`scene.turn_on`) and rings that button. The ring means "as the scene
  left it", so it goes with any command from the window, with a change from elsewhere once the
  scene has held still for 3 s, and with the window. HA records only when a scene was last
  activated, so no scene is ever shown as on. A refusal marks it FAILED.
- **The SCENES panel**: Show hidden scenes (checkbox), Default scene, Scene action (K43, K70).

**The control deck sits under the hero where the window has the height** (K23): the 4B and the 7"
panels; the P4_5, the one board too short, keeps it beside the slider. The slider, its words and
the deck are centered. The control deck never moves; in Scenes, where there is no deck, its room is
kept so the slider stays at the same height.

**The control deck after TouchFLO 3D** (K24). A **ribbon** three quarters of a switch tall, and on
it the **selector**, as tall as the deck, carrying the chosen switch's icon a size up in the accent.
The selector can be grabbed and slid: its icon becomes a neighbor's once it is more than halfway
over it, and on release it snaps to the nearest switch, which is chosen. A tap on a switch glides it
there (140 ms). Power is an action, never selected: it has a short ribbon of its own with a break
before the modes' ribbon. The selector is metal (a two-tone face with a sharp step a little below
the middle: gunmetal on the dark schemes, silver on Linen); lit corner chips take the same metal.

**The slider's place** (K26, `placeWide()`): in Scenes, and Color under a tall window, the slider
stays exactly where the controls put it and the buttons or swatches sit just to its right, centered
top to bottom in its height. Only what does not fit moves anything, and only as far as it must:
wider than the room (more scene columns), the slider goes left by the difference; running under
the clapperboard chip, the content first drops to the bottom of the slider's height, then the slider
goes left until it clears; and when neither is enough (the 4B's seven scenes), the scenes take the
columns that fit beside the chip and scroll. The row is moved with `translate_x`, so taps land
where it is drawn.

**Four looks, hand-drawn faces** (K27, K31, K36): Black - Square, Black - Round, Silver - Square,
Silver - Round; Black - Square on Midnight and Fleet and Silver - Round on Linen unless chosen. The
choice is per scheme (UI Tweaks, or a card's own in Card UI). The faces are ARGB8888 images painted
per pixel (`paintFace()`): the light and dark halves meet in a line that curves up in the middle,
with faint brushed streaks and a fine edge; an unlit chip is a soft dent. A face is painted the
first time a size is needed and kept in PSRAM in 12 slots (~40-55 KB each on the P4_5), **taken back
when full**: a slot painted in another scheme's colors first, else the one asked for longest ago by
an earlier window, never one the open window may be showing. They are **dithered to the 16-bit grid**
(4x4 ordered), since a smooth grey ramp truncated to RGB565 bands pink and green. The selector's
icon is scaled 1.4x; lit chips use the next face up; pressed chips tint.

**SETTINGS' rows** (K25): one list of rows (`deckSpecs()`) gives both the panel's size and its
contents. Dropdowns for choices, a checkbox for a yes/no. The rows: **Active state** (Any / All
members are on - only for a group made here with a member that can be on); **Tap action** (Toggle,
Details view, Group view, Members view, History view, Scenes..., Nothing - only what the card can
do); **Long press opens** (Details view, Members view, Group view, History view - never a toggle);
**Units** (a temperature's C or F, only where there is a choice); **Entity properties** (a button to
a read-only view: HA's name, entity id, HA's area, device, source, class, unit, members, the card's
label and id, and its other cards); **Visibility** (a button to a view whose options depend on where
the card is, `ha-cards.md` §3); **Card UI** (a button to the card's own looks: Entity label, Card
label, Selector and its shape, Card density, Window timeout, Timer bar, and for a group Card state,
Card appearance and Card margins; each starts at Inherit, D-28). Paused is in Visibility (K111).

**Text on the panels is US English** (K30). Color shows only the swatches, with no word over them.

## 17. Card ids and saved settings

What the problem is: a device stays customized instead of reverting to its compiled configuration at
every reset.

**How it works.** The board boots from what is compiled in (Garage - Doors and the Fleet page's
Panel, `ha-cards.md` §2), then applies the saved file on top; since K171 the file holds nearly every
card. A value comes from the first layer that sets it: the file, the card's spec, the page's spec,
the built-in default (D-7). New firmware keeps every saved change for cards that are still there.

**Ids (K33).** Every saved change says which card it belongs to. An id is made once, when the card
is created, never changed and never reused. Format `<area>_<label>_<yymmdd>_<hhmm>`, for example
`office_desk_261008_0310`; the label is dropped where it repeats the area, and a same-minute twin
takes a letter (`..._0310b`). `checkCardIds()` runs at boot: a card with no id, a malformed one or a
duplicate draws but is never given its id, so it saves nothing. The same entity on several cards is
several ids.

**A card's life (K35, K66, K160).**

| State | Its saved settings |
|---|---|
| Shown | - |
| Dropped because the grid is too small | still on its page; listed under Hidden as "No room" with nothing to restore |
| Hidden from its page (Visibility's Hide from this page) | in the page's pool, kept (`removed`: `yes`); listed under Hidden in the page's Edit cards, where Restore brings it back |
| Unpinned from its group | still a member (`hidden`: `yes`), off the page, in the group view and Members; Pin this card, or Edit cards' tap, puts it back |
| Deleted | only by the web app (K117) |
| No longer in the dashboard definition | kept, listed as "unclaimed", cleared only by the owner |
| Its entity gone from HA | kept, listed as "entity missing": the board cannot tell "gone" from "HA restarting" or "renamed" |

**The file (K34).** One JSON file on LittleFS in the `spiffs` partition, in the build sheet's shape.
Only what was changed is stored; choices by name, never by enum number, so reordering an enum cannot
change what a saved choice means; keys this firmware does not know are kept when it rewrites the
file. Credentials stay in NVS. It is written whole to a temporary file and renamed, once, when the
window closes, only if something changed - on a task of its own with its stack in internal RAM (a
flash write from a task with a PSRAM stack reboots the board, LESSONS). `Settings.h` describes the
file's behavior and `HTTP_REFERENCE.md` the addresses that read and change it.

```json
{ "schema": 1,
  "schemes":  { "linen": { "selector": "silver_round" } },
  "entities": { "ha_light_office_lamp": { "paused": true } },
  "cards":    { "office_desk_261008_0310": { "label": "custom", "name": "Desk lamp", "scenes": "all" } } }
```

Paused lives in this file too (`EntityRegistry::PauseStore`). Active state and Scenes are read each
time a page is built, so they survive a page swipe.

**Card settings that the window keeps**: Label (Inherit / From HA / State / Custom / None, K40), Tap
action and Long press opens (K37, K39, K156), Scenes (Visible / All, K21), Scene action and Default
scene (K70), Units, Active state, and every History choice (K56).

**Labels (K42, K49).** A name the dashboard gives a card ("Desk" for HA's "Office") is a **Custom**
name; **Inherit** means the card has no name of its own and shows HA's. The system panel's label knob
is a test tool: State or None shows the whole page that way; Name shows each card's own
(`cardResolveLabel()` in `CardIcons.cpp`).

**Groups for card labels (K46, K47).** One kind of group: the dashboard's own version of an area - a
name, a color, perhaps a link to an HA area - belonging to the device, so it spans pages (the group
model: `group-cards.md`). A card's label shows its group's name unless it has its own words.

| Group id | What it is | Its name |
|---|---|---|
| `ha_<area_id>` | every HA area an entity here is in (HA's `area_id()`, learnt with the scenes) | a local rename (saved), else the dashboard's name for it, else HA's |
| `sheet_<name>` | a dashboard area that is not one HA area | the dashboard's |
| `<name>_<stamp>` | made by the owner (kept), or by "Own group" for one card (`kind: solo`, deleted when its last card leaves) | saved |

A card's group is its saved `group` (an id or `none`), else its dashboard area's. Its words are
`label_text` if it has its own (the group's color kept), else the group's name. **Color is the key to
a group and never follows a rename**: a group's color is saved, else the color of its own name (the
dashboard's or HA's, or its id for one made here). **Groups try to have colors of their own**
(K47, `CardGroups.cpp` `autoColour()`): colors set on purpose first, then each group its own name's
color if no group before it has it, then the free colors for the rest. A group's color is worked
out again for every card when any of HA's areas changes (`CardBinder::tick()`). A group made with
"Own group" takes a color no other group uses while one is left. Renaming a group renames every card
in it. From a PC: `/settings?group=new&name=..`, `group=<id>&name=..`, `group=<id>&color=RRGGBB`,
`group=<id>&delete=1`, `card=<id>&group=..`, `card=<id>&label_text=..`. The System Doctor's
[SETTINGS] lists every group and `/popup?groups=1` (debug) lists the groups and each card's.

**A lost setting must never happen** (A17): a file that will not read makes the boot read-only
instead of being overwritten, and `/settings?stats=1` says what the boot saw.

## 18. History

**Scope:** Home Assistant's entities; the board's own and MQTT entities have no history yet.

**What the view shows follows the entity** (K52):
- **A number** (a sensor whose value is a number): a chart over a span picked on the view, with now,
  min and max. The left panel is **CHART**: Span, Legend (None / X axis / Y axis / X / Y axes), Show
  min/max, Shading: under line, Span buttons.
- **Anything that turns on and off** (light, switch, binary sensor, input boolean, a text sensor):
  its **activity** - each change, newest first, the state in the card's own words (On / Off,
  Detected / Clear, Open / Closed). The left panel is **ACTIVITY**: Span (Last 24 hours / Last 7 days),
  Show duration, Show triggers, Span buttons.
- **A group defined in HA** (Desk = `light.office`) is its own entity, as in HA (K54): its members'
  histories are each member's, reached through Members. A group made on the board has none of its own.
- A light defaults to its activity; a card that toggles nothing taps to History (K57).
- **Every History choice is kept per card** (K56), by name under the card id; a member's own History
  keeps nothing.

**The chart.** A line over the span with a gradient under it (a checkbox, K61); the span chips
24 / 12 / 6 / 1 h are a compact selector on the view (K75), with "Showing: ..." in their place when
hidden, which a tap steps through the spans. **One top line** (K63, K64): Min over Max (the numbers
right-aligned) at the left, "Now: ..." at the right, the chips centered between them (narrowing to
fit) - it reaches up into the header row's empty lower half (`histLift()`; only the middle is
clickable there, so the corner buttons keep their taps), and `histTopRow()` builds it for the chart,
the activity and the loading line so nothing jumps when the answer arrives. Min and max are faint
dotted lines at the time each happened, carrying their times ("Max: 3:53 PM"), their values
inside the plot above or below the line, never on it; **a finger on the chart** puts a dotted line
at that time with its time and value, and slides. Axes are in round steps: values a label every
~2.4 lines of text of plot height, each step 1 / 2 / 2.5 / 5 x 10^n, the range widened to whole
steps, the unit on the top label; times ticks on the local clock (5, 10, 15, 30 min, 1, 2, 3, 4, 6,
12 h - the shortest whose labels fit), a faint grid line at every tick. Legend chooses which labels;
the grid stays. The chart samples HA's points at even steps across the span, each step holding the
last value at or before it, so changes are steps as HA draws a sensor; the y range is the span's
own plus a tenth; values follow the card's C/F.

**Activity** is one view (K62): the state on the left (centered when alone); the time over "12
minutes ago" / "Yesterday" / "3 days ago - Oct 8" on the right (`TfAgo::AGO_DATED`); day headings
always. Show duration ("for: 38 sec", "(and counting)", off by default) and Show triggers add small
lines under the state; a tap adds the trigger, or with triggers on, HA's own words about it. Each
change says what caused it: a person ("By psyki"), an automation, another entity, an action. Rows
at most ~70 mm wide; 10 rows, then "Show more". Wording goes through `TimeFormat` (K59).

**Where the data comes from** (K53). A number's real changes (`history/history_during_period`,
minimal, no attributes), with 5-minute statistics (`recorder/statistics_during_period`) only as the
fallback for a sensor too chatty for the receive buffer; activity from `logbook/get_events`, which
carries the cause (history has none); people named from `render_template` over `states.person`,
once a session. `HaHistory` (`components/Fleet_HA`, no LVGL) asks on the loop task; its answers are
told from everything else by their id through a raw hook HaProvider offers before its filtered
parse, and parsed on the websocket task - with ArduinoJson's pool in PSRAM - into tables in PSRAM
(up to 1,024 points). The window's tick notices the answer and draws. One request at a time; a new
one replaces the last, and a late answer is dropped by its id. HaClient's receive buffer is 48 KB in
PSRAM where the board has it (8 KB internal otherwise); an answer past it is dropped and counted,
a chart then asks for statistics, and activity says "Too much history to show here". A light's
brightness over time would need history with attributes: not built.
