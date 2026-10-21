// The system panel - the status icons' panel (2.8 round 3). See
// include/UI/Panel_System.h.
#include "UI/Panel_System.h"
#include "UI/AboutSheet.h"
#include "UI/UIIcons.h"          // MDI_HOME_ASSISTANT
#include "UI/UITokens.h"

// The single System panel, so the SystemReport sink and the buttons' callbacks
// (plain function pointers) can reach it. One panel exists by construction.
static Panel_System *s_self = nullptr;

static constexpr uint32_t CONN_MS = 1000;   // CONNECTIVITY's words, while it shows

Panel_System::Panel_System() {}

Panel_System::~Panel_System() {
    if (_ui_timer) lv_timer_delete(_ui_timer);
}

void Panel_System::reportSink(const char *line) {
    if (s_self) s_self->log("%s", line);
}

// Where the drawer hangs from. GUIManager calls this when the header is
// hidden or shown, because the drawer has to travel with the thing it is
// supposed to be attached to - otherwise hiding the bar leaves it floating a
// header's height down an empty screen.
void Panel_System::setTopOffset(int32_t y) {
    Kit::drawerSetTop(_drawer, y);
}

void Panel_System::setAnchorX(int32_t centreX) {
    Kit::drawerPlace(_drawer, centreX, _drawer.top);
}

void Panel_System::setScheme(uint8_t i) {
    Kit::selectorSet(_schemeSel, i);
}


// The buttons: capture-less callbacks, the panel through s_self.
static void logCb(lv_event_t *)    { if (s_self && s_self->isExpanded()) s_self->close(); if (s_self) s_self->requestLog(); }
static void tweaksCb(lv_event_t *) { if (s_self) s_self->requestTweaks(); }
static void aboutCb(lv_event_t *)  { AboutSheet::open(); }
static void colDownCb(lv_event_t *) { if (s_self) s_self->requestGrid(Panel_System::GridAction::CARD_W_DOWN); }
static void colUpCb(lv_event_t *)   { if (s_self) s_self->requestGrid(Panel_System::GridAction::CARD_W_UP); }
static void rowDownCb(lv_event_t *) { if (s_self) s_self->requestGrid(Panel_System::GridAction::ASPECT_DOWN); }
static void rowUpCb(lv_event_t *)   { if (s_self) s_self->requestGrid(Panel_System::GridAction::ASPECT_UP); }
static void gridAutoCb(lv_event_t *) { if (s_self) s_self->requestGrid(Panel_System::GridAction::GRID_AUTO); }
static void schemeChosen(uint8_t i) { if (s_self) s_self->chooseScheme(i); }

void Panel_System::init(lv_obj_t* parent, Panel_Header* headerRef) {
    _headerRef = headerRef;

    // Receive the System Doctor's output. Registering rather than being
    // written into is what lets SystemReport stay LVGL-free.
    s_self = this;
    SystemReport::addSink(reportSink);

    // WIDTH IS DERIVED FROM THE SCREEN, NOT FROM THE BOARD (issue #50): half a
    // wide screen, three quarters of a middling one, all of a narrow one - in
    // LOGICAL pixels, a judgement about how much fits. The fleet lands in four
    // well-separated groups: 330 (CYD_S3_3248) | 480 (both 4B) | 727-740
    // (8048, S3_5B, P4_5) | 1024 (7B, 1060).
    const int32_t screenW  = lv_obj_get_width(lv_screen_active());
    const int32_t logicalW = (int32_t)((float)screenW / UIToolkit::scale());
    int32_t panelW;
    if      (logicalW >= PANEL_WIDE_LOGICAL) panelW = screenW / 2;
    else if (logicalW >= PANEL_MID_LOGICAL)  panelW = (screenW * 3) / 4;
    else                                     panelW = screenW;

    // THE FRAME IS THE KIT'S DRAWER (D-20): the clip that grows, the face
    // riding its edge (#81), one shape with the status slot's tab (D-23). It
    // hangs under the status slot (setAnchorX()); its top is the live header's
    // (setTopOffset() - a stale number once detached it from the bar, #50).
    Kit::drawer(_drawer, parent, panelW);
    _drawer.onClosed   = [](void *u) { Panel_System *p = (Panel_System *)u; if (p->_onClosed) p->_onClosed(); };
    _drawer.closedUser = this;
    _ui_content = _drawer.face;
    Kit::drawerPlace(_drawer, screenW, UIToolkit::systemHeaderPx());
    buildContent();

    _ui_timer = lv_timer_create(_ui_timer_cb, 50, this);   // 50 ms for faster log flushing

    lv_obj_update_layout(_ui_content);
    Serial.printf("[SysPanel] %ld px wide (screen %ld, %ld logical), top %ld, face %ld px\n",
                  (long)panelW, (long)screenW, (long)logicalW,
                  (long)UIToolkit::systemHeaderPx(), (long)lv_obj_get_height(_ui_content));
}

// THE ROWS ARE BUILT AGAIN whenever the panel opens and whenever the scheme
// changes. Their buttons and the selector are finished faces (#99), and a face
// lives in a cache of 16 slots that each new window may take back: built once,
// at boot, the panel's buttons went on pointing at slots UI Tweaks' selectors
// had repainted (round 3: doubled faces, Col / Row turned silver). And a face
// is painted in the scheme it was made in (Linen once showed Midnight's dark
// buttons). The panel is closed whenever a window opens, so building at open
// is enough.
void Panel_System::rebuild() {
    if (!_ui_content) return;
    lv_obj_clean(_ui_content);
    for (lv_obj_t *&v : _connVal) v = nullptr;
    _gridVal = nullptr;
    _schemeSel = Kit::Selector{};
    buildContent();
}

// From the next timer pass: the scheme may have been chosen on this panel's own
// selector, mid-glide.
void Panel_System::restyle() {
    lv_async_call([](void *p) { ((Panel_System *)p)->rebuild(); }, this);
}

void Panel_System::buildContent() {
    const int32_t panelW = _drawer.w;
    const int32_t gap   = UI::sc(UI::hdr().DRAWER_GAP);
    const int32_t inner = panelW - 2 * UI::sc(UI::hdr().DRAWER_PAD);
    const int32_t btnH  = Kit::controlH();

    // --- CONNECTIVITY: each link, its glyph and its state in words ---------
    Kit::drawerTitle(_ui_content, "CONNECTIVITY");
    lv_obj_t *room = nullptr;
    _connVal[0] = Kit::statusRow(_ui_content, "Wi-Fi", &room);
    if (_conn) _wifiGlyph.init(room, _conn, GlyphStyle::GLYPH_BARE);
    _connVal[1] = Kit::statusRow(_ui_content, "Home Assistant", &room);
    lv_label_set_text(Kit::label(room, Kit::KitStyle::KS_HDR_ICON), MDI_HOME_ASSISTANT);
    _connVal[2] = Kit::statusRow(_ui_content, "MQTT", &room);
    if (_mqtt) _linkGlyph.init(room, _mqtt, GlyphStyle::GLYPH_BARE);

    // --- SYSTEM - DIAGNOSTICS: development tools for now (D-17) ------------
    Kit::drawerTitle(_ui_content, "SYSTEM - DIAGNOSTICS");
    lv_obj_t *r = Kit::drawerRow(_ui_content);
    const int32_t w3 = (inner - 2 * gap) / 3;
    Kit::button(r, Kit::ButtonRole::BTN_CANCEL, "Log", w3, btnH, logCb);
    Kit::button(r, Kit::ButtonRole::BTN_CANCEL, "UI Tweaks", w3, btnH, tweaksCb);
    Kit::button(r, Kit::ButtonRole::BTN_CANCEL, "About", w3, btnH, aboutCb);

    // "Scheme:" and one of three (round 4: the Status bar toggle went - the page
    // panel's Show / Hide is the one way to hide the header).
    r = Kit::drawerRow(_ui_content);
    lv_label_set_text(Kit::label(r, Kit::KitStyle::KS_LABEL_NAME_DIM), "Scheme:");
    static const char *const SCHEMES[] = { "Fleet", "Midnight", "Linen" };
    Kit::SelectorSpec sp = {};
    for (uint8_t i = 0; i < 3; i++) sp.glyph[i] = SCHEMES[i];
    sp.n = 3;
    sp.chosen = UI::schemeIndex() < 3 ? UI::schemeIndex() : 1;
    sp.compact = true;
    sp.room = inner - Kit::textW("Scheme:", UI::type().NAME) - gap;
    sp.onChosen = schemeChosen;
    Kit::selector(_schemeSel, r, sp);

    // The page's grid (2.11 round 9): pinned counts or the arranger's choice, in
    // words, and Auto to take the pins away; then the counts, each press a pin.
    const int32_t w4 = (inner - 3 * gap) / 4;
    r = Kit::drawerRow(_ui_content);
    lv_label_set_text(Kit::label(r, Kit::KitStyle::KS_LABEL_NAME_DIM), "Grid:");
    _gridVal = Kit::label(r, Kit::KitStyle::KS_LABEL_NAME);
    lv_label_set_long_mode(_gridVal, LV_LABEL_LONG_DOT);
    lv_obj_set_flex_grow(_gridVal, 1);
    Kit::button(r, Kit::ButtonRole::BTN_CANCEL, "Auto", w4, btnH, gridAutoCb);
    refreshGrid();
    r = Kit::drawerRow(_ui_content);
    Kit::button(r, Kit::ButtonRole::BTN_CANCEL, "Col -", w4, btnH, colDownCb);
    Kit::button(r, Kit::ButtonRole::BTN_CANCEL, "Col +", w4, btnH, colUpCb);
    Kit::button(r, Kit::ButtonRole::BTN_CANCEL, "Row -", w4, btnH, rowDownCb);
    Kit::button(r, Kit::ButtonRole::BTN_CANCEL, "Row +", w4, btnH, rowUpCb);
}

void Panel_System::chooseScheme(uint8_t i) {
    if (_onScheme) _onScheme(i);
}

void Panel_System::refreshGrid() {
    if (!_gridVal) return;
    char t[48] = "";
    if (_gridText) _gridText(t, sizeof(t));
    lv_label_set_text(_gridVal, t);
}

void Panel_System::requestLog()    { if (_onLog) _onLog(); }
void Panel_System::requestTweaks() { close(); if (_onTweaks) _onTweaks(); }

// CONNECTIVITY's words, once a second while the panel shows (the glyphs tick
// themselves).
void Panel_System::refreshConnectivity() {
    if (!_connText) return;
    char w[64] = "", h[64] = "", m[64] = "";
    _connText(w, h, m, sizeof(w));
    const char *v[3] = { w, h, m };
    for (uint8_t i = 0; i < 3; i++)
        if (_connVal[i] && strcmp(lv_label_get_text(_connVal[i]), v[i])) lv_label_set_text(_connVal[i], v[i]);
}

// Open or close. How tall it opens is MEASURED from its face, never guessed
// (#50: it once opened to most of the screen whatever it held) - the drawer's
// own rule, Kit::drawerOpen(), as every slot's panel.
void Panel_System::toggle() {
    _expanded = !_expanded;
    if (_onToggle) _onToggle(_expanded);
    if (_expanded) {
        UIToolkit::closeActiveAccordion();
        rebuild();   // fresh faces (above)
        refreshConnectivity();
        _connMs = millis();
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

void Panel_System::_ui_timer_cb(lv_timer_t* timer) {
    Panel_System* p = (Panel_System*)lv_timer_get_user_data(timer);
    if(p) p->_tick();
}

void Panel_System::_tick() {
    // CONNECTIVITY, live while it shows.
    if (_expanded) {
        _wifiGlyph.tick();
        _linkGlyph.tick();
        if (millis() - _connMs >= CONN_MS) { _connMs = millis(); refreshConnectivity(); }
    }

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

        // REDRAW WHILE THE PAGE IS OPEN: the log scrolling upward in real time
        // as output arrives, five lines a tick (and Dump from the log page
        // shows the report it just ran).
        if (_log_on_change) _log_on_change(_log_text.c_str());
    }
}
