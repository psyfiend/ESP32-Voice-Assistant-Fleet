# Startup design (issue #12)

**Status: implemented.** `SystemCore::begin()` and `LVGL_Startup` are the code; this document
gives the reasoning.

What this doc settles: which file owns what at boot, the order things start in *and why*, and
where the LVGL boundary sits. The split moved code and reversed dependency arrows without changing
behaviour; a board that behaves differently from before the split has a bug.

---

## 1. The problem the split solved

Startup used to live in one file, `src/LVGL_Test_UI.cpp` (459 lines), doing five unrelated jobs.
In this section and §3, "currently" and "today" describe that file, not the present code:

| Lines | Job | Belongs to |
|---|---|---|
| 34–56 | Declare 10 subsystem globals | a composition root |
| 83–286 | `debug_dump_config()` — the System Doctor | diagnostics |
| 296–320 | Bring up hardware + the data layer, in a load-bearing order expressed only by line order | hardware startup |
| 322–410 | Build the LVGL screen: root, decks, header, panels, z-order | screen content |
| 419–459 | `loop()` pumping nine things | both |

And two dependency arrows point the wrong way:

- `GUIManager` **owns the hardware** — `DisplayManager displayMgr; TouchManager touchMgr;` are
  public members ([include/GUIManager.h:11](../../include/GUIManager.h)). `Panel_Display` reaches
  through it for brightness and raw touch. Every reference implementation puts hardware *below*
  the LVGL layer and passes handles up.
- `Panel_System.cpp:4` declares `extern void debug_dump_config(bool)` — a UI file calling back
  into `main`. That single line is what would stop a GUI-less build from linking.

Neither was a bug, and both get expensive once cards exist.

---

## 2. What the reference implementations do

Verified in `reference/`, not recalled:

| Source | Shape |
|---|---|
| `WaveShare-S3-Touch-LCD-5B/Arduino/examples/09_lvgl_Porting/` | `board->begin()` → `lvgl_port_init(lcd, tp)` → build UI. Engine is its own file whose entire API is `init` / `deinit` / `lock` / `unlock`. **The port receives the initialized LCD and touch; it does not own them.** |
| `Waveshare-P4-WIFI6-Touch-LCD-7B/examples/BSP/include/esp32_p4_wifi6_touch_lcd_7b.h` | Same split behind `bsp_i2c_init()` / `bsp_display_start()` → `lv_display_t*` / `bsp_display_lock()`. The matching `main.c` is 8 lines. |
| `Waveshare-P4-WIFI6-Touch-LCD-5/examples/arduino/examples/04_LVGLV9_Arduino/` | Arduino_GFX + `lv_display_create` + `lv_display_set_buffers` + `loop(){ lv_timer_handler(); delay(5); }`. Closest to our stack; confirms our engine code is idiomatic — just not separated. |

All three agree on the layering. They disagree on threading, which §5 settles.

---

## 3. The five files

```
src/main.cpp          setup() and loop(). Nothing else. ~50 lines.
src/SystemCore.*      Owns every non-UI subsystem. begin() encodes the start order
                      with a written reason per step. loop() pumps them. No LVGL.
src/SystemReport.*    The System Doctor, writing to a sink. No LVGL.
src/LVGL_Startup.*    LVGL engine plumbing. Receives DisplayManager& + TouchManager&.
src/GUIManager.*      Screen content only. Name finally matches the file.
```

`src/LVGL_Test_UI.cpp` is renamed to `src/main.cpp`. The name has been wrong since the file
stopped being a test.

### 3.1 `SystemCore`

Holds, in one place, what is currently ten file-scope globals: `displayMgr`, `touchMgr`,
`audioMgr`, `connMgr`, `mqttMgr`, `entities`, `sysProvider`, `haPub`, `mqttProv`. Exposes each
by accessor (`core.display()`, `core.entities()`, …).

**Why a container rather than tidier globals:** the ordering in §4 is real — `mqttMgr` needs the
link, `haPub` needs a populated registry — and today it survives only because nobody reorders
those lines. Putting it in one function with a comment per step makes the constraint reviewable.
It also means `GUIManager::begin(SystemCore&)` takes one argument instead of six, and keeps
taking one when Phase 2.7's cards need the registry.

**`SystemCore` never includes an LVGL header.** Not a build-flag rule, just a fact about what is
in the file — which is what makes a GUI-less variant cheap later (§7).

### 3.2 `LVGL_Startup`

Everything currently in `GUIManager.cpp` except `UIToolkit::init()`: `lv_init()`, the tick and
log callbacks, the buffer-allocation matrix, `my_disp_flush`, `my_touch_read`, and display +
indev registration.

```cpp
namespace LVGL_Startup {
    bool begin(DisplayManager &display, TouchManager &touch);
    void tick();                    // lv_timer_handler()
    bool lock(int timeout_ms = -1); // see §5
    void unlock();
    lv_display_t *display();
    lv_indev_t   *indev();
}
```

Signature deliberately mirrors `lvgl_port_init(lcd, tp)` and `bsp_display_start()`: hardware is
initialized by `SystemCore` first and handed in. `LVGL_Startup` borrows, never owns.

The flush and touch callbacks currently reach hardware via `lv_display_get_user_data()` casting
to `GUIManager*`. They will hold the two references directly instead — same indirection, one
fewer layer, and it stops `GUIManager` from being on the render path at all.

`UIToolkit::init()` moves out to `GUIManager::begin()`, where it belongs: it builds styles and
the toast layer, which is design system, not engine. That is also the seam milestone 2.2 lands on.

### 3.3 `GUIManager`

What the name always implied: root screen, the two decks, the z-order sandwich, header, the three
panels, and their `tick()`s. `GUIManager::update()` becomes `GUIManager::tick()` and pumps only
UI (`header`, `pnlDisplay`, `pnlAudio`); `lv_timer_handler()` is `LVGL_Startup::tick()`'s job.

`Panel_Display`'s constructor changes from `GUIManager&` to `(DisplayManager&, TouchManager&)` —
the two things it actually uses. Narrower than passing `SystemCore&`, and it keeps a panel from
growing a habit of reaching for whatever it likes.

### 3.4 `SystemReport` — the System Doctor

Today's `debug_dump_config()` writes into `pnlSystem.log()` and is called back from the UI by
`extern`. Inverted:

```cpp
namespace SystemReport {
    using Sink = void (*)(const char *line);
    void addSink(Sink s);            // Serial sink registered by default
    void addSection(const char *name, void (*fill)(void));  // GUI adds [UI STATE]
    void run(SystemCore &core, bool echoSerial);
}
```

- Default sink is Serial, so the report exists and works with no GUI present.
- `Panel_System` registers *itself* as a second sink at `init()`. The `extern` disappears and the
  arrow points UI → diagnostics, which is the right way round.
- `[UI STATE]` (the one genuinely LVGL-dependent section) becomes a section the GUI registers,
  not a hardcoded `#ifdef`.
- The "Dump Config" button calls `SystemReport::run(...)` through a callback `GUIManager`
  registers, not through a linker-level `extern`.

This is the shape you described wanting: default serial output now, the same content available to
a Settings → System Info page later, and a button that re-runs it to serial on demand. When that
page arrives in Phase 4 it registers a sink and gets the whole report for free — no second copy
of the formatting.

**Open, deliberately deferred:** whether each section should also be `DEBUG_<AREA>`-gated. Right
now the whole report runs once at boot and on demand. Gating individual sections is easy to add
once the Settings page exists and we can see which sections anyone actually reads.

### 3.5 Subfolders

```
src/                 main, SystemCore, SystemReport, LVGL_Startup, GUIManager, Settings, HttpServer
src/UI/              UIToolkit, tokens, panels, the component kit (UI/Kit/)
src/Cards/           Card base class, card types, the card window (Cards/Popup/)
src/Layout/          placement
include/UI/          matching headers
include/Cards/
include/Layout/
```

Directory names are `PascalCase` to match the files inside them, and `UI` keeps its capitals
under the project-wide rule.

- `build_src_filter = +<*>` recurses, so most environments need no change. An exclusion names the
  path: `WS_S3_TOUCH_LCD_5B` excludes `UI/Panel_Audio.cpp`.
- Subfolders of `include/` are **not** on the include path the way `include/` itself is, so an
  include names its path from `include/` (`"UI/Panel_Header.h"`).

---

## 4. Start order, with reasons

`SystemCore::begin()` is the source of truth for the order; the table gives the core steps and
each one's constraint. Steps the table folds together: the HTTP server, `PanelDebug` and `HangWatch`
start with the display; `Settings::begin()` runs after the registry storage and before the GUI; the
battery, virtual and HA providers (`_ha`, `_haRest`, `_haProv`, the `fleet` catalogue `_haCat`)
follow the other providers; the HA cards made on the board are adopted from the settings file
(`adoptHaCard`); the command router and HA history come last.

LVGL initialises after every non-UI subsystem (§4.1).

| # | Step | Why here |
|---|---|---|
| 1 | `FleetI2C::begin()` | Hoisted out of `DisplayManager::begin()`, where it is currently a side effect. Touch, audio and the expander all need the bus; only display happened to be first. Same instant in time, now named — mirrors Waveshare's separate `bsp_i2c_init()`. |
| 2 | `displayMgr.begin()` | Panel + backlight. Must precede LVGL, which needs `gfx->width()/height()` to size buffers. |
| 3 | `touchMgr.begin()` | Needs the I2C bus; some boards need the panel's reset line first. |
| 4 | `audioMgr.begin()` | Needs I2C for the codecs. `HAS_AUDIO_HW` only. |
| 5 | `connMgr.begin()` | Independent of display. Sets hostname before `WiFi.mode()` — a documented ordering trap inside `ConnectivityManager`, unchanged here. |
| 6 | `mqttMgr.begin(&connMgr)` | Needs the link object. Does nothing until it reports online. |
| 7 | Registry storage + `entities.begin()` | PSRAM allocation. Must precede any provider. |
| 8 | `sysProvider` / `haPub` / `mqttProv` `.begin()` | All need the registry; `haPub` and `mqttProv` also need `mqttMgr`. |
| 9 | Register `EXTERNAL_ENTITIES` | Must be after the registry and before `mqttProv` derives subscriptions from it. Temporary stand-in for the build sheet (#20). |

Then, back in `main.cpp` and only if a GUI is built:

| # | Step | Why here |
|---|---|---|
| 10 | `LVGL_Startup::begin(core.display(), core.touch())` | Needs both initialized. Allocates draw buffers — after the registry's PSRAM claim, so the existing internal-SRAM headroom on `CYD_S3_3248` is unchanged. |
| 11 | `gui.begin(core)` | `UIToolkit::init()` then screen content. Needs LVGL alive. |
| 12 | `SystemReport::run(core, false)` | Last, so it reports the finished state of everything above. |

### 4.1 LVGL initialises last

Order: display → touch → audio → connectivity → MQTT → registry → providers → **LVGL**.

`gui` does not own the hardware, so the LVGL step is not bundled inside `gui.begin()`. Worth being
explicit about, because **it decides who gets first claim on internal SRAM**, and this project has already
lost a session to exactly that class of bug (`CYD_S3_3248`, `softAP()` panicking inside
`ieee80211_hostap_attach`).

Concretely, on `CYD_S3_3248` — the only board whose LVGL draw buffers must live in internal SRAM —
the WiFi driver takes its internal RAM first and LVGL allocates its 30,720-byte buffers after it.

**Keeping the new order, deliberately, for one reason: LVGL degrades and WiFi does not.**
`LVGL_Startup::begin()` has a three-step fallback — internal SRAM, then PSRAM, then plain
`malloc()` — so a squeezed LVGL gets slower, not broken. The WiFi driver has no fallback; when its
internal DRAM allocation fails it dereferences the null and panics with no error message. Giving
the allocator with no fallback first claim is the safer arrangement, and it is the one we now
have. Do not move LVGL's initialisation earlier.

The number to watch on `CYD_S3_3248W535` is the internal heap free after
`[LVGL] Allocating: 30720 bytes per buffer... Success.` (22 KB when measured).

`loop()` keeps today's call order — LVGL first, then the data layer:

```cpp
void loop() {
    LVGL_Startup::tick();   // lv_timer_handler()
    gui.tick();            // header, pnlDisplay, pnlAudio
    core.loop();           // connMgr, mqttMgr, providers, entities.tick(), haPub
    delay(2);
}
```

---

## 5. Threading — decision and reasoning

**Decision: LVGL stays on `loop()` with `LV_USE_OS = LV_OS_NONE`. `LVGL_Startup::lock()` and
`unlock()` exist from day one and are no-ops.**

The reference implementations split here. `esp_lvgl_port` and `esp_lvgl_adapter` (ESP-IDF) run
LVGL in a dedicated FreeRTOS task behind a mutex, and callers must take
`bsp_display_lock()`/`lvgl_port_lock()`. The Arduino_GFX examples — including Waveshare's own for
our exact P4 panels — run `lv_timer_handler()` from `loop()`.

Staying in `loop()` because:

1. **ROADMAP §4.2's rule is stronger than a mutex.** "Providers never touch LVGL" plus
   `EntityRegistry`'s existing `std::mutex` + dirty set + `drainDirty()` already delivers the
   guarantee a display mutex would. A second locking discipline on top would be redundant *and*
   would create two ways to be correct, which is worse than one.
2. **A mutex is a rule every future card author has to remember.** A one-directional drain is a
   rule the architecture enforces. We have eight boards and one maintainer.
3. **It is what our stack's own vendor examples do.** Arduino_GFX is not thread-safe in ways the
   ESP-IDF `esp_lcd` layer is; the task model is easier to adopt from the IDF side.
4. **This milestone is meant to be behaviour-preserving.** Changing the threading model during a
   file reorganisation would make any regression impossible to attribute.

Why `lock()`/`unlock()` exist anyway, as no-ops: they are the *only* thing that makes reversing
this decision cheap. If every non-LVGL-task caller goes through them from the start, switching to
`LV_OS_FREERTOS` and a dedicated task later is a change to one file. Without them it is an audit
of the whole tree. They also match the API shape of both `esp_lvgl_port` and `esp_lvgl_adapter`,
which is what "keep an ESP-IDF port open" actually costs at this stage: not adopting their
runtime, but not painting over their interface.

**On `esp_lvgl_adapter` vs `esp_lvgl_port` — you remembered right.** Verified in `reference/`:
the newest Waveshare BSPs (P4-4B and P4-7B, `waveshare/esp32_p4_*` 3.0.1) depend on
`espressif/esp_lvgl_adapter` ~0.6.x, while the older S3 repo and all three of the non-Waveshare
reference projects still use `espressif/esp_lvgl_port`. So the migration is real and visible in
the vendor tree. I have **not** confirmed upstream what Espressif's long-term intent is, and it
does not change anything here: both are ESP-IDF components we cannot use from Arduino +
Arduino_GFX. What transfers is the interface shape, which §5 adopts.

**On using both cores (the Allsky project).** Orthogonal to this decision and still available: a
provider doing something slow can be moved to its own task on the other core *without* touching
LVGL, precisely because it only ever writes to the registry. That is the payoff of §4.2's rule,
and it needs no change here.

---

## 6. Ripple — everything this touches

| File | Change |
|---|---|
| `src/LVGL_Test_UI.cpp` | Renamed `src/main.cpp`; contents distributed per §3 |
| `src/GUIManager.*` | Engine code leaves; screen content arrives; `update()` → `tick()` |
| `src/Panel_Display.*` | Constructor `GUIManager&` → `(DisplayManager&, TouchManager&)` |
| `src/Panel_System.*` | `extern debug_dump_config` removed; registers itself as a sink |
| `include/UIToolkit.h` | **Done** — dropped its unused `#include "GUIManager.h"`, a leftover that dragged `DisplayManager` and `Arduino_GFX` into every panel header |
| `platformio.ini` | No change. `build_src_filter = +<*>` already picks up new files in `src/` |

**Filename casing, worth fixing in passing.** The file is `include/UIToolkit.h` but is included as
both `"UIToolkit.h"` and `"UIToolkit.h"`. Windows does not care; a Linux CI build (or anyone
cloning on Linux) fails outright. Normalising to `UIToolkit.h` everywhere costs one commit now
and is a mystifying breakage later.

---

## 7. The GUI-less build

**Not built, not tested, deliberately.** Downgraded from requirement to "keep the door open."

The door stays open at zero cost, because it is not a feature we add — it is a property of §3
being done properly. After this milestone, `main.cpp`'s only LVGL-touching lines are steps 10–11
plus two lines in `loop()`, and `SystemCore` / `SystemReport` include no LVGL header at all.

If a sensor-hub variant is ever wanted, it is: one `-D FLEET_NO_GUI` guarding those four call
sites, a `build_src_filter` dropping `LVGL_Startup.cpp` / `GUIManager.cpp` / `UIToolkit.cpp` /
`Panel_*.cpp`, and `lib_ignore = lvgl`. An afternoon, not a redesign.

No `#if` guards are being added now. Conditional compilation that nothing builds is
conditional compilation that rots — the same reason `LESSONS.md` distinguishes "compiles" from
"runs."

---

## 8. Invariants

1. `WS_P4_TOUCH_LCD_5` and `CYD_S3_3248W535` build clean. Together they cover both branches of the
   allocation matrix in `LVGL_Startup`: the MIPI full-frame PSRAM path and the QSPI internal-SRAM
   path.
2. `src/main.cpp` calls no `lv_*` function directly.
3. `SystemCore` and `SystemReport` include no LVGL header.
4. No `extern` function declarations remain in any `Panel_*.cpp`.
5. The full System Doctor report renders with `[UI STATE]` between `[DISPLAY]` and
   `[I2C BUS SCAN]`.

Clear `.pio/build_cache` before verifying a build after file renames: a content-addressed cache
is not worth trusting there.

---

## 9. Implementation notes

- `GUIManager` could not stop owning `DisplayManager` and `TouchManager` until `LVGL_Startup`
  existed to receive them, so the hardware-ownership move and the `LVGL_Startup` extraction are one
  change. The `SystemReport` inversion went in with it, because `main.cpp` cannot shed
  `debug_dump_config()` and keep it at once.
- `FleetI2C::begin()` is hoisted into `SystemCore::begin()` as step 1, and `DisplayManager`'s own
  call stays. That is safe because `FleetI2C::begin()` is idempotent, so `DisplayManager` remains
  usable standalone and nothing initialises twice.
- The generic device-info prints (firmware version, device name, touch panel, PSRAM and flash size)
  live in `SystemCore::printIdentity()`, called as step 0, not in the display driver.

