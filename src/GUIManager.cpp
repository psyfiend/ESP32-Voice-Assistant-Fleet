#include "GUIManager.h"
#include "UI/UIToolkit.h"
#include "UI/UITokens.h"
#include "SystemReport.h"
#include "Cards/CardDemo.h"
#include "UI/ReferencePage.h"
#include "UI/LogPage.h"
#include "Dashboards/Dashboard_Fleet.h"
#include "bsp_loader.h"

// LVGL's event and callback APIs take plain function pointers with no user
// context of their own for some of the toolkit hooks, so the single dashboard
// instance is reachable from a file-static. There is exactly one GUIManager by
// construction; if that ever stops being true these become member callbacks
// with user_data, which LVGL does support for lv_event_cb.
static GUIManager *s_self = nullptr;

GUIManager::GUIManager(SystemCore &core)
    : _core(core)
    , _pnlDisplay(core.display(), core.touch())
#ifdef HAS_AUDIO_HW
    , _pnlAudio(core.audio())
#endif
{
    s_self = this;
}

void GUIManager::headerIconClickCb(lv_event_t *e) {
    (void)e;
    if (s_self) s_self->_pnlSystem.toggle();
}

void GUIManager::closeSystemPanelCb() {
    if (s_self) s_self->_pnlSystem.close();
}

// The one part of the System Doctor that genuinely needs LVGL. Registered with
// SystemReport rather than living inside it, which is what keeps SystemReport
// free of any LVGL include.
void GUIManager::reportUiSection() {
    lv_obj_t *activePnl = UIToolkit::getActiveAccordionPanel();
    SystemReport::line("  Active Panel: %s", activePnl ? "EXPANDED" : "NONE (Collapsed)");
}

void GUIManager::begin() {
    // Styles, semantic fonts and the toast layer. Design system, not engine -
    // which is why it lives here and not in LVGL_Startup. This is the seam
    // milestone 2.2 lands on.
    SystemCore::heapMark("before UI");
    UIToolkit::init();

    // The starting scheme. UITokens defaults to Fleet; the owner's pick is
    // Slate, and applying it here rather than editing the token file keeps
    // "which scheme ships" a GUIManager decision alongside the other defaults.
    UI::setScheme(UI_PAL_SLATE, UI_MET_DARK);

    // --= ROOT SCREEN =--
    lv_obj_t *screen = lv_screen_active();

    // Design tokens. Started here rather than in LVGL_Startup for the same
    // reason UIToolkit is: this is the design system, not the engine. The
    // viewport comes from the live screen so it is already rotated - deriving
    // the grid from bsp_display.WIDTH/HEIGHT would be wrong on every board
    // running at rotation 1 or 3.
    UI::begin(lv_obj_get_width(screen), lv_obj_get_height(screen));
    applyGround();
    lv_obj_clear_flag        (screen, LV_OBJ_FLAG_SCROLLABLE);               // Disable global scrolling

    // --= LAYER 3: HEADER BAR =--
    // Header click -> toggle system panel
    _header.init(screen, bsp_hw.device_name, &_core.conn());
    lv_obj_add_event_cb(_header.getStatusIcon(), headerIconClickCb, LV_EVENT_CLICKED, NULL);

    // Bottom deck height = screen height - header height.
    int32_t header_h = UIToolkit::systemHeaderPx();
    // The deck ends a FIXED distance below the screen, so that exactly one
    // panel header shows however tall the system header is.
    //
    // It used to be as tall as the whole display while starting below the
    // header, which meant its overhang WAS the header height - so the visible
    // strip changed whenever the header did. That is what put a gap under the
    // word AUDIO when the header went to 35. Making the deck stop at the
    // screen edge fixed the gap and broke the effect instead: the panels
    // became floating buttons with four rounded corners. Both wrong; this is
    // the relationship they were each half of.
    int32_t deck_h   = lv_obj_get_height(screen) - header_h + UIToolkit::deckOverhangPx();

    // --= LAYER 1: BOTTOM DECK =--
    // Contains the Audio/Display panels.
    lv_obj_t *deck = lv_obj_create(screen);
    _deck = deck;
    lv_obj_set_size               (deck, lv_pct(100), deck_h);
    lv_obj_set_y                  (deck, header_h); // Bottom of header
    lv_obj_set_flex_flow          (deck, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align         (deck, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_END);
    lv_obj_set_style_bg_opa       (deck, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width (deck, 0, 0);
    lv_obj_set_style_pad_all      (deck, UIToolkit::sc(10), 0);
    lv_obj_set_style_pad_gap      (deck, UIToolkit::sc(10), 0);
    lv_obj_clear_flag             (deck, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag             (deck, LV_OBJ_FLAG_SCROLLABLE);

    // --= LAYER 2: UPPER DECK (System Panel) =--
    // Full screen transparent layer to hold the system drawer.
    lv_obj_t *upper_deck = lv_obj_create(screen);
    lv_obj_set_size               (upper_deck, lv_pct(100), lv_pct(100));
    lv_obj_set_y                  (upper_deck, 0); // Hidden behind header
    lv_obj_set_style_bg_opa       (upper_deck, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width (upper_deck, 0, 0);
    lv_obj_clear_flag             (upper_deck, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag             (upper_deck, LV_OBJ_FLAG_SCROLLABLE);

    // Bottom panel open -> close system panel
    UIToolkit::registerSystemCloseCb(closeSystemPanelCb);

    // System open -> hide touch window
    _pnlSystem.setOnToggleCallback([](bool isOpen) {
        (void)isOpen;
        // If System Panel is OPEN (true), Hide Touch Window (false)
        // pnlDisplay.setTouchWindowVisibility(!isOpen);
    });

#ifdef HAS_AUDIO_HW
    _pnlAudio.init(deck);
#endif
    _pnlDisplay.init(deck);
    _pnlSystem.init(upper_deck, &_header);

    // The System panel's "Dump Config" button re-runs the report with Serial
    // echo on. Registered rather than reached for: Panel_System used to call
    // an `extern void debug_dump_config(bool)` straight into main.
    _pnlSystem.setOnDumpRequested([this]() {
        SystemReport::run(_core, true); // manually triggered - mirror to Serial too
    });

    // The card binder. Started AFTER UI::begin() because a card reads tokens
    // the moment it is built, and before any page exists because a page
    // registers its cards with it. Nothing has a card yet - this only starts
    // the pump.
    _binder.begin(&_core.entities());

    // Cards read UI::pal() on every render and cache nothing, which is what
    // makes a live scheme change possible at all. This is the other half of
    // that bargain: somebody has to tell them the scheme moved.
    UI::onSchemeChanged([]() { if (s_self) s_self->_binder.restyleAll(); });

    // The System panel's "Cards" button. Registered rather than reached for,
    // for the same reason Dump Config is: Panel_System has no business knowing
    // the entity registry exists.
    _pnlSystem.setOnCardsRequested([this]() {
        // THE DASHBOARD COMES DOWN FIRST.
        //
        // The bench builds its own page of thirteen more cards. With the
        // dashboard still alive that is twenty-six cards and two full widget
        // trees, which is precisely the condition 2.4 documented as fatal:
        // "Allocating layer buffer failed", then a reboot. The owner hit it
        // immediately - tapping Cards reset the board.
        //
        // The close handler below rebuilds it on the way back.
        destroyDashboard();
        CardDemo::show(_core.entities(), _binder);
    });

    _pnlSystem.setOnTokensRequested([this]() { openTokens();  });
    _pnlSystem.setOnLogRequested   ([this]() { openLog();     });

    // The log page's own Dump button re-runs the report and redraws it in
    // place. The panel keeps buffering while the page is open, so this is just
    // "run it again and show me".
    LogPage::setDumpHandler([this]() {
        SystemReport::run(_core, true);
        LogPage::refresh(_pnlSystem.logText());
    });
    LogPage::setCloseHandler([this]() { rebuildDashboard(); });
    _pnlSystem.setOnSchemeRequested([this]() { cycleScheme(); });

    // The grid knobs. The panel knows a button was pressed; what a column is
    // remains entirely this class's business.
    _pnlSystem.setOnGridAction([this](Panel_System::GridAction a) {
        switch (a) {
            case Panel_System::GridAction::CARD_W_DOWN: nudgeColumns(-1);  break;
            case Panel_System::GridAction::CARD_W_UP:   nudgeColumns(+1);  break;
            case Panel_System::GridAction::ASPECT_DOWN: nudgeRows(-1);     break;
            case Panel_System::GridAction::ASPECT_UP:   nudgeRows(+1);     break;
            case Panel_System::GridAction::DECK_TOGGLE: toggleDeck();       break;
            case Panel_System::GridAction::HDR_CYCLE:   cycleHeader();     break;
            case Panel_System::GridAction::BAR_CYCLE:   cycleHeaderBar();  break;
        }
    });

    // The card page's own Dump button runs the same report the System panel's
    // does, Serial echo and all.
    CardDemo::setDumpHandler([this]() { SystemReport::run(_core, true); });

    // The bench derives the grid from its own host - a screen minus a button
    // bar - and UI::grid() is global. Rebuilding on the way back is the only
    // thing that reliably restores the dashboard's own geometry.
    CardDemo::setCloseHandler([this]() { rebuildDashboard(); });
    ReferencePage::setCloseHandler([]() { if (s_self) s_self->rebuildDashboard(); });

    // Contribute the one LVGL-dependent section of the report.
    SystemReport::addSection("UI STATE", reportUiSection);

    // --= Z-INDEX SANDWICH =--
    // 0. Touch overlay (bottom - hidden by default, set in Panel_Display::init)
    // 1. Dashboard (the cards)
    // 2. Deck (bottom panels, which may expand OVER the cards)
    // 3. System panel (middle - slides out)
    // 4. Header (top - covers the system panel's top edge)
    lv_obj_move_to_index(deck, 1);
    lv_obj_move_to_index(upper_deck, 2);
    lv_obj_move_to_index(_header.getContainer(), 3);

    // THE DASHBOARD IS THE BOOT SCREEN NOW.
    //
    // Until 2.5 the device booted into the Phase 1 UI - a header and two
    // accordion panels - and the cards were a demo behind a button in the
    // System drawer. That was the right shape while the card layer was being
    // built and the wrong one the moment it worked.
    //
    // Built LAST so it can read the real geometry of everything above it, and
    // moved to index 1 so the deck's panels expand over the cards rather than
    // pushing them - which is what the owner asked to see.
    buildDashboard();
    SystemCore::heapMark("after dashboard");
}

// ---------------------------------------------------------------------------
// The dashboard
// ---------------------------------------------------------------------------

void GUIManager::buildDashboard() {
    lv_obj_t *screen = lv_screen_active();

    const int32_t headerH = UIToolkit::systemHeaderPx();

    // WHAT THE DECK ACTUALLY COSTS, MEASURED RATHER THAN ASSUMED.
    //
    // The first version of this reserved sc(85) + sc(20), on the reasoning that
    // UIToolkit builds a collapsed panel at sc(85). That was wrong by about
    // 60 px and the owner spotted it on the glass: "with the deck present there
    // is always a massive gap between the bottom row and the deck".
    //
    // The panel really is 85 px tall. It is just that the deck is as tall as
    // the whole screen and starts BELOW the header, so its bottom edge hangs
    // 50 px off the bottom of the panel - and a bottom-aligned panel therefore
    // has its lower ~40 px off-screen. What you can actually see is the panel's
    // sc(45) header and nothing else, which is exactly what he described.
    //
    // Rather than encode that coincidence as a number, ask the objects where
    // they are. This survives someone changing a panel's height, and it is the
    // same "verify from outside" rule the connectivity work runs on.
    int32_t deckReserve = 0;
    if (_showDeck && _deck) {
        lv_obj_update_layout(screen);

        // lv_obj_get_coords() gives ABSOLUTE screen coordinates. The x/y
        // accessors are relative to the parent, and the deck's children have a
        // different parent from the screen - mixing the two would measure
        // nothing meaningful.
        lv_area_t sc_area;
        lv_obj_get_coords(screen, &sc_area);
        const int32_t screenBottom = sc_area.y2;

        int32_t topMost = screenBottom;
        const uint32_t kids = lv_obj_get_child_count(_deck);
        for (uint32_t i = 0; i < kids; i++) {
            lv_obj_t *k = lv_obj_get_child(_deck, i);
            if (!k || lv_obj_has_flag(k, LV_OBJ_FLAG_HIDDEN)) continue;
            lv_area_t k_area;
            lv_obj_get_coords(k, &k_area);
            if (k_area.y1 < topMost) topMost = k_area.y1;
        }
        deckReserve = screenBottom - topMost;
        if (deckReserve < 0) deckReserve = 0;
        // NO EXTRA GAP. The page's own INSET already holds the bottom row off
        // the edge of its host, and with the deck hidden that inset is exactly
        // the margin the owner liked ("6x3 sits neatly against the bottom
        // margin"). Adding a gap on top of it made the deck case visibly
        // looser than the hidden case for no reason - his "modest gap".
    }

    int32_t h = lv_obj_get_height(screen) - headerH - deckReserve;
    if (h < UIToolkit::sc(80)) h = lv_obj_get_height(screen) - headerH;

    _dashHost = lv_obj_create(screen);
    lv_obj_set_size               (_dashHost, lv_pct(100), h);
    lv_obj_set_y                  (_dashHost, headerH);
    lv_obj_set_style_bg_opa       (_dashHost, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width (_dashHost, 0, 0);
    lv_obj_set_style_pad_all      (_dashHost, 0, 0);
    lv_obj_clear_flag             (_dashHost, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag             (_dashHost, LV_OBJ_FLAG_CLICKABLE);

    // The grid is derived from THIS CONTAINER, not from the screen and not
    // from bsp_display.WIDTH/HEIGHT. Deriving it from the panel would be wrong
    // on every rotated board and wrong here as well, because the cards do not
    // get the whole screen - the same mistake begin() already documents for
    // UI::begin().
    lv_obj_update_layout(screen);
    UI::setViewport(lv_obj_get_content_width(_dashHost),
                    lv_obj_get_content_height(_dashHost));

    _page = new CardPage();
    _page->begin(_dashHost, &_binder);
    _page->setRowsOverride(_rowsOverride);

    // The fleet spec is const and carries a header default; the live choice is
    // laid over a copy of it. PageSpec is a plain aggregate, so this is a copy
    // and an assignment rather than any kind of mechanism - which is the point
    // of the spec being data.
    PageSpec page = FLEET_PAGE;
    page.headerDefault = _hdr;
    _page->applySpec(page, _core.entities());

    _pnlSystem.setHeaderLabel(_hdr == CardHeaderStyle::HDR_TAG  ? "Tag"
                            : _hdr == CardHeaderStyle::HDR_BAR  ? "Bar"
                                                                : "No hdr");

    // THE DASHBOARD GOES TO THE BACK, by role rather than by index.
    //
    // This was `move_to_index(_dashHost, 1)`, which assumed the touch overlay
    // was still the screen's child 0. Moving that overlay to lv_layer_top()
    // shifted every remaining index down by one, so index 1 quietly became
    // ABOVE the deck instead of below it - and the panels that used to animate
    // over the cards started expanding behind them.
    //
    // move_background() says what is actually meant and cannot rot when the
    // screen's child list changes again.
    lv_obj_move_background(_dashHost);

    if (_deck) {
        if (_showDeck) lv_obj_clear_flag(_deck, LV_OBJ_FLAG_HIDDEN);
        else           lv_obj_add_flag  (_deck, LV_OBJ_FLAG_HIDDEN);
    }
}

void GUIManager::destroyDashboard() {
    // The page first: it deletes its cards, and a card deregisters from the
    // binder in its destructor's path. Deleting the host out from under them
    // would take the widgets without the bookkeeping.
    if (_page)     { delete _page;              _page = nullptr; }
    if (_dashHost) { lv_obj_delete(_dashHost);  _dashHost = nullptr; }
}

void GUIManager::rebuildDashboard() {
    destroyDashboard();
    buildDashboard();
    // A rebuild is the one thing that happens over and over on a running
    // board. If internal heap trends down across these, the leak is here.
    SystemCore::heapMark("after rebuild");
}

// ---------------------------------------------------------------------------
// The grid knobs
//
// A rebuild rather than CardPage::relayout(), and the difference is the whole
// reason these exist: a card decides compact-vs-full from its cell height when
// it is BUILT, so moving the grid without rebuilding would re-place the same
// cards at the same level of detail and show nothing.
// ---------------------------------------------------------------------------

// Wraps through 0 = AUTO, so the button can always get back to the derived
// layout. From auto the first press moves one step from what is on screen,
// which is what makes it feel like a nudge rather than a jump.
static uint8_t cycleCount(uint8_t current, uint8_t showing, int8_t steps, uint8_t maxN) {
    int n = (current ? current : showing) + steps;
    if (n > (int)maxN) return 0;      // past the top -> auto
    if (n < 1)         return 0;      // past the bottom -> auto
    return (uint8_t)n;
}

// The screen's own background. Called at start-up and again on every scheme
// change, which is the part that was missing: GROUND was applied once in
// begin() and never again, so switching scheme left the strip behind the deck
// painted in the old scheme's colour. Reported on both P4 boards.
void GUIManager::applyGround() {
    lv_obj_t *screen = lv_screen_active();
    if (screen) lv_obj_set_style_bg_color(screen, UI::c(UI::pal().GROUND), LV_PART_MAIN);
}

void GUIManager::cycleScheme() {
    _scheme = (uint8_t)((_scheme + 1) % 4);
    switch (_scheme) {
        case 0:  UI::setScheme(UI_PAL_FLEET,    UI_MET_DARK);  break;
        case 1:  UI::setScheme(UI_PAL_SLATE,    UI_MET_DARK);  break;
        case 2:  UI::setScheme(UI_PAL_MIDNIGHT, UI_MET_DARK);  break;
        default: UI::setScheme(UI_PAL_PAPER,    UI_MET_LIGHT); break;
    }
    applyGround();
    _header.restyle();
    _pnlSystem.setSchemeLabel(UI::pal().name);

    // A rebuild rather than a restyle. Cards would survive restyleAll() - that
    // is what "never cache a colour" buys - but a METRICS change moves radii
    // and padding, which a card reads when it is built.
    rebuildDashboard();
}

void GUIManager::openTokens() {
    // THE DASHBOARD STANDS DOWN FIRST, exactly as it does for the card bench.
    //
    // ReferencePage builds a whole second screen. With the dashboard's cards
    // still alive that is two full widget trees, and LVGL then fails to
    // allocate the layer buffers it needs to composite - "No memory: 482x17",
    // repeated, and on WS_P4_5 an unrecoverable board. The owner hit it by
    // going into Tokens, changing scheme, and coming back.
    destroyDashboard();
    ReferencePage::show();
}

void GUIManager::cycleHeaderBar() {
    static const uint8_t STEPS[] = { 50, 45, 40, 35, 30, 0 };
    uint8_t i = 0;
    for (; i < sizeof(STEPS); i++) if (STEPS[i] == UIToolkit::systemHeaderH) break;
    UIToolkit::systemHeaderH = STEPS[(i + 1) % sizeof(STEPS)];

    // The header object itself is Phase-1 UI built once in begin(), so it is
    // resized in place rather than rebuilt.
    lv_obj_t *hdr = _header.getContainer();
    if (hdr) {
        lv_obj_set_height(hdr, UIToolkit::systemHeaderPx());
        if (UIToolkit::systemHeaderH) lv_obj_clear_flag(hdr, LV_OBJ_FLAG_HIDDEN);
        else                          lv_obj_add_flag  (hdr, LV_OBJ_FLAG_HIDDEN);
    }

    // With no bar there is no status icon, and the status icon is the only way
    // back into this drawer. Leave an invisible strip on the top layer that
    // toggles it - otherwise choosing "none" and closing the drawer means a
    // reboot.
    if (!UIToolkit::systemHeaderH && !_hiddenBarTap) {
        _hiddenBarTap = lv_obj_create(lv_layer_top());
        lv_obj_set_size               (_hiddenBarTap, lv_pct(100), UI::minTouch() / 2);
        lv_obj_align                  (_hiddenBarTap, LV_ALIGN_TOP_MID, 0, 0);
        lv_obj_set_style_bg_opa       (_hiddenBarTap, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width (_hiddenBarTap, 0, 0);
        lv_obj_add_flag               (_hiddenBarTap, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb           (_hiddenBarTap, [](lv_event_t *e) {
            (void)e; if (s_self) s_self->_pnlSystem.toggle();
        }, LV_EVENT_CLICKED, nullptr);
    } else if (UIToolkit::systemHeaderH && _hiddenBarTap) {
        lv_obj_delete(_hiddenBarTap);
        _hiddenBarTap = nullptr;
    }

    char lbl[12];
    if (UIToolkit::systemHeaderH) snprintf(lbl, sizeof(lbl), "Bar %u", (unsigned)UIToolkit::systemHeaderH);
    else                          snprintf(lbl, sizeof(lbl), "Bar off");
    _pnlSystem.setBarLabel(lbl);

    rebuildDashboard();
    Serial.printf("[UI] system header %u logical px -> %ld real\n",
                  (unsigned)UIToolkit::systemHeaderH, (long)UIToolkit::systemHeaderPx());
}

void GUIManager::openLog() {
    // Dashboard down first, same as the bench and the token page - two full
    // widget trees is what exhausts LVGL's layer buffers.
    destroyDashboard();
    LogPage::show(_pnlSystem.logText());
}

void GUIManager::cycleHeader() {
    // A rebuild, not a restyle. A header bar is CREATED in Card::build() rather
    // than styled in restyle(), and that is correct - it is a structural choice
    // a card makes once, not a live style. CardDemo's own header button has
    // always worked this way for the same reason.
    _hdr = (_hdr == CardHeaderStyle::HDR_TAG) ? CardHeaderStyle::HDR_BAR
         : (_hdr == CardHeaderStyle::HDR_BAR) ? CardHeaderStyle::HDR_NONE
                                              : CardHeaderStyle::HDR_TAG;
    rebuildDashboard();
    Serial.printf("[Cards] header mode -> %s\n",
                  _hdr == CardHeaderStyle::HDR_TAG ? "tag"
                : _hdr == CardHeaderStyle::HDR_BAR ? "bar" : "none");
}

void GUIManager::nudgeColumns(int8_t steps) {
    _colsOverride = cycleCount(_colsOverride, (uint8_t)UI::grid().cols, steps, UI_MAX_COLS);
    UI::setColumnsOverride(_colsOverride);
    rebuildDashboard();

    char lbl[12];
    if (_colsOverride) snprintf(lbl, sizeof(lbl), "Col %u", (unsigned)_colsOverride);
    else               snprintf(lbl, sizeof(lbl), "Col A");
    _pnlSystem.setColsLabel(lbl);
}

void GUIManager::nudgeRows(int8_t steps) {
    // The page is rebuilt from scratch on every change, so the override is
    // held HERE and handed to each new page - a CardPage cannot remember it.
    _rowsOverride = cycleCount(_rowsOverride, _page ? _page->cellRows() : 1, steps, UI_MAX_ROWS);
    rebuildDashboard();

    char lbl[12];
    if (_rowsOverride) snprintf(lbl, sizeof(lbl), "Row %u", (unsigned)_rowsOverride);
    else               snprintf(lbl, sizeof(lbl), "Row A");
    _pnlSystem.setRowsLabel(lbl);
}

void GUIManager::toggleDeck() {
    _showDeck = !_showDeck;
    rebuildDashboard();
    Serial.printf("[Cards] deck %s\n", _showDeck ? "shown" : "hidden");
}

void GUIManager::tick() {
    _header.tick();
    _pnlDisplay.tick();
#ifdef HAS_AUDIO_HW
    _pnlAudio.tick();
#endif
}
