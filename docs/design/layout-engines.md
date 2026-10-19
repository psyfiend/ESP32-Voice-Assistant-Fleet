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

### As prototyped: Layout Playground version 5 (2026-10-19, #78) - a proposal, not yet tuned

The playground's arranger v4 was rejected by the owner on use: it missed layouts that existed, made
one cluster huge, and stretched others into long rows. Its code says why - first-fit packing in
reading order, five shapes per cluster, a stop after 4,000 tries, and spare cells handed out after
packing, each cluster in turn grabbing what is beside it. Version 5, in the artifact
(`https://claude.ai/artifact/SBSanCnwviNbKzQmkwiJXp`), is the candidate for the C++ port:

- **Grids**: the board's own (the owner's pick on glass) and its neighbours, one row or column
  either way, each with cells between the smallest and biggest allowed size.
- **Shapes**: every rectangle that holds a cluster's tiles at the smallest tile size; one cell only
  for a cluster with nothing in its header (K97). A shape is dropped when a smaller shape inside it
  plus empty cells for the difference costs no more.
- **Search**: fill the FIRST EMPTY CELL in reading order, with one of the next three unplaced blocks
  in each of its shapes, or leave it empty - every tiling, blocks kept near their list order. Branch
  and bound: a branch stops once it costs more than the best page so far, counting each block left
  at its cheapest shape and the cells that must stay empty.
- **Cost** (each a knob): an empty cell 60 (the owner's, K105 - 40 when first published); a cluster's room beyond comfortable (1.6x the smallest
  tiles' area) charged by its square, 25 x excess^2 - so spare room spreads over clusters; a long
  cluster (beyond 1.8 : 1) 40 x the log excess squared; a block placed ahead of earlier ones 4 each;
  each row or column away from the board's grid 6; single cards 10 x how far the cell drifts from
  today's ~20 mm card.
- **Deterministic**; no stability term yet (each page is arranged afresh).
- **Cost to compute: the open problem for the port.** In the browser the P4_5's House page takes
  ~300,000 steps (~45 ms) - **on the owner's PC, in JavaScript**; C++ on the P4 is expected to be
  several times slower, an S3 slower again, so hundreds of milliseconds are possible until it is
  measured. A page with much spare room hits the 150,000-step cap per grid. #78's PASS is under 5 ms
  on an S3. **Decided (K105)**: build it as prototyped; arrange when a page's blocks change and keep
  the result (by card id, as above), never on every page build (D-9 rebuilds pages on arrival); time
  it on the P4_5 and an S3 first. Then, if needed: a time budget, a stronger bound, fewer shapes.
- **Tuning happens on glass** (the owner: "this is going to require a lot of experimental tuning on
  glass"): the weights are adjustable from the PC, and the playground stays the place to see what
  each one does.

### As built (2.11 round 2, 2026-10-19)

- **`Layout::arrange()`** (`include/Layout/Arranger.h`, `src/Layout/Arranger.cpp`): a line-for-line
  port of the playground's `arrangeV5()` - doubles, the same loops and tie-breaks. **Its host tests
  are the playground's own answers** (`host-tests/test_arranger`: the playground's House blocks on
  its seven boards, run with node on its code): every grid, place and score matches.
- **Run when a page's blocks change, and kept** (`Cards/PageArranger`): a page's blocks are its
  cards and clusters (a cluster's tiles, header members and members). Its answer is kept in the
  settings file - `pages.<slug>.layout`: `v1 <fingerprint> <cols>x<rows>` and each block's
  `c,r,w,h,one` - the fingerprint covering the blocks and their card ids, the page's room, the
  weights and sizes. A build whose fingerprint matches places every block from it (pinned, on its
  grid); one that does not flows as before and starts the arranger **on a task of its own**
  (priority 1, core 0), and the page is built again once the answer is kept. The screen never waits.
- **Timed** (K105): the P4_5's House page (9 blocks: two door cards, three clusters, four cards) -
  **150,031 steps in 414 ms** (one neighbouring grid hits the 150,000-step cap; 5 x 3, no empty
  cell); the 4B's - **57 steps, 1 ms** (4 x 3). No S3 is on the desk to time (the S3_4B is locked,
  #69); it compiles. A page the arranger finds no page for flows (the Fleet page's 16 cards, at the
  playground's cell sizes), and is not tried again until its blocks or the weights change; so do
  pages of more than 24 blocks.
- **The page's grid**: the board's own (`UI::grid()`) and its neighbours; the page's own columns or
  rows when set in UI Tweaks (obeyed, the other the board's). A cluster always counts one header
  member, so it is never given a single headerless cell (K103's one-cell clusters are round 4's).
- **Tuning from the PC**: `/settings?arranger=<hole|comfy|loose|long|order|away|maxcell|mincell|
  tilew|tileh|hdr>&v=<number>` (empty: the playground's default) - the next build arranges again.
  What the last run did: the System Doctor's page report, and `/popup`'s first lines.

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
