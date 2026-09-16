# The real dashboard — 18 entities for `WS_P4_7B`

**Status: the target, not the build.** None of these entities exist on the device yet. They arrive
with #43 (Home Assistant over the websocket), and this page becomes the `PageSpec` that replaces
`Dashboard_Fleet.h`'s stand-in content the day they do.

Supplied by the owner 2026-09-15 (`Fleet_Dashboard_test_-_18_entities.txt`), and reproduced here
because a list that lives outside the repo is a list that goes stale silently.

---

## The list

| # | Area | Type | Name | Corner icon | Hero icon | Entity |
|---|---|---|---|---|---|---|
| 1 | Living | light | Overhead | `mdi:lightbulb` | `mdi:ceiling-light-outline` | `switch.tv_room_switch_1` |
| 2 | Kitchen | light | Sink | `mdi:lightbulb` | `mdi:lights-recessed` | `light.kitchen_switch_1` |
| 3 | Kitchen | light | Table | `mdi:lightbulb` | `mdi:lights-recessed` | `light.dining_room_light` |
| 4 | Kitchen | temperature | — | `mdi:thermometer` | — | `sensor.kitchen_mmwave_temperature` |
| 5 | Kitchen | occupancy | — | `mdi:motion-sensor` | `mdi:motion-sensor` / `-off` | `binary_sensor.kitchen_occupancy` |
| 6 | Front | temperature | Thermo | `mdi:thermometer` | — | `sensor.thermostat_temperature` |
| 7 | Office | light | Desk | `mdi:lightbulb-group` | — | `light.office` |
| 8 | Office | light | Overhead | `mdi:lightbulb` | `mdi:light-recessed` | `light.office_overhead` |
| 9 | Office | occupancy | — | `mdi:motion-sensor` | `mdi:motion-sensor` / `-off` | `binary_sensor.office_occupancy` |
| 10 | Office | temperature | — | `mdi:thermometer` | — | `sensor.mmwave_temperature` |
| 11 | Bedroom | temperature | Eric | `mdi:thermometer` | — | `sensor.temp_3_eric_bedroom_temperature` |
| 12 | Outside | temperature | Front | `mdi:thermometer` | — | `sensor.temp_2_front_door_temperature` |
| 13 | Outside | temperature | Deck | `mdi:thermometer` | — | `sensor.outdoor_deck_motion_temperature` |
| 14 | Outside | light | Front | `mdi:lightbulb` | `mdi:coach-lamp` | `light.porch_switch_1` |
| 15 | Outside | lux | Deck | `mdi:brightness-5` | — | `sensor.outdoor_deck_motion_illuminance` |
| 16 | Garage | door | North | `mdi:garage` | `mdi:garage` / `garage-open` | `binary_sensor.door_sensor_2_garage_north` |
| 17 | Garage | door | South | `mdi:garage` | `mdi:garage` / `garage-open` | `binary_sensor.door_sensor_3_south_garage_opening_2` |
| 18 | Garage | temperature | — | `mdi:thermometer` | — | `sensor.temp_1_garage_temperature` |

Six areas — Living, Kitchen, Front, Office, Outside, Bedroom, Garage — and 18 cards against the
7B's 18-to-21 usable cells. It fits, with no room spare, which makes it a real test of priority
degradation on the smaller boards rather than a theoretical one.

---

## What this list tells us that the code does not yet do

It is worth reading as a requirements document, because five things in it are not built.

**1. The corner icon is the DOMAIN; the hero is the specific thing.** Rows 1, 2 and 14 are all
lights and all carry `mdi:lightbulb` in the corner, but their heroes are a ceiling light, recessed
lights and a coach lamp. This was already recorded in `HANDOFF.md` as deliberately postponed — it
is now specified. `cardIconFor()` currently returns one icon per entity; it needs to become two
lookups, one keyed on domain and one on the specific fixture.

**2. Hero icons change with state, and not just in colour.** Occupancy wants
`mdi:motion-sensor-off` when clear; a garage door wants `mdi:garage-open` when open.
`cardIconForState()` exists and is the right hook — the glyphs do not exist in the generated
subset yet, and adding them means regenerating every board's icon font.

**3. `door` is a card type we do not have.** Rows 16 and 17 are `binary_sensor` with a garage
device class. Today they would render as a generic `BinarySensorCard`. Milestone 2.7 territory.

**4. `light.office` is a group entity, dimmable, RGB and colour-temp.** `LightCard` is currently
identical to `SwitchCard`. `cards.md` §4 already says a light card mirrors colour and colour
temperature where the light reports them, and row 7 is the entity that will demand it.
`light.dining_room_light` (row 3) is dimmable too.

**5. The area names are the owner's, not Home Assistant's.** "Front", "Outside", "Living". Once
#43 can read the area registry we will find out whether HA's own area names match these; where
they do not, the build sheet's `area` override is the answer and this table is the record of what
he actually wants to read.

---

## One naming caution

The owner intends to rename his HA entities so their ids carry area, domain and function. **The
entity ids in this table will therefore change.** Nothing should be written that parses meaning
out of an entity id — the current set already disproves that idea on its own, since
`switch.office_plug_3d_printer` is named "Living Room Plug".
