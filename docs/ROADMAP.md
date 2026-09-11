# Fleet Dashboard — Blueprint & Roadmap

**Status: AGREED and in progress.** Phases 0 and 1 are complete. Section 8's open questions
are answered except where marked, and the answers are recorded there rather than in chat.

This is the *plan* doc. It answers "what are we building, in what order, and how do we know
we're making progress." It does not replace:

| Doc | Answers |
|---|---|
| `CLAUDE.md` | How the HAL/BSP works today (stable facts) |
| `docs/HARDWARE_STATUS.md` | Which board does what; what is untested; build-environment issues |
| `docs/FUTURE_IMPROVEMENTS.md` | Fleet-wide deferred work |
| `docs/LESSONS.md` | Mistakes worth not repeating |
| `docs/REFERENCE_PROJECTS.md` | What is in `reference/` and what to mine from it |
| `docs/GUI_FRAMEWORK.md` | Superseded by this doc; retained as a pointer |
| GitHub issues | What is being worked on right now, and what is blocked |

---

## 0. Where we are — 2026-09-08

**Phase 0 (repo hygiene) and Phase 1 (connectivity) are done.** The dashboard has a working
data pipeline in both directions and nothing to draw it with yet.

**What exists and runs on hardware:**

- WiFi station, AP and APSTA, with classified failures and retry ladders, on five boards.
- Device hostnames in DHCP, verified from the router's lease table.
- An MQTT broker session with its own backoff, LWT and reconnect handling.
- An Entity Registry: one list of everything the panel knows, with staleness and optimistic
  writes, and zero dependencies so it compiles and tests on a PC.
- Two providers. `SystemProvider` publishes this board's own telemetry; `MqttProvider` reads
  entities other devices own.
- Home Assistant discovery — the device appears in HA with correct BSP-derived identity and
  four diagnostic entities, and four Zigbee2MQTT values arrive back the other way.

**Verified on two boards chosen to cover the real axis of difference:** `WS_P4_5` (MIPI/DSI,
P4) and `CYD_S3_3248` (QSPI, S3) — both chip families and both LVGL buffer strategies. A
fleet-wide boot pass on 2026-09-08 covered the other six.

**One thing is fixed but unproven: the AP path.** `softAP()` has not run on any board since
the crash that motivated the PSRAM change, because the default mode moved to
`STA_WITH_AP_FALLBACK` and a board with working credentials never raises an AP. Tracked as
issue #45, and it matters because the AP is the rescue path.

**The literal next step: Phase 2.1, the startup reorganisation.** Split `main` /
`LVGL_Startup` / `GuiManager` *before* adding UI rather than after — it is the cheapest it
will ever be, and every card built beforehand would have to move. Written up in
`FUTURE_IMPROVEMENTS.md`.

**What we are deliberately leaving behind for now.** None of these are blocked; each was
descoped so the framework could progress. Every one keeps an open issue with its reasoning:

| Left behind | Issue | Why it can wait |
|---|---|---|
| Captive portal | #6 | Needs a web server that does not arrive until Phase 4 |
| On-device connectivity settings screen | #7 | The System panel already shows network state during development; better built after the Phase 2 design system |
| `_proven` credential fingerprint | #39 | Cannot be hit by a user, only by a developer changing credentials |
| Proven/unproven behaviour matrix (tests 2 and 3) | — | Coded and reviewed, never observed; the matrix was consuming the schedule |
| HA access without MQTT | #43 | **Important, not urgent.** Most HA users have no broker, so this blocks other people before it blocks us |
| Proving the AP path actually works | #45 | Every board now uses `STA_WITH_AP_FALLBACK`, so `softAP()` has not run since the crash that motivated the PSRAM fix. The highest-value loose end |
| Outbound entity commands | #10 | The registry supports them; nothing yet has a control to send one |

---

## 1. Repo health check — findings (2026-09-03)

Working tree: **clean.** No uncommitted changes, no stashes, nothing untracked.

Three real issues found, in severity order:

### 1.1 CRITICAL — the repo cannot be cloned into a buildable state

There are **five gitlink entries** (mode `160000`, i.e. submodule pointers) in the index, and
**no `.gitmodules` file at all**:

```
160000 components/GFX_Library_for_Arduino
160000 components/bb_captouch_fork
160000 components/lvgl
160000 lib/!Private Originals/GFX Library for Arduino
160000 lib/!Private Originals/bb_captouch_fork
```

`git submodule status` currently fails outright:
`fatal: no submodule mapping found in .gitmodules for path 'components/GFX_Library_for_Arduino'`

**What this means in practice:** if this repo were cloned to a new machine (or a CI runner, or
after a disk failure), `components/lvgl/`, `components/GFX_Library_for_Arduino/` and
`components/bb_captouch_fork/` would all be **empty directories**. The build would not run.
The only reason it works today is that those directories happen to exist on this one machine.
On GitHub they show as grey "commit" links that go nowhere.

This also means `CLAUDE.md` is partly wrong: it says bb_captouch_fork is "not a proper git
submodule and the parent has zero history for it." The parent *does* record a commit SHA for it
(`586cf77`) — it's a half-submodule: pointer recorded, URL mapping missing.

Fix is small and mechanical (Phase 0.1). It has to happen before OTA/CI/multi-machine work.

### 1.2 Two stale gitlinks under `lib/!Private Originals/`

Those two entries point at commits of libraries that have since moved to `components/`. The
directories are **not** empty on disk — `bb_captouch_fork` has 42 files and
`GFX Library for Arduino` has 353, each with its own `.git` — but git tracks **zero** of those
files in this repo. Only the two pointers are recorded, so the contents exist on this machine
and nowhere else.

Decision (2026-09-03): no longer needed, and re-obtainable from upstream if they ever are.
Remove both index entries and delete the directories.

### 1.3 Minor / cosmetic

- Local `main` (`90af28a`) is 1 commit ahead of `origin/main`. That commit is *not* at risk —
  it's reachable from `origin/wifi-testing` — but the GitHub `main` branch is stale and doesn't
  reflect the connectivity architecture doc.
- `wifi-testing` is 9 commits ahead of `main` and fully contains it. Clean merge available.
- An empty `VA Testing/` directory tree sits in the project root (contains only an empty
  `.claude/`). Invisible to git, harmless, but confusing. Recommend deleting on disk.
- `platform = espressif32` is still unpinned (already noted in FUTURE_IMPROVEMENTS). Same class
  of problem as 1.1 — the build is only reproducible on this machine.

---

## 2. How we work together (the token-efficiency contract)

This section exists because "don't burn the whole month's allotment" is a hard requirement, not
a nice-to-have. These are the levers that actually matter, in rough order of impact.

**1. One session = one milestone.** The biggest cost driver is a long session where every later
message re-reads a growing history. When a milestone is done and committed, start a fresh
session. The docs (this one, `HARDWARE_STATUS.md`, `CLAUDE.md`) are the handoff mechanism — they
let a new session get up to speed in ~5k tokens instead of ~60k of re-exploration.

**2. Write the design down before writing the code.** A one-page `docs/design/<subsystem>.md`
agreed in advance means implementation sessions read 3KB of spec instead of grepping 40 files to
re-derive intent. This is *the* reason a blueprint pays for itself.

**3. Point me at files, not at "the codebase."** "Read `src/UIToolkit.cpp` and
`include/Panel_Header.h`" costs ~2k tokens. "Figure out how the UI works" costs ~40k.

**4. No subagents unless we agree.** Each spawned agent starts cold and re-derives context you
and I already have. They're the expensive path. I won't use them without you asking.

**5. Batch hardware feedback.** Flash all the boards you can, collect all the serial output,
paste it once. Ten separate "here's another log" round-trips each re-send the conversation.

**6. Compile locally, not through me.** Run `pio run -e <env>` yourself and paste only the
errors. **Superseded 2026-09-09:** Claude builds as a matter of course now — a build is the
cheapest possible check that a refactor did not break anything, and paying for it in tokens beats
finding out on the bench. The rule that survives is the one about *hardware* feedback: batch it
(§2.5). (Reminder from FUTURE_IMPROVEMENTS: run `pio` from **PowerShell, not Git Bash** —
pioarduino 55.03.311 rejects MSYS shells.)

**7. Track progress in GitHub Issues, not in chat.** See §3.4.

**8. Ask for code + a one-line doc update, not code + an essay.** I'll keep prose in commits and
docs where it's durable, and keep chat replies short.

---

## 3. Git, versioning, branching, releases

You said you'd defer to me here, so this is a concrete recommendation, not a menu. It's
deliberately the *simplest* thing that gives you real safety and real progress tracking.

### 3.1 Branch model — short-lived feature branches off `main`

```
main          <- always builds, always flashable. Never commit directly.
 |- feat/wifi-ap-fallback        (one milestone)
 |- feat/mqtt-transport          (one milestone)
 |- feat/entity-registry         (one milestone)
 \- fix/cyd3248-brownout         (one bug)
```

Rules:

- **`main` is sacred.** If `main` doesn't build for all 8 environments, that's a stop-the-line
  event. Everything else can be broken; `main` cannot.
- **One branch = one milestone = one merge.** If a branch is open more than ~a week, or touches
  two unrelated subsystems, it was scoped too big.
- Naming: `feat/`, `fix/`, `docs/`, `chore/`, `spike/` prefixes. `spike/` = throwaway
  experiment, expected to be deleted rather than merged.
- Delete branches after merge. Stale branches are how a solo repo gets confusing.

**Why not one long-lived branch per subsystem** (e.g. keeping `wifi-testing` open for months):
you've already paid the cost — `wifi-testing` currently carries 9 commits including two
unrelated board fixes and a submodule bump, and `main` on GitHub is days stale and
misrepresents the project. Short branches keep `main` honest.

### 3.2 Merging — squash, with one exception

**Default: squash merge.** Each milestone lands on `main` as exactly one commit with a good
message. `git log main --oneline` then reads as a literal progress log — one line per completed
milestone. That is your measurable-progress ledger, free.

**Exception: bring-up/investigation branches merge with `--no-ff`** (keeping individual commits)
when the intermediate steps have diagnostic value — e.g. a board bring-up where "tried X, didn't
work, tried Y" is the useful record. Rare.

`wifi-testing` is a judgement call: it has 9 commits of genuinely distinct work. Recommend
`--no-ff` for this one, squash for everything after.

### 3.3 Versioning & release tags

Firmware needs a version because OTA and HA discovery both report one.

**Scheme: `A.B.C.D` — adopted 2026-09-03**, per the four-level split you described. Your
description was accurate, and the four-part form is common in firmware precisely because a build
counter is useful there in a way it isn't for libraries.

| Level | Name | Bumped when | Set by |
|---|---|---|---|
| **A** | Major | Breaking changes — UI overhaul, architecture rewrite, build-sheet schema break | you, by tagging |
| **B** | Minor | New features, new card types, new tools — backwards compatible within the same A | you, by tagging |
| **C** | Phase | **The roadmap sub-phase just completed.** Finishing 2.1 tags `v0.2.1`; finishing 2.2 tags `v0.2.2` | you, by tagging |
| **D** | Build | Hotfixes, typos, padding tweaks — every commit | **automatic** |

**D auto-increments**, which is the part you said you liked. It's derived from
`git describe --tags --long`: tag `v0.1.0`, make 14 commits, and it reports `v0.1.0-14-gabc1234`,
which becomes version `0.1.0.14`. Monotonic, never typed by hand, and it can't drift — because
nobody maintains it. You only ever tag `A.B.C`; `D` takes care of itself.

- Tag on `main` only. Start at `v0.1.0` — pre-1.0 honestly signals "schema may still change."
- **C tracks the roadmap, revised 2026-09-10.** It was originally "patch / stability fixes", but
  this project's version has one real job: telling you where you are. Mapping `B.C` onto the phase
  number does that for free and never needs a judgement call — `v0.2.1` *is* Phase 2.1. The cost is
  that a mid-phase stability fix has nowhere of its own to go; it lands in `D` with everything
  else, which is fine because `D` is what the firmware actually reports.
- Retroactive: `v0.2.1` tags the Phase 2.1 merge (`b5fc6d7`). Phase 1 keeps `v0.2.0`.
- A small `extra_scripts` Python hook runs `git describe` at build time and injects
  `-D FW_VERSION='"0.1.0.14"'`, so the firmware always knows exactly which commit it is.
- A dirty working tree appends `+dirty`, so you can never mistake a hand-modified local build for
  a real release when debugging a board on the wall.
- Every OTA image and every HA discovery payload carries it.

### 3.4 Progress tracking — GitHub Issues + one Project board

You want measurable progress and you're not a software engineer, so use the tool built for
exactly this rather than markdown checkboxes that go stale.

- **One GitHub Issue per milestone** in §7 below. Title = milestone name. Body = the acceptance
  criteria from this doc.
- **One GitHub Project board** (Todo / In Progress / Blocked / Done). Milestones move across it.
- Branch names reference the issue (`feat/mqtt-transport-14`), commit messages say `Closes #14`,
  so merging auto-closes the issue. Zero manual bookkeeping.
- Phases become **GitHub Milestones**, which give you a literal percentage-complete bar.

~30 minutes of one-time setup, and it's the difference between "am I making progress?" being a
feeling and being a number.

### 3.5 Protecting yourself

- Push every branch to `origin` daily, even mid-work. A local-only branch is one disk failure
  from gone (this repo has already lost work that way — the bb_captouch revert).
- Before any risky refactor: `git tag checkpoint/<date>` and push it. Cheap insurance.
- After Phase 0.1, verify recovery works: clone the repo into a scratch folder and confirm it
  builds. If it doesn't, the backup is theoretical.

---

## 4. Target architecture

Five layers. The rule that makes it modular: **each layer only talks to the one below it.**

```
+---------------------------------------------------------------+
| 5. BUILD SHEET      declarative dashboard definition           |
|                     (grid size, which cards, which entities)   |
+---------------------------------------------------------------+
| 4. UI SHELL         tileview nav, header bar, navbar,          |
|                     context sheets, settings pages             |
+---------------------------------------------------------------+
| 3. CARD LIBRARY     Card base class + card types               |
|                     (sensor, button, light, switch, climate,   |
|                      binary_sensor, weather, ...)              |
+---------------------------------------------------------------+
| 2. ENTITY REGISTRY  single source of truth for values.         |
|                     Cards read it. Providers write it.         |
|                     HA discovery is generated FROM it.         |
+---------------------------------------------------------------+
| 1. PROVIDERS        MqttProvider - LocalSensorProvider -       |
|                     SystemProvider (rssi/ip/heap/uptime) -     |
|                     HaProvider                                 |
+---------------------------------------------------------------+
        ^ existing HAL: Fleet_BSP - DisplayManager - TouchManager
          - AudioManager - FleetI2C - Fleet_Connectivity
```

### 4.1 The Entity Registry is the keystone

This is the most important decision in the whole plan, so it gets its own justification.

`docs/GUI_FRAMEWORK.md` sketched a "data-source abstraction" for the UI. `FUTURE_IMPROVEMENTS.md`
separately sketched an `MqttEntityDescriptor` registry for HA discovery. **Those are the same
object.** Build them as one thing.

An `Entity` is:

| field | purpose |
|---|---|
| `id` | stable string key, e.g. `living_room_temp` |
| `kind` | sensor / binary_sensor / switch / light / button / climate / weather / text |
| `value` | tagged union: float / bool / int / string |
| `unit`, `device_class`, `icon` | display + HA discovery metadata |
| `source` | LOCAL / MQTT / HA / SYSTEM / VIRTUAL |
| `writable` | can the UI command it? |
| `state_topic` / `command_topic` | filled in centrally, never by the descriptor |
| `last_update`, `stale_after` | staleness -> cards grey out instead of lying |
| `advertise` | should this be published to HA discovery? |

Why one registry pays off:

- A card binds to `id` and does not know or care where the value came from. The same card renders
  a local I2C sensor and a remote HA entity.
- HA discovery payloads are **generated** by walking the registry. No hand-written JSON per
  peripheral, and no drift between "what the UI shows" and "what HA knows about."
- Adding a new physical sensor = register an entity. It automatically becomes placeable on a
  dashboard *and* automatically appears in HA. One step, not three.
- Staleness is handled once, centrally, instead of per-card.

### 4.2 Threading — the biggest unanticipated trap in this whole plan

`lv_conf.h` currently has `LV_USE_OS = LV_OS_NONE`. LVGL is not thread-safe in that mode.

MQTT callbacks, the web server, and the WiFi event handler all run on **different FreeRTOS tasks**
than `loop()`. **Calling any `lv_*` function from an MQTT callback will corrupt LVGL's internal
state.** The failure mode is nasty: it doesn't crash immediately, it crashes randomly hours later,
or silently renders garbage. This will absolutely happen if we don't design against it up front.

**Rule: providers never touch LVGL.** A provider's only job is to write a value into the Entity
Registry (under a short mutex) and mark it dirty. An `lv_timer` running in the LVGL task drains
the dirty set every ~100ms and updates the bound widgets. One direction, one thread, no locks in
UI code.

This also gives free rate-limiting: a topic firing 50x/sec produces at most 10 redraws/sec.

### 4.3 What happens to the existing panels

Don't delete them — **repurpose them.** Proposed:

- The accordion/expanding-panel mechanism in `UIToolkit` becomes the **context sheet**: a bottom
  sheet that opens on long-press of a card, populated from that card's entity (long-press a light
  card -> brightness + colour-temp sliders; long-press a media card -> transport controls). That
  is exactly your "panels dynamically change content based on the screen" idea, and it gives the
  animation you like a real job instead of being the primary navigation.
- `Panel_Display` / `Panel_Audio` / `Panel_System` become **settings pages**, reachable from the
  settings screen and the navbar — not always-on decks at the bottom of every dashboard.
- `Panel_Header` becomes the **header bar** proper: a configurable slot list rather than a fixed
  title + one icon.
- `UIToolkit` keeps its genuinely good parts: `sc()` DPI scaling, semantic fonts, the toast layer.
  Those are the seeds of a real design system.

Rough estimate: ~40% of existing UI code survives as-is, ~40% moves, ~20% is replaced.

### 4.4 Naming collision worth fixing now

You called the grid+cards unit a "screen." LVGL already uses `lv_screen` / `lv_screen_load()` for
something else. Recommend calling ours a **page** (a Dashboard has many Pages; a Page has a grid
of Cards). One search-and-replace now saves a lot of ambiguity for the project's whole life.

---

## 5. Key technology recommendations

### 5.1 Grid — use LVGL's native grid layout

`LV_USE_GRID` is already enabled. Use `lv_obj_set_grid_dsc_array()` with fractional units
(`LV_GRID_FR(1)`) and `lv_obj_set_grid_cell()` with row/column **spans**. That gives:

- Card sizing (1x1, 2x1, 3x2) for free via span parameters.
- Resolution independence for free — the same 5x3 build sheet lays out correctly on 1024x600 and
  1280x800 without changing a number.

Don't hand-roll pixel math. It'll fight `UI_SCALE` and lose.

Sane grid defaults per resolution class (tunable in settings, per your requirement):

| Panel | Class | Default grid | Approx cell |
|---|---|---|---|
| 320x480 (CYD_S3_3248) | small portrait | 2x3 | ~145x140 |
| 480x480 / 720x720 | square | 3x3 | ~150 / ~230 |
| 800x480 (CYD_S3_8048) | wide | 4x2 or 4x3 | ~190x150 |
| 1024x600 (WS_P4_7B, CYD_P4_1060) | wide | 5x3 | ~195x180 |
| 1280x800 | large | 6x3 | ~205x250 |

**Card minimum readable size is ~140x120px** for a mushroom-style card (icon + title + value).
Below that, drop to a compact variant. A 3x3 grid on 320x480 gives 106px cells — too small; hence
2x3 above.

### 5.2 Navigation — `lv_tileview`

LVGL's `tileview` is purpose-built for exactly what you described: swipe left/right along X,
swipe up/down along Y, with native drag-follow and snap. Pages go on the X axis, menus on Y.

**Two gotchas that will bite:**

1. **Gesture conflict.** A swipe that begins on a slider, button, or scrollable card gets
   swallowed by that widget and the page won't change. This is the single most common failure in
   LVGL dashboards. Mitigations: make cards non-scrollable
   (`lv_obj_remove_flag(card, LV_OBJ_FLAG_SCROLLABLE)`), keep interactive sub-widgets *out* of
   dashboard cards (put sliders in the context sheet instead — a second, independent argument for
   §4.3), and reserve a gesture-safe margin.
2. **Memory.** Tileview keeps every tile's object tree alive. 5 pages x 15 cards x ~8 LVGL
   objects x ~200 bytes is roughly 120KB — against an `LV_MEM_SIZE` of 128KB. **That will not
   fit.** Mitigations: lazy-build tiles on a neighbour's `LV_EVENT_SCROLL_BEGIN` and free
   non-adjacent ones, or move `LV_MEM` into PSRAM (all 8 boards have it, but PSRAM allocation is
   slower). Needs measuring on the *smallest* board, not the biggest. Tracked as milestone 2.3.

### 5.3 Build sheet format

Recommend the **same layering you already chose for WiFi**: a compile-time default overridden at
runtime by stored config. Consistency with an existing, working pattern is worth a lot.

- Compile-time: a C++ struct array in a per-dashboard header, BSP-style. Zero parse cost,
  type-safe, guaranteed to exist on a fresh flash.
- Runtime override: JSON on LittleFS, written by the web config page or the on-device settings UI.
  Parsed with ArduinoJson streaming from file (no full-file RAM copy).
- Boot: if the LittleFS build sheet exists and parses, use it; else fall back to the compiled
  default. Identical in shape to `ConnectivityManager`'s NVS-then-default logic.

JSON over YAML: ArduinoJson is mature, streaming and low-RAM; embedded YAML parsers are not.
(This is Q3 below — I have a recommendation but it's your call.)

### 5.4 Icons & visual quality

The gap between "functional" and "gorgeous" here is almost entirely **icons and typography**, not
layout code.

- Generate a **Material Design Icons subset as an LVGL font** (the standard trick every
  mushroom-alike project uses). ~150 glyphs at 2-3 sizes gives HA-native iconography and crisp
  scaling at roughly 40KB of flash. Do this in Phase 2, not later — retrofitting icons after
  cards exist means touching every card.
- Consider replacing Montserrat with something with more character for hero values. Font
  conversion is a build step, so design it in early.
- Per-card accent colour driven by `device_class` (temperature = amber, humidity = blue, etc.),
  with a user override. Colour carries meaning, not just decoration.

### 5.5 Web config page — use the synchronous server

`ESPAsyncWebServer` runs handlers on its own task, which walks straight into the §4.2 threading
trap, and it has had real compatibility churn against arduino-esp32 3.x. For a config page
serving one browser occasionally, the built-in synchronous `WebServer` pumped from `loop()` is
fast enough and removes an entire class of bugs. The same server instance serves the captive
portal in AP mode.

---

## 6. UI gotchas, risks, and things you probably haven't anticipated

Ordered by how likely they are to cost you a weekend.

**6.1 LVGL thread safety.** See §4.2. Highest-severity item in the document.

**6.2 STA+AP simultaneous mode on the P4 boards is unverified.** You want APSTA as the boot
default. On the four P4 boards, WiFi isn't native — it goes through an ESP32-C6 co-processor over
SDIO (`esp_hosted`). Whether that transport supports `softAP()` concurrently with STA is
**unknown and needs testing before the design depends on it.** Also, even where APSTA works, the
ESP32 forces both interfaces onto the *same radio channel* (the STA's), so the AP's channel
follows your router — normal, but surprising when debugging.

**6.3 APSTA may worsen the CYD_S3_3248 reset problem.** That board already resets several times
before connecting (suspected brownout when the PA keys up). Running AP+STA increases current
draw. Test that board specifically and early.

**6.4 Leaving an AP up forever is a security surface.** Recommend the fallback AP be WPA2 with a
per-device password derived from the MAC, displayed on-screen in Settings. Better UX than a
shared secret, and not an open network sitting in your house indefinitely.

**6.5 Touch dead zones.** `CYD_S3_3248` doesn't register touches in roughly the outer 12-14px of
the digitizer. Grid outer padding must exceed that or edge cards become partly untappable. Worth
checking whether the other GT911 boards share it.

**6.6 Redraw scoping.** Updating one card must invalidate one card, not the screen. Get this
wrong and a 15-card dashboard with a 1Hz clock repaints everything every second. On
`CYD_S3_3248` (QSPI + software rotation, already documented as sluggish) that will be visibly
bad. Corollary: **update the clock label only when the displayed minute changes**, not every tick.

**6.7 Animation budget per board.** `CYD_S3_3248` pays a per-pixel software rotation cost on
every draw. Card-heavy pages plus slide animations will be worse there than on the DSI boards.
Plan for a per-board animation-quality flag rather than discovering it late.

**6.8 Touch target size.** 44-48px minimum for reliable finger taps. On a 320x480 panel that's a
third of a cell. Argues for whole-card tap targets, with controls living in context sheets.

**6.9 Don't encode state in colour alone.** These panels have poor viewing angles and washed-out
gamma. Pair every colour state with an icon or text change.

**6.10 OTA needs partition space you should decide on now.** `default_16MB.csv` has two app slots
(good), but LittleFS space for build sheets and web assets may need a custom partition CSV.
Changing the partition table later means a **full serial reflash of every board** — it cannot be
done over OTA. Decide the layout before the first OTA-capable release ships.

**6.11 OTA needs a rollback path.** An OTA that bricks a wall-mounted panel means getting a
screwdriver. Use `esp_ota_mark_app_valid_cancel_rollback()` gated behind "did WiFi connect and
did LVGL render a frame," so a bad image auto-reverts on next boot instead of bricking.

**6.12 Build-sheet schema versioning.** The moment a build sheet lives on LittleFS, a firmware
update can arrive that no longer understands it. Put a `schema_version` in the sheet from day one
and refuse-with-fallback rather than crash. Cheap now, painful to retrofit.

**6.13 NVS wear.** Don't write settings to NVS on every slider drag. Debounce writes by a few
seconds. NVS has finite erase cycles and a brightness slider can generate hundreds of writes a
minute.

**6.14 MQTT reconnect storms.** Eight devices that all reconnect immediately on broker restart
will hammer it. Exponential backoff with jitter, plus a Last Will & Testament so HA marks devices
unavailable promptly.

**6.15 Licensing, since you mentioned borrowing code.** MIT/Apache/BSD: fine, keep the copyright
header. **GPL/AGPL: viral** — if you copy GPL code into this repo and publish it, the whole repo
becomes GPL. Worth a 10-second license check before copying, especially from ESPHome-adjacent
projects (much of ESPHome's own codebase is GPLv3). Reading someone's code for *ideas* and
reimplementing is unrestricted; copying files is not.

---

## 7. Phased roadmap

Each milestone is sized to be **one session, one branch, one merge**. Acceptance criteria are
written to be objectively checkable — that's what makes progress measurable.

### Phase 0 — Repo hygiene (blocking, ~1 session)

Per-library decisions made 2026-09-03 (see §1.1 for why this matters at all):

| Library | Decision | Rationale |
|---|---|---|
| **LVGL** | Stay a submodule; **update to `v9.5.0`** | Currently pinned to an *untagged* master commit (`v9.4.0-134-g040dc399`). v9.5.0 exists upstream. Moving to a real release tag is both newer *and* more stable than where we sit now. Zero local modifications, so nothing to lose. |
| **GFX_Library_for_Arduino** | Stay a submodule; **add an `upstream` remote** | Fork has 3 real local commits on top of `1.6.0-Waveshare` (the bounce-buffer fix + 2 srcFilter exclusions). Only `origin` (your fork) is configured — no upstream remote exists, so pulling new releases is currently impossible. This is the library most likely to need upstream updates for future boards. |
| **bb_captouch_fork** | **Un-submodule it** — vendor the files directly into this repo | Modifications have gone past cosmetic; syncing with the original author is no longer expected. Becomes a normal folder whose files are tracked in *this* repo's history, so future changes are versioned here and nothing can be lost the way the earlier revert was. |

| # | Milestone | Status | Acceptance criteria |
|---|---|---|---|
| 0.1 | Vendor `bb_captouch_fork` | **done** | Nested `.git` removed; 11 files tracked here. Prior history verified pushed to `psyfiend/bb_captouch` (`586cf77`) before removal |
| 0.2 | Fix remaining submodules | **done** | `.gitmodules` maps LVGL + GFX; `git submodule status` works (previously failed outright); stale `lib/` gitlinks + dirs and `VA Testing/` removed; exactly 2 gitlinks remain |
| 0.3 | Update LVGL to `v9.5.0` | **done (build)** | Submodule at `v9.5.0`. Full rebuild after clearing `build_cache`: both dev targets SUCCESS. Cost on CYD_S3_3248: RAM +0, flash +9,932 (+0.7%). *Flash-test for visual regressions still pending.* |
| 0.4 | Add GFX `upstream` remote | **done** | `upstream` -> `moononournation/Arduino_GFX`; fetch verified, pulled tags through **v1.6.7** (fork was on `1.6.0-Waveshare`+4, i.e. 7 releases behind with no way to see it). Documented in `CLAUDE.md` |
| 0.5 | Pin the platform | in progress | `platformio.ini` pins pioarduino `55.03.311` by release-zip URL (tag + asset both verified to exist) |
| 0.6 | Version plumbing | in progress | `scripts/fw_version.py` injects `FW_VERSION`/`FW_COMMIT` from `git describe`; wired via `extra_scripts` in `[env]` |
| 0.7 | **Clone test** | **done (with caveat)** | Fresh clone + `git submodule update --init --recursive` verified: LVGL checks out at `v9.5.0`, GFX at `1.6.0-Waveshare-4-g861a218`, `bb_captouch_fork` and `boards/*.json` present. **Building from a clone at a different path still requires fixing the hardcoded `symlink://` prefix** — deliberate, see below |
| 0.8 | Merge to `main` + tag | pending | `--no-ff`; `main` pushed; `v0.1.0` tagged; all 8 envs build |

**What the clone test actually found — three independent clone-breakers**, none visible from inside
the working repo, and each of which would have surfaced only during a recovery or on a new machine:

1. **No `.gitmodules`** despite five recorded gitlinks — clone produced empty component dirs. *Fixed.*
2. **`boards/esp32-s3-n16r8-Guition_JC3248W535EN.json` existed only inside PlatformIO's install
   directory**, never in the repo. Surfaced when pinning the platform re-extracted the package and
   deleted it. Would also have broken this machine on any future `pio pkg update`. *Fixed by
   vendoring it into `boards/`.*
3. **26 hardcoded absolute `symlink://` paths** in `platformio.ini`. A clone on this machine would
   have compiled the *original* repo's components while appearing to build its own — passing for the
   wrong reason. *Deliberately not fixed; see `CLAUDE.md`.*

Two incidental findings worth keeping:

- Windows `MAX_PATH`: cloning into a deep directory fails on
  `components/SensorLib/examples/...` paths. Use `git config --global core.longpaths true`.
- `default_envs = WS_P4_TOUCH_LCD_7B` means a bare `pio run` builds **one** environment, not eight.
  The "`main` must build everywhere" rule needs the explicit 8-env invocation (milestone 5.3's
  fleet build script should absorb this).
| 0.9 | Issue tracker setup | **blocked** | Issues + Project board created from this roadmap. `gh` 2.99.0 is installed but **not authenticated** — needs `gh auth login` |

**Carried forward from Phase 0 (not blocking, don't lose these):**

- `include/lvgl/lv_conf.h` is still labelled *"Configuration file for v9.4.0"*. No hard version
  gate exists and the build is correct, but options **added in 9.5 are silently taking defaults**
  rather than being reviewed. Worth a diff against 9.5's `lv_conf_template.h` — likely relevant
  to the §5.2 memory work, since draw-buffer and cache options live there.
- `src/Panel_Header.cpp:18` warns `bitwise operation between different enumeration types
  'lv_part_t' and 'lv_state_t' is deprecated`. Pre-existing, harmless, but it's the standard
  `LV_PART_MAIN | LV_STATE_DEFAULT` idiom and will recur in every new card and panel. Worth
  settling on a cast helper in the design system (§2.2) before writing 8 card types that each
  reproduce it.

### Phase 1 — Connectivity — **COMPLETE 2026-09-08**

| # | Milestone | Status |
|---|---|---|
| 1.0 | Device hostnames (#38) | **DONE.** Verified in the router's DHCP lease table, not in serial output — see `LESSONS.md` |
| 1.1 | Connectivity state machine (#4) | **DONE**, hardware-verified on five boards. Four modes, classified failures, retry ladders. One criterion met differently: state is a polled atomic rather than a callback, which avoids firing UI work from the WiFi event task (§4.2) |
| 1.2 | APSTA feasibility spike (#5) | **DONE — yes.** `STA_PLUS_AP` confirmed on three P4 boards and an S3, so mode 2 is offered fleet-wide with no per-board gating |
| 1.3 | AP + captive portal (#6) | **AP half DONE.** Portal descoped — needs the Phase 4 web server |
| 1.4 | Connectivity settings UI (#7) | **Header glyph DONE.** Screen descoped to after the Phase 2 design system |
| 1.5 | Flash-test remaining boards (#8) | **DONE, 8 of 8.** The fleet-wide flash pass put the full stack on every board; each joined the network, connected to the broker and appeared in Home Assistant |
| 1.6 | MQTT transport (#9) | **DONE**, connected to a real broker. Classified failures, own backoff ladder, LWT plus explicit `offline` on clean shutdown, subscriptions replayed on reconnect |
| 1.7 | Entity Registry (#10) | **DONE.** Registry, staleness, optimistic writes, and two providers. Zero-dependency build proven by compiling it standalone. Outbound commands remain unimplemented — nothing has a control to send one yet |
| 1.8 | HA discovery (#11) | **DONE**, verified in Home Assistant. Device-based discovery, one retained payload generated by walking the registry |

Ordering note, now vindicated: **1.7 before 1.8.** Because discovery is a *projection* of the
registry, it turned out to be a single serializer rather than a per-entity publish loop, and
the `cmps` map of device-based discovery mapped straight onto the registry's contents.

**Verified end to end on `WS_P4_5`:** four entities published outward to HA with correct
BSP-derived device identity, four Zigbee2MQTT entities read back inward, all through one
registry that neither side knows the shape of.

**The scope cut that made this finishable (2026-09-06).** The proven/unproven behaviour matrix
was consuming the schedule for diminishing returns, so the bar was deliberately lowered to
*"the common paths are verified; the edge cases are coded, reviewed, and honestly recorded as
unobserved."* That decision stands, and it is worth keeping as a template for later phases.

One result is worth carrying forward: the single test retained purely because it was cheap and
had **never been run** immediately exposed a real bug (#42). When scope has to be cut again,
"never observed at all" beats "most likely to fail" as the selection criterion.

### Phase 2 — UI foundation

| # | Milestone | Acceptance criteria |
|---|---|---|
| 2.1 | Startup reorg | **DONE 2026-09-09, hardware-verified on both dev targets.** Landed as a 5-way split — `main` / `SystemCore` / `SystemReport` / `LVGL_Startup` / `GUIManager`. Identical UI behaviour, boot serial, entities and HA discovery on `WS_P4_5` and `CYD_S3_3248`. Design, decisions and the one real re-sequencing (LVGL now initialises last) in `docs/design/startup.md` |
| 2.2 | Design system | **DONE 2026-09-10, hardware-verified.** `UITokens` - palette incl. semantic state colours, per-scheme metrics, derived grid, type scale. Reference page reachable from the System panel, three schemes switchable live. Panels ported off hardcoded colours. **MDI icon font deferred to 2.4** - see below. `docs/design/tokens.md` |
| 2.3 | Memory budget spike | **DONE 2026-09-10, measured on `CYD_S3_3248`.** ~715 B per card in `lv_mem`; pool steady at 35% with dashboard + reference page. **Decision: neither lazy-loading nor a PSRAM `LV_MEM` is needed** - see below. No longer gates 2.5 |

**What 2.3 actually found, and why the decision went the way it did.**

The question was framed as "how many cards fit". It turned out to be the wrong question, twice.

- **Cards are cheap.** ~715 bytes each in LVGL's pool. A 5x3 grid is ~11 KB of a 124 KB pool; a
  dense 20-card page is ~14 KB. You would need well over a hundred cards before `lv_mem` bound
  anything. Tileview lazy-loading solves a problem we do not have.
- **The measurement everyone would reach for is the wrong one.** `ESP.getFreeHeap()` barely moves
  when a card is created. `LV_USE_STDLIB_MALLOC` is `LV_STDLIB_BUILTIN` with `LV_MEM_ADR 0`, so
  widgets come out of a **128 KB static array in internal DRAM** - which is ~70% of
  `CYD_S3_3248`'s entire 188 KB static footprint. `lv_mem_monitor()` is the instrument.
- **The real cost is fonts, not widgets.** One referenced font face is **~96 KB of flash** -
  measured, not estimated. Enabling sizes in `lv_conf.h` is free; *referencing* them is what
  costs. That reframes the icon question completely (see below) and is worth more than the
  per-card number.
- **Fragmentation is stable.** It climbs to ~29% after repeated page rebuilds and parks there.
  Worth re-checking on a panel that has been up for weeks, but it is not growing.

**Moving `LV_MEM` to PSRAM stays available and is not needed yet.** At 35% used there is ample
headroom, and widget access is frequent enough that PSRAM's slower access is a real cost. Revisit
only if a future page genuinely fills the pool.
| 2.4 | `Card` base class | Grid placement with spans, entity binding, staleness handling, tap + long-press, compact/full variants. Also the agreed moment to introduce `src/UI/` and `src/Cards/` — see `docs/design/startup.md` §3.5 |
| 2.5 | Page + grid engine | A Page renders a grid from a config struct; cards place with spans; correct on 3 different resolutions |
| 2.6 | Tileview navigation | Swipe L/R between pages, U/D to menus; gesture conflicts resolved (§5.2); page indicator dots |
| 2.7 | First 4 card types | `sensor`, `binary_sensor`, `switch`, `button` — bound to real HA entities over MQTT |
| 2.8 | Header bar v2 | Configurable slot list: clock, WiFi/MQTT status, optional sensor slots |

### Phase 3 — Build sheet

| # | Milestone | Acceptance criteria |
|---|---|---|
| 3.1 | Schema v1 + `schema_version` | Documented in `docs/design/build-sheet.md` |
| 3.2 | Compile-time loader | Dashboards defined as C++ structs; two real dashboards for your HA setup |
| 3.3 | Runtime JSON loader | LittleFS override with compiled fallback; graceful parse-failure handling |

### Phase 4 — Settings, navbar, web config

| # | Milestone | Acceptance criteria |
|---|---|---|
| 4.1 | Settings pages | Brightness, volume, screen dimming/timeout, device info, network info |
| 4.2 | Layout settings | Grid rows/cols, spacing, card sizes, page reordering — applied live |
| 4.3 | Navbar | Configurable edge strip navigating to pages and fullscreen cards |
| 4.4 | Context sheets | Existing accordion panels repurposed per §4.3 |
| 4.5 | Web config page | Synchronous server; same settings surface as on-device; also serves the captive portal |

### Phase 5 — OTA

| # | Milestone | Acceptance criteria |
|---|---|---|
| 5.1 | Partition layout decision | Finalised **before** the first OTA release (§6.10) |
| 5.2 | OTA with rollback | Push-to-device + on-device "check for update"; auto-rollback on failed boot (§6.11) |
| 5.3 | Fleet build script | One command builds all 8 environments and stages images with version metadata |

### Phase 6 — Expansion

Card library growth (light, climate, weather, media, camera), local hardware libraries (AXP2101,
PCF85063, QMI8658, RS485/Modbus), the weather-station fullscreen template, and the alarm-clock
page.

---

## 8. Open questions — answer before Phase 2

For each: the question, then **what I would have assumed and why**, so you can just say "yes,
assume that" wherever you agree.

### Q1 — AP behaviour — **DECIDED 2026-09-03**

Four selectable connectivity modes, exposed in both the build sheet and the UI settings:

| Mode | Name | Behaviour |
|---|---|---|
| 0 | `OFF` | Connectivity disabled entirely. Parent option — overrides the rest. |
| 1 | `STA_WITH_AP_FALLBACK` | **Default.** STA only; raise the AP only when STA fails. |
| 2 | `STA_PLUS_AP` | APSTA from boot. AP auto-shuts-down after `ap_idle_timeout_min` with no client; `0` means never shut down. |
| 3 | `STA_ONLY` | Never raise an AP. WiFi must be configured on-device. |

Mode 0 is a genuinely good call and I hadn't proposed it: it makes the whole networking stack
optional, which means a board can be flashed and used as a purely local display with no radio at
all. That has to be designed in from the start — every provider and every card has to tolerate
"no network, ever" as a normal state rather than a failure state.

Implementation notes:

- Mode 1 being the default resolves risk §6.3 (APSTA worsening the CYD_S3_3248 brownout) for the
  default path. The APSTA spike (milestone 1.2) is still required, because mode 2 has to work on
  the P4/`esp_hosted` boards for the setting to be honest.
- Mode 3 needs a real escape hatch: if a user picks STA-only and then the AP's password changes,
  the device is unreachable. Requires a physical recovery path — hold BOOT for N seconds to force
  mode 1 for one boot. **Must ship at the same time as mode 3, not after.**
- `ap_idle_timeout_min` as a float with `0` = never is your spec; storing it as a `uint16_t`
  count of minutes with `0` = never is equivalent and avoids float comparison. Will use that
  unless you want sub-minute granularity.

### Q2 — Build sheet: authored where, and by whom?

Is a build sheet something (a) you hand-write in a text editor and I compile in, (b) you edit in
the web UI on the device, or (c) both?

**DECIDED 2026-09-04: both. Three-layer precedence, plus a separate hierarchical lock.**

Clarified: "the compile-time sheet has the final say" means it **overrules built-in defaults** and
is the single source of truth for what a device starts as. It does **not** mean runtime
customization is forbidden. Precedence, lowest to highest:

| Layer | Role |
|---|---|
| 1. Built-in defaults | Baseline used when no sheet says otherwise (the public/first-flash path from Q4) |
| 2. **Build sheet** (compile-time) | Overrules defaults. The single source of truth for the device's starting state, and the factory-reset target |
| 3. Runtime / user edits | Overrule the build sheet — **unless blocked by an authority lock** |

This matches the WiFi/NVS layering rather than inverting it, which is the more consistent outcome.

### Authority locks — hierarchical, inherited, overridable

Not over-thinking it. This is the same inheritance pattern as CSS, filesystem ACLs, and LVGL's
own style system, and it's cheap: one field per node plus a resolver.

`authority` is set at any level of the nesting (Dashboard > Page > Grid > Card > Entity). It
propagates down until something overrides it.

**It must be a tri-state, not a boolean:**

| Value | Meaning |
|---|---|
| `INHERIT` *(default)* | Take the effective value from the nearest ancestor that sets one |
| `LOCKED` | Frozen here and, by inheritance, below |
| `UNLOCKED` | Explicitly editable, **even under a `LOCKED` ancestor** |

A boolean can't express the last row, and that row is the whole point of your "unless manually
overridden by discrete downstream components" — e.g. a locked dashboard with one deliberately
user-editable card. Resolution walks up until it finds a non-`INHERIT`; the root defaults to
`UNLOCKED`.

**Critical distinction — lock configuration, never interaction.** Your examples mix two things:
"pages are locked so new cards cannot be added or deleted" is *structure*, but a naive single
LOCKED flag would also stop the cards from working. A locked light card must still toggle the
light. So the lock governs **what the build sheet describes** (layout, placement, which cards
exist, their config), never whether a control responds to touch. Otherwise a locked kiosk
dashboard degrades into a photograph.

If a use case later needs "visible but not operable," that's a separate per-card `read_only`
flag, not a mode of `authority`.

**UI consequence:** when something is locked by inheritance, the settings screen has to say *why*
and name the ancestor that locked it. A control that silently refuses to change is a bug report
waiting to happen.

Scoped into milestone 3.1 (schema) and 4.2 (live layout settings).

### Q3 — Build sheet format — **DECIDED 2026-09-03**

Your nested-struct proposal (Dashboard > Pages > Grid > Cards > Entities, ESP-IDF/BSP style) is
adopted **as the in-memory model**. The key move is separating two things that felt like one
question:

- **The model** — nested plain-C++ structs, exactly your hierarchy. This is the contract
  everything else is written against. Arduino-free, per the Q8 rule.
- **The loaders** — two of them, both producing that same model:
  - *Compile-time*: struct literals in a header, BSP style. Zero parse cost, type-safe.
  - *Runtime*: JSON on LittleFS, parsed with ArduinoJson streaming.

Format is not the same question as model. Once the model is the contract, the two loaders are
interchangeable and neither the grid engine nor the cards know which one ran.

**On "as universally readable as possible" if this takes off** — that's the argument that settles
format. A C++ header is readable to C++ programmers. JSON is readable by every language, every
tool, and any web UI you build. So: **struct model internally, JSON as the interchange format.**
If you later want a friendlier authoring syntax (YAML, or something custom), it becomes a
converter that runs on your PC and emits JSON — never a parser that has to run on the device.

Two things worth knowing before you commit to hand-authoring the compile-time form:

- C++ designated initializers must be listed in declaration order (already documented in
  `CLAUDE.md` for the BSP). Nested designated initializers with 12-card arrays get visually
  heavy fast. Workable, but it will not feel as pleasant as an ESPHome YAML block.
- Compile-time struct arrays are fixed-size and can't be resized at runtime, so the runtime
  loader needs its own allocation path. That's expected and fine — it's why there are two
  loaders rather than one.

### Q3c — Every default is build-sheet overridable — **DECIDED 2026-09-04**

Requirement: anyone should be able to write a build sheet that redefines *any* default
setting — not enums or core function names, but every tunable value. `WiFiDefaults` and
`TimeDefaults` are exactly the shape this applies to.

The design that makes this tractable rather than a maintenance treadmill: **each `*Defaults`
struct is a schema, and a build sheet is a set of overrides against it.** Adding a field to a
Defaults struct makes it overridable automatically — no per-setting plumbing, no second list
to keep in sync, no way for the two to drift.

The mechanism to adopt is the one the NINA project already proved: `settings_table.h` is an
**X-macro table**, one row per setting carrying its key, default and valid range, which drives
`settings_defaults_apply()`, `settings_clamp_apply()` and JSON serialization from a single
declaration. Its own comment warns against "improving" a range without updating both the
firmware behaviour and the comment — learned the hard way, and worth heeding.

**The line that stays drawn:** `Fleet_BSP` is *not* overridable. Pin assignments, panel
timings and bus configuration are hardware facts, not preferences; exposing them to a config
file just lets someone brick a board by editing text. The existing Defaults-vs-BSP split
already draws this correctly and should not be blurred to satisfy "override everything".

Concretely in scope for build-sheet override: hostname, `APPEND_MAC_SUFFIX`, AP SSID/password,
AP IP/subnet, idle timeout, connect timeout, retry count, TX power cap, timezone, NTP servers,
12/24-hour clock — and every field added to a Defaults struct hereafter.

Scoped into milestone 3.1 (schema).

### Q3b — Sub-cell / fractional grid placement — **DECIDED 2026-09-03**

You raised the case of a 3x3 page with a special card occupying "a quarter of the grid." Worth
solving up front, as you said, because it's very expensive to retrofit.

A quarter isn't expressible in a 3x3 grid (3 isn't divisible by 2). Three ways out:

1. **Sub-grid units (subdivision) — chosen.** Author the page as `3x3` *cells*, but allocate
   `6x6` *units* underneath (each cell = 2x2 units). Cards span in units. A quarter-page card is
   3x3 units — exactly expressible. A normal card is 2x2 units. Half-cell cards are 2x1 or 1x2.
   `subdivision` is a per-page property defaulting to 2, so a page that needs thirds can set 3.
2. Nested grids (a cell containing its own grid). Simpler, but a card then cannot span *across*
   a parent cell boundary — which is precisely the quarter-page case. Rejected.
3. Free positioning for special cards. Escape hatch that abandons the grid model. Rejected.

This is what CSS grid does, and LVGL's grid supports arbitrary track counts natively, so it costs
nothing structurally.

**On your two sub-questions** — dynamically shrinking neighbours vs. making half-spaces
un-assignable: I'd recommend neither.

- *Dynamic neighbour shrinking* is a constraint solver. Layouts become unpredictable, hard to
  author and very hard to debug ("why did that card move?"). Strongly recommend against.
- *Un-assignable half-spaces* is simple but wastes real estate on exactly the pages you cared
  enough to customise.
- **Chosen third option: placement at unit granularity, plus a validator.** Any card may be
  placed at any unit coordinate with any unit span. The loader detects overlaps and
  out-of-bounds and reports them — at *compile time* for compiled sheets, at *parse time* for
  JSON. No solver, fully predictable, and the half-spaces stay usable by anything declaring a
  matching span.

The responsive degradation from Q4 (`preferred_span` / `min_span` / `priority`) operates in units
too, so the two systems compose rather than fight.

### Q4 — Per-device vs fleet-shared — **DECIDED 2026-09-03**

Both, serving two different audiences:

**A. Public/default path — "works on first flash, no build sheet required."**
A baseline standard for header, badges and navbar, plus a per-resolution-class default grid
(§5.1's table). Ships with a handful of **local-only demo cards** that need no network at all:
system info, device temperature, memory usage, uptime, WiFi RSSI. Someone can flash this repo
onto a supported board and get a working, good-looking dashboard without touching a config file
or owning a Home Assistant.

This pairs exactly with connectivity mode 0 from Q1 — the demo cards are all `SYSTEM`-source
entities, so the default build is fully functional with the radio off. That makes "no network"
a first-class supported configuration rather than a degraded one.

**B. Personal path — heavily customised per device.**
Pages authored per room; each device's default page is the room it lives in. Devices in
different rooms show different things.

**The card sizing consequence, which is the real design requirement here:**

Cards must be **responsive, not fixed-size**. Rather than authoring a different page per screen
size, a card declares a *preferred* span and a *minimum* span, and the grid engine degrades it
to fit:

```
card: weather
  preferred: 2x1     <- big screens get the roomy layout
  minimum:   1x1     <- small screens get the compact variant
  priority:  40      <- if the page still overflows, lowest priority drops off
```

So a 1024x600 board renders 10 cards with some at 2x1, and a 320x480 board renders the same page
definition with everything collapsed to 1x1 and the two lowest-priority cards omitted. One page
definition, three renderings.

This makes **every card type a two-variant widget from day one** (compact + full), which is a
real cost — it roughly doubles the work per card type. It's worth it: the alternative is
hand-maintaining a page variant per screen size across 8 boards, which is the exact combinatorial
trap the `#ifdef` approach already fell into on the HAL side.

Consequence for milestone 2.4: the `Card` base class must carry `preferred_span`, `min_span` and
`priority` from the very first version. Retrofitting responsive sizing after 8 card types exist
means rewriting all 8.

### Q5 — HA naming scheme — **DECIDED 2026-09-07**

Concrete proposal:

```
device_id     = fleet_<board_macro_lowercase>_<last6 of MAC>   e.g. fleet_ws_p4_4b_a1b2c3
object_id     = <peripheral>_<measurement>                     e.g. es8311_mic_level
unique_id     = <device_id>_<object_id>
discovery     = homeassistant/<component>/<device_id>/<object_id>/config
state         = fleet/<device_id>/<object_id>/state
command       = fleet/<device_id>/<object_id>/set
availability  = fleet/<device_id>/status        (LWT: online/offline)
```

The HA `device` block gets `identifiers=[device_id]`, `name` = user-set friendly name,
`manufacturer` = `bsp_hw.MANUFACTURER`, `model` = `bsp_hw.MODEL`, `sw_version` = `FW_VERSION`.

This solves your "two boards with the same temperature sensor" problem: `unique_id` is
device-scoped, so identical peripherals never collide, while the friendly name stays human
("Kitchen Panel Temperature").

**Answered by the owner's own prior project** (`reference/.../Shed Power Monitor`, his code and
therefore freely reusable). The scheme above stands **except for the discovery topic**, which is
replaced:

| | Original proposal | Adopted |
|---|---|---|
| discovery | `homeassistant/<component>/<device_id>/<object_id>/config`, one payload per entity | **`homeassistant/device/<device_id>/config`, ONE payload for the whole device** |

Everything else is unchanged and turns out to be directly compatible: `device_id`, `object_id`,
`unique_id`, and the `state` / `command` / `availability` topics all survive as written. The
state and command topics also compose neatly with MQTT's `~` base-topic abbreviation
(`~` = `fleet/<device_id>/<object_id>`, then `stat_t` = `~/state`, `cmd_t` = `~/set`).

**Why device-based discovery.** It is the modern HA format (2024.11+) and it maps directly onto
the Entity Registry: with per-entity discovery you publish N payloads each repeating the full
device block; with device-based, **the registry *is* the `cmps` map** and discovery becomes one
serializer over it. It also makes entity *removal* work properly - under per-entity discovery a
removed entity needs an empty payload published to its config topic to evict the retained one,
or it haunts HA forever.

Requires HA >= 2024.11. Confirmed available: the target install is **2026.8.1**.

Full reasoning and the list of practices adopted alongside it: GitHub issue #11.

### Q6 — One HA device per board, or a fleet parent device? — **DECIDED 2026-09-07**

**One HA device per board. No fleet parent.**

Each board publishes its own device-based discovery payload and appears in Home Assistant as its
own device with its own entities. Both reference projects do exactly this, and the device-based
discovery format assumes that shape.

A parent device would mean boards declaring `via_device` against a hub that does not exist -
publishing a fiction to make an organisational chart. If a real coordinator ever appears, HA
supports `via_device` and this can be revisited without changing entity identity.

Assumptions: 1. **8 independent HA devices** — standard and simplest. 2. One HA device with 8
sub-devices (HA supports this now; more complex). 3. 8 devices plus a virtual "fleet" device
carrying aggregate status. I'd recommend **#1**.

### Q7 — Terminology — **DECIDED 2026-09-03: "page"**

A **Dashboard** contains **Pages**; a Page has a **Grid**; a Grid holds **Cards**; a Card binds
**Entities**. "Screen" now refers only to the physical display, and `lv_screen` keeps its LVGL
meaning. Usage: *"Page 1 is on the screen; swiping right-to-left shows Page 2."*

### Q8 — Voice assistant — **DECIDED 2026-09-03: parked, not abandoned**

`AudioManager`, `es7210`/`es8311` and the AEC path are **protected** during the UI refactor. No
audio code gets deleted or restructured for the dashboard's convenience.

**Open long-term question raised in the answer, deliberately not decided now:** the cutting-edge
voice work is happening in ESP-IDF, not Arduino. Two paths eventually:

1. Port IDF components into Arduino (what was done originally for the WS_P4_7B).
2. Migrate the whole project to ESP-IDF.

This does not need deciding now, but it **does** need a design constraint applied now: keep the
Arduino-specific surface area thin and concentrated. Specifically — the Entity Registry, Card
library, grid engine and build-sheet parser should be **plain C++ with no Arduino dependencies**,
touching Arduino only through the existing manager classes. If those layers stay
framework-agnostic, an eventual IDF migration is "rewrite the HAL and the LVGL port glue," not
"rewrite the entire dashboard." If they don't, the migration becomes prohibitive.

That constraint is nearly free to honour now and very expensive to retrofit, so it's adopted as
an architecture rule regardless of which path is taken later. Tracked as a review item on every
Phase 2/3 milestone.

### Q9 — Ethernet + library boundaries — **DECIDED 2026-09-03**

**No rename needed.** `Fleet_Connectivity` is already transport-agnostic as a name — it says
"connectivity," not "WiFi." Ethernet belongs inside it, not in a separate library; a second
library just for a wired link would duplicate the entire state machine, the mode selection and
the NVS settings layer for no gain.

Ethernet is designed into the interface **now**, implemented later. Concretely: the state machine
from milestone 1.1 tracks a `link` that is `WIFI_STA | WIFI_AP | WIFI_APSTA | ETHERNET | NONE`,
and callers only ever ask `isOnline()` / `getIP()` / `getLinkType()`. Only the WiFi backend gets
written for now. Two boards make this non-hypothetical: `CYD_P4_1060` has a physical port today,
and other `WS_P4_4B` variants ship with one.

**Three libraries, agreeing with your instinct to split MQTT out:**

| Library | Owns | Depends on |
|---|---|---|
| `Fleet_Connectivity` | Link layer: WiFi STA/AP/APSTA, Ethernet (later), the 4 modes from Q1, credentials in NVS, the connection state machine | Arduino/WiFi |
| `Fleet_MQTT` | Broker session: connect, pub/sub, LWT, backoff/reconnect, HA discovery publishing | `Fleet_Connectivity` (needs *a* link, doesn't care which) |
| `Fleet_Entities` | The Entity Registry (§4.1) and its dirty-flag bridge | **nothing** — plain C++, Arduino-free |

The boundaries earn their keep: `Fleet_MQTT` never learns whether the link is WiFi or Ethernet,
and `Fleet_Entities` never learns MQTT exists. Providers are the only code that bridges them,
which is exactly what makes a card render a local sensor and a remote HA entity identically.

`Fleet_Entities` having zero dependencies is also what makes the Q8 ESP-IDF escape hatch real —
it, the grid engine and the build-sheet model can be compiled and unit-tested on a PC.

### Q10 — External code: how do you want to hand it to me?

Answering your question directly, cheapest-first:

1. **Best: clone it locally and give me the path.** `git clone <url>` into e.g.
   `reference/<project-name>/` (gitignored). I read the actual files with zero network cost and
   full fidelity, and I can grep across the whole thing. By a wide margin the most
   token-efficient option.
2. **Second: give me raw file URLs** (`raw.githubusercontent.com/...`) for specific files. Works
   fine, costs a fetch per file, and I can't grep across the project.
3. **Don't bother forking.** Forking to your account doesn't make it more readable to me — it's
   only useful if you intend to modify and track changes against upstream.

**Always point me at specific files or directories, not "the repo."** "Reproduce the look of
their alarm clock page" on a large project is a 100k-token exploration; "here's
`src/ui/pages/clock.cpp` and `src/ui/theme.h`, reproduce this look" is a 5k-token task.

Same answer for your existing HA discovery code: drop the files in `reference/` and tell me the
path.

And yes to "point me at a repo and have me reproduce certain aspects of it" — with the caveat in
§6.15 about licenses if we copy rather than reimplement.

### Q11 — Dev targets — **DECIDED 2026-09-03, REVISED 2026-09-09**

- **Primary: `WS_P4_TOUCH_LCD_5`** (`WS_P4_5`). MIPI/DSI, P4 silicon. Revised from
  `WS_P4_TOUCH_LCD_7B` — the 5 is the board actually on the bench, and it is the one Phase 1 was
  verified on. The 7B remains fleet-supported, just not the day-to-day target.
- **Stress case: `CYD_S3_3248W535`** (`CYD_S3_3248`). Smallest screen, slowest bus (only QSPI
  panel in the fleet), known sluggish animations, known touch dead margins. If a page renders
  acceptably here, it renders anywhere.

Every milestone builds for both. That pairing deliberately brackets the fleet: the responsive
card system from Q4 is exercised at both extremes on every single change, rather than being
discovered broken on the small board months later.

**New hardware fact (2026-09-03): `WS_P4_7B` WiFi STA now confirmed working on real hardware.**
`docs/HARDWARE_STATUS.md` still lists it as untested/enclosed — needs updating. Same benign
`hostedHasUpdate()` / `Req_GetCoprocessorFwVersion` warning as `WS_P4_4B`, as expected for the
shared P4+C6 architecture.

Follow-up raised: flashing/rebuilding the ESP32-C6 co-processor firmware appears to be a known,
not-difficult procedure in several projects. Worth investigating in Phase 1 — it may clear the
cosmetic RPC warning outright, and it's useful knowledge regardless for a fleet whose four P4
boards all depend on that co-processor. Not blocking; the warning is confirmed harmless.

---

## 9. What I need from you to start

Nothing blocks **Phase 0** — it's mechanical repo hygiene and I can do all of it now.
**Phase 1.1 and 1.2** are also unblocked (the state machine, and the APSTA feasibility spike,
whose answer we need regardless of how Q1 lands).

Everything from Phase 1.3 onward wants Q1 answered. Everything from Phase 2 onward wants Q2, Q4,
Q7 and Q11.
