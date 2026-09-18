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
    void setColsLabel(const char *text);
    void setRowsLabel(const char *text);

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

private:
    // -- UI Objects --
    lv_obj_t* _ui_root;    // The Outer Wrapper (Animates Height, No Padding)
    lv_obj_t* _ui_content; // The Inner Container (Has Padding & Style, Auto Height)
    lv_obj_t* _ui_actions; // Button Row
    lv_obj_t* _ui_grid;    // Second button row - the 2.5 grid knobs
    lv_obj_t* _lbl_hdr;    // label inside the header-mode button
    lv_obj_t* _lbl_scheme; // label inside the scheme button
    lv_obj_t* _lbl_bar;    // label inside the header-size button
    lv_obj_t* _lbl_cols;
    lv_obj_t* _lbl_rows;
    
    lv_obj_t* txt_log;     // The log text label
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
    bool _log_dirty;
    DumpCallback  _onDumpRequested  = nullptr;
    CardsCallback _onCardsRequested = nullptr;
    GridCallback  _onGridAction     = nullptr;
    TokensCallback _onTokensRequested = nullptr;
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