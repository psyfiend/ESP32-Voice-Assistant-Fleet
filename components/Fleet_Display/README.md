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
| `src/fleet_dsi_panel.{h,c}` | MIPI-DSI bring-up in C (the vendor config macros are not valid C++) |
| `src/esp_lcd_hx8394.{h,c}` | **Vendored** HX8394 driver, `WS_P4_5`'s panel |

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
