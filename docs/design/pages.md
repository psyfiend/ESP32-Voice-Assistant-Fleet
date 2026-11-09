# Pages - model and navigation

How a page is defined, how the screen moves between pages, and what is not built. Companion to
`cards.md` (what is on a page), `dashboard.md` (the grid a page lays out on), `group-cards.md` and
`layout-engines.md` (groups and the arranger) and `ha-cards.md` §3.4-3.5 (the page panel's CARDS section:
Edit cards and New group).

## 1. What a page is

A `PageSpec` (`include/Cards/PageSpec.h`) is data, not code. It has a numeric `id` that is never
reused and a string `slug` that is the external key (the settings file keys a page's saved choices by
slug: `pages.<slug>.*`). See `REFERENCE_PROJECTS.md`, "the page/view/grid back-end".

Layout belongs to the page, not the device: sub-grid `subdivision`, header slots and whether the
header shows, label and variant defaults, temperature unit, the cluster header style. The column and
row counts are chosen by the arranger; a page's held count (`pages.<slug>.cols` / `rows`, set from
the system panel's Col / Row) is saved with it. The scheme is one for every page (`system.ui.scheme`).

The dashboard has two pages, in swipe order **House** (`HA_PAGE`) and **Fleet** (`FLEET_PAGE`),
listed in `DASH_PAGES` in `GUIManager.cpp`, on every board. Boot lands on House; the last page is
not remembered.

## 2. Overflow

A page never overflows onto another page: that would be unpredictable. It shows what fits and drops
the rest by priority, lowest first, and says so in the log. Edit cards lists the cards crowded off a
page as "No room" (K113). More cards on a page means a second page.

## 3. Navigation

- A horizontal swipe from anywhere changes the page, through `GUIManager::goToPage()`, which commits
  one change at a time. A swipe while a panel, the system panel or a peeking header is open closes
  that instead of turning the page.
- **Wrap-around** is on in both directions.
- The header's page slot shows the page's name and one dot per page, this page's larger and filled
  (`Panel_Header.cpp`). A toast with the page's title and position ("House  -  1 of 2", ASCII only)
  shows for 1.5 s after every change, whether or not the header is visible.
- A page is rebuilt on arrival: one page's cards are in LVGL's memory at a time (D-9).

### Edge gestures

Each starts in a band at the screen's edge (`GUIManager.cpp`):

| Gesture | Does |
|---|---|
| down from the top, right half | the system panel; with the header hidden, the first swipe peeks the header for three seconds and the second opens the panel |
| down from the top, left half | UI Tweaks |
| up from the bottom, right half | the FPS and CPU overlay |
| up from the bottom, left half | the legacy deck |
| up, anywhere, while a panel or a peeked header is open | puts it away |

## 4. Not built

- **Linked pages** (a page reached only by a card or a gesture, outside the swipe order, with a way
  back) and **group pages** (K99, K118). A group's members open in its group view instead (K154).
- **A table of gesture targets** configurable per page; the gestures above are code.
- **An overview of pages** ("alt-tab"). Only one page's widgets exist at a time, so thumbnails would
  have to be snapshots taken as a page is left (#58).
- **Transitions.** A page change is an instant swap. A fade of a page object forces a full-screen
  LVGL layer, the allocation that froze `WS_P4_5` (LESSONS); a slide of two live pages needs both
  pages' cards in memory at once; a slide of a snapshot needs the snapshot capability. LVGL's PPA
  draw unit (`LV_USE_PPA`, 0 in `lv_conf.h`) would accelerate image blends and transforms on the P4s.
