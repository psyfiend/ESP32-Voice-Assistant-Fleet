# Milestone 2.4 — sign-off test plan

**Status: awaiting hardware.** Every acceptance criterion in issue #15 has an implementation and
all eight environments build. What 2.4 needs now is confirmation on glass, not more code.

Two boards, in this order. **Flash `CYD_S3_3248W535` first** — it is the veto board, it is the one
that froze, and it is where an off-by-one becomes a crash rather than a cosmetic bug.

```bash
pio run -e CYD_S3_3248W535 -t upload
```

```bash
pio run -e WS_P4_TOUCH_LCD_5 -t upload
```

Open the card page from **System panel → Cards**. Six buttons across the top, all independent:

| Button | Cycles through |
|---|---|
| `< Back` | returns to the dashboard |
| scheme name | Fleet → Slate → Midnight → Paper |
| `Tag` / `Bar` / `No hdr` | the three header modes |
| `Area on` / `Area off` | whether the area is displayed |
| `Fill` / `Icon` | how an active state card shows it |
| `Live` … `Paused` | pins every card to one state; 6th press releases |

---

## A. Gates — these block sign-off

Nothing else matters if one of these fails.

| # | Test | Pass | Fail |
|---|---|---|---|
| **A1** | 3248: open System → Cards | Page renders; serial prints `[Cards] Page 2x3 visible…` then `[Cards] 9 cards, lv_mem…` | Freeze needing a power cycle. **Send serial and stop** — this is the unconfirmed fix |
| **A2** | P4_5: open System → Cards | Same, reporting `Page 5x2 visible` | as above |
| **A3** | Both: leave the page open 2 min | No reboot, no panic, header clock still moving | Reboot loop or watchdog |
| **A4** | Both: open and close Cards 5× | The `lv_mem` baseline returns to roughly the same number each time | Baseline climbs every open — a leak |

**A1 is the whole point of this flash.** It failed before because `CardPage` defined only as many
grid rows as *fit* the viewport, while placement flowed into as many as the cards *needed*. LVGL
indexed past the end of that array and hit `LV_ASSERT`, which `lv_conf.h` defines as `while(1);`.
The P4 survived by luck — nine cards at five columns fill exactly 5×2 with nothing left over.

---

## B. Layout and grid

| # | Test | Pass |
|---|---|---|
| **B1** | Grid size | P4_5 reports `5x2`; 3248 reports `2x3` |
| **B2** | All nine cards exist | The 3248 scrolls to reach rows 4–5 |
| **B3** | `Free Heap` span | Two columns wide on P4_5; shrinks to one on the 3248 rather than wrapping onto a row alone |
| **B4** | No horizontal scroll | The page never moves sideways |

---

## C. The three header modes

Cycle the header button. **The card's width and grid position must not change between modes** —
only the body inset, and the card height in tag mode.

| # | Mode | Pass | Fail |
|---|---|---|---|
| **C1** | Bar | Filled band across the card's top, inside it. Area left, STALE right | Band pokes square corners past the card's rounded top |
| **C2** | Tag | Pill above the card's top-left, attached, **outside** the card | Pill covers the card's own contents, or floats detached |
| **C3** | Tag: heights | **Every** card is shorter by the tag's height — tagged or not, all identical | Cards with tags sit at a different height from cards without |
| **C4** | Tag: spacing | The space between a tag and the card **above** it looks the same as the space between two cards in bar mode | Tags crowd the card above them |
| **C5** | No hdr | No area text anywhere; the body uses the whole card | Area still rendered somewhere |

---

## D. Area on / off

| # | Test | Pass |
|---|---|---|
| **D1** | Area off + Bar | Band remains — it is part of the card's shape in this mode — just without area text |
| **D2** | Area off + Tag | No pills appear, and **nothing else on the page moves**. Card heights and positions identical to area-on |
| **D3** | Area off + No hdr | No visible change; this mode never showed an area |

**D2 is the interesting one.** Clearance belongs to the *mode*, not to whether a given card happens
to have an area — which is what makes a mixed page lay out consistently.

---

## E. States — use the state button, do not wait

Nothing on a running panel goes stale inside a test session: the Zigbee entities are on a 30-minute
window, the system ones refresh every 5 s so they never expire, and long-stale is an hour. The state
button pins every card so all five treatments are one tap apart.

Worth running **in each header mode**, since each presents the marker differently.

| # | State | Pass | Fail |
|---|---|---|---|
| **E1** | Stale | Marker appears — in the band (bar), as a second pill on the right (tag), as a floating badge top-right (none). **Card does not dim** | Any dimming |
| **E2** | Long | As stale, **plus** a thick diagonal corner to corner. Value still readable through it | Value obscured |
| **E3** | Failed | As long, in the red / bad colour | |
| **E4** | Partial | Warning marker appears, but the card body keeps its **normal** state colours | The body is taken over — a partial failure must not claim a total one |
| **E5** | Paused | The card **dims**. The only state that does | |
| **E6** | 6th press | Everything returns to live and normal derivation resumes | A card stays pinned |

---

## F. Interaction

Only `Obeys`, `Ignores` and `Both` respond to a tap. Everything else is read-only — outbound
commands (#44) are deliberately out of scope for 2.4.

| # | Test | Pass |
|---|---|---|
| **F1** | Tap `Obeys` | Goes active within ~1 s and stays |
| **F2** | Tap `Ignores` | Goes active, then ~5 s later shows FAILED plus the diagonal |
| **F3** | Tap `Both` | Both children are commanded; once `Ignores` fails, `Both` shows **PARTIAL**, not FAILED |
| **F4** | **Order independence** | Tap `Ignores` alone and note `Both`. Release, tap `Both` alone, note `Both`. Release, tap `Obeys` then `Ignores`. **All three orders must leave `Both` in the same state** |
| **F5** | Long-press any card | Dims and shows PAUSED. Long-press again releases it |

**F4 is the regression test for a real bug.** Card state used to depend on which card you had
tapped, because each tracked only its own commands. The verdict now lives on the entity, so a
parent and a child bound to the same switch cannot disagree.

---

## G. Icons and type

| # | Test | Pass | Fail |
|---|---|---|---|
| **G1** | Deck temperature icon | A thermometer | A droplet — the MDI subset is not being used |
| **G2** | Deck motion icon | A motion sensor, and the glyph **differs** between occupied and clear | The same glyph either way |
| **G3** | Signal / Free Heap | A wifi glyph and a memory glyph | A generic gauge on both |
| **G4** | Battery corner | A battery glyph matching the level, beside the percentage | |
| **G5** | Smallest text | About the size of the `AUDIO` / `DISPLAY` panel titles — that was the anchor the whole type scale was derived from | Noticeably smaller |
| **G6** | Unit beside value | Smaller than the number and sharing its bottom edge | Same size, or sitting beside the card's *name* |
| **G7** | `Deck` label | One line | Wrapped to `Dec` / `k` |

---

## H. Variants and diagnostics

| # | Test | Pass |
|---|---|---|
| **H1** | System panel → Dump Config | A `CARDS` section listing each card with span, min span, priority, **variant** and state |
| **H2** | 3248 variants | Some or all cards report `compact` — its cells are 141×121 |
| **H3** | A compact value card | Status corners **absent** (battery and last-seen) |
| **H4** | A compact state card | Name **absent**; disc centred and still present |

Cards going compact on the 3248 is the feature working, not a bug. What *would* be a bug is text
shrinking below the type scale instead of content being dropped.

---

## I. Schemes

| # | Test | Pass | Fail |
|---|---|---|---|
| **I1** | Cycle all four schemes | Every card repaints completely | A card keeps an old colour — something cached one, which the design forbids |
| **I2** | Paper (light) scheme | Readable; borders and shadows still make sense | |

---

## Things that look wrong and are not

- **Deck sensors and system cards ignore taps.** Read-only. #44 is out of scope for 2.4.
- **`Last seen:` may show as a bare age on the 3248.** A 141 px card cannot hold the prefix beside a
  battery reading, so it drops it rather than colliding. Measured at render, not guessed per board.
- **Tag mode gives the body more room than bar mode.** That is the trade between them: a tag costs
  the card nothing, a bar costs it a strip.
- **Only paused dims.** Staleness is deliberately conspicuous — `cards.md` §3 rejects dimming for it.
- **A card may show `--`.** It has never received a value, which is a different thing from zero.

---

## What to capture

**Serial, from boot through a few page opens.** The lines that matter:

```
[Cards] Binder running at 100 ms
[Cards] Page 5x2 visible, cell 230x264 px, row gap 55 (gap + tag)
[Cards] 9 cards, lv_mem 24928 -> 49772 (+24844, 2760 B/card), frag 1%
[Cards] forced state: stale
```

Plus the `CARDS` section of a Dump Config, which carries the variant resolution.

**Screenshots** for anything visual — the three header modes, tag spacing, a compact card, and any
state treatment that looks wrong.

**One batch beats one message per finding.** Fixes get turned around together.

---

## After sign-off

1. Merge `feat/2.4-card-base` to `main` with `--no-ff`.
2. Tag `v0.2.4`.
3. Close #15. See `ROADMAP.md` §7 for what 2.4 does and does not let us close.
