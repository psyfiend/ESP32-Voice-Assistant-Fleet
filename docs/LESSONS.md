# Lessons

Things that cost us real time, written down so they cost it once. Each entry is the
**conclusion**, not the investigation — the investigations live in git history and the
bring-up docs.

Add to this whenever something takes more than an hour to work out. The test for
belonging here is *"would we make this mistake again in six months?"*

---

## The big one: verify from outside the device

**Three diagnostics have now lied to us.** All three were the device confidently reporting
its own state, and all three were wrong:

| What lied | How |
|---|---|
| `WiFi.getHostname()` | Reads back the same global buffer `setHostname()` wrote, so the firmware reported a hostname the interface had never been given. Only the router's lease table exposed it. (The interface-level call is `WiFi.STA.setHostname()`.) |
| `esp_wifi_connect()` | Returns `ESP_ERR_WIFI_CONN` when a connect is already in flight, so a "re-issuing connect" log line described a retry that never happened. |
| `ConnState::AP_ACTIVE` | Survived `stopAp()`, so the header glyph advertised a setup network that was not broadcasting. |

**Rule: a claim about the outside world must be verified from the outside world.** A DHCP
hostname is confirmed in the router's lease table. An MQTT publish is confirmed with
`mosquitto_sub` or MQTT Explorer. "The device says it published" and "the broker received
it" are different claims.

Corollary, learned on the MQTT work: we published `sys_ip/state` correctly for hours while
Home Assistant showed nothing, because HA had rejected the *discovery config* and never
created an entity to consume the topic. Publishing successfully says nothing about whether
anyone accepted it.

---

## Build system

**The PlatformIO build cache can serve stale objects after a BSP-only edit.**
`build_cache_dir` is content-addressed and **`pio run -t clean` does not clear it**.
`bsp_loader.h` includes the board header through a macro (`#include BSP_HEADER`), and a
file whose only dependency on a BSP value runs through that indirection can keep serving a
stale object indefinitely. If a BSP field change appears to have no effect: `rm -rf
.pio/build_cache`. Confirmed with a `ROTATION` change.

**"SUCCESS" can mean "your library was never compiled."** PlatformIO's LDF only builds a
library something actually `#include`s. `Fleet_MQTT` was added to `lib_deps`, the build
passed, and none of it had been compiled. **Verify a new library by finding its `.o`:**

```bash
find .pio/build/<env> -name "MyFile.cpp.o"
```

**A version macro on the command line rebuilds everything.** `scripts/fw_version.py`
injects `FW_VERSION`/`FW_COMMIT` as `CPPDEFINES`, so they land on every file's compile
command. Every commit changes the describe count and hash; every edit flips `+dirty`. Each
one is a full rebuild. Injecting into a generated header included by one file would make
this a few seconds instead of minutes — **GitHub issue #46**. This is also why the VSCode
upload arrow appears to hang: it silently rebuilds before flashing.

**`platformio.ini`'s 26 `symlink://` paths are absolute and machine-specific — but only the
paths are.** Committing it from the original machine is correct and normal. What must never
be committed is a *clone's* rewritten prefix, which would break the original. Board flags
like `ARDUINO_USB_CDC_ON_BOOT` are board properties, not machine properties, and belong in
the repo.

---

## C++ and Arduino traps

**Arduino's global macro namespace will eat your enum.** `esp32-hal-gpio.h` defines bare
`DISABLED`, `RISING`, `FALLING`, `CHANGE`, `HIGH`, `LOW`, `INPUT`, `OUTPUT`, `ANALOG`. They
are macros, so scoping does not protect you: `MqttState::DISABLED` became
`MqttState::0x00`. The errors pointed at the framework header and the call sites, never at
the declaration. **Use compound enumerator names** (`SESSION_OFF`, `RADIO_OFF`).

**Designated initializers must follow declaration order.** Reordering fields you *do* set
fails with `designator order for field 'X' does not match declaration order`. Applies to
BSP headers and to every other struct we initialise this way — it caught us again on
`EntityDescriptor` months after the BSP rule was written down.

**The `DISABLED` trap caught us AGAIN at #49, with the lesson already written down.**
`Widget_MqttStatus`'s state enum had a `DISABLED` member. The rule was in `CLAUDE.md`, it was in
this file naming that exact identifier, and it was hit anyway - because "remember to avoid
ALL-CAPS enumerator names" is a thing you have to think of at the moment you type one, and the
error message still points at `esp32-hal-gpio.h` and at the call sites rather than at the
declaration.

**So make it a check rather than a memory.** Before adding enumerators, run them past the
framework headers:

    cd ~/.platformio/packages/framework-arduinoespressif32/cores/esp32
    for m in MY_NAMES HERE; do grep -rhwE "^ *# *define +$m" . | head -1; done

Ten seconds, and it is the difference between a rename and twenty minutes reading errors that
point nowhere near the cause. The compound-name habit (`SESSION_OFF`, `LINK_DEAD`) is still the
right default; the grep is what catches the one you did not think to compound.

**A board's identity macro must not match a struct instance name.** Once `#define WS_P4_7B`
exists, the preprocessor rewrites every bare occurrence — including a struct's own
declaration — to `1`.

---

## Data paths

**A fixed buffer sized by guess in the middle of a data path fails as corruption, not as an
error.** `MqttProvider` copied payloads into a `char[257]` before parsing. Real
Zigbee2MQTT messages are larger, so every message was cut mid-JSON and the fragment handed
to the parser — which then reported a parse error *about the sender*. Parse from
`(pointer, length)` and never copy "just to be safe".

**`payload[length] = '\0'` writes one byte past a buffer you do not own.** The MQTT library
owns that memory. It usually appears to work, which is exactly what makes it worth avoiding
deliberately. Use a length-aware constructor or copy into your own buffer.

---

## MQTT and Home Assistant

**PubSubClient's default buffer is 256 bytes and it fails silently.** `publish()` returns
`false` and nothing reaches the broker. Any real discovery payload exceeds it. Call
`setBufferSize()` and check the return.

**Ignore retained payloads on COMMAND topics.** A broker redelivers retained messages on
every reconnect. On a state topic that is what you want; on a command topic it is a stale
instruction replayed forever — and a retained `reboot` command means reboot, reconnect,
receive it again. Note PubSubClient does not expose the retain flag to its callback.

**The Last Will only fires on an *ungraceful* disconnect.** A clean shutdown must publish
`offline` itself, or every entity stays "available" in HA until the keepalive lapses.

**HA's `text`, `switch`, `number`, `light` and `button` are COMMAND platforms.** They
require a `command_topic` and HA rejects a config without one **silently** — the entity
never appears and nothing is logged. A read-only string is a `sensor` whose value happens
to be text. "The value is text" and "the entity is a text input" are different claims.

**`state_class` is what makes HA remember.** `measurement` for instantaneous values gets
graphing and long-term statistics; without it HA forgets. **But not for uptime** —
`total_increasing` sounds right and is wrong, because a reboot resets it to zero and HA
reads that as a counter rollover.

**Use device-based discovery**, one retained payload to
`homeassistant/device/<device_id>/config` with a `cmps` map — not one payload per entity.
Entities appear atomically, the device block is written once, and removing an entity is one
republish instead of publishing an empty payload to evict a retained config.

**Zigbee2MQTT does not retain state topics by default.** Subscribing gets you nothing until
the device next publishes, which for a quiet outdoor sensor can be a long time.

---

## Hardware

**`WS_P4_5` needs `-D ARDUINO_USB_CDC_ON_BOOT=0` or there is no serial output at all.** Its
second USB-C is the USB **OTG** port, wired to the P4's OTG PHY rather than to any serial
bridge, so it never enumerates in any state — not with blank flash, not with the factory
demo. No driver fixes this; there is no device to bind to. UART0 via the onboard CH343 is
the only channel.

**Always pass `--upload-port` explicitly.** Auto-detect happily selects virtual ports —
`COM4` on the laptop was Intel AMT Serial-over-LAN, and `COM1` on the desktop is a legacy
motherboard port.

**`-D CORE_DEBUG_LEVEL=4` is what makes the DSI path visible.** Without it every `ESP_LOGI`
in the display driver compiles out and a failure looks like total silence. It only affects
code compiled here; ESP-IDF's own `esp_lcd` internals are prebuilt archives and stay quiet
regardless.

---

## Memory: internal SRAM is the scarce resource, and not evenly

**Not all boards have the same internal RAM headroom, and the difference is the display
bus.** `GuiManager` places LVGL buffers by bus type: MIPI/RGB boards get full framebuffers in
**PSRAM**; SPI/QSPI boards get partial buffers in **internal SRAM**, because a QSPI panel
cannot stream from PSRAM fast enough.

`CYD_S3_3248` is the fleet's only QSPI panel, so it is the only board spending ~61 KB of
internal SRAM on display buffers. Adding a 21 KB `EntityRegistry` to internal `.bss`
alongside that starved the WiFi driver, and `softAP()` panicked inside
`ieee80211_hostap_attach` — a **null deref with no error message**, because the driver does
not check that allocation.

Two lessons:

- **When something works on seven boards and fails on one, look for what that board does
  differently at the resource level**, not at the feature level. It was not "S3 versus P4";
  it was "the one board whose display cannot use PSRAM."
- **Large fixed arrays do not belong in internal RAM.** Put the storage in PSRAM and inject
  it, which also removes the ceiling — a build can size a table for what it needs rather than
  for the fleet's worst case.

A useful trick for finding the offenders:

```bash
xtensa-esp32s3-elf-nm -C --size-sort -r -S .pio/build/<env>/firmware.elf | awk '$3=="b"||$3=="B"'
```

**A WiFi driver crash with `A2 = 0x00000000` and a small `EXCVADDR` is an out-of-memory
symptom**, not a logic bug. `ieee80211_hostap_attach` reading address `0x2c` means it
allocated, got NULL, and dereferenced it.

**And decode the backtrace before theorising.** Raw PCs look opaque but are two seconds of
work, and they said "this is entirely inside the WiFi stack" — which immediately ruled out
every one of our own new libraries:

```bash
xtensa-esp32s3-elf-addr2line -pfiaC -e .pio/build/<env>/firmware.elf 0x... 0x...
```

## A default left over from a spike is a bug that hides

`ConnMode::STA_PLUS_AP` was set as the fleet default during the APSTA feasibility spike
(#5) and never set back, so every board stood up an access point on every boot for months.
Nothing failed, so nothing drew attention to it — it only surfaced because the AP path
crashed on one board.

**When a setting is changed to enable a test, the change is not done until it is reverted or
deliberately kept.** Worth a grep through `*Defaults.h` after any spike.

## Method

**When a vendor ships a known-working copy of a library you have forked, diff the whole
tree before theorising.** The `WS_P4_5` display bug cost most of a session to wrong
theories. The fix was sitting in Waveshare's own copy of `Arduino_DSI_Display.cpp`, with a
comment naming the exact failure mode. Earlier rounds had diffed only
`Arduino_ESP32DSIPanel.cpp`. Six vendor trees sit in `reference/` and only one has ever
been diffed — see `REFERENCE_PROJECTS.md`.

**Inference from config symbols loses to direct measurement.** Several confident theories
about P4 silicon revisions were derived by reading the wrong `sdkconfig` — the framework
ships two P4 lib variants and the build used the other one. Check which artefact is
actually on the include path before reasoning about its contents.

**A test kept only because it was cheap is the one that found a real bug.** When scope has
to be cut, "never observed at all" is a better selection criterion than "most likely to
fail" — connectivity test 4 was retained on those grounds and immediately exposed the
`AP_ACTIVE` state bug.

## LVGL, from milestone 2.4

**An out-of-range grid row is a hard freeze, not a wrong layout.** `lv_conf.h` defines
`LV_ASSERT_HANDLER` as `while(1);`, so indexing a grid's row descriptor past its end locks the
board with no serial output and no reboot — indistinguishable from a hang in your own code.
`CardPage` defined exactly as many rows as *fit* the viewport while placement flowed into as many
as the cards *needed*. `WS_P4_5` survived it by luck: nine cards at five columns fill exactly
5×2 with nothing left over. `CYD_S3_3248` at two columns needs five rows and froze solid.

Two things follow. **Flash the veto board first, not second** — the tightest board is where an
off-by-one becomes a crash rather than a cosmetic bug. And when a board freezes with no output at
all, `LV_ASSERT` is a better first hypothesis than an infinite loop of your own.

**`lv_obj_align_to()` resolves against the reference object's position at the moment it is
called.** Not at layout time. Positioning a widget from a sibling that was re-aligned earlier in
the same function places it against the sibling's *stale* coordinates — which is how a
measurement card's unit ended up rendering beside the card's name instead of beside its number.
The same mistake, in three places in one file, also wrapped "Deck" onto two lines.

**The fix is to stop hand-positioning and let a layout do it.** A flex row measures after layout
rather than during render, which is the only time the measurements are real. Reach for
`lv_obj_align_to()` only against something whose position is already settled.

**`LV_LABEL_LONG_DOT` needs a real width to ellipsise against**, and silently wraps without one.
`flex_grow` on the label gives it that width; setting only the parent's does not.

**A child that must escape its parent's bounds needs `LV_OBJ_FLAG_OVERFLOW_VISIBLE`.** LVGL clips
children by default and does so silently — the widget simply is not there. The card's area tag
hangs above its cell and vanished entirely until the cell carried that flag.

## A paraphrase can outrank the spec

`docs/design/cards.md` lists card types by Home Assistant domain, and ROADMAP 2.7 names the first
four the same way. The phrase "two layout families — Measure cards and Actor cards" existed in
exactly one place: `HANDOFF.md`, as a **summary written to brief the next session**.

The next session read the summary first, promoted it to the class names, and skipped the domain
layer the spec actually called for. Nothing contradicted anything — the paraphrase was accurate
about layout and silent about types, and silence is what got implemented.

**When a handoff note compresses a spec, it becomes the spec for whoever reads it first.** The
owner caught this by instinct ("I am not a fan of the actor/measure nomenclature") well before
anyone went back to the source document. Two defences, both cheap: say *which* document a summary
is compressing, and treat a summary's vocabulary as suspect whenever it does not appear in the
thing it summarises.

## Writing C string literals through a script

`\n` inside a Python heredoc becomes a real newline in the emitted C, producing `missing
terminating " character` — a warning `HANDOFF.md` already carried, and which still cost a build
cycle in this milestone. Prefer a line-based edit for anything containing an escape sequence, and
re-read the emitted line rather than trusting the patch.


**It happened three more times at 2.5, in one session, after the lesson above had been read.**
Quoting the heredoc delimiter (`<<'PYEOF'`) is *not* enough: one level of backslash is still
consumed before Python sees the string, so a doubled escape arrives as a single one and Python
turns it into the character it denotes. The damage is not always a compile error:

| Written | Landed in the file | Symptom |
|---|---|---|
| `"\xC2\xB0"` | the two UTF-8 bytes it denotes, as real characters | file reads as "binary", compiles, renders wrong |
| `'\0'` | a literal NUL byte inside the source | file reads as "binary", string silently terminates |
| `"... px\n"` | a real newline inside a string literal | missing terminating quote - the only loud one |

Only the last one announces itself; the first two produce a file that builds and behaves wrongly.

**So the rule is not "prefer a line-based edit" - it is: never put a backslash escape in text a
script writes.** Build the backslash explicitly with `chr(92)`, or use an editor tool rather than
a script for that file. `grep` calling a source file "binary" is the tell, and anything a script
just wrote is worth a byte scan:

    python -c "d=open(F,'rb').read(); print([hex(b) for b in d if b<9 or b>126])"

A second, unrelated trap from the same session: an anchor string for a patch must not assume the
section it anchors to is followed by a blank line. This one was at the end of the file.


## Internal heap is the scarcest thing on this fleet, and nothing announces it

Three separate faults in one week all presented as "MQTT keeps dropping" and none of them were
MQTT. Internal DRAM is what the WiFi driver and LWIP take socket buffers from, and it is also
where static arrays and LVGL draw buffers land. When it runs low the network fails first, loudly,
and in a way that points everywhere except the cause.

**The three, so the shapes are recognisable:**

| What happened | Looked like | Actually was |
|---|---|---|
| `LV_MEM_SIZE` raised 128 -> 192 KB on P4 | broker refusing us | 11 KB internal heap left; no socket could be allocated |
| `DOUBLE_BUFFERING` ignored on CYD_S3_3248 | a heap leak | two 30 KB draw buffers where the BSP asked for one; board booted with 10 KB free |
| Socket never released on a detected drop | broker timing us out | a leaked TCP socket per reconnect; the broker was reaping ghosts |

**The rules that came out of it:**

- **"It links" is not a memory test.** 192 KB linked with room to spare and still broke the board.
  Linking proves the array fits in the layout; it says nothing about what is left at runtime.
  Watch `ESP.getFreeHeap()` on a board that is trying to hold a socket.
- **A falling number is a leak; a low flat number is a budget.** They need opposite fixes and the
  disconnect log could not tell them apart until it printed heap and elapsed time. It does now.
- **Below ~40 KB internal, expect network failure.** `SystemCore` prints internal free heap at boot
  and marks it.
- **`ESP.getFreeHeap()` is internal heap only.** PSRAM being 8 MB free is irrelevant to a socket.

## LVGL allocates a LAYER whenever something has to be composited

`clip_corner`, transforms, and object opacity below `LV_OPA_COVER` all force LVGL to render an
object to an intermediate buffer. **That buffer is sized by the object's WIDTH**, so it scales
with the screen and with card spans:

- a 615 px deck panel wanted **49 KB** for a single 20 px band
- a 482 px two-cell card in `HDR_BAR` wanted **38 KB**

Both failed intermittently and hung boards. `Card::build()` already avoided `clip_corner` for this
reason and said so in a comment; `UIToolkit::create_collapsible_panel()` did it anyway, months
apart, and cost an evening. **Before adding any of those three properties to a wide object, work
out what the layer will cost.**

A corollary found the same week: **a child larger than its parent also forces a layer**, because
the parent has to be composited to clip it. A "floor" on a child size that can exceed its
container is therefore not a safe fix.

## A knob that moves an input nobody can predict is not a control

The Col/Row buttons originally nudged `TARGET_CARD_W` and `ASPECT_PCT` and hoped the derivation
landed somewhere useful. On `CYD_S3_3248` it could not: rows come from how many cards there are,
so no aspect value could ever subtract a row. The button moved a number, printed a number, and
changed nothing on screen.

They now demand a count outright. The lesson generalises: **when a user wants to say "three
columns", let them say three columns.** Deriving is the right default; it is not a user interface.

Related, same session: a knob's log must report what the SYSTEM did, not what the knob set. Those
buttons printed `UI::grid()`'s estimate - "2x2 cells of 141x205" - beside per-card lines saying
the cell was 97 px tall. Two numbers describing the same thing that disagree is worse than one.

## Never put a backslash escape in text a script writes

> **The authoritative version of this now lives in `CLAUDE.md`, in the "STOP - how to edit files in
> this repo" section at the top.** It was moved there on 2026-09-18 because this file is read
> *before debugging* and the mistake happens *while editing* - so the warning sat in a document
> nobody had opened yet. The account below is kept for the failure history; the procedure to follow
> is the one in `CLAUDE.md`.

Recorded above and worth restating because it happened **six times** across two sessions, in both
directions - in the text being written *and* in the search string used to find an anchor. Quoting
the heredoc delimiter is not enough. Build the backslash with `chr(92)`, or use an editor tool.
The `grep`-says-binary tell only catches two of the three failure modes.

## Ask the far end

The MQTT fault was solved in one step by reading Mosquitto's own log, which said in plain words
what four rounds of device-side inference had not: `disconnected: exceeded timeout`. The broker,
the router's lease table, the HA device page and a ping all know things the firmware cannot.

This is the oldest rule in the project - "verify from outside the device" - and it keeps earning
its place. When a device-side theory needs a fifth iteration, stop and ask something else.

## A script that prints "ok" has not necessarily done anything

2026-09-19, and this is the **reverse direction** of the backslash rule above — the one the
`CLAUDE.md` section calls out and which still caught a session that had just written it.

`lv_indev_wait_release()` was supposed to be added to the gesture handler. The script that added it
matched on an anchor string containing `\n`. The heredoc ate one backslash, Python turned the rest
into a real newline, the anchor no longer matched anything in the file, `str.replace()` replaced
nothing, and the script printed its success message and exited 0.

**Nothing failed. Nothing warned. The call was simply never there**, and three separate user-facing
bugs were attributed to other causes for a day: a swipe's start acting as a tap, a drawer opening
and instantly closing, and a swipe-up bringing back the deck *and* expanding a panel *and*
squashing the grid.

Two defences, and the second is the one that would have caught it:

- Never put a backslash escape in text a script writes **or in the string it searches for**.
- **Confirm the file changed.** `grep -c` for the thing you just added. A replace that matched
  nothing and a replace that worked look identical from the outside, and only one of them leaves a
  trace in the file.

## Events do not reach the screen through a clickable child

LVGL delivers an event to the object that was hit and stops there unless `LV_OBJ_FLAG_EVENT_BUBBLE`
is set. A card is clickable, so a press that lands on one never reaches the screen.

This was shipped with a comment claiming the bubbling existed - "on the SCREEN with EVENT_BUBBLE set
on the things above it" - while nothing anywhere set it. The comment described the design; the code
implemented half of it, and the uninitialised press origin `{0,0}` then read as "left half, top
edge" for every gesture on the display.

**A comment that describes a mechanism in another file is a claim, and claims get checked.** Name
the file that holds the other half, so the pair can be found.

## Release the touch BEFORE an action that rebuilds the screen

LVGL delivers a gesture and then still delivers the press and click of the same finger.
`lv_indev_wait_release()` exists for exactly this, and WHERE it is called matters as much as
whether it is.

Called after the action, a rebuild has already happened - so LVGL puts new objects under a finger
it still considers pressed, and the press lands on whatever appeared there. That is how a swipe-up
to reveal the deck also expanded the first panel that materialised under the fingertip.

Release first. Then rebuild.

---

## `pio` dies with UnicodeEncodeError the moment its output is not a console

Four upload attempts in a row appeared to hang: no output, no error, timeout. They were not hanging.
They were crashing, and the crash was invisible because it happened while writing the crash to a
pipe.

PlatformIO prints box-drawing characters in the upload summary. When stdout is a terminal, Windows
renders them. When stdout is a **pipe or a file**, Python falls back to the `cp1252` console
codepage and the write raises:

    UnicodeEncodeError: 'charmap' codec can't encode characters in position 23-52

So `pio run -t upload | tail`, `| grep`, and `> file` all fail, while the identical command run
bare succeeds. The build itself already warns about this - *"Firmware metrics can not be shown. Set
the terminal codepage to utf-8"* - which reads like a cosmetic note and is not.

**Always set `PYTHONIOENCODING=utf-8` before `pio` when the output is being captured.**

```bash
export PYTHONIOENCODING=utf-8 && pio run -e <env> -t upload --upload-port COMn
```

### The second-order damage is worse than the first

A killed `pio` leaves **orphaned `esptool` and `python` children**. Those keep holding the serial
port and `.pio/build/<env>/firmware.bin`, so the next attempt fails with
`The process cannot access the file because it is being used by another process` - an error that
points at the filesystem and says nothing about the real cause. Check for strays before concluding
anything about the board:

```bash
powershell "Get-Process | Where-Object { $_.ProcessName -match 'python|esptool|pio' }"
```

---

## `Serial` does not come out of the port you flashed through

`WS_S3_TOUCH_LCD_4B` was flashed successfully over COM8 and then printed nothing but the ROM
bootloader header. The app was fine; the output was somewhere else.

The board sets `-D ARDUINO_USB_CDC_ON_BOOT=1`, which maps Arduino's `Serial` to the ESP32-S3's
**native USB CDC**, not to UART0. COM8 is the CH343 bridge - correct for flashing, and permanently
silent for application output. The CDC port is a *separate* device that only enumerates once the
app is running (`VID_303A&PID_1001`).

If the native USB socket is not physically cabled, there is no way to read that board's log at all,
however well it is running. Six of the eight environments set this flag; `CYD_P4_1060P470` is the
one that explicitly sets it to `0` with a comment saying native USB is not supported there.

**Before debugging silence, check which transport the board's `Serial` is on:**

```bash
grep -A3 "^\[env:<NAME>\]" platformio.ini | grep USB_CDC_ON_BOOT
powershell "Get-CimInstance Win32_PnPEntity | Where-Object { $_.Name -match 'COM\d+' } | Select-Object Name, DeviceID"
```

A `VID_303A` device is an Espressif CDC port and is where the log is. A `VID_1A86` device is a CH343
bridge and is not.
