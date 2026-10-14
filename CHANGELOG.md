# Changelog

Versions are `v0.<phase>.<release>`; a build adds the commits since that tag, and `+dirty` for a
modified tree (`v0.2.8.14+dirty`). The phase follows the roadmap. The release number counts tagged,
signed-off merges to `main` in the order they happen - it does not follow milestone numbers, which
began finishing out of order (decided 2026-10-05; `docs/DECISIONS.md` W3). This file is where a
version is matched to its milestones.

## v0.2.13 - 2026-10-14

2.12 - the UI foundation: the card window's code rebuilt into a foundation for 2.8 and 2.11, with
nothing changed on screen (DECISIONS K71-K74, D-10, W13; `docs/design/ui-architecture.md`).

- **A safety net**: `GET /popup?tree=` prints every LVGL object with its style as LVGL resolves it;
  `scripts/ui_golden.py` walks 46 states per board and compares object trees and screenshots, telling
  the house's own changes apart.
- **The card window split by concern**: `CardPopup.cpp`'s 6,325 lines into `src/Cards/Popup/`.
- **Shape tokens in millimetres** (`UIShape`): no number left in the window's code.
- **The component kit** (`Kit::`): each look a role - shared styles that follow the colour scheme;
  buttons, dialogs, chip rows, list items, panel rows, folder panels, the selector.
- **The Sheet**: every window's frame is one component; "About this board" is its second client
  (`/popup?about=1`, debug builds).
- **A layout engine**: the dashboard's placement as `Layout::FlowEngine`, pure C++, with 17 tests
  that run on the PC (`host-tests/`).
- **Tools**: `scripts/structure_check.py` (with every gate, W13); `scripts/release/gate.ps1` fixed for
  Windows PowerShell 5.1; `docs/HTTP_REFERENCE.md`, every route on the boards and what it changes.
- Each phase proved by A/B on WS_P4_5, WS_P4_4B and CYD_P4_1060: no tree or pixel differences. The
  window builds ~3.5 ms faster. All nine environments build through the gate. Signed off by the
  owner (`docs/archive/TEST_2.12.md`); the 4B dialog's overlapping buttons, present before, are #96.

## v0.2.12 - 2026-10-13

K17 - a group's member made a card of its own, and what its testing found (DECISIONS K65-K70).

- **Add card to dashboard**: in a member's own view (Desk > Members > a lamp), SETTINGS makes a
  copy of the member as a card - right after its group card, with its priority and its group, HA's
  name. Kept in the settings file (`CardAdded`); a member learnt after boot appears once HA lists
  it. **Remove card** on such a card asks in a dialog over everything; removed cards stay in the
  file, the page's pool.
- **The owner's group model** recorded for 2.11 (K67, K68, `docs/design/group-cards.md` §5).
- **Every MDI icon the owner's HA serves** is in the icon font (66 more), so a card made from any
  entity draws HA's icon; `scan_ha_icons.py` follows groups' members.
- **A long name never shrinks the hero**: one line's room; a second line only where it fits under
  the disc, else "...".
- **SETTINGS follows its rows**: it is made again when they change (another view, Added) - the
  rows of whichever view it first opened in had stuck.
- **Scenes on a tap**: Tap action "Scenes..."; SCENES' Default scene and Scene action (Load
  default / Cycle scenes). Entity properties in the owner's order. Visibility hidden until
  clusters exist (K68).
- Signed off by the owner on WS_P4_5, WS_P4_4B and CYD_P4_1060 (`docs/archive/TEST_K17.md`); all
  nine environments build.

## v0.2.11 - 2026-10-12

2.10d and 2.10e, merged together (K51) - the card popup is done (#65).

- **2.10d, choices saved (#65)**: every card has a stable id (`<area>_<label>_<yymmdd>_<hhmm>`,
  K33) and the owner's choices live in one file on LittleFS (`/cfg/settings.json`), by name, applied
  over the compiled dashboard and written once when the window closes (`docs/design/card-sheet.md`
  §17, CLAUDE.md "Saved settings"). Groups for card labels, HA areas linked in, colour as the key
  (K46-K47). A file that will not read makes the boot read-only. The window's panels are dragged
  whole - up to show hidden rows, down to fold (K50).
- **2.10e, History from Home Assistant (#65)**: a number's chart and anything on/off as its
  activity (`card-sheet.md` §18, K52-K64). The chart: 24 / 12 / 6 / 1 h, axes in round steps on
  the local clock, a gradient under the line, min and max in the top line and as dotted lines at
  their times, a finger reading any point, history first and 5-minute statistics as the fallback.
  The activity: day headings, the time and how long ago, how long each state lasted and what HA
  says triggered it, 24 h / 7 days. Span buttons or "Showing: ..." that a tap steps; every choice
  kept per card. `HaHistory` asks HA's websocket; its 48 KB buffer is in PSRAM. `TimeFormat`, after
  NINA's `time_parse`, says every time and duration.
- **SETTINGS**: Units (°C / °F per card) and an Entity properties view (HA's name, id, area,
  device). A card whose tap toggles nothing opens on History.
- **Fixed**: the P4 freezes (A15) - the PPA hanging on a rotated strip, IDF #19023, patched in the
  rebuilt P4 libraries, with no freeze seen since; `-D DEBUG_HANG` (HangWatch, `GET /hang`) for the next one. Card label
  colours when HA's areas arrive late; an on card's fill lost when they did.
- Open, filed: the P4_5's white flash (#91), the window title's name (#90), chart options (#92).
- Signed off by the owner on WS_P4_5, WS_P4_4B and CYD_P4_1060 (`docs/archive/TEST_2.10d.md`,
  `TEST_2.10e.md`); all nine environments build.

## v0.2.10 - 2026-10-07

- **2.10c, Home Assistant's lights get the controls (#65)**: an HA light's levels (brightness,
  colour temperature, hue) are read from its attributes and set with `light.turn_on` and its data;
  a refused call is FAILED at once and says why. A group defined in HA or Hue (`light.office`) has
  its members learnt and gets the Members view, commanded as itself unless a member is paused; a
  group is paused exactly when all its members are. A light's scenes are learnt from HA (hidden ones
  hidden, a card setting Visible / All / Off) and shown in a Scenes view under the chart.
- **The window, redesigned over nine rounds** (`docs/archive/TEST_2.10c.md`): corner chips as tabs with
  hard-linked back navigation and a title that links up (and gives way on narrow windows); a control
  deck after HTC's TouchFLO 3D - a selector that slides and snaps, in four hand-drawn metal looks
  that follow the scheme; unlit chips as soft dents; the slider keeps its place in Scenes and Color;
  a SETTINGS panel as wide as its rows with dropdowns and a checkbox, and a CHART panel (a demo)
  beside it on History; US spelling on screen.
- **Measured and fixed**: the panels' slides on the P4_5 (35-105 ms frames to 8-40), the 1060's deck
  moving a pixel on Color, faces drawn flat once a cache filled.
- **`GET /panel?resend=1`** on DSI boards: the panel's init sequence again, for the 4B's
  intermittent washed-out picture. Reading from a DSI panel that shows a picture blanks it (LESSONS).
- Signed off by the owner on WS_P4_5, WS_P4_4B and CYD_P4_1060, also seen on WS_P4_7B and
  CYD_P4_4880 (the window in portrait for the first time); all nine environments build.

## v0.2.9 - 2026-10-06

- **2.10b, the light controls (#65)**: a light's window opens on a tall slider - brightness, colour
  temperature or a hue strip with eight swatches - with Power | Brightness, Temperature, Colour
  showing only what the light can do; a tap jumps, a drag sends every 300 ms and on release. Light
  state lives in the entity registry: what a light can do, its levels, and light commands confirmed
  only by a matching report (else reverted and FAILED). Four kinds of virtual lamp.
- **Groups act as HA's light group**: what any member can do is offered, each member is sent what it
  can take, the card fills to the mean brightness of the members that are on, "On when: any / all"
  (`GroupOn`), and a tap on a group follows HA's rule. A paused member is out of its group; Members
  rows open a member's own controls.
- **The popup**: up to 2:1 and at most 100 mm wide, its contents the same share of it on every
  board, the group centred and nothing moving within one window; a paused window greyed with a
  PAUSED pill; Paused is a switch; the chart button always in the corner; the deck's rows built on
  first open; the tab's corner specks gone. Signed off on WS_P4_5 after seven rounds
  (`docs/archive/TEST_2.10b.md`), also seen on WS_P4_4B and CYD_P4_1060.
- **LVGL's pool in PSRAM on every P4, 512 KB (#88)**: +128 KB internal heap; full-screen frames ~10%
  slower.
- **The system drawer's contents slide with its edge (#81).**
- Filed: #86 card control styles, #87 a companion web app, #88 LVGL memory and 9.6.

## v0.2.8 - 2026-10-05

- **2.10a, the card popup (#65)**: long press (250 ms) opens a window on every card - usable in its
  first frame, closed by the X, a tap outside, a drag down on its title or 60 s untouched; the card
  pressed in with an accent border while held and while its window is open; a switch's toggle that
  slides and can be dragged; a SETTINGS deck shaped like a folder tab, with Pause (kept on the
  device) and the label / custom name / shown rows waiting for 2.10d; sparks along the window's
  edge; history and members views as placeholders; the chart icon. Long press no longer pauses a
  card. Signed off on WS_P4_5 after twelve rounds (`docs/archive/TEST_2.10a.md`).
- **Fixed on every esp_lcd DSI board (#67):** the repair fallback raced the PPA's strip rotations
  and left stale lines on glass (`docs/display/history.md`).
- Already on `main` since v0.2.7, untagged until now: `GET /screenshot` (#58); 2.9's esp_lcd display
  stack on six of nine boards, `GET /bench` (#67); overlay layers that never scroll (#68); the
  battery prototype (#72); wall-clock time from SNTP (#74); the floating tag (#80); Linen round two;
  the README and MIT licence; design interview sections 1 and 2.

## v0.2.7 - 2026-09-24

- Through milestone 2.7 - the last tag whose number was also a milestone's.
