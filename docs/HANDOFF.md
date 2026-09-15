# Handoff — 2026-09-13

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

**Milestone 2.4 is DONE and hardware-verified** on `WS_P4_5`, `CYD_S3_3248` and `WS_P4_7B`, on
branch `feat/2.4-card-base`. All eight environments build. Ready to merge and tag `v0.2.4`.

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

## What is immediately next

**Flash both dev targets and work `docs/TEST_2.4.md`.** The 3248 first — it is the veto board and
the one that froze. Section A of that document is the gate; nothing else matters if A1 fails.

After sign-off: merge `--no-ff`, tag `v0.2.4`, close #15. `ROADMAP.md` §7 has the table of which
other issues 2.4 does and does not let us close — **#18 (2.7) is half done and must stay open**,
because the card types exist but are not yet bound to real HA entities.

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

Still assumed, and the reason `docs/TEST_2.4.md` exists:

- **Everything visual in 2.4.** The branch has been flashed twice, both times before the header
  rework, the icon subset, the domain types and the compact variants landed.
- **The 3248 freeze fix.** Diagnosed precisely and never confirmed on the board.
- **Six boards have not been flashed at all** since the type scale changed. They compile.

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

**Work `docs/TEST_2.4.md`.** Flash `CYD_S3_3248W535` first — it is the veto board and the one that
froze. Section A is the gate; if A1 fails, send serial and stop, because nothing below it matters.

Do not write more card code before that. Every acceptance criterion in #15 has an implementation;
what is unknown is whether it looks right, and two flashes' worth of visual feedback has already
changed the design twice.

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
