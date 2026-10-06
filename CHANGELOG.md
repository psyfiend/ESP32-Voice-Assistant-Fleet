# Changelog

Versions are `v0.<phase>.<release>`; a build adds the commits since that tag, and `+dirty` for a
modified tree (`v0.2.8.14+dirty`). The phase follows the roadmap. The release number counts tagged,
signed-off merges to `main` in the order they happen - it does not follow milestone numbers, which
began finishing out of order (decided 2026-10-05; `docs/DECISIONS.md` W3). This file is where a
version is matched to its milestones.

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
