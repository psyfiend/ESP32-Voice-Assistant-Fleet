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
#include "UI/Panel_Header.h"
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

    // Cycle the header treatment on the live dashboard: tag -> bar -> none.
    //
    // All three modes ship permanently - cards.md is explicit that picking one
    // is "what a card looks like when nobody chose", not an elimination - so
    // this is how a default gets chosen by looking at it. It is deliberately
    // RUNTIME state rather than a rebuild-time constant, because the device
    // settings page (4.1) will own exactly this control later.
    void cycleHeader();

    // Cycle the colour scheme, from the System panel rather than the reference
    // page. Also re-applies the SCREEN's own background, which nothing did
    // before: the ground colour behind the deck was set once at start-up and
    // then never followed the scheme.
    void cycleScheme();

    // Open the design-token reference page, standing the dashboard down first.
    void openTokens();
    void openLog();      // the System Doctor report, on its own screen

    // Cycle the SYSTEM header bar height: 50 -> 45 -> 40 -> 35 -> 30 -> none.
    //
    // "none" hides the bar entirely, which would otherwise strand the user:
    // the header's status icon is the only way to open this drawer. So a
    // transparent strip is left on the top layer to take the tap - the
    // simplest form of the owner's swipe-down-to-reveal idea, and a footgun
    // guard rather than a feature.
    void cycleHeaderBar();

    // The three controls #50 added. Each drives a card-layer static that has
    // existed since 2.4 with nothing on the device able to reach it.
    void cycleVariant();   // auto / full / compact - argue with the measurement
    void cycleFill();      // how an active StateCard reads
    void toggleArea();     // area on every card, or not
    bool deckShown() const { return _showDeck; }

    Panel_Header &header()      { return _header; }
    Panel_System &systemPanel() { return _pnlSystem; }
    CardBinder   &cards()       { return _binder; }

private:
    void applyGround();              // the screen background, per scheme
    void buildDashboard();           // creates _dashHost and _page
    void destroyDashboard();

    static void headerIconClickCb(lv_event_t *e);
    static void closeSystemPanelCb();
    static void reportUiSection();   // [UI STATE] in the System Doctor
    // Milestone 2.6, first cut. Registered on the SCREEN so a scrollable child
    // still wins its own drag - see the definition.
    static void screenGestureCb(lv_event_t *e);

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

    // Overrides PageSpec::headerDefault. The spec is const; this is the live
    // choice laid over it in buildDashboard().
    // DEFAULTS CHOSEN ON THE GLASS, 2026-09-17. Every one of these is still a
    // knob on the System panel, and every one of them becomes a stored setting
    // at 4.1 - these are the values a fresh flash starts from, not decisions
    // that have been closed.
    CardHeaderStyle _hdr    = CardHeaderStyle::HDR_BAR;
    CardVariant     _variant = CardVariant::VAR_AUTO;   // derive, until told otherwise
    StateCardFill   _fill    = StateCardFill::FILL_SURFACE;
    bool            _showArea = true;
    uint8_t         _scheme = 1;            // Slate

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
#elif defined(WS_P4_4B) || defined(WS_S3_4B)
    uint8_t         _colsOverride = 3;
    uint8_t         _rowsOverride = 4;
#else
    uint8_t         _colsOverride = 0;      // 0 = derive from TARGET_CARD_W
    uint8_t         _rowsOverride = 0;      // 0 = let the cards decide
#endif
    lv_obj_t       *_hiddenBarTap = nullptr;
    lv_obj_t     *_deck     = nullptr;

    Panel_Header  _header;
    Panel_System  _pnlSystem;
    Panel_Display _pnlDisplay;
#ifdef HAS_AUDIO_HW
    Panel_Audio   _pnlAudio;
#endif
};
