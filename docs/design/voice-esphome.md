# Voice and the ESPHome native API (Phase 7)

**Status: intent, not design.** Recorded 2026-11-02 (DECISIONS A18) so that the choices made before
Phase 7 do not block it. Nothing here is built; the design is done when the phase is placed in the
running order.

## What we want

1. **HA sees each board as an ESPHome device**, adopted through its ESPHome integration: its
   controls, sensors and screen as entities, managed from HA's device page. Optional, for those who
   want it; MQTT stays and keeps carrying device states.
2. **Voice into HA's Assist pipeline**: wake word on the board, audio to Assist, the reply on the
   board's speaker - an `assist_satellite` entity, as HA's Voice PE and Kiosk Satellite are.

## Why this route

**Kiosk Satellite proves it works without ESPHome firmware.** It is an Android app that implements
ESPHome's native API itself: port 6053, Noise encryption with a key the app generates, HA adopting
it as an ordinary ESPHome device. Voice runs on that same connection; the wake word runs on the
device. Our boards can do the same from our own ESP-IDF firmware. The protocol is protobuf over TCP,
defined in ESPHome's `api.proto`; HA's own client is `aioesphomeapi` (Python).

Other implementations worth reading when the phase starts:

- `OHF-Voice/linux-voice-assistant` - a Linux voice satellite that speaks the ESPHome API; the
  closest model for 7.2's audio side.
- `LRuesink-WebArray/esphome-native-api` - the API in Node.js; a second reading of the protocol.
- Kiosk Satellite (`jxlarrea/kiosk-satellite`, `kiosksatellite.com/docs/esphome/`) - which entity
  types it exposes, how it carries notifications as ESPHome actions, its screenshot camera.

**Not Wyoming.** Its satellite has been replaced by the ESPHome-API one above; Wyoming remains the
protocol HA uses to reach local speech engines (Whisper, Piper), which is HA's side and not ours.

## Constraints the earlier milestones respect

- **ESPHome entities carry no attributes, and an HA state is at most 255 characters.** A board's
  whole configuration therefore moves through an ESPHome action or the board's own HTTP
  (`POST /settings`), never through an entity's state.
- **Alerts and roll-ups are plain HA entities** (2.13 / 2.14), readable by the boards, HA's
  dashboards, phone pushes and a voice pipeline alike.
- **Every board setting lives in the one settings file** (`/cfg/settings.json`), so whatever
  surfaces settings in HA later has one place to read and write.
- **MQTT discovery is not extended** with screen images or configuration entities in the meantime:
  the API carries those in Phase 7, and doing it twice is waste.
- **Two connections, two directions.** The board's websocket to HA (reading entities, A8) stays.
  The ESPHome connection is HA connecting to the board.

## Open, for when the phase is designed

- Which entities a board exposes (screen light, screenshot camera, page select, alert acknowledge
  buttons, settings as numbers and selects), and which stay MQTT's.
- Wake word on the S3s and the P4s (`esp-sr` / microWakeWord), the mic paths (ES7210, or the
  ES8311's own ADC), and whether AEC (`ENABLE_AEC`, parked) is needed for barge-in.
- Encryption key handling and adoption on first boot.
- How the companion web app (#87) relates: it could manage boards over this API, over HTTP, or both.
