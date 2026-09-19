#include "UI/Panel_System.h"
#include "UI/ReferencePage.h"
#include "UI/UITokens.h"

// The single System panel, so the SystemReport sink (a plain function pointer)
// can reach it. One panel exists by construction.
static Panel_System *s_self = nullptr;

Panel_System::Panel_System() {
    _ui_root    = NULL;
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

void Panel_System::anim_height_cb(void * var, int32_t v) {
    Panel_System* p = (Panel_System*)var;
    if (!p || !p->_ui_root) return; 
    lv_obj_set_height(p->_ui_root, v);
}

void Panel_System::btn_action_cb(lv_event_t* e) {
    Panel_System* p = (Panel_System*)lv_event_get_user_data(e);
    
    if (p) {
        p->log("> Action: Dump Config...");
        if (p->_onDumpRequested) p->_onDumpRequested();
    }
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
    if (_ui_root) lv_obj_set_y(_ui_root, y);
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
    lv_obj_set_style_bg_color     (b, UI::c(UI::pal().SURFACE_ALT), 0);
    lv_obj_set_style_border_width (b, 1, 0);
    lv_obj_set_style_border_color (b, UI::border(), 0);

    lv_obj_t *l = lv_label_create(b);
    lv_label_set_text             (l, text);
    lv_obj_center                 (l);
    lv_obj_set_style_text_font    (l, UIToolkit::Font_Button, 0);
    lv_obj_set_style_text_color   (l, UI::c(UI::pal().TEXT), 0);
    return b;
}

void Panel_System::init(lv_obj_t* parent, Panel_Header* headerRef) {
    _headerRef = headerRef;

    // Receive the System Doctor's output. Registering rather than being
    // written into is what lets SystemReport stay LVGL-free.
    s_self = this;
    SystemReport::addSink(reportSink);

    // 1. Create the WRAPPER (_ui_root)
    // Acts as the "Viewmask". Positioned explicitly below the header.
    //
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

    // ANCHORED RIGHT, with the deck's own margin rather than hard against the
    // edge - so it reads as coming from the status icon that opened it, and
    // lines up with the Audio/Display panels below. GUIManager pads the deck
    // by sc(10); matching that is what makes the two look like one system.
    const int32_t margin = (panelW == screenW) ? 0 : UIToolkit::sc(10);

    // Y COMES FROM THE LIVE HEADER HEIGHT, not from a constant.
    //
    // This was sc(30) hard-coded while the header is a knob that cycles
    // 50/45/40/35/30/none. At any height but 30 the drawer detached from the
    // bar and appeared out of nothing, which is the "fix the slide origin"
    // item in #50 - it was never a slide bug, it was a stale number.
    _ui_root = lv_obj_create    (parent);
    lv_obj_set_width            (_ui_root, panelW);
    lv_obj_set_height           (_ui_root, 0);
    lv_obj_set_pos              (_ui_root, screenW - panelW - margin,
                                           UIToolkit::systemHeaderPx());

    // Wrapper Style (Invisible, Clipping)
    lv_obj_set_style_bg_opa         (_ui_root, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width   (_ui_root, 0, 0);
    lv_obj_set_style_pad_all        (_ui_root, 0, 0); 
    lv_obj_set_style_radius         (_ui_root, 0, 0);
    lv_obj_set_scrollbar_mode       (_ui_root, LV_SCROLLBAR_MODE_OFF);
    // clip_corner was set here and has been REMOVED. It did nothing - this
    // object's radius is 0, and clip_corner only masks a radius - but it is
    // one of the three styles that force LVGL to render an object to an
    // intermediate layer sized by its WIDTH, and this was the widest object on
    // the screen. That is the exact shape that froze WS_P4_5 on boot at the
    // end of 2.5 (docs/LESSONS.md). Harmless today, a trap for whoever gives
    // this panel a radius later.

    // 2. Create the CONTENT CONTAINER (_ui_content)
    _ui_content = lv_obj_create     (_ui_root);
    // SIZED BY ITS CONTENT, not by the wrapper (issue #50, dynamic height).
    //
    // It was lv_pct(100) of a wrapper whose height was animated, which meant
    // the content could never say how tall it wanted to be - so the wrapper
    // had to guess, and it guessed "most of the screen". Reversing the
    // dependency is the whole fix: the content measures itself, and the
    // wrapper animates to that. See contentHeight().
    lv_obj_set_width                (_ui_content, lv_pct(100));
    lv_obj_set_height               (_ui_content, LV_SIZE_CONTENT);

    // Content Style
    lv_obj_set_style_bg_color       (_ui_content, UI::c(UI::pal().SURFACE), 0);
    
    // -- CORNER HACK --
    // We want square top corners (to connect to header) and rounded bottom corners.
    // LVGL radius applies to all corners.
    // Trick: Move content UP by the radius amount to clip the top rounded corners off.
    int32_t radius = UIToolkit::sc(15);
    lv_obj_set_style_radius     (_ui_content, UIToolkit::sc(15), 0);
    lv_obj_set_y                (_ui_content, -(UIToolkit::sc(15))); // Shift up to hide top curves
    
    // The shift has to be PAID FOR in top padding, or it eats the first row.
    //
    // Shifting up by `radius` hides the top corners; it also moves the first
    // child up by the same amount, straight under the wrapper's clip. Adding
    // the radius back as padding puts the content where it was and makes the
    // measured height honest - which now matters, because that height is what
    // the drawer animates to.
    lv_obj_set_style_pad_top        (_ui_content, radius + UIToolkit::sc(10), 0);

    lv_obj_set_style_border_color   (_ui_content, UI::border(), 0);
    lv_obj_set_style_border_width   (_ui_content, UIToolkit::sc(2), 0);
    // lv_obj_set_style_pad_all        (_ui_content, UIToolkit::sc(10), 0);
    lv_obj_set_style_pad_left       (_ui_content, UIToolkit::sc(10), 0);
    lv_obj_set_style_pad_right      (_ui_content, UIToolkit::sc(10), 0);
    lv_obj_set_style_pad_bottom     (_ui_content, UIToolkit::sc(10), 0);
    lv_obj_clear_flag               (_ui_content, LV_OBJ_FLAG_SCROLLABLE); // Static background

    lv_obj_set_flex_flow            (_ui_content, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align           (_ui_content, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_row        (_ui_content, UIToolkit::sc(8), 0);

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
        lv_obj_set_style_bg_color    (r, UI::c(UI::pal().ACCENT), 0);
        return r;
    };

    rule();
    lbl_stats = lv_label_create     (titleRow);
    lv_label_set_text               (lbl_stats, "SYSTEM  -  DIAGNOSTICS");
    lv_obj_set_style_text_color     (lbl_stats, UI::c(UI::pal().ACCENT), 0);
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

    // ROW 1 is Log / Tokens / Cards, per #50. "Dump Config" has LEFT this
    // panel - the Log page carries its own Dump button, which re-runs the
    // report and redraws it in place, so the copy here only ever ran the
    // report at a screen nobody was looking at.
    //
    // Button: Design Tokens - opens the 2.2 reference page as its own screen.
    // Lives here rather than behind a build flag so the schemes can be flipped
    // and compared without reflashing, and so the page survives into the Phase
    // 4 settings surface instead of being throwaway scaffolding.
    lv_obj_t* btnRef = lv_button_create(_ui_actions);
    lv_obj_set_height               (btnRef, UIToolkit::sc(32));
    lv_obj_set_width                (btnRef, LV_SIZE_CONTENT);
    lv_obj_set_flex_grow            (btnRef, 1);
    lv_obj_add_event_cb             (btnRef, [](lv_event_t *e) {
                                        Panel_System *self = (Panel_System *)lv_event_get_user_data(e);
                                        if (self) self->requestTokens();
                                     }, LV_EVENT_CLICKED, this);
    lv_obj_set_style_bg_color       (btnRef, UI::c(UI::pal().SURFACE_ALT), 0);
    lv_obj_set_style_border_width   (btnRef, 1, 0);
    lv_obj_set_style_border_color   (btnRef, UI::border(), 0);

    lv_obj_t* lblRef = lv_label_create(btnRef);
    lv_label_set_text               (lblRef, "Tokens");
    lv_obj_center                   (lblRef);
    lv_obj_set_style_text_font      (lblRef, UIToolkit::Font_Button, 0);
    lv_obj_set_style_text_color     (lblRef, UI::c(UI::pal().TEXT), 0);

    // Button: Log - the System Doctor's output, on its own page now.
    lv_obj_t* btnLog = lv_button_create(_ui_actions);
    lv_obj_set_height               (btnLog, UIToolkit::sc(32));
    lv_obj_set_width                (btnLog, LV_SIZE_CONTENT);
    lv_obj_set_flex_grow            (btnLog, 1);
    lv_obj_add_event_cb             (btnLog, [](lv_event_t *e) {
                                        Panel_System *self = (Panel_System *)lv_event_get_user_data(e);
                                        if (self) self->requestLog();
                                     }, LV_EVENT_CLICKED, this);
    lv_obj_set_style_bg_color       (btnLog, UI::c(UI::pal().SURFACE_ALT), 0);
    lv_obj_set_style_border_width   (btnLog, 1, 0);
    lv_obj_set_style_border_color   (btnLog, UI::border(), 0);
    lv_obj_t* lblLog = lv_label_create(btnLog);
    lv_label_set_text               (lblLog, "Log");
    lv_obj_center                   (lblLog);
    lv_obj_set_style_text_font      (lblLog, UIToolkit::Font_Button, 0);
    lv_obj_set_style_text_color     (lblLog, UI::c(UI::pal().TEXT), 0);

    // Button: Cards - opens the 2.4 card demo as its own screen.
    //
    // Same shape as Tokens above and for the same reason: the dashboard
    // underneath stays untouched, so a card layout can be judged without
    // disturbing anything that already works. Unlike Tokens it goes through a
    // registered callback, because the page it opens needs the entity registry
    // and the card binder and this panel must not know about either.
    lv_obj_t* btnCards = lv_button_create(_ui_actions);
    lv_obj_set_height               (btnCards, UIToolkit::sc(32));
    lv_obj_set_width                (btnCards, LV_SIZE_CONTENT);
    lv_obj_set_flex_grow            (btnCards, 1);
    lv_obj_add_event_cb             (btnCards, [](lv_event_t *e) {
                                        Panel_System *self = (Panel_System *)lv_event_get_user_data(e);
                                        if (self) self->requestCards();
                                     }, LV_EVENT_CLICKED, this);
    lv_obj_set_style_bg_color       (btnCards, UI::c(UI::pal().SURFACE_ALT), 0);
    lv_obj_set_style_border_width   (btnCards, 1, 0);
    lv_obj_set_style_border_color   (btnCards, UI::border(), 0);

    lv_obj_t* lblCards = lv_label_create(btnCards);
    lv_label_set_text               (lblCards, "Cards");
    lv_obj_center                   (lblCards);
    lv_obj_set_style_text_font      (lblCards, UIToolkit::Font_Button, 0);
    lv_obj_set_style_text_color     (lblCards, UI::c(UI::pal().TEXT), 0);

    // Source order is Tokens, Log, Cards; #50 asks for Log first. Reordered by
    // index rather than by moving ninety lines of button construction around,
    // which is the same call GUIManager uses for the screen's z-order.
    lv_obj_move_to_index            (btnLog, 0);

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
    lv_obj_t *btnVar = knobButton(_ui_row3, this, "Auto", [](lv_event_t *e) {
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

    // Scheme, on the dashboard rather than on the reference page. The owner
    // asked for it, and it is also the fix for WHY he was on that page: the
    // only reason to open Tokens during normal use was to change the scheme.
    lv_obj_t *btnScheme = knobButton(_ui_row3, this, "Fleet", [](lv_event_t *e) {
        Panel_System *p = (Panel_System *)lv_event_get_user_data(e);
        if (p) p->requestScheme();
    });
    _lbl_scheme = lv_obj_get_child(btnScheme, 0);

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
    Serial.printf("[SysPanel] %ld px wide at x=%ld (screen %ld, %ld logical), "
                  "top %ld, content %ld px\n",
                  (long)panelW, (long)(screenW - panelW - margin), (long)screenW,
                  (long)logicalW, (long)UIToolkit::systemHeaderPx(),
                  (long)contentHeight());
}

// How tall the drawer needs to be to show everything in it, and no taller.
//
// It used to open to `screen_h - sc(50) - sc(100)` - most of the display,
// whatever was actually inside. Two magic numbers standing in for a header
// this panel no longer has to guess at and a gap nobody could name.
//
// The measurement works because _ui_content is LV_SIZE_CONTENT and is NOT
// sized from its parent, so it has a real height even while the wrapper around
// it is collapsed to zero. lv_obj_update_layout() is normally something to
// avoid - docs/design/card-layout.md 1.1 calls it out for walking the whole
// screen - but that warning is about calling it in a RENDER path, once per
// card per repaint. This is one call on one subtree when a finger hits a
// button, which is the case the function is for.
//
// The radius comes back off because _ui_content is shifted up by exactly that
// much to square off its top corners against the header bar (see the corner
// hack in init). Its visible height is therefore its own height minus the part
// hidden above the wrapper.
int32_t Panel_System::contentHeight() const {
    if (!_ui_content) return 0;
    lv_obj_update_layout(_ui_content);
    const int32_t h = lv_obj_get_height(_ui_content) - UIToolkit::sc(15);

    // Never taller than the screen below the header. A board small enough for
    // the content not to fit still has to stop somewhere, and stopping at the
    // bottom edge is better than drawing past it.
    const int32_t avail = lv_obj_get_height(lv_screen_active())
                        - UIToolkit::systemHeaderPx() - UIToolkit::sc(10);
    return (h > avail) ? avail : h;
}

void Panel_System::toggle() {
    _expanded = !_expanded;
    if (_onToggle) _onToggle(_expanded);
    if (_expanded) UIToolkit::closeActiveAccordion();
    
    int32_t start_h = lv_obj_get_height(_ui_root);
    int32_t end_h   = _expanded ? contentHeight() : 0;

    lv_anim_del(this, anim_height_cb); 

    lv_anim_t a;
    lv_anim_init        (&a);
    lv_anim_set_var     (&a, this); 
    lv_anim_set_values  (&a, start_h, end_h);
    lv_anim_set_time    (&a, 350); 
    lv_anim_set_exec_cb (&a, anim_height_cb);
    lv_anim_set_path_cb (&a, lv_anim_path_ease_out); 
    lv_anim_start       (&a);
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

    // Push to Queue
    if (_log_queue.size() < 100) { // Limit queue depth
        _log_queue.push_back(std::string(buf));
        _log_dirty = true;
    }
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
            _log_text += _log_queue.front();
            _log_text += "\n";
            _log_queue.erase(_log_queue.begin());
            processed++;
        }

        // Same 4 KB ceiling as before and for the same reason: this is a
        // diagnostic tail, not a history.
        if (_log_text.size() > 4000) _log_text = "(log trimmed)\n";

        if (_log_queue.empty()) _log_dirty = false;
    }

    if (_stats_dirty && lbl_stats) {
        lv_label_set_text_fmt(lbl_stats, "Bat: %.2fV  %.0fmA  |  WiFi: %d dBm", 
                              _batt_volts, _batt_amps, _rssi);
        _stats_dirty = false;
    }
}