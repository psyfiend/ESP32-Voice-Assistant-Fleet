# Home Assistant over the websocket — what the API actually gives us

**Status: measured, not designed.** Every number here came from the owner's live instance
(HA 2026.8.1 at `192.168.0.70:8123`) on 2026-09-15 and 2026-09-16, run from a PC. The standing
instruction was not to guess what HA exposes; this is the answer to that, and the design for
issue #43 should be built against it.

Nothing on the device implements any of this yet.

---

## 1. The connection

Plain HTTP/WS on the LAN, no TLS. `ws://192.168.0.70:8123/api/websocket`.

Handshake is three messages and takes milliseconds:

1. server sends `{"type":"auth_required","ha_version":"2026.8.1"}`
2. we send `{"type":"auth","access_token":"..."}`
3. server sends `{"type":"auth_ok"}`

After that every request carries an incrementing `id` and the reply comes back as
`{"id":N,"type":"result","success":true,"result":...}`.

The token is a long-lived access token, already in the gitignored `ConnectivityLocalSecrets.h` as
`LOCAL_HA_ACCESS_TOKEN`.

---

## 2. What each call costs

Measured against 1,662 entities and 152 devices.

| Call | Items | Bytes | Time |
|---|---|---|---|
| `get_states` | 1662 | **787,728** | 0.15 s |
| `config/entity_registry/list` | 2165 | **1,364,297** | 0.24 s |
| `config/device_registry/list` | 152 | 119,713 | 0.02 s |
| `config/area_registry/list` | 14 | **3,663** | instant |
| `get_services` | 73 domains | 79,973 | 0.02 s |
| `get_config` | — | 4,578 | instant |
| `config/entity_registry/get` (one entity) | 1 | **842** | < 1 ms |
| REST `GET /api/states/<id>` (one entity) | 1 | **399–843** | 4–25 ms |

**Two of these are unusable on an ESP32 and two are the way around them.** The full state list and
the full entity registry are three quarters of a megabyte and 1.3 MB respectively. The per-entity
forms are under a kilobyte each, and the area registry — the thing we most want — is under 4 KB
whole.

---

## 3. The live feed, and the mistake to avoid

`subscribe_events` with `event_type: state_changed` is the obvious call and it is the wrong one.
Measured over 60 seconds on this instance:

| | Events | Bytes | Rate |
|---|---|---|---|
| **All `state_changed`** | 731 | **913,205** | 12.2 /s |
| **Our 18 via `subscribe_trigger`** | 0 | 0 | 0 /s |

That is roughly **15 KB/s of JSON, continuously**, to find the handful of messages we care about —
and on this instance three solar sensors alone produced 140 of those 731 events.

**`subscribe_trigger` does the filtering on the server**, and it accepts a list:

```json
{"id":1,"type":"subscribe_trigger",
 "trigger":{"platform":"state","entity_id":["light.office", "..."]}}
```

Verified working. That is a ~100x reduction in traffic and in parsing, and it is the single most
important finding in this document.

### Event sizes

| | Bytes |
|---|---|
| One `state_changed` event | 1,286 |
| …of which `old_state` | 488 — we never need it |
| One `subscribe_trigger` event | 1,365 |
| What we actually keep (id, state, icon) | **71** |

So even on the filtered feed, ~95% of each message is discarded. A streaming parse with an
ArduinoJson filter is the right shape; buffering whole messages and then picking fields is not.

---

## 4. Areas — the thing MQTT could never give us

All 14 areas, 3,663 bytes. Resolution is two hops:

```
entity  --area_id-->  area          (when the entity is assigned directly)
entity  --device_id-->  device  --area_id-->  area   (the usual case)
```

`light.office` has `area_id: null` and inherits from its device, so **both hops are required** —
an implementation that only reads the entity's own `area_id` will find nothing for most entities.

All 18 of the owner's target entities resolve, and the names are nearly the ones he wrote by hand:

| His name | HA's area |
|---|---|
| Living | Living Room |
| Kitchen | Kitchen |
| Front | Front Room |
| Office | Office |
| Bedroom | Eric Bedroom |
| Outside | Outside |
| Garage | Garage |

HA's full list: Back Yard, Bedroom, Eric Bedroom, Front Room, Garage, Kitchen, Lawn, Living Room,
Office, Outside, Ring, Servers, Shed, Sprinklers.

**The device registry is the expensive hop** at 120 KB. It is fetched once at boot and only
`id` + `area_id` are kept (152 × ~50 bytes ≈ 7.6 KB), which a streaming filter does without ever
holding the whole document.

---

## 5. Icons come from HA, including the state-dependent ones

This is the answer to "the different hero icons per entity — is that pulled from my actual HA
instance?" **Yes, and mostly it already matches the list written by hand.**

Nine of the eighteen carry an explicit icon, and they are the same glyphs:

| Entity | HA's icon |
|---|---|
| `switch.tv_room_switch_1` | `mdi:ceiling-light-outline` |
| `light.kitchen_switch_1` | `mdi:light-recessed` |
| `light.dining_room_light` | `mdi:light-recessed` |
| `light.office_overhead` | `mdi:light-recessed` |
| `light.porch_switch_1` | `mdi:coach-lamp` |
| `binary_sensor.kitchen_occupancy` | `mdi:motion-sensor-off` |
| `binary_sensor.office_occupancy` | `mdi:motion-sensor-off` |
| `binary_sensor.door_sensor_2_garage_north` | `mdi:garage` |
| `binary_sensor.door_sensor_3_south_garage_opening_2` | `mdi:garage` |

The other nine are temperatures and an illuminance with no icon set, which fall back to
`device_class` — exactly what `cardIconFor()` already does.

### Two fields, and only one of them changes

| Field | Where | Behaviour |
|---|---|---|
| `icon` in the **entity registry** | static | the user's own override, never changes |
| `icon` in a state's **attributes** | live | what HA would draw *right now* |

`binary_sensor.kitchen_occupancy` has no registry icon but its attributes say
`mdi:motion-sensor-off` while it is `off`. **HA computes the state-dependent glyph and ships it on
every state update.** So reading `attributes.icon` gets the correct hero icon, state variant
included, for nothing.

**The exception is worth telling the owner about.** The garage doors have a *static* registry
override of `mdi:garage`, so they will never show `mdi:garage-open` — his own override is
suppressing the behaviour he asked for. Removing the icon override in HA would give the
open/closed pair automatically, since the entities already carry `device_class: garage_door`.

---

## 6. Capabilities are machine-readable

`supported_color_modes` says exactly what a light card has to be able to do, with no guessing:

| Entity | `supported_color_modes` |
|---|---|
| `light.office` | `['color_temp', 'xy']` |
| `light.dining_room_light` | `['brightness']` |
| `light.kitchen_switch_1`, `light.office_overhead`, `light.porch_switch_1` | `['onoff']` |

That matches the owner's own notes on his list, and it means `LightCard` can decide what controls
to offer from data rather than from a build-sheet flag.

`binary_sensor.door_sensor_2_garage_north` reports `device_class: garage_door`, so the `door` card
type 2.7 owes has a real discriminator.

---

## 7. The shape this implies

Not a decision — the sequence the measurements point at, for whoever designs #43.

**At boot:**
1. connect, `auth`
2. `config/area_registry/list` — 3.6 KB, keep whole
3. `config/device_registry/list` — 120 KB streamed, keep `id` + `area_id` only
4. `config/entity_registry/get` per entity — 842 B each, gives `area_id` / `device_id` / `device_class`
5. initial values: REST `GET /api/states/<id>` per entity, 399–843 B each — **not** `get_states`

**Then, live:** one `subscribe_trigger` naming every entity the build sheet asked for, parsed with
a filter that keeps `entity_id`, `state` and `attributes.icon` and discards `old_state`.

**Outbound (#44):** `call_service`, which `get_services` confirms carries `light.turn_on`,
`switch.toggle` and the rest. Note this is the point where the panel starts *changing* the house
rather than reading it, and it is the first thing in the project that does.

**MQTT does not go away.** The owner's constraint, recorded 2026-09-15: he intends to add sensors
and controls to these boards, and publishing entities *into* HA is an MQTT-discovery feature that
the websocket cannot replace. Websocket inbound for HA's entities, MQTT outbound for ours.

---

## 8. Still unmeasured

- **Reconnection behaviour.** What HA does when the socket drops mid-subscription, and whether
  trigger subscriptions survive. Needs a deliberate disconnect test.
- **The trigger event shape on a real change of one of OUR entities.** The 60-second window caught
  zero — those entities were simply quiet. The shape was confirmed against a busy solar sensor
  instead (`event.variables.trigger.to_state`), and it should be re-confirmed on a light.
- **`call_service` has not been exercised at all.** Doing so turns on a light in the owner's
  house, so it waits for him to be present and to say go.
- **Sensor history** (`history/stream` or the REST history endpoint) for the sparkline `cards.md`
  §4 wants. Not looked at.
