// GUIManager - the header's slots and their panels (2.8). Split from
// GUIManager.cpp, which was already past the structure check's 1,000 lines;
// the class is one, its header-slot half is here. See UI/HeaderSlots.h for the
// slots, UI/SlotPanels.h for what their panels hold.
#include "GUIManager.h"

#include <stdio.h>
#include <string.h>
#include "Settings.h"
#include "UI/SlotPanels.h"
#include "UI/UIToolkit.h"

// A tap on a slot (Panel_Header's callback): the status slot's panel is the
// system panel; any other slot's is the slot drawer, built for it now. A second
// tap on the open slot puts its panel away.
void GUIManager::onSlotTap(uint8_t slot) {
    if (_header.slotKind(slot) == SlotKind::SLOT_STATUS) { toggleSystemPanel(); return; }
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
    c.headerShown = !_headerHidden;
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
    c.setHeaderShown = [](void *user, bool shown) {
        GUIManager *g = (GUIManager *)user;
        g->closeSlotPanel(false);
        g->setPageHeaderShown(shown);
    };
    if (!SlotPanels::build(_header.slotKind(slot), _slotDrawer.face, c)) return;
    syncPanelOffset();
    Kit::drawerPlace(_slotDrawer, _header.slotCentreX(slot), _slotDrawer.top);
    Kit::drawerOpen(_slotDrawer);
    _slotPanel = (int8_t)slot;
    _header.setOpenSlot(slot);
    syncScrim();
}

void GUIManager::closeSlotPanel(bool animate) {
    if (_slotPanel < 0) return;
    Kit::drawerClose(_slotDrawer, animate);
    if (_header.openSlot() == _slotPanel) _header.setOpenSlot(-1);
    _slotPanel = -1;
    syncScrim();
}

// The dismiss scrim (GUIManager::begin()) takes a tap anywhere outside while any
// panel is open, so the tap only puts it away.
void GUIManager::syncScrim() {
    if (!_dismissScrim) return;
    if (_pnlSystem.isExpanded() || _slotPanel >= 0) lv_obj_add_flag  (_dismissScrim, LV_OBJ_FLAG_CLICKABLE);
    else                                            lv_obj_clear_flag(_dismissScrim, LV_OBJ_FLAG_CLICKABLE);
}

void GUIManager::cycleGlyphs() {
    const uint8_t n = (uint8_t)GlyphStyle::GLYPH_STYLE_COUNT;
    const GlyphStyle next = (GlyphStyle)(((uint8_t)_header.glyphStyle() + 1) % n);
    _header.setGlyphStyle(next);
    // Saved by name; the default (bare) as nothing.
    Settings::setSystem("header", "glyphs", next == GlyphStyle::GLYPH_BARE ? nullptr : glyphStyleName(next));
    Settings::save();
    Serial.printf("[UI] header glyphs: %s\n", glyphStyleName(next));
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
    applyHeaderHidden(!shown);
    rebuildDashboard();
    Serial.printf("[UI] page %s: header %s\n", p.slug, shown ? "shown" : "hidden");
}

// /popup's header keys (DEBUG_POPUP, for the harness): slot=N opens slot N's
// panel as a tap does, slot=-1 closes any; glyphs=0|1|2 shows a glyph style
// without saving it, glyphs=3 the saved one again; pagehdr=0|1 hides or shows
// this page's header, saved as the page panel saves it.
const char *GUIManager::headerDebug(int slot, int glyphs, int pageHdr) {
    static char out[96];
    if (pageHdr >= 0) {
        closeSlotPanel(false);
        setPageHeaderShown(pageHdr != 0);
        snprintf(out, sizeof(out), "page %s: header %s (saved)", currentPageSlug(), pageHdr ? "shown" : "hidden");
        return out;
    }
    if (glyphs >= 0) {
        GlyphStyle gs = GlyphStyle::GLYPH_BARE;
        if (glyphs < (int)GlyphStyle::GLYPH_STYLE_COUNT) {
            gs = (GlyphStyle)glyphs;
        } else {
            char v[12];
            if (Settings::system("header", "glyphs", v, sizeof(v))) glyphStyleFromName(v, gs);
        }
        _header.setGlyphStyle(gs);
        snprintf(out, sizeof(out), "glyphs: %s (not saved)", glyphStyleName(gs));
        return out;
    }
    if (slot < 0) {
        closeSlotPanel(false);
        if (_pnlSystem.isExpanded()) _pnlSystem.close();
        return "slot: closed";
    }
    if (slot >= _header.slotCount()) return "slot: no such slot";
    const SlotKind k = _header.slotKind((uint8_t)slot);
    if (_slotPanel == slot || (k == SlotKind::SLOT_STATUS && _pnlSystem.isExpanded())) return "slot: already open";
    onSlotTap((uint8_t)slot);
    snprintf(out, sizeof(out), "slot %d (%s): opening", slot, slotKindName(k));
    return out;
}
