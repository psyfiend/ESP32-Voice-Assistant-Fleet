#include "GUIManager.h"
#include "UIToolkit.h"
#include "UITokens.h"
#include "SystemReport.h"
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

    // Contribute the one LVGL-dependent section of the report.
    SystemReport::addSection("UI STATE", reportUiSection);

    // --= Z-INDEX SANDWICH =--
    // 0. Touch overlay (bottom - hidden by default, set in Panel_Display::init)
    // 1. Deck (bottom panels)
    // 2. System panel (middle - slides out)
    // 3. Header (top - covers the system panel's top edge)
    lv_obj_move_to_index(deck, 1);
    lv_obj_move_to_index(upper_deck, 2);
    lv_obj_move_to_index(_header.getContainer(), 3);
}

void GUIManager::tick() {
    _header.tick();
    _pnlDisplay.tick();
#ifdef HAS_AUDIO_HW
    _pnlAudio.tick();
#endif
}
