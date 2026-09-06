# WS_P4_5 Bring-Up — Status & History

Working notes from bringing up the Waveshare ESP32-P4-WIFI6-Touch-LCD-5. High volatility —
update or prune as things resolve. See `PROJECT_STATUS.md` for the rest of the fleet,
`CLAUDE.md` for the BSP architecture, `FUTURE_IMPROVEMENTS.md` for deferred items.

Branch: `feat/p4-5-display-bringup`, cut from `feat/connectivity-state-machine`.

Vendor reference tree (local only, not part of this repo):
`C:/Users/eric/Documents/ESP32-P4-WIFI6-Touch-LCD-5` — Waveshare's own repo: their ESP-IDF
BSP, Arduino examples, **confirmed-working copies of the Arduino libraries**, a
schematic-backed `docs/IO.md`, revision-profile docs in `docs/CI.md`, and
`firmware/...FactoryOnly...bin`. That vendor library copy is the single most useful artefact
here — the root cause below was found by diffing against it.

---

## PRIME SUSPECT (2026-09-06): the panel's reset is ACTIVE HIGH and we drive it active LOW

**IMPLEMENTED AND BUILT on this branch; not yet flashed.** Flash and read the log — the new
`panel reset done (active HIGH)` line confirms the new path ran.

Waveshare's own copy of `Arduino_DSI_Display.cpp` carries a patch our fork does not, with an
explicit comment:

> The Waveshare ESP32-P4-WIFI6-Touch-LCD-5 HX8394 panel declares an ACTIVE-HIGH reset (BSP
> `flags.reset_active_high = 1`): assert = HIGH, release = LOW. **The generic Arduino sequence
> (active low) would hold the panel in reset forever.** Mirror `esp_lcd panel_hx8394_reset`:
> release first, assert 10 ms, release 10 ms, settle 120 ms.

| | sequence | ends at |
|---|---|---|
| **Vendor (correct for this panel)** | `LOW` -> 10 ms -> `HIGH` -> 10 ms -> `LOW` -> 120 ms | **LOW = released** |
| **Our fork (generic active-low)** | `HIGH` -> 5 ms -> `LOW` -> 10 ms -> `HIGH` -> 120 ms | **HIGH = asserted** |

Our sequence leaves RST asserted, so the HX8394 is held in reset for the whole init.

This explains every observation simultaneously:

| observation | explanation under this hypothesis |
|---|---|
| DSI bus, DBI IO and DPI panel all create successfully | all host-side; the panel is not involved |
| commands 0-18 return instantly | they are only landing in the DSI command FIFO |
| command 19 blocks forever | ~238 bytes in, FIFO full, nothing draining because the panel is in reset |
| backlight never lights | `DisplayManager` lights it after `gfx->begin()`, which never returns |
| the shipped factory demo drives the panel fine | IDF's `esp_lcd_hx8394` driver honours `flags.reset_active_high` |
| `WS_P4_7B`, `WS_P4_4B`, `CYD_P4_1060` all work | their panels are active-LOW; the generic sequence is correct for them |

### The fix, as implemented

`DisplayConfig` gains `uint8_t RST_ACTIVE_HIGH`, declared next to the reset pin. `0` (the
zero-filled default) means active-low, so the three working P4 boards take a bit-for-bit
identical path; only `BSP_WS_P4_TOUCH_LCD_5.h` sets `.RST_ACTIVE_HIGH = 1`. It is plumbed
through `Arduino_DSI_Display`'s constructor, which now branches:

```cpp
if (_rst_active_high) { LOW; 10ms; HIGH; 10ms; LOW; 120ms; }   // assert = HIGH
else                  { HIGH; 5ms; LOW; 10ms; HIGH; 120ms; }   // generic, unchanged
```

An `ESP_LOGI("panel reset done (active %s)")` reports which branch ran, so a stale build is
visible immediately.

The vendor's sequence was deliberately **not** copied in unconditionally — that would invert
reset on the three boards that currently work.

All four P4 environments build clean with this change.

---

## Current state

- **Display: NOT working.** Firmware boots and runs; hangs inside `_gfx->begin()` while
  transmitting DSI init command index 19.
- **Touch, audio, WiFi, SD: untested**, blocked behind the display.
- **The board is healthy.** Not bricked, not a bad flash, not a bad BSP, and the shipped
  factory firmware drives the panel — so the hardware and the panel are known good.
- **Chip is ESP32-P4 rev v1.3**, and (see corrections) that turned out to be a non-issue.
- Committed on this branch: a per-board `PHY_CLK_SRC` and `NUM_FB` mechanism plus `STEP`
  instrumentation. Both experiments came back negative; the mechanism is kept because it is
  the right shape for the reset-polarity fix and costs the other boards nothing.

## Where it hangs, precisely

Line numbers shift as instrumentation changes, so this is expressed by call. From a
`CORE_DEBUG_LEVEL=4` boot:

```
STEP -> esp_lcd_new_panel_io_dbi
STEP dbi-io done
STEP -> esp_lcd_new_panel_dpi (fbs=2[BSP=2], 720x1280 @ 58MHz)
STEP dpi-panel done
STEP -> init cmd loop (25 cmds)
  cmd[0] = 0x11  ...  cmd[18] = 0xB1
  cmd[19] = 0xBD          <- printed, then nothing, indefinitely
```

| call | status |
|---|---|
| `esp_ldo_acquire_channel` | OK |
| `esp_lcd_new_dsi_bus` | OK (with both clock sources tested) |
| `esp_lcd_new_panel_io_dbi` | OK |
| `esp_lcd_new_panel_dpi` | OK (with `num_fbs` 1 and 2) |
| init loop `esp_lcd_panel_io_tx_param` | **blocks at index 19** |
| `esp_lcd_panel_init` | never reached |

The trace prints *before* the transmit, so the block is inside the tx call for index 19.

**Index 19 is not a bad command.** Index 17 is the same `0xBD` with data `0x01` and goes
through. What matters is position: by index 19 roughly **238 bytes** have been pushed into the
DSI command FIFO (indices 8-12 alone are 21/33/44/44/58 bytes). A link that is not draining
fills the FIFO and blocks on the *Nth* write, not the first. The working model is **"the DSI
command channel is not draining"**, which is exactly what a panel held in reset would cause.

It is a hang, not an error: every call is wrapped in `ESP_ERROR_CHECK`, which panics and
reboots on failure. No panic, no reset loop, no coredump.

## Ruled out, with evidence

| Ruled out | Evidence |
|---|---|
| Bricked by `pio run -t erase` | ROM bootloader is mask ROM and cannot be erased; `esptool` connects fine. Note `-t erase` wipes the **whole chip**, not just NVS — the demo disappearing was expected |
| Bad flash / bad partition table | Read back from the chip: bootloader magic `e9` at `0x2000` with entry `0x4ff29ed0` matching the boot log; app magic `e9` at `0x10000`; all 6 partitions valid |
| Partition scheme (16 MB CSV on a 32 MB chip) | No mechanism — partitions govern flash layout only; DSI init reads no flash. Map is coherent and ends at `0xFF0000`, inside 16 MB. The bootloader image header already carries the 32 MB size nibble and the banner reports 32 MB. A real inconsistency with vendor docs, worth cleaning up to recover the lost 16 MB, but inert here |
| Framework built for the wrong silicon revision | The build uses the **`esp32p4_es`** lib variant (630 include-path entries), whose sdkconfig has `SELECTS_REV_LESS_V3=y`, `REV_MIN_1`, PSRAM 200 MHz — correct for rev1.x. See corrections |
| Wrong BSP values | Every value matches Waveshare's BSP *and* their Arduino `displays_config.h`: 720x1280, hsync 20/20/40, vsync 4/10/24, 58 MHz, 700 Mbps, 2 lanes, LDO ch3 @ 2500 mV, RST 27, BL 26, RGB565/16bpp |
| Malformed init command table | **All 25 entries diffed against Waveshare's `vendor_specific_init_default`: byte-identical** — same order, same data, same delays, including the counter-intuitive `0xB9` SETEXTC at index 4 arriving *after* the `0xBA` at index 3 |
| DSI PHY reference clock source | Tested both `PLL_F20M` and IDF-auto (selector 1 -> enum 0). Byte-identical behaviour, same hang, same index. `esp_lcd_new_dsi_bus` accepted `0` without `abort()`, so the driver resolves the sentinel |
| `num_fbs` | Tested `1` and `2` (vendor uses 2). `fbs=2[BSP=2]` confirmed applied. Identical hang |
| Missing BSP fields (`NUM_DSI_LANES`, `PCLK_HZ`, the two LDO fields) | Zero consumers repo-wide; the library hardcodes 2 lanes and LDO ch3/2500 mV. Dead config on every board |
| Brownout from single-USB power | A brownout trips the detector, prints `Brownout detector was triggered`, and resets. Neither seen. The dark panel is a consequence of the hang (backlight is initialised after `gfx->begin()`), not evidence of a power fault |
| DPI clock source is revision-specific | `soc_periph_mipi_dsi_dpi_clk_src_t` has no revision split; `DEFAULT = PLL_F240M` on all revisions |

## You cannot see serial on this board without `ARDUINO_USB_CDC_ON_BOOT=0`

This cost hours and will cost them again. The P4_5's second USB-C connector **never enumerates
in any state** — not with the factory demo, not with our firmware, not with blank flash — so it
is not wired to USB-Serial-JTAG. With `ARDUINO_USB_CDC_ON_BOOT=1`, `Serial` binds to USB CDC and
its output goes nowhere. UART0 via the onboard CH343 bridge is the only channel.

`WS_P4_TOUCH_LCD_4B` is the one P4 env that never set `CDC_ON_BOOT=1`, which is plausibly why it
has always been the easiest board in the fleet to get data from.

Two more practical traps:

- On the laptop, **`COM4` is an Intel AMT Serial-over-LAN virtual port, not a board.** The P4_5
  enumerates as `USB-Enhanced-SERIAL CH343` (VID_1A86 / PID_55D3). Always pass `--upload-port`
  explicitly rather than trusting auto-detect.
- `-D CORE_DEBUG_LEVEL=4` is what made any of this visible. Without it every `ESP_LOGI` in the
  DSI path is compiled out and the failure looks like total silence after `entry`. It only
  enables logging in code compiled *here*; ESP-IDF's own `esp_lcd` internals live in prebuilt
  `.a` archives and stay silent regardless.

## Chip revision (and why it turned out not to matter)

**ESP32-P4 rev v1.3**, confirmed twice: `espefuse summary` gives `WAFER_VERSION_MAJOR = 1`,
`WAFER_VERSION_MINOR = 3`; the Arduino banner reports `Revision : 1.03` with board string
`Espressif ESP32-P4 Function EV Board (ES pre rev.300)`.

`pioarduino` ships **two** prebuilt lib variants and selects between them automatically from the
board definition:

| variant | `SELECTS_REV_LESS_V3` | `REV_MIN` | PSRAM | used here? |
|---|---|---|---|---|
| `esp32p4` | not set | `301` | 200 | no |
| **`esp32p4_es`** | **`y`** | **`1`** | 200 | **yes — 630 include-path entries** |

So the framework in use is already the correct pre-rev3 build for this silicon: rev<3 HAL, rev<3
CLIC, rev<3 clock sources. **Silicon revision is a non-issue for this bug.**

`BoardHardware.SI_REV` should stay `"unconfirmed"` on all P4 boards regardless: silicon revision
is a property of the individual chip, not the board model, so a per-board-model header cannot
represent it correctly. If it is ever made load-bearing, read it at runtime and compare, the way
`checkAudioBspSanity()` does for the ES7210 enums.

Still worth doing when convenient: `esptool flash-id` on the other three P4 boards, purely to
know what silicon the fleet actually contains.

## What is committed on this branch

Two per-board override mechanisms plus instrumentation. Both experiments came back negative, but
the mechanism is retained because it is exactly the shape the reset-polarity fix needs.

| file | change |
|---|---|
| `components/Fleet_BSP/include/Fleet_BSP.h` | `BSP_PHY_CLK_SRC_*` selectors; `uint8_t PHY_CLK_SRC` and `uint8_t NUM_FB` at the end of the MIPI-DSI block (both `0` = change nothing) |
| `components/Fleet_BSP/include/BSP_WS_P4_TOUCH_LCD_5.h` | `.PHY_CLK_SRC = BSP_PHY_CLK_SRC_IDF_AUTO`, `.NUM_FB = 2` |
| `components/DisplayManager/DisplayManager.cpp` | passes both to the panel constructor |
| `components/GFX_Library_for_Arduino` (**submodule**) | `Arduino_ESP32DSIPanel.{h,cpp}`: constructor parameters, selector-to-enum mapping, `STEP` instrumentation around each call, per-command trace in the init loop |

`PHY_CLK_SRC` holds a small Fleet-defined code, **not** a raw
`mipi_dsi_phy_pllref_clock_source_t`: those are positional ordinals inside `soc_module_clk_t`, so
storing their integers would silently repoint every board if Espressif reordered that enum. The
driver maps the codes to real constants by name.

To undo the experiments but keep the mechanism, set `.PHY_CLK_SRC` and `.NUM_FB` back to `0` in
the board header. To revert everything:

```bash
git checkout feat/connectivity-state-machine -- components/Fleet_BSP components/DisplayManager
git -C components/GFX_Library_for_Arduino checkout -- src/databus
```

`platformio.ini` is **never** committed — it carries machine-specific `symlink://` paths. On this
laptop it also carries `-D ARDUINO_USB_CDC_ON_BOOT=0` and `-D CORE_DEBUG_LEVEL=4` for the
`WS_P4_TOUCH_LCD_5` env; **both are required to debug this board and must be re-applied on any
other machine.**

## Next steps, in order

1. **Test the reset polarity fix.** Add `RST_ACTIVE_HIGH` to `DisplayConfig` (default `0` =
   active low), plumb it through `Arduino_DSI_Display`, set it on the P4_5, and use the vendor
   sequence when set: `LOW` -> 10 ms -> `HIGH` -> 10 ms -> `LOW` -> 120 ms. Expect the init loop
   to run past index 19 and `gfx->begin()` to return.
2. **If that works**, reset `.PHY_CLK_SRC` and `.NUM_FB` to `0` and re-test, to confirm neither
   was load-bearing before leaving them in. Then move the `STEP` instrumentation behind
   `#ifdef DEBUG_DISPLAY` per the repo's debug-flag convention rather than deleting it.
3. **If it does not work**, stop theorising and run the two controls:
   - Flash `firmware/...FactoryOnly...bin` from the vendor tree — restores the shipped demo and
     re-proves the hardware.
   - Build Waveshare's `01_HelloWorld` against **their** bundled libraries. Their Arduino path
     uses the same prebuilt `arduino-esp32` libs we do, so the framework is byte-identical and
     any difference is purely library/config code — directly bisectable.
4. **Adopt Waveshare's `DSI_STEP_CHECK`** in place of `ESP_ERROR_CHECK` regardless of outcome: it
   prints the failing step and error code instead of aborting.
5. **Diff the rest of the vendor GFX tree.** The reset finding surfaced only because
   `Arduino_DSI_Display.cpp` was diffed. These also differ and have not been examined:
   `Arduino_GFX.h`, `Arduino_ESP32RGBPanel.cpp`, `Arduino_ESP32SPIDMA.cpp`,
   `Arduino_DSI_Display.h`, `Arduino_RGB_Display.h`. Some may matter for other boards.
6. **Backlight ordering.** Waveshare lights the backlight *before* `gfx->begin()`;
   `DisplayManager` does it after, deliberately, to override `pinMode()` calls. Not a bug, but it
   means a dark panel carries no diagnostic information. Consider an early low-brightness enable
   so panel state is visible during init.

## Corrections — theories that were confidently wrong

Recorded so nobody re-runs them. The pattern in every case: inference from config symbols and
partial comparisons lost to direct measurement and full diffs.

1. **"arduino-esp32 3.3.11 requires P4 rev >= 3.01, so the bootloader refuses to boot."** Wrong
   twice over. The flashed image headers carry `min_chip_rev_full = 0` (no gate), and the build
   does not even use the `esp32p4` libs whose sdkconfig said `REV_MIN_301`.
2. **"The framework is a rev3-profile hybrid, so rev<3 workarounds are compiled out."** Wrong —
   read from `esp32p4/sdkconfig` while the build actually uses `esp32p4_es`, which has
   `SELECTS_REV_LESS_V3=y` and `REV_MIN_1`. The CLIC / PVT / sleep differences catalogued from
   that file are real ESP-IDF behaviour but do not apply to this build.
3. **"The BSP is missing `NUM_DSI_LANES` and the LDO fields, so DSI initialises with 0 lanes."**
   Wrong. Those fields have zero consumers; the library hardcodes 2 lanes and LDO ch3/2500 mV.
4. **"`phy_clk_src = PLL_F20M` is wrong; Waveshare uses `0`."** Wrong. `PLL_F20M` is the
   rev<3-legal source and `DEFAULT_LEGACY` *is* `PLL_F20M`. Tested directly: both produce an
   identical hang.
5. **"The init table matches Waveshare exactly."** Right conclusion, but originally asserted from
   a `tail -8` comparison and only later verified across all 25 entries. The claim held; the
   evidence did not support it at the time.
6. **"`num_fbs` 1 vs 2 is the last remaining delta."** Wrong — it was the last delta *in the files
   already examined*. `Arduino_DSI_Display.cpp` had never been diffed, and that is where the
   reset-polarity patch lives.
