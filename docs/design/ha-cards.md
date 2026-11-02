# 2.13 - Cards from Home Assistant

**What 2.13 builds, and how.** Decisions: DECISIONS K130-K145 (the owner's, 2026-10-30 to 11-02).
The look: artifact "2.13 Add Cards Mocks" (six screens); the alerts it was designed with (2.14):
artifact "2.13 Alert Flow Mocks", K138-K139. Issue #116.

## 1. What the owner sees

| Screen | What | Decision |
|---|---|---|
| 1 | A cluster's **Add cards** with a source choice, On this board / From Home Assistant: its area's `fleet` entities first, each row saying what is already held, then Other areas | K140 |
| 2 | The page panel's **Add card**: every area (HA's floors as sections, No area last) beside its entities | K140 |
| 3 | The page panel's **New cluster**: K91's flow from an area - Tiles and Header preselected, a name, Create | K140, K91 |
| 4 | The CLUSTER panel's **Area** row (the group's link); "1 new" on Add cards; the notice "New in Office: 3D Printer" | K141, K142 |
| 5 | **The group view**: a tap on a cluster; every member, hidden ones last and marked; fills the page area; list and grid buttons between it and Members | K143 |
| 6 | **The thermostat**, read-only: card and window | K144 |

Words: K145. Nothing from HA is added by itself (K133).

## 2. The foundation (Claude's design, from the code as of `v0.2.20`)

Read from the code by an explorer, 2026-11-02; the numbers are the code's.

### 2.1 Subscribing to more than ~45 entities

`HaProvider::sendSubscribe()` builds one `subscribe_trigger` in a 1,536-byte frame and refuses the
whole subscription when the list does not fit - about 45 typical entity ids. The owner labels 70
entities `fleet`. **The subscription is sent in chunks**: one `subscribe_trigger` per frame-full, the
next starting where the last stopped (HA keeps every one), so any number of entities is subscribed.
The scene-and-area template's 4 KB reply buffer and 3 KB request get the same treatment: the
reply is copied into a buffer sized to it (PSRAM), the request built in a larger one.

### 2.2 Room

| Limit | Was | Now | Why |
|---|---|---|---|
| `ENTITY_MAX` (registry slots, PSRAM, ~1 KB each) | 128 | 192 | 38 at boot + 70 `fleet` + learnt members and scenes; the index type allows 255 |
| `HaPublisher`'s per-slot arrays | internal RAM, ~62 B x `ENTITY_MAX` | PSRAM | they grew with the registry on boards with 20 KB to spare |
| `CardAdded::MAX` (cards made on the board) | 24, internal RAM | 64, PSRAM | Add card from HA makes cards by the handful |

### 2.3 An entity made at runtime

An entity HA has and the compiled table does not is made the way 2.10c's learnt members are
(A13): its id from HA's (`switch.office_3d_printer` -> `ha_switch_office_3d_printer`), its kind from
the domain (light, switch / input_boolean / fan -> switch, binary_sensor, sensor, climate, scene ->
button). **It is kept on the card that uses it**: the card entry gets `ha` (HA's entity id) and,
from the catalogue at the moment it was added, `ha_dc` (device class) and `ha_unit`. At boot,
right after the compiled HA table, every card entry with `ha` makes its entity, so the page finds it
when it is built; HaProvider subscribes it and HaRest fetches its value as for any other. An entity
the board already has (compiled or learnt) is used as it is, with no `ha` key.

### 2.4 The catalogue: what HA offers

One `render_template` per request, answered once and cancelled (the scene query's pattern):
`label_entities('fleet')` with each entity's area id and name, its area's floor, its name, device
class, unit and state, and the floors' and areas' order. Kept in PSRAM, read by the pickers under its
own lock; asked again when a picker opens and when HA says its entity or area registry changed
(`subscribe_events` for `entity_registry_updated` and `area_registry_updated`, debounced).

**New** (K142): the ids the board has seen are kept in the settings file (`system.ha.known`). An id
in the catalogue and not known, in an area a cluster's group is linked to, is new: a notice once,
"1 new" on that cluster's Add cards, New on its row - known once Add cards has shown it. The first
catalogue after this build knows everything (nothing is new on day one).

### 2.5 The notice queue (K142)

`UIToolkit::show_toast()` replaces whatever is showing. Notices are queued instead, each shown for
its time with a bar that runs down, the next one after it - NINA's toast idea, info notices only in
2.13; 2.14 adds the alert levels on the same queue.

## 3. Build progress

Branch `feat/2.13-add-from-ha`, off `main` at `fd3d59e`. Unattended run from 2026-11-02 05:30, with
the owner's permission to flash the three dev boards (settings saved and restored), toggle Office
lights, and reword HA's docs for K133.

- [ ] 2.1 subscription in chunks; the template buffers
- [ ] 2.2 room: registry 192, HaPublisher to PSRAM, CardAdded 64 in PSRAM
- [ ] 2.3 runtime entities from card entries
- [ ] 2.4 the catalogue, registry events, known ids
- [ ] Screen 1 - a cluster's Add cards from HA
- [ ] Screen 2 - Add card
- [ ] Screen 5 - the group view, tap and long press
- [ ] Screen 6 - the thermostat
- [ ] Screen 4 - Area row, "1 new", the notice queue
- [ ] Screen 3 - New cluster
- [ ] Leftovers: Show helper text (K128); a cluster's card state; HA docs (K133)
- [ ] Flash the dev boards, verify, `docs/TEST_2.13.md`
