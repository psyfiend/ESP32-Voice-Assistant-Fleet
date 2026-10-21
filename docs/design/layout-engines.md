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

### Prototyped next: Layout Playground version 6 (2026-10-19, round 4) - for the owner to try

The owner's round-4 notes: a tile's icon must be a solo card's size and look; clusters keep their
whole cell (labels are for solo cards); clusters need more of the page than a 1x1 cell, "a cluster
deserves 1x the proportional cell space, whereas a solo card should occupy .75x"; tiles and cards of
the same shape. Version 6, in the same artifact (v5 and v4 still selectable beside it):

- **Sub-units**: the page is a finer grid. A solo card covers CARD x CARD sub-units and a tile
  TILE x TILE, two knobs. A cluster of tc x tr tiles is a (TILE*tc) x (TILE*tr) block, or one
  sub-unit wider or taller to close a gap; a card never changes size. Clusters wear no label and no
  header (a Cluster header switch puts one back), so a card pays for its label inside its own square.
- **Tiles drawn as cards are** - disc, glyph and name at a card's proportions - clear until on, then
  filled (the Fill card state); or bare (no ring), the glyph taking the disc's room.
- **Cost**: a tile icon a different size from a card's, `40 x 100 x log(ratio)^2`; a tile shaped
  unlike a card, `20 x 100 x log(ratio)^2`; empty sub-units at the empty-cell price per card's worth;
  long clusters, order and leaving the board's grid as v5; an empty tile slot 8.
- **Grids**: every sub-unit grid with a solo card between the smallest and biggest size and within
  two cards across or down of the board's own; the cheapest first, and a grid that cannot beat the
  sixth-best page is skipped. Rows are bit masks; 25,000 steps per grid. ~100 ms for the P4_5 in the
  browser.

What it showed on the House page (16 entities, by area, no header): **tiles a third bigger than
cards (4:3) fit nowhere but the 7-inch board** - a tile bigger than a card needs more page than the
same entity as a card would. **Equal squares (3:3)** fit five of the seven boards with no empty
sub-unit, tile icons within 7% of card icons - a card's label costs about what a cluster's padding
does. **Bare tile icons at 2:3** fit all seven, and every icon on the page grows (P4_5: a 3.9 mm
glyph against 3.4 mm). The owner decides from the playground before the C++ arranger changes.

### Then version 7 (2026-10-19, round 5): units

The owner's reminder: the device already divides a cell into units (`dashboard.md` §2, 2 x 2 - a
quarter-page card on a 3 x 3 page, the Fleet page's 1.5-cell cards), and multiples of cells are the
same idea. v6's "sub-units" are now **cells** in the playground, and **units** are a cell's parts.
The arranger still places blocks on cells; a **finish** then moves each **solo card** within its room,
in units, and never touches a cluster:

- **Nearer square**: a card taller than it is wide (as seen, its label paid) gives up units at its top
  while that brings it nearer square and keeps it at least 0.85 as tall as wide; its foot stays, so
  its hero sits lower, nearer the tiles of a cluster beside it. Cards much wider than tall give up
  units in pairs, one each side.
- **Units down a cell**: 2, 3, 4, or **Auto** - the fewest that gets every card on the page near
  square ("if 1 unit would make them too short and flat, increase the vertical multiplier"). Across
  stays 2.
- **Even margins**: whole unit rows free above or below a card, across its width, are shared out -
  the card centred, the odd unit below. **Up and down only**: centred across a free row, a card
  wandered away from its neighbours (the 4B).
- **A unit's step is the cell's pitch over the units**, so spans join exactly as the device's unit
  tracks do (a gap between every track, dimensionally transparent).

Checked off the page: the owner's 15 x 8 example (P4_5, cluster headers) moves Overhead and Thermo
down one unit each; the device's 5 x 2 House page trims each 1 x 1 card one unit at 4 units down
(228 x 292 px to 228 x 213), where 2 down would leave it too flat. **Why the icons shrink** with more
rows (owner, round 5): a card's hero is sized from its height, and tiles copy the 1 x 1 card's hero
(round 4), so short wide cards make every icon on the page small; squarer cards are the fix that keeps
tiles and cards matching. The port waits for the owner's answer.

### Then version 8 (2026-10-20, round 6): the owner's rules, in units - K109

The owner's night notes (TEST_2.11 round 5, "Your results") set the rules and the limits, and left
the logic to Claude. **Every block is placed on a grid of units** - the device's own, a cell's halves
- in any shape its limits allow, so a cluster can be 2.5 cells wide and a row 1.5 cells tall. A
block's shapes:

- **A solo card**: any w x h units with at least the area of one card of the P4_5 at 7 x 3
  (13.5 x 16.2 mm in the playground) and no taller for its width (1.20), at most 1.7 as wide as tall.
- **A cluster**: any w x h units, its tiles in the arrangement (tc across x tr down, no empty row)
  that costs least, each tile at least one of a 3 x 1 cluster on the 4B at 4 x 4 (15.3 x 6.7 mm, its
  header paid). Clusters wear the device's header and padding (UIShape C_HDR_H, C_PAD, C_TILE_GAP).

**The owner's knobs** (2026-10-20, tuned in the playground, now its defaults): `size` 75, `square`
30, `wide` 20, `twin` 70, `gap8` 11, `order` 4.

**Cost**: an entity's size is the side of the square with its area (a solo card after its label, a
tile after its cluster's header and padding). `size` x log(size / target)^2 per entity - the target
the page's equal share first, then the average the first pass found (two passes per grid); `square` x
4 x log(h/w)^2 per solo card and 0.4 x that per tile; `wide` x 2 x log(h/w) when a cluster is taller
than wide; `twin` x |log(area ratio)| between clusters with as many tiles (as each is placed); an empty
unit `gap8` per entity-sized area it wastes (6 - deliberately cheap: "I'd prefer small amounts of dead
space over grossly disproportionate units per tile"); `order` per block ahead of an earlier one. The
search is v5's (first empty unit, the next three blocks in each shape, or leave it), rows as bit
masks, 20,000 steps per pass. Grids: units across from 2 x (the board's columns - 1) to
2 x (its columns + 2), down 2 x (its rows -/+ 1). Solo cards then take spare unit rows above or below
as even margins (v7's finish, units of 1).

**On the owner's page** (P4_5): Kitchen 8 x 4 units across the top, Outside and Office 5 x 4 each
below - the owner's "what SHOULD happen" - and Doors 2 x 3 with a unit of margin above it; every
entity 16.9 to 18.8 mm. The 4B is bound by the tile limit (three tiles need three quarters of its
width); ten times the step budget found nothing better. ~200 ms and 600,000 steps in the browser on
the P4_5: the C++ port needs a budget, as v5's did. Waits for the owner's look.

### v8 as built (2.11 round 9, 2026-10-20)

- **`Layout::arrange()`** is a line-for-line port of `arrangeV8()` with its shapes, search, score,
  references and the finish (a unit a step) - doubles, the same loops, sums and tie-breaks. **Its
  host tests are the playground's answers** (`host-tests/test_arranger`, 19 tests): the device's
  House blocks and the playground's own House page on its seven boards, every grid, place, finished
  box, score and empty unit; the P4_5 pinned at 4 x 2 and 5 x 3; the 4B pinned at its 4 x 3, where
  no page holds the device's blocks. The budget is the playground's, 20,000 steps a pass.
- **Units on the page**: the arranger's grid is the page's unit grid (`CardPage::useUnits()`), the
  rows sharing the whole height; each block goes where its finished box says - a solo card in its
  even margins - and a cluster's tiles across (`tc`) are the arranger's (`Layout::tileGrid()`'s
  `cols`). The label it pays for is the page's first solo card's (Float, Tag as folder, Band, None).
- **Kept** as `pages.<slug>.layout` = `v2 <fingerprint> <C>x<R>` and `c,r,w,h,tc` a block; round
  2's `v1` answers are not read, so every page arranges once on the new firmware.
- **Pins**: the system panel's Col / Row pin that count for the page (saved as `cols` / `rows`);
  the arranger keeps a pinned count and searches the other, and **Auto** takes both pins away. The
  board's own grid (5 x 3 on the P4_5, 4 x 3 on the 4B) is where an unpinned search starts.
- **Timed on the boards** (the task's stack 24 KB, its high-water mark logged): the P4_5's House page
  pinned at 4 x 2, 627 steps in 19 ms; unpinned, 313,539 steps in 1.95 s (10 x 8 units, as the
  playground); its Fleet page (16 cards), 205,360 steps in 1.2 s. The 4B pinned at 4 rows, 0.8 s;
  unpinned 1.9 s. The 1060 pinned at 5 x 2, 76 ms; unpinned 3.2 s. At most 13 KB of stack used. All
  in the background (the page flows meanwhile, then is built again); `WS_S3_4B` compiles, untimed.

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
