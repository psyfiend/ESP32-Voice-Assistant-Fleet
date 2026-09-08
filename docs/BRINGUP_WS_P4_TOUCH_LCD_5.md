# WS_P4_5 Bring-Up — Waveshare ESP32-P4-WIFI6-Touch-LCD-5

**Resolved 2026-09-06. The board is at full fleet parity.** This is the record of what was
wrong and what the board needs — not the investigation, which is in git history. The
transferable lessons are in `LESSONS.md`.

Vendor reference tree: `reference/Waveshare Official Repos/Waveshare-P4-WIFI6-Touch-LCD-5`.
Their bundled, confirmed-working copy of the Arduino GFX library is the single most useful
artefact in it — the root cause below was found by diffing against it.

---

## The bug: the panel's reset is ACTIVE HIGH

`Arduino_DSI_Display::begin()` hardcoded a generic active-LOW reset sequence that *ends with
the pin HIGH*. This panel's HX8394 declares reset **active HIGH**, so HIGH means asserted —
the panel was held in reset for the entire init.

| | sequence | ends at |
|---|---|---|
| **Correct for this panel** | `LOW` → 10 ms → `HIGH` → 10 ms → `LOW` → 120 ms | **LOW = released** |
| **What the fork did** | `HIGH` → 5 ms → `LOW` → 10 ms → `HIGH` → 120 ms | **HIGH = asserted** |

It explained every symptom at once. Host-side calls all succeeded because the panel was never
involved; init commands landed in the DSI command FIFO and blocked part-way through when it
filled with nothing draining it; the backlight never lit because `gfx->begin()` never
returned; and the factory firmware worked because IDF's `esp_lcd_hx8394` driver honours
`flags.reset_active_high`. The other three P4 boards were unaffected because their panels are
active-LOW.

**The fix.** `DisplayConfig` gained `uint8_t RST_ACTIVE_HIGH`, zero-defaulting to active-low
so the three working P4 boards take a bit-for-bit identical path. Only this board sets it.
`Arduino_DSI_Display` branches on it, mirroring `esp_lcd panel_hx8394_reset`.

Waveshare had already patched their own copy of that exact file, with a comment naming the
consequence outright. Several rounds of hypotheses were spent because only
`Arduino_ESP32DSIPanel.cpp` had ever been compared — see `LESSONS.md`, "Method".

---

## What this board needs

**`-D ARDUINO_USB_CDC_ON_BOOT=0` is mandatory, or there is no serial output at all.** The
second USB-C is the USB **OTG** port, wired to the P4's OTG PHY rather than to any serial
bridge, so it never enumerates in any state. No driver fixes this. UART0 via the onboard
CH343 is the only channel, and it enumerates as `USB-Enhanced-SERIAL CH343`
(VID_1A86 / PID_55D3) — always pass `--upload-port` explicitly.

Confirmed working configuration:

| Setting | Value |
|---|---|
| Panel | MIPI/DSI HX8394, 720x1280 native portrait |
| `ROTATION` | `1` (landscape) — our preference; Waveshare specify `0` |
| `RST_ACTIVE_HIGH` | `1` — the fix above |
| `NUM_FB` | `2`, matching Waveshare's own working library |
| `PHY_CLK_SRC` | `0` (library default `PLL_F20M`) — an experiment, retired and confirmed irrelevant |
| Backlight | GPIO26, 5 kHz PWM |
| Touch | GT911 at `0x5D`, INT and RST both `-1` |
| Audio | ES8311 out at `0x18`, ES7210 in at `0x40`, amp enable GPIO53 |
| `HIGH_DPI_DISPLAY` | enabled; suits this panel |
| Silicon | ESP32-P4 rev v1.3 (eFuse-confirmed) |

---

## Verified on hardware

Display, GT911 touch (including at rotation 1, on the generic `mapCoordinates()` branch),
ES8311 output, ES7210 codec init, WiFi STA on a real DHCP lease, AP, and `STA_PLUS_AP`.

This is also the first and so far only board running the full entity pipeline: MQTT broker
session, HA discovery, four system entities published outward, and four Zigbee2MQTT entities
read back inward.

**Not yet exercised:** actual mic capture, SD card.

---

## Two follow-ups this board surfaced

**ESP32-C6 co-processor firmware reports `0.0.0`.** `Req_GetCoprocessorFwVersion` times out
against host `2.12.11`. WiFi works regardless, but it costs roughly a second of boot time in
failed RPC retries. Likely fleet-wide across the P4 boards — see `HARDWARE_STATUS.md`.

**I2C speed is reported twice, differently.** `i2cInit()` logs `freq=100000` while
`[TouchMgr]` logs `Speed:400000`. Both work. Probably two reporters rather than two buses,
but `CLAUDE.md` records that `I2C_CLOCK_SPEED` was collapsed from two fields into the
touch-side value, so it is worth confirming which one actually reaches the bus.

---

## Silicon revision — why it turned out not to matter

`pioarduino` ships two prebuilt P4 lib variants and selects between them from the board
definition. `board = esp32-p4-evboard` selects **`esp32p4_es`** (`SELECTS_REV_LESS_V3=y`,
`REV_MIN_1`), which is correct for this rev1.3 chip. The framework was never the problem.

`BoardHardware.SI_REV` should stay `"unconfirmed"` on every P4 board regardless: silicon
revision is a property of the individual chip, not the board model, so a per-board-model
header cannot represent it correctly. If it is ever made load-bearing, read it at runtime and
compare — the way `checkAudioBspSanity()` does for the ES7210 enums.

Fuller detail, including the rev3-only `XTAL` PHY clock source that `PHY_CLK_SRC` exists to
reach, is in `FUTURE_IMPROVEMENTS.md`.
