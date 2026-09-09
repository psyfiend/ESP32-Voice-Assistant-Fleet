# Handoff — 2026-09-09

For whoever picks this up next, human or Claude. Written at the end of the session that closed
Phase 1, while the context was still warm.

**Read `docs/ROADMAP.md` §0 "Where we are" first.** This file is the shorter, more opinionated
version: what to know that is *not* obvious from the docs, and what I would have wanted told to
me.

---

## The one-paragraph version

Phase 0 (repo hygiene) and Phase 1 (connectivity) are done and merged to `main` at tag
`v0.2.0`. The fleet has a working data pipeline in both directions — board telemetry publishes
to Home Assistant, and real Zigbee2MQTT sensors read back — through an Entity Registry that
neither side knows the shape of. There is **nothing to draw it with yet**.

**Phase 2.1 (startup reorganisation, #12) is done and hardware-verified**, on branch
`feat/startup-reorg-12`, not yet merged. Startup is now five files —
`main` / `SystemCore` / `SystemReport` / `LVGL_Startup` / `GUIManager` — with the design, the
decisions and their reasons in `docs/design/startup.md`. Read that before touching startup;
it is the doc that explains why LVGL initialises last and why `LVGL_Startup::lock()` is a no-op
that you should nevertheless call. Next up is 2.2 (design system) or 2.3 (memory spike).

**One thing to know if you are new to this branch:** `SystemCore` and `SystemReport` contain no
LVGL include, and that is load-bearing rather than tidy. It is what would make a GUI-less build
a `build_src_filter` line instead of a redesign. Do not casually add one — same class of rule as
`Fleet_Entities`' zero dependencies.

---

## How to think about the architecture

If you read one thing, read the whiteboard model in `ROADMAP.md` §4.1 and §4.2:

**Providers write on a whiteboard. Cards read from it. Neither knows the other exists.**

- `Fleet_Connectivity` gets on the network.
- `Fleet_MQTT` talks to a broker. It never learns what an entity is; it asks the link exactly
  one question, `isOnline()`.
- `Fleet_Entities` is the whiteboard. **Zero dependencies on purpose** — no Arduino, no LVGL,
  no JSON — so it compiles and unit-tests on a PC. Do not casually add an include here; that
  property is load-bearing (ROADMAP Q9) and it is why the registry takes caller-provided
  storage instead of allocating its own.
- `Fleet_Providers` is where Arduino and ESP calls are allowed to live. A provider writes
  values in and does nothing else — never renders, never publishes.

**The threading rule (§4.2) is not optional.** Providers must never touch LVGL. They write to
the registry and mark it dirty; the LVGL task drains it. Breaking this does not crash
immediately — it corrupts LVGL and crashes hours later somewhere unrelated.

Two flags carry more weight than their size suggests:
- `advertise` separates entities **we own** (publish to HA) from entities **someone else owns**
  (subscribe and render). One flag, no second code path.
- `source` records provenance, and **a card must never branch on it**. That is the whole point.

---

## Things that will bite you

`docs/LESSONS.md` is the full list and it is worth twenty minutes. The four most likely to
catch you out in the next session:

1. **"SUCCESS" can mean your library was never compiled.** PlatformIO's LDF only builds what
   something `#include`s. Verify with `find .pio/build/<env> -name "MyFile.cpp.o"`.
2. **Every commit triggers a full rebuild** because `FW_VERSION` is a global `-D` (issue #46).
   This is also why the VSCode upload arrow seems to hang — it rebuilds first.
3. **`CYD_S3_3248` is the memory constraint**, not any P4 board. It is the fleet's only QSPI
   panel, so it is the only board whose LVGL buffers must sit in internal SRAM. Check any new
   static allocation against it.
4. **Verify claims about the outside world from outside.** Three diagnostics have lied to us.
   A hostname is confirmed in the router's lease table; an MQTT publish in MQTT Explorer.

---

## What is deliberately unfinished

Everything here is descoped by decision with reasoning on its issue — none of it is blocked or
forgotten.

| Item | Issue | Why it can wait |
|---|---|---|
| **AP path fixed but unproven** | **#45** | `softAP()` has not run since the crash fix. Highest-value loose end |
| `AP_ACTIVE → DEGRADED` compile-verified only | #42 | Same test as #45 covers it |
| HA access without MQTT | #43 | Most HA users have no broker; blocks *others* before us |
| Captive portal | #6 | Needs a web server that arrives in Phase 4 |
| On-device settings screen | #7 | System panel covers development needs |
| `_proven` credential fingerprint | #39 | Only a developer can hit it |
| Outbound entity commands | #44 | Nothing has a control to send one yet |
| Mic capture, SD card, rotation on most boards | — | Peripheral coverage, not framework work |
| Discovery payload will outgrow the buffer | #47 | ~10 entities of headroom today |

**If you do one thing from this list, do #45.** The AP is the rescue path — it runs when
something has already gone wrong — and we currently cannot say whether it works. One junk-SSID
flash settles #45 and #42 together.

---

## Conventions that are not obvious from the code

- **Debug output is gated** behind `-D DEBUG_<AREA>` in an environment's `build_flags`, never
  deleted. `DEBUG_WIFI` and `DEBUG_MQTT` are currently on **fleet-wide** while the pipeline is
  under test — turn them off when it settles.
- **Errors are not gated.** Anything that means something is genuinely wrong prints
  unconditionally. Hiding those behind a flag recreates the silent failures that have cost us
  most.
- **Designated initialisers must follow declaration order.** Applies to BSP headers *and* to
  entity tables.
- **Avoid single-word ALL-CAPS enumerators.** Arduino's macro namespace will eat them.
- **`platformio.ini` is committed from this machine and that is correct.** Only a *clone's*
  rewritten `symlink://` prefix must never be committed.

---

## Open design questions worth knowing about

**The overlay model for defaults (#20).** The owner's intent is that virtually any value in any
`*Defaults.h` should be overridable from the build sheet, layered like Windows Mobile ROM
"kitchen" packages — load order decides, last one wins. The `#if defined(BOARD)` trees now in
`DeviceIdentity.cpp` and `ConnectivityDefaults.h` are a cheap imitation that will not scale.
**Build-sheet schema v1 should be designed with this in mind**, or it gets designed twice.

**`ENTITY_MAX` is 48** and was picked, not derived. Now that storage is in PSRAM the ceiling
matters much less, but #14's memory spike should still measure the real numbers.

**We are not adopting ESPHome**, and the reason is explicitly personal rather than technical —
this is a learning project. See `GUI_FRAMEWORK.md`. The technical case for adopting something
off-the-shelf will keep presenting itself; that is not a reason to revisit it.

---

## Working style that has been productive

- **Decide before building.** Structural choices — a new library, a new field on a shared
  struct — are worth a turn of discussion first. The owner is not a developer, and code
  arriving faster than it can be evaluated produces the "I don't follow this and can't fix it"
  feeling. He catches real design problems when he can see where the choices were: he spotted
  the provider/entity conflation in `SystemProvider` and the fact that no issue existed for the
  AP test.
- **Say what is verified and what is inferred.** This project has been bitten repeatedly by
  confident claims that turned out to be untested. "Compiles" and "runs" are different words.
- **Reference projects in `reference/`** are a genuine asset — but **the two chvvkumar repos
  have no licence**, so read for architecture and do not copy code. `ha-dashboard` is MIT. See
  `REFERENCE_PROJECTS.md`, which also lists what has *not* been mined yet.

---

## Immediate next steps

1. **Merge `feat/startup-reorg-12`.** Done and verified; only the last commit (the
   device-identity banner move) has not been re-flashed, and its output is byte-identical by
   construction.
2. Then Phase 2 proper: design system (#13), memory spike (#14), card base class (#15).
   **#14 has grown two extra deliverables** — see `FUTURE_IMPROVEMENTS.md`: a written
   internal-SRAM/PSRAM allocation-order table, and the free-heap number on `CYD_S3_3248`
   (22 KB at boot, measured once) turned into something actually measured.
3. Fold #45 into whenever a board is next on the bench with time to spare. It now settles three
   things, not two: `raiseAp()`'s mode line was fixed on this branch and is equally unexercised,
   because a board with working credentials never calls `softAP()`.

`ConnectivityManager::raiseAp()`'s identical-branch ternary is **fixed** (`b4b4336`). The
interesting part was which branch was right: the STA retry ladder runs the whole time the AP is
up and re-issues `WiFi.begin()` without touching the mode, so `WIFI_AP_STA` is correct
unconditionally and an AP-only branch would have stranded a board over a router reboot.
