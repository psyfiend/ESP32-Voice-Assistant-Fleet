# Card design — decisions and open questions

**Status: decisions captured 2026-09-10, not yet implemented.** Owner's design direction from the
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

## 2. Area is a tag, and it lives outside the border

Colour-coded, **attached to the card rather than inside it**. Putting it inside alongside label +
value + icon + secondary + status is what tips a card from dense into busy — the owner called this
before it was built, and the bench's inside-the-border version confirmed it.

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
2. **Stale** — a bright, noticeable tag at the top of the card.
3. **Long stale** (past a second threshold) — the tag **grows into a header band** across the top
   of the card. The card is still readable; it just cannot be mistaken for live.

Both thresholds are per-entity (`Entity.stale_after` already exists) and the second one needs a
name and a default. **Open: what is the second threshold, and is it a multiple of the first or its
own field?**

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
Open question — a history buffer is per-entity RAM the registry does not currently have, and on
`CYD_S3_3248` that is the scarce pool. Sizing this is work for #14.

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
- **Groupable by room.** One tap = "turn on all the kitchen lights". Long-press (or double-tap)
  opens a sheet listing the individual lights and their states.
- Open: does a group card show "3 of 6 on", a single aggregate state, or both?

### Action / scene
New type, not previously in the roadmap's list. Cards that fire an automation, scene or template —
"all living room lights", "bedroom 50%", "bedroom 80%". Over MQTT these are outbound commands,
which makes this the **first real consumer of #44**, currently deferred as "nothing has a control
to send one yet." That is no longer true.

### Weather station
A custom, standalone card showing several published temperatures and/or forecast data. Almost
certainly a multi-cell span rather than a grid unit, which makes it the first test of whether
"cards" and "full-width panels" are the same object. Phase 6 in the roadmap; the requirement is
now specific enough to design against earlier.

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
| `CYD_S3_8048` | 800×480 | **4×2** | Owner's call |
| `WS_P4_5` | 1280×720 | **5×3**, possibly 5×4 | `espcontrol` runs 5×4 on jc8012p4a1 at 1280×800 |
| `WS_S3_5B` | 1024×600 | 5×3 on pixels, **but see note** | Tearing and slow refresh may cap it below what the pixels allow |
| `CYD_P4_1060` | 1024×600 | **5×3** | `espcontrol` ships exactly this for jc1060p470 — the same panel |
| `WS_P4_7B` | 1024×600 | **5×3 minimum**, 6×4 plausible | Owner's estimate |

Two things this table settles:

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

## 8. Open questions, collected

Each of these blocks something specific; none blocks 2.2.

| # | Question | Blocks |
|---|---|---|
| 1 | Second staleness threshold — own field, or a multiple of `stale_after`? | 2.4 |
| 2 | How many icon state-variants can the flash budget afford? | The MDI subset — answer **before** generating it |
| 3 | Per-entity history buffer: how many samples, in which RAM? | Sensor card history, #14 |
| 4 | Group light card: "3 of 6 on", aggregate state, or both? | 2.7 |
| 5 | Is the weather-station card a card, or a different object that owns a page? | Phase 6, but the answer shapes 2.5 |
| 6 | Does a card bind one entity or primary + N secondaries? | **2.4 — the biggest one.** §1's status row is the argument for primary + 2 |

---

## 9. Reference material — read, do not copy

Two projects added 2026-09-10. **Check `docs/REFERENCE_PROJECTS.md` before reusing anything from
either**; one of them is not permissively licensed.

- **`esphome-modular-lvgl-buttons`** — **MIT**, reusable with attribution. Its `ui/<type>/` split
  into `local.yaml` / `remote.yaml` / `detail.yaml` per entity type is the same shape as our card
  types plus context sheets, arrived at independently.
- **`espcontrol`** — **PolyForm Noncommercial 1.0.0.** Architecture and layout decisions may be
  studied; **code may not be copied**, and the licence bars commercial use of the software itself.
  Its per-device grid definitions are the evidence in §7.
