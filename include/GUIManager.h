#pragma once
//
// GUIManager - screen content: the dashboard, root screen, the two decks, the
// z-order sandwich, the header and the panels. The LVGL engine (buffers,
// lv_init(), driver registration, the flush and touch callbacks) is
// LVGL_Startup's.
//
// Hardware is owned by SystemCore, not here. GUIManager takes a reference and
// reads from it; it never brings anything up.
//
#include <lvgl.h>
// Pulls in the active board's identity macro (WS_P4_5, CYD_S3_3248, ...),
// which the per-board default grid below branches on. Included HERE rather
// than relied upon from the .cpp on purpose: GUIManager.cpp includes
// "GUIManager.h" first and "bsp_loader.h" eight lines later, so a board macro
// tested in this header would be undefined at the point it is tested, quietly
// take the fallback branch, and compile without a word. Same reason
// ConnectivityDefaults.h includes it directly.
#include "bsp_loader.h"
#include "SystemCore.h"
#include "Cards/CardBinder.h"
#include "Cards/CardPage.h"
#include "Cards/StateCard.h"   // StateCardFill, for the #50 Fill control
#include "Cards/PageSpec.h"    // the pages
#include "UI/UIToolkit.h"       // SYSTEM_HEADER_FULL_H
#include "UI/Panel_Header.h"
#include "UI/Kit.h"               // Kit::Drawer, a slot's panel
#include "UI/Panel_Display.h"
#include "UI/Panel_System.h"
#ifdef HAS_AUDIO_HW
#include "UI/Panel_Audio.h"
#endif

namespace EditCards { struct Item; }   // Edit cards' rows (UI/EditCards.h)

class GUIManager {
public:
    explicit GUIManager(SystemCore &core);

    // Builds the dashboard. Call after LVGL_Startup::begin().
    void begin();

    // Per-frame UI work. Does NOT call lv_timer_handler() - that is
    // LVGL_Startup::tick()'s job, and keeping them separate is what lets a
    // future LVGL task own the engine without touching this class.
    void tick();

    // Tear the dashboard down and build it again from the spec. Needed
    // whenever the GRID moves rather than the values on it - a card resolves
    // compact-vs-full from its cell height at build time, so a re-layout is
    // not enough. The knobs on the System panel and the return from the card
    // bench both come through here.
    void rebuildDashboard();

    // The grid knobs: demand a column / row count. Each wraps through AUTO, so
    // there is always a way back to the derived layout. Auto starts from
    // whatever the grid is currently showing, so the first press moves by one
    // rather than jumping. Each press PINS that count for the page - the
    // arranger keeps it and chooses the rest - and gridAuto() takes both pins
    // away.
    void nudgeColumns(int8_t steps);
    void nudgeRows(int8_t steps);
    void gridAuto();
    void toggleDeck();

    // LVGL's FPS / CPU overlay, bottom-right. Swipe up from the bottom-right
    // to toggle; the bottom-left keeps the deck. Hidden at boot. The CPU
    // figure is LVGL's own load, not the chip's - see lv_conf.h.
    void togglePerf();

    // --- UI TWEAKS (D-22, D-25; GUIManagerTweaks.cpp) ------------------------
    // The window of the card looks, every row a choice made outright and
    // SAVED (Settings system.ui.*, the glyphs system.header.glyphs, the
    // timeout system.window.*), for EVERY page: a choice is device-wide (the
    // per-page working set below takes it). Reset all puts them back, except
    // the scheme and the grid, which are the system panel's. The deck rises
    // under it while it is open.
    void openUiTweaks();
    void chooseScheme(uint8_t i, bool save = true);   // 0 Fleet, 1 Midnight, 2 Linen (save: a PC's look is not)
    void setCardLabelStyle(CardHeaderStyle s);    // Float / Band / Tag / None
    void setEntityLabel(CardLabel l);             // Name (HA) / State / Custom / None
    // Fill / Icon glow / Bracket / Corner cut; `save`
    // false: the harness's look, not the owner's choice.
    void setCardState(StateCardFill f, bool save = true);
    static StateCardFill savedCardState();
    void setDensity(CardVariant v);               // Compact / Full / Auto
    void setGlyphs(GlyphStyle g);
    void resetUiTweaks();
    // The saved choices onto the working set and every page's: at boot (before
    // the pages are made), after a reset.
    void applyUiSettings();

    // Open the design-token reference page, standing the dashboard down first.
    void openTokens();
    void openLog();      // the System Doctor report, on its own screen


    bool deckShown() const { return _showDeck; }

    // --- Pages. docs/design/pages.md. ---------------------------------------
    //
    // THE ONE WAY TO CHANGE PAGE. A swipe, and later a card link, a gesture
    // target or the overview, all ask here; nothing else rebuilds a different
    // page. One owner of page changes (NINA's navigation arbiter) means two
    // sources can never fight over which page is showing.
    //
    // `index` is a position in the swipe order, and it WRAPS, so that with many
    // pages there is no swiping all the way back to reach page 1.
    void goToPage(int16_t index);
    void nextPage() { goToPage((int16_t)_curPage + 1); }
    void prevPage() { goToPage((int16_t)_curPage - 1); }

    // Read-only views for GET /bench (UI/Bench.cpp), which needs to know
    // where it is, put things back afterwards, and find one card to redraw.
    uint8_t     pageCount() const   { return _nPages; }
    uint8_t     currentPage() const { return _curPage; }
    const char *currentPageSlug() const {
        return _curPage < _nPages && _pages[_curPage].spec ? _pages[_curPage].spec->slug : "";
    }
    // The first visible card on the page, or nullptr on an empty page.
    lv_obj_t   *firstCard() const;
    // The deck's accordion panels in on-screen order (Audio, Display), or
    // nullptr past the last one. For /bench?what=anim, which opens them the
    // way a tap does, by clicking their headers.
    lv_obj_t   *deckPanel(uint8_t i) const {
        return _deck && i < lv_obj_get_child_count(_deck) ? lv_obj_get_child(_deck, i) : nullptr;
    }

    Panel_Header &header()      { return _header; }
    Panel_System &systemPanel() { return _pnlSystem; }
    CardBinder   &cards()       { return _binder; }

private:
    void applyGround();              // the screen background, per scheme
    void buildDashboard();           // creates _dashHost and _page
    void destroyDashboard();

    static void closeSystemPanelCb();

    // THE HEADER'S SLOTS. A tap on a slot opens its panel - the status
    // slot's is the system panel, the others' a Kit::Drawer built from
    // SlotPanels - and a second tap, a tap outside, a swipe up or a page change
    // puts it away. One panel at a time.
    void onSlotTap(uint8_t slot);
    void openSlotPanel(uint8_t slot);
    void closeSlotPanel(bool animate);
    // The page's hidden cards, each with Restore. No button opens it now (Edit
    // cards has the Hidden list, K148); only /popup?restore=-2 reaches it.
    void openHiddenSheet();
    uint8_t hiddenCount();    // ...how many: from the page and from its clusters
    // CARDS' Edit cards (K148): the page's cards, its hidden ones and
    // HA's; its two lists read from here (`gui` is this).
    void openEditCards();
    static uint8_t editFillPage(EditCards::Item *out, uint8_t cap, void *gui);
    static uint8_t editFillHidden(EditCards::Item *out, uint8_t cap, void *gui);
    void syncScrim();                // the dismiss scrim catches taps while any panel is open
    // The page's header, shown or hidden (D-14): the owner's choice for the
    // page (Settings pages.<slug>.header), else the page's compiled default.
    bool pageHeaderShown() const;
    void setPageHeaderShown(bool shown);   // saves it, applies it, rebuilds
    void applyHeaderHidden(bool hidden);   // the bar and the drawers' top
    static GlyphStyle savedGlyphStyle();   // the owner's, else GLYPH_DEFAULT
    // /popup's slot=, glyphs=, pagehdr= and alarm= (DEBUG_POPUP): one line saying what it did.
    const char *headerDebug(const char *slot, int glyphs, int pageHdr, int alarm, int alarmIn);
    // The system panel's CONNECTIVITY lines: Wi-Fi, Home Assistant, MQTT.
    void connectivityWords(char *wifi, char *ha, char *mqtt, size_t n);
    // UI Tweaks' halves (GUIManagerTweaks.cpp): the owner's scheme, before
    // anything is built; this page's grid counts, saved with the page; a choice
    // put on every page's working set. GUITweaks is its rows and the deck that
    // rises under its window.
    void applySavedScheme();
    void saveGrid();
    // The development Reset: the dashboard and its clusters back to the compiled
    // ones, the looks and pins kept; its line's words. The system panel has no
    // Reset button (K187, K192); only /popup?dash=2 runs it.
    void resetDashboard();
    void dashboardText(char *brief, size_t nb, char *detail, size_t nd);
    template <typename F> void everyPage(F f) { for (uint8_t i = 0; i < _nPages; i++) f(_pages[i]); }
    friend struct GUITweaks;
    bool        _deckRaised   = false;   // the deck is up under UI Tweaks
    // When a header panel was last touched or opened: it closes on the window
    // timeout as a window does (D-22).
    uint32_t    _panelTouchMs = 0;
    Kit::Drawer _slotDrawer;
    int8_t      _slotPanel = -1;     // the slot whose drawer is open, or -1
    // A slot's tab stays open until its drawer has finished closing,
    // and the header stays visible while any header panel is open (D-30) - a
    // Show / Hide chosen meanwhile waits for them to close.
    int8_t      _closingSlot   = -1; // the slot whose drawer is closing, or -1
    bool        _headerPending = false;
    void slotDrawerClosed();
    void headerPanelsClosed();
    static void reportUiSection();   // [UI STATE] in the System Doctor
    // Registered on the SCREEN so a scrollable child still wins its own drag -
    // see the definition.
    static void screenGestureCb(lv_event_t *e);
    static void screenPressCb(lv_event_t *e);

    // The hidden-header peek. One swipe shows the bar for a few seconds
    // so the status glyphs can be read; a second opens the drawer. Drawn over
    // the page on the top layer rather than rebuilt into it, so a glance never
    // re-plans the grid.
    // Where the drawer hangs from, computed in ONE place so that a peeked
    // header is always accounted for.
    void syncPanelOffset();
    void toggleSystemPanel();

    void peekHeader();
    void unpeekHeader();

    // How tall a peeking header is, and how long it stays. The height is its
    // own constant rather than systemHeaderH because that is 0 in this mode -
    // "hidden" is exactly the state we are temporarily undoing.
    static constexpr uint8_t  UI_HEADER_PEEK_H  = 35;      // logical px
    static constexpr uint32_t UI_HEADER_PEEK_MS = 3000;    // the owner's 3 seconds

    SystemCore   &_core;

    // The one registry-to-UI pump for the whole device. Owned here rather than
    // by a page because pages come and go and it must not: every page shares
    // this single lv_timer. See CardBinder.h.
    CardBinder    _binder;

    // The dashboard: a host container sized to what is left between the
    // header and the deck, and the page of cards inside it.
    lv_obj_t     *_dashHost = nullptr;
    CardPage     *_page     = nullptr;

    // Whether the Audio/Display accordion deck is on the dashboard at all.
    // It costs a row of cards on every board, which is a real trade rather
    // than a preference - see buildDashboard().
    bool          _showDeck = false;    // the cards get the height instead
    bool          _showPerf = false;    // the FPS/CPU overlay

    // Overrides PageSpec::headerDefault. The spec is const; this is the live
    // choice laid over it in buildDashboard().
    // The looks a fresh flash starts from; UI Tweaks' saved choices replace
    // them (applyUiSettings()).
    // A floating card label by default.
    CardHeaderStyle _hdr    = CardHeaderStyle::HDR_TAG_FLOAT;
    // Compact by default: without the "Seen" line the icons, disc and text
    // all breathe. Auto and Full are one choice away.
    CardVariant     _variant = CardVariant::VAR_COMPACT;
    // The corner cut is the default card state (D-33).
    StateCardFill   _fill    = StateCardFill::FRAME_CORNER;
    bool            _showArea = true;

    // PER-BOARD DEFAULT GRID (D-6), chosen on the glass by the owner.
    //
    // A board boots into ITS layout rather than into whatever the derivation
    // produces; the Col / Row buttons override it. These are EXACT COUNTS,
    // not hints, set through COLS_OVERRIDE and CardPage::setRowsOverride() the
    // way a knob sets them: TARGET_CARD_W decides columns by derivation, which
    // is the right default for a board with no pick here, but a count chosen
    // by a person must be a count (docs/LESSONS.md).
    //
    // Same #ifdef-on-the-board-identity-macro pattern as
    // ConnectivityDefaults.h and UITokens.cpp's TARGET_CARD_W; 0 means
    // "derive it". Boards with no entry here (CYD_S3_3248, WS_P4_7B,
    // CYD_P4_1060) derive their grid.
#if   defined(WS_P4_5)
    uint8_t         _colsOverride = 5;
    uint8_t         _rowsOverride = 3;
// 4 x 3 on the 4B panels: each cell is wider than tall, so the icon disc,
// which is sized from the cell, stays round.
#elif defined(WS_P4_4B) || defined(WS_S3_4B)
    uint8_t         _colsOverride = 4;
    uint8_t         _rowsOverride = 3;
// Portrait 2 x 5 whatever the number of cards; landscape 4 x 3. Chosen by the
// BSP's ROTATION, since this board runs both ways.
#elif defined(CYD_P4_4880)
    uint8_t         _colsOverride = (bsp_display.ROTATION & 1) ? 4 : 2;
    uint8_t         _rowsOverride = (bsp_display.ROTATION & 1) ? 3 : 5;
#else
    uint8_t         _colsOverride = 0;      // 0 = derive from TARGET_CARD_W
    uint8_t         _rowsOverride = 0;      // 0 = let the cards decide
#endif
    // THE PAGE'S PINS: the counts the owner chose with Col / Row, which the
    // arranger keeps; 0 = not pinned (it chooses, starting from the board's
    // grid above). A pinned count is also the flow's override.
    uint8_t         _pinCols = 0, _pinRows = 0;
    // WHAT EACH PAGE REMEMBERS. The fields above (_hdr, _variant, _fill,
    // _showArea, _colsOverride, _rowsOverride) are the CURRENT page's working
    // set, which is what every knob edits. Changing page stores them here and
    // loads the next page's. UI Tweaks' choices are device-wide and are put on
    // every page's set; the grid counts are per page and saved with it. The
    // working sets live in RAM; the saved choices are in the settings file.
    struct PageLive {
        const PageSpec *spec;
        CardHeaderStyle hdr;
        CardVariant     variant;
        StateCardFill   fill;
        CardLabel       label;
        bool            showArea;
        uint8_t         cols;
        uint8_t         rows;
        uint8_t         scheme;     // UI::schemeIndex(); the same on every page (D-29)
        uint8_t         pinCols;    // the counts the owner pinned; 0 = the arranger's
        uint8_t         pinRows;
    };
    static constexpr uint8_t GUI_MAX_PAGES = 8;
    PageLive  _pages[GUI_MAX_PAGES] = {};
    uint8_t   _nPages  = 0;
    uint8_t   _curPage = 0;

    void initPages();                // the swipe order, with each page's defaults
    void savePageState();            // working set -> _pages[_curPage]
    void loadPageState();            // _pages[_curPage] -> working set, applied
    void refreshKnobLabels();        // every drawer label from the working set

    lv_obj_t       *_hiddenBarTap = nullptr;
    lv_obj_t       *_dismissScrim = nullptr;
    lv_obj_t       *_deckScrim    = nullptr;   // the deck's tap-away sheet
    // The deck's height with every panel COLLAPSED, measured on the last
    // build that had none open. See buildDashboard().
    int32_t         _deckReserveCollapsed = 0;
    // Where the current press began. lv_indev_get_point() gives the CURRENT
    // point, which for a completed swipe is the far end of it - useless for
    // asking which edge it started from.
    lv_point_t      _pressStart = {0, 0};
    // How close to an edge a swipe must begin, in LOGICAL px, so it is the
    // same physical band on every panel.
    static constexpr int32_t UI_EDGE_BAND = 40;
    bool            _headerPeeking = false;
    bool            _headerHidden  = false;
    // THE system header height: one size; the header can be hidden or shown.
    static constexpr uint8_t UI_HEADER_H = UIToolkit::SYSTEM_HEADER_FULL_H;
    lv_timer_t     *_peekTimer     = nullptr;
    lv_obj_t     *_deck     = nullptr;

    Panel_Header  _header;
    Panel_System  _pnlSystem;
    Panel_Display _pnlDisplay;
#ifdef HAS_AUDIO_HW
    Panel_Audio   _pnlAudio;
#endif
};
