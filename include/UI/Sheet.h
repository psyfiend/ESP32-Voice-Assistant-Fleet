#pragma once
// ---------------------------------------------------------------------------
// THE SHEET (2.12 P4) - docs/design/components.md, "Sheet"
//
// A window over the page, from a spec: placed in the middle of the screen,
// with a header row (a close or back button, a title in two parts, tabs on the
// right), a stage for what it shows, a bar counting down to it closing itself,
// sparks now and then along its edge, and two folder panels under it -
// SETTINGS on the right, its view's own options on the left. Modal: a tap
// outside closes it, and while a panel is open a tap in the window only folds
// the panel. Built in one quiet pass, so its first frame draws only itself
// (card-sheet section 14, "Usable first").
//
// What a sheet shows is its client's - its views, which tabs are lit, its
// titles, its rows, what changes while it is open; the frame is the sheet's.
// The card window (CardPopup) is the first client; "About this board" (debug
// builds) the second. One sheet is open at a time.
// ---------------------------------------------------------------------------
#include "UI/Kit.h"

namespace Kit {

constexpr uint8_t  SHEET_TABS_MAX     = 2;
constexpr uint32_t SHEET_AUTOCLOSE_MS = 60000;   // D6: it closes after 60 s untouched - unless the
                                                 // system setting window.close says otherwise (T6)
constexpr uint32_t SHEET_TICK_MS      = 250;

// A panel: below the screen, its tab peeking up, or open.
enum PanelState : uint8_t { PANEL_HIDDEN, PANEL_PEEK, PANEL_OPEN };

struct SheetSpec {
    // --- The header row ---
    lv_event_cb_t leftCb;                    // the left button: close, or back
    lv_event_cb_t crumbCb;                   // a tap on the title's first part (null: not a link)
    uint8_t       nTabs;                     // icon buttons on the right, made in order: the last
    const char   *tabGlyph[SHEET_TABS_MAX];  //   made is the one in the corner
    lv_event_cb_t tabCb[SHEET_TABS_MAX];
    const char   *floatGlyph;                // a tab of its own under the right slot, hidden until
    lv_event_cb_t floatCb;                   //   shown (null: none)
    lv_event_cb_t headerLongCb;              // a long press on the header row (null: none)
    // --- Its life ---
    void        (*buildFirst)();             // the first view, into the stage, inside the quiet build
    void        (*tick)(uint32_t now);       // every SHEET_TICK_MS while open, after the auto-close
    void        (*closing)();                // the moment it starts to go
    void        (*closed)();                 // once it has gone
    bool          sparks;                    // now and then a spark runs along its edge
    // --- SETTINGS, the panel on the right (null rows: no panel) ---
    const char   *panelTitle;
    uint8_t     (*panelRows)(RowSpec *r);    // read for its size when made, for its rows when first opened
    void        (*panelGone)();              // its rows went: forget their controls
    // --- The panel on the left (sheetSide()) ---
    void        (*sideGone)();               // its rows went: forget their controls
};

// A folder panel's objects and where it stands (Kit::folder() builds it).
struct SheetPanel {
    lv_obj_t *root = nullptr, *tab = nullptr, *tabLbl = nullptr, *pane = nullptr, *body = nullptr;
    lv_obj_t *track = nullptr, *up = nullptr, *down = nullptr;
    int32_t   h = 0;
    uint8_t   state = PANEL_HIDDEN;
    bool      filled = false;                // its rows exist (SETTINGS builds them at first open)
    uint8_t   kind = 0;                      // the left panel: the client's name for it (0: none)
    uint32_t  sig = 0;                       // the rows it was made for (sheetRightSync())
    const char *title = nullptr;
    uint8_t (*rows)(RowSpec *r) = nullptr;
};

// A sheet's state, kept by its client while it is open (the card window's
// own state extends it). Every object here goes with the sheet.
struct Sheet {
    const SheetSpec *spec = nullptr;
    lv_area_t  winRect = {};
    int32_t    winRadius = 0;
    int32_t    pad = 0;
    int32_t    propH = 0;                    // the height its contents are sized from (sheetPlace())
    lv_obj_t  *win = nullptr;
    lv_obj_t  *btnLeft = nullptr, *lblLeft = nullptr;
    lv_obj_t  *tab[SHEET_TABS_MAX] = {nullptr, nullptr};
    lv_obj_t  *floatTab = nullptr;
    lv_obj_t  *lblTitleArea = nullptr, *lblTitleName = nullptr;
    int32_t    titleW = 0;                   // what the two title labels may use together, centred
    lv_obj_t  *titleLeft = nullptr;          // the left button's slot: as wide as the right one
    int32_t    titleSlotW = 0, titleSlack = 0;   // its width, and the room in it beside the button
    lv_obj_t  *stage = nullptr, *autoBar = nullptr;
    lv_timer_t *timer = nullptr;
    uint32_t   lastTouchMs = 0;
    uint32_t   closeMs = SHEET_AUTOCLOSE_MS;   // untouched this long, it closes (0: never) - read at open
    lv_point_t pressStart = {0, 0};
    bool       closeQueued = false;
    // The panels: SETTINGS on the right, the view's own on the left; the strip
    // a folded tab shows, and how much of an open pane stays below the screen.
    SheetPanel right, left;
    int32_t    panelHead = 0, panelHide = 0;
};

// --- The sheet ------------------------------------------------------------------
void   sheetBegin();                         // once, at boot: the tap catcher and the touch hook
void   sheetPlace(Sheet &S);                 // where it goes: winRect, propH, winRadius, pad
void   sheetOpen(Sheet &S, const SheetSpec &spec);   // spec: kept until it closes
void   sheetClose(Sheet &S);                 // from the next timer pass, never inside an event
Sheet *sheetShowing();                       // the open sheet, or null
void   sheetLayoutTitle(Sheet &S);           // after the title's words change
void   sheetTabLook(lv_obj_t *tab, bool shown, bool lit);   // lit: the tab of the view showing
uint32_t sheetLeftMs(const Sheet &S);        // until it closes itself (0: closing; UINT32_MAX: never)

// --- The countdown bar (#97, #23) ----------------------------------------------------
// How long until the open sheet closes itself: a thin bar (H_BAR) on the bottom
// edge of `box` - inside its border, between its rounded corners - shrinking
// toward its middle. The sheet's window has one (S.autoBar; null when the system
// settings hide it, or the window never closes itself - T6); a dialog over the
// sheet shows the same. `boxW`: the box's width as set, before LVGL has laid it out.
lv_obj_t *countdownBar(lv_obj_t *box, int32_t boxW);
void      countdownSet(lv_obj_t *bar, const Sheet &S);   // as long as S has left to wait

// --- The panels (KitSheetPanels.cpp) -------------------------------------------------
bool   sheetPanelOpen(const Sheet &S);       // either open: the window is out of reach
bool   sheetInPanel(const Sheet &S, const lv_point_t &pt);   // on a tab or a pane, as they stand
void   sheetRightSet(Sheet &S, uint8_t state);
void   sheetRightOpen(Sheet &S);             // its rows built the first time
void   sheetRightSync(Sheet &S);             // made again in place when its rows are not the ones it has
void   sheetSide(Sheet &S, uint8_t kind, const char *title, uint8_t (*rows)(RowSpec *r));   // made, peeking
void   sheetSideSet(Sheet &S, uint8_t state);
void   sheetSideFold(Sheet &S);
void   sheetSideEnd(Sheet &S);

// --- The sparks (KitSparks.cpp): now and then, once per sheet, or never ------------
enum class SparksMode : uint8_t { SPARKS_NOW_AND_THEN, SPARKS_ONCE, SPARKS_OFF };
SparksMode sparksMode();
void       setSparksMode(SparksMode m);      // from the next sheet

} // namespace Kit
