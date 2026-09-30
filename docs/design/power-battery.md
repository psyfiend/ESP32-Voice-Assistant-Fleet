# Power and battery

**Status (2026-09-30): prototype, merged to `main`.** Tracker: #72 (tests P1-P6 and every bench
result, in its comments). P1-P3 pass on the 7B; P5 fails (below, §4); P6 not run. The owner's
direction: do not perfect the voltage inference - use fuel gauges (MAX17043 boards ordered) on the
units that really carry a battery. This file is the current picture: what each board's
power hardware is, how it behaves on glass, how the firmware reads it, and what is open. Header
glyphs: #19. RTC backup cells: #32.

## 1. The hardware, per board

| Board | Battery input | Charger | Battery sense | Charge status to a GPIO | RTC backup cell |
|---|---|---|---|---|---|
| `WS_P4_5` | yes | ETA6098 | GPIO20 (ADC1 ch4), 200K/100K, x3.000 | no | 2-pin header, **rechargeable only** (Waveshare) |
| `WS_P4_7B` | yes, J4 | ETA6098 | GPIO20 (ADC1 ch4), 200K/100K, x3.000 (net unlabelled on the schematic) | no - the side LED is the charger's own | holder sized CR1220: fit an **ML1220** (rechargeable only) |
| `WS_P4_4B` | **none** | - | - | - | 2-pin header, rechargeable only |
| `CYD_P4_1060` | yes | IP5306 (charger + 5 V boost) | GPIO53 (ADC2 ch4), 68K/100K, x1.680 | no | - |
| `CYD_P4_4880` | yes | IP5306 | GPIO53 (ADC2 ch4), 68K/100K, x1.680 | no | - |
| `WS_S3_4B` | yes | **AXP2101 PMU** (I2C 0x34) | fuel gauge in the PMU | **yes** - the PMU reports it | - |
| `WS_S3_5B` | charger on board | ? | not investigated | ? | 927 holder, cell type unconfirmed |
| `CYD_S3_3248` / `CYD_S3_8048` | yes | IP5306 | GPIO5 / GPIO17, divider unknown | no | - |

GPIO17 on the S3 is ADC2, which the S3's Wi-Fi owns: the 8048 may need another approach. Every
divider above comes from the vendor schematic; the 4880's was checked against a multimeter (within
50 mV).

**BSP fields:** `BoardHardware.BAT_ADC` (the pin) and `BAT_DIV_X1000` (the divider's
(top + bottom) / bottom x 1000). **`BAT_DIV_X1000 = 0` means no battery input** - needed because
`BAT_ADC`'s own zero-fill default is a real GPIO.

**`-D HAS_BATTERY`** (per environment, `platformio.ini`) says **this unit carries a cell**. The
BSP says the *board* can take one; the flag says *this device* has one - not every board will
(owner, 2026-09-30). All battery logic runs only when both hold: the provider, the entities and
cards, `[POWER]`'s battery lines, and later the header glyph and charging logic. Set on
`WS_P4_7B` and `CYD_P4_4880` today. The Doctor says so when a board has an input but no flag.

## 2. Behaviour measured on glass

| Event | `CYD_P4_4880` (IP5306) | `WS_P4_7B` (ETA6098) |
|---|---|---|
| Cold boot on a PC's USB alone | browns out, repeatedly - with or without a cell | backlight on, never boots - with or without a cell |
| Cold boot on a 1.5 A+ wall supply | boots | boots (the 7B and 1060 need two PC cables otherwise) |
| USB (wall) pulled while running, cell fitted | **restarts**, then runs on the cell by itself | **restarts** (reset reason *power-on*, not brownout), then runs on the cell |
| Wall USB plugged in while running on the cell | - | **no restart**; reading steps **~+100 mV** at once, then climbs |
| PC USB plugged in while running on the cell | - | **restarts and hangs** (charge LED red) until a wall supply is added |
| No cell fitted, on USB | BAT pin floats **2.9-3.1 V** | BAT pin held at **~4.17 V**, steady |
| Charging | 4162 mV, climbing | 4134 mV |
| On the cell | ~4060 mV (load sag) | 4026 mV, falling |

**So on both P4 families a battery is a restart-once backup, not a UPS**: losing mains restarts
the board, which then runs on the cell. Gaining a *wall* supply is seamless. A PC port cannot carry
the board and the charger together, before or after boot. Not fixable in firmware; a hold-up
capacitor on the 5 V rail might fix the restart, untested.

## 3. How the firmware reads it (`components/Fleet_Providers/BatteryProvider.*`)

- **ESP-IDF oneshot ADC + eFuse curve-fitting calibration**, not `analogRead` (IDF-first rule).
  64 samples averaged every 5 s; 12 dB attenuation.
- **Entities** (`BatteryEntities.h`), advertised to HA as diagnostics: `sys_batt` (%,
  `device_class: battery`) and `sys_batt_mv` (mV, the raw evidence). Registered only when
  `BAT_DIV_X1000 != 0`, so cards bound to them are skipped on other boards.
- **Percentage:** a 1S Li-ion open-circuit curve, piecewise linear (4200 = 100 %, 3700 = 30 %,
  3300 = 0 %). Under load it reads low, under charge high - a gauge, not a fuel meter. Light
  smoothing (1/4 new) so it does not flicker across a boundary.
- **Below 3.2 V nothing is published** and the cards grey out: that catches "no cell" on the IP5306
  boards, whose pin floats at ~3 V, and a board running on its cell shuts down near 3.0 V anyway.
- **Cards:** two on the Fleet page; a battery-% card's corner icon draws its level in six steps.

### 3.1 Power state, inferred

No divider board routes a charge status to a GPIO, so the state is worked out from evidence,
strongest first, and **the System Doctor prints the evidence with the state** (`[POWER]`):

1. **A step** of >= 60 mV between two 5-s readings: a charger arrived (+) or left (-). Measured
   +100 and +156 mV on the 7B; **P3 PASS** - the Doctor said "charging (a +step: a charger
   arrived)" within seconds.
2. **A PC on the chip's own USB-Serial-JTAG port** means external power (P1 PASS on the 7B).
3. **A restart by brownout** means power was lost. *In practice it never fires*: losing USB resets
   the 7B as a power-on (P2). Kept, harmless; to be revisited.
4. **The trend** over up to 5 minutes (needs 3): >= +1.5 mV/min charging, <= -1.5 on battery.
   This is what caught "running on battery" in P2, after 3 minutes.

**Full** = external power, >= 4150 mV, flatter than 1 mV/min. With no step ever seen the Doctor
adds "or no cell: some chargers hold this with none" - the ETA6098 at ~4.17 V is exactly that, and
voltage alone cannot tell the two apart.

`[POWER]` also prints the last reset reason, the sense pin / ADC / divider / calibration, mV, pin mV,
raw and %, the trend, and the last step with its age. The serial log prints the state once a minute
and at once on a step.

## 4. Open

- **No-cell detection - on BOTH chargers while USB is present** (P5 FAIL, 2026-09-30): pull the
  cell from a running 4880 and the IP5306 holds BAT at ~4.16 V for as long as USB is there, like
  the ETA6098; the 2.9-3.1 V "no cell" reading only happens on a cold boot with no cell ever
  fitted. The removal shows as a +step on external power, which the inference currently misreads
  as "a charger arrived" - a candidate rule, not worth perfecting (owner): fuel gauges are the
  answer for units that carry a battery.
- **MAX17043 fuel gauge** (owner has ordered boards): I2C 0x36, state of charge in % directly - a
  third source behind the same entities, for the units fitted with one.
- **Deciding sooner after a mains loss**: a power-on boot with no USB host and a falling voltage
  could be called "on battery" in well under 3 minutes.
- **The AXP2101 on `WS_S3_4B`**: a second source behind the same entities, with a real charging
  flag and USB-present flag, charge current/voltage, the fuel gauge. Overlaps #32 (the power key and
  rails stay there). Structural - agree the shape with the owner first.
- **A power-state entity** for HA and the header (#19): the glyphs need charging / on-battery /
  full / mains-only.
- The CYD S3s' IP5306 dividers; `WS_S3_5B`; the 1060 on glass.
- Tests P3-P6 in #72.
