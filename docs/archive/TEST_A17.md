# TEST - A17: a failed mount never costs the settings

Branch `fix/a17-settings-safety` (off `main` at `v0.2.16`), built unattended on 2026-10-15 from
HANDOFF's spec and DECISIONS A17 (your answer to `archive/TEST_UI_FOLLOWUPS.md` Q1). **No pull
request yet** - for your tests and answers first. Nothing on the screen changes.

## What changed

| Where | Before | Now |
|---|---|---|
| **The mount** (`Settings::begin()`) | `format_if_mount_failed`: any mount failure formatted the partition - most likely how the P4_5 lost its file on 2026-10-14 | never formats on a failure. The whole `spiffs` partition is read: **every byte 0xFF** is a first boot (format, mount); **anything else is left untouched**, and that boot is read-only, from RAM |
| **Saying so** | `?stats=1`: "NOT MOUNTED - RAM only" | the boot note says why: `LittleFS did not mount (ESP_FAIL); the partition holds data (from 0x0) - left untouched, read-only this boot, settings in RAM` - in `?stats=1` and in a new **Boot:** line under the System Doctor's [SETTINGS] |
| **A restore** | none | `POST /settings` with a settings file as the body: checked (JSON, an object, a schema it knows), written by the settings task, answered when written; `?reboot=1` restarts after. A read-only boot refuses (409) and says why |
| **Copies on the PC** | by hand, `curl -o` | `scripts/release/save_settings.ps1 <host> <board>` keeps `bench/settings/<board>/<yyyymmdd-hhmm>.json` (never overwriting a copy); `restore_settings.ps1 <host> <file>` puts one back |
| **`flash_release.ps1`** | flashed the three P4s | **saves each board's settings first** and does not flash a board it could not save (`-NoBackup` to flash anyway); **the 1060 is written app-only at 460800, ending with a watchdog reset** (LESSONS, 2026-10-13) - it was pio's upload at 1.5 Mbaud |
| **The test switch** | - | `-D DEBUG_SETTINGS_NOMOUNT`, a local line: the mount reports a failure. The pre-commit hook refuses it like `DEBUG_POPUP` |
| **Code layout** | `Settings.cpp` | A17 took it to 1,001 lines, so the PC's addresses moved to `SettingsHttp.cpp` (296); the store is 774. Nothing else moved |

## How it was checked (on the desk, 2026-10-15 05:40-06:30)

- **Every board's settings saved before any flash**: `bench/settings/WS_P4_TOUCH_LCD_5/20261015-0542.json`
  (1652 bytes, 9 cards), `WS_P4_TOUCH_LCD_4B/20261015-0542.json` (2325 bytes, 12 cards),
  `CYD_P4_1060P470/20261015-0542.json` (1772 bytes, 13 cards); again before each flash after that.
- **A failed mount, P4_5** (built with `DEBUG_SETTINGS_NOMOUNT`): the boot said `LittleFS did not
  mount (ESP_FAIL); the partition holds data (from 0x0) - left untouched, read-only this boot`;
  `?stats=1` said `NOT MOUNTED - RAM only, READ-ONLY this boot`; a restore was refused with 409 and
  that reason; a setting changed from the PC stayed in RAM ("pending yes", nothing written). **Then a
  normal A17 boot read the same file, byte-identical to the 05:42 copy** (1652 bytes, 9 cards, 2
  schemes).
- **A restore round trip, 4B** (its own file): a card renamed from the PC (`office_desk` -> "A17
  test", written: 2394 bytes); the 05:42 copy restored with a restart (`restored: 2325 bytes, 12
  card(s), written in 58 ms - restarting`); after the restart the file is **byte-identical to the
  05:42 copy** and the rename is gone. (That copy also put back your `window_close=10`, which I had
  turned off for the harness: the 4B is as you left it.)
- **The same on the P4_5 after the split**, without a restart: renamed, restored in 40 ms, the
  document byte-identical; a file saying `"schema": 9` and a body that is not JSON both refused
  with 400 and "nothing changed".
- **The harness**, P4_5, `v0.2.16` against A17 (48 states): **no differences** - and again after the
  split: see the result line below.
- **All nine compile** (`gate.ps1`, on `d3887f3`, 11:31): every board SUCCESS; the re-run on the
  final commit: below. **Structure check**: files over 1,000 lines back to the two that were already
  there (`GUIManager.cpp` 1551, `Card.cpp` 1462); style calls outside the kit 523 in 25 files, colour
  literals 44 in 4 files - unchanged from `main`; no shape numbers; no LVGL in the core layers.
- **Not checked on hardware: the first-boot path** (a blank partition formatted). It needs a board's
  partition erased - your call (T6).

## Tests for you

| # | Do this | PASS if |
|---|---|---|
| T1 | `curl "http://fleet-ws-p4-5/settings?stats=1"` | `mounted`, and the boot line lists your cards as before |
| T2 | System panel > Log (a Dump first) - the [SETTINGS] section | a `Boot:` line saying what was read at boot |
| T3 | `powershell -ExecutionPolicy Bypass -File scripts/release/save_settings.ps1 fleet-ws-p4-5 WS_P4_TOUCH_LCD_5` | `saved WS_P4_TOUCH_LCD_5: ... card(s) -> bench\settings\...` and the file is there |
| T4 | Rename a card on the device (SETTINGS > its name), then `restore_settings.ps1 fleet-ws-p4-5 <T3's file>` | the board restarts (`... - restarting`) and the card has its old name |
| T5 | Optional: add `-D DEBUG_SETTINGS_NOMOUNT` to the P4_5's local lines, flash, look at `?stats=1`; change something on the device; remove the line, flash again | read-only with the reason; after the second flash your file as before, the change gone |
| T6 | Optional, wipes a board's file - your call: T3 for the 4B, then `esptool --chip esp32p4 -p COM7 erase-region 0xc90000 0x360000`, boot, then `restore_settings.ps1` | the boot log says the partition was blank and formatted ("no settings file yet"); after the restore your settings are back |
| T7 | The next release flash (`flash_release.ps1`) | three `saved ...` lines, three `FLASHED`; the 1060 up again without its BOOT button |

## Choices that were mine (the documents left them open)

1. **Blank means the whole partition is 0xFF**, not only its start (HANDOFF said "the partition's
   start"). A16's erase mode erased exactly the start and left the file's blocks behind it, so a
   start-only test would have formatted the one case worth keeping. It costs a read of 3.4 MB only
   when a mount has failed (~100 ms), never on a normal boot.
2. **A restore restarts the board by default** in the script (`-NoReboot` to stay up): many settings
   are read once, at boot or when a page is built, so a restart is the only state that is surely the
   file's.
3. **`flash_release.ps1` refuses to flash a board it could not back up**; `-NoBackup` overrides it
   (a board that does not boot cannot answer).
4. **The 1060 in `flash_release.ps1`**: built by pio, written by esptool, app only, 460800, watchdog
   reset - as HANDOFF says to flash it by hand. It was pio's upload at full speed.
5. **The restore answers only after the file is written** (or 5 s), so the script's "restored" means
   on flash.
6. **Settings.cpp split** at 1,001 lines, into `SettingsHttp.cpp` - the structure check's mark.

## Questions for you

- **Q1. A partition that holds something that is not LittleFS** - a new board, or a vendor's demo
  data in that region - now stays read-only until it is erased with esptool (T6's command). An
  address to format it from the PC, accepted only on a read-only boot? **Recommended: no** - an
  erase should be a deliberate act at the desk.
- **Q2. A read-only boot on the screen**: today it is in `?stats=1` and the System Doctor only. A
  toast at boot ("Settings are read-only this boot")? **Recommended: yes, later** - with #76's
  notification queue rather than a one-off.
- **Q3. Choices 1-4 above**: keep? **Recommended: keep all four.**

## Results

- **The harness after the split** (P4_5, 48 states against `v0.2.16`): no differences in the
  tree or the pixels; 11 of Desk's states were flagged "the house changed" - the Desk light changed
  between the two captures (the capture before the split, of the same store, had none).
- **The gate on the final commit `b233677`**: all nine SUCCESS in 9:38 (P4_5 41.0% flash, the
  S3 boards 34.5-36.6%).
- **The boards now**: the P4_5 runs `b233677`, the 4B `d3887f3` (the same behaviour before the
  split), the 1060 `v0.2.16` - all with your local lines; each back on `main` when I stop, unless
  2.8's sheet says otherwise.

## Your results

**Signed off by the owner, 2026-10-15**, who saved the P4_5's file with T3's script and handed the
rest to Claude: "if you can test A17 then go ahead, I'm placing my trust in you to ensure the safety
of the settings file." Q1-Q3 not answered; the recommendations stand as built (no format address,
the toast with #76 later, choices 1-4 kept).

Claude's run, on the final build (`4d4241f`, with `main` merged in):

| # | Result |
|---|---|
| T1 | **PASS** - P4_5 and 4B `mounted`, the boot line lists their cards (9 and 12) |
| T2 | not seen on glass this round (the Log page); the line is in the report's code path |
| T3 | **PASS** - the owner's copy (15:15) and Claude's before every flash |
| T4 | **PASS** - P4_5: Desk renamed from the PC, the 15:21 copy restored with a restart; the file byte-identical to the copy |
| T5 | **PASS** (earlier in the day, `DEBUG_SETTINGS_NOMOUNT`) - read-only with its reason, the file untouched by the next boot |
| T6 | **PASS** - 4B: its copy checked identical to the morning's original, the partition erased (`erase-region 0xc90000 0x360000`); the boot said `the "spiffs" partition is blank (3456 KB, checked in 387.8 ms) - a first boot: formatting it`, then `no settings file yet`; the copy restored with a restart - byte-identical |
| T7 | at the release flash (`v0.2.17`) |
