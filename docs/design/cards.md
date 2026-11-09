# Cards - what a card contains and how it behaves

**The card design: what a card may show, how it says what state it is in, and what each type
draws.** Built (`src/Cards/`, `include/Cards/`). Companion to `docs/design/startup.md` and
`tokens.md` (the values); the long-press window is `card-sheet.md` and a set of entities shown
together is `group-cards.md`. Decisions: K1, K2, K42, K49, K172 and the card-type decisions in
`DECISIONS.md`.

---

## 0. Color carries meaning

Several semantic colors on screen at once, rather than one accent repeated on every card, is what
makes a page read as inviting. State color is therefore a first-class token set
(`UI::pal()`'s semantic state palette: active, idle, open, closed, alert, stale), separate from the
single accent, and every card type declares which of its states map onto it.

---

## 1. What a card may contain

**One primary value plus at most two secondaries.** Beyond that a card is unreadable at the smallest
sizes, and every candidate for a third turned out to belong in a group card
(`group-cards.md`).

A legitimate secondary is another entity **of the same physical device** as the primary - which in
practice is **battery, and last seen** (last seen always refers to the primary entity, never to a
sibling).

| Slot | Content |
|---|---|
| Primary | The value, or the state; its size is set by the type scale, not per card |
| Icon | State-reflecting where possible (§5) |
| Label | What this *is* - often the location, not the measurement ("Deck", not "Temperature") |
| Secondary | Contextual and minimal (`2h`, `60%`) - never a sentence |
| Status row | Battery and/or last seen; absent in the compact variant and when the entity has neither |
| Card header | The card label (§2), and the STALE tag |

**Provenance never appears on a card.** A card must never branch on an entity's `source`, and
rendering it is the same coupling. Provenance is diagnostic and belongs in `SystemReport`.

For a dimmable light, brightness is primary content, not status-row material.

**Compact is the fleet default.** A card measures whether its cell can seat a title row, the hero and
an optional row at the type scale's sizes, and draws **less** when it cannot - never the same thing
smaller, which would undo the type scale. `ValueCard` drops the status corners; `StateCard` drops
the name and keeps the icon, because state is the icon and its color. Full and Auto remain choices
(`CardVariant`, Card density in UI Tweaks and Card UI).

---

## 2. The card label

A card's colored name is its **card label** (K42): the words are the *card label text* (a group's
name, or the card's own), the shape is the *card label type*. The types (`CardHeaderStyle`) are
**float** (pills lying across the card's top corners; the default), **tag** (pills hanging outside,
above the card), **band** (a filled band inside the card, edge to edge) and **none** (plain text in
the top strip). The label type is a UI Tweaks choice (`card-sheet.md` §11.3) and the text is chosen
in Card UI. A card with no area hides its pill and keeps its space, so cards stay lined up.

- The label is drawn in the card's **background** color on a solid group-color fill: dark text on a
  light fill, light text on a dark one.
- **Tag mode costs the row gap:** the tag hangs outside the card, so the cell must not clip
  (`LV_OBJ_FLAG_OVERFLOW_VISIBLE`), the tag parents to the cell rather than the card, and the page
  widens its row gap to `gap + tagHeight` (a sum, so the space above a tag equals the space between
  two plain cards). Every card in tag mode takes the shorter height whether it carries a tag or not.
- **The band overhangs the card's corners** (it is `lv_pct(100)` of the surface with the card's
  radius and is not masked by it; `clip_corner` would hide it but froze the P4_5, LESSONS).
- **A solo card's header keeps this formula** (its label on the left, STALE on the right); slots
  are for the page header and for group headers (`header-slots.md` §8).
- Area is learnt from HA (the websocket, `ha-websocket.md`), not from MQTT: MQTT carries a value per
  topic and nothing else.

---

## 3. Staleness, failure and pause

**Dimming is never the signal for stale data**: a dimmed card is easy to miss, and a dashboard that
quietly lies is worse than a blank one. The states (`CardState`):

| State | Means | Shown as |
|---|---|---|
| `ST_LIVE` | fresh | nothing extra |
| `ST_STALE` | past the entity's `staleAfterMs` | a bright STALE tag in the header |
| `ST_LONG_STALE` | past the second threshold | unmistakable: the tag grows or a diagonal line crosses the card; the card stays readable |
| `ST_REFUSED` | **we commanded it and it did not happen** | FAILED, at once - never waiting for a timeout |
| `ST_PARTIAL` | some of what a group was commanded did not happen | PARTIAL |
| `ST_PAUSED` | the user's own choice | quiet; it may dim |

- **The second threshold is its own field, not a multiple.** Defaults live per data type in a
  library header (`CardDefaults.h`): a switch or light goes stale in seconds, temperature or
  occupancy in minutes or hours; a build sheet may override them.
- **Stale means "I have not heard from this"; refused means "I told it to act and it did not".**
  They use different words on screen. The refused state is the entity registry's optimistic write
  reverting when no echo arrives (A4); `Entity::cmdFailed` records the verdict on the entity so
  every card bound to it reads the same fact, and a parent's state derives from its children's:
  all failed -> FAILED, some -> PARTIAL.
- A source that says an entity is not there is a different state again (#56).
- A **paused** card (the entity's own setting) is quiet by design.

---

## 4. Card types

Types are named for the entity's domain (`CardCatalog.h`, reached through `cardForKind()`); the two
**layouts** are abstract - `ValueCard` and `StateCard` - and a user picks `sensor`, the framework
picks the arrangement (K1).

| Kind | Type | What it shows |
|---|---|---|
| sensor | `SensorCard` (value layout) | the value dominant, the label the location; History on a tap |
| binary sensor | `BinarySensorCard` (state layout) | a state: icon and words from a `device_class` table, §5 |
| light | `LightCard` | the whole surface reflects state; the disc takes the light's own color; brightness fills the card from the bottom up |
| switch | `SwitchCard` | on or off |
| button / scene | `ButtonCard` | fires an action or scene |
| climate | `ClimateCard` | the thermostat's temperature and "Heat to 68°"; read-only (`ha-cards.md` §3.7) |
| a group of entities | `ClusterCard` | `group-cards.md` |

Number, text and weather entities have no card type. A weather card (a minimum 2 x 2, with an
optional full page) is not built.

**Sensors.** The label is the location, the icon already says the quantity. History is fetched from
HA on demand and kept only while the chart is on screen (`card-sheet.md` §18), so a sensor costs no
permanent RAM; a purely local sensor has no HA history and shows none.

**Binary sensors** are one card type driven by a `device_class` table - a row per class carries the
corner glyph, the on/off hero pair and the on/off words; adding `valve` or `moisture` is a row.
HA's "Show as" is a `device_class` override, so it feeds this table. The label is name, state word
or none (default name, because the name tells two doors in one area apart); the state word is the
table's ("Open" / "Closed", "Detected" / "Clear"), never a raw on/off. The color is the ordinary
active color for an open door and a detected presence alike; the icon carries the difference.

**Lights.**
- The disc takes the light's own color when it reports one (HA sends `rgb_color` for every color
  mode). The whole surface is not tinted: legibility on a saturated color is the risk.
- Brightness fills the card from the bottom up after HA's own tile: off is empty, 100% fully
  filled, a light with no brightness fills fully when on. The disc says on/off; the fill says how
  much.
- A tap toggles and a long press opens the window (K3). There is no slider on the card.
- **Mixed state gets its own indicator** rather than picking a side (a group of doors reads "Both", "All" or "2 open").

**Scene / action.** An outbound command (Button); over MQTT the first consumer of #44.

---

## 5. Icons

Two icons per card, answering different questions:

| | Question | Source, first match wins |
|---|---|---|
| **Corner** | what KIND of card this is | build-sheet override, then our table keyed on `device_class`, then domain. HA has no per-entity "type" icon |
| **Hero** | this THING in its current state | build-sheet on/off pair, build-sheet icon, HA's live `attributes.icon`, our `device_class` state pair, the corner icon |

- **Only value cards (temperature, humidity, light level) keep a corner icon**; on/off cards -
  lights, doors, occupancy, switches - have none, their hero icon says it (K172). The corner icon is
  glued to the corner (the same distance from the card's left edge as from whatever bounds its top)
  and scales with the card; it is out of the vertical flow, so the centered hero never collides.
  The two icons must not touch.
- **HA core does not compute a state-dependent icon**; `attributes.icon` exists only when a user or
  integration set one. Our own state pairs are therefore the main source for binary sensors. A
  static icon set in HA freezes the hero, as in HA's own frontend; the build-sheet pair is the
  escape hatch.
- **Any override glyph must be in the generated font** (`scripts/gen_icon_font.py`'s `GLYPHS`,
  maintained by hand; `scripts/scan_ha_icons.py` reports what HA uses that we do not ship). A name
  the font lacks falls through to the next source rather than drawing a blank box. The subset covers
  every MDI icon the owner's HA serves, since a card can be made from an entity nobody declared.
- **Corner icons are tinted by kind**: lighting yellow-white (deeper amber on light schemes),
  openings blue, presence teal; quantities keep their own tints. The ink on an active fill is chosen
  by contrast (`UI::contrastOf()`), not the scheme's card color. A group's bulb is its hero.
- **Flash.** One referenced font face costs 6-73 KB (`CLAUDE.md`, "Two costs"): state variants are
  cheap, extra sizes are not. The shipped subset is two sizes per board - a state card's disc and a
  value card's title row.

---

## 6. Grid density per board

The column count is derived: pick a target card width and the page fits as many whole cards as the
screen allows, with a fixed gap and cluster inset. The same tokens give 5x3 on the P4_5 and 2x3 on
the 3248 portrait, with no per-board layout code. The 4B pair (`WS_S3_4B` 480 x 480, `WS_P4_4B`
720 x 720 at 1.5x) is the same layout in different pixels: the same effective space, the same
grid. Per-board defaults: D-6. `WS_S3_5B` has the pixels for 5x3 but the refresh of something much
smaller (`FUTURE_IMPROVEMENTS.md`, the RGB `num_fbs` investigation).

---

## 7. Type sizes by card size

Only the roles that make a card cramped step with its cell: the **hero value**, the **hero icon**
and the **corner icon** (the card-size faces `VALUE_SM` and `ICON_MD`, chosen by the cell's size in
millimeters so a card looks the same across boards). Name, unit, tag and status text stay one size
per board.

- **TAG is not scaled**: what makes a card cramped is the vertical stack and VALUE dominates it;
  shrinking a small label reclaims almost no height. `gen_type_scale.py`'s `SCALE` takes a per-role
  dict as well as a float, because sizes quantize to even pixels and a global factor lands several
  scales on one size.
- **A generated VALUE face carries only the glyphs the script lists** (its `num` range). Anything a
  card prints in the VALUE role must be in that range - digits, colon, `d` for durations, and any
  future unit or separator.
- **A card's name sits centered between the disc's bottom edge and the card's bottom edge** in
  compact, measured against the state card's disc on both layouts so a mixed row keeps its names
  level.

---

## 8. One slot system

The page header and a group's header are one mechanism: named slots filled from a configuration and
placed by a planner (`header-slots.md`). A solo card's header is its label formula (§2).

---

## 9. Values and formats

- **Temperature unit** (`TempUnit`): C or F as displayed, whatever the source reports; conversion
  happens at format time (the registry keeps what the source said, because that is what an echo is
  compared with, what an optimistic write reverts to and what our own discovery publishes). The
  default measures the source; a card's Units row overrides it (`card-sheet.md` §16). The unit label
  goes through `cardDisplayUnit()`.
- **Duration** (`DurationFormat`): `DUR_AUTO` (the default) switches by magnitude (`09:58`,
  `1:06:40`, `3d 04:15`), `DUR_CLOCK` is fixed `H:MM:SS`, `DUR_SECONDS` the raw count. It is keyed on
  `device_class: duration`, not the unit string, and the unit label is suppressed for a formatted
  duration.
- **The entity label** (K49): a name the dashboard gives a card is **Custom**; **Inherit** means the
  card has no name of its own and shows HA's.
- **Last changed.** `Seen:` reads `lastUpdateMs`, which `EntityRegistry::setValue()` sets for every
  report including unchanged values - a device-liveness time that staleness uses; a card's own
  "changed" line reads `lastChangeMs` (#57).
- **Cards bubble their events** (`LV_OBJ_FLAG_EVENT_BUBBLE` on the surface): the page swipe records
  its press origin on the screen, and a clickable object that consumes its own events would report
  an origin of `{0,0}`.

---

## 10. Surfaces

- **Schemes** are Fleet, Midnight (the default) and Linen, in one order (`UI::cycleScheme()`).
  Linen's cards have a drop shadow and a 2 px darker border; the dark schemes have none.
- **An active card** says so by its card state (Fill, Icon glow, Bracket, Corner cut) drawn in the
  surface's draw events (`CLAUDE.md`, "Design tokens").
- The accent rail is not decoration: it exists only as the state indicator, at least 10 px wide.

---

## 11. Reference material

`docs/REFERENCE_PROJECTS.md` says what each project in `reference/` is and how it is licensed -
read it before reusing anything. `esphome-modular-lvgl-buttons` (MIT) splits its UI per entity type
the way card types and context sheets are split here; `espcontrol` (PolyForm Noncommercial) may be
studied for architecture and per-device grids but its code may not be copied.
