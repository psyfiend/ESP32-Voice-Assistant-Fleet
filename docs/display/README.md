# Display stack — start here

**The one status page for the display.** Where every board stands, what is open, and which file
answers which question. Kept current; if a line here is wrong, fix it here first.

Milestone 2.9 (#67) moved the fleet from Arduino_GFX to raw `esp_lcd`, one board at a time behind
`-D DISPLAY_ESPLCD`. Scope was narrowed on 2026-09-29 to "esp_lcd working on every board we can
test"; tuning and features were split into their own issues.

## Where each board is (2026-09-29)

| Board | Bus / panel | Path | Present mode | Rot | Soak | Glass | Open |
|---|---|---|---|---|---|---|---|
| `WS_P4_5` | DSI, HX8394, 1280x720 | esp_lcd | `TRIPLE_PARTIAL`, PPA 90 | 1 | 30 min | signed off | - |
| `WS_P4_4B` | DSI, ST7703, 720x720 | esp_lcd | `TRIPLE_PARTIAL` | 0 | 150 min | signed off; faint lines on the log page's right edge | #69 |
| `WS_P4_7B` | DSI, EK79007, 1024x600 | esp_lcd | `TRIPLE_PARTIAL`, PPA 180 | 2 | 2 h | signed off | - |
| `CYD_P4_1060` | DSI, JD9165, 1024x600 | esp_lcd | `TRIPLE_PARTIAL` | 0 | 2 h | signed off | - |
| `CYD_P4_4880` | DSI, ST7701, 480x800 | esp_lcd | `TRIPLE_PARTIAL` | 0 | - | signed off (2x5 portrait) | #73 fonts |
| `WS_S3_4B` | RGB, ST7701 (init over expander SPI), 480x480 | esp_lcd | `DOUBLE_DIRECT` | 0 | 6 h | clean after the RTNI change | #69, #70 |
| `CYD_S3_8048` | RGB, ST7262, 800x480 | Arduino_GFX | - | | | | **#67: to do** |
| `WS_S3_5B` | RGB, ST7262, 1024x600 | Arduino_GFX | - | | | | **#67: to do** |
| `CYD_S3_3248` | QSPI, AXS15231B, 320x480 | Arduino_GFX | - | | | | hardware broken (USB); step 5 + deleting Arduino_GFX wait on it |

## Open issues

| Issue | What |
|---|---|
| #67 | 2.9 itself: `CYD_S3_8048` and `WS_S3_5B` on esp_lcd, then close |
| #69 | Panel timing on the 4B pair - a 7-day watch on the glass |
| #70 | S3 LVGL performance: LVGL's pool back in SRAM, S3 framework settings |
| #71 | Rotation as a setting; auto-rotate from the S3_4B's IMU |
| #73 | Type ladder - chrome text sizes (seen first on the 4880) |

## Which file answers what

| File | Read it for |
|---|---|
| [architecture.md](architecture.md) | How the display works **now**: Fleet_Display, present modes, choosing a driver by name, the two flush paths, the rules that keep it correct |
| [panels.md](panels.md) | Per panel chip: init lists, timings, datasheet findings, what was tried on glass |
| [performance.md](performance.md) | How we measure, the current numbers per board, every lever tried and its verdict |
| [history.md](history.md) | Problems already solved and how - read when something looks familiar, not every session |
| [test-log.md](test-log.md) | The dated test record: what was run, on which build, and the owner's verdicts |

**The rule for adding to these:** a fact about how it works now goes in `architecture.md` or
`panels.md`; a number goes in `performance.md`; a finished investigation goes in `history.md` as a
few lines, not a narrative; a test run goes in `test-log.md`. The original design and research
documents are in `docs/archive/display/`, unchanged, for the full reasoning behind a decision.

## Instruments

- `GET /bench` (`src/UI/Bench.cpp`, field meanings at its top) and `python scripts/bench.py <host>` -
  full, card, anim, page, copy and verify runs.
- `GET /bench?what=verify` - LVGL's render against the panel's frame buffer in the same instant, on
  the device. The only instrument that proves the glass is fed correctly.
- `python scripts/soak.py --minutes N <hosts>` - the soak gate: failures, reboots, UI freezes, worst
  frame, lowest internal heap.
- `GET /screenshot` (`?fb=1` = the panel's frame buffer) and `python scripts/screenshot.py <host>`.

None of them can see the glass itself: timing faults (#69) are only visible to the owner.
