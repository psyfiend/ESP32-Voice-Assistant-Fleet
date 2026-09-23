# Pages — model, navigation, and what 2.6 builds

**Status: 2.6 BUILT, 2026-09-23, on `feat/17-page-swipes`; awaiting glass (`docs/TEST_2.6.md`).**
Written from the owner's brain-dump of 2026-09-22. His answers of 2026-09-23 settled the proposals:
**wrap-around on** (test it now, decide the default later); **every knob is per page except Deck
and Hide Bar** - including the colour scheme; **page 2 is strictly the Fleet dashboard**, no
overflow; card links wait for 2.10. The page TITLE is centred in the header bar ("House", "Fleet"),
with the dots beside it; the device name keeps its place on the left.
Items marked **DECIDED** are his; items marked **PROPOSED** were the questions put to him. Companion to
`cards.md` (what is on a page) and `dashboard.md` (the grid a page lays out on).

---

## 1. What a page is

A `PageSpec` (`include/Cards/PageSpec.h`) — data, not code — with NINA's frozen-identity rule
already in place: a numeric `id` that is never reused and a string `slug` that is the external key.
See `REFERENCE_PROJECTS.md`, "the page/view/grid back-end".

**DECIDED — layouts are PER PAGE.** Columns, rows, header mode, variant, label mode, area on/off and
temperature unit belong to the page, not the device. Required anyway for pages opened from a card
(a room page wants a different grid from the home page). Most of these fields already exist on
`PageSpec`; columns and rows do not yet.

**PROPOSED — the drawer's knobs edit the CURRENT page**, live and in RAM, until the build sheet and
the settings pages (Phase 3 / 4) make them persistent.

## 2. Two kinds of page

**DECIDED (owner's idea):**

| Kind | Reached by | In the swipe order | Gets a dot |
|---|---|---|---|
| **swipe** | horizontal swipe, and everything below | yes | yes |
| **linked** | only an intentional act: a card's tap/long-press, a gesture target, the overview | no | no — the header shows its title instead |

A linked page needs a way back. **PROPOSED:** a swipe right from a linked page returns to where it
was opened from (a one-deep history is enough to start), plus a back mark in the header bar.

This is the owner's "group cards link to a page with all the group's entities" and "pages by Area,
opened from the main page rather than swiped to". It is also where `cards.md`'s per-card detail
view can live for types that deserve a whole page rather than the 2.10 popup.

## 3. Overflow

**DECIDED — no automatic overflow onto the next page.** The owner considered it and rejected it:
"clumsy and not a good strategy to be useful, predictable, and reliable". A page shows what fits,
drops the rest by priority (what `CardPage::plan()` already does since 2.5), and says so in the log.
A build-sheet flag that lets a page *accept* overflow was considered and set aside with it.

Consequence worth stating: on `CYD_S3_3248` the House page shows 8 of its 18 entities. Seeing the
rest means authoring a second page, which is the build sheet's job, not the engine's.

## 4. Navigation

**One owner of page changes** — NINA's `nina_nav_arbiter` lesson. Nothing calls "show page X"
directly; every source (swipe, card link, gesture target, overview, boot) asks one navigator, which
commits at most one change. The navigator is where history, wrap-around and the indicator live.

- **Order: House (HA) first, then Fleet.** DECIDED. Boot lands on the first swipe page.
- **Wrap-around. PROPOSED: on.** The owner's objection to no-wrap was right: with ten pages the way
  back to page 1 is nine swipes. With the indicator always showing where you are, wrapping is not
  disorienting, and card links plus the overview cover "jump somewhere specific". A build-sheet
  flag can turn it off later.
- **Boot always lands on the home page**; the last page is not remembered (no NVS write per swipe).

### The indicator

**DECIDED to try:** dots in the system header bar, one per swipe page, the current one filled —
NINA does the same with a row of 10 px dots. Past about a dozen pages, dots stop being countable
and become "3 / 15" text. When the bar is hidden, a toast carries it: **"House - 1 of 2"** for about
1.5 s after every page change (the owner's idea), shown whether or not the bar is visible.

## 5. Gesture targets

**DECIDED (owner's idea), PROPOSED shape:** the edge swipes become a small table rather than code —

| Gesture | Today | Configurable to |
|---|---|---|
| down from top, right half | system drawer | any page, panel or action |
| down from top, left half | log | " |
| up from bottom | deck | " |
| left / right | — | page navigation (2.6) |

2.6 can move the existing three into the table with today's targets unchanged, so the build sheet
only has to fill it in.

## 6. The overview ("alt-tab")

**Possible, with one honest limit: the thumbnails would not be live.** Only one page's widgets
exist at a time — a page is rebuilt on arrival, which is what keeps `lv_mem` inside its 128 KB — so
there is nothing to render a second page from. What is possible is a **snapshot taken each time a
page is left**, scaled down and kept in PSRAM, then shown as a grid of thumbnails. A thumbnail at a
quarter size is ~115 KB on `WS_P4_5`, ~19 KB on the 3248.

It depends on the snapshot capability in #58, so it comes after that.

## 7. Transitions

Today: an instant swap. What else is possible, and what each costs:

| Effect | How | Cost / risk |
|---|---|---|
| **Slide, two live pages** (what NINA does) | both pages built, animate `translate_x` | both pages' cards in `lv_mem` at once - ~2.8 KB a card, ~85 KB for House + Fleet against a 128 KB pool. Too tight |
| **Slide, snapshot** | snapshot the old page into PSRAM, build the new one, slide the picture off | one live page, no `lv_mem` spike, no layer. Drawing a full-screen image per frame: fine on P4, slow on the S3s |
| **Fade** of a page object | `opa` on the page | forces a full-screen LAYER - the allocation that froze `WS_P4_5` (LESSONS.md). No |
| **Fade** of a snapshot | `image_opa` on the picture | no layer needed. Cheap |
| **Zoom from a dot / card** | scale transform on the snapshot | software scaling per frame is heavy; this is exactly what the P4's PPA accelerates |

**So the snapshot is the enabler for all three: transitions, the overview, and screenshots.** One
capability, three features. And LVGL 9.5 ships a **PPA draw unit** (`LV_USE_PPA`, currently 0 in
`lv_conf.h`) that accelerates exactly the image blends and transforms the fancy effects need, on
the P4 boards. It does not touch Arduino_GFX's per-pixel rotation, which is 2.9's problem.

## 8. What 2.6 builds — PROPOSED

1. `PageSpec` gains `kind` (swipe / linked) and per-page column and row overrides.
2. A page list: House (id 2), Fleet (id 1). `-D USE_HA_DASHBOARD` goes away.
3. The navigator: one entry point, wrap-around, one-deep history for linked pages.
4. Horizontal swipes, from anywhere, through the navigator.
5. Header dots, and the "House - 1 of 2" toast.
6. Knobs act on the current page.
7. The three existing edge gestures moved into a target table, behaviour unchanged.

**Not in 2.6:** card -> page links (they belong with the long-press work in 2.10, which decides
what a long press does), linked pages beyond the mechanism, the overview, transitions beyond the
instant swap, snapshots (#58).
