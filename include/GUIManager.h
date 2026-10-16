#pragma once
//
// GUIManager - screen content. The name finally matches the file.
//
// Everything this class used to hold - buffer allocation, lv_init(), driver
// registration, the flush and touch callbacks - now lives in LVGL_Startup.
// What is left is the actual dashboard: root screen, the two decks, the
// z-order sandwich, the header and the panels.
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
#include "Cards/PageSpec.h"    // the pages, 2.6
#include "UI/UIToolkit.h"       // SYSTEM_HEADER_FULL_H
#include "UI/Panel_Header.h"
#include "UI/Kit.h"               // Kit::Drawer, a slot's panel (2.8)
#include "UI/Panel_Display.h"
#include "UI/Panel_System.h"
#ifdef HAS_AUDIO_HW
#include "UI/Panel_Audio.h"
#endif

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

    // The grid knobs. Milestone 2.5 folded these in at the owner's request:
    // the 7B derives a grid that forces every card compact, and the question
    // of which cell size is right is one only the glass can answer.
    // Demand a column / row count. Each wraps through AUTO, so there is always
    // a way back to the derived layout. Auto starts from whatever the grid is
    // currently showing, so the first press moves by one rather than jumping.
    void nudgeColumns(int8_t steps);
    void nudgeRows(int8_t steps);
    void toggleDeck();

    // LVGL's FPS / CPU overlay, bottom-right. Swipe up from the bottom-right
    // to toggle; the bottom-left keeps the deck. Hidden at boot. The CPU
    // figure is LVGL's own load, not the chip's - see lv_conf.h.
    void togglePerf();

    // --- UI TWEAKS (2.8 round 3, D-22, D-25; GUIManagerTweaks.cpp) -----------
    // The development window of the card looks, every row a choice made
    // outright - not a cycle - and SAVED (Settings system.ui.*, the glyphs
    // system.header.glyphs, the timeout system.window.*), for EVERY page: a
    // choice is device-wide (the per-page working set below takes it).
    // Reset all puts them back. The deck rises under it while it is open.
    void openUiTweaks();
    void chooseScheme(uint8_t i, bool save = true);   // 0 Fleet, 1 Midnight, 2 Linen (save: a PC's look is not)
    void setCardLabelStyle(CardHeaderStyle s);    // Float / Band / Tag / None
    void setEntityLabel(CardLabel l);             // Name (HA) / State / Custom / None
    void setCardState(StateCardFill f);           // Fill / Icon glow
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

    // --- Pages, milestone 2.6. docs/design/pages.md. -----------------------
    //
    // THE ONE WAY TO CHANGE PAGE. A swipe, and later a card link, a gesture
    // target or the overview, all ask here; nothing else rebuilds a different
    // page. NINA's navigation arbiter taught the rule: one owner of page
    // changes, so two sources can never fight over which page is showing.
    //
    // `index` is a position in the swipe order, and it WRAPS - the owner:
    // "with 10 pages, you need to swipe all the way back just to reach page 1?"
    void goToPage(int16_t index);
    void nextPage() { goToPage((int16_t)_curPage + 1); }
    void prevPage() { goToPage((int16_t)_curPage - 1); }

    // Read-only views for GET /bench (2.9, UI/Bench.cpp), which needs to know
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

    // THE HEADER'S SLOTS (2.8). A tap on a slot opens its panel - the status
    // slot's is the system panel, the others' a Kit::Drawer built from
    // SlotPanels - and a second tap, a tap outside, a swipe up or a page change
    // puts it away. One panel at a time.
    void onSlotTap(uint8_t slot);
    void openSlotPanel(uint8_t slot);
    void closeSlotPanel(bool animate);
    void syncScrim();                // the dismiss scrim catches taps while any panel is open
    // The page's header, shown or hidden (D-14): the owner's choice for the
    // page (Settings pages.<slug>.header), else the page's compiled default.
    bool pageHeaderShown() const;
    void setPageHeaderShown(bool shown);   // saves it, applies it, rebuilds
    void applyHeaderHidden(bool hidden);   // the bar, the drawers' top, the knob's label
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
    template <typename F> void everyPage(F f) { for (uint8_t i = 0; i < _nPages; i++) f(_pages[i]); }
    friend struct GUITweaks;
    bool        _deckRaised   = false;   // the deck is up under UI Tweaks
    // When a header panel was last touched or opened: it closes on the window
    // timeout as a window does (D-22).
    uint32_t    _panelTouchMs = 0;
    Kit::Drawer _slotDrawer;
    int8_t      _slotPanel = -1;     // the slot whose drawer is open, or -1
    // ROUND 4: a slot's tab stays open until its drawer has finished closing,
    // and the header stays visible while any header panel is open - a Show /
    // Hide chosen meanwhile waits for them to close.
    int8_t      _closingSlot   = -1; // the slot whose drawer is closing, or -1
    bool        _headerPending = false;
    void slotDrawerClosed();
    void headerPanelsClosed();
    static void reportUiSection();   // [UI STATE] in the System Doctor
    // Milestone 2.6, first cut. Registered on the SCREEN so a scrollable child
    // still wins its own drag - see the definition.
    static void screenGestureCb(lv_event_t *e);
    static void screenPressCb(lv_event_t *e);

    // The hidden-header peek (2.6). One swipe shows the bar for a few seconds
    // so the status glyphs can be read; a second opens the drawer. Drawn over
    // the page on the top layer rather than rebuilt into it, so a glance never
    // re-plans the grid.
    // Where the drawer hangs from, in ONE place. Three sites used to compute
    // it and one of them did not know about a peeked header, which is how
    // tapping the status icon opened the drawer behind the bar.
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
    // by a page because pages come and go and it must not: milestone 2.6's
    // tileview will have several pages sharing this single lv_timer, and an
    // off-screen page still needs its values. See CardBinder.h.
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
    // DEFAULTS CHOSEN ON THE GLASS, 2026-09-17. Every one of these is still a
    // knob on the System panel, and every one of them becomes a stored setting
    // at 4.1 - these are the values a fresh flash starts from, not decisions
    // that have been closed.
    // FLOATING TAG since 2026-10-01 (design interview §1): the owner wants the
    // tag as the default "until we fix the card header bar", floating
    // preferred.
    CardHeaderStyle _hdr    = CardHeaderStyle::HDR_TAG_FLOAT;
    // COMPACT by default since 2.7 round two - the owner, on glass: the "Seen"
    // line was less useful than expected (HA's own tiles do not surface it),
    // and without it the icons, disc and text all breathe. Auto and Full stay
    // one knob press away.
    CardVariant     _variant = CardVariant::VAR_COMPACT;
    StateCardFill   _fill    = StateCardFill::FILL_SURFACE;
    bool            _showArea = true;

    // PER-BOARD DEFAULT GRID, chosen on the glass by the owner.
    //
    // A board should boot into ITS layout rather than into whatever the
    // derivation happens to produce - ROADMAP section 7 records the picks and
    // notes they belong in the flashed image. These are that, and the Col/Row
    // buttons remain the override.
    //
    // They are EXACT COUNTS, not hints, and that is deliberate. TARGET_CARD_W
    // decides columns by derivation, which is the right default but is not a
    // user interface: `docs/LESSONS.md` records the session where the Col/Row
    // knobs nudged TARGET_CARD_W and ASPECT_PCT and hoped, and on
    // CYD_S3_3248 no aspect value could ever subtract a row. "When a user
    // wants to say three columns, let them say three columns" - so a default
    // says it the same way a knob does, through COLS_OVERRIDE and
    // CardPage::setRowsOverride().
    //
    // Same #ifdef-on-the-board-identity-macro pattern as
    // ConnectivityDefaults.h and UITokens.cpp's TARGET_CARD_W. No new
    // machinery, and 0 still means "derive it".
    //
    // Set 2026-09-18 for the boards the owner named. The remaining picks in
    // ROADMAP section 7 (CYD_S3_3248 2x4, WS_P4_7B and CYD_P4_1060 6x3) are
    // NOT applied here yet - he asked for these two and guessing the rest
    // would put numbers on boards nobody has looked at since.
#if   defined(WS_P4_5)
    uint8_t         _colsOverride = 5;
    uint8_t         _rowsOverride = 3;
// 4x3, changed from 3x4 on 2026-09-21 after seeing the real 18-card dashboard
// on both 4B panels. The owner: "4x3 looks and fits better than 3x4 on the 4B
// screens, the circle around icons actually fits then."
//
// It is the same cell COUNT either way. What changes is the aspect: three rows
// of four gives each cell more width and less height, and the icon disc - which
// is sized from the cell - stops being squashed into an ellipse. A card is
// wider than it is tall in every mock we have, and 3x4 was fighting that.
#elif defined(WS_P4_4B) || defined(WS_S3_4B)
    uint8_t         _colsOverride = 4;
    uint8_t         _rowsOverride = 3;
// Owner, 2026-09-29, on glass: portrait 2x5 "regardless of the number of
// cards" (3x5 also usable); landscape 4x3 "a great fit". Chosen by the BSP's
// ROTATION, since this board has been run both ways.
#elif defined(CYD_P4_4880)
    uint8_t         _colsOverride = (bsp_display.ROTATION & 1) ? 4 : 2;
    uint8_t         _rowsOverride = (bsp_display.ROTATION & 1) ? 3 : 5;
#else
    uint8_t         _colsOverride = 0;      // 0 = derive from TARGET_CARD_W
    uint8_t         _rowsOverride = 0;      // 0 = let the cards decide
#endif
    // WHAT EACH PAGE REMEMBERS. Every drawer knob except Deck and Hide Bar is
    // PER PAGE - the owner, 2026-09-23: "the header and the deck are
    // functional components of the dashboard in general so those should
    // persist across pages", while density, colour, header mode, fill, area,
    // label and grid belong to the page, because a page built for a fixed set
    // of cards needs its own arrangement.
    //
    // The fields above (_hdr, _variant, _fill, _showArea, _colsOverride,
    // _rowsOverride) remain the CURRENT page's working set, which is what every
    // knob already edits. Changing page stores them here and loads the next
    // page's - the smallest change that makes all the existing knobs per-page
    // without touching any of them. In RAM only: the build sheet and the
    // settings pages are what will make them persistent.
    struct PageLive {
        const PageSpec *spec;
        CardHeaderStyle hdr;
        CardVariant     variant;
        StateCardFill   fill;
        CardLabel       label;
        bool            showArea;
        uint8_t         cols;
        uint8_t         rows;
        uint8_t         scheme;     // UI::schemeIndex()
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
    // THE permanent system header height. Owner's decision 2026-09-19: the
    // size cycle is gone, only hide/show remains.
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
