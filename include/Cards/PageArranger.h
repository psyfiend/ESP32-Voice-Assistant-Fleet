#pragma once
// ---------------------------------------------------------------------------
// PageArranger - the arranger (Layout/Arranger.h) for the dashboard's pages
// (2.11, #78, DECISIONS K105, K109).
//
// ARRANGED WHEN A PAGE'S BLOCKS CHANGE, AND KEPT (K105): a page is built often
// (on arrival, D-9), so the arranger is never run on a build. Its answer is kept
// in the settings file - pages.<slug>.layout: "v2 <fingerprint> <C>x<R>" and
// each block's box in units, "c,r,w,h,tc" (tc a cluster's tiles across), the
// fingerprint covering what it was arranged for (the blocks and their ids, the
// screen, the pins, the knobs) - and a build whose blocks match uses it. Round
// 2's "v1" answers (cells) are not read. When they do not (a first boot, a
// cluster made or hidden, a weight changed), the page is built as before (the
// flow engine) and the arranger runs on a task of its own; once its answer is
// kept, takeDone() says so and the page is built again with it. So the screen
// never waits for it, however long it takes - which is what K105 asks to time on
// the boards before the page relies on it.
//
// Its knobs are system.arranger.* in the settings file (size, square, wide,
// twin, gap8, order - Layout Playground v8's), the owner's tuned values when
// unset, so he can tune on glass from the PC (/settings?arranger=...).
//
// UI code; called on the LVGL thread. The arranger itself runs on its task and
// touches nothing but its own copy of the input.
// ---------------------------------------------------------------------------
#include <stddef.h>
#include <stdint.h>
#include "Layout/Arranger.h"

namespace PageArranger {

struct Input {
    const char        *slug = nullptr;
    Layout::ArrScreen  scr;
    Layout::ArrBlock   blocks[Layout::ARR_BLOCKS_MAX];
    const char        *ids[Layout::ARR_BLOCKS_MAX] = {};   // each block's card id (the fingerprint)
    uint8_t            n = 0;
};

// The kept answer for exactly these blocks on this screen, into `out`; false when
// there is none (or it was arranged for something else).
bool kept(const Input &in, Layout::ArrResult &out);

// Arrange in the background (one at a time; a request while one runs waits for
// the next build). Its answer is kept when it arrives.
void start(const Input &in);

// True once after an answer has been kept: the page should be built again.
bool takeDone();

// A page built without the arranger (it is switched off, or the page has too many
// blocks): said in lastText().
void flowed(const char *slug, const char *why);

// The last run, for the System Doctor and /popup: what was arranged, the grid,
// the steps and how long it took - or that the kept answer was used.
const char *lastText();

}  // namespace PageArranger
