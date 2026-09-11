# Handoff — 2026-09-10

**Start here.** `CLAUDE.md` is the stable how-it-works; this is where we actually are, what will
bite you, and what to do next.

---

## Where the project is

**Phases 0, 1 and 2.1–2.3 are done and merged to `main`.** Tagged `v0.2.2`.

The device boots, joins WiFi, talks to an MQTT broker, appears in Home Assistant with correct
identity, publishes its own telemetry and reads other devices' entities — all through one Entity
Registry that neither side knows the shape of. Startup is a clean five-way split, and there is now
a complete design-token system with a live reference page on the device.

**What there still isn't: a card.** Nothing renders an entity as a tile yet. That is milestone
2.4, and it is the next thing to build.

### Read in this order

1. `CLAUDE.md` — the HAL/BSP, the startup split, the token rules, the traps.
2. `docs/design/cards.md` — **every card decision is already made.** Fourteen of them, plus six
   answered questions. Do not redesign cards; read this and build what it says.
3. `docs/design/tokens.md` — the design system and the measurements behind it.
4. `docs/design/startup.md` — only if you are touching boot order or LVGL setup.
5. `docs/ROADMAP.md` §7 for the milestone list, GitHub issues for what is open.

`docs/research/` holds three background reports (display-stack migration, ESP-IDF migration, voice
pipeline). They are not on the critical path — read them when the owner raises the topic.

---

## The next milestone: 2.4, `Card` base class (#15)

Everything it needs has been decided. `docs/design/cards.md` is the spec; the short version:

- A card binds **one primary entity plus at most two secondaries**, where a secondary is another
  entity *of the same physical device* (battery, last-seen). Anything wanting more is a **group
  card** — a separate type holding several primaries, spanning multiple cells.
- **Two layout families.** *Measure* cards (temperature, lux, power): small tinted icon top-left,
  name beside it, value centred and dominant — and the name is the **location**, not the
  measurement. *Actor* cards (lights, switches, doors, motion): big icon in a disc, centred, name
  below. **No "On"/"Off"/"Open"/"Closed" text anywhere** — state is the icon and its colour.
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

Measured this session, all on hardware:

| | |
|---|---|
| Card cost | ~715 B in `lv_mem` on `CYD_S3_3248` |
| LVGL pool | 128 KB static array in **internal DRAM**, 35% used, frag stable at 29% |
| One font face | **~96 KB of flash** |
| Fleet density | 165–294 PPI; scale derived as `PPI / 170` |
| Grid | one token set → 5×3 on `WS_P4_5`, 2×3 on `CYD_S3_3248` portrait |

Still assumed: how any of this looks on the six boards not yet flashed since the scale change
(`CYD_S3_8048` moved 1.0 → 1.10 and is the one to check first).

---

## Open loose ends

| What | Issue | Note |
|---|---|---|
| Reason 36 treated as real signal | **#48** | Self-inflicted `STA_LEAVING` causes six wasted re-associations per attempt, and the report names the wrong cause. Small, well-specified fix |
| AP idle-down leaves `AP_ACTIVE` stale | **#42** | Needs its own test: mode 3, junk SSID, ten minutes untouched. The #45 test did **not** cover it |
| HA discovery will outgrow the MQTT buffer | **#47** | Not urgent at 8 entities; will bite as cards add more |
| Outbound commands | **#44** | No longer deferrable — action/scene cards and light toggles both need it |
| Read HA without MQTT | **#43** | Now also the path for sensor-card history, which is fetched rather than stored |
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

Read `docs/design/cards.md` end to end, then build the `Card` base class against it. Do not
relitigate the decisions in it — they came from several rounds of looking at real panels, and they
are recorded precisely so the next session does not have to redo that.

Build for `WS_P4_TOUCH_LCD_5` and `CYD_S3_3248W535` on every change. They bracket the fleet: the
densest panel and the tightest memory.
