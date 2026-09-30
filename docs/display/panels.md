# Panels — per chip

What we run on each panel controller, where it came from, and what has been tried on glass. The BSP
header is the source of truth for values; this file explains them. **Commented-out values beside a
BSP timing field are deliberate** (CLAUDE.md): published "working" timings disagree, vendor
documents included. Compare on glass before "correcting" one.

Datasheets we hold (gitignored): `reference/datasheets/ST7701S_SPEC_V1.4.pdf`,
`ST7703_DS_v01_20160128.pdf`. Sources: [ST7701S v1.4 (Espressif)](https://dl.espressif.com/AE/esp-iot-solution/ST7701S_SPEC_%20V1.4.pdf),
[ST7703 v01 (Pine64)](https://files.pine64.org/doc/datasheet/pinephone/ST7703_DS_v01_20160128.pdf).

## Timing at a glance (BSP values, 2026-09-29)

| Board | Chip | Pixel clock | H sync / back / front | V sync / back / front | DSI lanes | Refresh (measured) |
|---|---|---|---|---|---|---|
| `WS_P4_5` | HX8394 | 58 MHz | 20 / 20 / 40 | 4 / 10 / 24 | 2 x 700 Mbps | ~55 Hz |
| `WS_P4_4B` | ST7703 | 46 MHz | 20 / 80 / 80 | 4 / 12 / 30 | 2 x 1000 Mbps | 66.7 Hz |
| `WS_P4_7B` | EK79007 | 52 MHz | 10 / 160 / 160 | 1 / 23 / 12 | 2 x 1000 Mbps | 60.5 Hz |
| `CYD_P4_1060` | JD9165 | 48 MHz | 24 / 136 / 160 | 2 / 21 / 12 | 2 x 750 Mbps | 56.3 Hz |
| `CYD_P4_4880` | ST7701 (DSI) | 28 MHz | 12 / 42 / 42 | 2 / 8 / 166 | 2 x 750 Mbps | - |
| `WS_S3_4B` | ST7701 (RGB) | 16 MHz | 8 / 50 / 10 | 8 / 20 / 10 | - | ~56 Hz |
| `WS_S3_5B` | ST7262 (RGB) | 21 MHz | 30 / 145 / 170 | 2 / 23 / 12 | - | - |
| `CYD_S3_8048` | ST7262 (RGB) | 16 MHz | 4 / 8 / 8 | 4 / 8 / 8 | - | - |

Refresh = pixel clock / (H total x V total). The pixel clock of an RGB panel is
`DisplayConfig.PREFER_SPEED`; `PCLK_HZ` is unused on both paths.

## How the numbers relate — and why several "work"

Every line is sync + back porch + visible pixels + front porch; every frame the same, in lines. The
blanking is when the panel's own circuitry turns around (gate drivers, row pre-charge). Constraints:

1. Blanking at least what the panel IC needs - and for some chips, the host must send **exactly** the
   porches the IC is configured for (the ST7701 in DE mode).
2. Pixel clock under the panel's maximum (ST7701: 30 MHz) and within the host's bandwidth (PSRAM on
   the S3, the lane rate on DSI).
3. Inside those, many combinations are equally valid. Failures start at the edges, and the edges
   move with temperature and supply voltage: a marginal setting can be perfect cold and glitch
   after hours warm. That is the shape of #69.

## HX8394 — `WS_P4_5` (DSI)

- **Reset is active-HIGH** (`RST_ACTIVE_HIGH = 1`). With the generic active-low sequence the panel
  is held in reset and DSI init commands fill the host FIFO. Found in Waveshare's own copy of
  `Arduino_DSI_Display.cpp`. `docs/BRINGUP_WS_P4_TOUCH_LCD_5.md`.
- The vendored driver's board-specific I2C sequence (legacy I2C driver, device 0x45) is switched off.
- No mirror: its 90-degree rotation is the PPA's.

## ST7703 — `WS_P4_4B` (DSI)

- **Ours runs faster than Waveshare's.** Theirs: 38 MHz, H 20/50/50, V 4/20/20, lanes 480 Mbps =
  59.2 Hz. Measured (2026-09-26 sweep): **timing changes nothing about speed**; the choice is only
  about the picture.
- **`BAh SETMIPI`** sets the DSI receiver: byte 3 = IHSRX, HS receiver drive x1..x16; `HFP_OSC` /
  `HBP_OSC` = the minimum blanking the IC expects in DSI mode; RTERM. Ours differed from Waveshare's
  driver default in IHSRX (`0x05` vs `0x0F`) and `HBP_OSC` (`0x0E` vs `0x06`).
- **Test T1 (2026-09-29):** IHSRX -> `0x0F`. 150-min soak PASS, but faint lines remain at the
  **right** edge on the log page. Next, one at a time: lanes 480 Mbps, a larger HFP, `HBP_OSC 0x06`.
  #69.
- Driver `mirror()` is Y only; the board runs rotation 0 (owner, 2026-09-26).

## EK79007 — `WS_P4_7B` (DSI)

- Matches its vendor timing exactly.
- **Panel-side 180 degrees does not work.** MADCTL after init with (x=0, y=1) and (x=1, y=0), and
  0x36 = 0x02 inside the init list: the picture never moved (three builds, owner on glass,
  2026-09-28). Rotation 2 stays with the PPA.

## JD9165 — `CYD_P4_1060` (DSI)

- Came up first time with the BSP's reset pin; whether that pin is right or the panel ignores reset
  is unknown. Rotation 0.

## ST7701 — `CYD_P4_4880` (DSI) and `WS_S3_4B` (RGB)

One vendored driver (Espressif 2.0.2), two buses. The driver sends MADCTL and COLMOD itself; a BSP
list must not contain 0x36 or 0x3A.

**4880 (DSI):** Guition's init sequence and timing from their IDF BSP (their Arduino port says
500 Mbps lanes; 750 has room: 28 MHz x 16 bpp over 2 lanes is ~224 Mbps a lane). DSI PHY on LDO 3 at
2500 mV. Rotation 0 = native portrait.

**S3_4B (RGB):**
- Init over 3-wire SPI through the TCA9554 expander ([architecture.md](architecture.md) §5), with
  a reset sequence on expander pins. 18 bpp panel, 16-bit bus.
- **Two init lists in the BSP:** (a) ours, converted command for command from the Arduino_GFX
  stream - running; (b) Waveshare's (sleep-out first, `C2 21 08`, `B1 30`, `B2 87`, no VAP/VAN) -
  not yet compared on glass.
- **Datasheet rules that matter here** (ST7701S v1.4):
  - **DE mode (ours): the host's porches must match `C1h PORCTRL`** (p.75). `C1 0D 02` = VBP 13 /
    VFP 2; the host sends 8+20 / 10.
  - **`C2h` INVSET, second byte = RTNI: minimum clocks per line = 512 + 16 x RTNI.** Our line is
    8+50+480+10 = 548.
  - DOTCLK cycle >= 33 ns (<= 30 MHz); `C3h` selects DE/HV mode and polarities.
- **Tests on glass (2026-09-29, #69):**

  | Test | Change | Result |
  |---|---|---|
  | T1 | `C1 0D 02` -> `1C 0A` (porches matched to the host, as the datasheet says) | **worse**: more lines, reaching further right. Reverted |
  | **T2** | `C2 31 05` -> `C2 31 02` (RTNI minimum 592 -> 544, under our 548) | **clean** - running, 7-day watch |

  T1 going the wrong way is itself information: the porch registers visibly move the symptom, so the
  lever is right even though "match the host" was not the answer. `C1`'s VBP may not count the sync
  pulse (then 20, not 28), untested.

## ST7262 — `WS_S3_5B`, `CYD_S3_8048` (RGB)

Plain RGB, no init commands. Waveshare's S3_5B timing and 10-line bounce buffer match ours exactly.
**Not on esp_lcd yet** - #67's last two boards.

## AXS15231B — `CYD_S3_3248` (QSPI)

Not moved; board out of commission. Guition's driver never sends a row address over QSPI (writes
start at row 0 or continue from the last), so arbitrary partial windows are unproven. TE pin on
GPIO 38. `docs/archive/display/waveshare-esp-lcd-survey.md` §5.
