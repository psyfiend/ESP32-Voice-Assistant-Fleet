# Building the voice-assistant half — signal path, integration targets, MVP

**Status: research only, no decision.** Written 2026-09-10. Tags: **verified in `<path>`**
(read directly in this repo), **verified via web** (fetched/searched this session, source
cited), **believed** (confident, not directly checked this session), **unknown**.

**Headline finding that changes the shape of this whole report:** the previous attempt to port
IDF audio features into Arduino and shelve it (CLAUDE.md, `ENABLE_AEC` off) predates a piece of
groundwork Espressif has since shipped *inside Arduino itself*. `arduino-esp32` **3.3.11 — the
exact core version this project already pins** — bundles a built-in `ESP_SR` library
(`libraries/ESP_SR/`, verified by reading the actual installed files at
`C:\Users\Marge\.platformio\packages\framework-arduinoespressif32\libraries\ESP_SR\`) that wraps
WakeNet (wake word) + MultiNet (fixed-command recognition) **and internally initializes AFE**
(`afe_config_init()` / `afe_handle->fetch()`, verified by reading `esp32-hal-sr.c`) — Espressif's
audio front end that includes AEC, noise suppression and VAD. It is gated to
`CONFIG_IDF_TARGET_ESP32S3 || CONFIG_IDF_TARGET_ESP32P4` (verified in `ESP_SR.h`) — both chip
families this fleet uses. **This means on-device wake word, and possibly AEC, may not require
the ESP-IDF migration in Report 1 at all.** Section 3 covers this in detail, including the real
integration friction it has with this project's own `AudioManager`.

---

## 1. Audio in/out — `esp_codec_dev`, `audio_hal`, and what survives

**Current state, verified in `components/AudioManager/`:** `AudioManager.cpp` already calls
native ESP-IDF I2S (`i2s_new_channel`, `i2s_channel_init_std_mode`/`_tdm_mode`,
`i2s_channel_read`/`write`) directly, not an Arduino I2S wrapper. `es7210.cpp`/`es8311.cpp` are,
per CLAUDE.md, adapted from Espressif's own reference drivers and follow the `audio_hal` pattern
(`audio_hal_codec_config_t`, `AUDIO_HAL_CODEC_MODE_*`) — the same shape Espressif's own BSPs use.
Concretely, `AudioManager::initCodecOutput()`/`initCodecInput()` (verified,
`AudioManager.cpp:195-296`) already implement the mutually-exclusive mic-path logic CLAUDE.md
documents: ES7210 present → it owns all mic input and ES8311 stays `DECODE`-only; ES7210 absent
→ ES8311 runs `AUDIO_HAL_CODEC_MODE_BOTH` and uses its own built-in ADC.

**`esp_codec_dev`** (verified via web this session — see the audio-pipeline research below):
lives inside the `esp-adf` monorepo at `components/esp_codec_dev`, and is also published
standalone on the ESP Component Registry as `espressif/esp_codec_dev` (latest v1.6.2). It
explicitly supports both ES8311 and ES7210 among ~14 codecs, with a POSIX-like
open/read/write/close API (`esp_codec_dev_open/read/write/close`,
`esp_codec_dev_set_out_vol`/`_set_in_gain`) that separates the I2C control path from the I2S
data path. Espressif's own `esp-bsp` project has **deprecated its standalone ES8311 driver in
favor of `esp_codec_dev`** — a clear signal of current direction, not a legacy alternative.

**Should we adopt it?** Not a clean win, and not required. `esp_codec_dev` would replace two
hand-adapted driver files with one standardized abstraction, but it wants to *own* the I2S data
path itself — handing that over is real integration cost against the existing 4-channel
TDM/AEC wiring already built in `AudioManager` (the `ENABLE_AEC` path, `i2s_tdm_config_t`,
verified in `AudioManager.cpp:88-118`). Net effect: different abstraction, similar amount of
code, not obviously less work. **Recommendation: keep the current hand-rolled drivers unless a
specific `esp_codec_dev`-only feature is needed** (e.g. its device-agnostic API matters if the
fleet ever adds a codec chip `es7210.cpp`/`es8311.cpp` don't cover).

---

## 2. AEC (acoustic echo cancellation)

**What actually provides it:** esp-sr's **AFE** (Audio Front End), not a separate esp-adf audio
processor. AFE bundles AEC + noise suppression (NS) + voice activity detection (VAD) + blind
source separation (BSS) + a deep-NS model (NSNET) as one pipeline that also feeds WakeNet
(verified via web, `docs.espressif.com/projects/esp-sr/.../audio_front_end/`).

**Chip support — the P4 question, answered:** AFE/AEC runs on **both S3 and P4**, verified via
web against esp-sr's own README and its parallel P4 documentation tree
(`docs.espressif.com/projects/esp-sr/en/latest/esp32p4/...`). This is not a token or lagging
port — esp-sr's README notes P4-specific beamforming/DOA work added as recently as August 2026.
Espressif's own guidance names **S3 and P4 as the two chips it recommends for speech tasks**
specifically. Whether P4's RISC-V core (no vector ISA extensions the way S3's Xtensa core has)
fully substitutes for S3's DSP-oriented instructions is not stated explicitly anywhere found this
session (**unknown**), but the benchmark numbers below suggest comparable-or-better throughput
at a higher absolute CPU percentage — i.e. P4 spends more raw clock cycles to get there, which is
plausible given its much higher clock and larger core count budget.

**Real numbers, verified via web against esp-sr's own published benchmark pages** (S3
`MR,VC LOW_COST` config vs. the closest P4 config):

| | ESP32-S3 | ESP32-P4 |
|---|---|---|
| Internal RAM | 48.7-91.1 KB | 68-79 KB |
| PSRAM | ~820 KB | ~1150-1200 KB |
| CPU, feed stage | 30.6-32.2% of one core | 23.7-24.9% of one core |
| CPU, fetch stage | 4.7% | 22.9% |

Standalone flash size for the AFE library binary itself was not published as a separate number on
either benchmark page (**unknown**) — flash cost in practice is dominated by whichever
WakeNet/MultiNet models get linked alongside it (§3).

**Why AEC is generally required before wake-word works on a device that also plays audio:** the
device's own speaker output is picked up by its own microphone as a loud, temporally-correlated
"echo" mixed directly into the signal WakeNet is scanning. Without cancelling it, that self-noise
either swamps genuine speech (missed wake-ups) or gets misdetected as speech itself (false
triggers on the TV, on TTS replies, on music). Espressif explicitly benchmarks an "interrupting
wake-up rate" — accuracy while the device is itself playing audio — as a required metric for any
AEC-capable product, which is a fairly strong statement that this is treated as a hard
prerequisite for barge-in, not a nice-to-have.

**The alternative that sidesteps AEC entirely, and is legitimate:** mute/pause the microphone
path (or simply don't run WakeNet) while `AudioManager` is actively playing TTS or a tone, and
resume listening once playback ends. This trades "can interrupt the assistant mid-sentence" for
"zero AEC engineering," which is a completely reasonable trade for a first version — see §5.

---

## 3. Wake word + ASR — WakeNet, MultiNet, and the Arduino-bundled `ESP_SR` library

**On-device vs. off-device:** WakeNet always runs on-device (that's its purpose). MultiNet also
runs entirely on-device, but it is a **fixed-vocabulary command matcher** (up to ~300 trained
phrases), not open-ended transcription — verified via web against esp-sr's README. For arbitrary
speech ("what's the weather going to do this afternoon"), MultiNet is the wrong tool; that needs
real ASR, which today means sending audio somewhere with an actual speech-to-text model — off
this class of MCU (see §4).

**Costs, verified via web against esp-sr's benchmark pages** (S3 / P4):

| Model | Internal RAM | PSRAM | Compute |
|---|---|---|---|
| WakeNet9 (2ch, quantized) | 16 KB | 324 KB | ~3.0 ms/frame (S3) / 2.6 ms/frame (P4) |
| WakeNet10 (3ch) | 16-17 KB | ~523 KB | 14-23% of one core |
| MultiNet7 | 18 KB | ~2920 KB | 8-11 ms/frame |

PSRAM (model weights) dominates cost; internal RAM stays in the tens of KB. Per-model flash
image sizes weren't separately published (**unknown**); TTS models were incidentally noted at
2.2 MB flash, which is not relevant to a device-side wake word/command path.

**Chip support:** WakeNet9/9l/9s and WakeNet10, and MultiNet7, all explicitly list ESP32-S3 and
**ESP32-P4** — verified via web against esp-sr's README. Espressif's current framing puts S3 and
P4 on equal footing for speech work, ahead of the smaller C-series chips.

### 3.1 The bundled Arduino `ESP_SR` library — verified by reading the actual installed files

> **Second pass, 2026-09-10 — independently re-verified, and the picture is better than this
> section originally concluded.** Every item below was read off this machine, not inferred.
>
> | Check | Result |
> |---|---|
> | `libraries/ESP_SR/` in core `3.3.11` | **present** — the exact pinned core |
> | Target gate in `ESP_SR.h` | `CONFIG_IDF_TARGET_ESP32S3 \|\| CONFIG_IDF_TARGET_ESP32P4` — **all 8 boards** |
> | Second gate: `CONFIG_MODEL_IN_FLASH \|\| CONFIG_MODEL_IN_SDCARD` | **already satisfied** — `CONFIG_MODEL_IN_FLASH=y` in the prebuilt `sdkconfig` for **both** `esp32s3` and `esp32p4_es` |
> | Wake-word model selected | `CONFIG_SR_WN_WN9_HIESP=y` — "Hi ESP" enabled by default |
> | Archives | `libespressif__esp-sr.a`, `libwakenet.a`, `libmultinet.a`, `libdl_lib.a` all shipped |
> | Linked by default? | **yes** — `-lespressif__esp-sr` is already in `flags/ld_libs` |
> | Model blob | `esp32s3/esp_sr/srmodels.bin` ships with the package |
>
> **So the only two things standing between this project and on-device wake word are:**
>
> 1. A `model` partition in the partition table, with `srmodels.bin` flashed into it. That is a
>    real change — `board_build.partitions` is load-bearing on every environment (see the note in
>    `platformio.ini`) and the current `default_16MB.csv` has no such partition.
> 2. A `sr_fill_cb` that hands audio buffers to the recogniser.
>
> **The "I2S ownership conflict" this report worried about is smaller than feared.** `sr_start()`
> takes `sr_fill_cb fill_cb` — a *pull* callback (`esp32-hal-sr.h:49`). ESP_SR does not open or own
> an I2S channel; it asks the caller for PCM. `AudioManager` keeps ownership of the codec and the
> I2S channel exactly as it does today and simply serves buffers on request. That is a much
> smaller change than handing the peripheral over.
>
> Nothing here has been *run*. "Links and is configured" and "detects a wake word on a board in a
> room" are still different claims, and only the second one matters.



This is new information not implied by anything in this project's existing docs, so it's worth
being precise about exactly what was read. Files:
`C:\Users\Marge\.platformio\packages\framework-arduinoespressif32\libraries\ESP_SR\src\ESP_SR.h`,
`esp32-hal-sr.h`, `esp32-hal-sr.c`, and the partition table
`...\tools\partitions\esp_sr_16.csv` — all physically present on this machine already, as part
of the exact arduino-esp32 3.3.11 package this project's `platformio.ini` pins.

- **Gating, verified in `ESP_SR.h`:** `#if (CONFIG_IDF_TARGET_ESP32S3 || CONFIG_IDF_TARGET_ESP32P4)
  && (CONFIG_MODEL_IN_FLASH || CONFIG_MODEL_IN_SDCARD)`. Both our chip families qualify. Whether
  `CONFIG_MODEL_IN_FLASH` is already `y` in this fleet's default sdkconfig, or needs a custom
  sdkconfig fragment (the kind `platformio.ini`'s `flag_custom_sdkconfig` machinery, referenced
  in Report 1 §4, exists to apply) is **unknown** — not checked this session, and worth
  confirming before assuming this compiles out of the box.
- **API shape, verified in `ESP_SR.h`:** a single `ESP_SR_Class` with `begin(I2SClass &i2s,
  const sr_cmd_t *commands, size_t len, sr_channels_t rx_chan, sr_mode_t mode, const char
  *input_format)`, an `onEvent(sr_cb)` callback delivering wake-word/command/timeout events, and
  `pause()`/`resume()`. This is a genuinely small, approachable surface — closer to a typical
  Arduino library than to raw esp-sr.
- **AFE is used internally, verified in `esp32-hal-sr.c`:** it calls `afe_config_init()` and
  drains audio via `afe_handle->fetch()` before feeding WakeNet/MultiNet — meaning NS/VAD (and
  potentially AEC) preprocessing is genuinely present, not just the raw neural nets. **What is
  not confirmed:** whether AEC specifically is enabled/reachable through this wrapper's public
  surface, as opposed to just NS+VAD (**unknown** — would need either a source read one level
  deeper than this session went, or a bench test).
- **The `input_format` string is the actual answer to "does this support our AEC wiring":**
  verified in `ESP_SR.h`'s doc comment — `"M"` = microphone channel, `"R"` = **playback reference
  channel**, `"N"` = unused. Example given: `"MMNR"` — two mics, one unused slot, one reference
  channel. **This is structurally the same 4-channel topology `AudioManager.h`'s own
  `ENABLE_AEC` comment already describes** ("4-Channel TDM Input (Mic1/2 + Ref1/2)", verified
  `AudioManager.h:14-16`) and the same TDM slot layout `AudioManager.cpp`'s `tdm_cfg` already
  builds (verified, `AudioManager.cpp:88-118`, four TDM slots, currently wired for exactly this
  channel count). **This is the strongest concrete lead in this whole report**: the previously-
  shelved AEC wiring appears to already be shaped the way Espressif's own bundled Arduino
  wrapper expects its input, which was not true (or at least not confirmable) at the time it was
  shelved, before this bundled library existed in the pinned core version.
- **The integration friction, verified in `ESP_SR.h`:** `begin()` takes an **Arduino
  `I2SClass&` (`ESP_I2S.h`)**, not a raw `i2s_chan_handle_t`. `AudioManager` currently owns the
  I2S peripheral directly via native calls and never constructs an `I2SClass`. Only one owner can
  bind a given I2S peripheral/pin set at a time, so adopting `ESP_SR` as-is means either (a)
  rewriting `AudioManager`'s RX side to go through `I2SClass` instead of raw `i2s_chan_handle_t`
  so `ESP_SR` can be handed the same object, or (b) some other arrangement neither read nor
  designed this session. **This is real work, not a drop-in**, and is the first thing to spike
  before committing to this path.
- **Partition table, verified in `esp_sr_16.csv`:** a dedicated 16MB layout — `app0`/`app1` at 3MB
  each (vs. this project's current unspecified default OTA split under `default_16MB.csv`), a 6MB
  `spiffs` partition, and a **dedicated 3.9MB `model` partition** (`0xC10000`, size `0x3E0000`,
  mounted as spiffs) for WakeNet/MultiNet model files. Adopting `ESP_SR` means adopting this (or
  an equivalent custom) partition table — which is exactly the kind of change ROADMAP §6.10
  already flags as needing to be decided **before** the first OTA-capable release, because
  changing it later needs a full serial reflash of every board, not an OTA push.

**Net read on §3:** on-device wake word (and command recognition for a small fixed set) is
reachable **without** the ESP-IDF migration from Report 1, using tooling already sitting on this
machine in the exact pinned framework version. AEC specifically is plausible but unconfirmed
through this path — that's the one gap worth a half-day spike rather than a guess either way.

---

## 4. Two integration targets

### 4a. Local LLM wired to Home Assistant

**HA's Assist pipeline** (verified via web, `developers.home-assistant.io/docs/voice/pipelines`)
is four independently-swappable stages: wake word → STT → intent/conversation agent → TTS. A
fully local stack (Whisper + HA's own intent engine or a local LLM via Ollama + Piper +
openWakeWord) is an officially documented HA configuration, not a stretch goal — verified at
`home-assistant.io/voice_control/voice_remote_local_assistant`.

Three ways an external device participates, in order of how much this project would have to
build:

1. **Raw MQTT + custom scripting.** Lowest conceptual lift (MQTT already works end-to-end here),
   but there is no standard "send audio" MQTT schema and no documented native path for MQTT audio
   into `assist_pipeline` (**unknown/believed — not found**). You'd be invalidating the whole
   point of using a standard integration: writing a bespoke HA-side automation to shuttle audio
   out of a topic and into the pipeline, which nobody else's HA install understands. Most
   flexible, least supported.
2. **Wyoming protocol.** A small, transport-agnostic protocol — one-line JSON header + optional
   raw-PCM binary payload over TCP (verified via web, `github.com/rhasspy/wyoming` README).
   Minimum satellite exchange: `audio-start` → repeated `audio-chunk` → `audio-stop`, plus
   `detect`/`detection` for wake-word events. Built directly into HA core since 2023.5 (verified,
   `home-assistant.io/integrations/wyoming`) — **no separate add-on needed** for the protocol
   itself, though the actual STT/TTS/wake-word services (Whisper, Piper, openWakeWord) run as
   their own add-ons regardless of transport. **Important and specific finding:** the reference
   Python satellite implementation, `rhasspy/wyoming-satellite`, is **now unmaintained** — its own
   README says it "has been replaced by Linux Voice Assistant that uses the ESPHome protocol"
   (verified via web). No bare-metal C/C++ Wyoming satellite for ESP32 was found in this
   session's research (**unknown** whether one exists anywhere) — every real-world example
   targets a Raspberry Pi running Python. Implementing Wyoming's wire format in C++ on an ESP32
   is plausible on protocol-simplicity grounds, but you'd be pioneering the "bare ESP32 Wyoming
   satellite" niche essentially alone, against the ecosystem's own stated direction away from it.
3. **ESPHome's native API, voice-assistant subset.** Protobuf-over-TCP on port 6053, with a
   public, actively-maintained `.proto` schema (verified,
   `github.com/esphome/aioesphomeapi/blob/main/aioesphomeapi/api.proto`). Framing is documented
   well enough to hand-implement: a plaintext mode (no handshake — `0x00` + varint length + varint
   type + protobuf body) or an optional Noise-encrypted mode (verified,
   `developers.esphome.io/architecture/api/protocol_details`). The voice-relevant slice
   (`DeviceInfoRequest/Response`, `SubscribeVoiceAssistantRequest`,
   `VoiceAssistantConfigurationRequest/SetConfiguration`, `MediaPlayerCommandRequest`) is a small
   fraction of the full ~2861-line proto (**believed**, roughly 10-15% of message types, not
   independently counted with precision this session). This is the direction the ecosystem is
   actually moving — even Wyoming's own now-deprecated satellite project points here as its
   replacement's chosen protocol. Higher up-front parsing cost than Wyoming, no HA add-on
   required beyond the built-in ESPHome integration, and it's the actively-maintained target.

**Recommendation for this project specifically:** neither Wyoming nor the ESPHome subset is
obviously "the small option" — Wyoming is simpler to parse but pointed away from; ESPHome's
subset is more code but is where the ecosystem is actually going and buys on-device wake-word
handoff, timers, and "continue conversation" for free later. Given this project already has a
proven MQTT+HA pipeline, **the pragmatic MVP (§5) skips both** and uses HA the same way the
dashboard already does — publish a recognized command as an MQTT message the same shape as
every other outbound entity write — deferring the Wyoming-vs-ESPHome-API decision until
open-ended (non-fixed-vocabulary) speech is actually wanted.

### 4b. Cloud assistant (Claude/Gemini) driving Home Assistant via MCP

**Audio input differs sharply between the two named vendors — verified via web:** the Claude API
accepts `text`, image, and PDF/document content blocks; **there is no audio content type today**
(`platform.claude.com/docs/en/build-with-claude/vision`). A device talking to Claude must do its
own STT and send text. Gemini's Live API, by contrast, accepts native audio input/output for
low-latency bidirectional voice (`ai.google.dev/gemini-api/docs/live-api`) — a materially
different device-side design (stream raw audio) than the Claude path (transcribe locally or via
a separate cloud STT call, then send text).

**MCP side is real and already shipped:** Home Assistant has an official **MCP Server
integration** exposing exposed entities/Assist intents as MCP tools
(`home-assistant.io/integrations/mcp_server`, verified via web) — so "cloud LLM drives HA" is
solved on the HA end already, independent of anything this project builds. MCP itself carries no
audio primitive; it only starts once you already have text (a transcribed command or a model's
tool-call decision).

**What the device side needs, regardless of vendor:**
- **Endpointing/VAD** — knowing when the user stopped talking. esp-sr's AFE (§2/§3) already
  includes a VAD stage (VADNet), so this is available on-device if the WakeNet/AFE path is
  already running for local wake-word detection — one more reason a local wake-word stage is
  worth having even in a cloud-backed design, rather than always-streaming audio.
- **HTTPS/WebSocket path with TLS.** `WiFiClientSecure` (arduino-esp32) supports a certificate
  bundle mechanism (`esp_crt_bundle`, Mozilla's root CA set) — a filtered ~38-certificate subset
  covers an estimated ~93% of real-world TLS endpoints at roughly **64 KB flash**, verified via
  web against `docs.espressif.com/.../esp_crt_bundle.html`. This is a solved problem on this
  platform, not a research gap.
- **Latency and cost, believed/directional only** (aggregated from industry pricing/benchmark
  writeups, not vendor pricing pages checked directly this session): a non-streaming cloud
  STT→LLM→TTS round trip for a short query typically lands in the **1-3 second** range;
  streaming/realtime designs target well under a second. Per-query cost is small — streaming STT
  pricing clusters around $0.0015-$0.024/minute, bundled realtime voice-agent pricing around
  $0.03-$0.07/minute of audio — meaning a several-second query plus a short reply is typically a
  fraction of a cent to a few cents. Treat these numbers as directional or budget-planning-only
  until checked against current vendor pricing pages at implementation time.

---

## 5. The realistic minimum viable version

**Smallest thing that would actually work, and why this shape specifically:**

**On-device wake word + a small fixed command set, muting the mic during playback, publishing
recognized commands over the existing MQTT/Entity Registry pipeline. Board: `WS_P4_TOUCH_LCD_5`.**

Reasoning, tying together everything above:

- **It needs no AEC.** Pausing WakeNet/MultiNet while `AudioManager` is playing anything sidesteps
  the entire §2 problem. A fixed fleet-wide UX rule ("the assistant doesn't talk over itself, and
  doesn't listen while replying") is a legitimate, common design for a v1 device — it's the same
  trade every cheap smart speaker made before far-field AEC was standard.
- **It needs no cloud, no Wyoming, no ESPHome protocol, no HA Assist pipeline integration at
  all.** MultiNet's fixed-vocabulary matching (§3) is enough for "turn on the kitchen light" /
  "turn off the fan" style commands, and this project already has a working, proven path from a
  recognized event to Home Assistant: write to `EntityRegistry`, let `MqttProvider`/`HaPublisher`
  carry it out over the MQTT session that's been hardware-verified on 8 boards since Phase 1.
  Zero new integration surface on the HA side.
- **It plausibly needs no ESP-IDF migration.** Per §3.1, the bundled `arduino-esp32` 3.3.11
  `ESP_SR` library targets exactly this shape (WakeNet wake word → MultiNet fixed commands) and
  is gated to S3/P4 — this fleet's exact two chip families — without requiring Report 1's
  migration. The two real risks are the `I2SClass`-vs-native-I2S ownership conflict with
  `AudioManager` (§3.1) and the partition-table change, both spike-able in isolation before
  committing.
- **Board choice: `WS_P4_TOUCH_LCD_5`, not a stress-test board.** It is already this project's
  primary dev target (ROADMAP §Q11) with the display/touch/connectivity/MQTT stack proven on
  real hardware. It has **both** ES7210 (4-channel mic ADC — the multi-mic input WakeNet/AFE
  wants) and ES8311 (speaker output) per its `HAS_ES7210`/`HAS_ES8311` build flags — verified in
  `platformio.ini`. It is a P4 board, one of the two chip families esp-sr explicitly recommends
  for speech work. By contrast, `CYD_S3_3248` (the fleet's other dev/stress target) has **neither
  codec chip** — verified in `BSP_CYD_S3_3248W535.h`, its `AudioConfig` is an NS4168 power-amp,
  output-only, no `I2S_DIN` wired at all — so it is not a candidate for this feature without a
  hardware change, regardless of framework choice.
- **What it deliberately defers:** open-ended speech (needs real ASR — cloud, or a local
  server-class STT this class of MCU cannot run itself), barge-in/AEC (§2), and any HA
  Assist-pipeline/Wyoming/ESPHome-protocol integration (§4a) — all real, all bigger, all
  buildable later on top of a v1 that already proves the on-device wake-word-to-MQTT path works
  on real hardware.

---

## 6. `esp-adf`'s `wifi_service` — what it is, and whether we should use it

The owner asked specifically about
`github.com/espressif/esp-adf/tree/release/v2.x/components/wifi_service`. Everything below is
verified via web this session by reading the actual source files at that path (headers,
`CMakeLists.txt`, and `wifi_service.c`), not inferred from esp-adf's general reputation.

**What it does:** more than a thin connect/disconnect wrapper. `wifi_service.h` exposes a small
state machine — `wifi_service_create/destroy`, `_connect/_disconnect`, `_set_sta_info`,
`_update_sta_info`, status/diagnostics (`_state_get`, `_disconnect_reason_get`), and a persisted-
credential store (`wifi_ssid_manager.h`, NVS-backed, with `_erase_ssid_manager_info` /
`_get_last_ssid_cfg`). Provisioning transports (see below) register into it as plugins through
`esp_wifi_setting.h`'s handle type (`create` + `start`/`stop`/`teardown` function pointers), via
`wifi_service_register_setting_handle()` / `_setting_start()`/`_setting_stop()`.

**What it depends on — and the coupling is real, not just declared.** Verified in
`wifi_service/CMakeLists.txt`: `REQUIRES bt mbedtls nvs_flash (esp_timer on IDF>=5.0)`,
`PRIV_REQUIRES audio_sal esp_dispatcher esp_actions`. One non-obvious structural fact: **`periph_service.c`/`.h` — the generic "service" lifecycle abstraction — physically lives inside
the `esp_dispatcher` component**, not inside `esp_peripherals` as the name might suggest.
`esp_dispatcher`'s own README frames "Peripheral Service" as one of its two built-in service
kinds (the other is "Audio Service"), explicitly naming WiFi/network-config as a peripheral-
service use case. `wifi_service.c` itself directly includes `esp_action_def.h`/`esp_delegate.h`
from `esp_actions` and implements its own init as a registered "action" — this is genuine
architectural coupling to esp-adf's action/dispatch framework, not an optional convenience.
`esp_peripherals` (the older, separate component with its own simpler `periph_wifi.h`) is **not**
required — `wifi_service` and `periph_wifi` are two independent, non-interoperating WiFi-setup
abstractions that happen to both live in esp-adf. Pulling `wifi_service` into a plain
ESP-IDF/Arduino project would mean vendoring `wifi_service` + `esp_dispatcher` (which is where
`periph_service` actually lives) + `esp_actions` + the lightweight `audio_sal` shim — real but
bounded; **not** the whole of esp-adf's audio pipeline.

**Provisioning transports, verified in the component's own headers:** BluFi (BLE,
`blufi_config.h`), SmartConfig/ESPTouch (`smart_config.h`, wraps `esp_smartconfig.h`), and
AirKiss (`airkiss_config.h`) — with AirKiss explicitly excluded from ESP32-C3/C6/**P4**/C5 builds
per its own `CMakeLists.txt`, which matters directly for this fleet's four P4 boards. Espressif's
separate, more standard `wifi_provisioning` component (BLE+SoftAP via `protocomm`) still existed
in IDF v5.3's tree at the time of checking but no longer appears in current IDF's top-level
component listing, with docs now describing a newer "Unified Provisioning" — the exact successor
name/location is **unknown** (not traced further this session). What is confirmed: esp-adf's
`wifi_service` and IDF's `wifi_provisioning` are **independent, non-interoperating**
implementations of the same general idea. esp-adf did not adopt or wrap `wifi_provisioning`.

**Maintenance:** actively maintained — the most recent commit touching this directory at the
time of checking was 2025-09-18 (a P4/`esp_hosted` bugfix), with 2025 commits adding IDF-5.4
support and a BluFi CLI example.

**License — worth flagging on its own, since it changes the framing of "could we use it":** esp-adf
is **not** Apache-2.0. Its root `LICENSE` and every checked source file header carry the
"ESPRESSIF MIT License" — a modified MIT grant restricted to use *on Espressif Systems products*.
That's not a blocker for this fleet (it's entirely Espressif silicon), but it is a materially
different grant than the permissive, hardware-agnostic license the question's framing assumed,
and worth knowing before citing it as "just Apache/MIT" anywhere.

### 6.1 Comparison with `Fleet_Connectivity/ConnectivityManager`

**Verdict: `wifi_service` does not offer anything `ConnectivityManager` doesn't already have for
this project's actual use case, and it would cost a real dependency tree to adopt.**

| Capability | `ConnectivityManager` (ours) | `wifi_service` |
|---|---|---|
| Disconnect-reason classification | **Yes** — `classifyDisconnect()` maps `wifi_err_reason_t` to `JoinResult` (AUTH/NO_AP/transient), verified `ConnectivityManager.cpp:179-191` | `_disconnect_reason_get()` exposes the raw reason; no evidence of built-in classification into an equivalent taxonomy |
| Multiple connectivity modes | **Yes** — 4 modes (`OFF`/`STA_WITH_AP_FALLBACK`/`STA_PLUS_AP`/`STA_ONLY`), verified `ConnectivityTypes.h` | Connect/disconnect + provisioning; no equivalent mode vocabulary found |
| Retry ladder with backoff | **Yes** — AP-mode STA retry doubles 120s→1800min-capped-at-30min, verified `ConnectivityManager.cpp:26-33, 512-563` | Not found in the API surface read this session (**unknown** whether internal retry exists — not exposed as a documented policy either way) |
| AP fallback | **Yes** — `raiseAp()`, AP-client deferral, idle timeout, verified throughout `ConnectivityManager.cpp` | Not part of `wifi_service`'s scope at all — it's STA-connect-and-provision, not an AP-fallback rescue path |
| Proven/unproven credentials | **Yes** — `_proven`, NVS-backed, distinguishes "never worked, stop retrying" from "worked before, keep trying," verified `ConnectivityManager.h:91` | No equivalent found |
| Provisioning transport (BluFi/SmartConfig) | **No** — not built, no issue tracks it | **Yes** — this is the one genuinely new capability |

**The one real, specific thing `wifi_service` offers that this project does not have: a
provisioning transport that doesn't need a phone to join the device's own AP and use a web
form** — BluFi (BLE) or SmartConfig let a phone push credentials without ever associating to the
device's network. That's a real UX improvement over "join `Fleet-XXXXXX`, open a captive portal"
(ROADMAP #6, currently descoped to Phase 4 anyway). But it comes bundled with `esp_dispatcher` +
`esp_actions` + `audio_sal` + Bluetooth + mbedTLS as hard dependencies, all to get a feature this
project could add far more cheaply via IDF's own `wifi_provisioning` (BLE/SoftAP), or by simply
keeping the AP+captive-portal path already on the roadmap. **Recommendation: do not adopt
`wifi_service`.** If BLE provisioning is ever wanted, evaluate IDF's own `wifi_provisioning`
component on its own terms (not researched in depth this session — flagged as a better-targeted
follow-up than esp-adf's version, precisely because it doesn't drag in the audio/action
dispatch framework this project doesn't use anywhere else). Everything else `wifi_service`
does, `ConnectivityManager` already does, mostly with more nuance for this fleet's specific
failure modes (the CYD_S3_3248 brownout-under-radio-load concern, the AP-client-deferral logic)
than `wifi_service`'s API surface suggests it has.
