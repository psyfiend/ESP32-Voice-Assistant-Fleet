# Test sheet — milestone 2.7, first round

Branch `feat/18-card-types`. Flashed to `CYD_S3_3248` (COM10) and `WS_P4_5` (COM15). Decisions
behind every item: `docs/design/cards.md` §13.

Each test says what to do, what PASS looks like, and what FAIL looks like. "Knob" means a button in
the System drawer (swipe down from the top-right, or tap the status icon).

**Nothing below has been seen on glass by anyone yet.** Every result is new information.

---

## A. Boot and data

**T1 — Boot, both boards.** Power-cycle each.
- PASS: dashboard appears, cards show live values within ~5 s, no freeze.
- FAIL: a freeze with no output, a blank card grid, or a reboot loop. (A freeze with nothing on
  serial is most likely an `LV_ASSERT` — LESSONS.md — so note which page and header mode.)

## B. Icons

**T2 — Every card has a corner icon.** Look at every card.
- PASS: lights show a bulb; **Desk** a *group* bulb (its corner override); garage doors a garage;
  occupancy a motion sensor; temperatures a thermometer; lux a sun.
- FAIL: a card with no corner icon, or a corner icon that is an empty box.

**T3 — Corner icon glued to the corner in No-header mode.** Knob **Tag/Bar/None** until None.
- PASS: the corner icon sits as far from the card's top border as from its left border.
- FAIL: it sits clearly lower than it is far from the side (the old behaviour: a header's height
  too low). Also note if the hero or name now collide with anything, or with a STALE badge.

**T4 — Corner icon size.** No change is expected on these two boards: at today's grids the rule
puts only `WS_P4_7B` and `CYD_P4_1060` on the larger face.
- PASS: same size as before on the 3248 and P4_5.
- To judge the actual change, the 7B has to be flashed. Worth doing before sign-off.

**T5 — Garage door, open and close.** Open one door, then close it.
- PASS: open -> garage-open glyph, card in the active colour; closed -> closed glyph, idle.
  The label stays "North"/"South".
- FAIL: the glyph does not change, or the card changes colour but not glyph.

**T6 — Occupancy.** Walk into the kitchen or office.
- PASS: the glyph flips between motion-sensor and motion-sensor-off. This one comes from HA's live
  icon (your template sets it), so it exercises the live-icon path.

## C. Lights

**T7 — Brightness fill.** In HA, set **Kitchen Table** (dimmable) to about 50%, then 100%, then off.
Repeat with **Desk**.
- PASS: 50% fills the card from the bottom up to about half; 100% is the fully-filled card you
  know; off is empty. Name and icon stay readable wherever the fill edge lands.
- FAIL: no fill change, a fill from the top, a soft/blurred edge, text the same colour as what is
  behind it, or anything poking outside the card's rounded corners.

**T8 — Light colour in the disc.** Turn **Desk** on. Then change its colour in HA to something
saturated (blue).
- PASS: warm white shows as a warm disc; blue shows a blue disc with a light glyph. Non-colour
  lights keep the ordinary disc.
- Judge: disc vs glyph. The glyph takes the colour only when a card is too cramped to draw a disc.

**T9 — #63, fading light.** Tap **Desk** off (it fades).
- PASS: it goes to off and stays there. No FAILED.
- FAIL: FAILED appears. **If it appears at about 5 s** rather than immediately, the fade is longer
  than the reconcile window — tell me roughly how long the fade takes.

**T10 — #63, the cost.** A command that genuinely fails now reports FAILED after the 5 s window
instead of instantly. The HA page has no entity that refuses commands, so this is not testable here
without unplugging a light. Optional.

## D. Labels

**T11 — Label knob.** Knob **Name** -> **State** -> **No lbl**.
- PASS: Name shows names; State shows "Open"/"Closed" on doors, "Detected"/"Clear" on occupancy,
  "On"/"Off" on lights; No lbl hides the line and the disc re-centres.
- FAIL: raw "on"/"off", or a clipped line.

## E. Sizes (#62)

**T12 — Hero value on cramped cards.** Compare the P4_5 against memory or a photo from before.
- PASS: values that were clipped or crowded now fit, at a smaller size; cards that were compact may
  now show their status line (battery / "Seen") with a smaller number.
- FAIL: a value still clipped, or text sizes that look inconsistent card to card in a way that
  bothers you. That last one is taste, and taste is the point of this test.

## F. Scheme repaint (#64)

**T13 — Scheme cycle.** Open the drawer and a deck panel. Knob scheme: Fleet -> Slate -> Midnight ->
Paper.
- PASS: the deck panels, the drawer background and its buttons change with each scheme.
- Judge on Paper: your open question on #64 — do the panels separate enough from the cards?

**T14 — Scheme with a lit light.** With Desk on at a partial brightness, cycle schemes.
- PASS: the fill and its edge repaint in each scheme's active colour.

## Round two — flashed 2026-09-22

First-round results: T1-T3, T5-T9, T11-T13 PASS. T4 needs the 7B; T10, T15 not run.

**R1 — Compact by default.** Boot. The variant knob reads **Cmpct** and no card shows a "Seen" line.
**R2 — Compact name position.** PASS: every name sits midway between its disc's bottom edge and the
card's bottom edge, and names in one row are level whether the card is a light or a temperature.
**R3 — Corner tints.** Lights yellow-white, garage doors blue, occupancy teal; temps and lux as before.
**R4 — Desk.** Corner: the plain bulb. Hero: the group bulb, filled when on, outline when off.
**R5 — Label knob, four steps.** Name -> State -> No lbl -> No icon. No lbl hides names on the
temperature cards too. No icon: names centred, no discs, corner icons and fills still there.
**R6 — Seen, deterministic.** Knob to **Full**, then Name/State/Auto in any order. PASS: a given card
shows "Seen: 22m" or "22m" the same way every time; only a narrow card with a battery drops the prefix.
**R7 — Active ink.** In Paper, Linen and Frost, icons and names on a lit card are dark, Desk included.
**R8 — Light schemes.** The scheme knob now runs Fleet, Slate, Midnight, **Linen, Frost**, Paper.
Judge the two new ones: card vs background, and the device name / panel titles / SYSTEM - DIAGNOSTICS.
Also: "Show Touches" in the Display panel is readable on all three light schemes.
**R9 — 3 s window.** Tap Desk off: still no FAILED.

## G. Residuals

**T15 — `ST_UNAVAILABLE`.** Unplug one sensor that HA marks unavailable quickly.
- PASS: the card shows N/A with the diagonal. Never yet seen on hardware.
