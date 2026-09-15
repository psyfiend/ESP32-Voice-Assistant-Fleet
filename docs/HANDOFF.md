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

**Phases 0, 1 and 2.1–2.3 are done and merged to `main`, tagged `v0.2.2`.**

**Milestone 2.4 is DONE, hardware-verified, merged and tagged `v0.2.4`** — thirty commits, signed
off on `WS_P4_5`, `CYD_S3_3248` and `WS_P4_7B` against `docs/TEST_2.4.md`. All eight environments
build. Phase 2 is half shipped: 2.1 through 2.4 done, 2.5 through 2.9 open.

The device boots, joins WiFi, talks to a broker, appears in Home Assistant, publishes its telemetry
and reads other devices' entities — all through one Entity Registry that neither side knows the
shape of. And it now renders those entities as cards.

### Read in this order

1. `CLAUDE.md` — the HAL/BSP, the startup split, the token rules, the traps.
2. **`docs/design/card-layout.md` — read this BEFORE moving anything on a card.** The layout
   model, and the three traps that produced every visual defect in 2.4. It exists because the
   same class of bug came back four times in four places and was each time fixed by adjusting
   a number and reflashing. That is not how the next change should go.
3. `docs/design/cards.md` — the card spec. Read the "Implementation notes" section at the end
   first: it records where the build deviates from the body of the document, and why.
4. `docs/design/tokens.md` — the design system and the measurements behind it.
5. `docs/design/startup.md` — only if you are touching boot order or LVGL setup.
6. `docs/ROADMAP.md` §7 — the milestone list, and what 2.4 lets us close in the tracker.

`docs/research/` holds three background reports. Not on the critical path — read them when the owner
raises the topic.

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
and nothing else.** A light at `home/office/light/state` tells you the light is on. Its area,
its battery, its last-seen, the device it belongs to, whether it is dimmable — each needs its
own hand-crafted topic, or an automation maintaining one, per entity. At eight entities that
is tedious. At a house's worth it is unmanageable, and all of it is work the owner has to do
by hand before the panel shows anything. A JSON payload helps at the margins and does not fix
it: somebody still has to decide and configure what goes in it.

HA's own websocket API already knows all of it, because it is the same data the HA frontend
draws. So #43 stops being "an alternative way to reach HA" and becomes **how HA entities are
going to work**. MQTT keeps what it is genuinely good at: our own telemetry outbound, plus
plain broker topics that have nothing to do with HA.

**Nothing about that API is assumed here, deliberately.** The owner's instruction on exactly
this point was not to guess at what does or does not come from HA — confirm it against the
real thing before designing to it. What is settled is the direction, not the schema.

Three things already built lean on it, which is the argument for doing it sooner:

- **Area** is build-sheet-supplied today. `Card::setShowArea()`, the three header modes and
  `cardAreaColor()` were all written assuming area would arrive from somewhere eventually.
  This is that somewhere. (`cards.md` §2's claim that area comes free from MQTT discovery is
  wrong — `suggested_area` is outbound only. Corrected in §11.)
- **Sensor history**, for the sparkline `cards.md` §4 wants, is fetched rather than stored.
  Same client, same session.
- **Outbound commands** (#44) have exactly two virtual switches to talk to right now.

Two things worth deciding early: whether this is a **new provider beside `MqttProvider`**
rather than a change to it — the Entity Registry's whole shape says yes, since a card must
never learn where its value came from — and whether a long-lived authenticated websocket
plus a REST fallback is something the connectivity layer can hold without a rethink. That
second question is the one that could turn #43 from a milestone into two.

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

Verified against `docs/TEST_2.4.md` at sign-off:

- **Everything visual in 2.4**, on three boards. `WS_P4_5` clean, `CYD_S3_3248` clean after the
  stack fix, `WS_P4_7B` correct but compact — a grid-token matter, not a defect.
- **The 3248 freeze.** It was the loop task's stack, not `lv_mem`. Confirmed on the board.

Still assumed:

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

**Ask which of the two above he wants first**, because they are a genuine fork and he has not
picked. #43 makes the panel worth owning; #20 makes it configurable without a rebuild. #20 is
smaller and unblocks 2.5 — but doing it first risks designing a format around the fields MQTT
happens to carry, which is the mistake #43 exists to undo.

Whichever it is, **read `docs/design/card-layout.md` before moving anything on a card**, and do
not tune a number and reflash to find out. That loop is what §1 of that document exists to end.

Build for `WS_P4_TOUCH_LCD_5` and `CYD_S3_3248W535` on every change. They bracket the fleet: the
densest panel and the tightest memory. **Do not run two `pio` invocations at once** — they contend
for `.pio/build` and fail with a directory-lock error that looks nothing like a compile error.

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
