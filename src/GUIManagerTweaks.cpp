// GUIManager - UI Tweaks (2.8 round 3, D-22 / D-25 / D-27). Split from
// GUIManager.cpp as the header's half was: what each of UI Tweaks' rows sets
// and where it is saved, the scheme and the grid's counts (the system panel's),
// the system panel's CONNECTIVITY words, and the deck rising under the window.
// The window itself is UI/TweaksSheet.
#include "GUIManager.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "Settings.h"
#include "Cards/CardIcons.h"     // cardLabelMode(), cardSetLabelMode()
#include "Cards/CardPopup.h"
#include "UI/Sheet.h"            // windowCloseMs(), sheetBeside()
#include "UI/TweaksSheet.h"
#include "UI/UIToolkit.h"

// --- Saved by name (Settings.h) -------------------------------------------------
// system.ui.<key>; each table in its enum's order, so an index is the enum.
static const char *const SCHEME_NAMES[] = { "fleet", "midnight", "linen" };   // UITokens' order
static const char *const HDR_NAMES[]    = { "none", "band", "tag", "float" };  // CardHeaderStyle
static const char *const VAR_NAMES[]    = { "auto", "full", "compact" };       // CardVariant
static const char *const FILL_NAMES[]   = { "fill", "glow" };                  // StateCardFill
constexpr uint8_t SCHEME_N = sizeof(SCHEME_NAMES) / sizeof(SCHEME_NAMES[0]);

static bool readName(const char *key, const char *const *names, uint8_t n, uint8_t &out) {
    char v[16];
    if (!Settings::system("ui", key, v, sizeof(v))) return false;
    for (uint8_t i = 0; i < n; i++)
        if (!strcmp(v, names[i])) { out = i; return true; }
    return false;
}

// A choice equal to how the board shipped is saved as nothing.
static void saveName(const char *key, const char *const *names, uint8_t i, uint8_t ship) {
    Settings::setSystem("ui", key, i == ship ? nullptr : names[i]);
    Settings::save();
}

// HOW THE BOARD SHIPPED: the working set before any saved choice is laid over
// it (applyUiSettings()'s first call, at boot), and the scheme begin() starts
// in. Reset all puts the looks back to it; a grid count equal to it is saved
// as nothing.
static struct {
    bool            taken = false;
    uint8_t         scheme = 0xFF;
    CardHeaderStyle hdr;
    CardVariant     variant;
    StateCardFill   fill;
    CardLabel       label;
    uint8_t         cols, rows;
} s_ship;

// --- The scheme (the system panel's, D-22) ----------------------------------------
void GUIManager::applySavedScheme() {
    if (s_ship.scheme == 0xFF) s_ship.scheme = UI::schemeIndex();
    uint8_t i;
    if (readName("scheme", SCHEME_NAMES, SCHEME_N, i)) UI::setSchemeIndex(i);
}

// DEVICE-WIDE since 2.8 round 3: every page takes it, and it is saved. A
// rebuild rather than a restyle - a METRICS change moves radii and padding,
// which a card reads when it is built.
void GUIManager::chooseScheme(uint8_t i, bool save) {
    if (i >= SCHEME_N) return;
    const bool changed = UI::schemeIndex() != i;
    UI::setSchemeIndex(i);
    everyPage([i](PageLive &p) { p.scheme = i; });
    if (save) saveName("scheme", SCHEME_NAMES, i, s_ship.scheme);
    _pnlSystem.setScheme(i);
    if (!changed) return;
    applyGround();
    _header.restyle();
    rebuildDashboard();
    Serial.printf("[UI] scheme -> %s\n", UI::pal().name);
}

// --- The grid's counts (the system panel's Col / Row) -------------------------------
// PER PAGE, as they always were (owner, 2026-09-23: "a page built for a fixed
// set of cards needs its own arrangement") - and saved with the page now.
void GUIManager::saveGrid() {
    UI::setColumnsOverride(_colsOverride);
    if (_curPage < _nPages && _pages[_curPage].spec) {
        PageLive &p = _pages[_curPage];
        p.cols = _colsOverride;
        p.rows = _rowsOverride;
        char c[6], r[6];
        snprintf(c, sizeof(c), "%u", (unsigned)_colsOverride);
        snprintf(r, sizeof(r), "%u", (unsigned)_rowsOverride);
        Settings::setPage(p.spec->slug, "cols", _colsOverride == s_ship.cols ? nullptr : _colsOverride ? c : "auto");
        Settings::setPage(p.spec->slug, "rows", _rowsOverride == s_ship.rows ? nullptr : _rowsOverride ? r : "auto");
        Settings::save();
    }
    rebuildDashboard();
    Serial.printf("[UI] grid -> %u x %u (0: auto)\n", (unsigned)_colsOverride, (unsigned)_rowsOverride);
}

// --- The card looks (UI Tweaks) ----------------------------------------------------
void GUIManager::applyUiSettings() {
    if (!s_ship.taken) {
        s_ship.taken   = true;
        s_ship.hdr     = _hdr;
        s_ship.variant = _variant;
        s_ship.fill    = _fill;
        s_ship.label   = cardLabelMode();
        s_ship.cols    = _colsOverride;
        s_ship.rows    = _rowsOverride;
    }
    uint8_t i;
    _hdr     = readName("label",   HDR_NAMES,  4, i) ? (CardHeaderStyle)i : s_ship.hdr;
    _variant = readName("density", VAR_NAMES,  3, i) ? (CardVariant)i     : s_ship.variant;
    _fill    = readName("state",   FILL_NAMES, 2, i) ? (StateCardFill)i   : s_ship.fill;
    CardLabel l = s_ship.label;
    char v[16];
    if (Settings::system("ui", "entity", v, sizeof(v)) && !cardLabelFromName(v, l)) l = s_ship.label;
    if (l == CardLabel::LBL_INHERIT) l = s_ship.label;   // the fleet's has nothing to inherit from
    StateCard::setFill(_fill);
    cardSetLabelMode(l);
    everyPage([this, l](PageLive &p) { p.hdr = _hdr; p.variant = _variant; p.fill = _fill; p.label = l; });
}

void GUIManager::setCardLabelStyle(CardHeaderStyle s) {
    _hdr = s;
    everyPage([s](PageLive &p) { p.hdr = s; });
    saveName("label", HDR_NAMES, (uint8_t)s, (uint8_t)s_ship.hdr);
    rebuildDashboard();   // a header bar is made when a card is built
    Serial.printf("[UI] card label -> %s\n", cardHeaderName(s));
}

void GUIManager::setEntityLabel(CardLabel l) {
    cardSetLabelMode(l);
    everyPage([l](PageLive &p) { p.label = l; });
    Settings::setSystem("ui", "entity", l == s_ship.label ? nullptr : cardLabelName(l));
    Settings::save();
    _binder.restyleAll();  // resolved in render()
    Serial.printf("[UI] entity label -> %s\n", cardLabelName(l));
}

void GUIManager::setCardState(StateCardFill f) {
    _fill = f;
    StateCard::setFill(f);
    everyPage([f](PageLive &p) { p.fill = f; });
    saveName("state", FILL_NAMES, (uint8_t)f, (uint8_t)s_ship.fill);
    _binder.restyleAll();  // a live style: render() reads it
    Serial.printf("[UI] card state -> %s\n", FILL_NAMES[(uint8_t)f]);
}

void GUIManager::setDensity(CardVariant v) {
    _variant = v;
    everyPage([v](PageLive &p) { p.variant = v; });
    saveName("density", VAR_NAMES, (uint8_t)v, (uint8_t)s_ship.variant);
    rebuildDashboard();   // a card decides compact or full when it is built
    Serial.printf("[UI] card density -> %s\n", VAR_NAMES[(uint8_t)v]);
}

void GUIManager::setGlyphs(GlyphStyle g) {
    _header.setGlyphStyle(g);
    // Saved by name; the default (square, D-20) as nothing.
    Settings::setSystem("header", "glyphs", g == GLYPH_DEFAULT ? nullptr : glyphStyleName(g));
    Settings::save();
    Serial.printf("[UI] header glyphs -> %s\n", glyphStyleName(g));
}

// Every UI Tweaks choice back to how the board shipped: the looks, the glyphs,
// every scheme's selector, the window timeout and its bar. Not the scheme or
// the grid - those are the system panel's.
void GUIManager::resetUiTweaks() {
    static const char *const KEYS[] = { "label", "entity", "state", "density" };
    for (const char *k : KEYS) Settings::setSystem("ui", k, nullptr);
    Settings::setSystem("header", "glyphs", nullptr);
    Settings::setSystem("window", "close", nullptr);
    Settings::setSystem("window", "bar", nullptr);
    for (const char *s : SCHEME_NAMES) Settings::setScheme(s, "selector", nullptr);
    Kit::forgetSelectorLooks();
    Settings::save();
    applyUiSettings();
    _header.setGlyphStyle(savedGlyphStyle());
    rebuildDashboard();
    Serial.println("[UI] UI Tweaks reset");
}

// --- The system panel's CONNECTIVITY, in words (D-22) ----------------------------------
void GUIManager::connectivityWords(char *wifi, char *ha, char *mqtt, size_t n) {
    ConnectivityManager &c = _core.conn();
    char ssid[33] = "";
    c.getSsid(ssid, sizeof(ssid));
    const ConnState cs = c.getState();
    if (cs == ConnState::STA_CONNECTED || cs == ConnState::APSTA) {
        const IPAddress ip = c.getIP();
        snprintf(wifi, n, "%s, %d dBm, %u.%u.%u.%u%s", ssid[0] ? ssid : "-", (int)c.getRssi(),
                 ip[0], ip[1], ip[2], ip[3], cs == ConnState::APSTA ? ", AP on" : "");
    } else if (cs == ConnState::STA_CONNECTING) {
        if (c.getAttempt() > 1) snprintf(wifi, n, "Joining %s, try %u", ssid, (unsigned)c.getAttempt());
        else                    snprintf(wifi, n, "Joining %s", ssid);
    } else if (cs == ConnState::AP_ACTIVE) {
        snprintf(wifi, n, "Access point %s", c.getApSsid());
    } else if (cs == ConnState::DEGRADED) {
        snprintf(wifi, n, "Not connected");
    } else if (cs == ConnState::RADIO_OFF) {
        snprintf(wifi, n, "Off");
    } else {
        snprintf(wifi, n, "Starting");
    }

    EntityRegistry &reg = _core.entities();
    unsigned nHa = 0;
    for (uint8_t i = 0; i < reg.count(); i++)
        if (const Entity *e = reg.at(i)) if (e->desc.source == EntitySource::HA) nHa++;
    switch (_core.ha().state()) {
        case HaLink::LINK_READY:   snprintf(ha, n, "Connected, %u entities", nHa); break;
        case HaLink::LINK_OPENING:
        case HaLink::LINK_AUTHING: snprintf(ha, n, "Connecting"); break;
        case HaLink::LINK_IDLE:    snprintf(ha, n, "Waiting for the network"); break;
        case HaLink::LINK_BACKOFF: snprintf(ha, n, "Lost, retrying"); break;
        case HaLink::LINK_REFUSED: snprintf(ha, n, "Token refused"); break;
        default:                   snprintf(ha, n, "Off"); break;
    }

    MqttManager &q = _core.mqtt();
    switch (q.getState()) {
        case MqttState::CONNECTED: {
            const uint32_t ms = q.msSinceLastInbound();
            if (ms == UINT32_MAX) snprintf(mqtt, n, "Connected");   // nothing subscribed has spoken yet
            else if (ms < 60000)  snprintf(mqtt, n, "Connected, heard %lu s ago", (unsigned long)(ms / 1000));
            else                  snprintf(mqtt, n, "Connected, heard %lu min ago", (unsigned long)(ms / 60000));
            break;
        }
        case MqttState::CONNECTING:   snprintf(mqtt, n, "Connecting"); break;
        case MqttState::NO_LINK:      snprintf(mqtt, n, "Waiting for the network"); break;
        case MqttState::BACKOFF:      snprintf(mqtt, n, "Retrying in %lu s", (unsigned long)q.secondsUntilRetry()); break;
        case MqttState::AUTH_STOPPED: snprintf(mqtt, n, "Login refused"); break;
        default:                      snprintf(mqtt, n, "Off"); break;
    }
}

// --- UI Tweaks' rows, and the deck under the window ----------------------------------------
// A friend of GUIManager: each row reads and sets what it shows through the
// one GUIManager, and the deck is GUIManager's.
struct GUITweaks {
    static GUIManager *g;

    // The words, in the order the rows show them.
    static constexpr const char *GLYPHS[]  = { "Bare", "Square", "Circle" };
    static constexpr const char *LABEL[]   = { "Float", "Band", "Tag", "None" };
    static constexpr const char *ENTITY[]  = { "Name (HA)", "State", "Custom", "None" };
    static constexpr const char *STATE[]   = { "Fill", "Icon glow" };
    static constexpr const char *METAL[]   = { "Scheme's", "Black", "Silver" };
    static constexpr const char *SHAPE[]   = { "Square", "Round" };
    static constexpr const char *DENSITY[] = { "Compact", "Full", "Auto" };
    static constexpr const char *TIMEOUT[] = { "60 s", "30 s", "10 s", "Off" };
    static constexpr const char *ONOFF[]   = { "On", "Off" };

    // ...and what each position is.
    static constexpr CardHeaderStyle LABEL_IS[]   = { CardHeaderStyle::HDR_TAG_FLOAT, CardHeaderStyle::HDR_BAND,
                                                      CardHeaderStyle::HDR_TAG, CardHeaderStyle::HDR_NONE };
    static constexpr CardLabel       ENTITY_IS[]  = { CardLabel::LBL_HA, CardLabel::LBL_STATE,
                                                      CardLabel::LBL_NAME, CardLabel::LBL_NONE };
    static constexpr StateCardFill   STATE_IS[]   = { StateCardFill::FILL_SURFACE, StateCardFill::LIGHT_ICON };
    static constexpr CardVariant     DENSITY_IS[] = { CardVariant::VAR_COMPACT, CardVariant::VAR_FULL,
                                                      CardVariant::VAR_AUTO };
    static constexpr const char     *TIMEOUT_IS[] = { nullptr, "30", "10", "off" };   // nothing: 60 s (D6)

    template <typename T, size_t N> static uint8_t posOf(const T (&is)[N], T v) {
        for (uint8_t i = 0; i < N; i++) if (is[i] == v) return i;
        return 0;
    }

    static uint8_t glyphsNow()          { return g ? (uint8_t)g->_header.glyphStyle() : 0; }
    static void    glyphsSet(uint8_t i) { if (g) g->setGlyphs((GlyphStyle)i); }
    static uint8_t labelNow()           { return g ? posOf(LABEL_IS, g->_hdr) : 0; }
    static void    labelSet(uint8_t i)  { if (g && i < 4) g->setCardLabelStyle(LABEL_IS[i]); }
    static uint8_t entityNow()          { return posOf(ENTITY_IS, cardLabelMode()); }
    static void    entitySet(uint8_t i) { if (g && i < 4) g->setEntityLabel(ENTITY_IS[i]); }
    static uint8_t stateNow()           { return g ? posOf(STATE_IS, g->_fill) : 0; }
    static void    stateSet(uint8_t i)  { if (g && i < 2) g->setCardState(STATE_IS[i]); }
    static uint8_t densityNow()         { return g ? posOf(DENSITY_IS, g->_variant) : 0; }
    static void    densitySet(uint8_t i){ if (g && i < 3) g->setDensity(DENSITY_IS[i]); }

    // THE SELECTOR'S LOOK, for the scheme showing (K45): its metal - the
    // scheme's own, or chosen - and its shape. Choosing a shape chooses the look.
    static uint8_t metalNow() {
        const int8_t l = Kit::selectorLook();
        return l < 0 ? 0 : (l & 2) ? 2 : 1;
    }
    static void metalSet(uint8_t i) {
        Kit::keepSelectorLook(i == 0 ? (int8_t)-1 : (int8_t)((Kit::selectorLookNow() & 1) | (i == 2 ? 2 : 0)));
        Settings::save();
    }
    static uint8_t shapeNow()          { return Kit::selectorLookNow() & 1; }
    static void    shapeSet(uint8_t i) {
        Kit::keepSelectorLook((int8_t)((Kit::selectorLookNow() & 2) | (i & 1)));
        Settings::save();
    }

    // How long a window waits, and its bar (T6): system.window.
    static uint8_t timeoutNow() {
        char v[8];
        if (!Settings::system("window", "close", v, sizeof(v))) return 0;
        for (uint8_t i = 1; i < 4; i++) if (!strcmp(v, TIMEOUT_IS[i])) return i;
        return 0;
    }
    static void timeoutSet(uint8_t i) {
        if (i >= 4) return;
        Settings::setSystem("window", "close", TIMEOUT_IS[i]);
        Settings::save();
    }
    static uint8_t barNow()          { return Kit::windowBarShown() ? 0 : 1; }
    static void    barSet(uint8_t i) { Settings::setSystem("window", "bar", i ? "off" : nullptr); Settings::save(); }

    static constexpr TweaksSheet::Row ROWS[] = {
        { "Glyphs",         GLYPHS,  3, glyphsNow,  glyphsSet,  false },
        { "Card label",     LABEL,   4, labelNow,   labelSet,   false },
        { "Entity label",   ENTITY,  4, entityNow,  entitySet,  false },
        { "Card state",     STATE,   2, stateNow,   stateSet,   false },
        { "Selector",       METAL,   3, metalNow,   metalSet,   true  },
        { "Selector shape", SHAPE,   2, shapeNow,   shapeSet,   true  },
        { "Card density",   DENSITY, 3, densityNow, densitySet, false },
        { "Window timeout", TIMEOUT, 4, timeoutNow, timeoutSet, false },
        { "Timer bar",      ONOFF,   2, barNow,     barSet,     false },
    };

    // THE DECK RISES UNDER THE WINDOW (D-25): its panels' headers along the
    // screen's foot, splitting the window's width as the card window's panels
    // do - the only way to the deck while its options wait to be rehomed. Only
    // as tall as an open panel needs while it is up, so bringing it forward
    // redraws that strip and not the screen; its foot stays where it was, so a
    // panel opens to the same height.
    struct Was { int32_t x, y, h, index; };
    static Was was;
    static bool besideOpen() { return UIToolkit::getActiveAccordionPanel() != nullptr; }
    static void besideFold() { UIToolkit::closeActiveAccordion(); }
    static Kit::SheetBeside beside;   // the deck's panels, let through the window's catcher

    static void slideY(void *o, int32_t v) { lv_obj_set_y((lv_obj_t *)o, v); }
    static void slide(lv_obj_t *d, int32_t from, int32_t to, lv_anim_completed_cb_t done) {
        lv_anim_delete(d, slideY);
        lv_anim_t a;
        lv_anim_init(&a);
        lv_anim_set_var         (&a, d);
        lv_anim_set_values      (&a, from, to);
        lv_anim_set_duration    (&a, 220);
        lv_anim_set_path_cb     (&a, lv_anim_path_ease_out);
        lv_anim_set_exec_cb     (&a, slideY);
        if (done) lv_anim_set_completed_cb(&a, done);
        lv_anim_start(&a);
    }

    static void deckUp(const lv_area_t &win) {
        if (!g || !g->_deck) return;
        lv_obj_t *d = g->_deck;
        UIToolkit::closeActiveAccordion();
        lv_anim_delete(d, slideY);
        if (!g->_deckRaised) was = { lv_obj_get_x(d), lv_obj_get_y(d), lv_obj_get_height(d), lv_obj_get_index(d) };
        const int32_t pad   = UIToolkit::sc(UIToolkit::DECK_PAD);
        const int32_t foot  = was.y + was.h;   // below the screen by the overhang
        const int32_t h     = UIToolkit::sc(UIToolkit::PANEL_EXPANDED_H) + 2 * pad;
        const int32_t strip = UIToolkit::sc(UIToolkit::PANEL_HEADER_H) + pad;
        lv_obj_set_size(d, lv_area_get_width(&win) + 2 * pad, h);
        lv_obj_set_pos (d, win.x1 - pad, foot - h + strip);
        lv_obj_clear_flag(d, LV_OBJ_FLAG_HIDDEN);
        lv_obj_move_foreground(d);
        g->_deckRaised = true;
        beside.holder = d;
        Kit::sheetBeside(&beside);
        slide(d, foot - h + strip, foot - h, nullptr);
    }

    static void deckBack(lv_anim_t *) {
        if (!g || !g->_deck || g->_deckRaised) return;
        lv_obj_t *d = g->_deck;
        lv_obj_set_size(d, lv_pct(100), was.h);
        lv_obj_set_pos (d, was.x, was.y);
        lv_obj_move_to_index(d, was.index);
        if (!g->_showDeck) lv_obj_add_flag(d, LV_OBJ_FLAG_HIDDEN);
    }

    static void deckDown() {
        if (!g || !g->_deck || !g->_deckRaised) return;
        g->_deckRaised = false;
        UIToolkit::closeActiveAccordion();
        lv_obj_t *d = g->_deck;
        const int32_t strip = UIToolkit::sc(UIToolkit::PANEL_HEADER_H) + UIToolkit::sc(UIToolkit::DECK_PAD);
        slide(d, lv_obj_get_y(d), lv_obj_get_y(d) + strip, deckBack);
    }
};

GUIManager      *GUITweaks::g      = nullptr;
GUITweaks::Was   GUITweaks::was    = {};
Kit::SheetBeside GUITweaks::beside = { nullptr, GUITweaks::besideOpen, GUITweaks::besideFold };

void GUIManager::openUiTweaks() {
    if (Kit::sheetShowing()) return;
    if (_pnlSystem.isExpanded()) _pnlSystem.close();
    closeSlotPanel(false);
    GUITweaks::g = this;
    TweaksSheet::Spec sp = {};
    sp.rows     = GUITweaks::ROWS;
    sp.nRows    = sizeof(GUITweaks::ROWS) / sizeof(GUITweaks::ROWS[0]);
    sp.tokens   = []() { if (GUITweaks::g) GUITweaks::g->openTokens(); };
    sp.resetAll = []() { if (GUITweaks::g) GUITweaks::g->resetUiTweaks(); };
    sp.opened   = GUITweaks::deckUp;
    sp.closing  = GUITweaks::deckDown;
    if (TweaksSheet::open(sp)) Serial.println("[UI] UI Tweaks open");
}
