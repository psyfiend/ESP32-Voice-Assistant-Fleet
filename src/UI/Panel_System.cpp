#include "UI/Panel_System.h"
#include "UI/AboutSheet.h"
#include "UI/ReferencePage.h"
#include "UI/UITokens.h"

// The single System panel, so the SystemReport sink (a plain function pointer)
// can reach it. One panel exists by construction.
static Panel_System *s_self = nullptr;

Panel_System::Panel_System() {
    _ui_content = NULL;
    _ui_actions = NULL;
    _ui_grid    = NULL;
    _ui_row3    = NULL;
    _ui_row4    = NULL;
    _lbl_variant = NULL;
    _lbl_fill    = NULL;
    _lbl_area    = NULL;
    _lbl_hdr    = NULL;
    _lbl_scheme = NULL;
    _lbl_bar    = NULL;
    _lbl_cols   = NULL;
    _lbl_rows   = NULL;
    lbl_stats   = NULL;
    _headerRef  = NULL;
    _expanded   = false;
    _onToggle   = nullptr;
    
    _ui_timer   = NULL;
    _log_dirty  = false;
    _stats_dirty = false;
}

Panel_System::~Panel_System() {
    if (_ui_timer) lv_timer_delete      (_ui_timer);
}

void Panel_System::setOnToggleCallback  (ToggleCallback cb) {
    _onToggle = cb;
}

void Panel_System::btn_action_cb(lv_event_t* e) {
    Panel_System* p = (Panel_System*)lv_event_get_user_data(e);
    
    // No "> Action" marker line any more: the dump empties the log first, so
    // the report's own header is the first line.
    if (p && p->_onDumpRequested) p->_onDumpRequested();
}

void Panel_System::reportSink(const char *line) {
    if (s_self) s_self->log("%s", line);
}

// One knob button. Capture-less lambdas only - an lv_event_cb_t is a plain
// function pointer, so the panel arrives as user_data rather than in a capture.
void Panel_System::setHeaderLabel(const char *text) {
    if (_lbl_hdr && text) lv_label_set_text(_lbl_hdr, text);
}

void Panel_System::setSchemeLabel(const char *text) {
    if (_lbl_scheme && text) lv_label_set_text(_lbl_scheme, text);
}

void Panel_System::setSelectorLabel(const char *text) {
    if (_lbl_selector && text) lv_label_set_text(_lbl_selector, text);
}

void Panel_System::setBarLabel(const char *text) {
    if (_lbl_bar && text) lv_label_set_text(_lbl_bar, text);
}

void Panel_System::setColsLabel(const char *text) {
    if (_lbl_cols && text) lv_label_set_text(_lbl_cols, text);
}

void Panel_System::setRowsLabel(const char *text) {
    if (_lbl_rows && text) lv_label_set_text(_lbl_rows, text);
}

// Where the drawer hangs from. GUIManager calls this when the header is
// hidden or shown, because the drawer has to travel with the thing it is
// supposed to be attached to - otherwise hiding the bar leaves it floating a
// header's height down an empty screen.
void Panel_System::setTopOffset(int32_t y) {
    _drawer.top = y;
    if (_drawer.root) lv_obj_set_y(_drawer.root, y);
}

void Panel_System::setAnchorX(int32_t centreX) {
    Kit::drawerPlace(_drawer, centreX, _drawer.top);
}

void Panel_System::setVariantLabel(const char *text) {
    if (_lbl_variant && text) lv_label_set_text(_lbl_variant, text);
}

void Panel_System::setFillLabel(const char *text) {
    if (_lbl_fill && text) lv_label_set_text(_lbl_fill, text);
}

void Panel_System::setAreaLabel(const char *text) {
    if (_lbl_area && text) lv_label_set_text(_lbl_area, text);
}

void Panel_System::setLabelModeLabel(const char *text) {
    if (_lbl_label && text) lv_label_set_text(_lbl_label, text);
}

// One transparent flex row of buttons. Four of these existed as four
// copy-pasted blocks the moment #50 added rows 3 and 4, which is three too
// many places to change a gap.
static lv_obj_t *makeRow(lv_obj_t *parent) {
    lv_obj_t *r = lv_obj_create(parent);
    lv_obj_set_width              (r, lv_pct(100));
    lv_obj_set_height             (r, LV_SIZE_CONTENT);
    lv_obj_set_layout             (r, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow          (r, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align         (r, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER,
                                      LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_bg_opa       (r, LV_OPA_TRANSP, 0);
    lv_obj_set_style_pad_all      (r, 0, 0);
    lv_obj_set_style_border_width (r, 0, 0);
    lv_obj_set_style_pad_gap      (r, UIToolkit::sc(8), 0);
    lv_obj_clear_flag             (r, LV_OBJ_FLAG_SCROLLABLE);
    // The row is exactly its buttons' height, so it would clip their small
    // Linen shadow - see UI::unclipShadows().
    UI::unclipShadows(r);
    UI::tameScroll                (r);
    return r;
}

static lv_obj_t *knobButton(lv_obj_t *parent, Panel_System *self,
                            const char *text, lv_event_cb_t cb) {
    lv_obj_t *b = lv_button_create(parent);
    lv_obj_set_height             (b, UIToolkit::sc(32));
    lv_obj_set_width              (b, LV_SIZE_CONTENT);
    lv_obj_set_flex_grow          (b, 1);
    lv_obj_add_event_cb           (b, cb, LV_EVENT_CLICKED, self);
    // Shared paints, so a scheme change repaints them - #64.
    // Border colour AND width come from the paint, so they follow the scheme.
    lv_obj_add_style              (b, UI::paint(UIPaint::PAINT_SURFACE_ALT), 0);
    lv_obj_add_style              (b, UI::paint(UIPaint::PAINT_LIFT_SM), 0);

    lv_obj_t *l = lv_label_create(b);
    lv_label_set_text             (l, text);
    lv_obj_center                 (l);
    lv_obj_set_style_text_font    (l, UIToolkit::Font_Button, 0);
    lv_obj_add_style              (l, UI::paint(UIPaint::PAINT_TEXT), 0);
    return b;
}

void Panel_System::init(lv_obj_t* parent, Panel_Header* headerRef) {
    _headerRef = headerRef;

    // Receive the System Doctor's output. Registering rather than being
    // written into is what lets SystemReport stay LVGL-free.
    s_self = this;
    SystemReport::addSink(reportSink);

    // WIDTH IS DERIVED FROM THE SCREEN, NOT FROM THE BOARD (issue #50).
    //
    // It was lv_pct(100) - full bleed on every panel, which on a 1280 px board
    // is a wall of buttons for a drawer that holds eight of them. The owner's
    // rule, and his constraint was as important as the rule itself: "I just
    // want to avoid doing basically anything on a per-board one-off
    // condition." So there is no board macro here, and a ninth board gets a
    // sensible answer without anyone editing this file.
    //
    // LOGICAL pixels, not physical. This is a judgement about how much UI fits,
    // which is exactly what the scale factor exists to express - and the two
    // disagree sharply on the densest panels. WS_P4_5 is 1280 px across and
    // only 740 logical at 1.73x, so a physical test would file it with the
    // 7B (1024 physical, 1024 logical) when it actually has less room than a
    // 5-inch board. The fleet lands in four well-separated groups:
    //
    //   330 (CYD_S3_3248) | 480 (both 4B) | 727-740 (8048, S3_5B, P4_5) | 1024 (7B, 1060)
    //
    // The gaps are what make a threshold safe rather than lucky.
    const int32_t screenW = lv_obj_get_width(lv_screen_active());
    const int32_t logicalW = (int32_t)((float)screenW / UIToolkit::scale());

    int32_t panelW;
    if      (logicalW >= PANEL_WIDE_LOGICAL) panelW = screenW / 2;
    else if (logicalW >= PANEL_MID_LOGICAL)  panelW = (screenW * 3) / 4;
    else                                     panelW = screenW;   // no room to spare

    // THE FRAME IS THE KIT'S DRAWER (2.8 round 2, D-20): the same clip that
    // grows to reveal its face, the face square where it meets the bar and
    // riding the growing edge (#81), the scheme's shadow on the clip, no
    // clip_corner (the layer that froze the P4_5 - LESSONS) - which is what
    // this panel built by hand before the kit existed, and what every other
    // slot's panel is. The face is the rows' column.
    //
    // It hangs under the status slot (GUIManager: setAnchorX()), kept on the
    // screen with the deck's own margin; Y COMES FROM THE LIVE HEADER HEIGHT
    // (setTopOffset()), not from a constant - a stale number once detached it
    // from the bar (#50).
    Kit::drawer(_drawer, parent, panelW);
    _ui_content = _drawer.face;
    Kit::drawerPlace(_drawer, screenW, UIToolkit::systemHeaderPx());

    // -- TITLE: centred, with a rule running out to each edge ---------------
    //
    // Replaces "System Ready." - which was a status line from the Phase 1 UI
    // that had long since stopped reporting anything, and read as a leftover
    // rather than a heading. The owner: "an archaic throwback that has no
    // place on the panel today."
    //
    // A flex row of [rule][label][rule] rather than a label with a background:
    // the rules grow to fill whatever the label does not, so it stays centred
    // on every board without anyone computing a width.
    lv_obj_t *titleRow = makeRow(_ui_content);
    lv_obj_set_flex_align(titleRow, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER,
                                    LV_FLEX_ALIGN_CENTER);

    auto rule = [&](void) {
        lv_obj_t *r = lv_obj_create(titleRow);
        lv_obj_remove_style_all      (r);
        lv_obj_set_height            (r, UIToolkit::sc(1) < 1 ? 1 : UIToolkit::sc(1));
        lv_obj_set_flex_grow         (r, 1);
        lv_obj_set_style_bg_opa      (r, LV_OPA_50, 0);
        lv_obj_add_style             (r, UI::paint(UIPaint::PAINT_ACCENT_BG), 0);
        return r;
    };

    rule();
    lbl_stats = lv_label_create     (titleRow);
    lv_label_set_text               (lbl_stats, "SYSTEM  -  DIAGNOSTICS");
    lv_obj_add_style                (lbl_stats, UI::paint(UIPaint::PAINT_ACCENT_TEXT), 0);
    lv_obj_set_style_text_font      (lbl_stats, UIToolkit::Font_Label, 0);
    lv_obj_set_style_pad_left       (lbl_stats, UIToolkit::sc(10), 0);
    lv_obj_set_style_pad_right      (lbl_stats, UIToolkit::sc(10), 0);
    rule();

    // -- ROW 2: Actions --
    _ui_actions = lv_obj_create     (_ui_content);
    lv_obj_set_width                (_ui_actions, lv_pct(100));
    lv_obj_set_height               (_ui_actions, LV_SIZE_CONTENT);
    lv_obj_set_layout               (_ui_actions, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow            (_ui_actions, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align           (_ui_actions, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_bg_opa         (_ui_actions, LV_OPA_TRANSP, 0);
    lv_obj_set_style_pad_all        (_ui_actions, 0, 0);
    lv_obj_set_style_border_width   (_ui_actions, 0, 0);
    lv_obj_set_style_pad_gap        (_ui_actions, UIToolkit::sc(8), 0);
    lv_obj_clear_flag               (_ui_actions, LV_OBJ_FLAG_SCROLLABLE);
    UI::tameScroll                  (_ui_actions);

    // ROW 1 is Log / Tokens / Cards / About, per #50 and #97. "Dump Config"
    // has LEFT this panel - the Log page carries its own Dump button, which
    // re-runs the report and redraws it in place, so the copy here only ever
    // ran the report at a screen nobody was looking at. The same knob buttons
    // as the rows below (they were three copies of knobButton() written out).
    //
    // Log - the System Doctor's output, on its own page now.
    knobButton(_ui_actions, this, "Log", [](lv_event_t *e) {
        Panel_System *p = (Panel_System *)lv_event_get_user_data(e);
        if (p) p->requestLog();
    });

    // Tokens - opens the 2.2 reference page as its own screen. Lives here
    // rather than behind a build flag so the schemes can be flipped and
    // compared without reflashing, and so the page survives into the Phase 4
    // settings surface instead of being throwaway scaffolding.
    knobButton(_ui_actions, this, "Tokens", [](lv_event_t *e) {
        Panel_System *p = (Panel_System *)lv_event_get_user_data(e);
        if (p) p->requestTokens();
    });

    // Cards - opens the 2.4 card demo as its own screen. The dashboard
    // underneath stays untouched, so a card layout can be judged without
    // disturbing anything that already works. Through a registered callback,
    // because the page it opens needs the entity registry and the card binder
    // and this panel must not know about either.
    knobButton(_ui_actions, this, "Cards", [](lv_event_t *e) {
        Panel_System *p = (Panel_System *)lv_event_get_user_data(e);
        if (p) p->requestCards();
    });

    // About - "About this board", a sheet over everything (K74, #97). It
    // needs nothing this panel does not have, so it is opened directly; the
    // drawer stays as it was underneath and is there again when it closes.
    knobButton(_ui_actions, this, "About", [](lv_event_t *e) {
        (void)e;
        AboutSheet::open();
    });

    // Glyphs - the header's glyph style, bare / square / circle (2.8, #19's
    // open question): a button so the three can be compared on glass. The bar
    // itself shows which is on.
    knobButton(_ui_actions, this, "Glyphs", [](lv_event_t *e) {
        Panel_System *p = (Panel_System *)lv_event_get_user_data(e);
        if (p) p->requestGrid(Panel_System::GridAction::GLYPH_CYCLE);
    });

    // -- ROWS 2-4: the knobs ----------------------------------------------
    //
    // Four rows rather than one, and the reason is physical: eight flex-grown
    // buttons on one row are each about a finger-width too narrow to hit on
    // CYD_S3_3248 at 320 px - and every other board's panel is now NARROWER
    // than it was, so the argument got stronger rather than weaker.
    _ui_grid = makeRow(_ui_content);   // Col -/+, Row -/+
    _ui_row3 = makeRow(_ui_content);   // Deck, Compact, Theme, Bar
    _ui_row4 = makeRow(_ui_content);   // card header mode, Fill, Area

    // COUNTS, not nudges. These used to move a target width and an aspect
    // ceiling and hope the arithmetic landed somewhere useful; now they demand
    // a column and row count outright and the layout obeys. Each wraps through
    // "A" for auto. The labels are updated by GUIManager, which owns the state.
    knobButton(_ui_grid, this, "Col -", [](lv_event_t *e) {
        Panel_System *p = (Panel_System *)lv_event_get_user_data(e);
        if (p) p->requestGrid(Panel_System::GridAction::CARD_W_DOWN);
    });
    lv_obj_t *btnCols = knobButton(_ui_grid, this, "Col A", [](lv_event_t *e) {
        Panel_System *p = (Panel_System *)lv_event_get_user_data(e);
        if (p) p->requestGrid(Panel_System::GridAction::CARD_W_UP);
    });
    _lbl_cols = lv_obj_get_child(btnCols, 0);

    knobButton(_ui_grid, this, "Row -", [](lv_event_t *e) {
        Panel_System *p = (Panel_System *)lv_event_get_user_data(e);
        if (p) p->requestGrid(Panel_System::GridAction::ASPECT_DOWN);
    });
    lv_obj_t *btnRows = knobButton(_ui_grid, this, "Row A", [](lv_event_t *e) {
        Panel_System *p = (Panel_System *)lv_event_get_user_data(e);
        if (p) p->requestGrid(Panel_System::GridAction::ASPECT_UP);
    });
    _lbl_rows = lv_obj_get_child(btnRows, 0);
    // --- ROW 3 -----------------------------------------------------------
    knobButton(_ui_row3, this, "Deck", [](lv_event_t *e) {
        Panel_System *p = (Panel_System *)lv_event_get_user_data(e);
        if (p) p->requestGrid(Panel_System::GridAction::DECK_TOGGLE);
    });

    // Manual variant override. VAR_AUTO measures the cell and decides, which
    // is right and is also the thing you want to argue with when judging a
    // layout - docs/design/card-layout.md records the threshold being guessed
    // wrong twice before it was measured. This is how you check the
    // measurement by eye instead of reflashing.
    // Labelled for GUIManager's default, which is compact since 2.7.
    lv_obj_t *btnVar = knobButton(_ui_row3, this, "Cmpct", [](lv_event_t *e) {
        Panel_System *p = (Panel_System *)lv_event_get_user_data(e);
        if (p) p->requestGrid(Panel_System::GridAction::VARIANT_CYCLE);
    });
    _lbl_variant = lv_obj_get_child(btnVar, 0);

    // Header mode, on the LIVE dashboard rather than on the bench. All three
    // modes ship - cards.md treats this as "what a card looks like when nobody
    // chose", not an elimination - so this is the owner picking a default by
    // looking at it, and it is the same control the device settings page will
    // eventually own.
    // --- ROW 4 -----------------------------------------------------------
    lv_obj_t *btnHdr = knobButton(_ui_row4, this, "Tag", [](lv_event_t *e) {
        Panel_System *p = (Panel_System *)lv_event_get_user_data(e);
        if (p) p->requestGrid(Panel_System::GridAction::HDR_CYCLE);
    });
    _lbl_hdr = lv_obj_get_child(btnHdr, 0);

    // How an ACTIVE StateCard reads - a filled surface, or a tinted icon on
    // the normal one. Already a static on StateCard; it just had no control.
    lv_obj_t *btnFill = knobButton(_ui_row4, this, "Fill", [](lv_event_t *e) {
        Panel_System *p = (Panel_System *)lv_event_get_user_data(e);
        if (p) p->requestGrid(Panel_System::GridAction::FILL_CYCLE);
    });
    _lbl_fill = lv_obj_get_child(btnFill, 0);

    // Area on or off for every card. Card::setShowAreaDefault() has existed
    // since 2.4 and was only reachable from a build sheet.
    lv_obj_t *btnArea = knobButton(_ui_row4, this, "Area", [](lv_event_t *e) {
        Panel_System *p = (Panel_System *)lv_event_get_user_data(e);
        if (p) p->requestGrid(Panel_System::GridAction::AREA_TOGGLE);
    });
    _lbl_area = lv_obj_get_child(btnArea, 0);

    // What a state card says under its hero: name, the state word, or nothing.
    // 2.7, cards.md section 13. A control so the three can be compared on
    // glass rather than by rebuilding.
    lv_obj_t *btnLabel = knobButton(_ui_row4, this, "Name", [](lv_event_t *e) {
        Panel_System *p = (Panel_System *)lv_event_get_user_data(e);
        if (p) p->requestGrid(Panel_System::GridAction::LABEL_CYCLE);
    });
    _lbl_label = lv_obj_get_child(btnLabel, 0);

    // Scheme, on the dashboard rather than on the reference page. The owner
    // asked for it, and it is also the fix for WHY he was on that page: the
    // only reason to open Tokens during normal use was to change the scheme.
    lv_obj_t *btnScheme = knobButton(_ui_row3, this, "Fleet", [](lv_event_t *e) {
        Panel_System *p = (Panel_System *)lv_event_get_user_data(e);
        if (p) p->requestScheme();
    });
    _lbl_scheme = lv_obj_get_child(btnScheme, 0);

    // The card window's Selector look for the scheme showing (2.10d, K45),
    // beside the scheme it belongs to: the scheme's own, then the four looks.
    // A fourth button in this row, for now - the drawer is a stand-in for the
    // device's settings page (4.1).
    lv_obj_t *btnSel = knobButton(_ui_row3, this, "Sel Own", [](lv_event_t *e) {
        Panel_System *p = (Panel_System *)lv_event_get_user_data(e);
        if (p) p->requestSelector();
    });
    _lbl_selector = lv_obj_get_child(btnSel, 0);

    // System header bar height: 50 -> 45 -> 40 -> 35 -> 30 -> none -> 50.
    // Hide / show the header. NOT a size cycle - the owner fixed the height at
    // 35 on 2026-09-19 ("35 is now the permanent header bar size") and the
    // cycle had a trap in it: one of its six positions was "gone", and from
    // there nothing could bring the bar back.
    lv_obj_t *btnBar = knobButton(_ui_row3, this, "Hide Bar", [](lv_event_t *e) {
        Panel_System *p = (Panel_System *)lv_event_get_user_data(e);
        if (p) p->requestGrid(Panel_System::GridAction::BAR_CYCLE);
    });
    _lbl_bar = lv_obj_get_child(btnBar, 0);

    // -- ROW 3: Log Container --
    // THE LOG BOX IS GONE FROM THIS PANEL - see UI/LogPage.h.
    //
    // A scrollable child inside an accordion is re-laid-out and re-clipped on
    // every frame of the height animation, which is why this panel has always
    // been choppy while the deck Audio/Display panels - same animation, no
    // nested scroller - are smooth. The report now lives on its own screen,
    // reached by the "Log" button, and this panel is a plain box again.

    // The Col/Row knobs go to the BOTTOM. Owner's layout, 2026-09-19: "the
    // first 3 rows each have 3 buttons and the bottom row is the only row with
    // 4". Reordered by index rather than by moving their construction, the
    // same call the Log button uses above.
    lv_obj_move_to_index(_ui_grid, lv_obj_get_child_count(_ui_content) - 1);

    _ui_timer = lv_timer_create     (_ui_timer_cb, 50, this); // 50ms for faster log flushing

    // One line, once, at start-up - and it is not decoration.
    //
    // Three of #50's four changes are geometry that is only visible once a
    // finger has opened the drawer, which makes them exactly the kind of thing
    // that ships wrong and is noticed a week later. Printing what was computed
    // means a board can be checked from the serial log at boot: a width that
    // is not half or three-quarters of the screen, an x that does not put the
    // right edge one margin in, or a height of 0 or "most of the screen" are
    // all obvious here and all invisible until tapped.
    lv_obj_update_layout(_ui_content);
    Serial.printf("[SysPanel] %ld px wide (screen %ld, %ld logical), top %ld, face %ld px\n",
                  (long)panelW, (long)screenW, (long)logicalW,
                  (long)UIToolkit::systemHeaderPx(), (long)lv_obj_get_height(_ui_content));
}

// Open or close. How tall it opens is MEASURED from its face, never guessed
// (#50: it once opened to most of the screen whatever it held) - the drawer's
// own rule, Kit::drawerOpen(), as every slot's panel.
void Panel_System::toggle() {
    _expanded = !_expanded;
    if (_onToggle) _onToggle(_expanded);
    if (_expanded) {
        UIToolkit::closeActiveAccordion();
        Kit::drawerOpen(_drawer);
    } else {
        Kit::drawerClose(_drawer, true);
    }
}

void Panel_System::close() {
    if (_expanded) toggle();
}

void Panel_System::log(const char* fmt, ...) {
    char buf[256];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);

    // No Serial mirroring here. SystemReport owns that decision now, and
    // doing it in both places would double every report line.

    // Push to the queue, bounded by bytes (see _log_queue_bytes). A line that
    // does not fit is counted, and the count is reported in the log itself
    // when the queue drains - a cut that says so, not a silent one.
    const size_t len = strlen(buf) + 1;
    if (_log_queue_bytes + len <= LOG_TAIL_MAX) {
        _log_queue.push_back(std::string(buf));
        _log_queue_bytes += len;
    } else if (_log_dropped < UINT16_MAX) {
        _log_dropped++;
    }
    _log_dirty = true;
}

void Panel_System::updateSystemStats(float voltage, float current, int wifi_rssi) {
    _batt_volts = voltage;
    _batt_amps = current;
    _rssi = wifi_rssi;
    _stats_dirty = true;
}

void Panel_System::_ui_timer_cb(lv_timer_t* timer) {
    Panel_System* p = (Panel_System*)lv_timer_get_user_data(timer);
    if(p) p->_tick();
}

void Panel_System::_tick() {
    // Process LOG Queue
    if (_log_dirty) {
        // Drained into a STRING, not into a label. No LVGL work happens here
        // at all now, which is the point: this used to insert text into a
        // widget five lines per tick whether or not anyone was looking at it.
        int processed = 0;
        while (!_log_queue.empty() && processed < 5) {
            _log_queue_bytes -= _log_queue.front().size() + 1;
            _log_text += _log_queue.front();
            _log_text += "\n";
            _log_queue.erase(_log_queue.begin());
            processed++;
        }
        if (_log_queue.empty() && _log_dropped) {
            char note[64];
            snprintf(note, sizeof(note), "(log: %u lines dropped - queue full)\n",
                     (unsigned)_log_dropped);
            _log_text += note;
            _log_dropped = 0;
        }

        // A TAIL, TRIMMED FROM THE FRONT - not a bucket that empties itself.
        //
        // This used to be `if (size > 4000) _log_text = "(log trimmed)"`, which
        // threw the whole log away the moment it filled. The owner met that as:
        // two dumps fit, the third is cut off mid-report, and a fourth is
        // needed to read anything. The report has grown a great deal since 4 KB
        // was chosen and one dump is now a sizeable fraction of it.
        //
        // Dropping whole lines off the FRONT keeps the newest output intact -
        // the half anyone actually wants - and never truncates the dump you
        // just asked for.
        if (_log_text.size() > LOG_TAIL_MAX) {
            size_t cut = _log_text.size() - LOG_TAIL_MAX;
            // Round forward to a line boundary so the tail never opens
            // mid-sentence.
            size_t nl = _log_text.find('\n', cut);
            _log_text.erase(0, (nl == std::string::npos) ? cut : nl + 1);
        }

        if (_log_queue.empty() && !_log_dropped) _log_dirty = false;

        // REDRAW WHILE THE PAGE IS OPEN.
        //
        // Two things at once. It fixes the owner's bug - tapping Dump from the
        // log page did nothing visible, because LogPage::refresh() ran
        // immediately after SystemReport::run() while every one of those lines
        // was still sitting in _log_queue, unappended. The page was faithfully
        // redrawing the text from BEFORE the dump.
        //
        // And it restores what he liked about the panel's first incarnation:
        // the log scrolling upward in real time as output arrives, five lines a
        // tick, instead of appearing all at once when you leave and come back.
        if (_log_on_change) _log_on_change(_log_text.c_str());
    }

    if (_stats_dirty && lbl_stats) {
        lv_label_set_text_fmt(lbl_stats, "Bat: %.2fV  %.0fmA  |  WiFi: %d dBm", 
                              _batt_volts, _batt_amps, _rssi);
        _stats_dirty = false;
    }
}