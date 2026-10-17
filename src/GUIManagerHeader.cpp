// GUIManager - the header's slots and their panels (2.8). Split from
// GUIManager.cpp, which was already past the structure check's 1,000 lines;
// the class is one, its header-slot half is here. See UI/HeaderSlots.h for the
// slots, UI/SlotPanels.h for what their panels hold.
#include "GUIManager.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "Alarms.h"
#include "Settings.h"
#include "UI/SlotPanels.h"
#include "UI/TweaksSheet.h"
#include "UI/UIToolkit.h"

// A tap on a slot (Panel_Header's callback): the status slot's panel is the
// system panel; any other slot's is the slot drawer, built for it now. A second
// tap on the open slot puts its panel away.
void GUIManager::onSlotTap(uint8_t slot) {
    // The status glyphs and the AP mast share the system panel (its CONNECTIVITY).
    const SlotKind k = _header.slotKind(slot);
    if (k == SlotKind::SLOT_STATUS || k == SlotKind::SLOT_AP) { toggleSystemPanel(); return; }
    if (_slotPanel == (int8_t)slot) { closeSlotPanel(true); return; }
    if (_pnlSystem.isExpanded()) _pnlSystem.close();
    UIToolkit::closeActiveAccordion();
    openSlotPanel(slot);
}

void GUIManager::openSlotPanel(uint8_t slot) {
    if (!_slotDrawer.root) return;
    closeSlotPanel(false);
    lv_obj_clean(_slotDrawer.face);   // what the last panel held
    SlotPanels::Context c;
    for (uint8_t i = 0; i < _nPages && i < SlotPanels::PAGES_MAX; i++)
        c.pageTitles[i] = _pages[i].spec && _pages[i].spec->title ? _pages[i].spec->title : "Page";
    c.pageCount   = _nPages;
    c.pageIndex   = _curPage;
    c.headerShown = pageHeaderShown();   // the page's choice, whatever the bar shows meanwhile
    c.entity      = _header.slotEntity(slot);
    c.reg         = &_core.entities();
    c.user        = this;
    // The panel is put away first: what it showed belongs to the page or the
    // header that is about to change.
    c.goToPage = [](void *user, uint8_t p) {
        GUIManager *g = (GUIManager *)user;
        g->closeSlotPanel(false);
        g->goToPage(p);
    };
    // Show / Hide leaves the panel open (owner, round 4): the header is always
    // visible while a header panel is open, and the choice shows once the
    // panel has closed (headerPanelsClosed()).
    c.setHeaderShown = [](void *user, bool shown) {
        ((GUIManager *)user)->setPageHeaderShown(shown);
    };
    // The alarm's notice (D-21): dismissed, it leaves the bar with its panel;
    // its Settings opens the clock's panel, where the warning time is set.
    c.dismissWarning = [](void *user) {
        GUIManager *g = (GUIManager *)user;
        Alarms::dismissWarning();
        g->closeSlotPanel(true);
        g->_header.refresh();
    };
    c.openClock = [](void *user) {
        GUIManager *g = (GUIManager *)user;
        const int s = g->_header.slotOf(SlotKind::SLOT_CLOCK);
        if (s >= 0) g->openSlotPanel((uint8_t)s);
    };
    c.refreshHeader = [](void *user) { ((GUIManager *)user)->_header.refresh(); };
    if (!SlotPanels::build(_header.slotKind(slot), _slotDrawer.face, c)) return;
    syncPanelOffset();
    Kit::drawerPlace(_slotDrawer, _header.slotCentreX(slot), _slotDrawer.top);
    // One shape (D-23): the tab's coves where the panel reaches past the tab
    // (setOpenSlot() below); the panel's walls meet the line square.
    Kit::drawerOpen(_slotDrawer);
    _slotPanel = (int8_t)slot;
    _panelTouchMs = millis();   // it waits the window timeout from now (D-22)
    _header.setOpenSlot(slot, _slotDrawer.x, _slotDrawer.x + _slotDrawer.w);
    syncScrim();
}

// Its tab closes once the drawer has finished closing (slotDrawerClosed()).
void GUIManager::closeSlotPanel(bool animate) {
    if (_slotPanel < 0) return;
    _closingSlot = _slotPanel;
    _slotPanel = -1;
    syncScrim();
    Kit::drawerClose(_slotDrawer, animate);
}

// The slot drawer has gone: its tab closes, unless another slot's panel has
// opened meanwhile (that one's tab is the open one now).
void GUIManager::slotDrawerClosed() {
    if (_closingSlot >= 0 && _header.openSlot() == _closingSlot) _header.setOpenSlot(-1);
    _closingSlot = -1;
    headerPanelsClosed();
}

// WHAT WAITS FOR NO HEADER PANEL TO BE OPEN (owner, round 4): the header is
// always visible while one is open. A Show / Hide chosen meanwhile takes effect
// now; a peeked header goes on its own timer (peekHeader()). From the next
// timer pass, so a panel opening straight after another's close is seen first.
void GUIManager::headerPanelsClosed() {
    lv_async_call([](void *p) {
        GUIManager *g = (GUIManager *)p;
        if (g->_pnlSystem.isExpanded() || g->_slotPanel >= 0 || !g->_headerPending) return;
        g->_headerPending = false;
        g->applyHeaderHidden(!g->pageHeaderShown());
        g->rebuildDashboard();
    }, this);
}

// The dismiss scrim (GUIManager::begin()) takes a tap anywhere outside while any
// panel is open, so the tap only puts it away.
void GUIManager::syncScrim() {
    if (!_dismissScrim) return;
    if (_pnlSystem.isExpanded() || _slotPanel >= 0) lv_obj_add_flag  (_dismissScrim, LV_OBJ_FLAG_CLICKABLE);
    else                                            lv_obj_clear_flag(_dismissScrim, LV_OBJ_FLAG_CLICKABLE);
}

bool GUIManager::pageHeaderShown() const {
    if (_curPage >= _nPages || !_pages[_curPage].spec) return true;
    const PageSpec &p = *_pages[_curPage].spec;
    char v[12];
    if (Settings::page(p.slug, "header", v, sizeof(v))) {
        if (!strcmp(v, "hidden")) return false;
        if (!strcmp(v, "shown"))  return true;
    }
    return p.headerShown;
}

void GUIManager::setPageHeaderShown(bool shown) {
    if (_curPage >= _nPages || !_pages[_curPage].spec) return;
    const PageSpec &p = *_pages[_curPage].spec;
    // Only a choice that differs from the page's own default is stored.
    Settings::setPage(p.slug, "header", shown == p.headerShown ? nullptr : (shown ? "shown" : "hidden"));
    Settings::save();
    // A header panel open (the page panel's own Show / Hide): it waits.
    if (_pnlSystem.isExpanded() || _slotPanel >= 0) {
        _headerPending = true;
    } else {
        applyHeaderHidden(!shown);
        rebuildDashboard();
    }
    Serial.printf("[UI] page %s: header %s%s\n", p.slug, shown ? "shown" : "hidden",
                  _headerPending ? " (when the panel has closed)" : "");
}

// /popup's header keys (DEBUG_POPUP, for the harness): slot=<kind> opens that
// slot's panel as a tap does (clock, sensor, page, alarm, status), slot=-1
// closes any; glyphs=0|1|2 shows a glyph style without saving it, glyphs=3 the
// saved one again; pagehdr=0|1 hides or shows this page's header, saved as the
// page panel saves it; alarm=HHMM sets a pretend next alarm (RAM only, until
// K84), alarm=-1 clears it.
const char *GUIManager::headerDebug(const char *slotName, int glyphs, int pageHdr, int alarm, int alarmIn) {
    static char out[96];
    if (alarmIn >= 0) {
        // N minutes from now, as HHMM: the notice (within 12 hours) whatever the hour.
        time_t at = time(nullptr) + (time_t)alarmIn * 60 + 59;
        struct tm t;
        localtime_r(&at, &t);
        alarm = t.tm_hour * 100 + t.tm_min;
    }
    if (alarm > -2) {
        const bool ok = Alarms::setPretend(alarm < 0 ? -1 : alarm / 100, alarm < 0 ? 0 : alarm % 100, time(nullptr));
        // Its panel goes with its slot; then the alarm's claim, now.
        if (_slotPanel >= 0 && _header.slotKind((uint8_t)_slotPanel) == SlotKind::SLOT_ALARM) closeSlotPanel(false);
        _header.refresh();
        if (!ok)            snprintf(out, sizeof(out), "alarm: not set (HHMM, 24 h; or the time is not known yet)");
        else if (alarm < 0) snprintf(out, sizeof(out), "alarm: cleared");
        else                snprintf(out, sizeof(out), "alarm: next at %02d:%02d (pretend, not saved)", alarm / 100, alarm % 100);
        return out;
    }
    if (pageHdr >= 0) {
        closeSlotPanel(false);
        setPageHeaderShown(pageHdr != 0);
        snprintf(out, sizeof(out), "page %s: header %s (saved)", currentPageSlug(), pageHdr ? "shown" : "hidden");
        return out;
    }
    if (glyphs >= 0) {
        GlyphStyle gs = savedGlyphStyle();
        if (glyphs < (int)GlyphStyle::GLYPH_STYLE_COUNT) gs = (GlyphStyle)glyphs;
        _header.setGlyphStyle(gs);
        snprintf(out, sizeof(out), "glyphs: %s (not saved)", glyphStyleName(gs));
        return out;
    }
    if (!strcmp(slotName, "-1")) {
        closeSlotPanel(false);
        if (_pnlSystem.isExpanded()) _pnlSystem.close();
        return "slot: closed";
    }
    // /popup?scheme=N: the scheme on every page, as the system panel shows it,
    // NOT saved - a capture leaves the owner's choice in the file.
    if (!strncmp(slotName, "scheme", 6)) {
        closeSlotPanel(false);
        chooseScheme((uint8_t)atoi(slotName + 6), false);
        snprintf(out, sizeof(out), "scheme %s (not saved)", UI::pal().name);
        return out;
    }
    // /popup?cardstate=N (round 4): the cards' active state, 0 Fill .. 3 Corner
    // cut, NOT saved; -1 the owner's again.
    if (!strncmp(slotName, "cs", 2)) {
        const int n = atoi(slotName + 2);
        setCardState(n < 0 || n > 3 ? savedCardState() : (StateCardFill)n, false);
        snprintf(out, sizeof(out), "card state %d (not saved)", n);
        return out;
    }
    // /popup?sensorerr=1|0 (T10): the header's sensor shown failed, RAM only.
    if (!strcmp(slotName, "sensorerr") || !strcmp(slotName, "sensorok")) {
        _header.pretendSensorError(slotName[6] == 'e');
        return slotName[6] == 'e' ? "sensor: shown as failed (not saved)" : "sensor: as its entity says";
    }
    // Round 3: UI Tweaks (tweaks=1 comes here as "tweaks"), and a deck panel's
    // header tapped - deck0, deck1 - as /bench?what=anim does (the deck is up
    // under UI Tweaks, D-25).
    if (!strcmp(slotName, "tweaks")) {
        openUiTweaks();
        return TweaksSheet::isOpen() ? "tweaks: open" : "tweaks: not opened (a sheet is showing)";
    }
    if (!strncmp(slotName, "deck", 4)) {
        lv_obj_t *p = deckPanel((uint8_t)atoi(slotName + 4));
        lv_obj_t *h = p ? lv_obj_get_child(p, 0) : nullptr;
        if (!h) return "deck: no such panel";
        lv_obj_send_event(h, LV_EVENT_CLICKED, nullptr);
        return "deck: its header tapped";
    }
    SlotKind k;
    if (!slotKindFromName(slotName, k)) return "slot: no such kind (clock, sensor, page, alarm, status)";
    const int slot = _header.slotOf(k);
    if (slot < 0) return "slot: not in this page's header";
    if (!_header.slotShown((uint8_t)slot)) return "slot: not on the bar now (an alarm: alarm=HHMM first)";
    if (_slotPanel == slot || (k == SlotKind::SLOT_STATUS && _pnlSystem.isExpanded())) return "slot: already open";
    onSlotTap((uint8_t)slot);
    snprintf(out, sizeof(out), "slot %d (%s): opening", slot, slotKindName(k));
    return out;
}

// The owner's glyph style, else the default (square, D-20).
GlyphStyle GUIManager::savedGlyphStyle() {
    char v[12];
    GlyphStyle gs = GLYPH_DEFAULT;
    if (Settings::system("header", "glyphs", v, sizeof(v))) glyphStyleFromName(v, gs);
    return gs;
}
