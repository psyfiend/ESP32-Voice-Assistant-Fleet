#include "GUIManager.h"
#include "UI/UIToolkit.h"
#include "UI/UITokens.h"
#include "SystemReport.h"
#include "Cards/CardDemo.h"
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
    UIToolkit::init();

    // --= ROOT SCREEN =--
    lv_obj_t *screen = lv_screen_active();

    // Design tokens. Started here rather than in LVGL_Startup for the same
    // reason UIToolkit is: this is the design system, not the engine. The
    // viewport comes from the live screen so it is already rotated - deriving
    // the grid from bsp_display.WIDTH/HEIGHT would be wrong on every board
    // running at rotation 1 or 3.
    UI::begin(lv_obj_get_width(screen), lv_obj_get_height(screen));
    lv_obj_set_style_bg_color(screen, UI::c(UI::pal().GROUND), LV_PART_MAIN); // Dark background
    lv_obj_clear_flag        (screen, LV_OBJ_FLAG_SCROLLABLE);               // Disable global scrolling

    // --= LAYER 3: HEADER BAR =--
    // Header click -> toggle system panel
    _header.init(screen, bsp_hw.device_name, &_core.conn());
    lv_obj_add_event_cb(_header.getStatusIcon(), headerIconClickCb, LV_EVENT_CLICKED, NULL);

    // Bottom deck height = screen height - header height.
    int32_t header_h = UIToolkit::sc(50);
    int32_t deck_h   = lv_obj_get_height(screen);

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
        CardDemo::show(_core.entities(), _binder);
    });

    // The grid knobs. The panel knows a button was pressed; what a column is
    // remains entirely this class's business.
    _pnlSystem.setOnGridAction([this](Panel_System::GridAction a) {
        switch (a) {
            case Panel_System::GridAction::CARD_W_DOWN: nudgeCardWidth(-1); break;
            case Panel_System::GridAction::CARD_W_UP:   nudgeCardWidth(+1); break;
            case Panel_System::GridAction::ASPECT_DOWN: nudgeAspect(-1);    break;
            case Panel_System::GridAction::ASPECT_UP:   nudgeAspect(+1);    break;
            case Panel_System::GridAction::DECK_TOGGLE: toggleDeck();       break;
        }
    });

    // The card page's own Dump button runs the same report the System panel's
    // does, Serial echo and all.
    CardDemo::setDumpHandler([this]() { SystemReport::run(_core, true); });

    // The bench derives the grid from its own host - a screen minus a button
    // bar - and UI::grid() is global. Rebuilding on the way back is the only
    // thing that reliably restores the dashboard's own geometry.
    CardDemo::setCloseHandler([this]() { rebuildDashboard(); });

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
}

// ---------------------------------------------------------------------------
// The dashboard
// ---------------------------------------------------------------------------

void GUIManager::buildDashboard() {
    lv_obj_t *screen = lv_screen_active();

    const int32_t headerH = UIToolkit::sc(50);

    // What the accordion deck keeps for itself while every panel is COLLAPSED.
    // UIToolkit builds a collapsed panel at sc(85) and the deck pads itself by
    // sc(10) - see UIToolkit.cpp. An EXPANDED panel is sc(280) and is allowed
    // to cover cards; only the resting state costs the grid anything.
    //
    // This is not free and it is not a preference: on WS_P4_7B it is ~105 px
    // of 600, which is what takes the page from four rows to three.
    const int32_t deckReserve = _showDeck ? UIToolkit::sc(85) + UIToolkit::sc(20) : 0;

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
    _page->applySpec(FLEET_PAGE, _core.entities());

    lv_obj_move_to_index(_dashHost, 1);

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
}

// ---------------------------------------------------------------------------
// The grid knobs
//
// A rebuild rather than CardPage::relayout(), and the difference is the whole
// reason these exist: a card decides compact-vs-full from its cell height when
// it is BUILT, so moving the grid without rebuilding would re-place the same
// cards at the same level of detail and show nothing.
// ---------------------------------------------------------------------------

void GUIManager::nudgeCardWidth(int8_t steps) {
    int32_t w = (int32_t)UI::grid().TARGET_CARD_W + steps * 5;
    if (w < 80)  w = 80;
    if (w > 300) w = 300;
    UI::setTargetCardWidth((uint16_t)w);
    rebuildDashboard();
    Serial.printf("[Cards] TARGET_CARD_W %ld -> %ux%u cells of %ux%u px\n",
                  (long)w, (unsigned)UI::grid().cols, (unsigned)UI::grid().rows,
                  (unsigned)UI::grid().cellW, (unsigned)UI::grid().cellH);
}

void GUIManager::nudgeAspect(int8_t steps) {
    int32_t a = (int32_t)UI::grid().ASPECT_PCT + steps * 5;
    UI::setAspectPct((uint8_t)(a < 40 ? 40 : (a > 200 ? 200 : a)));
    rebuildDashboard();
    Serial.printf("[Cards] ASPECT_PCT %u -> %ux%u cells of %ux%u px\n",
                  (unsigned)UI::grid().ASPECT_PCT,
                  (unsigned)UI::grid().cols, (unsigned)UI::grid().rows,
                  (unsigned)UI::grid().cellW, (unsigned)UI::grid().cellH);
}

void GUIManager::toggleDeck() {
    _showDeck = !_showDeck;
    rebuildDashboard();
    Serial.printf("[Cards] deck %s -> %ux%u cells of %ux%u px\n",
                  _showDeck ? "shown" : "hidden",
                  (unsigned)UI::grid().cols, (unsigned)UI::grid().rows,
                  (unsigned)UI::grid().cellW, (unsigned)UI::grid().cellH);
}

void GUIManager::tick() {
    _header.tick();
    _pnlDisplay.tick();
#ifdef HAS_AUDIO_HW
    _pnlAudio.tick();
#endif
}
