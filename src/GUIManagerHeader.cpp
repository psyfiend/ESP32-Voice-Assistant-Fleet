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
#include "Cards/CardAdded.h"   // this page's hidden cards (K66, K96)
#include "Cards/CardTypes.h"   // ...and its clusters' (GroupPresent, round 7)
#include "Cards/PageSpec.h"
#include "UI/HiddenSheet.h"    // ...in their own sheet (round 6)
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
    // THIS PAGE'S HIDDEN CARDS (2.11, K66, K96): how many, and a button to their
    // own sheet (round 6) - the panel goes first.
    if (_header.slotKind(slot) == SlotKind::SLOT_PAGE && _pages[_curPage].spec) {
        c.hiddenCount = hiddenCount();
        c.viewHidden  = [](void *user) {
            GUIManager *g = (GUIManager *)user;
            g->closeSlotPanel(true);
            g->openHiddenSheet();
        };
    }
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

namespace {
// WHAT A PAGE HAS HIDDEN (round 7, the owner: a card is in the page's hidden
// cards only because someone hid it with "Hide from this page" - never because
// it was moved into a cluster). The page's pool, then the cards crowded off it.
// A member hidden from its cluster is not here (round 13, K125): the cluster's
// Members list shows it and restores it.
struct Hidden {
    char id[CARD_ID_MAX + 1];
    char name[ENTITY_NAME_MAX + 32];
    char where[ENTITY_SHORT_MAX];   // a card's area, for two of one name
    bool crowded;                   // not hidden: the page had no room for it
};

void nameOf(const EntityRegistry &reg, const char *label, const char *entity, const char *id, char *out, size_t cap) {
    const Entity *e = entity && entity[0] ? reg.find(entity) : nullptr;
    snprintf(out, cap, "%s", label && label[0] ? label : e && e->desc.name[0] ? e->desc.name
                             : e && e->sourceName[0] ? e->sourceName : id);
}

uint8_t collectHidden(const PageSpec &page, const EntityRegistry &reg, const CardPage *built, Hidden *out, uint8_t cap) {
    static CardAdded::PoolItem pool[HiddenSheet::MAX];
    uint8_t n = CardAdded::pool(page, pool, LV_MIN(cap, HiddenSheet::MAX));
    for (uint8_t i = 0; i < n; i++) {
        Hidden &h = out[i];
        h = Hidden{};
        snprintf(h.id, sizeof(h.id), "%s", pool[i].id);
        snprintf(h.where, sizeof(h.where), "%s", pool[i].area);
        nameOf(reg, pool[i].label, pool[i].entity, pool[i].id, h.name, sizeof(h.name));
    }
    // CROWDED OFF THE PAGE (the sprint after round 7): not hidden, not shown -
    // the page had no room, and dropped the lowest priority first. Listed so they
    // are not forgotten; they come back when there is room.
    for (uint8_t i = 0; built && i < built->crowdedCount() && n < cap; i++) {
        const CardPage::Crowded &k = built->crowded(i);
        Hidden &h = out[n++];
        h = Hidden{};
        snprintf(h.id, sizeof(h.id), "%s", k.id);
        snprintf(h.where, sizeof(h.where), "%s", k.area);
        snprintf(h.name, sizeof(h.name), k.cluster ? "%s cluster" : "%s", k.name);
        h.crowded = true;
    }
    // Two cards called the same say where each is from ("Office Right (Office)").
    bool twin[HiddenSheet::MAX] = {};
    for (uint8_t i = 0; i < n && i < HiddenSheet::MAX; i++)
        for (uint8_t j = 0; j < n && !twin[i]; j++) twin[i] = j != i && !strcmp(out[j].name, out[i].name);
    for (uint8_t i = 0; i < n && i < HiddenSheet::MAX; i++) {
        const size_t len = strlen(out[i].name);
        if (twin[i] && out[i].where[0] && len + 4 < sizeof(out[i].name))
            snprintf(out[i].name + len, sizeof(out[i].name) - len, " (%s)", out[i].where);
    }
    return n;
}
}  // namespace

uint8_t GUIManager::hiddenCount() {
    if (_curPage >= _nPages || !_pages[_curPage].spec) return 0;
    static Hidden s_count[HiddenSheet::MAX];
    return collectHidden(*_pages[_curPage].spec, _core.entities(), _page, s_count, HiddenSheet::MAX);
}

// THE PAGE'S HIDDEN CARDS, IN THEIR OWN SHEET (2.11 round 6): each goes back to
// the page, which is built again under the sheet as each one is.
void GUIManager::openHiddenSheet() {
    if (_curPage >= _nPages || !_pages[_curPage].spec) return;
    static Hidden s_items[HiddenSheet::MAX];
    HiddenSheet::Spec sp;
    sp.page = _pages[_curPage].spec->title;
    sp.n    = collectHidden(*_pages[_curPage].spec, _core.entities(), _page, s_items, HiddenSheet::MAX);
    bool crowded = false;
    for (uint8_t i = 0; i < sp.n; i++) {
        sp.names[i] = s_items[i].name;
        sp.chip[i]  = s_items[i].crowded ? "No room" : "Restore to dashboard";
        crowded |= s_items[i].crowded;
    }
    if (crowded)
        sp.note = "No room: not hidden - this page had no room for it, and gave way lowest priority first. It "
                  "comes back by itself when there is room: hide another card, or change the page's rows.";
    sp.restore = [](void *user, uint8_t i) {
        if (i >= HiddenSheet::MAX) return false;
        const Hidden &h = s_items[i];
        if (h.crowded) {   // nothing to restore: it was never hidden
            UIToolkit::show_toast("No room on this page for it");
            return false;
        }
        if (!CardAdded::restore(h.id)) return false;
        Settings::save();
        return true;
    };
    sp.user = this;
    HiddenSheet::open(sp);
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
    // /popup?dash=1|2 (2.11 round 9): the development Reset - 1 its dialog, over
    // the system panel; 2 done without asking (Saved). /popup?grid=: the panel's
    // Auto (a) and Col / Row (cu, cd, ru, rd) - each Saved, as on the glass.
    if (!strncmp(slotName, "dash", 4)) {
        if (slotName[4] == '2') { resetDashboard(); return "dashboard: reset to the compiled one (saved)"; }
        closeSlotPanel(false);
        if (!_pnlSystem.isExpanded()) _pnlSystem.toggle();
        _pnlSystem.askDashReset();
        return "dashboard: the Reset's dialog";
    }
    if (!strncmp(slotName, "grid", 4)) {
        const char *g = slotName + 4;
        if      (!strcmp(g, "a"))  gridAuto();
        else if (!strcmp(g, "cu")) nudgeColumns(+1);
        else if (!strcmp(g, "cd")) nudgeColumns(-1);
        else if (!strcmp(g, "ru")) nudgeRows(+1);
        else if (!strcmp(g, "rd")) nudgeRows(-1);
        snprintf(out, sizeof(out), "grid pins: %u x %u (0: the arranger's; saved)", (unsigned)_pinCols, (unsigned)_pinRows);
        return out;
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
    // /popup?active=RRGGBB (round 6): the active colour on glass, NOT saved -
    // the screenshots' colours are not the panel's (Linen's olive, D-31);
    // active=0 the scheme's own again, as a scheme change also brings back.
    if (!strncmp(slotName, "act", 3)) {
        UI::setActiveColour(strtoul(slotName + 3, nullptr, 16));
        _binder.restyleAll();   // render() reads it; the frames are drawn
        snprintf(out, sizeof(out), "active colour %06lX (not saved)", (unsigned long)UI::pal().ST_ACTIVE);
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
