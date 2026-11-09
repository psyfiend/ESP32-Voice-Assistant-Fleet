# Lessons

Mistakes that cost real time, written down so they cost it once. Each entry opens with the rule,
then the mechanism, then the evidence in a line or two; the investigations are in git history and
the bring-up docs. The test for adding one: would we make this mistake again in six months?

Editing files from a shell is covered at the top of `CLAUDE.md` ("STOP - how to edit files"); this
file does not repeat it.

---

## Verifying

**A claim about the outside world is verified from the outside world.** A device reports its own
state through the same code that set it, so a wrong setting reads back as right. Three diagnostics
lied this way:

| What lied | Why |
|---|---|
| `WiFi.getHostname()` | Reads back the global buffer `setHostname()` wrote, not what the interface was given. Only the router's lease table showed the real name. (The interface-level call is `WiFi.STA.setHostname()`.) |
| `esp_wifi_connect()` | Returns `ESP_ERR_WIFI_CONN` when a connect is already in flight, so a "re-issuing connect" log line described a retry that never happened. |
| `ConnState::AP_ACTIVE` | Survived `stopAp()`, so the header glyph advertised a setup network that was not broadcasting. |

A DHCP hostname is confirmed in the router's lease table; an MQTT publish with `mosquitto_sub` or
MQTT Explorer. "The device says it published" and "the broker received it" are different claims.
Publishing successfully also says nothing about whether anyone accepted it: `sys_ip/state` was
published correctly while Home Assistant showed nothing, because HA had rejected the discovery
config and no entity existed to read the topic.

**When a device-side theory needs a fifth iteration, ask the far end.** The broker's log said
`disconnected: exceeded timeout` in plain words after four rounds of inference from the device. The
broker, the router, the HA device page and a ping all know things the firmware cannot.

**When a symptom is sharply hardware-specific and appears across a whole class of board, search the
vendor's issue tracker and its master branch before the third hypothesis.** Weeks of theories about
Wi-Fi dropouts on the P4 boards (NVS writes starving the SDIO transport, the C6 firmware version, our
reconnect ladder) were all wrong: it was an open Espressif issue (esp-hosted-mcu#243), with our exact
log line (`H_SDIO_DRV: RX buffer alloc failed`, `Req_WifiStaGetApInfo` timing out). Keep the exact
error text; it is the search term. A hang found the same way (`ppa_do_operation()` waiting forever
for the PPA) was fixed on IDF master and not on `release/v5.5`, the branch our libraries are built
from, so a search of the release branch alone would have said "no known fix".

**When a vendor ships a known-working copy of a library you forked, diff the whole tree before
theorising.** The `WS_P4_5` display bug was fixed in Waveshare's own `Arduino_DSI_Display.cpp`, with
a comment naming the failure, after rounds spent on a different file. Six vendor trees sit in
`reference/`; only the P4-5's has been diffed, and only partly (FUTURE_IMPROVEMENTS, #40).

**Inference from config symbols loses to direct measurement.** Check which artefact is on the include
path before reasoning about its contents: the framework ships two P4 lib variants (`esp32p4_es` and
`esp32p4`), and reading the other one's `sdkconfig` produced confident, wrong theories about silicon
revisions.

**A measurement is about the thing it was taken on, not the class.** Two measurements were quoted as
class facts and shaped decisions: "HA computes the state-dependent icon" (one occupancy sensor,
which is a template that sets its own icon) and "a font face costs ~96 KB" (one large full-ASCII
Montserrat; the real range is 6-73 KB). When writing a measurement down, name what it was taken on.

**A measured list is only as wide as what was measured.** The icon font is a subset built from
`scan_ha_icons.py`'s reading of the icons HA serves for the declared entities. Cards made from
entities nobody declared drew plain bulbs, silently, because a glyph the subset lacks falls through
to the kind's icon. When a feature widens what can reach the screen, widen the measurement with it;
the font now carries every MDI icon the owner's HA serves.

**A comment that describes a mechanism in another file is a claim, and claims get checked.** A comment
said events bubble from cards to the screen while nothing set `LV_OBJ_FLAG_EVENT_BUBBLE`, and the
uninitialised press origin `{0,0}` then read as "left half, top edge" for every gesture. Name the file
that holds the other half so the pair can be found.

**A handoff summary becomes the spec for whoever reads it first.** A one-line "two layout families"
summary of the card spec was read before the spec, promoted to class names, and the domain layer the
spec called for was skipped. Say which document a summary compresses, and distrust a summary's
vocabulary when it does not appear in the source.

**When the parts pass and the whole fails, test the whole the way it runs.** Isolated checks of
`esp_async_fbcpy` (whole frame, odd widths, two copies at once) all passed while the flush put stale
pixels on the glass, because only the flush queued copies; the cause was a `static` config shared by
every handle in Espressif's code. `/bench?what=verify` compares LVGL's render with the frame buffer
in one instant on the device. A comparison taken from two snapshots at two times cannot judge a
moving picture: a ticking value looks like a stale buffer.

**Measure the case the user complains about, the way it runs.** A forced full-screen benchmark showed
-45% per frame while a deck-panel swap still stuttered: an animation redraws a few overlapping areas
each frame and the framebuffer repair re-copied nearly all of them (200-500k px of PPA work a
frame). Only `/bench?what=anim` with unforced frames and the repair's own numbers showed it. Measure
per frame; an average hides the late ones.

**Drive the UI from the PC before the owner touches it.** `GET /popup` (`DEBUG_POPUP`) opens windows,
switches controls and reads the pool, so owner rounds find layout and feel instead of plumbing.
It cannot test touch. **A probe must not move what it measures**: a listing call that also switched
the window's view left the group's view holding a member, so every later result looked like a bug in
the code under test. When a whole run fails at once, suspect the instrument first.

**Catch where a frozen screen stopped.** `HangWatch` (`-D DEBUG_HANG`, `/hang`) runs on the other
core, notices `loop()` stopping for 3 s, suspends it and records the program counter and the code
addresses on its stack. One capture decoded to the cause where reading code did not. Check the
watcher with a stall made on purpose first, so its report can be trusted.

**A crash backtrace is two seconds of work: decode it before theorising.**
`xtensa-esp32s3-elf-addr2line -pfiaC -e .pio/build/<env>/firmware.elf 0x... 0x...` (`riscv32-esp-elf-`
for the P4). A WiFi-driver crash with `A2 = 0x00000000` and a small `EXCVADDR` is an out-of-memory
symptom: `ieee80211_hostap_attach` allocated, got NULL and dereferenced it.

**A script that prints "ok" has not necessarily done anything.** A `str.replace()` whose anchor
matched nothing exits 0 with its success message. Confirm the file changed (`grep -c` for what you
added); a replace that matched nothing and one that worked look the same from outside.

**When something works on seven boards and fails on one, look for what that board does differently at
the resource level**, not the feature level (see the QSPI board under Memory).

---

## Build system and tooling

**The PlatformIO build cache can serve stale objects after a BSP-only edit.** `build_cache_dir` is
content-addressed and `pio run -t clean` does not clear it. `bsp_loader.h` includes the board header
through a macro (`#include BSP_HEADER`), and a file whose only dependency on a BSP value runs through
that indirection can keep a stale object. If a BSP field change has no effect: `rm -rf
.pio/build_cache`.

**"SUCCESS" can mean the library was never compiled.** The LDF builds only a library something
`#include`s. Verify a new one by finding its object: `find .pio/build/<env> -name "MyFile.cpp.o"`.

**Do not put a value that changes every commit in a compile flag.** A `-D` lands on every file's
command line and invalidates the content-addressed cache for the whole tree, so every commit rebuilt
everything. `scripts/fw_version.py` writes a generated header and rewrites it only when its content
changes, so only the few files that include it rebuild.

**Build flags go in `platformio.ini`, never `PLATFORMIO_BUILD_FLAGS`.** The environment variable makes
PlatformIO's project checksum disagree with any other pio process (the VS Code extension indexing the
tree), and one of them deletes `.pio/build` mid-build ("cannot find the path specified").

**`platformio.ini`'s `symlink://` library paths are absolute and machine-specific, but only the paths
are.** Committing it from the original machine is correct. What must never be committed is a clone's
rewritten prefix. Board flags such as `ARDUINO_USB_CDC_ON_BOOT` are board properties and belong in
the repo.

**`pio` crashes with `UnicodeEncodeError` when its output is not a console.** It prints box-drawing
characters in the upload summary; into a pipe or a file Python falls back to cp1252 and the write
raises, so `pio run -t upload | tail` and `> file` fail while the bare command works. The crash is
invisible because it happens while writing the crash. Set `PYTHONIOENCODING=utf-8` before `pio` or
`esptool` whenever output is captured, and give an unattended upload a `timeout` (`timeout 900 pio run
...`). A killed `pio` leaves orphaned `esptool` and `python` children holding the serial port and
`firmware.bin`, so the next attempt fails with "being used by another process"; look for strays
before suspecting the board:
`powershell "Get-Process | Where-Object { $_.ProcessName -match 'python|esptool|pio' }"`.

**Always pass `--upload-port` explicitly.** Auto-detect selects virtual ports (Intel AMT
Serial-over-LAN, a legacy motherboard COM port).

**A spike's temporary setting is not done until it is reverted or deliberately kept.**
`ConnMode::STA_PLUS_AP` was set for the APSTA feasibility spike and never set back, so every board
stood up an access point on every boot for months; it surfaced only when the AP path crashed on one
board. Grep `*Defaults.h` after any spike. The same holds for a debug flag: `DEBUG_CARDS` printed
~3 KB per page change, and at 115200 baud a UART whose buffer has filled blocks the caller, so ~90 ms
of every P4 page swipe was serial output nobody read. When the debugging is done the flag comes off,
and when something is slow, check what it prints first.

**Rebuilding a vendor's prebuilt libraries: capture what it was, diff afterwards, and require every
difference to be explained.** The P4 libraries were rebuilt to change two sdkconfig settings
(`docs/REBUILD_P4_LIBS.md`). The traps:

- lib-builder branches are named after ESP-IDF, not Arduino: `release/v3.3` pins ESP-IDF 3.3.
- `build.sh -t` takes the chip variant, not the target: `-t esp32p4` silently skips `esp32p4_es` and
  builds for silicon we do not have.
- menuconfig cannot work: the all-targets path starts with `rm -rf build sdkconfig out`. Only a
  defconfig in `SDKCONFIG_DEFAULTS` survives, and it must hold only the deltas.
- Asserting `CONFIG_CACHE_L2_CACHE_LINE_64B=y` silently halved the L2 cache (256 KB to 128 KB):
  Kconfig satisfied the option's `depends on` by falling back to the size choice's default. It was
  caught because the shipped `sdkconfig` was copied first and a checker classified every difference
  (`scripts/verify_p4_sdkconfig.py`); "the build succeeded" is not evidence.
- Espressif's prerequisites list omits `jq`; `-s` skips the environment setup that puts `idf.py` on
  `PATH` (source `esp-idf/export.sh` first); keep the stock folder, since the rollback is a rename and
  git cannot undo this change.

---

## C++ and Arduino traps

**Arduino's global macro namespace eats enumerators.** `esp32-hal-gpio.h` defines bare `DISABLED`,
`RISING`, `FALLING`, `CHANGE`, `HIGH`, `LOW`, `INPUT`, `OUTPUT`, `ANALOG` and more. Macros ignore
scope, so `MqttState::DISABLED` became `MqttState::0x00`, and the errors point at the framework header
and the call sites, never the declaration. It happened twice, the second time with the rule already
written down, because avoiding the names is a thing to remember at the moment of typing. Use compound
names (`SESSION_OFF`, `RADIO_OFF`), and check new enumerators against the framework headers:

    cd ~/.platformio/packages/framework-arduinoespressif32/cores/esp32
    for m in MY_NAMES HERE; do grep -rhwE "^ *# *define +$m" . | head -1; done

**A board identity macro must not match a struct instance name.** Once `#define WS_P4_7B` exists, the
preprocessor rewrites every bare `WS_P4_7B`, including a struct's own declaration, to `1`.

**Designated initializers must follow declaration order.** Reordering fields you do set fails with
`designator order for field 'X' does not match declaration order`. This applies to BSP headers and to
every struct initialised this way (`EntityDescriptor` hit it).

**A fixed buffer sized by guess in a data path fails as corruption, not as an error.** `MqttProvider`
copied payloads into a `char[257]`; real Zigbee2MQTT messages are larger, so each was cut mid-JSON and
the parser reported an error about the sender. Parse from `(pointer, length)`; never copy "to be safe".
And `payload[length] = '\0'` writes one byte past memory the MQTT library owns: use a length-aware
constructor or your own buffer.

**A debug string worth keeping goes behind a `DEBUG_<AREA>` flag**, and the flag is off in committed
builds (see the debug flag note above).

---

## MQTT and Home Assistant

- **PubSubClient's default buffer is 256 bytes and it fails silently**: `publish()` returns `false`
  and nothing reaches the broker. Any real discovery payload exceeds it. Call `setBufferSize()` and
  check the return.
- **Ignore retained payloads on command topics.** A broker redelivers retained messages on every
  reconnect; on a command topic that is a stale instruction replayed forever (a retained `reboot`
  means reboot, reconnect, receive it again). PubSubClient does not pass the retain flag to its
  callback.
- **The Last Will fires only on an ungraceful disconnect.** A clean shutdown must publish `offline`
  itself, or every entity stays "available" until the keepalive lapses.
- **HA's `text`, `switch`, `number`, `light` and `button` are command platforms.** They need a
  `command_topic`, and HA rejects a config without one silently: the entity never appears and nothing
  is logged. A read-only string is a `sensor` whose value happens to be text.
- **`state_class` is what makes HA remember.** `measurement` gets graphing and long-term statistics.
  Not for uptime: `total_increasing` reads a reboot's reset to zero as a counter rollover.
- **Use device-based discovery**: one retained payload at `homeassistant/device/<device_id>/config`
  with a `cmps` map. Entities appear atomically, the device block is written once, and removing an
  entity is one republish instead of publishing an empty payload to evict a retained config.
- **Zigbee2MQTT does not retain state topics by default**, so a subscriber gets nothing until the
  device next publishes, which for a quiet outdoor sensor can be a long time.
- **HA's reply order is not the bridge's.** After a command to a Hue room, a bulb's report can lag the
  room's by a second or more. Read HA again before calling a mismatch a bug.

---

## Hardware

- **`WS_P4_5` needs `-D ARDUINO_USB_CDC_ON_BOOT=0` or there is no serial output.** Its second USB-C is
  the USB OTG port, wired to the P4's OTG PHY rather than a serial bridge, so it never enumerates. UART0
  through the onboard CH343 is the only channel.
- **`Serial` is not on the port you flashed through when `ARDUINO_USB_CDC_ON_BOOT=1`.** It maps to the
  chip's native USB CDC, which enumerates as a separate device (`VID_303A`) only while the app runs;
  the CH343 bridge (`VID_1A86`) flashes and is silent for application output. Check the board's flag
  in `platformio.ini` before debugging silence. Six boards set it to 1 (7B, CYD_P4_1060, CYD_P4_4880,
  WS_S3_4B, CYD_S3_3248, WS_S3_5B); with no native-USB cable, such a board's log cannot be read.
- **A native-USB board plugged into a PC can freeze on every `Serial.print`.** Arduino's `HWCDC`
  retries each write for up to 20 x its 100 ms TX timeout when the PC is attached but not reading the
  port: up to 2 s of `loop()` per print. The 4880 froze on every swipe and tap while plugged in, and
  never on a wall adapter. `Serial.setTxTimeoutMs(0)` in `setup()` (`main.cpp`) drops output instead
  of waiting. Judge a native-USB board's smoothness off the PC, or with that in: an idle COM port
  looks exactly like a slow display stack.
- **`-D CORE_DEBUG_LEVEL=4` makes the DSI path visible.** Without it every `ESP_LOGI` in the display
  driver compiles out and a failure looks like silence. It affects only code compiled here; ESP-IDF's
  prebuilt `esp_lcd` internals stay quiet regardless.
- **A driver's `mirror()` is not proof the panel mirrors.** The EK79007 driver (`WS_P4_7B`) implements
  `mirror()` by sending MADCTL (0x36); in DSI video mode this panel ignores MADCTL, and the picture
  never moved. A driver function is what the vendor wrote; the glass decides. Espressif also advises
  keeping 0x36 out of init sequences, since the driver tracks MADCTL itself.
- **Never read from a DSI panel that is showing a picture.** Reading the status registers (DCS
  0Ah-0Dh) with `esp_lcd_panel_io_rx_param()` returned sensible values and stopped the picture on all
  three P4s that were tried. The drivers read the panel ID only before video starts, the only safe
  time. Writes were not seen to do harm.
- **A dark screen after a good flash can mean the chip is still in download mode.** On the native-USB
  P4s, esptool's RTS hard reset does not leave a download mode the BOOT pin forced; `--after
  watchdog-reset` does. `esptool --before no-reset --after no-reset chip-id` connecting without a
  reset proves it is in the ROM bootloader. For a native-USB board that will not flash: hold BOOT while
  plugging in, use a slower baud (460800), and end with a watchdog reset.
- **A software restart can leave a P4 boot-looping.** After `esp_restart()` (`POST /settings?reboot=1`)
  the C6 co-processor may not answer over SDIO (`sdmmc_host_wait_for_event returned 0x107`,
  `H_SDIO_DRV: failed to read registers`) and esp-hosted restarts it every ~20 s, off the network.
  It has happened on the 1060 and the 4B, so treat it as any P4 with a C6. Restore a settings copy
  without `reboot=1`, and restart with `esptool ... --after watchdog-reset read-mac`. Read such a board
  over its native USB without resetting it (pyserial with `dtr = rts = False`).

---

## Memory: internal SRAM is the scarce resource

**Internal DRAM is what the Wi-Fi driver and LWIP take socket buffers from, and it is also where static
arrays and LVGL buffers land.** When it runs low the network fails first, loudly, and in a way that
points everywhere except the cause. Faults that all looked like "MQTT keeps dropping" and were not MQTT:

| Looked like | Actually was |
|---|---|
| broker refusing us | `LV_MEM_SIZE` raised from 128 to 192 KB: 11 KB internal heap left, no socket could be allocated |
| a heap leak | two 30 KB draw buffers where the BSP asked for one: 10 KB free at boot |
| broker timing us out | a TCP socket leaked per reconnect on a detected drop: the broker was reaping ghosts |
| HA's websocket could not open a socket, `/bench` replies cut short (`WS_S3_4B`) | the HTTP server's request load took an idle 12-17 KB internal free down to ~4 KB |

Rules:

- **"It links" is not a memory test.** 192 KB linked with room to spare and still broke the board.
  Watch `ESP.getFreeHeap()` on a board that is trying to hold a socket.
- **A falling number is a leak; a low flat number is a budget.** They need opposite fixes; the
  disconnect log prints heap and elapsed time so it can tell them apart.
- **Below ~40 KB internal, expect network failure.** `SystemCore` prints internal free heap at boot
  and marks it. `ESP.getFreeHeap()` is internal heap only; free PSRAM is irrelevant to a socket.
- **The HA free-heap sensor's history is the instrument** for "is this new".
- **Large fixed arrays do not belong in internal RAM.** Put the storage in PSRAM and inject it
  (`PsramArray`, `include/PsramArray.h`; `SystemCore` allocates the entity registry in PSRAM).
- **Give LVGL's own pool to PSRAM and never to the system `malloc` on the P4.** `-D FLEET_LV_MEM_PSRAM`
  makes LVGL take its pool from PSRAM at `lv_init()` (`lv_conf.h`): the `WS_S3_4B` went from 7 KB to
  133 KB internal free, drawing ~5-10% slower. Handing LVGL to the system `malloc` looks like the ideal
  split (allocations under 4 KB go to internal RAM, larger to PSRAM), but thousands of small LVGL pieces
  fragmented the internal heap until esp-hosted's 4.6 KB receive buffer could not be found and the
  board rebooted (NINA's `lv_mem_psram.c` records this). Decision A11.

**The one board whose display cannot use PSRAM carries the cost.** `LVGL_Flush_Gfx.cpp` and
`LVGL_Flush_EspLcd.cpp` place draw buffers by bus: MIPI and RGB boards get full frame buffers in PSRAM; QSPI partial buffers stay in
internal SRAM because a QSPI panel cannot stream from PSRAM fast enough. `CYD_S3_3248` is the only
QSPI board, so it spends ~61 KB of internal SRAM on draw buffers. Adding a 21 KB `EntityRegistry` to
its internal `.bss` starved the Wi-Fi driver and `softAP()` crashed in `ieee80211_hostap_attach`, a null
dereference with no message because the driver does not check that allocation. To find the offenders:
`xtensa-esp32s3-elf-nm -C --size-sort -r -S .pio/build/<env>/firmware.elf | awk '$3=="b"||$3=="B"'`.

**A flash write from a task whose stack is in PSRAM reboots the board.** A flash write turns the CPU
cache off, and ESP-IDF asserts that the calling task's stack is not in PSRAM
(`esp_task_stack_is_sane_cache_disabled()`). The HTTP server's stack is in PSRAM on purpose
(`HttpServer.cpp`, `task_caps`), so a flash write or a LittleFS read in a handler reboots. Anything
that touches flash runs on a task made with `xTaskCreate()` (stack in internal RAM): the settings save,
any handler that saves, the `/panel?flash=` test. A handler serves what such a task left in RAM. The
loop task is fine.

**A CPU-bound task that never blocks runs at the idle task's priority.** The arranger's search runs on
its own task on core 0. At priority 1 it never let IDLE0 run, and the task watchdog watches IDLE0
(`CONFIG_ESP_TASK_WDT_TIMEOUT_S`, 5 s): a long search aborted the chip, and since no answer was ever
kept, every boot searched and aborted again. The log says it (`task_wdt: - IDLE0 (CPU 0)` /
`CPU 0: arrange`). The task is created at `tskIDLE_PRIORITY` (`PageArranger.cpp`): equal-priority tasks
time-slice, IDLE0 feeds the watchdog between slices, and the search still gets what the core has
spare. Any non-blocking task (a search, an encoder, a checksum) goes at priority 0 or yields with
`vTaskDelay(1)` at least every second. Read the serial before guessing: `pio device monitor -p COMn
--rts 0 --dtr 0`, or pyserial from PlatformIO's own Python (`~/.platformio/penv/Scripts/python.exe`).

---

## LVGL

### Layout and objects

- **An out-of-range grid row is a hard freeze.** `lv_conf.h` defines `LV_ASSERT_HANDLER` as
  `while(1);`, so indexing a grid's row descriptor past its end locks the board with no output and no
  reboot, indistinguishable from a hang in your own code. A page defined as many rows as fit the
  viewport while placement flowed into as many as the cards needed; the tightest board
  (`CYD_S3_3248`, five rows at two columns) froze. Flash the tightest board first, and when a board
  freezes silently, suspect `LV_ASSERT` before an infinite loop of your own.
- **`lv_obj_align_to()` resolves against the reference object's position at the moment it is called**,
  not at layout time, so a sibling re-aligned earlier in the same function is read at stale
  coordinates. Let a flex row do the positioning (it measures after layout); align against something
  whose position is settled.
- **A new or just-resized object reports the old geometry until LVGL lays it out.**
  `lv_obj_get_width()` right after `lv_obj_set_width()` returns the old width, and a newly made
  object's `lv_obj_get_y()` is 0. Position and animate from the value you just set.
- **`lv_obj_update_layout(obj)` lays out the whole screen `obj` is on.** Called after a page's cards
  exist, it does the page's entire layout (~115 ms, charged to "the header" in a profile). Time layout
  on its own (`DEBUG_PAGE_TIMING`) before blaming the caller.
- **`LV_LABEL_LONG_DOT` needs a real width, and rewrites its own text.** Without a width it wraps
  silently; `flex_grow` on the label gives it one. It puts the "..." into the label's buffer, so
  `lv_label_get_text()` after a layout at too small a width returns "Sce...", not "Scenes". Measure
  from your own string, or size the label before anything lays it out.
- **`LV_SIZE_CONTENT` counts a child that only ignores the layout.** A face image as big as its parent
  with `LV_OBJ_FLAG_IGNORE_LAYOUT` reached into the parent's padding and grew the rows (68 px to 106).
  `LV_OBJ_FLAG_FLOATING` takes a child out of the content size too.
- **A flex column centres its children twice.** With `LV_FLEX_ALIGN_CENTER` on the cross axis LVGL
  centres a track as wide as the widest child, then each child in the track; two integer roundings move
  a child a pixel when a sibling changes width. To centre against the parent alone, give the child the
  parent's full width and centre inside it.
- **A kit component may find its own parts by child index** (`Kit::chipTable()` finds row N as its Nth
  child), so never add children to one built by the kit. Put the component and the extras in a column
  of your own.
- **A touch goes to the topmost clickable object**, so a bare scrolling list cannot be swiped from its
  gaps: a day's heading (a label) let the touch fall through the non-clickable list to the window.
  `UI::tameScroll()` makes every scrolling list clickable.
- **Never keep a pointer to an LVGL object you do not own; ask it to tell you when it goes.** The popup
  kept the pressed card's pointer while its hold animated; a swipe rebuilt the page, deleted the card,
  and the release restyled it: a load access fault in `lv_realloc`. `lv_obj_is_valid()` is not the
  fix: a rebuild allocates new objects from the same pool, so the old address soon belongs to a
  different live object and the check passes. Use an `LV_EVENT_DELETE` callback that clears the pointer,
  removed again when the pointer is let go. (An animation whose `var` is the object is safe: LVGL
  deletes it with the object.)
- **A touch-driven rebuild: release the touch first.** LVGL delivers a gesture and then still delivers
  the press and click of the same finger. Call `lv_indev_wait_release()` before the action that
  rebuilds the screen; after it, new objects appear under a finger LVGL still considers pressed, and the
  press lands on whatever materialised there (a swipe-up also expanded the panel under the fingertip).
- **Events do not reach the screen through a clickable child** unless `LV_OBJ_FLAG_EVENT_BUBBLE` is set
  on the objects between.
- **The top and system layers are scrollable.** LVGL 9.5 removes only `CLICKABLE` from them
  (`lv_display.c`). Dragging a clickable child that cannot scroll hands the scroll to the first
  ancestor that can, the layer, which then carries every overlay (the toast, the Touch Points panel)
  with it, and only when the layer has room to scroll in the drag's direction, so it looked random.
  `LVGL_Startup::begin()` removes `SCROLLABLE` from both. When an object is in the wrong place but its
  own position is right, check whether its parent has moved; this was found by measuring the layer's
  scroll offset, not by adjusting the toast a third time.
- **A cache that never gives anything back fills up in a way one screen never shows.** The hand-drawn
  faces had twelve slots, one per scheme, look and size, never freed. Schemes are per page, so swiping
  filled it and every later face came out flat.
- **A knob that moves an input nobody can predict is not a control.** Col/Row buttons that nudged
  `TARGET_CARD_W` could not subtract a row on a board where rows follow the card count. A user who
  wants three columns is told "three columns" outright. Deriving is the right default; it is not a user
  interface. A knob's log reports what the system did, not what the knob set.

### Drawing and clipping

- **A style that is set is not a style that is seen: check what clips it.** Every card asked for an
  8 px shadow and none drew one: the styled surface filled a transparent wrapper exactly, so the whole
  shadow lay outside its parent and LVGL clips children to their parent, silently. `OVERFLOW_VISIBLE`
  on the parent is the first half. The second: LVGL lets a child draw outside an overflow-visible
  parent only as far as the parent's own extra draw size (`LV_EVENT_REFR_EXT_DRAW_SIZE`, 0 for a plain
  object). So the flag plus an ext-draw callback returning the shadow's reach (`UI::shadowReachPx()`),
  then `lv_obj_refresh_ext_draw_size()` on each ancestor (`bareExtDrawCb` in `KitSheet.cpp`,
  `UI::unclipShadows()`). Check every ancestor between the shadow and the screen; one clipping parent
  is enough. The card's area tag, hanging above its cell, needs the same flag.
- **Draw outside an object in `DRAW_MAIN_END`, never `DRAW_POST`.** LVGL 9.5 clips an object's
  `DRAW_POST` to its own area whenever it has children, and skips it when the redrawn area misses the
  object. `DRAW_MAIN_BEGIN` / `MAIN` / `MAIN_END` get the area plus the ext draw size. Anything that
  must be over the children but inside the object goes in `DRAW_POST`; the corner cut does both
  (`Cards/CardFrame.cpp`).
- **Two anti-aliased triangles sharing an edge leave a pale seam**: each fades out over the same pixel.
  Cover the edge with a 2 px line in the same colour, or use one triangle whose other edges lie outside
  the clip.
- **To match LVGL's drawing, copy its arithmetic, not its intent.** The corner cut's shadow was one
  16-bit step lighter than LVGL's own. LVGL blends into RGB565 with `lv_color_16_16_mix()`: the mix cut to
  32 steps, each channel rounded down; an 8-bit blend rounded to nearest and truncated lands a step
  lighter about half the time. Doing it in 5 and 6 bits made the values identical. The shadow shape was
  measured (a rectangle with the card's shadow on an offscreen canvas, alpha read from the middle of each
  side), because LVGL's blur is two box passes over a rounded corner, not a ramp.
- **A wide soft shadow is an image painted once, not LVGL's shadow.** A 5 mm halo as an LVGL shadow
  raised a full redraw with a card window open from 129 to 175 ms on the P4_5 (LVGL recomputes the blur
  unless its single shadow cache holds that exact size, and three sizes were on screen) and banded
  (black to Midnight's ground is a handful of RGB565 levels). Painted as A8 images (four corners, four
  16 px edge strips tiled), black by recolour, alpha falling with distance, ordered-dithered with a 4x4
  Bayer matrix, it costs ~28 ms and does not band (`UI/Kit/KitHalo.cpp`). ARGB8888 costs ~7 ms more
  than A8, since LVGL blends an A8 image as a masked fill.
- **A smooth grey ramp bands pink and green on a 16-bit panel.** RGB565 steps green twice as finely as
  red and blue. A hand-painted ARGB8888 face needs ordered dithering to the 565 grid, the same threshold
  on every channel; LVGL's own gradients do not help.
- **An image with alpha is blended a pixel at a time; an opaque 16-bit one is copied.** Three ARGB8888
  satin rows made a full redraw of Members 22 ms slower on the P4_5; as RGB565 with the corners cut by
  the image's `radius` style (`clip_radius`) they cost 11. A face that is opaque but for its corners
  should be 16-bit.
- **An image both recoloured and corner-clipped loses its clip.** LVGL's software image draw has a path
  for a radius (`radius_only()`) and one for a recolour (`recolor_only()`); with both it falls to the
  general path and draws the square image. Tint a clipped face with a rounded veil object over it,
  never with `image_recolor`.
- **Rounded ends on a multi-stop strip**: with `LV_GRADIENT_MAX_STOPS 2`, let the slider draw rounded
  ends flat in the end colours and lay square pieces over the straight middle; end pieces that carry
  the rounding and reach into their neighbours hide most of the next piece.
- **A 0-wide object still draws its outline**, and **a shadow spreads sideways too.** Hide small
  objects with HIDDEN (it redraws only their pixels); size a mask that covers a tab's foot to include
  the shadow's reach, or its crescent shows beside it.
- **`lv_area_intersect()` and `lv_image_cache_drop()` are not in `lvgl.h` in 9.5**: include
  `src/misc/lv_area_private.h` or `src/misc/cache/instance/lv_image_cache.h`, or clip by hand.

### Layers

LVGL allocates a layer whenever something must be composited: `clip_corner`, transforms, and object
opacity below `LV_OPA_COVER` all render the object to an intermediate buffer sized by its **width**. A
615 px deck panel wanted 49 KB for one 20 px band; a 482 px two-cell card in `HDR_BAR` wanted 38 KB.
Both failed intermittently and hung boards. `Card::build()` avoids `clip_corner` for this reason;
`UIToolkit::create_collapsible_panel()` did it anyway and cost an evening. Work out what the layer
costs before adding any of the three to a wide object. A child larger than its parent also forces a
layer, since the parent must be composited to clip it, so a "floor" on a child size that can exceed its
container is not a safe fix.

### Effects that cover the screen

The card popup's grow was built four ways on `WS_P4_5` and measured frame by frame (`-D DEBUG_POPUP -D
DEBUG_FRAMES`), then dropped for a window that appears at once (W6, snappy first). What it showed:

- **A frame costs about the area that changes, times what lies under it.** A full-screen redraw on
  `WS_P4_5` is ~100 ms in Midnight and ~165 ms in Linen (its shadows); one card is 5-8 ms. So animate
  small things (a tab, a knob, one card) and treat anything screen-sized as a full redraw per change.
- **LVGL's top layer never hides anything.** It is drawn over the screen, so a redraw under a top-layer
  object draws the cards beneath it first. An opaque object that is the screen's topmost child is what
  `lv_refr_get_top_obj()` finds covering an area, and then nothing under it is drawn. Opaque windows
  go on the screen; the top layer is for small or transparent things.
- **A dim is a full-screen redraw at both ends.** The page is drawn under it the frame it appears and
  again the frame it goes. LVGL draws an invalidated area in full-width strips (50 lines here), so an
  opaque window in the middle saves nothing inside a full-screen invalidation; to keep a window out of
  a dim, dim the bands around it, never one full-screen object.
- **A growing filled box costs its whole area every frame; painting only each step's new ring costs
  the ring.** The motion was never the slow part; its ends were.
- **A picture of the page under moving objects is slower than drawing the cards** (84-180 ms a frame):
  LVGL's draw buffers are in PSRAM on the esp_lcd path, so the copy is PSRAM to PSRAM every frame, and
  the page stops being live.
- **LVGL 9.5's backdrop blur would be worse than a dim** on a live page: anything changing under a
  blurred object invalidates the whole object (`lv_obj_pos.c`), and the blur clips to the strip being
  drawn (`lv_draw_sw_blur.c`), so 50-line strips would likely show seams. Read from source, not tried.
- **An animation scheduled by time collapses behind one slow frame**: a 155 ms first frame of a 180 ms
  shrink left 2-3 frames instead of 6.
- **A moving object redraws its whole area, so a transparent holder costs everything under it.** A
  panel in a transparent object as wide as the window redrew that width every frame, and with nothing
  opaque over the strips LVGL drew the cards, window and panel in each (35-105 ms a frame on the P4_5).
  Sized to the tab and pane, most strips lie inside the opaque pane (8-40 ms). Keep moving things
  tight.
- **Never toggle HIDDEN on a screen-sized object at the start of an animation.** A transparent
  full-screen tap-away sheet unhidden as a deck panel began to open invalidated the whole screen, so
  every card and shadow redrew in the first frame; LVGL animations are time-based, so the result was a
  stall followed by a rush. Switching `LV_OBJ_FLAG_CLICKABLE` instead costs no redraw, and
  `lv_obj_hit_test()` passes over a non-clickable object.
- **A new big object is briefly invalidated at (0,0), full size.** LVGL lays out an object's size before
  its position (`lv_obj_refr_size`, then `lv_obj_refr_pos`), so a 787x545 window created at (246,83) also
  invalidated (0,0)..(786,544), joined into one area from the screen's corner that the window does not
  cover, so the cards under it and the header were drawn too. Build such things quiet: invalidation off,
  create, lay out, invalidation on, then invalidate exactly the rectangle that changed (`showWindow()`,
  `invalidateWindow()`). On `WS_P4_5` that took the frame from 73-87 to 42-48 ms in Midnight.
- **A shadow makes an object's redraw area wider than the object** (its ext draw size), which defeats
  covering the same way. Invalidate the object's own rectangle and thin bands for the shadow
  separately.
- **A panel that changes while open: animate the part that changes.** Re-measuring an open panel with
  the drawer's own opening slide moved the whole face, so every tap made the panel jump up and slide
  down. Animate the line's own height (`Kit::drawerReveal()`: 0 to full height, a label clips its text
  to its height) and set the panel's foot each frame, with the face held at the top. An animation built
  to open a container is the wrong tool for resizing one.
- **Switching a control live while a selector is dragged** is a measurement first (`DEBUG_FRAMES`), not
  a change.

---

## Network drivers and the display stack

The two findings behind the P4 library rebuild (the search that found them is under Verifying):

- Under bursty inbound TCP the host asks esp-hosted for an RX buffer with `MALLOC_CAP_INTERNAL |
  MALLOC_CAP_DMA`; the P4's DMA-capable internal region is small, the allocation fails, and the retry
  never runs because `NEW_PACKET` was cleared first. One failed allocation disables RX for the life of
  the boot, while host-to-slave writes still succeed: **a dead board reports itself connected**.
- The shipped `esp32p4` sdkconfig has `CONFIG_ESP_HOSTED_MEMPOOL_PREFER_SPIRAM` unset and
  `CONFIG_CACHE_L2_CACHE_LINE_128B=y`. Setting the first and using 64-byte lines took the same device and
  workload from 4 stalls in 13 minutes to none in 2 h 52 m. These are sdkconfig options in prebuilt
  libraries, so no `-D` reaches them: the P4 framework libraries are rebuilt (`docs/REBUILD_P4_LIBS.md`).

**The framebuffer getters cannot express a second buffer.** `esp_lcd_rgb_panel_get_frame_buffer()` and
`esp_lcd_dpi_panel_get_frame_buffer()` are variadic and their second parameter is the number of buffers
requested, so `get_frame_buffer(panel, 1, &fb)` means "give me one" and returns fb0, not "index 1".
`Arduino_GFX::getFrameBuffer()` returns a single pointer, so no fix behind that signature gives double
buffering. This is the argument for `esp_lcd` directly (`docs/display/README.md`).

---

## Rendering facts worth remembering

- **LVGL's DPI scaling scales shapes, never fonts.** Every DPI-sensitive path goes through
  `LV_DPX_CALC` (default object sizes, minimum scrollbar, slider and arc touch tolerance, the default
  theme's radius, border, padding and shadow). The default theme takes `font_normal` as a parameter and
  never consults DPI, so `lv_display_set_dpi()` is not an alternative to the generated type scale.
- **A test kept only because it was cheap can be the one that finds the bug.** When scope must be cut,
  "never observed at all" is a better criterion than "most likely to fail"; the connectivity test kept
  on those grounds exposed the `AP_ACTIVE` state bug.
