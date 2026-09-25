# Handoff — 2026-09-25

**Start here.** `CLAUDE.md` is the stable how-it-works. This file is only: where we are, what to do
next, what will bite you, and how to work with the owner. It was cut from ~680 lines to this on
2026-09-24; resolved investigations live in `docs/LESSONS.md` (the conclusions) and git history
(the detail). If you are about to add a long "how we found it" story here, it belongs in LESSONS.

**A warning about this file.** When it paraphrases a spec, the paraphrase becomes the spec for
whoever reads it first. Say *which* document a summary compresses, and treat vocabulary that does
not appear in the source as suspect.

---

## Where the project is

**`v0.2.7`, tagged on `main` 2026-09-24** (merge `c7f082e`). Phases 0, 1 and milestones 2.1–2.7
are done and signed off on glass. Boards report `v0.2.7.x`. Since the tag: #58 screenshots (merged
2026-09-25, not a milestone, so no tag).

What a board does today: boots into **two pages** (House = the owner's 18 HA entities over the
websocket, Fleet = MQTT/system/virtual cards), swiped horizontally with wrap-around; per-page
settings from the System drawer; three colour schemes (Fleet, **Midnight** default, **Linen** the
light one, with real drop shadows); card types with corner/hero icons, a `device_class`-driven
binary-sensor card, light brightness fill and colour; an FPS/CPU overlay.

**Five boards attached, all on v0.2.7:**

| Board | Port | Serial log |
|---|---|---|
| `CYD_S3_3248` | COM10 (native USB) | quiet - read it from HA instead |
| `WS_P4_5` | COM15 (CH343) | yes, UART0 |
| `WS_P4_7B` | COM6 (native USB) | - |
| `WS_P4_4B` | COM7 (CH343) | - |
| `WS_S3_4B` | COM8 (CH343) | no - `Serial` is native USB, not cabled |

Ports move when boards are re-plugged. **Identify a board by its USB serial**, not its COM number:
`Get-CimInstance Win32_PnPEntity | ? Name -match 'COM\d+' | select Name, DeviceID`. The P4_5 is
`...5B90154141`.

**Is a board alive?** Ask Home Assistant, not the cable: each board publishes
`sensor.<device>_uptime`, and its `last_updated` answers for the whole fleet at once.

## What is next — ROADMAP §7 "Running order"

**#58 screenshots is DONE and merged (2026-09-25).** `http://<board>/screenshot` gives a PNG of
exactly what the board shows. `python scripts/screenshot.py [host]` saves them to `screenshots/`.
**Use it to judge a layout before asking the owner for a photo.** Hostnames resolve on the owner's
network (`fleet-ws-p4-5` and so on). There is no auth; it is gated by `-D ENABLE_SCREENSHOT`.

1. **2.9 (#67), display stack: Arduino_GFX -> `esp_lcd`. IN PROGRESS, and the plan is written:
   `docs/design/display-stack.md`. Read all of it first.** Decisions are taken (raw `esp_lcd`;
   measure first; `WS_P4_5` first; one board at a time behind a build flag).
   **Step 1 is built** on `feat/67-bench` (2026-09-25), awaiting the owner's run of
   `docs/TEST_2.9.md`: `GET /bench` + `python scripts/bench.py [host]`, numbers in
   `display-stack.md` §8. Re-run it after every later step; that is what it is for. **Step 2 is
   next**, and the owner has given permission (2026-09-25) to fetch the libraries it needs
   (`waveshare/esp_lcd_hx8394`, the P4_5 BSP component, `esp_lvgl_adapter` into `reference/`).
   Two things from step 1 that the plan did not know: every board runs `AUTO_FLUSH = true`, so the
   cache write-back happens per chunk inside `draw16bitRGBBitmap()` and `gfx->flush()` does nothing
   on DSI/RGB; and `LV_USE_PPA` makes drawing *slower* (gate kept, off: `-D FLEET_LV_PPA`).
2. **2.10 (#65)** long-press popup groundwork - brightness/colour sliders, colour picker; pause
   moves into it. Tap stays a one-touch toggle.
3. **2.8 (#19)** header slots. 4. **2.11 (#66)** group cards. 5. **3.1 (#20)** schema.

The page overview and snapshot transitions (`pages.md` §6-7) come after 2.9: on the S3s they are
only worth building once drawing is fast.

**Loose ends, all small:**
- Three HA test entities (Avail / Reading / Refuse) sit at the top of the Fleet page, with three
  helpers in the owner's HA. Remove both when no longer wanted (`ExternalEntities_HA.h`,
  `Dashboard_Fleet.h`).
- #44's MQTT half is still open: nothing is both writable AND advertised yet.
- Ideas recorded, not scheduled: #52-#55; the owner's page ideas (linked pages, custom swipe
  targets, an alt-tab overview) are in `docs/design/pages.md`.

## Read in this order

1. `CLAUDE.md` — the HAL/BSP, the token and paint rules, **and the file-editing rule at the top**.
2. `docs/LESSONS.md` — before debugging anything.
3. `docs/design/cards.md` §13 — every card decision of 2.7, in rounds, and why.
4. `docs/design/pages.md` — the page model and what 2.6 did and did not build.
5. `docs/design/dashboard.md`, `card-layout.md` (before moving anything inside a card),
   `ha-websocket.md` (what HA's API gives us, measured), `tokens.md`, `startup.md`.
6. `docs/ROADMAP.md` §7.

## Environment state git cannot see

**The ESP32-P4 framework libraries are REBUILT, not stock.** `esp32p4_es` in
`~/.platformio/packages/framework-arduinoespressif32-libs/` carries `MEMPOOL_PREFER_SPIRAM` and a
64-byte L2 line - the fix for esp-hosted-mcu#243, the P4 WiFi dropouts (#49). The stock copy is
kept beside it as `esp32p4_es.stock.55.03.311`; rollback is a rename. A `pio pkg update` or a
platform reinstall silently puts the bug back. Procedure: `docs/REBUILD_P4_LIBS.md`.

**Do not use NINA's C6 updater** on these boards - it hung `WS_P4_5`. The C6 update is not needed.

## Things that will bite you

- **Build from PowerShell, not Git Bash** - pioarduino rejects MSYS shells.
- **`PYTHONIOENCODING=utf-8` before any `pio` whose output you pipe**, or it dies silently
  (LESSONS). A killed `pio` leaves orphaned `esptool`s holding the port.
- **Do not run two `pio` at once, and do not edit the tree while one builds** - it compiles your
  half-finished edit. Docs are safe to edit; source is not.
- **Clear `.pio/build_cache` after changing a struct's layout, a BSP header or `lv_conf.h`.**
  Stale objects against a changed struct corrupt memory rather than failing.
- **`pio run` with no `-e` builds ONE environment.** Before a merge, build all eight - but only
  then. While developing, build and flash just `CYD_S3_3248` and `WS_P4_5` (owner); the all-eight
  build takes ~25 minutes and blocks source edits while it runs.
- **A serial monitor resets the board when it opens.** Attach with `--rts 0 --dtr 0`. On
  `CYD_S3_3248`'s native USB port, even a pyserial open with RTS/DTR set false appeared to reset it
  (2026-09-24; not proven). Read that board through HA or `/screenshot`, not its port.
- **Prefer ESP-IDF facilities to Arduino-only ones** (owner): see CLAUDE.md. The HTTP server is
  `esp_http_server`, and its handlers run on their own task, so they must never touch LVGL.
- **`Edit` on a CRLF file**: deleting a line by matching a leading `\n` joins two lines. See
  CLAUDE.md; check `git diff` for a `+` line holding two statements.
- **HaProvider receives on the WEBSOCKET TASK, not `loop()`.** It must never touch LVGL; it writes
  through `EntityRegistry`'s mutex. Sends happen on the loop task only. HA request ids must strictly
  increase per connection, and a reconnect is a cold start (subscriptions are gone) -
  `ha-websocket.md`.
- **`staleAfterMs` is 0 on the HA entities on purpose**: `subscribe_trigger` fires on change, so
  silence means nothing. Death arrives as HA's word "unavailable" (`ST_UNAVAILABLE`).
- **LVGL clips children to their parent** - shadows, tags, anything that overhangs. Use
  `UI::unclipShadows()`; the flag alone is not enough (LESSONS).
- **Never toggle HIDDEN on a screen-sized object when an animation starts** - it redraws the whole
  screen in the first frame (LESSONS). Switch `CLICKABLE` instead.
- **Anything drawn on a panel stays ASCII, except `°`.**
- **Verify from outside the device** - the router, the broker, HA. It is the project's oldest rule.

## How to work with this owner

He is a hobbyist and an ESP32 enthusiast, not a professional developer, and explicit about that —
but he reads code, spots real bugs, and has caught several that were not obvious. **Treat his
instincts as data.** He is also a little colour-blind: never distinguish two states by colour alone.

**What works:**

- **Show, don't spec.** Build something he can react to; he flashes fast and tests thoroughly,
  reporting back test by test against a sheet. Write `docs/TEST_<milestone>.md` with PASS/FAIL
  criteria for anything he will judge on glass.
- **Plain language, not metaphor.** His words: "sometimes I get a little lost in the slang."
- **Give a recommendation, not a menu** - but ask before building on an assumption, and discuss a
  structural choice a turn before implementing it.
- **Read the tool, not the name.** Check the source before asserting how something behaves.
- **Own mistakes plainly and move on.** Report negative results as clearly as wins.
- **Tell him what has NOT been tested.** He acts on it immediately.
- **Structure long answers**: what was done, what it took, how it looks different, issues hit,
  what you are unsure of, caveats, test steps with pass/fail.
- **He is often right about the shape of a thing before he can name it.** "I'm wondering
  whether..." is usually a design instinct - engage with it.
- **He will solve your problem if you tell him what you are stuck on.** #49 ended because he found
  the upstream issue. Say plainly what is unexplained.

**What to avoid:**

- Don't commit straight to `main`. Feature branch, then `--no-ff` merge once he signs off.
- **Don't guess a fourth time.** Instrument it or ask the far end.
- **Don't claim a script worked because it printed something.** Assert the anchor, then grep the
  file. This cost three separate features on 2026-09-20.
- Don't say "we should wait until phase X" as a reflex, and don't trust a browser mock - the glass
  decides.

**Versioning:** `A.B.C.D`, where **C is the milestone within the phase**. Tag on `main` at merge
when a milestone completes (`v0.2.7` = through 2.7), never during development. D is commits since
the tag; a dirty tree appends `+dirty`.

**The issue tracker is yours to manage**, and keeping it, HANDOFF and ROADMAP current is part of the
work, not a follow-up.
