# TEST 2.12 - the UI foundation: everything should look and behave exactly as at `v0.2.12`

Branch `refactor/ui-foundation` (off `main` at `v0.2.12`). The brief, the working rules and the
record of each phase: `docs/design/ui-foundation.md` (§6). **The P4_5, the 4B and the 1060 run the
branch** (flashed 2026-10-13, with the local debug lines; the 1060 after you brought it back).

**What changed is the code, not the device.** The card window's 6,325-line file is a folder of files
by concern (P1); its shapes are millimetre tokens (P2); its buttons, dialogs, chips, rows, panels and
selector are a component kit (P3); its frame is a Sheet any window can use, proved by a second one,
"About this board" (P4); the dashboard's placement is a layout engine with tests that run on the PC
(P5); a structure check reports drift (P6). Proposals U1-U5 and W13 in `DECISIONS.md` are yours to
accept or turn down.

**Checked from the PC before you.** At every phase the harness (`scripts/ui_golden.py`) walked each
board through 46 states - both pages, each kind of card's window, every view, every panel, the
Remove dialog (43 on the 4B, whose page has no room for the Panel cards) - saving each one's object
tree (every object: class, place, size, flags, style as LVGL resolves it, text) and a screenshot,
and compared them with the phase before on the same board, minutes apart: **no differences, on all
three boards, at every phase.** A state the house changed under (a light switched, a temperature
moved) was captured again both ways. The
window builds in the same time or less (P4: ~3.5 ms faster, one computation no longer done twice)
and LVGL's pool per window is the same or smaller. WS_S3_4B compiled at every phase, and all nine
environments at the end; no S3 board was flashed.

**What the harness cannot check, so it is yours:** touch - drags, long presses, how a slide feels -
a scheme change, the auto-close, the sparks, a reboot.

| # | Do this | PASS if |
|---|---|---|
| T1 | Both pages, in each scheme (Midnight, Fleet, Linen) | Every card where it was, looking as it did |
| T2 | Long press each kind of card: Desk (HA light group), Deck (temperature), North (garage door), Overhead (switch), All Lamps, Lamp 1 | Its window opens as before: header, controls, the corner tabs; the first frame as quick as before |
| T3 | In a light's window: the control deck's selector, in each of its looks (chosen on the system panel, per scheme) | It slides and draws as before, in every look |
| T4 | SETTINGS: drag it up and back, open a dropdown, tick a checkbox, a row button (Entity properties) | Slides, carets, rows and their values as before |
| T5 | History: the chart, its span chips, scrubbing the dotted line; Activity; their left panel | As before |
| T6 | Members, a member's own view and back, Properties, Scenes | As before |
| T7 | With a panel open, tap inside the window | The panel folds; nothing under your finger is pressed |
| T8 | Tap outside a window; then leave one untouched for a minute | It closes; the untouched one closes itself |
| T9 | Remove card on an added card | The dialog as before: Cancel and a tap outside leave it, Remove removes it |
| T10 | Long press a window's title (debug builds) | The sparks cycle: now and then, once per window, off - as before |
| T11 | Reboot (unplug) | Boots to the dashboard as before; your saved choices kept |
| T12 | From the PC: `http://<board>/popup?about=1`, then `?about=0` (debug builds) | "About this board": the board, its screen and chip, the firmware, how long it has run, free memory, updated each second; it closes like any window |

## Questions for you (nothing below was changed - the zero-change rule)

- **Q1. Merge near-equal sizes?** `tokens.md` §7 lists seven candidates (e.g. the gutter's 1.5 mm
  into the 1.6 mm spacing step), each with its pixels on the three boards. Recommended there: merge
  five (the span chips' one after a look on the 4B), keep Members' spacing (set on glass in 2.10c),
  and choose the dropdowns' and buttons' radii side by side on glass. Each merge is a small look
  change, so none was made.
- **Q2. Where should "About this board" live?** Debug builds only until you say. Recommended: a row
  in the system panel ("About this board"); the alternative is a long press on the header's board
  name.
- **Q3. A compiler on the PC for the host tests?** `pio test -d host-tests` needs gcc on Windows
  (MSYS2 or WinLibs - an install, so yours to choose); meanwhile `wsl bash host-tests/run_in_wsl.sh`
  runs them in WSL's Ubuntu. Recommended: keep WSL until it gets in the way.
- **Q4. The structure check with the gate (W13, proposed)?** `scripts/structure_check.py` reports
  files over 1,000 lines, style calls outside the kit, colour literals, shape numbers, LVGL in the
  core layers; it never fails a build. Recommended: run it with every all-nine gate and quote it in
  the merge report.
- **Q5. Button roles PRIMARY and QUIET** (in the brief) have no user yet - the kit has CANCEL and
  DESTRUCTIVE. Recommended: add each with its first user.
- **Q6. Found on the way, all present at `v0.2.12`:** on the 4B, All Lamps' Members view is titled
  "All Lamp..." when opened straight from a fresh window ("All Lamps >" through Properties); the
  window's colour-picker mark is white and black literals, the same on every scheme; SETTINGS'
  `rowLive()` has been unused since K70; a card span of 0 counts as a whole cell when the page counts
  rows, one unit when it places. Recommended: the title as a small fix after the merge; the rest as
  they are.
- **Q7. Fix the release gate for Windows PowerShell 5.1?** `scripts/release/gate.ps1` stopped at
  the first S3 environment: with `$ErrorActionPreference = "Stop"`, the S3 linker's warning on
  stderr counts as a fatal error in PowerShell 5.1 (the only one on this PC). Its `finally`
  restored your `platformio.ini` as designed. The five P4s had built; the four S3s were compiled
  directly (your local lines touch only P4 environments, so the config is the same). Recommended:
  `$ErrorActionPreference = "Continue"` around its `& $pio run` line, the result still read from
  `$LASTEXITCODE`. It is your reviewed script, so it is unchanged until you say.

## Your results

**Run 1 (owner, 2026-10-14): T1-T8 and T10-T12 PASS. T9 FAIL** - on the 4B the dialog's two
buttons overlap. **Present at `v0.2.12`**: the baseline captured before the refactor
(`test/golden/WS_P4_4B/p0c/desk_dialog.png`) shows the same overlap, and every phase kept it exactly,
as the zero-change rule says. Cause: each button is a fixed 24 mm (`W_DIALOG_BTN`) - 241 px at the
4B's 255 PPI - and two of them (482 px) do not fit the 4B dialog's 362 px button row, so the row
lays them over each other. To fix after the merge: the buttons share the row's width, 24 mm at most.

Also from run 1, after the merge: Entity properties and About this board with both columns left
aligned, the gap between them halved.

Answers:
- Q1: what it asks, explained again in plain terms - open.
- Q2: the system panel, as a button on the row with Log | Tokens | Cards (after the merge).
- Q3: WSL, as recommended; Claude makes the case for any install first.
- Q4: yes - W13 accepted.
- Q5: yes - each role arrives with its first user.
- Q6: **withdrawn** - Claude's misreading. On the 4B the window hides the "All Lamps >" part of the
  title in Members for lack of room, so it shows "Members", which is right; "All Lamp..." is the
  text inside that hidden label, never seen.
- Q7: yes - `gate.ps1` fixed: stderr is logged and the exit code decides.
