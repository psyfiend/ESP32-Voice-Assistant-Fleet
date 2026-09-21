#pragma once
#include <lvgl.h>
#include <Arduino.h>
#include <string>
#include <vector>
#include <functional> // Required for std::function
#include "UI/Panel_Header.h"
#include "UI/UIToolkit.h"
#include "SystemReport.h"

class Panel_System {
public:
    Panel_System();
    ~Panel_System();

    // Init needs the header to link the animation
    void init(lv_obj_t* parent, Panel_Header* headerRef);
    
    // Public logging - SAFE to call from anywhere (buffers data)
    void log(const char* fmt, ...);

    // What to run when the "Dump Config" button is pressed. Registered by
    // GUIManager. This replaces an `extern void debug_dump_config(bool)` that
    // reached from this UI file straight into main.
    using DumpCallback = std::function<void()>;
    void setOnDumpRequested(DumpCallback cb) { _onDumpRequested = cb; }

    // What to run when the "Cards" button is pressed. Registered by GUIManager
    // for the same reason the dump callback is: the card demo needs the entity
    // registry and the card binder, and this panel has no business knowing
    // either exists. Lower layers never reach up into UI, and UI files do not
    // reach sideways into each other's dependencies.
    using CardsCallback = std::function<void()>;
    void setOnCardsRequested(CardsCallback cb) { _onCardsRequested = cb; }

    // Same again for the design-token reference page. It used to call
    // ReferencePage::show() straight from the button, which meant a second
    // full screen was built while the dashboard was still alive - the exact
    // condition that exhausts LVGL's layer buffers and hangs the board. The
    // dashboard's owner has to be given the chance to stand down first.
    using TokensCallback = std::function<void()>;
    void setOnTokensRequested(TokensCallback cb) { _onTokensRequested = cb; }
    void requestTokens() { if (_onTokensRequested) _onTokensRequested(); }

    // Cycle the colour scheme without going near the reference page. The owner
    // asked for this directly, and it also removes the only reason to open
    // that page during normal use.
    using SchemeCallback = std::function<void()>;
    void setOnSchemeRequested(SchemeCallback cb) { _onSchemeRequested = cb; }
    void requestScheme() { if (_onSchemeRequested) _onSchemeRequested(); }
    void setSchemeLabel(const char *text);
    void setBarLabel(const char *text);

    // The accumulated report, as plain text. The panel BUFFERS the log; it no
    // longer displays it - LogPage does. Keeping the buffer here means the
    // boot-time dump is already waiting when the page is first opened.
    const char *logText() const { return _log_text.c_str(); }

    // Called whenever the drained log text changes, so an open LogPage can
    // redraw as output arrives rather than only when it is reopened.
    void setLogOnChange(std::function<void(const char *)> cb) { _log_on_change = cb; }

    // Empty the log and tell any listener. The "Clear log" button, so a fresh
    // dump is not stacked underneath the previous three.
    void clearLog() {
        _log_text.clear();
        _log_queue.clear();
        _log_dirty = false;
        if (_log_on_change) _log_on_change(_log_text.c_str());
    }

    // What the "Log" button runs. Registered by GUIManager, which stands the
    // dashboard down before the page opens.
    using LogCallback = std::function<void()>;
    void setOnLogRequested(LogCallback cb) { _onLogRequested = cb; }
    void requestLog() { if (_onLogRequested) _onLogRequested(); }

    // The accumulated report, as plain text. The panel BUFFERS the log; it no
    // longer displays it - LogPage does. Keeping the buffer here means the
    // boot-time dump is already waiting when the page is first opened.

    // What the "Log" button runs. Registered by GUIManager, which stands the
    // dashboard down before the page opens.
    void setColsLabel(const char *text);
    void setRowsLabel(const char *text);
    void setTopOffset(int32_t y);
    void setVariantLabel(const char *text);
    void setFillLabel(const char *text);
    void setAreaLabel(const char *text);

    // Fires the above. Public because a capture-less lv_event_cb lambda is a
    // free function, not a member, so it cannot reach a private field.
    void requestCards() { if (_onCardsRequested) _onCardsRequested(); }

    // The 2.5 GRID KNOBS.
    //
    // They live here rather than on the card bench deliberately: this drawer
    // opens OVER the real dashboard, so what is being tuned is the page the
    // device actually boots into. The bench derives its grid from a different
    // host - a screen minus a button bar - and a number tuned there would be
    // right for the bench and wrong for the dashboard.
    //
    // One callback carrying a code rather than five callbacks, because this
    // panel is a remote control: it knows a button was pressed and nothing at
    // all about what a column is.
    enum class GridAction : uint8_t {
        CARD_W_DOWN, CARD_W_UP,    // TARGET_CARD_W - decides COLUMNS
        ASPECT_DOWN, ASPECT_UP,    // ASPECT_PCT    - decides ROWS
        DECK_TOGGLE,               // the deck costs a row on every board
        HDR_CYCLE,                 // bar / tag / none, on the live dashboard
        BAR_CYCLE,                 // the SYSTEM header bar height
        // Added with the #50 rework. All three drive statics that already
        // existed on the card layer - Card::setShowAreaDefault(),
        // StateCard::setFill() and the per-card variant - so these are new
        // CONTROLS over settled behaviour rather than new behaviour.
        VARIANT_CYCLE,             // auto / full / compact, overriding the measurement
        FILL_CYCLE,                // how an active StateCard reads: fill or icon
        AREA_TOGGLE,               // show the area on every card, or not
    };
    using GridCallback = std::function<void(GridAction)>;
    void setOnGridAction(GridCallback cb) { _onGridAction = cb; }
    void requestGrid(GridAction a) { if (_onGridAction) _onGridAction(a); }

    // The header-mode button says which mode is live. Set by GUIManager, which
    // owns the choice - this panel only knows a button was pressed.
    void setHeaderLabel(const char *text);

    // Update system stats - SAFE to call from anywhere
    void updateSystemStats(float voltage, float current, int wifi_rssi);

    // Toggle the drawer
    void close(); 
    void toggle();

    // Callback registration
    using ToggleCallback = std::function<void(bool isOpen)>;
    void setOnToggleCallback(ToggleCallback cb);
    
    // Returns the main wrapper (for parenting checks if needed)
    lv_obj_t* getContainer() { return _ui_root; }
    // For the 2.6 swipe handler: a swipe up should put an open drawer away
    // rather than toggle the deck behind it.
    bool isExpanded() const { return _expanded; }

private:
    // Measured, not guessed - see the definition. Const because opening the
    // drawer must not be able to change what is in it.
    int32_t contentHeight() const;

    // Width thresholds, in LOGICAL pixels. Named rather than inline because
    // they are a judgement about readable panel width and the only way to know
    // they are right is to look at one. The fleet's logical widths cluster at
    // 330 / 480 / 727-740 / 1024, so both sit in a wide gap.
    static constexpr int32_t PANEL_WIDE_LOGICAL = 700;   // and up -> half width
    static constexpr int32_t PANEL_MID_LOGICAL  = 400;   // and up -> three quarters

public:

private:
    // -- UI Objects --
    lv_obj_t* _ui_root;    // The Outer Wrapper (Animates Height, No Padding)
    lv_obj_t* _ui_content; // The Inner Container (Has Padding & Style, Auto Height)
    // Four button rows (issue #50). Splitting them is not decoration: eight
    // flex-grown buttons on one row are each a finger-width too narrow on
    // CYD_S3_3248, and the panel is now narrower on every board but that one.
    lv_obj_t* _ui_actions; // row 1 - Log / Tokens / Cards
    lv_obj_t* _ui_grid;    // row 2 - Col -/+, Row -/+
    lv_obj_t* _ui_row3;    // row 3 - Deck / Compact / Theme
    lv_obj_t* _ui_row4;    // row 4 - header mode / Fill / Area
    lv_obj_t* _lbl_variant;
    lv_obj_t* _lbl_fill;
    lv_obj_t* _lbl_area;
    lv_obj_t* _lbl_hdr;    // label inside the header-mode button
    lv_obj_t* _lbl_scheme; // label inside the scheme button
    lv_obj_t* _lbl_bar;    // label inside the header-size button
    lv_obj_t* _lbl_cols;
    lv_obj_t* _lbl_rows;
    
    lv_obj_t* lbl_stats;   // The stats header label

    Panel_Header* _headerRef;
    
    // -- Logic --
    bool _expanded;
    ToggleCallback _onToggle;

    // -- Animation --
    static void anim_height_cb(void * var, int32_t v);

    // -- Callbacks --
    static void btn_action_cb(lv_event_t* e);
    
    // -- Safe Data Buffering --
    std::vector<std::string> _log_queue;
    std::string              _log_text;   // what LogPage renders

    // How much log to keep, trimmed from the FRONT. Raised from the original
    // 4,000 because the System Doctor's report has grown a lot since that
    // number was picked - at 4 KB a third dump was being cut off mid-report.
    // This is a diagnostic tail, not a history, so it is still bounded.
    static constexpr size_t  LOG_TAIL_MAX = 12000;

    std::function<void(const char *)> _log_on_change;
    bool _log_dirty;
    DumpCallback  _onDumpRequested  = nullptr;
    CardsCallback _onCardsRequested = nullptr;
    GridCallback  _onGridAction     = nullptr;
    TokensCallback _onTokensRequested = nullptr;
    LogCallback    _onLogRequested    = nullptr;
    SchemeCallback _onSchemeRequested = nullptr;

    // Sink registered with SystemReport in init(), so report lines land in this
    // panel. Serial mirroring is SystemReport's job now, not log()'s.
    static void reportSink(const char *line);
    
    float _batt_volts = 0;
    float _batt_amps = 0;
    int _rssi = 0;
    bool _stats_dirty;

    // -- Timer for UI Updates --
    lv_timer_t* _ui_timer;
    static void _ui_timer_cb(lv_timer_t* timer);
    void _tick(); // Instance method called by timer
};