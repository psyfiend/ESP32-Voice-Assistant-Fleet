# TEST - #99: satin and glass

Branch `feat/99-satin` (off `main` at `v0.2.15`), built 2026-10-14 evening from the owner's go on
the approach. The mock it follows: "Glass, Satin, Flat"
(https://claude.ai/artifact/G5XANwGMdgWKR19HS1ZQvj); the decision K77, how it is built K85,
`docs/design/components.md` "Finishes". **The P4_5 and the 4B run it** (with your local lines); the
1060 runs `v0.2.15`.

## What changed

| Where | Before | Now |
|---|---|---|
| **Members' rows** | flat | **satin**; the state in colour - "On, 100%, 2710 K" in the active colour, "Off" white, Paused / Unavailable dimmed (it was all dim grey) |
| **Scenes** | flat pills; a blue ring round the one loaded | **satin**; the one loaded is **glass**, its name in the accent (no ring) |
| **The Remove dialog** | flat buttons | **satin**, Remove keeping its red |
| **Power** beside the control deck | a plain button on a ribbon of its own; the icon orange while on | **glass while the light is on** (icon orange), **satin while off** (icon white); no ribbon of its own |
| **Pressed** | each its own tint | satin **darkens**, glass **lightens**; the button **sinks 1 px** |

A paused (greyed) window keeps its scenes and Power plain, as before. History's activity rows stay
flat (Q1). Nothing else changed.

**How it is drawn.** LVGL's own gradients band pink and green on these 16-bit panels (LESSONS), so
satin is painted by hand and dithered, as the selector's metal is. Painted as a full-colour image
with see-through corners it made a full redraw of Members 22 ms slower on the P4_5; painted in the
panel's own 16 bits, LVGL cutting its corners, the cost halved.

## The pictures (before above, after below; P4_5 unless named)

Members (P4_5, then the 4B):

![Members, P4_5](followups/99_members_p45.png)
![Members, 4B](followups/99_members_4b.png)

Scenes - before; after; after with Bright loaded:

![Scenes](followups/99_scenes_p45.png)

The Remove dialog:

![The dialog](followups/99_dialog_p45.png)

Power - before (light on); after, light on (glass); after, a light that is off (satin, Lamp 1);
then the 4B before / after:

![Power, P4_5](followups/99_power_p45.png)
![Power, 4B](followups/99_power_4b.png)

## How it was checked

- **The harness**, every state, `v0.2.15` against the branch on the P4_5 (48 states): only the
  intended states changed - every light window (Power), Members, Scenes and the dialog. History,
  Properties, SETTINGS, the drawer and About: no differences. The 4B: the same five states, as
  expected. WS_S3_TOUCH_LCD_4B compiles.
- **What it costs to draw**, P4_5, a full redraw with the window open (`/bench what=full`, 20
  frames; `v0.2.15` -> now): Members 113.1 -> 124.1 ms, Scenes 115.8 -> 121.1, the dialog 165.9 ->
  170.3, the controls 127.3 -> 128.0. A press redraws only that button. The first window after a
  boot opens about 35 ms slower (Power's two faces painted once, then kept); later opens as before.
- **A scene loaded** (Bright, from the PC): glass at once, also when the view is opened after. Bright
  left the Office lights where they were (100%, 2710 K).
- **Not measured on an S3** (W6): none can be flashed on the desk (Q5).

## Tests for you

| # | Do this | PASS if |
|---|---|---|
| T1 | Desk > Members | Rows satin - a faint light at the top, a soft edge; each state in colour (on orange, off white). Press a row: it darkens and goes down a pixel; let go: it opens the member |
| T2 | Desk > Scenes; load one | The scenes satin; the one loaded turns glass with its name in blue; move the light's slider - it is satin again |
| T3 | Remove card's dialog (a card you added, or `/popup?removecard=3`) | Cancel satin, Remove satin in its red; each darkens and sinks while pressed |
| T4 | Desk's controls; Power off and on | On: glass, orange icon; off: satin, white icon; pressed: lighter (glass) or darker (satin), down a pixel |
| T5 | Pause a light (SETTINGS) and open its window | Scenes and Power plain and greyed, as before |
| T6 | The Linen and Fleet schemes | Satin still quiet and visible; on Linen the rows' edge a soft shade |
| T7 | Open History, Properties, SETTINGS, About | As before |
| T8 | Use it for a while | It feels as quick as before (scrolling Members, opening windows) |

## Questions for you

- **Q1. History's activity rows** can be tapped (a row opens its details), so by K77 they are satin
  too - but they are left flat for now: a list of ten or more satin rows costs the most to draw, and
  that wants the S3 measured first. **Recommended: satin, after the S3 measurement.**
- **Q2. Pressed**: the mock flips satin's light; here it darkens - one face, tinted - and sinks a
  pixel, which costs nothing to keep. Enough? Recommended: yes.
- **Q3. Power without its own ribbon** (as the mock): it now stands alone beside the selector's
  ribbon. Recommended: keep.
- **Q4. The first window after a boot** opens about 35 ms slower, painting Power's faces once.
  Recommended: accept (a paint in whole numbers could halve it later).
- **Q5. An S3 for the frame check** (W6): the 8048 or the 5B plugged in, and I measure Members, Scenes
  and the dialog there before this merges. Or merge on the P4 numbers and measure later?
- **Q6. The component pictures** in `components.md` (scenes, dialog, selector) show the old look;
  they are refreshed at sign-off.

## Your results

(empty)
