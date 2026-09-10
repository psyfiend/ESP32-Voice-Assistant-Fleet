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
| 2 | How many icon state-variants can flash afford? | **Unanswered, and mine to measure.** Blocks the MDI subset — see below |
| 3 | Per-entity history: how many samples, in which RAM? | **Dissolved.** Fetch from HA on demand, store nothing (§4.1) |
| 4 | Group light card: aggregate, count, or both? | Tap toggles all; **mixed state gets its own indicator**; long-press opens per-light cards |
| 5 | Weather station: card or page? | **Both.** A card with a 2×2 minimum, plus an optional full-page view opened from it |
| 6 | One entity per card, or primary + N? | **Primary + up to 2 siblings of the same device** (§1). A card that wants more is a group card |

**Question 2 is the only one still open, and it is the one that gates real work.** The MDI subset
must not be generated until the state-variant budget is known, because regenerating it later means
regenerating every board's font blob. It is answered by measurement, not discussion — folded into
#14.

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
