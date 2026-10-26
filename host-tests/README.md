# Host tests (2.12 P5)

The firmware's pure modules - code with no LVGL, Arduino or ESP-IDF in it - tested on the PC, in
seconds, off the board:

| Suite | What it pins down |
|---|---|
| `test/test_layout` | `Layout::FlowEngine`, the dashboard's placement: reading order, the widest span that fits at the first free spot, pins first, a bad pin flowed and reported, the lowest priority dropped (not the item that failed), clamping, absent items, `rowsWanted()` |
| `test/test_timeformat` | `TimeFormat`: the clock, durations, "2+ hours ago", day headings - the owner's wording |
| `test/test_arranger` | `Layout::arrange()` / `finishPage()` (2.11): **generated** - every case's expected places, boxes, rectangles and scores are the Layout Playground's own answers (below) |
| `test/test_cluster`, `test_header_plan`, `test_glyph_shapes` | a cluster's span and tile grid; the header's slot planner; the status glyphs' shapes |

Run them before a board build whenever `src/Layout/` or `src/TimeFormat.cpp` changes.

## The arranger's tests come from the playground

`tools/playground.html` is the Layout Playground artifact's page (the arranger's spec; the C++ is its
line-for-line port). `tools/run_playground.js` runs the page's own arranger in node and prints its
answers; `tools/gen_arranger_tests.py` turns them into `test/test_arranger/test_main.cpp`. When the
arranger changes: change the playground first (and republish the artifact), copy its page here,
regenerate, port the change to `src/Layout/Arranger.cpp`, and run the suites - the port is right when
they pass. The command is at the top of `gen_arranger_tests.py`.

## Running them

With gcc or clang on the PATH, from the repo root:

    pio test -d host-tests

It is its own PlatformIO project (`platformio.ini` here, sources from `../src`), so the firmware's
nine environments and the release gate never need a host compiler.

**This PC has no Windows compiler** (2026-10-13: only the ESP cross compilers), so that command
fetches Unity and then stops at "'g++' is not recognized". Until one is installed, WSL's Ubuntu has
g++, and the same suites run there:

    wsl bash host-tests/run_in_wsl.sh

(after `pio test -d host-tests` has run once, which fetches Unity). On 2026-10-13 it ran all 17
tests, passing, with no warnings; and three deliberate breakages of the engine - ties dropping the
earlier item, pins not placed first, the narrowest span instead of the widest - each failed the test
written for that rule.

A compiler on Windows is the owner's choice, since it is an install on the PC: MSYS2's
`mingw-w64-ucrt-x86_64-gcc`, or WinLibs (a zip), with its `bin` on the PATH. On MinGW,
`localtime_r` (used by TimeFormat) needs `-D_POSIX_THREAD_SAFE_FUNCTIONS` in `build_flags` - not
yet tried here.

## Adding a suite

A folder `test/test_<name>/` with a `test_main.cpp` (`setUp`, `tearDown`, `main` running
`RUN_TEST`s). A module under test must stay pure - no LVGL, Arduino or ESP-IDF header - and be
listed in `build_src_filter` here and in `SRC` in `run_in_wsl.sh`.
