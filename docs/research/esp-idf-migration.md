# Migrating from Arduino to native ESP-IDF — what it would take

**Status: research only, no decision.** Written 2026-09-10. Assumes the decision to explore
this seriously has already been made, per the brief. Every claim below is tagged **verified in
`<path>`** (read directly), **verified via web** (fetched/searched this session, source cited),
**believed** (confident but not directly checked this session), or **unknown**.

## 0. The one-paragraph answer

This project is closer to ESP-IDF than it looks. `AudioManager`, `bb_captouch_fork`,
`LVGL_Startup`'s buffer allocator, and most of `Fleet_MQTT`/`Fleet_Entities`/`Fleet_Providers`
already call native ESP-IDF APIs directly (`driver/i2s_std.h`, `driver/gpio.h`, `driver/i2c.h`,
`heap_caps_malloc`) rather than Arduino wrappers — Arduino is mostly present as `Serial`,
`Wire`, `pinMode`/`digitalWrite`, `WiFi`, `Preferences`, and **the entire display stack**
(`Arduino_GFX_Library`). That last one is the whole migration, cost-wise: display + touch is
~700 lines of glue that must be rewritten against `esp_lcd`; everything else is a few days each
of mechanical substitution. The realistic total is **6-10 focused sessions** (see §6), not a
rewrite. **Recommendation: do not do this now.** Finish Phase 2 (UI foundation) first — see §6.

---

## 1. Inventory — what replaces what

### 1.1 `components/` — our own code

| Component | Lines | ESP-IDF native equivalent | Survives? |
|---|---|---|---|
| **Fleet_Entities** | 615 | n/a — it's already portable C++ | **100%, verified in `components/Fleet_Entities/*.h,*.cpp`**: includes are only `<mutex>`, `<stdint.h>`, `<string.h>`, `<new>`. No Arduino, no ESP-IDF. Compiles under ESP-IDF's own toolchain (and its Linux host target, §3) with zero changes. This is the ROADMAP Q9/Q8 bet paying off exactly as designed. |
| **Fleet_MQTT** | 710 | `esp-mqtt` (`esp_mqtt_client_*`) | **~60-70%.** Verified in `MqttManager.h/.cpp`: depends on `WiFiClient` + `PubSubClient` for the transport, but the actual value — backoff, LWT policy, reconnect state machine, HA discovery topic building — is independent logic that doesn't reference PubSubClient's API beyond publish/subscribe/callback, so it re-hosts onto `esp-mqtt`'s event-loop model (`MQTT_EVENT_CONNECTED`/`_DATA` etc.) without a redesign. `esp-mqtt` is arguably a better fit — it has native TLS and QoS2, which PubSubClient lacks. |
| **Fleet_Connectivity** | 1769 | `esp_wifi` + `esp_netif` + `esp_event` | **~50%.** Verified in `ConnectivityManager.h/.cpp`: the state machine, `classifyDisconnect()`, the AP retry ladder, proven/unproven credential tracking, and the four `ConnMode` values are all pure logic keyed off `wifi_err_reason_t` codes and timers — these numbers and the decision tree carry over unchanged. What's replaced is the plumbing: Arduino `WiFi.h` event registration → `esp_event_handler_register(WIFI_EVENT, ...)`; `Preferences` (NVS wrapper) → raw `nvs_get_str`/`nvs_set_str`; `IPAddress` → `esp_netif_ip_info_t`. See Report 2/§3 for the deeper esp-adf `wifi_service` comparison, which is not a shortcut here (it doesn't do more than this file already does). |
| **Fleet_Providers** | 909 | n/a (application logic) | **~80%.** Verified: `ArduinoJson` (used in `MqttProvider.cpp`) is explicitly framework-agnostic — "it doesn't even depend on Arduino" (arduinojson.org, verified via web search) — so it ports unchanged. The `Arduino.h` includes in `HaPublisher.h`/`SystemProvider.h`/`MqttProvider.h` are there only for `String`/`millis()`/basic types; trivial to remove. |
| **AudioManager** | 2440 | `esp_codec_dev` (optional) + native `driver/i2s_std.h`/`i2s_tdm.h` (already used) | **~85%, verified in `AudioManager.cpp`.** This file already **is** ESP-IDF code: `i2s_new_channel`, `i2s_channel_init_std_mode`/`_tdm_mode`, `i2s_channel_read`/`write`, `i2s_channel_enable` are all native calls, not Arduino I2S. `es7210.cpp`/`es8311.cpp` (CLAUDE.md: adapted from Espressif reference drivers) call ESP-IDF's `i2c_master`/`driver/i2c.h`-style register I/O already. Arduino-only surface in this file: `Wire.begin()` (→ `i2c_master_bus_add_device` or via `FleetI2C`'s existing abstraction), `Serial.print*` (→ `ESP_LOGI`), `pinMode`/`digitalWrite` (→ `gpio_set_direction`/`gpio_set_level`), and the Arduino `map()` macro (→ one inline function). A day's work, not a rewrite. Whether to also adopt `esp_codec_dev` is a separate, optional decision — see Report 2 §1: it's Espressif's current-recommended direction (their own `esp-bsp` ES8311 driver is deprecated in its favor) but would mean handing I2S ownership to its data-path layer, a real integration cost against the existing 4-channel TDM/AEC path. Not required for the migration itself. |
| **DisplayManager** | 441 | `esp_lcd` (`esp_lcd_panel_io_*`, `esp_lcd_panel_*`) + `esp_lvgl_port` or `esp_lvgl_adapter` | **~15%. The single biggest item.** Verified in `DisplayManager.h`: built entirely on `Arduino_GFX_Library` (`Arduino_DataBus`, `Arduino_GFX`, `Arduino_Canvas`, `Arduino_DSI_Display`, `Arduino_ESP32DSIPanel` per CLAUDE.md) — an Arduino-only library family with no ESP-IDF counterpart. Native ESP-IDF equivalent is `esp_lcd` (panel IO + panel drivers per interface: RGB, DSI, QSPI custom) fed into `esp_lvgl_port`/`esp_lvgl_adapter` (verified referenced in `docs/design/startup.md` — Waveshare's newer P4 BSPs already use `esp_lvgl_adapter` ~0.6.x upstream; older boards use `esp_lvgl_port`). What survives: the *numbers* — every pin, timing, and init-command sequence in `Fleet_BSP`'s `DisplayConfig` instances is hardware fact, not Arduino_GFX API surface, and re-targets to `esp_lcd_panel_dev_config_t`/DSI/RGB config structs directly. What doesn't: the QSPI software-rotation `Arduino_Canvas` wrapper (CLAUDE.md) has no `esp_lcd` equivalent — that logic (or LVGL's own software rotation) has to be reimplemented. |
| **TouchManager** | 269 | `esp_lcd_touch` (component family, e.g. `esp_lcd_touch_gt911`) or keep `bb_captouch_fork` as-is | **~90%, verified in `TouchManager.h`/`bb_captouch_fork/src/bb_captouch.cpp`.** `bb_captouch_fork` already `#include`s `driver/gpio.h` and `driver/i2c.h` directly, and its Linux port target (`bb_captouch_fork/Linux/Makefile`, confirmed present) proves the author already treats Arduino as one of several backends, not the foundation. Since Phase 0 it's vendored (not tracked upstream), so there's no "diverged from upstream" tax to porting it further. Only `<Arduino.h>`/`<Wire.h>` in the header need removing. `TouchManager`'s own logic (debounce, `mapCoordinates()`, the `WS_P4_7B` passthrough special-case from CLAUDE.md) is board-state logic, framework-independent. |
| **FleetI2C** | 343 | `i2c_master` (new-style ESP-IDF I2C driver, "i2c-ng") | **~40%.** Verified in `FleetI2C.h`: it's an Arduino-`Wire`-shaped API (`beginTransmission`/`write`/`endTransmission`/`requestFrom`) wrapping three backends. Its own header states the default Wire backend is itself "built on ESP-IDF's newer i2c-ng driver generation," and the alternate `I2C_BACKEND_LEGACY` already calls `driver/i2c.h` directly. So the *bus programming* is already ESP-IDF; only the `Wire`-mimicking method-call shape goes away in favor of `i2c_master_bus_handle_t`/`i2c_master_dev_handle_t`, a smaller and arguably cleaner API. All call sites (`DisplayManager`, `TouchManager`, `AudioManager`, `CH422G`) go through this one class, so the rewrite is contained. |
| **CH422G** | 94 | none needed — trivial I2C register driver | **~90%.** Verified in `CH422G.h`: hand-rolled, four fixed pseudo-addresses, no Arduino API beyond `Wire`. Ports in under an hour once `FleetI2C` is ported. |
| **Fleet_BSP** | 2013 (headers) | plain structs — no equivalent needed | **Data: 100%. Types: ~90%.** Verified in `Fleet_BSP.h`: every field is a primitive (`const char*`, `int8_t`, `uint8_t`, `uint32_t`) or the `lcd_init_cmd_t` array — pure hardware facts, framework-agnostic by construction. The only Arduino coupling is the `#include <Arduino.h>` / `<Arduino_GFX_Library.h>` at the top of `Fleet_BSP.h`, needed for (a) basic integer types Arduino.h happens to pull in and (b) the P4-only `lcd_init_cmd_t` typedef, which the file *already* declares its own fallback for on non-P4 (see CLAUDE.md) — so removing the Arduino_GFX dependency here is a one-line typedef change, not a redesign. All 8 `BSP_<NAME>.h` files' actual *data* (every pin, resolution, timing, init sequence) is untouched. |
| **GFX_Library_for_Arduino** (submodule, fork) | 353 files | `esp_lcd` (replaces it) | **0% — retired, not ported.** This is Arduino-only by design (the name says so). It's the thing `esp_lcd` + `esp_lvgl_port`/`esp_lvgl_adapter` replace, not port. |
| **bb_captouch_fork** | 11 files | keep, or `esp_lcd_touch_*` | See TouchManager row. Vendored (not a submodule) since Phase 0, so no upstream-sync cost either way. |
| **lvgl** (submodule) | n/a | unchanged | **100%.** LVGL is portable C; verified in `components/lv_conf.h` that the only Arduino-integration options (`LV_USE_FS_ARDUINO_ESP_LITTLEFS`, `LV_USE_FS_ARDUINO_SD`) are both `0` (disabled) already. Nothing to change. |

### 1.2 `src/` — application layer

Verified in `src/main.cpp` (73 lines) and `docs/design/startup.md`: the five-way split
(`main` / `SystemCore` / `SystemReport` / `LVGL_Startup` / `GUIManager`) already isolates
Arduino to a thin seam. `main.cpp`'s only Arduino calls are `Serial.begin/println`, `delay()`,
and the two hardware-init calls — this maps directly onto ESP-IDF's `app_main()` (`Serial` →
`ESP_LOGI`/UART driver, `delay()` → `vTaskDelay()`, `setup()`+`loop()` → `app_main()` with a
`while(1)` or a dedicated FreeRTOS task). `SystemCore`/`SystemReport` already exclude LVGL by
rule (CLAUDE.md); they also turn out to mostly exclude *display* framework calls too, since
hardware bring-up is delegated to the manager classes covered in §1.1. `LVGL_Startup.cpp`
(201 lines) already allocates its draw buffers with `heap_caps_malloc(..., MALLOC_CAP_DMA |
MALLOC_CAP_SPIRAM/INTERNAL)` — native ESP-IDF heap API, verified in `LVGL_Startup.cpp` — so only
the flush callback (currently calling into `Arduino_GFX`) and the touch-read callback change.
`GUIManager.cpp` and the `Panel_*`/`Widget_*`/`UIToolkit` files are pure LVGL widget-tree code
with no Arduino calls beyond incidental `String`/`millis()` — low-risk, high-volume mechanical
work (this is most of the ~40% of "40% survives / 40% moves / 20% replaced" UI estimate already
recorded in ROADMAP §4.3 for a *different* refactor, and it applies again here).

**Net picture:** of roughly 9,600 lines across `components/` (excluding vendored `lvgl` and
`GFX_Library_for_Arduino`), a rough weighted estimate is **60-70% survives with no logic
change**, ~20% needs mechanical API substitution (I2C, GPIO, logging, NVS, WiFi events), and
~10-15% (display + touch driver plumbing) needs a genuine rewrite against `esp_lcd`.

---

## 2. What we would gain

- **Component manager** (`idf_component.yml` + the ESP Component Registry, components.espressif.com). Verified locally: `C:\Users\Marge\.platformio\platforms\espressif32\examples\espidf-arduino-matter-light\main\idf_component.yml` exists and pulls versioned components declaratively — closer to `npm`/`pip` than PlatformIO's `lib_deps` symlink hack this project currently needs (CLAUDE.md's "hardcodes this machine's path" problem). This is a real, if modest, win independent of voice features.
- **`menuconfig`** — fine-grained control over FreeRTOS tick rate, heap allocation strategy, WiFi buffer counts, and (relevant to a memory-constrained board like `CYD_S3_3248`) exact internal-SRAM vs PSRAM placement policy, instead of the current per-board `#ifdef`/build-flag pile.
- **`esp_lvgl_port` / `esp_lvgl_adapter`** — Espressif-maintained LVGL porting layers with a proper `lvgl_port_lock()`/`unlock()` and a dedicated LVGL task. Directly relevant to `docs/design/startup.md` §5's decision to keep `LV_OS_NONE` with no-op locks *specifically so this door stays open* — this is the payoff of that design choice, not a new idea.
- **Unit tests on host** — **partially already true, and migration doesn't unlock it so much as formalize it.** `Fleet_Entities` already compiles and (per ROADMAP 1.7) has been tested standalone with a plain host compiler — that's *not* an ESP-IDF feature, it's a consequence of the zero-dependency design. ESP-IDF additionally has an official (if still labeled "preview"/experimental) Linux host target — `idf.py --preview set-target linux` — verified via web search (docs.espressif.com host-apps.html, multiple ESP-IDF versions) that lets a whole component's ESP-IDF-facing code run under host GCC with GDB/Valgrind, not just framework-agnostic code. That's a genuine step up from today's "manually compile the zero-dep parts standalone," but it's explicitly a preview feature — expect rough edges.
- **PPA** (Picture Processing Accelerator, P4's 2D hardware block) — **not actually gated behind this migration.** Verified in `docs/REFERENCE_PROJECTS.md`: the `ESP32-P4-Allsky-Display` reference project runs a working `PPAAccelerator` class over `driver/ppa.h` **from Arduino framework**, because arduino-esp32 3.x exposes ESP-IDF driver headers directly. Migrating to ESP-IDF would not newly unlock PPA; it's already reachable. (Using it for actual LVGL framebuffer rotation, vs. that project's image scale/rotate use case, is unexplored either way — see CLAUDE.md's note that DSI/QSPI rotation is still CPU-only today.)
- **`esp-adf` / `esp-sr` access** — this is the real, migration-relevant gain, covered fully in Report 2. Short version: both are ESP-IDF-first; using them from Arduino means either the "Arduino as IDF component" pattern (§4) or hoping their headers happen to compile under arduino-esp32 (unverified, likely fragile for anything beyond individual .h/.c files).
- **Native TLS/mTLS via `esp-mqtt` and `esp_http_client`** — relevant if a cloud voice path (Report 2 §5b) is ever built; `esp-mqtt` has built-in TLS with certificate bundles, more mature than PubSubClient+WiFiClientSecure.

## 3. What we would lose — Arduino libraries currently in use, and replacement effort

From `platformio.ini` (`[common]` lib_deps plus per-env additions), verified:

| Library | Used for | Replacement | Effort |
|---|---|---|---|
| `GFX_Library_for_Arduino` (fork) | All display output | `esp_lcd` + per-panel driver components + `esp_lvgl_port`/`esp_lvgl_adapter` | **Large.** This is the migration's critical path — see §1.1. Three different panel families (RGB, QSPI/AXS15231B, MIPI-DSI/HX8394) each need their own `esp_lcd` driver, several of which (AXS15231B in particular) may not have an off-the-shelf ESP-IDF component and would need porting from the fork's own driver code. |
| `bb_captouch_fork` | Touch (GT911, AXS15231B-integrated, etc.) | Keep as-is (already ESP-IDF-flavored) or `esp_lcd_touch_*` components | **Small**, per §1.1. |
| `knolleary/PubSubClient` | MQTT transport | `esp-mqtt` | **Medium.** API shape is different (event-loop vs. callback-per-loop-call) but `MqttManager`'s actual state machine is transport-agnostic already (§1.1). |
| `bblanchon/ArduinoJson` | HA discovery payload, entity JSON | Keep — verified framework-agnostic | **None.** |
| Arduino `WiFi.h` | STA/AP, scanning | `esp_wifi` + `esp_netif` + `esp_event` | **Medium-large.** Most WiFi logic in `ConnectivityManager` is already reason-code/timer driven and event-shaped (see §1.1), but re-registering for raw `esp_event` (vs. Arduino's `WiFi.onEvent()` wrapper) and rebuilding scan-result handling against `esp_wifi_scan_get_ap_records()` is real work, and this is the file with the most edge-case logic in the whole codebase (proven/unproven, retry ladders, AP client deferral) to re-verify. |
| Arduino `Preferences` (NVS wrapper) | Connectivity + MQTT config persistence | raw `nvs_flash`/`nvs_get_*`/`nvs_set_*` | **Small.** Preferences is already a thin NVS wrapper; the key/value schema carries over unchanged. |
| Arduino `Wire` | I2C (via `FleetI2C`) | `i2c_master` | **Small-medium**, contained entirely inside `FleetI2C.cpp` per its own design (§1.1). |
| `SensorLib`, `XPowersLib` | Declared for `WS_S3_TOUCH_LCD_4B` (IMU/PMIC libs) | **Not currently used** | **N/A right now** — verified via grep across `src/`, `include/`, and `Fleet_BSP`: zero references to `SensorLib`/`XPowersLib`/`QMI8658`/`AXP2101` in application code today. They're declared `lib_deps` but never `#include`d — exactly the "SUCCESS can mean your library was never compiled" trap `docs/LESSONS.md` warns about. No migration cost exists yet because no code exists yet. |
| Arduino core basics (`Serial`, `pinMode`/`digitalWrite`, `millis()`/`delay()`, `String`) | Scattered everywhere | `ESP_LOG*`, `driver/gpio.h`, `esp_timer_get_time()`/`vTaskDelay`, `std::string`/`char[]` | **Large in surface area, trivial in difficulty.** This is the single most *frequent* change (touches nearly every file) but each instance is a mechanical, low-risk substitution — the kind of change a global search-and-replace plus a compile pass catches immediately, unlike the display rewrite which needs real design work. |

---

## 4. PlatformIO vs `idf.py` — can PlatformIO build `framework = espidf` for these boards?

**Yes, verified locally.** `C:\Users\Marge\.platformio\platforms\espressif32\platform.json` (the
exact pinned 55.03.311 install this project uses) declares both frameworks:

```json
"frameworks": {
  "arduino": { "script": "builder/frameworks/arduino.py" },
  "espidf":  { "package": "framework-espidf", "script": "builder/frameworks/espidf.py" }
}
```

`framework-espidf` resolves to `pioarduino/esp-idf` release **v5.5.5** (verified in the same
`platform.json`) — a current, non-ancient IDF version.

**Board-level support, verified by reading the installed board JSONs directly:**

| Board JSON | `frameworks` array |
|---|---|
| `esp32-p4-evboard.json` (all 4 P4 environments) | `["arduino", "espidf"]` — verified, also confirms `chip_variant: esp32p4_es` (the pre-rev3 silicon variant CLAUDE.md already documents) |
| `esp32-s3-devkitc1-n16r8.json` (most S3 environments) | `["arduino", "espidf"]` — verified |
| `boards/esp32-s3-n16r8-Guition_JC3248W535EN.json` (this repo's own vendored board file, for `CYD_S3_3248`) | `["arduino", "espidf"]` — verified |

All boards this fleet uses declare `espidf` support. **There is no board-level blocker.**

---

## 5. A phased migration path that never leaves the project unbuildable

**Yes — Arduino-as-an-IDF-component is real, and pioarduino exposes it two ways, both verified
locally/on GitHub this session:**

1. **pioarduino's own hybrid mode: `framework = arduino, espidf`.** Verified by reading
   `builder/frameworks/espidf.py` (search hits at lines 236, 933, 3011, 3106+) and, more
   concretely, by reading a real shipped example: `.platformio\platforms\espressif32\examples\
   espidf-arduino-blink\`. Its `platformio.ini` literally says `framework = arduino, espidf`;
   its `CMakeLists.txt` is a genuine ESP-IDF project root
   (`include($ENV{IDF_PATH}/tools/cmake/project.cmake)`); its `src/CMakeLists.txt` is
   `idf_component_register(SRCS "Blink.cpp")`; and `src/Blink.cpp` freely mixes
   `#include <Arduino.h>` with `#include <freertos/task.h>` and `#include <driver/gpio.h>` in
   the same file. This is a working, Espressif-adjacent-maintained example of exactly the
   stepping stone the brief asked about — not a theoretical capability.
2. **Espressif's own upstream path: arduino-esp32 published as an installable IDF component.**
   Verified via web search: `github.com/espressif/arduino-esp32/blob/master/idf_component.yml`
   exists, and the repo ships `idf_component_examples/hello_world/main/idf_component.yml`
   depending on `espressif/arduino-esp32`. This is the vendor-blessed, registry-distributed
   version of the same idea, independent of pioarduino.

**What this buys as a migration strategy:** convert one board environment at a time to
`framework = arduino, espidf` — this changes nothing about the C++ source, only the build
plumbing (a `CMakeLists.txt` per file/component instead of PlatformIO's automatic LDF) — and the
project keeps building and flashing identically throughout. Only *after* that scaffolding is in
place and proven on one board does replacing individual pieces (start with `FleetI2C`, since
everything depends on it and it's the smallest true rewrite) become a matter of swapping that
one component's implementation while everything else keeps compiling against `Arduino.h` as
before. Display (`DisplayManager`) is the last thing converted, specifically because it's the
biggest rewrite and the thing most boards can't run without.

**Caveats, both unknowns rather than confirmed problems:**
- Whether the hybrid mode's per-file CMakeLists requirement scales cleanly to this project's
  ~30-file, 12-component tree (vs. the single-file Blink example) is **unknown** — worth a
  half-day spike on one small component (`CH422G`, 94 lines) before committing the plan.
- Whether `esp-sr`/`esp-adf` components (Report 2) are known to build cleanly under pioarduino's
  hybrid mode specifically (vs. plain `idf.py`) is **unknown** — the examples found this session
  demonstrate the mechanism works, not that every third-party managed component is compatible.

---

## 6. Cost estimate and recommendation

**Estimate, in sessions (this project's own unit of work, per ROADMAP §2 — "one session, one
milestone"):**

| Phase | Work | Sessions |
|---|---|---|
| Spike | Hybrid-mode proof of concept on one small component + one board | 1 |
| `FleetI2C` → `i2c_master` | Smallest real rewrite, everything depends on it | 1 |
| `AudioManager`/`es7210`/`es8311`/`CH422G` → native calls | Mostly mechanical per §1.1 | 1 |
| `TouchManager`/`bb_captouch_fork` → drop Arduino header deps | Mostly mechanical per §1.1 | 1 |
| `Fleet_Connectivity` → `esp_wifi`/`esp_event`/NVS | The one with the most logic to re-verify | 1-2 |
| `Fleet_MQTT` → `esp-mqtt` | Medium | 1 |
| `DisplayManager` → `esp_lcd` + `esp_lvgl_port`/`adapter`, per panel family (RGB / QSPI / DSI) | The critical path | 2-3 |
| Fleet-wide re-verification (8 boards) | This project already treats "verified on 2 dev targets, spot-checked on the rest" as the bar (ROADMAP §Q11) | 1 |

**Total: roughly 9-12 sessions**, i.e. weeks of hobbyist time at the project's own historical
pace (Phase 0+1 together took about a week of sessions per the git log), **not** a rewrite —
but not a weekend either, and the display rewrite is genuinely hard, not just long.

**Recommendation on timing: not now.** Three independent reasons, all already implicit in this
project's own decisions:

1. **ROADMAP Q8 already answered this "not yet, but design for it."** The constraint it imposes
   — keep `Fleet_Entities`/registry/grid/build-sheet Arduino-free — is already being honored and
   is exactly what keeps this migration's blast radius contained to the HAL layer rather than
   the whole dashboard. Nothing about Phase 2 (UI foundation, in progress) makes migration
   harder later; if anything, finishing the card library on the current stack means fewer files
   are mid-refactor when a migration eventually happens.
2. **The single biggest migration cost (display/`esp_lcd`) and the single biggest Phase 2 cost
   (design system, card base class) are unrelated work on the same files.** Doing both at once
   multiplies risk for no benefit; sequencing them serially is strictly cheaper in review effort.
3. **This project's own prior note is the same conclusion, independently reached.**
   `docs/FUTURE_IMPROVEMENTS.md` already states that if voice work resumes, "the plan is a full
   ESP-IDF rewrite (separate project) ... not continuing to build AEC out here" — i.e. the
   owner has already implicitly decided migration is a *voice-feature* decision, not a
   *dashboard* decision. That argues for revisiting this analysis when voice work is actually
   scheduled (see Report 2), not against the current Phase 2/3/4 roadmap.

**When it would make sense:** immediately before starting real voice-assistant work, if that
work turns out to need `esp-sr`/`esp-adf` (see Report 2) badly enough to justify it — and even
then, §5's phased path means it does not have to be an all-or-nothing decision made in one
sitting.
