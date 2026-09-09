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
