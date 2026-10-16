#pragma once
// ---------------------------------------------------------------------------
// THE SYSTEM PANEL - the status icons' panel (2.8, D-13, D-22, D-25).
//
//   CONNECTIVITY          Wi-Fi (its network, signal and address), Home
//                         Assistant, MQTT - each its live state, its glyph
//   SYSTEM - DIAGNOSTICS  Log, UI Tweaks, About; the scheme (Fleet / Midnight /
//                         Linen) and the Status bar toggle; Col - / + Row - / +
//
// Its frame is the kit's drawer (D-20), one shape with the status slot's tab
// (D-23). The knobs that used to fill it are UI Tweaks' (a window of its own,
// D-22); the deck is reached from there. Deep links to the Wi-Fi settings and
// the connectivity stats come with the settings pages (Q6, 4.1).
//
// It also BUFFERS THE SYSTEM DOCTOR'S LOG (a SystemReport sink) for LogPage,
// whether or not it is open.
// ---------------------------------------------------------------------------
#include <lvgl.h>
#include <Arduino.h>
#include <string>
#include <vector>
#include <functional>
#include "UI/Kit.h"           // Kit::Drawer, Kit::Selector
#include "UI/Panel_Header.h"
#include "UI/UIToolkit.h"
#include "UI/Widget_ConnStatus.h"
#include "UI/Widget_MqttStatus.h"
#include "SystemReport.h"

class Panel_System {
public:
    Panel_System();
    ~Panel_System();

    void init(lv_obj_t* parent, Panel_Header* headerRef);

    // Public logging - SAFE to call from anywhere (buffers data)
    void log(const char* fmt, ...);

    // What the Log page's Dump runs (GUIManager): the report, run afresh.
    using DumpCallback = std::function<void()>;
    void setOnDumpRequested(DumpCallback cb) { _onDumpRequested = cb; }

    // The accumulated report, as plain text. The panel BUFFERS the log; it no
    // longer displays it - LogPage does. Keeping the buffer here means the
    // boot-time dump is already waiting when the page is first opened.
    const char *logText() const { return _log_text.c_str(); }
    // Called whenever the drained log text changes, so an open LogPage can
    // redraw as output arrives rather than only when it is reopened.
    void setLogOnChange(std::function<void(const char *)> cb) { _log_on_change = cb; }
    // Empty the log and tell any listener. Every Dump runs this first, so the
    // log page holds exactly one report (GUIManager's dumpFresh).
    void clearLog() {
        _log_text.clear();
        _log_queue.clear();
        _log_queue_bytes = 0;
        _log_dropped = 0;
        _log_dirty = false;
        if (_log_on_change) _log_on_change(_log_text.c_str());
    }

    // --- What its buttons do: GUIManager's (the panel only knows a button was
    //     pressed) ---
    using Action = std::function<void()>;
    void setOnLogRequested(Action cb)      { _onLog = cb; }
    void setOnUiTweaksRequested(Action cb) { _onTweaks = cb; }
    // The scheme chosen: 0 Fleet, 1 Midnight, 2 Linen (UITokens' order).
    void setOnSchemeChosen(std::function<void(uint8_t)> cb) { _onScheme = cb; }
    void setScheme(uint8_t i);                   // shown as chosen (changed elsewhere)
    void setBarShown(bool shown);                // the Status bar toggle's state
    // Fire the above (public: the buttons' callbacks are plain functions).
    void requestLog();
    void requestTweaks();                        // puts the panel away first
    void chooseScheme(uint8_t i);

    // The 2.5 GRID KNOBS - counts, not nudges; "A" (auto) past the ends - and
    // the Status bar (this page's header, D-14). One callback carrying a code:
    // this panel is a remote control and knows nothing of what a column is.
    enum class GridAction : uint8_t { CARD_W_DOWN, CARD_W_UP, ASPECT_DOWN, ASPECT_UP, BAR_CYCLE };
    using GridCallback = std::function<void(GridAction)>;
    void setOnGridAction(GridCallback cb) { _onGridAction = cb; }
    void requestGrid(GridAction a) { if (_onGridAction) _onGridAction(a); }

    // CONNECTIVITY's sources: the two glyphs' managers, and its three lines'
    // words - Wi-Fi, Home Assistant, MQTT - asked once a second while it shows.
    void setLinks(ConnectivityManager *conn, MqttManager *mqtt) { _conn = conn; _mqtt = mqtt; }
    using ConnText = std::function<void(char *wifi, char *ha, char *mqtt, size_t n)>;
    void setConnectivityText(ConnText cb) { _connText = cb; }

    void setTopOffset(int32_t y);
    // THE STATUS SLOT'S PANEL (2.8, D-13): it hangs under its slot, centred on
    // `centreX` and kept on the screen; a Kit::Drawer like every other slot's
    // panel (D-20), one shape with its tab (D-23).
    void setAnchorX(int32_t centreX);
    // The status slot's tab [l, r), for the panel's flares (D-23); and where the
    // panel stands [l, r), for the tab's coves.
    void setTab(int32_t l, int32_t r) { _tabL = l; _tabR = r; }
    void panelSpan(int32_t &l, int32_t &r) const { l = _drawer.x; r = _drawer.x + _drawer.w; }

    void close();
    void toggle();

    using ToggleCallback = std::function<void(bool isOpen)>;
    void setOnToggleCallback(ToggleCallback cb) { _onToggle = cb; }

    lv_obj_t* getContainer() { return _drawer.root; }
    // For the swipe handler: a swipe up should put an open drawer away rather
    // than toggle anything behind it.
    bool isExpanded() const { return _expanded; }

private:
    // Width thresholds, in LOGICAL pixels: a judgement about readable panel
    // width (#50). The fleet's logical widths cluster at 330 / 480 / 727-740 /
    // 1024, so both sit in a wide gap.
    static constexpr int32_t PANEL_WIDE_LOGICAL = 700;   // and up -> half width
    static constexpr int32_t PANEL_MID_LOGICAL  = 400;   // and up -> three quarters

    void refreshConnectivity();
    static void reportSink(const char *line);

    Kit::Drawer _drawer;
    lv_obj_t *_ui_content = nullptr;                 // the drawer's face
    lv_obj_t *_connVal[3] = {};                      // Wi-Fi, HA, MQTT: their words
    lv_obj_t *_barBtn = nullptr;                     // Status bar
    Kit::Selector _schemeSel;
    Widget_ConnStatus _wifiGlyph;
    Widget_MqttStatus _linkGlyph;
    ConnectivityManager *_conn = nullptr;
    MqttManager         *_mqtt = nullptr;
    ConnText _connText;
    uint32_t _connMs = 0;
    bool     _barShown = true;

    Panel_Header* _headerRef = nullptr;
    bool _expanded = false;
    int32_t _tabL = -1, _tabR = -1;
    ToggleCallback _onToggle;
    Action _onLog, _onTweaks;
    std::function<void(uint8_t)> _onScheme;
    GridCallback _onGridAction;
    DumpCallback _onDumpRequested;

    // -- Safe Data Buffering --
    std::vector<std::string> _log_queue;
    std::string              _log_text;   // what LogPage renders
    // How much log to keep, trimmed from the FRONT: a diagnostic tail, not a
    // history (at 4 KB a third dump was cut off mid-report).
    static constexpr size_t  LOG_TAIL_MAX = 12000;
    // The queue between log() and the tick that drains it is capped by BYTES,
    // at the same budget as the text it feeds (a cap of 100 LINES silently
    // dropped the tail of every dump once the report passed 100 lines).
    size_t   _log_queue_bytes = 0;
    uint16_t _log_dropped     = 0;   // lines refused since the last drain, reported once
    std::function<void(const char *)> _log_on_change;
    bool _log_dirty = false;

    lv_timer_t* _ui_timer = nullptr;
    static void _ui_timer_cb(lv_timer_t* timer);
    void _tick();
};
