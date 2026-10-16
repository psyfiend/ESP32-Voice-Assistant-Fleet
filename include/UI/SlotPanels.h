#pragma once
// ---------------------------------------------------------------------------
// THE SLOTS' PANELS (2.8, D-13): what slides down from a header slot when it is
// tapped. The status slot's is the system panel (Panel_System); these are the
// other kinds', built into a Kit::Drawer's face each time one opens (it is
// emptied when it closes). Each starts small - header-slots.md section 3 says
// what each may hold later, and each panel's last line says so on glass.
//
//   clock   the time and the date
//   page    the pages, to go to one; this page's header, shown or hidden (D-14)
//   sensor  its reading, when it last reported, or what went wrong
//   alarm   the next alarm and how long until it (on the bar only while set, D-20)
//
// GUIManager owns the drawer and what a choice does (`goToPage`,
// `setHeaderShown`); nothing here keeps state beyond the open panel.
// ---------------------------------------------------------------------------
#include <lvgl.h>
#include "UI/HeaderSlots.h"

class EntityRegistry;

namespace SlotPanels {

static constexpr uint8_t PAGES_MAX = 12;

struct Context {
    const char *pageTitles[PAGES_MAX] = {};
    uint8_t     pageCount   = 0;
    uint8_t     pageIndex   = 0;
    bool        headerShown = true;
    const char *entity      = nullptr;   // the sensor slot's
    EntityRegistry *reg     = nullptr;
    void *user = nullptr;                // handed back to the two below
    void (*goToPage)(void *user, uint8_t page)    = nullptr;
    void (*setHeaderShown)(void *user, bool shown) = nullptr;
};

// Build the panel for a slot of `kind` into `face`. False when that kind has no
// panel here (status: the system panel is its own).
bool build(SlotKind kind, lv_obj_t *face, const Context &c);

}  // namespace SlotPanels
