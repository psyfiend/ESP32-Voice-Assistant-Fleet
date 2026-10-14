# Layout engines - where cards go (design for 2.11)

2.12 (P5) built the interface and the one engine the dashboard has always had; this file is the
design 2.11 builds on it. The interface is decided (DECISIONS D-10); **the engines below are
proposals for the design interview**, not decisions. Moved here from `ui-foundation.md` §8 on
2026-10-14, with the owner's first hero cards added.

## The interface (built, 2.12)

`include/Layout/Layout.h`, `src/Layout/Layout.cpp` - pure C++, no LVGL, Arduino or ESP-IDF, tested on
the PC in `host-tests/test/test_layout`.

- A **`Container`** is a grid of units (columns x rows; a dashboard cell is 2 x 2 units at the
  default subdivision).
- An **`Item`** has the span it would like (`prefSpanX/Y`) and the least it will accept
  (`minSpanX/Y`), an optional pinned spot (`col`, `row`) and a `priority`; `present = false` is an
  empty slot.
- An **`Engine`** takes the container and the items and writes a **`Place`** for each - where it
  went and at what span, or that it did not fit; `pinFail` says a pin was refused, and stays set
  across calls so it can be reported.
- **`Events`** tells what the engine decided as it decides (a pin refused, an item dropped), for a
  log.
- **`FlowEngine`** is today's dashboard: pins first, then reading order, the widest span that fits
  at the first free spot, and while anything does not fit, the lowest priority is dropped (ties: the
  later one) and the whole layout tried again.

`CardPage` turns its cards into items, asks its engine, and lays out the places that come back
(`src/Cards/CardPage.cpp`). **A new arrangement is a new class implementing `Layout::Engine`**, with
host tests for its properties before it draws anything; a page chooses its engine.

## A cluster is a container inside an item

On the page, a cluster (a group panel, `group-cards.md` §5.3) is one item, its span from its member
count (or declared). Inside it, an engine runs again on the cluster's own sub-grid - its span in
units less its frame - with the members as items, each a unit or two (a member drawn without its own
border, compact). The page places the cluster as it places a card; the cluster places its members as
the page places cards. Same interface, a smaller container; the flow engine works there unchanged.

## The packing ("bento") engine

What it must be: **dense** (no holes a smaller card could fill), **deterministic** (the same items
give the same layout, every boot) and **stable** (one card more or less moves as few others as
possible - a layout that reshuffles itself is worse than one with a hole).

How: items sorted by area, largest first, ties by declaration order; each placed where it leaves the
least unusable space (a maximal-rectangles packer on the unit grid), trying spans from preferred
down to least; and stability from the LAST layout - each item first tries its previous place
(`Place` kept per card id, K33, in the settings file), and only those that no longer fit are packed
afresh. Priority still decides who leaves when nothing fits. Testable off the board: the same input
twice gives the same output; one item added moves at most the items after it.

## The domain weight map

Which cards earn space: a weight per card - its domain's (climate and media heavier than a lone
sensor), times how much it is used (taps counted in the settings file) - turned into a target area,
spans chosen within each card's least and preferred. The weights are data (a table beside the
tokens, a page able to override them), never code.

## The hero region ("primary-region page")

One hero region takes a fixed share of the page - a 2 x 2 at the top left, or a column - and the
rest of the page is flowed or packed around it: the container split into regions, each region its
own engine run. A card is put in the region by a `primary` flag in its spec.

**The first heroes the owner wants (2026-10-14):** a music player, and **NINA's multi-face clock**
(`ESP32-P4-NINA-Display`, `REFERENCE_PROJECTS.md`). The clock is designed for a square or round
screen, so a hero region needs an **aspect** as well as a share: square for the clock, whatever the
page's cells are. Two consequences to design for:

- the region's size comes from the page - the largest square the page can give without starving the
  rest - and the hero card draws to it, the way the card window draws to its window (`pm()`);
- NINA's code comes with its author's permission for non-commercial use, and **that condition
  travels with anything taken from it** (`REFERENCE_PROJECTS.md`).

The clock is a side project that waits for this region; it is not part of 2.11's scope unless the
owner says so.

## Orbital

A page arranged around a centre - the room as one large card, its devices around it, heaviest
nearest: positions by distance from the centre cell, ring by ring, clockwise from the top. For the
large panels; on a grid too small for a ring it falls back to flow.
