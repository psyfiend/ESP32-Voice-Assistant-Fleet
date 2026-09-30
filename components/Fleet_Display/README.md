# Fleet_Display

The board's display on raw `esp_lcd`, milestone 2.9 (#67). **How it works:
`docs/display/architecture.md`; status per board: `docs/display/README.md`.** This file is only
the library's own inventory. Built only with `-D DISPLAY_ESPLCD`; every other board keeps
`DisplayManager` (Arduino_GFX), which this library does not touch. `include/BoardDisplay.h` picks
one per board. Step 6 of 2.9 deletes `DisplayManager` and leaves this.

Every source file here is wholly inside `#if defined(DISPLAY_ESPLCD)` or `#if
SOC_MIPI_DSI_SUPPORTED`: PlatformIO's library finder does not honour `#ifdef`s when it decides
what to compile, so each file has to be empty on boards that do not use it.

| File | What it is |
|---|---|
| `src/Fleet_Display.{h,cpp}` | The class `SystemCore` owns: bring-up, backlight, the frame buffers, and which one the panel is showing |
| `src/fleet_dsi_panel.{h,c}` | MIPI-DSI bring-up in C (the vendor config macros are not valid C++): one sequence for every chip, only the driver's constructor differs |
| `src/fleet_dsi_<chip>.c` | **Ours.** One small wrapper per vendored driver: its constructor with our BSP's init sequence, and its name/version, as one `fleet_dsi_driver_<CHIP>` |
| `src/esp_lcd_hx8394.{h,c}` | **Vendored** HX8394 driver, `WS_P4_5`'s panel (2.9 step 2) |
| `src/esp_lcd_st7703.{h,c}` | **Vendored** ST7703 driver, `WS_P4_4B`'s panel (2.9 step 3) |
| `src/esp_lcd_ek79007.{h,c}` | **Vendored** EK79007 driver, `WS_P4_7B`'s panel (running since 2026-09-28) |
| `src/esp_lcd_jd9165.{h,c}` | **Vendored** JD9165 driver, `CYD_P4_1060`'s panel (running since 2026-09-28) |
| `src/esp_lcd_st7701{,_mipi,_rgb}.c`, `esp_lcd_st7701{,_interface}.h` | **Vendored** ST7701 driver (Espressif 2.0.2): `CYD_P4_4880`'s panel over DSI and `WS_S3_4B`'s over RGB, both running. Local changes listed at the top of `esp_lcd_st7701.c` |
| `src/fleet_rgb_panel.{h,c}`, `src/fleet_rgb_st7701.c` | **Ours.** RGB bring-up, and the ST7701's 3-wire SPI init through the TCA9554 expander |
| `src/esp_io_expander*.{h,c}`, `src/esp_lcd_panel_io_3wire_spi.c`, `esp_lcd_panel_io_additions.h` | **Vendored** (Espressif): the expander driver and the 3-wire SPI panel IO the S3_4B's init runs over |

**The driver comes from the BSP by name, with no list of chips anywhere.** The board's BSP says
`#define BSP_PANEL_DRIVER HX8394`, and `Fleet_Display.cpp` pastes that into
`fleet_dsi_driver_HX8394`, defined in `fleet_dsi_hx8394.c`. **Adding a DSI panel:** vendor its
driver, copy one of the four wrappers and change the chip name, types and vendor-config fields
(they differ: the HX8394 and EK79007 take a lane count, the ST7703 and JD9165 do not), add its
version line to `fleet_display_versions.h`, set `BSP_PANEL_DRIVER`. No edit to `Fleet_Display` or
`fleet_dsi_panel`. A BSP naming a chip with no wrapper fails to LINK:
`undefined reference to 'fleet_dsi_driver_<CHIP>'`. Wrappers nothing names are dropped by the
linker, so each board carries only its own driver.

**How many frame buffers, and how frames reach the panel, is the present mode** - derived by
`bspPresentMode()` (`bsp_loader.h`), overridable per board with `DisplayConfig.PRESENT_MODE`.
One is built per bus: `TRIPLE_PARTIAL` on DSI (`src/LVGL_Flush_EspLcd.cpp`), `DOUBLE_DIRECT` on
RGB (`src/LVGL_Flush_EspLcdDirect.cpp`); `begin()` refuses the others and runs the bus's own,
saying so in the boot log and the System Doctor.

**Driver versions live in ONE place, `src/fleet_display_versions.h`.** Each vendored driver
includes it for the version macros IDF's component build would inject (that include IS each
driver's version-macro local change), and the System Doctor's `[DISPLAY]` section prints the same
numbers through each wrapper's `name`. Updating a driver = new upstream files + its line there. The DSI PHY's LDO channel
and voltage and the lane count come from the BSP's `TEST_MIPI_DSI_PHY_PWR_LDO_*` and
`NUM_DSI_LANES` where a board sets them (7B, CYD_P4_1060), otherwise LDO 3 / 2500 mV / 2 lanes -
which is what every board uses today. `PHY_CLK_SRC` is **not** wired through yet: IDF picks.

**Moving a board over** is one line - `-D DISPLAY_ESPLCD` in its environment - done with the board
on the desk. `CYD_S3_8048` and `WS_S3_5B` (ST7262, plain RGB, no init list) are the last two.

## Vendored: `esp_lcd_st7701`

From `espressif/esp_lcd_st7701` **2.0.2** (the registry copy in `reference/esp-registry/`),
Apache-2.0 (`LICENSE_esp_lcd_st7701.txt`), added 2026-09-28 for the Guition JC4880P443. Upstream's
`priv_include/esp_lcd_st7701_interface.h` sits beside the rest (flattened). **Two local changes**,
listed in `FLEET LOCAL CHANGES` at the top of `esp_lcd_st7701.c`: the version macros, and the
driver compiling on MIPI-DSI targets only (the RGB half too), so the S3 environments compile none
of it - 2.9 step 4 lifts that for `WS_S3_4B`. Guition ships a modified 1.1.3 of this driver; we
take Espressif's and feed it Guition's init sequence from the BSP instead. The driver sends MADCTL
and COLMOD itself before the BSP's list, so a BSP list must not contain 0x36 or 0x3A.

## Vendored: `esp_lcd_ek79007`, `esp_lcd_jd9165`

From `espressif/esp_lcd_ek79007` **2.0.2** and `espressif/esp_lcd_jd9165` **2.0.2**, Apache-2.0
(`LICENSE_esp_lcd_ek79007.txt`, `LICENSE_esp_lcd_jd9165.txt`). **One local change each**, marked
`FLEET LOCAL CHANGES`: the version macros. Neither does board-level I2C.

## Vendored: `esp_lcd_st7703`

From `waveshare/esp_lcd_st7703` **2.0.0** on the Espressif component registry. The package declares
MIT (`LICENSE_esp_lcd_st7703.txt`); the source file's own header says Apache-2.0 (Espressif). Both
are permissive and both are kept exactly as found. **One local change**, marked `FLEET LOCAL
CHANGES` near the top of `esp_lcd_st7703.c`: the version macros, as for the HX8394. Unlike that
driver it does no board-level I2C, so nothing needed switching off. Note its `mirror()` supports Y
only ("Mirror X is not supported"), which is why a 180-degree turn stays with the PPA.

## Vendored: `esp_lcd_hx8394`

From `waveshare/esp_lcd_hx8394` **2.1.0** on the Espressif component registry, MIT
(`LICENSE_esp_lcd_hx8394.txt`). Copied verbatim, then **two local changes**, both marked
`FLEET LOCAL CHANGES` at the top of `esp_lcd_hx8394.c`:

1. **The board-specific I2C sequence defaults to skipped.** Upstream, the constructor opens I2C bus
   1 on pins 7/8 with ESP-IDF's *legacy* I2C driver and writes a device at `0x45`. Pins 7/8 are
   `WS_P4_5`'s shared bus, and the legacy driver cannot be linked beside the `i2c_master` driver
   Arduino's `Wire` uses. Waveshare's own P4_5 BSP skips it too. The `i2c_bus.h` include, which
   upstream makes unconditionally, only happens if the sequence is switched back on.
2. **Version macros defined in the file**, because PlatformIO does not inject them the way the
   ESP-IDF component build does.

To update: copy the new upstream files over, then re-apply both changes and diff against upstream.
