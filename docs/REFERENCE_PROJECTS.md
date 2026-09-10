# Reference Projects — what is in `reference/`, and what to mine from it

Two projects by the same author (chvvkumar), plus `ha-dashboard` by Tommzn. All are local-only
(`reference/` is gitignored). This file maps their contents onto our milestones so the material
gets used deliberately rather than rediscovered.

See `CLAUDE.md` for our architecture and `docs/ROADMAP.md` for the plan. This file is about
other projects' code and what it teaches.

---

## ⚠️ Licensing — settle this before reusing any code

| Project | Licence | What we may do |
|---|---|---|
| `ha-dashboard` (Tommzn) | **MIT** | Reuse with attribution |
| `esphome-modular-lvgl-buttons` (Andrew Gillis) | **MIT** | Reuse with attribution |
| `ESP32-P4-NINA-Display` (chvvkumar) | **none** | Read only |
| `ESP32-P4-Allsky-Display` (chvvkumar) | **none** | Read only |
| `espcontrol` | **PolyForm Noncommercial 1.0.0** | Read only — see below |

**`espcontrol` needs its own warning.** PolyForm Noncommercial is not a permissive licence and is
not "MIT with a nag". It permits use of *the software* for noncommercial purposes only, and grants
nothing resembling the right to copy its source into another project. Two consequences:

- **Do not copy code from it, in any amount, adapted or otherwise.** Same rule as the two
  unlicensed chvvkumar repos, for a different legal reason.
- **It constrains us further than "none" does in one respect**: an explicit noncommercial term is
  a deliberate restriction by the author rather than an oversight, so there is no point asking for
  a licence clarification the way there is with chvvkumar's repos.

Architecture, layout decisions and *facts about hardware* are not copyrightable, and those are why
it is here — its per-device grid dimensions are cited in `docs/design/cards.md` §7.

"No licence" is not "public domain" — under default copyright it means **all rights reserved**.
Public on GitHub grants no reuse rights. Neither chvvkumar repo has a LICENSE file, a README
licence section, or copyright headers in source, so **one question to the author covers both**.
Tracked as a GitHub issue.

Until that is answered:

- **Do** read them, study the architecture, and adopt *approaches and decisions* — those are not
  copyrightable.
- **Do not** copy source, even adapted. Not into our tree, not as a starting point.
- **Fonts are separate.** NINA's converted `lv_font_*.c` faces (Playfair, Bodoni, Saira, Hanken,
  Overpass, Montserrat, Stencil) are mostly Google Fonts under the SIL OFL and we may use those
  faces — but source and convert them **ourselves from the foundry**, never lift the converted
  `.c` out of an unlicensed repo.

---

## `ESP32-P4-Allsky-Display` — the closer match, and the surprise

**This is an Arduino sketch** (`ESP32-P4-Allsky-Display.ino` plus `.cpp`/`.h`), not ESP-IDF.
Same framework as us, same P4 silicon, same Waveshare panels. Its module layout mirrors ours
almost one-for-one, which makes it far more directly applicable than NINA despite being the
older and less polished of the two.

| Their file | Our counterpart / milestone |
|---|---|
| `network_manager.cpp/h` (`WiFiManager` class) | `Fleet_Connectivity` — **ours is ahead**, see below |
| `captive_portal.cpp/h` | **#6** — captive portal, in Arduino, on P4 |
| `wifi_qr_code.h` | #6 — join-the-AP QR code, a UX touch we had not considered |
| `mqtt_manager.cpp/h` | **#9** — MQTT in Arduino C++, not ESP-IDF `esp-mqtt` |
| `ha_discovery.cpp/h`, `ha_rest_client.cpp/h` | **#11** — HA discovery, plus a REST path as an alternative to MQTT |
| `web_config.cpp/h`, `web_config_api.cpp`, `web_config_pages.cpp` | **#27** — web config page |
| `ota_manager.cpp/h` | **#29** — OTA |
| `config_storage.cpp/h`, `config_backup.cpp/h` | Settings layering (ROADMAP Q2); NVS persistence |
| `ppa_accelerator.cpp/h` | The PPA item in `FUTURE_IMPROVEMENTS.md` — see below |
| `display_manager.cpp/h`, `displays_config.cpp/h` | `DisplayManager` plus our BSP |
| `gt911.cpp/h`, `touch.cpp/h`, `i2c.cpp/h` | `TouchManager`, `FleetI2C` |
| `crash_logger.cpp/h`, `device_health.cpp/h`, `system_monitor.cpp/h`, `watchdog_scope.h` | Nothing yet — diagnostics we do not have |
| `task_retry_handler.cpp/h` | Generic retry/backoff, reusable shape |

### Where we are already ahead

`network_manager.h`'s `WiFiManager` is a non-blocking connect with a 2 s to 60 s backoff and an
attempt counter. That is roughly where we were several commits ago. Our `ConnectivityManager`
additionally has disconnect-reason classification, the proven/unproven distinction, four
selectable modes, AP-client deferral, and the environmental ladder — so **do not treat their
connectivity as an upgrade path.** It is useful as a cross-check on known-good Arduino WiFi
setup for P4, which is what it was originally consulted for.

### `ppa_accelerator.h` — the one to study closely

`FUTURE_IMPROVEMENTS.md` carries "True LVGL+PPA hardware-accelerated rotation" as an
if-it-ever-becomes-a-problem item, noting the P4 has a Pixel Processing Accelerator that our
CPU-based per-pixel rotation ignores. **This is a working Arduino-framework PPA implementation
on our exact silicon.**

A `PPAAccelerator` class over `driver/ppa.h` offering scale, and scale+rotate, including a
zero-copy variant. The important part is the constraints it documents:

- Buffers **must be 64-byte aligned** and allocated `MALLOC_CAP_DMA | MALLOC_CAP_SPIRAM`.
- It includes `esp_cache.h` — PSRAM DMA needs explicit cache writeback/invalidate around the
  operation. That is the subtle part, and getting it wrong yields corrupt or stale pixels rather
  than a clean failure.
- The zero-copy path exists specifically to remove two full-buffer `memcpy`s per image.

**Caveat: they use PPA for image scale/rotate, not for LVGL framebuffer rotation.** Our use case
differs, so the code would not transfer even if licensed. What transfers is the API usage, the
alignment and capability requirements, and the cache discipline.

---

## `ESP32-P4-NINA-Display` — the more evolved one (ESP-IDF, C)

Patterns transfer; code mostly does not, being ESP-IDF C against our Arduino C++.

### The page/view/grid back-end — the most valuable part for our actual purpose

`main/ui/page_registry.h` is the piece to read first.

**The FROZEN-IDS / APPEND-ONLY contract.** Every page has a stable numeric id *and* a stable
string slug. Ids are persisted in NVS (slideshow stop lists, idle-page target, home-page
override), so the header states the rule outright: never renumber, reorder, or remove an id; a
new page takes the next free id; a retired id is **reserved forever and never reused**. They
learned this concretely — id 12 is a documented tombstone from an image-pages split.

**This is the single most important lesson available for our build sheet (#20).** Our sheets will
persist page and card references too, and the same mistake would silently re-point a user's saved
dashboard at a different page after a refactor. Slug = stable external key, id = stable internal
key, both permanent.

**Identity table and behaviour table are separate.** `page_ref_entry_t` is frozen metadata with
no function pointers. `page_ops_t` (create / destroy / get_obj / show / hide / apply_theme /
is_available) is an **opt-in** registration keyed by page id, so generic dispatch works without
hardcoded branches — and pages that have not registered keep flowing through the old branches.
That is a deliberate incremental migration path, not a big-bang refactor. Worth copying as an
approach for #16 and #17.

**Struct-evolution discipline**, directly parallel to our BSP designated-initializer rule: an
`icon` field is *reserved but deliberately not added*, with a note to append it as a trailing
member only when actually needed, so existing initializers stay valid.

**Lock discipline is documented per function**, not assumed — "caller must NOT hold the LVGL
lock; the arbiter takes it internally" versus "all ops are called with the display lock already
held". Given ROADMAP §4.2 calls threading our biggest trap, this per-API convention is worth
adopting wholesale.

### `nina_nav_arbiter.h` — navigation as a resolution ladder

ROADMAP §5.2 flags gesture conflicts as a risk. Their answer is a **single owner of page-commit
decisions**. Nothing navigates directly; sources submit claims, and one `resolve()` per cycle
commits at most one page change. The ladder is ordered: `BOOT`, `USER` (within a grace window),
`SLIDESHOW`, `SESSION`, `IDLE`, `DEFAULT`, `HOLD` (modal open, freeze), `HOME_LOCK`.

Details worth stealing: a **USER grace window** so auto-rotation cannot yank a page out from
under someone who just swiped; modal open/close freezing and then *restamping* the grace window
so the page does not jump when a dialog closes; and a runtime-only **pin** that holds a page with
no expiry and resets on reboot.

### `mqtt_ha.c` — see #9

Full write-up on that issue. Headlines: ignore retained payloads on *command* topics (a retained
reboot command boot-loops the device); publish `offline` explicitly on graceful shutdown, because
LWT only fires on an ungraceful drop; disable the library's auto-reconnect and own the backoff.

Also the claim behind #41: *NVS writes run with the CPU cache disabled, which can starve the
esp-hosted SDIO transport* — unverified for us, and it affects four of our eight boards.

### `clock_dial.h` and the clock pages — see #35

Seven named editorial layouts, each with a fixed palette deliberately independent of the global
theme. Geometry factored into a pure, header-only, host-tested file with no LVGL or ESP-IDF
dependency — a practice worth adopting generally.

### Scope worth noticing

Its `main/` also carries on-device settings, a full web UI with HTML fragments per settings page,
OTA from GitHub releases, telemetry, crash logging, log capture, and per-board profiles
(`board_detect.c`, `board_profile.c`) — while its *stated* purpose is a NINA astrophotography
client. The breadth is the point: one author, one panel family, all of Phases 4 through 6 already
built.

---

## `esphome-modular-lvgl-buttons` (Andrew Gillis) — MIT, ESPHome + LVGL

Added 2026-09-10. **The only reference project whose structure maps onto ours almost exactly, and
one we may legally borrow from.**

Its `ui/<type>/` layout gives every entity type three files:

```
ui/<type>/local.yaml    tile for a component on the same device
ui/<type>/remote.yaml   tile for a Home Assistant entity
ui/<type>/detail.yaml   full-screen detail page (complex types only)
```

That is our card library plus our context sheets, arrived at independently — and the
local/remote split is our `advertise` flag by another name (entities we own vs. entities someone
else owns). Worth reading `ARCHITECTURE.md` before finalising the `Card` base class in #15,
specifically for how it decides which types get a detail page and which do not.

Also relevant: the README claims support up to a 4×7 grid on the `WS_P4_7B`. **No confirmed
screenshots have been found**, so treat that as a claim about what the layout system permits, not
evidence that it is legible.

## `espcontrol` — PolyForm Noncommercial, ESPHome, and the density evidence

Added 2026-09-10. Deliberately plain, and effective *because* it is plain: at a glance you can see
what is happening. Its value to us is the per-device grid definitions in
`devices/*/device/lvgl.yaml`, which are real shipped numbers rather than estimates:

| Their device | Resolution | Grid |
|---|---|---|
| `guition-esp32-p4-jc1060p470` | 1024×600 | **5×3** |
| `guition-esp32-p4-jc8012p4a1` | 1280×800 | **5×4** |
| `esp32-p4-86` | 480×480 | 3×3 |
| `guition-esp32-s3-4848s040` | 480×480 | 3×3 |
| `guition-esp32-p4-jc4880p443` | 480×480 | 2×3 |

`jc1060p470` is **our `CYD_P4_1060`** — the same panel, so 5×3 on that board is confirmed rather
than guessed. And 5×4 at 1280×800 makes the owner's 5×3-or-5×4 estimate for `WS_P4_5` (1280×720)
well supported.

See the licence warning above before reading its source with intent.

## `ha-dashboard` (Tommzn) — MIT, smaller, ESP-IDF

Component split: `wifi_manager`, `ha_client`, `settings`, `time_sync`, `display`, `ui`,
`web_server`. Less evolved than either chvvkumar project, but **the only one of the three we may
actually reuse code from.** Worth checking here first whenever a needed pattern exists in more
than one of them.
