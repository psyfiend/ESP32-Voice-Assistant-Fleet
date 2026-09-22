# Card design — decisions and open questions

**Status: decisions captured and open questions answered 2026-09-10, not yet implemented.** Owner's design direction from the
Phase 2.2 bench sessions, written down so 2.4 (`Card` base class) and 3.1 (build-sheet schema)
build against it instead of relitigating it.

Companion to `docs/design/startup.md`. Related: ROADMAP §4.1 (Entity Registry), §4.3 (what happens
to the existing panels), issues #13 (design system), #15 (Card base class), #20 (build sheet),
#44 (outbound commands).

---

## 0. The finding that outranks the rest

Shown five layout variants, the owner's unprompted reaction to the one with several semantic
colours on screen at once was that it *"instantly makes it look more attractive and inviting."*

That reaction was not about the feature being demonstrated. It was about **colour carrying meaning
across a whole page** rather than a single accent hue repeated on every card. It is the most
useful signal produced by the bench so far, and it reorders the priorities below: state colour is
not a finishing touch to apply after the card library works, it is the thing the card library
exists to deliver.

Concretely, that means the design system needs a **semantic state palette** — active / idle /
open / closed / alert / stale — as a first-class token set, separate from the single accent, and
every card type must declare which of its states map onto it.

---

## 1. What a card is allowed to contain

**Hard cap: one primary value plus at most two secondaries.** Agreed both directions. Beyond that
the card becomes unreadable at 3248 sizes, and every candidate for a third secondary turned out
to belong somewhere else.

**What makes something a legitimate secondary — the rule, not the list.** A secondary is another
entity *of the same physical device* as the primary. That is why battery works: a Zigbee
temperature sensor publishes temperature, humidity, pressure and battery, and a Philips motion
sensor publishes occupancy, temperature, lux and battery. They arrive together because they *are*
one device.

In practice this collapses to a very short list: **battery, and last-seen** — where last-seen
always refers to the primary entity, never to a sibling. Anything else that wants to be on the
card is really asking to be a group card (see §4, "Group / room card").

| Slot | Content | Notes |
|---|---|---|
| Primary | The value, or the state | Prominent. Size set by the type scale, not per card |
| Icon | State-reflecting where possible | See §5 |
| Label | What this *is* | See §4 — often the location, not the measurement |
| Secondary 1 | Contextual, minimal | `2h`, `60%`, `▾0.4°` — never a sentence |
| Status row | Battery and/or last-seen | See §3 |
| Card header | Area and/or STALE | Optional, per card. See §2, §6 |

**Provenance never appears on a card.** The bench's verbose mode printed `Zigbee2MQTT · 40s ago`
and it was immediately rejected as noise. It is also an architectural violation: ROADMAP §4.1 says
a card must never branch on `source`, and rendering `source` is that same coupling arriving
through the front door. **Provenance is diagnostic** — it belongs in `SystemReport`, not on glass.

### The status row

- No divider rule above it. It reads as part of the card, not a separate compartment.
- It only ever carries **battery** and/or **last seen**. If the entity has neither, the row is
  simply absent — not an empty reserved strip.
- For a dimmable light, brightness is *not* status-row material: name and percentage are the
  primary content.

---

## 2. Area, and the card header bar

**A card may have its own header bar**, and where it does, the layout is fixed: **area on the
left, STALE on the right.** This is the same slot the escalating staleness band uses (§3), and the
same idea as the page header's configurable slot list (2.8) — see §8.

**Two treatments to prototype, not one.** Both go on the 2.2 reference page so they can be
compared on glass:

| | Internal header bar | External tag |
|---|---|---|
| Where | A band inside the card's top edge | A small tag bolted to the top edge, outside the border |
| Area | Left | Left |
| STALE | Right | Right |
| Fill | Solid accent | Solid accent |
| Text | The card's **background** colour — so dark text on Paper Coral, light text on Slate | Same |

Colour-coded either way. The original objection stands and shaped this: area *inside* the card
body, alongside label + value + icon + secondary + status, is what tips a card from dense into
busy. A header bar is not the card body — it is a separate register, which is why it can carry
what the body cannot.

Area comes free: Home Assistant already carries it on devices, so `MqttProvider` can populate it
from discovery rather than anyone tagging entities by hand.

**Deferred: visually grouping several cards by area** (a dim border around a cluster, a shared
colour band). This is a *page engine* feature, not a card feature — it needs group containers in
the grid, which changes 2.5's layout model. Take the tag at 2.4; revisit grouping at 2.5 or in the
build sheet.

---

## 3. Staleness — dimming is rejected

**Dim is out.** A dimmed card is easy to miss, which is the opposite of the requirement: stale
data must be conspicuous, because a dashboard that quietly lies is worse than one that is blank.

Agreed treatment, escalating with age:

1. **Fresh** — nothing.
2. **Stale** — a bright tag in the header bar (or the external tag), on the right.
3. **Long stale** — unmistakable. Two candidates, both to be prototyped: the STALE tag **grows**
   to occupy more of the header, or a **single diagonal accent line** is drawn corner to corner
   across the card in a warning colour. The card stays readable; it just cannot be mistaken for
   live.

**The second threshold is its own field, not a multiple.** Defaults live per *data type* in a
library header — a switch or light should go stale in seconds, while temperature or occupancy can
reasonably be minutes or hours — and any default is overridable from the build sheet, exactly the
overlay model #20 describes.

### A failed command is not staleness, and it must not wait for a timeout

Tap a light card, and if the light does not turn on, the card jumps **straight to the loud state**.
No gradual escalation: the user just did something and it did not happen, so the feedback is
immediate.

**This needs no new plumbing.** `EntityRegistry` already implements optimistic writes with a
revert when the echo never arrives (`EntityRegistry.h:95` and `:116`). That revert *is* the
"command didn't take" event — it exists, it is already timed, and nothing currently renders it.
The card layer only has to give it a face.

Worth naming this as its own state rather than folding it into stale: **stale means "I have not
heard from this", the reverted write means "I told it to do something and it refused".** They
deserve different words on screen.

**Separate from staleness: a per-card "pause / ignore this entity" setting.** A deliberately
paused card *may* dim, or be removed from the page entirely — because that state is the user's own
choice rather than a failure, so quiet is correct. Worth being explicit that these two look
different on purpose.

---

## 4. Card types, and what each one actually shows

### Sensor (temperature and friends)
Value dominant. **The label is the location, not the measurement** — "Deck", not "Temperature".
The icon already says what kind of quantity it is. This removes a whole line from every sensor
card in the fleet.

Wanted: **history**, as an inline sparkline or bar strip, with a tap opening a detailed view.

**Decided: fetch it, do not store it.** Rather than a per-entity ring buffer in RAM — which on
`CYD_S3_3248` competes with the LVGL draw buffers in internal SRAM — pull the series from Home
Assistant on demand, via its history REST endpoint, and keep it only for as long as the chart is
on screen. That turns a permanent per-entity memory cost into a transient one paid only by the
card the user is actually looking at.

This makes the HTTP path in ROADMAP layer 1 (`HaProvider`) real work rather than an alternative,
and it overlaps #43 (HA access without MQTT) — the same client serves both.

Two honest caveats, neither blocking:
- **Response size.** A day of temperature at native resolution is on the order of a thousand
  points. HA's `minimal_response` and significant-change filtering exist for exactly this, and we
  should ask for a downsampled series rather than parse a large body on a board with ~22 KB of
  free internal heap.
- **It only works for entities HA knows about.** A purely local I2C sensor has no HA history to
  fetch, so those either get no chart or get a small local buffer as a special case. Worth
  deciding once rather than per card.

### Binary sensor / occupancy
**Motion does not deserve a card.** It is one bit that matters per *area*, so the default
treatment is a small badge shown once per area, not a tile per sensor.

If someone does place motion as its own card, it behaves as a **state card**: prominent icon
(person with movement waves vs. person struck through) and the whole card shifts to its active
colour, exactly like a non-dimmable light.

### Light
- The card reflects state **across its whole surface**, not in a corner. On → the card takes the
  active colour.
- Where the light reports colour or colour temperature, the card mirrors it. Where it reports
  brightness, brightness is shown prominently.
- **Groupable by room.** One tap toggles every light in the group. Long-press or double-tap opens
  a sheet in which each light appears as its own standalone card.
- **Mixed state gets its own indicator** — an icon or badge saying the group is not uniform. The
  card does not have to pick a side and lie about it.

### Action / scene
New type, not previously in the roadmap's list. Cards that fire an automation, scene or template —
"all living room lights", "bedroom 50%", "bedroom 80%". Over MQTT these are outbound commands,
which makes this the **first real consumer of #44**, currently deferred as "nothing has a control
to send one yet." That is no longer true.

### Weather station
A card with a **minimum size** — 2×2 or larger — rather than a page of its own. It shows several
published temperatures and/or forecast data in that footprint.

**Plus an optional full-page version**, reachable either by swiping to it like any other page or
by opening it from the small card (long-press, double-tap, or the context sheet). So the same
content has two presentations, and only the large one is a page. That answers the "is it a card or
a page" question with *both*, and it means the grid engine needs minimum-span support but not a
second layout system.

### Group / room card — a distinct type

The one genuinely new card type, and the one that most affects 2.4's design.

A group card holds **several primaries**, not a primary plus secondaries. "Living Room" might
carry occupancy, temperature and two lights. It occupies more than one grid cell, and inside its
border the member entities appear as icons or symbols that are **individually interactive**, each
behaving like a small card of its own type.

Its header — internal bar or bolted-on external tag, the same two treatments as §2 — carries the
title and can also hold values that would crowd the body. Occupancy and temperature belong there;
the lights stay in the body where they can be tapped.

**"Secondary" means something different inside a group card**, and that is fine as long as it is
deliberate: on a normal card a secondary is a sibling entity of the same device (§1); inside a
group card, a header value is one of the group's own primaries, promoted for space. Two different
mechanisms, two different names needed.

---

## 5. Icons

**Prefer provider-supplied.** We already publish an `icon` in our own MQTT discovery payloads, so
the inbound direction should be symmetrical: if HA or an MQTT discovery message names an icon, use
it.

**Fallback: a local set whose glyph varies with the reading** — bulb off / half / on, thermometer
by band, occupancy present / absent. Cheap to reason about, but each variant is another glyph in
the LVGL font subset and therefore flash. **Open: how many state variants can we afford?** This is
a #14 question and it should be answered before the MDI subset is generated, because regenerating
the subset later means regenerating every board's font blob.

---

## 6. Chrome that was rejected, and what replaced it

**The accent rail as decoration is dead.** The owner's verdict: *"too funky"*, and it actively
conflicts with using colour to signal active state — which §0 says is the more valuable use of
colour. It survives only in the form where it *is* the state indicator, and then it needs to be
**≥10px** to read at all.

**A per-card header bar is in scope instead.** Optional per card, carrying the area tag and/or the
STALE band. This is the same slot §3's escalating staleness header uses, which is a good sign: two
independent requirements landed on the same structure.

---

## 7. Grid density per board

The owner's targets, plus what a shipping project actually does. `espcontrol` is the useful
comparison because it covers one of our exact boards.

| Board | Resolution (landscape) | Target | Evidence |
|---|---|---|---|
| `CYD_S3_3248` | 480×320 | **2×3** portrait, 3×2 landscape | Owner's call. The veto board |
| `WS_S3_4B` | 480×480 | **3×3** | `espcontrol` ships 3×3 on two different 480×480 boards |
| `WS_P4_4B` | 720×720 @ **1.5× DPI** | **3×3** | Same *effective* UI space as `WS_S3_4B` — see below |
| `CYD_S3_8048` | 800×480 | **4×2** | Owner's call |
| `WS_P4_5` | 1280×720 | **5×3**, possibly 5×4 | `espcontrol` runs 5×4 on jc8012p4a1 at 1280×800 |
| `WS_S3_5B` | 1024×600 | 5×3 on pixels, **but see note** | Tearing and slow refresh may cap it below what the pixels allow |
| `CYD_P4_1060` | 1024×600 | **5×3** | `espcontrol` ships exactly this for jc1060p470 — the same panel |
| `WS_P4_7B` | 1024×600 | **5×3 minimum**, 6×4 plausible | Owner's estimate |

**The two 4-inch boards are the same layout problem in different pixels**, and that is a useful
result rather than a coincidence. `WS_P4_4B` is 720×720 with `HIGH_DPI_DISPLAY` (1.5×), so its
effective UI space is 480×480 — exactly `WS_S3_4B`'s native size. Same grid, same token values,
same card count; one just draws them 1.5× larger. If the DPI-scaling approach is right, these two
boards should be visually indistinguishable apart from sharpness. That makes them a free
correctness check on the whole scheme, and worth flashing as a pair.

Three things this table settles:

- **Column count is derived, not configured.** The bench proved the model: pick a target card
  width, and the page fits as many whole cards as the screen allows, with a fixed gap and a fixed
  cluster inset. Same tokens, 6 columns on the P4 and 2 on the 3248, no per-board layout code.
- **`WS_S3_5B` is a performance cap, not a pixel cap.** It has the resolution for 5×3 and the
  refresh characteristics of something much smaller. Worth measuring rather than assuming — its
  known tearing is already recorded in `FUTURE_IMPROVEMENTS.md` under the RGB `num_fbs`
  investigation.

The owner's three signed-off token sets from the bench are the starting points for 2.2:

```
5×3 landscape, high-DPI      target 145px · gap 12 · inset 14 · 0.85:1 · radius 10 · pad 16
                             border 1px lighten @18% · no shadow · no rail
                             Signal blue on Slate/Raised · montserrat_40 / _16 · icon 34
                             no secondary · status row on · no area tag · stale badge

2×3 portrait, CYD_S3_3248    target 120px · gap 12 · inset 14 · 0.95:1 · radius 10 · pad 10
                             same colours · montserrat_32 / _14 · icon 34

4×3 landscape "Paper coral"  target 190px · gap 12 · inset 14 · 0.65:1 · radius 12 · pad 16
                             no border · shadow 6 · Coral on Paper/white
                             montserrat_40 / _16 · icon 28 · secondary min · stale badge
```

---

## 8. One slot system, used in three places

Noticed while answering the group-card question, and worth building once rather than three times.

The page header (2.8) is specified as a **configurable slot list** — clock, WiFi and MQTT glyphs,
optional sensor slots. A group card's header wants exactly the same thing: named slots, filled
from a config, rendered left-to-right. And a normal card's header bar (§2) is the degenerate case
of the same idea with two fixed slots, area and STALE.

So: **one slot mechanism, three consumers.** Page header, group-card header, card header. If 2.8
builds it as a general thing rather than as the page header specifically, the group card gets its
header free, and the build sheet gets one grammar to describe all three.

---

## 9. Open questions — answered 2026-09-10

| # | Question | Answer |
|---|---|---|
| 1 | Second staleness threshold — own field, or a multiple? | **Own field.** Defaults per data type in a library header; overridable from the build sheet |
| 2 | How many icon state-variants can flash afford? | **ANSWERED 2026-09-10 by measurement.** ~96 KB per referenced font face — see below |
| 3 | Per-entity history: how many samples, in which RAM? | **Dissolved.** Fetch from HA on demand, store nothing (§4.1) |
| 4 | Group light card: aggregate, count, or both? | Tap toggles all; **mixed state gets its own indicator**; long-press opens per-light cards |
| 5 | Weather station: card or page? | **Both.** A card with a 2×2 minimum, plus an optional full-page view opened from it |
| 6 | One entity per card, or primary + N? | **Primary + up to 2 siblings of the same device** (§1). A card that wants more is a group card |

**Question 2, answered.** Measured on `WS_P4_5` during milestone 2.2: **one referenced font face
costs ~96 KB of flash.** Enabling a size in `lv_conf.h` is free — the linker drops unreferenced
font objects — but *referencing* one is expensive.

What that means for icons:

- **The MDI subset must be a genuine subset**, at **one size**, not a family. A second icon size
  is another ~96 KB.
- **State variants are affordable; extra sizes are not.** Glyph count scales the bitmap data
  roughly linearly, and an icon set of 40-60 glyphs at card size is far smaller than a 95-glyph
  full-ASCII face at 40 px. Ten bulb/door/motion variants cost far less than one extra size.
- **Budget it against the type scale, not separately.** They come out of the same flash. The fleet
  currently references nine Montserrat faces; every one is ~96 KB, and trimming that list is the
  cheapest way to pay for icons.

So the guidance for 2.4: **one icon size, generous glyph count.** Generate the subset only once
the card types have settled which glyphs they need, because regenerating means regenerating every
board's font blob.

---

## 10. Reference material — read, do not copy

Two projects added 2026-09-10. **Check `docs/REFERENCE_PROJECTS.md` before reusing anything from
either**; one of them is not permissively licensed.

- **`esphome-modular-lvgl-buttons`** — **MIT**, reusable with attribution. Its `ui/<type>/` split
  into `local.yaml` / `remote.yaml` / `detail.yaml` per entity type is the same shape as our card
  types plus context sheets, arrived at independently.
- **`espcontrol`** — **PolyForm Noncommercial 1.0.0.** Architecture and layout decisions may be
  studied; **code may not be copied**, and the licence bars commercial use of the software itself.
  Its per-device grid definitions are the evidence in §7.

---

## 11. Implementation notes — milestone 2.4, 2026-09-13

**This section records where the build and the body of this document disagree, and why.** The
document above is the spec and was written before anything existed; these are the places real glass
sent it back. Nothing here is a silent override — each item was raised and decided.

### The two "layout families" were never card types

§4 lists card types by Home Assistant domain, and ROADMAP 2.7 names the first four the same way.
A summary in `HANDOFF.md` called the two layouts "Measure" and "Actor" cards, and that paraphrase
got promoted to the class names — skipping the domain layer entirely and leaving a build sheet
having to record which *layout* a temperature reading wants.

Corrected. `ValueCard` and `StateCard` are **abstract** layouts; concrete types are named for their
domain in `CardCatalog.h` and reached through `cardForKind()`. A user picks `sensor`; the framework
picks the arrangement. The split immediately exposed a latent bug: tap handling had been on the
shared layout, so a read-only `binary_sensor` had an `onTap()` that tried to command it.

### §2's area tag — three permanent modes, and area is a separate setting

§2 offers two header treatments "to prototype, not one", implying a winner would be chosen. The
owner's decision is that **all three ship** — bar, tag, and none — selected by one build-sheet
setting, with a **second, independent** setting for whether the area displays at all.

The three differ in what they cost the card:

| Mode | Card height | Body cost | Area | STALE |
|---|---|---|---|---|
| bar | X | a strip | in the band | in the band |
| tag | X − tag | nothing | pill above, outside | second pill above |
| none | X | nothing | **not shown at all** | floating badge, top-right |

`none` showing no area is the owner's explicit call: it is the mode you pick when you do not want
one, not a mode that renders one without decoration.

### The tag's clearance comes out of the row gap, and it is a sum

The tag hangs **outside** the card, as §2 says. Making that work without cards differing in size
took three attempts. What it requires: the cell must not clip (`LV_OBJ_FLAG_OVERFLOW_VISIBLE`), the
tag parents to the *cell* rather than the card, and the page widens its row gap to
**`gap + tagHeight`** — a sum, not a maximum, so the space above a tag equals the space between two
plain cards. Every card in tag mode takes the shorter height whether it carries a tag or not.

### §2's "area comes free from MQTT discovery" is wrong

> *"Area comes free: Home Assistant already carries it on devices, so `MqttProvider` can populate
> it from discovery rather than anyone tagging entities by hand."*

Verified against the code: nothing in the tree publishes or reads an area, and MQTT discovery's
`suggested_area` runs **outbound** — it is how a device suggests its own area to HA, not a way to
learn someone else's.

The owner's broader point, which supersedes this: **MQTT carries a value per topic and nothing
else.** Area, battery, last-seen and device grouping each need a hand-crafted topic or an
automation maintaining it, per entity — unmanageable at hundreds of entities. Full HA entity
integration goes over the **websocket** (#43). Until then, area is build-sheet supplied. See
`FUTURE_IMPROVEMENTS.md`.

### §9's "one icon size" was costed against the wrong thing

§9 concludes "one icon size, not a family", on a measured ~96 KB per referenced face. That figure
came from a **95-glyph full-ASCII** face. The shipped subset is 84 glyphs at two sizes per board —
one for a state card's disc, one for a value card's title row — and it cost **less than the single
Montserrat_48 the placeholder icons had been borrowing**: the 3248's flash went *down* 4,680 bytes.

§5's "prefer provider-supplied" is now honoured for the first time. Every `EntityDescriptor`
already carried `mdi:thermometer` and the like, and nothing could draw them, so all of them were
being ignored in favour of a `device_class` guess.

### A command's verdict belongs to the entity

§3 says the reverted optimistic write "*is* the event" and needs no new plumbing. True, but it is
**anonymous**: after the revert, `pending` is false and the value is back, indistinguishable from
an ordinary update. Worse, a *successful* echo carries the value the optimistic write already
applied, so nothing is dirtied and no card is told anything at all.

`Entity::cmdFailed` resolves both. Every card bound to an entity reads the identical fact, which is
what makes a parent's state derive from where its children *are* rather than the path they took —
all resolved children failed → `FAILED`, some → `PARTIAL` (a new state, §3 had only the total case).

### Compact and full are derived, not declared

Issue #15 asks for both. A card measures whether its cell can seat a title row, the hero and an
optional row at the type scale's sizes, and draws **less** when it cannot — never the same thing
smaller, which would undo the work the generated type scale exists to do. `ValueCard` drops the
status corners (§1 already treats that row as absent when empty); `StateCard` drops the **name**
and keeps the icon, because §4 says state *is* the icon and its colour.

### The corner icon is per DOMAIN; the hero icon is per THING

Owner's direction, 2026-09-14, recorded before it is built.

**Every card carries a small icon in its top-left corner**, including the ones
that already show a large one in the middle. The two answer different questions
and should be allowed to differ:

- the **corner** icon is the card's TYPE — eventually mapped straight from the
  domain, so a light card always wears the same corner mark wherever it appears
- the **hero** icon is the specific THING — a light might be a bulb, a ceiling
  fixture, a strip, a lamp; the corner stays a bulb regardless

A consequence worth noting now: on a group card, the mixed indicator moves from
the top-left to the top-RIGHT, because the corner belongs to the icon.

The corner icon costs nothing to place. It is deliberately **out of the vertical
flow** on both layouts - it sits in a corner the centred hero never uses, and
reserving a row for it is what broke `CYD_S3_3248`'s status line at 121 px. The
owner's constraint is only that the two must not touch: "It is perfectly fine if
part of the value or icon is adjacent to it horizontally."

### Long press pauses, for now

§4 wants a long press on a group to open a sheet of per-light cards. That needs an overlay this
milestone does not have. Long press currently toggles §3's per-card **pause**, on the base class,
for every type — the only whole-card action meaningful on a read-only sensor as well as a switch.
A group card will override it; everything else keeps pausing.

---

## 12. Corrections from hardware — 2026-09-19

### The HDR_BAR band DOES overhang the card's corners. Confirmed.

`Card.cpp`'s own comment said the band "gets the card's own radius instead. Its lower corners round
where they used to be square." `HANDOFF.md` said its "corners sit slightly outside them." **Those
are different artifacts and the handoff was right** — the owner photographed it and circled the
overhang at both top corners.

The code comment has been the misleading one since 2.5. Whoever takes the 2.8 slot rework should
trust the photo, not the comment: the band is `lv_pct(100)` of the surface with the card's radius,
and it escapes the rounded corners rather than being masked by them. `clip_corner` is what used to
hide it, and `clip_corner` is what froze `WS_P4_5` — see `LESSONS.md`.

### A generated VALUE face carries only the glyphs the script lists

`gen_type_scale.py`'s `num` range had no colon. A VALUE face is either a built-in Montserrat (full
ASCII) or one of these subsets, so `01:25` rendered correctly on five boards and as tofu boxes on
exactly the three using a generated face — `WS_P4_4B`, `WS_P4_5`, `WS_S3_5B`.

**The owner reasonably suspected the connectivity fault.** It was a missing glyph. `0x3A` and
`0x64` (for `3d 04:15`) are in the range now.

The general rule: **anything a card can print in the VALUE role must be in the `num` range.** That
includes whatever #51's duration formats and any future unit or separator produce.

### TAG is not scaled, and the reason generalises

What makes a card cramped is the **vertical stack**, and VALUE dominates it. TAG is a small label
in the area header; shrinking it reclaims almost no height and costs legibility immediately.

The owner asked whether a global 0.92 would fix it on the 4B pair. It cannot, and the reason is not
obvious: sizes quantise to even pixels, and on `WS_S3_4B` the TAG target is ~12.7 px at 1.0, so
0.85, 0.88, 0.90 and 0.92 **all land on 12**. The first scale that returns it to 14 is 1.00, which
drags VALUE from 34 back to 40 and undoes the change entirely.

Hence `SCALE` takes a per-role dict as well as a float. Scale what costs height; leave the label
faces alone.

### Duration display: three formats, resolved like TempUnit

`DurationFormat` sits beside `TempUnit` in `CardTypes.h` and resolves the same way — fleet default,
then page, then card — because it is the same problem: a value whose stored form and displayed
form differ. `DUR_AUTO` switches by magnitude (`09:58` / `1:06:40` / `3d 04:15`) and is the
default; `DUR_CLOCK` is fixed `H:MM:SS`; `DUR_SECONDS` is the raw count.

Keyed on `device_class: duration`, **not** on the unit string — `"s"` is a fine unit for something
that is not a duration. The unit label is suppressed for a formatted duration for the same reason
temperature goes through `cardDisplayUnit()`: `4:15:33 s` is a caption that lies.

### What the card's secondary line should measure — see #57

`Seen:` currently reads `lastUpdateMs`, which `EntityRegistry::setValue()` sets **unconditionally**,
including for unchanged values. On a Zigbee2MQTT device that publishes temperature, occupancy,
illuminance and battery in one payload, that means **`deck_temp`'s timestamp is bumped every time
somebody walks past the deck**.

It is a device-liveness timestamp wearing a value's name. Useful — it is what staleness should use
— but not what the card should display. #57 adds `lastChangeMs` for the card and keeps
`lastUpdateMs` for staleness, at which point `Last:` becomes the correct label and `Seen:` the
wrong one. The label and the semantics move together or not at all.

### Cards must bubble their events

`Card::build()` sets `LV_OBJ_FLAG_EVENT_BUBBLE` on `_surface`. This is not cosmetic: 2.6's swipe
handling records the press origin on the SCREEN, and a clickable object consumes its own events, so
without bubbling a swipe that starts on a card reports an origin of `{0,0}` — which reads as "left
half, top edge" for every gesture on the display.

If cards ever stop bubbling, gesture navigation silently returns to that behaviour rather than
failing. The two belong in the same thought.

---

## 13. Milestone 2.7 decisions — 2026-09-22

Agreed with the owner before any of 2.7 was built. Where this section and an earlier one disagree,
this one wins; the earlier text is kept as the record of how the design got here.

### Where a card's icons come from

**Measured first**, against the owner's instance (read-only websocket query, 2026-09-22):

| Entity | `attributes.icon` | Why |
|---|---|---|
| garage doors | *absent* | the owner removed his static override that morning |
| kitchen / office occupancy | changes with state | they are **template** entities whose template sets it |
| the lights, the TV-room switch | fixed | the owner's own registry override |
| temperatures, illuminance | *absent* | nobody set one |

So **HA core does not compute a state-dependent icon into the state object.** The frontend picks
its defaults client-side; `attributes.icon` only exists when a user or an integration set one.
`ha-websocket.md` §5 generalised from the two template sensors and was wrong about this. Our own
state pairs therefore stay the main source for binary sensors, not a fallback nobody reaches.

**Two icons per card, answering different questions** (§11 recorded the split; this settles it):

| | Question it answers | Source, first match wins |
|---|---|---|
| **Corner** | what KIND of card this is | build-sheet override -> **our table**, keyed on `device_class`, then domain. HA has no per-entity "type" icon, so this is always ours |
| **Hero** | this particular THING, in its current state | build-sheet on/off pair -> build-sheet icon -> HA's live `attributes.icon` -> our `device_class` state pair -> the corner icon |

Until the build sheet exists, `EntityDescriptor` stands in for it: `icon` is the user's override,
`iconOn`/`iconOff` are the owner's custom state pair. The descriptors that copied HA's icons have
been emptied, so HA's live icon wins as agreed.

**A static icon set in HA freezes the hero.** It outranks our open/closed pair, exactly as it does
in HA's own frontend. Accepted deliberately, for consistency; the build-sheet pair is the escape
hatch.

**Any override glyph must be in the generated font.** `scripts/gen_icon_font.py`'s `GLYPHS` list is
maintained by hand; `scripts/scan_ha_icons.py` reports what HA uses that we do not ship. A name the
font lacks falls through to the next source rather than drawing tofu. At 3.2 the generator should
read the build sheet's overrides so nobody maintains the list.

### Binary sensors: one card type, driven by a `device_class` table

Not a class per sub-type. A row per class carries the corner glyph, the on/off hero pair and the
on/off words; adding `valve` or `moisture` is a row, not a class. HA's own "Show as" setting is a
`device_class` override (the garage doors are `opening` underneath, shown as `garage_door`), so it
feeds this table for free. `door` survives only as a word a build sheet may use to mean "show as
door".

- **Label: name | state word | none**, per card, resolving fleet -> page -> card like `TempUnit`.
  Default **name**, because the name is what tells two doors in one area apart. The state word is
  the table's ("Open"/"Closed", "Detected"/"Clear"), never a raw "on"/"off". This relaxes the
  NO STATE WORDS rule in `StateCard.h` into a user choice that defaults to off.
- **Colour: the ordinary active colour** for an open door and a detected presence alike. The icon
  carries the difference.

### Lights

- **The disc takes the light's own colour** when it reports one. HA sends `rgb_color` for every
  colour mode, colour-temperature included (`light.office` reads `[255,167,88]` at 2710 K), so one
  field covers both. The whole surface is NOT tinted: legibility on a saturated colour is the risk.
- **Brightness fills the card from the bottom up**, after HA's own tile. Off is empty, 100% is the
  fully-filled card we have today, a light with no brightness fills fully when on. The disc still
  says on/off; the fill says how much.
- **Tap toggles. No slider on the card.** Cards are smaller and more numerous than was assumed, and
  a slider is the gesture conflict #17 warns about.
- **A long-press popup** mirroring HA's light dialog (brightness, colour, colour temperature) is
  the control surface. It is **not 2.7**: groundwork is milestone 2.10, after 2.6's horizontal
  swipes, with the full per-type content at 4.4. Long press stays PAUSE until then, and pause moves
  into the popup when it arrives.

### Corner icon geometry

- It must be **glued to the corner**: the same distance from the card's left edge as from whatever
  bounds its top — the band in bar mode, the top border in tag and none. The owner's report that it
  sits too low in No-header mode traces, in the code, to `Card::restyle()`: it reserved a header's
  height of top padding in every mode except tag, undoing `build()`, which reserved it only in bar.
  Fixed on this reading; confirmation is on glass.
- It **scales with the card**, not only with the board. See the type-scale note below.
- It now appears on **every** card. Until 2.7 only `ValueCard` drew one. The aggregate "2/3" badge
  that occupied `StateCard`'s top-left moves to the status row.

### Type sizes by card size

The "~96 KB per face" figure that has shaped every font decision was ONE large full-alphabet
Montserrat. Measured from the P4 builds' object files (an upper bound on flash):

| Face | Size per face |
|---|---|
| generated VALUE digits subset, 50-68 px | 6-10 KB |
| full MDI icon subset, 18-52 px | 10-73 KB |
| Montserrat 14-24, full ASCII | 14-29 KB |

So only the roles that make a card cramped step with the cell: the **hero value**, the **hero
icon** and the **corner icon**. Name, unit, tag and status text stay one size per board. Chosen by
the cell's size in millimetres, so a card looks the same across boards. Absorbs #62.
