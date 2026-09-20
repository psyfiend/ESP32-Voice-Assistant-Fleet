# esp_websocket_client — vendored, unmodified

Espressif's official WebSocket client, taken from
[`espressif/esp-protocols`](https://github.com/espressif/esp-protocols/tree/master/components/esp_websocket_client).

| | |
|---|---|
| **Upstream tag** | `websocket-v1.6.1` |
| **Fetched** | 2026-09-20 |
| **Licence** | Apache-2.0 (`LICENSE`, shipped verbatim) |
| **Local modifications** | **none** |

## Why this is a copy instead of a dependency

`esp_websocket_client` is an official Espressif component, but it lives in the **Component
Registry**, not in ESP-IDF core — which is exactly why it is absent from the Arduino prebuilt
libraries. PlatformIO's Arduino build cannot run the IDF component manager, so there is no way to
pull it. It is vendored here for the same reason `bb_captouch_fork` is: the alternative is writing
our own, and nobody should write their own WebSocket client.

Version `1.6.1` was not guessed. `reference/Examples and related projects/ESP32-P4-NINA-Display/main/idf_component.yml`
pins `espressif/esp_websocket_client: "~1.6.1"` against ESP-IDF 5.5.2 on a P4; our Arduino
framework is built against **ESP-IDF 5.5.5**. That is a known-good pairing on near-identical
hardware rather than "the newest tag".

## What it needs, and why nothing else had to be vendored

Its entire dependency set already ships with the Arduino framework, for **both** `esp32p4` and
`esp32s3`. This was verified before downloading anything, not assumed:

| Needs | Where it already is |
|---|---|
| `esp_transport.h`, `_tcp`, `_ssl`, **`_ws`** | `include/tcp_transport/include/`, and `libtcp_transport.a` |
| `http_parser.h` | `include/http_parser/` |
| `esp_tls_crypto.h` | `include/esp-tls/esp-tls-crypto/` |
| freertos, `esp_log`, `esp_timer`, `esp_system` | core |

**The WebSocket framing layer was already linked in.** `libtcp_transport.a` contains
`transport_ws.c.obj` — confirmed with `riscv32-esp-elf-ar t`. This component is the client *around*
that: the background task, the event loop, reconnection, keepalive ping/pong and fragment
reassembly. That is the part worth having and the part nobody wants to reimplement.

## Kconfig

Upstream exposes three `CONFIG_ESP_WS_CLIENT_*` options and **all three default to off**, so an
Arduino build with no Kconfig at all gets upstream's defaults by simply leaving the macros
undefined. Nothing needs to be passed in `build_flags`.

If dynamic buffers (`ESP_WS_CLIENT_ENABLE_DYNAMIC_BUFFER`, worth ~2 KB when idle) are ever wanted,
that becomes a `-D` in `platformio.ini` — not an edit to this tree.

## Updating it

Keep it unmodified. If a newer version is needed, replace both files from the upstream tag and
update the table above. If a local change ever becomes unavoidable, say so here and say why, in the
manner of `components/GFX_Library_for_Arduino`.

Byte-verified after download: 67,604 B for the source and 23,185 B for the header, matching the
sizes reported by the GitHub tree API, LF line endings, no stray control characters.

**A note before you diff this against upstream.** Git on this machine converts line endings on
checkout, so the working copy is CRLF while the repository and upstream are both LF. A naive diff
will report every line as changed. Compare with `git diff --ignore-cr-at-eol`, or diff the blob
rather than the file on disk. The files really are unmodified.
