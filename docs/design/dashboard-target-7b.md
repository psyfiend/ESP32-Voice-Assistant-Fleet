# The 18-entity test dashboard for `WS_P4_7B`

The owner's reference list of what a real House page holds: 18 cards in seven areas against the 7B's
18 to 21 usable cells. It fits with no room spare, which makes it a test of priority degradation on
the smaller boards. The cards are now made on the board and kept in each board's settings file
(K171); the specs that built them are kept under `#if 0` in `include/Dashboards/Dashboard_HA.h`, and
`scripts/release/retire_compiled.py` converted them. The entity ids in that header are the current
ones (K127); this page lists what each card is.

## The list

| # | Area | Type | Name | Corner icon | Hero icon |
|---|---|---|---|---|---|
| 1 | Living | light | Overhead | `mdi:lightbulb` | `mdi:ceiling-light-outline` |
| 2 | Kitchen | light | Sink | `mdi:lightbulb` | `mdi:lights-recessed` |
| 3 | Kitchen | light | Table | `mdi:lightbulb` | `mdi:lights-recessed` |
| 4 | Kitchen | temperature | - | `mdi:thermometer` | - |
| 5 | Kitchen | occupancy | - | `mdi:motion-sensor` | `mdi:motion-sensor` / `-off` |
| 6 | Front | temperature | Thermo | `mdi:thermometer` | - |
| 7 | Office | light | Desk | `mdi:lightbulb-group` | - |
| 8 | Office | light | Overhead | `mdi:lightbulb` | `mdi:light-recessed` |
| 9 | Office | occupancy | - | `mdi:motion-sensor` | `mdi:motion-sensor` / `-off` |
| 10 | Office | temperature | - | `mdi:thermometer` | - |
| 11 | Bedroom | temperature | Eric | `mdi:thermometer` | - |
| 12 | Outside | temperature | Front | `mdi:thermometer` | - |
| 13 | Outside | temperature | Deck | `mdi:thermometer` | - |
| 14 | Outside | light | Front | `mdi:lightbulb` | `mdi:coach-lamp` |
| 15 | Outside | lux | Deck | `mdi:brightness-5` | - |
| 16 | Garage | door | North | `mdi:garage` | `mdi:garage` / `garage-open` |
| 17 | Garage | door | South | `mdi:garage` | `mdi:garage` / `garage-open` |
| 18 | Garage | temperature | - | `mdi:thermometer` | - |

The names are the owner's ("Front", "Outside", "Living"), supplied as card names; HA's own area
names may differ, and a card's name and label are its own (K49).

## What the list requires, and where it is built

- **The corner icon is the domain; the hero is the specific thing.** Three lights carry
  `mdi:lightbulb` in the corner with a ceiling light, recessed lights and a coach lamp as heroes.
  `cardCornerIcon()` and the per-card icon (`Cards/CardIcons`) are two lookups. A card with a hero
  icon has no corner icon (K172).
- **A hero icon changes with state**: `mdi:motion-sensor-off` when clear, `mdi:garage-open` when
  open (the device-class table in `CardIcons.cpp`). Every glyph used must be in the generated icon
  subset (`scan_ha_icons.py`, K69).
- **A door is a binary sensor with a device class** (`garage_door`), drawn by `BinarySensorCard`.
- **`light.office` is a group entity**, dimmable, RGB and color-temperature; `LightCard` and the
  card window's controls follow what the light reports (`card-sheet.md`).

Nothing should parse meaning out of an entity id: the owner renames his HA entities, and
`switch.office_plug_3d_printer` is named "Living Room Plug".
