# Fleet_Display

The board's display on raw `esp_lcd`, milestone 2.9 (#67). Design and the owner's decisions:
`docs/design/esplcd-step2.md`. Built only with `-D DISPLAY_ESPLCD`; every other board keeps
`DisplayManager` (Arduino_GFX), which this library does not touch. `include/BoardDisplay.h` picks
one per board. Step 6 of 2.9 deletes `DisplayManager` and leaves this.

Every source file here is wholly inside `#if defined(DISPLAY_ESPLCD)` or `#if
SOC_MIPI_DSI_SUPPORTED`: PlatformIO's library finder does not honour `#ifdef`s when it decides
what to compile, so each file has to be empty on boards that do not use it.

| File | What it is |
|---|---|
| `src/Fleet_Display.{h,cpp}` | The class `SystemCore` owns: bring-up, backlight, the frame buffers, and which one the panel is showing |
| `src/fleet_dsi_panel.{h,c}` | MIPI-DSI bring-up in C (the vendor config macros are not valid C++): one sequence for every chip, only the driver's constructor differs |
| `src/esp_lcd_hx8394.{h,c}` | **Vendored** HX8394 driver, `WS_P4_5`'s panel (2.9 step 2) |
| `src/esp_lcd_st7703.{h,c}` | **Vendored** ST7703 driver, `WS_P4_4B`'s panel (2.9 step 3) |
| `src/esp_lcd_ek79007.{h,c}` | **Vendored** EK79007 driver, `WS_P4_7B`'s panel - **compiles, never run** |
| `src/esp_lcd_jd9165.{h,c}` | **Vendored** JD9165 driver, `CYD_P4_1060`'s panel - **compiles, never run** |

`Fleet_Display::begin()` picks the driver from the BSP's `PANEL_MODEL`.

**Driver versions live in ONE place, `src/fleet_display_versions.h`.** Each vendored driver
includes it for the version macros IDF's component build would inject (that include IS each
driver's version-macro local change), and the System Doctor's `[DISPLAY]` section prints the same
numbers through `fleet_dsi_driver_name()`. Updating a driver = new upstream files + its line there. The DSI PHY's LDO channel
and voltage and the lane count come from the BSP's `TEST_MIPI_DSI_PHY_PWR_LDO_*` and
`NUM_DSI_LANES` where a board sets them (7B, CYD_P4_1060), otherwise LDO 3 / 2500 mV / 2 lanes -
which is what every board uses today. `PHY_CLK_SRC` is **not** wired through yet: IDF picks.

**Moving the 7B or CYD_P4_1060 over** is one line - `-D DISPLAY_ESPLCD` in its environment - and
must be done with the board on the desk: nobody has seen either driver drive a panel. Both
compiled and linked on 2026-09-26 with the flag set for the build only.

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
