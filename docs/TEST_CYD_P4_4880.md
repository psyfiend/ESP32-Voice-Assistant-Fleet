# Test sheet — `CYD_P4_4880` (Guition JC4880P443), first flash

> **Built 2026-09-28 on `feat/board-cyd-p4-4880`, never flashed.** The BSP comes from Guition's
> pack alone (`docs/HARDWARE_STATUS.md` has the sources and where Guition's two examples
> disagree). It compiles and links with only its own panel driver (ST7701) in the image. Nobody
> has seen this build on this board.

**Flashing:** the full-speed USB-C (serial is the P4's own USB-Serial-JTAG, like the CYD_P4_1060;
there is no UART bridge). `pio run -e CYD_P4_4880P443 -t upload --upload-port COMn`. The board
registers as `fleet-cyd-p4-4880`. Its boot log is on that same port; open the monitor with
`--rts 0 --dtr 0`.

**N1 — First light.** Power the board.
- PASS: the dashboard, portrait (480 wide, 800 tall), upright - rotation 0 is the panel's native
  orientation, and which physical edge that puts the USB on is not known yet: tell me, and say if
  you would rather it were turned (rotation 2 is a PPA turn, measured free on the 4B). Right colours
  (watch the orange cards).
- FAIL, and what to try, in this order:
  - **Black**, backlight on: the panel reset. The BSP uses GPIO5 (the schematic and Guition's
    Arduino port); Guition's IDF BSP uses none. Try `.RST = -1`.
  - **Black, backlight off:** the backlight (GPIO23, PWM 5 kHz).
  - **Rolling, torn, flickering or shifted picture:** the timing. The BSP runs Guition's IDF clock
    (28 MHz, 49.8 Hz, lanes 750 Mbps); their Arduino port runs 34 MHz (60.5 Hz) with lanes at 500.
    Both candidates are in the BSP comments.
  - **Colours swapped (red/blue):** the RGB element order in `fleet_dsi_panel.c` - tell me.
- Tell me the boot log line `[Fleet_Display] Ready ...` and whether `LCD ID: ...` appears (the
  driver reads the panel's ID first; if that read fails, bring-up stops there).

**N2 — Touch.** Tap a card in each corner; swipe pages both ways.
- PASS: taps land under your finger. The GT911's INT and RST are wired to GPIO21/22 here (Guition's
  BSP leaves them unconnected), so the address can be either 0x5D or 0x14 - the BSP tries both.

**N3 — The rest.** WiFi joins and HA shows `fleet-cyd-p4-4880`; brightness slider; a sound (the
amp enable is GPIO11); the System Doctor's `[DISPLAY]` section (driver `esp_lcd_st7701 2.0.2`,
present mode `TRIPLE_PARTIAL`, the measured refresh rate).

**N4 — Layout.** It is the fleet's new small portrait board: 217 PPI, UI scale 1.28, fonts from
`gen_type_scale.py` (the same faces as the P4 4B). Look at the dashboard and say whether the grid
and text sizes feel right; `TARGET_CARD_W` is the knob, per board in `UITokens.cpp`.

**Then:** `/bench?what=verify&n=40&act=anim` and `act=page` (must be 0 bad), and
`python scripts/bench.py fleet-cyd-p4-4880` for its first numbers.
