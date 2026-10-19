#pragma once
// ---------------------------------------------------------------------------
// THE SLOTS' PANELS (2.8, D-13): what slides down from a header slot when it is
// tapped. The status slot's is the system panel (Panel_System); these are the
// other kinds', built into a Kit::Drawer's face each time one opens (it is
// emptied when it closes). Each starts small - header-slots.md section 3 says
// what each may hold later, and each panel's last line says so on glass.
//
//   clock   the time and the date; the next alarm and the warning time (D-21)
//   page    the pages, to go to one; this page's header, shown or hidden (D-14);
//           this page's hidden cards, each with Restore (2.11, K66, K96)
//   sensor  its reading, when it last reported, or what went wrong
//   alarm   the alarm's notice: when, Dismiss, Settings (the clock's panel)
//
// GUIManager owns the drawer and what a choice does (`goToPage`,
// `setHeaderShown`); nothing here keeps state beyond the open panel.
// ---------------------------------------------------------------------------
#include <lvgl.h>
#include "UI/HeaderSlots.h"

class EntityRegistry;

namespace SlotPanels {

static constexpr uint8_t PAGES_MAX  = 12;
static constexpr uint8_t HIDDEN_MAX = 12;   // this page's hidden cards listed, at most

struct Context {
    const char *pageTitles[PAGES_MAX] = {};
    uint8_t     pageCount   = 0;
    uint8_t     pageIndex   = 0;
    bool        headerShown = true;
    const char *hiddenNames[HIDDEN_MAX] = {};   // this page's pool (K66, K96), by name
    uint8_t     hiddenCount = 0;
    void (*restoreCard)(void *user, uint8_t i) = nullptr;   // hiddenNames[i] back on the page
    const char *entity      = nullptr;   // the sensor slot's
    EntityRegistry *reg     = nullptr;
    void *user = nullptr;                // handed back to those below
    void (*goToPage)(void *user, uint8_t page)    = nullptr;
    void (*setHeaderShown)(void *user, bool shown) = nullptr;
    void (*dismissWarning)(void *user) = nullptr;   // the alarm's notice: dismiss it
    void (*openClock)(void *user)      = nullptr;   // ...or open the clock's panel
    void (*refreshHeader)(void *user)  = nullptr;   // a setting the bar shows has changed
};

// Build the panel for a slot of `kind` into `face`. False when that kind has no
// panel here (status: the system panel is its own).
bool build(SlotKind kind, lv_obj_t *face, const Context &c);

#ifdef DEBUG_POPUP
// /popup?restore=N: as a tap on Restore beside the page panel's Nth hidden
// card. False when the page's panel has not been built with one.
bool restoreDebug(uint8_t i);
#endif

}  // namespace SlotPanels
