# Handoff — 2026-09-15

**Start here.** `CLAUDE.md` is the stable how-it-works; this is where we actually are, what will
bite you, and what to do next.

**A warning about this file, learned the hard way in 2.4.** When this document paraphrases a spec,
the paraphrase becomes the spec for whoever reads it first. A summary here called the two card
layouts "Measure" and "Actor" cards; the next session promoted those to class names and skipped the
domain layer that `cards.md` and the ROADMAP both actually call for. Say *which* document a summary
is compressing, and treat any vocabulary that does not appear in the source as suspect.

---

## Where the project is

**Phases 0, 1 and 2.1 through 2.4 are done and merged to `main`, tagged `v0.2.4`.**

**Milestone 2.5 is CODE-COMPLETE on `feat/2.5-page-grid-engine` and NOT YET FLASHED.** All eight
environments build. Nothing in it has been seen on glass, and two of its decisions are the kind
that have been wrong before - the compact/full threshold and the grid arithmetic both look right
and both have looked right before while being wrong on the board.

**The device now boots into a dashboard.** Until 2.5 it booted into the Phase 1 UI - a header and
two accordion panels - with the cards behind a button in the System drawer. That was correct while
the card layer was being built and wrong the moment it worked.

What it draws is still only what the device can see: four Zigbee2MQTT values off one deck sensor,
four pieces of this board's own telemetry, and two virtual switches. **That is the project's
binding constraint, and it is not a card problem.** See "Where this is going" below - the running
order changed after 2.4, and Home Assistant over the websocket now comes before the rest of
Phase 2.

### Read in this order

1. `CLAUDE.md` — the HAL/BSP, the startup split, the token rules, the traps.
2. **`docs/design/card-layout.md` — read this BEFORE moving anything on a card.** The layout
   model, and the three traps that produced every visual defect in 2.4. It exists because the
   same class of bug came back four times in four places and was each time fixed by adjusting
   a number and reflashing. That is not how the next change should go.
3. **`docs/design/dashboard.md` — the page spec, new at 2.5.** How a page is described, placed
   and degraded, the two grid knobs and what they each decide, and the unit policy.
4. **`docs/design/ha-websocket.md`** — what Home Assistant's websocket API actually gives us,
   measured against the real instance. Read before designing #43; it contains one finding that
   changes the architecture (`subscribe_trigger`, not `subscribe_events`).
5. `docs/design/cards.md` — the card spec. Read the "Implementation notes" section at the end
   first: it records where the build deviates from the body of the document, and why.
6. `docs/design/tokens.md` — the design system and the measurements behind it.
7. `docs/design/startup.md` — only if you are touching boot order or LVGL setup.
8. **`docs/REFERENCE_PROJECTS.md`** - read section "The page/view/grid back-end" before any
   navigation or build-sheet work. It was missing from this list and should not have been: the
   NINA project's frozen-id / append-only page registry is a decision we would otherwise make
   badly and only find out about after something had been persisted.
9. `docs/ROADMAP.md` §7 — the milestone list, and what 2.4 lets us close in the tracker.

`docs/research/` holds three background reports. Not on the critical path — read them when the owner
raises the topic.

---

## What 2.5 built

Full design in `docs/design/dashboard.md`. The parts that will not be obvious from the code:

**A page is DATA.** `include/Cards/PageSpec.h` declares `CardSpec` and `PageSpec`;
`include/Dashboards/Dashboard_Fleet.h` is the first one, and it is one page for the whole fleet.
`CardDemo` keeps its imperative path for the bench, but both now finish through
`CardPage::commit()` - one placement implementation, not two.

**Spans are in UNITS now, not cells.** ROADMAP Q3b's sub-grid, implemented: a page authored as
N x M cells allocates N*sub x M*sub units, `subdivision` defaults to 2, and an ordinary card is
therefore `2x2`. This is the one place 2.5 changed the meaning of a 2.4 struct - `CardPlacement`
has the same fields with a different unit, and its defaults moved from 1 to 2 to match. **If you
write a span, it is units.**

**The subdivision costs nothing dimensionally.** `sub` FR tracks plus the gaps between them
measure exactly what one cell did - the arithmetic cancels - so no card changes size when a page
subdivides. Rows lose up to `sub-1` px to integer division, which is why `commit()` tells each
card the height it ACTUALLY got rather than the one the token asked for.

**Priority is read at last.** An over-subscribed page drops the **lowest-priority** card and
re-plans - not the card that failed to place. Those differ whenever a low-priority card was
declared early, and dropping the failure would make the result depend on declaration order, which
is the thing priority exists to stop.

**A card no longer works out its own cell height.** `resolveVariant()` and `midHeight()` derived
it from `UI::grid()` and their own row span; that is wrong once a span is in units and was already
fragile, because `UI::grid()` is global state another page can move mid-build.

**Temperature is converted at FORMAT time, never on the way in.** The registry keeps what the
source said - that value is what an echo is compared against, what an optimistic write reverts to,
and what our own discovery would publish. The unit label comes from `cardDisplayUnit()` rather
than `desc.unit`, because printing the source's unit beside a converted number is a caption that
lies. Fleet default is Fahrenheit.

**The grid has two knobs and they are easy to confuse.** `TARGET_CARD_W` decides COLUMNS;
`ASPECT_PCT` decides ROWS. Neither sizes a card - both counts are stretched to fill the viewport,
so a card's ratio is an output. Nothing constrains a card to a ratio.

---

## Where 2.5 stands

**Flashed on `WS_P4_7B` 2026-09-16 and it works** - the device boots into the dashboard, cards
render, the panels animate over them, priority degradation drops the right cards. Five defects came
out of that session and all five are fixed on the branch but **not yet re-flashed**:

| Found | Cause | Fixed by |
|---|---|---|
| Every card compact; hiding the deck made it worse | row count came from geometry, so the page sized cards for rows nothing was in | rows now come from the cards - `CardPage::rowsWanted()` |
| Permanent gap between the bottom row and the deck | reserved `sc(85)+sc(20)` for a strip that is really 45 px | measured with `lv_obj_get_coords()` |
| Tapping "Cards" reset the board | bench built 13 more cards beside the dashboard's 13 - 2.4's documented layer-buffer death | dashboard torn down before the bench opens |
| "Show Touches" did nothing | overlay was a child of the screen, under the dashboard | moved to `lv_layer_top()` |
| "Seen: now" on the panel's own cards | last-seen shown for entities we own | hidden when `advertise == true` |

`ASPECT_PCT` changed meaning as part of the first fix: it is a **ceiling on card height**, not a
row-count hint. `TARGET_CARD_W` default moved to 135, which is the 6x3 layout the owner chose on
the 7B. Full reasoning in `docs/design/dashboard.md` section 6.

**To close 2.5:** re-flash the 7B and confirm the five above, then flash `WS_P4_5` and
`CYD_S3_3248` - the 3248 is the priority-degradation check, since it cannot fit 13 cards and
should drop the three `PRI_DEBUG` Panel cards first.

**Still open on the 7B, owner's own observation:** icons look undersized on large cards. Both icon
faces are chosen by pixel density alone and never by cell size. Recorded in `FUTURE_IMPROVEMENTS`
with the catch - the faces already exist per board, but every *referenced* face costs flash.

---

## What 2.4 built, and the reasoning that is not obvious from the code

**A card is a `Card` subclass named for its Home Assistant DOMAIN.** `SensorCard`,
`BinarySensorCard`, `SwitchCard`, `LightCard`, `ButtonCard`, all in `CardCatalog.h`, all reached
through `cardForKind(EntityKind)`. Underneath sit two **abstract** layouts — `ValueCard` (the hero
is a number) and `StateCard` (the hero is a state icon). They are abstract deliberately: the
compiler refuses to let anyone instantiate a layout, because picking one is the framework's job and
never the user's. A build sheet writes `type: sensor`.

**`CardBinder` is the only caller of `EntityRegistry::drainDirty()` in the application.** One
`lv_timer` at 100 ms. Values are pushed; **staleness is polled**, because an entity going stale sets
no dirty flag and nothing would wake the card.

**A command's verdict lives on the ENTITY, not on the card that sent it** (`Entity::cmdFailed`).
Cards used to track their own commands in bitmasks, which meant a parent card and a child card bound
to the same switch could disagree about whether it had failed — and which answer you got depended on
which one you had tapped. Reading the flag off the entity makes a parent's state derive from *where
its children are*, not the path they took. All resolved children failed → `FAILED`; some → `PARTIAL`.

**Three header modes, all permanent**, chosen by one build-sheet setting, with a second independent
setting for whether the area shows at all:

| Mode | Card height | Body cost | Area | STALE |
|---|---|---|---|---|
| `HDR_BAR` | X | a strip | in the band | in the band |
| `HDR_TAG` | X − tag | nothing | pill above, outside | second pill above |
| `HDR_NONE` | X | nothing | not shown at all | floating badge, top-right |

The tag hangs into the grid's **row gap**, which the page widens by `gap + tagHeight` — a sum, not
a maximum, so the space above a tag equals the space between two plain cards. Every card in tag mode
gets the shorter height whether it carries a tag or not.

**Everything about size derives from the board.** `BSP → PPI → { UI::sc(), the type scale, the icon
subset, minTouch(), the grid }`. Two of those resolve at build time because LVGL compiles fixed
bitmap fonts: `scripts/gen_type_scale.py` and `scripts/gen_icon_font.py` emit per-board headers.
Neither is an `extra_script` — generating needs `npx`, and an ordinary build must not depend on the
network. Their output is committed.

**Compact/full is derived, not declared.** A card measures whether its cell can seat a title row,
the hero and an optional row at the type scale's sizes, and draws *less* when it cannot — never the
same thing smaller. `ValueCard` drops the status corners; `StateCard` drops the name and keeps the
icon, because cards.md §4 says state *is* the icon and its colour.

**Long press pauses**, on the base class, every card type. When cards.md §4's long-press-opens-a-
sheet arrives it needs an overlay this milestone does not have; a group card will override it and
everything else keeps pausing.

---

## Where this is going

The panel renders cards. What it does not yet do is get its entities from anywhere a normal
person would call convenient, and everything on screen is still hardcoded in C++. Those two
gaps are the project, and each cuts across several milestones — which is why they are worth
thinking about before picking up a milestone number.

### Home Assistant over the websocket is the big one (#43)

**MQTT is the wrong transport for HA entities.** That is the owner's conclusion, reached
2026-09-13, and it is not about a missing field. It is that **MQTT carries a value per topic
and nothing else.** A light's area, battery, last-seen, the device it belongs to, whether it is
dimmable - each needs its own hand-crafted topic, or an automation maintaining one, per entity.
At a house's worth it is unmanageable, and all of it is work the owner does by hand before the
panel shows anything.

MQTT keeps what it is genuinely good at: our own telemetry outbound, plus plain broker topics
that have nothing to do with HA.

**Measured against the owner's real Home Assistant on 2026-09-15**, from a PC rather than from a
board - which is the cheap way to answer "what does HA actually give us" and cost about an hour:

| | |
|---|---|
| Reachable at | `http://192.168.0.70:8123`, also `homeassistant:8123`. Plain HTTP on the LAN, 6 ms |
| Token | a long-lived access token, `LOCAL_HA_ACCESS_TOKEN` in the gitignored `ConnectivityLocalSecrets.h` |
| **`/api/states`** | **742 KB across 1,662 entities** |
| One entity | 472 bytes of JSON on average, 1,328 at the largest |
| Units | 34 of 35 temperature sensors report Fahrenheit; `sensor.office_temperature` reports Celsius |

**The 742 KB is the decisive number.** No ESP32 holds that, so selective subscription is not an
optimisation, it is the only way this works - and `ENTITY_MAX` (raised to 128 at 2.5) is a ceiling
we choose rather than one we will hit. Per-entity payloads are comfortable to stream.

**And the entity_id does not tell you the room.** `switch.office_plug_3d_printer` is named
"Living Room Plug". That is the argument for the area registry made by the owner's own config, and
it is why his living room lights could not be found by name at all. Do not build anything that
parses an entity_id for meaning - he has said he intends to rename things to carry area, domain
and function, which will change the ids but not this conclusion.

**Still not designed, deliberately.** The owner's instruction stands: do not guess what the
websocket API does and does not expose, confirm it. What was confirmed above is REST; the
websocket half of the spike - the auth handshake, `subscribe_events` shape and rate, and the
area / device / entity registries - has not been run yet.

**One framework fact that bears on the cost.** `esp_http_client`, `esp-tls` and `esp_http_server`
are all in the prebuilt framework libs; **`esp_websocket_client` is not** - checked in the
`esp32p4`, `esp32p4_es` (the variant this fleet builds) and `esp32s3` include trees. So the
fetch-once half is free today and only the live half needs a library decision: an Arduino
websocket library, or roughly 300 lines of RFC 6455 we own outright. Given this project's history
with forked dependencies, the second is worth pricing seriously.

Three things already built lean on this, which is the argument for doing it sooner:

- **Area** is build-sheet-supplied today. `Card::setShowArea()`, the three header modes and
  `cardAreaColor()` were all written assuming area would arrive from somewhere. This is that
  somewhere. (`cards.md` section 2's claim that area comes free from MQTT discovery is wrong -
  `suggested_area` is outbound only.)
- **Sensor history**, for the sparkline `cards.md` section 4 wants, is fetched rather than stored.
- **Outbound commands** (#44) have exactly two virtual switches to talk to right now.

Two things worth deciding early: whether this is a **new provider beside `MqttProvider`** rather
than a change to it - the registry's whole shape says yes - and whether a long-lived
authenticated websocket plus a REST fallback is something the connectivity layer can hold without
a rethink. That second question is what could turn #43 into two milestones.

**What the owner wants on the first real dashboard** (his list, 2026-09-15), which is what the
websocket spike should go and look at specifically rather than dumping everything:

> Office lights / temp / occupancy, Kitchen lights / temp / occupancy, Living Room lights, all
> indoor temperature sensors except the AMS one, and front door, deck and garage temperatures.

Roughly 18 cards. Entities confirmed present include `light.office_left` / `_right` / `_lamp` /
`_overhead`, `binary_sensor.office_occupancy`, `binary_sensor.kitchen_occupancy`,
`light.kitchen_switch_1` ("Sink lights"), `sensor.temp_1_garage_temperature` and
`sensor.outdoor_deck_motion_temperature`. The living room lights need the area registry.

### The build sheet is what turns this from a demo into a product (#20)

Everything on a screen today is constructed in `CardDemo.cpp`. 2.4 settled what a sheet has to
be able to express, and it is more than it looked like at the start:

| A sheet entry carries | |
|---|---|
| a **domain** | `sensor`, `binary_sensor`, `switch`, `light`, `button` — never a layout |
| a header mode | `bar` / `tag` / `none` |
| area on, colour on | two independent settings, not one |
| spans and priority | `prefSpan`, `minSpan`, `priority` |
| a variant override | for when `VAR_AUTO` guesses wrong |
| staleness overrides | per data type, over `cardLongStaleMs()` |

2.5 needs a page-config struct anyway, so the two want doing in one thought rather than two.
The open question is the format — a header the build compiles, JSON on the filesystem, or
something served. Only the last two make it configurable without a rebuild, which is what the
owner has said he wants at the end of it.

### The rest of Phase 2, in the order it makes sense

| | |
|---|---|
| **2.5** (#16) | Pages and navigation. `priority`-based degradation is carried by every card and read by nothing; a page drops overflow in declaration order today |
| **2.6** | Theming at runtime — `UI::setScheme()` works, nothing exposes it |
| **2.7** (#18) | The remaining domains, and binding the existing ones to real HA entities. Half done: the types exist, their data does not |
| **2.8** (#19) | The slot system. Page header, group-card header and card header are one idea with three consumers; 2.4 hardcoded the card's two slots |
| **2.9** | Group / container cards. The owner described four distinct flavours. `Card` already binds 1..6 primaries and nothing assumes a card is a leaf, so the seam exists |

Grid tokens want a pass somewhere in 2.5: on `WS_P4_7B` the derivation produces 7 columns
where `cards.md` §7 targets 5×3, leaving every card ~13 px short of a full layout and forcing
the compact variant. Nothing is wrong with the arithmetic — `TARGET_CARD_W` is simply tuned
for the smaller panels.

### Small, self-contained things

Good when a whole milestone is too much in one sitting, and each genuinely independent:

- **#48** — reason 36 treated as real signal. Well-specified, and it costs six wasted
  re-associations per connect attempt today.
- **#42** — `AP_ACTIVE` goes stale after idle-down. Needs its own test: mode 3, junk SSID,
  ten minutes untouched.
- **#3** — `lv_conf.h` is still labelled for LVGL 9.4. Options added in 9.5 are taking
  defaults nobody has read, and the draw-buffer ones bear on everything above.
- **#47** — HA discovery will outgrow the MQTT buffer. Not urgent at eight entities; the card
  work is what makes more of them.
- **Flash `WS_P4_4B` and `WS_S3_4B` as a pair.** 720×720 at 1.5× is the same effective UI
  space as 480×480 at 1.0×, so the two together are a free correctness check on the whole
  scaling scheme. Six boards have not been flashed at all since the type scale changed.

### The one number to keep an eye on

**The LVGL task stack.** The System Doctor reports its high-water mark beside free heap. Every
crash in 2.4 was that stack, and it was misread as a memory problem three times before anyone
measured it; `SET_LOOP_TASK_STACK_SIZE(16 * 1024)` in `main.cpp` is the fix, and the margin
today is 7556 bytes free of 16384 on `CYD_S3_3248`. If that trends toward zero as cards gain
nesting, the answer is a **flatter widget tree**, not a bigger stack —
`docs/design/card-layout.md` §1.3 explains why the draw walk costs what it does.

---

## The scaffolding that dies, and when

| What | Dies with | Why it exists |
|---|---|---|
| `CardDemo.*` | #20, the build sheet | The build sheet's stand-in. Also the comparison bench — six buttons crossing every visual choice |
| `VirtualEntities/Provider` | #44, outbound commands | There is not one writable entity in the fleet, so the optimistic-write path had never executed. Two switches: one echoes, one is deliberately ignored |
| `debugForceState()` | never, probably | Pins a card to a state. Nothing on a running panel goes stale inside a test session, so without it five treatments are untestable |

---

## The decisions 2.4 was built against (kept for reference)

From `docs/design/cards.md`, which remains the spec:

- A card binds **one primary entity plus at most two secondaries**, where a secondary is another
  entity *of the same physical device* (battery, last-seen). Anything wanting more is a **group
  card** — a separate type holding several primaries, spanning multiple cells.
- **Card types are named for their Home Assistant DOMAIN** — `sensor`, `binary_sensor`, `switch`,
  `light`, `button` — exactly as `cards.md` §4 and ROADMAP 2.7 list them. A user picks a domain and
  supplies topics; the framework decides the arrangement. **They never pick a layout.**
- **Two layouts sit underneath, and they are an implementation detail.** `ValueCard` (hero is a
  number: small tinted icon top-left, name beside it, value centred and dominant — and the name is
  the **location**, not the measurement) and `StateCard` (hero is a state: big icon in a disc,
  centred, name below). Both are abstract; only domain types in `CardCatalog.h` are instantiable.
  **No "On"/"Off"/"Open"/"Closed" text anywhere** — state is the icon and its colour.

  *An earlier version of this file called these two "Measure" and "Actor" cards, as though they
  were the types rather than the layouts. That paraphrase got promoted to class names in 2.4 and
  the domain layer underneath was skipped entirely, which would have forced a build sheet to record
  a layout decision that is not a user's to make. Fixed the same milestone. Worth remembering that
  a summary written for the next session can outrank the spec if it is the thing that gets read
  first — which is exactly what this document is.*
- **Provenance never renders on a card.** Not `Zigbee2MQTT`, not the source. It is an Entity
  Registry violation as well as visual noise; it belongs in `SystemReport`.
- **Staleness never dims.** Tag, escalating to a fat corner-to-corner diagonal. A *paused* card may
  dim, because that is the user's choice rather than a failure — they must look different.
- **A failed command is its own state** and needs no new plumbing: `EntityRegistry` already
  implements optimistic writes that revert when the echo never arrives. That revert *is* the event;
  nothing renders it yet.

**Sequencing note:** generate the MDI icon subset **last**, once the card types have settled which
glyphs they need. Regenerating means regenerating every board's font blob, and each icon size costs
~96 KB of flash.

---

## Things that will bite you

**Build from PowerShell, not Git Bash.** pioarduino rejects MSYS shells. (In practice Bash has
worked, but the docs say otherwise and the failure mode is confusing — don't risk it.)

**`pio run` with no `-e` builds ONE environment**, because `default_envs` names a single board. The
fleet is eight explicit `-e` flags. This has caught us twice.

**Clear `.pio/build_cache` after editing any BSP header.** The content-addressed cache does not
track the macro-indirected `#include BSP_HEADER`, so a BSP value change can be silently ignored
forever. `pio run -t clean` does *not* clear it.

**Filename case matters, and this repo has been bitten twice.** `UIToolkit.h` and
`BSP_WS_S3_TOUCH_LCD_5B.h` were both tracked under a case that only worked on Windows. A
case-sensitive filesystem would fail one environment while the other seven built clean. When
renaming on Windows, `git mv` needs two steps.

**Escape sequences get mangled if you write C strings through a Python heredoc.** `\n` inside a
patch script becomes a real newline and produces "missing terminating `"` character". Build the
backslash explicitly (`chr(92)`) or use a line-based edit. This cost three rebuild cycles.

**Anything drawn on a panel stays ASCII, except `°`.** LVGL's stock Montserrat has no `·` or `—`;
they render as tofu boxes.

**Verify from outside the device.** The oldest rule here and it keeps paying: a serial line saying
`softAP() succeeded` is not evidence anyone can join the AP. Check the router's lease table, join
from a phone, look at Home Assistant.

---

## What is measured vs. what is assumed

The project has been burned by confident claims that were never tested, so this distinction is
tracked deliberately. **Say "verified at `<path>:<line>`" or say "I believe".**

Measured on hardware:

| | Where |
|---|---|
| Sample tile cost | ~715 B in `lv_mem` on `CYD_S3_3248` | 2.3 |
| **Real card cost** | **~2.8 KB** in `lv_mem` on `WS_P4_5` — ten `lv_obj`s against a sample tile's three | 2.4 |
| Page peak | ~75 KB of a 128 KB pool with two pages alive during a rebuild, on `WS_P4_5` | 2.4 |
| LVGL pool | 128 KB static array in **internal DRAM** | 2.3 |
| One full-ASCII font face | **~96 KB of flash** | 2.2 |
| **Icon subset** | 84 glyphs at two sizes cost **less than the one Montserrat_48 they replaced** — the 3248 went *down* 4,680 bytes | 2.4 |
| Fleet density | 165–294 PPI; scale derived as `PPI / 170` | 2.2 |
| Fleet flash | 24.4–25.9% across all eight environments | 2.4 |
| **HA `/api/states`** | **742 KB, 1,662 entities**, 472 B average per entity | 2.5, against the real HA |
| HA reachability | plain HTTP on the LAN, 6 ms to `/api/` | 2.5 |
| Full-card threshold, 7B | **129 px** row height (bar/tag), 109 px (none) | 2.5, derived from the type scale |
| Fleet build | all 8 environments build on `feat/2.5-page-grid-engine` | 2.5 |

Verified against `docs/TEST_2.4.md` at sign-off:

- **Everything visual in 2.4**, on three boards. `WS_P4_5` clean, `CYD_S3_3248` clean after the
  stack fix, `WS_P4_7B` correct but compact — a grid-token matter, not a defect.
- **The 3248 freeze.** It was the loop task's stack, not `lv_mem`. Confirmed on the board.

Still assumed:

- **NOTHING IN 2.5 HAS BEEN FLASHED.** The page engine, the boot screen, the unit conversion, the
  knobs and the deck reserve are all code-complete and unseen.
- **Six boards have not been flashed at all** since the type scale changed. They compile.
- **`TouchManager::mapCoordinates()`'s `WS_P4_7B` special case.** Undocumented, never re-tested
  against the alternative. `CLAUDE.md` flags it and it is still true.

---

## Open loose ends

| What | Issue | Note |
|---|---|---|
| Reason 36 treated as real signal | **#48** | Self-inflicted `STA_LEAVING` causes six wasted re-associations per attempt, and the report names the wrong cause. Small, well-specified fix |
| AP idle-down leaves `AP_ACTIVE` stale | **#42** | Needs its own test: mode 3, junk SSID, ten minutes untouched. The #45 test did **not** cover it |
| HA discovery will outgrow the MQTT buffer | **#47** | Not urgent at 8 entities; will bite as cards add more |
| Outbound commands | **#44** | Explicitly **not** a 2.4 requisite (owner's call). The path is exercised end to end by two virtual switches; what is missing is publishing to a real broker |
| **HA over websocket** | **#43** | **Promoted.** Not "an alternative way to reach HA" any more — MQTT carries a value per topic and nothing else, so area, battery, last-seen and grouping each need a hand-crafted topic per entity. Unmanageable at scale. See `FUTURE_IMPROVEMENTS.md` |
| `cards.md` §2 claims area comes free from MQTT discovery | — | **It does not.** Verified: nothing in the tree publishes or reads an area, and `suggested_area` is outbound only. Strike that sentence when cards.md is next edited |
| `lv_conf.h` vs LVGL 9.5 template | **#3** | Still labelled for 9.4; options added in 9.5 are taking defaults unreviewed |

Also carried, not filed: **`WS_P4_4B` and `WS_S3_4B` should be flashed as a pair** — 720×720 at
1.5× is the same effective UI space as 480×480 at 1.0×, so they are a free correctness check on
the whole scaling scheme.

---

## How to work with this owner

He is a hobbyist and an ESP32 enthusiast, not a professional developer, and he is explicit about
that — but he reads code, spots real bugs, and has caught several this session that were not
obvious. Treat his instincts as data.

**What works:**

- **Show, don't spec.** He says he knows what he likes when he sees it and finds originating visual
  design a slog. Build something he can react to. The browser bench (`card-bench.html`, published
  as an Artifact) drove every design decision in Phase 2.2 — but see the warning below.
- **Give a recommendation, not a menu.** When he asks "which should we do", he wants an opinion
  with reasoning, and he will push back when he disagrees.
- **He will answer an open question with a better question.** Asked "one entity per card or N", he
  replied with the *rule* — a secondary is a sibling on the same physical device — which was more
  useful than the number.
- **Own mistakes plainly and move on.** Several claims this session turned out wrong (the framebuffer
  "index 1" reading, the `lv_conf` font claim, the default scheme, the grid target). Correcting them
  in the commit message and the docs is the expected behaviour, not a big deal.
- **He flashes hardware fast.** If something can be settled by a flash, ask — he will usually have
  the answer within minutes, with full serial output.

**What to avoid:**

- Don't say "we should wait until phase X" as a reflex. When he asked for research into a full
  ESP-IDF port he pre-empted exactly that, and he was right to.
- Don't build a browser mock and trust it. **The bench modelled `WS_P4_5` at 1.5× when the board is
  really 1.73×**, so every value tuned in it was 15% off. It has been corrected, but the lesson
  stands: the bench narrows the options, the glass decides.
- Don't commit straight to `main`. Feature branch, then merge. (This was reinforced after some doc
  commits went direct.)

**Versioning:** `A.B.C.D` where **C is the roadmap phase** — finishing 2.2 tags `v0.2.2`. `D`
auto-increments from `git describe`. Tag on `main` at merge, never during development. A dirty tree
appends `+dirty`, which is working correctly and is useful.

---

## Suggested first move

**Flash `WS_P4_TOUCH_LCD_7B` and turn the grid knobs** - see above. That is 2.5's sign-off and it
needs nothing else.

**Then the websocket half of the #43 spike**, which is unblocked: the address, the token and the
REST findings are all recorded above. Run it from a PC first the way the REST half was run - it
answers "what does HA actually give us" with no firmware at all, and it is what the design should
be built against rather than guessed at.

Build for `WS_P4_TOUCH_LCD_5` and `CYD_S3_3248W535` on every change; they bracket the fleet. All
eight build as of this branch. **Do not run two `pio` invocations at once** - they contend for
`.pio/build` and fail with a directory-lock error that looks nothing like a compile error.

### Deliberately postponed — do not rediscover these

| What | Where it goes | Note |
|---|---|---|
| Corner icon on EVERY card, by domain | card library | The corner icon is the card's DOMAIN (a light always wears the same mark); the HERO is the specific thing (bulb vs ceiling vs strip). Moves a group card's mixed indicator to the top-right. Costs nothing — the corner is already out of the flow |
| One slot mechanism, three consumers | **2.8 (#19)** | `cards.md` §8. Page header, group-card header, card header are one idea. 2.4 hardcoded the card's two slots; 2.8 should generalise rather than build a third thing |
| `priority`-based degradation | **2.5 (#16)** | Carried, reported in the Doctor, read by nothing. A page drops cards that do not fit in declaration order today |
| Group / container cards | own milestone | The owner described four distinct flavours. `Card` binds 1..6 primaries and nothing assumes a card is a leaf, so the seam is there |
| Outbound commands | **#44** | Not a 2.4 requisite (owner's call). Two virtual switches exercise the path |
| HA over websocket | **#43**, promoted | MQTT carries a value per topic and nothing else. Every metadata field needs a hand-crafted topic per entity — unmanageable at scale |
| `UI::sc()` duplicates `lv_dpx()` | logged in FUTURE_IMPROVEMENTS | Same arithmetic, different reference constant. 1.0625x, invisible, not worth a re-tune yet |
| `ST_WARN` amber on the Paper scheme | owner's call | Reads poorly on white. Changing it contradicts "state colour is content, not decoration" |
| Sibling secondaries from one device | **#20** | All three deck cards come from one sensor and could share a battery. Which siblings a card inherits is build-sheet territory |

### Two decisions still open, both awaiting glass

Neither blocks sign-off; both are defaults the build sheet will carry.

1. **Which header mode is the default** — all three ship, so this is "what a card looks like when
   nobody chose", not an elimination.
2. **`Fill` vs `Icon`** for an active state card.

And one detail the owner flagged as unresolved: in `HDR_NONE` the STALE marker is a **floating**
rounded badge inset from the top-right corner. The alternative is flush against the card's edge.
Currently floating.
