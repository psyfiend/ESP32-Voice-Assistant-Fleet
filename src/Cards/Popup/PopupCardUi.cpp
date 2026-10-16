// Card UI (2.8 round 3, D-25 / D-28; round 4): the card's own looks, a view of
// the card window reached from the foot of SETTINGS. Each row is its words and
// an INHERIT toggle (owner, round 4): on, the row follows UI Tweaks and shows
// nothing more; off, the row's own choices appear under it, each selector only
// as wide as its own longest choice. Kept under the card's id; written when the
// window closes, as SETTINGS' rows are.
#include "PopupState.h"

namespace popup {

static Kit::Selector s_cuiSel[8];
static uint8_t       s_cuiN = 0;
static int32_t       s_scrollY = 0;           // kept across a rebuild of the view
static lv_obj_t     *s_list = nullptr;
static bool          s_rebuildPage = false;   // the page is built again once the window has gone

// --- What the rows are -----------------------------------------------------------
// Entity label, in the owner's order (K40; "Name (HA)" D-28).
static const char *const ENTITY_W[]  = { "Name (HA)", "State", "Custom", "None" };
static const CardLabel   ENTITY_IS[] = { CardLabel::LBL_HA, CardLabel::LBL_STATE, CardLabel::LBL_NAME,
                                         CardLabel::LBL_NONE };
static const char *const CLABEL_W[]  = { "Custom", "None" };     // CardGroups::LS_CUSTOM, LS_NONE
static const char *const METAL_W[]   = { "Black", "Silver" };
static const char *const SHAPE_W[]   = { "Square", "Round" };
static const char *const DENSITY_W[] = { "Compact", "Full", "Auto" };
static const char *const DENSITY_IS[] = { "compact", "full", "auto" };   // cardVariantName()'s words
static const char *const TIMEOUT_W[] = { "60 s", "30 s", "10 s", "Off" };
static const char *const TIMEOUT_IS[] = { "60", "30", "10", "off" };
static const char *const BAR_W[]     = { "On", "Off" };
static const char *const BAR_IS[]    = { "on", "off" };

enum CuiRow : uint8_t { CUI_ENTITY, CUI_CLABEL, CUI_SELECTOR, CUI_DENSITY, CUI_TIMEOUT, CUI_BAR, CUI_ROWS };
static const char *const ROW_WHAT[CUI_ROWS] = { "Entity label", "Card label", "Selector", "Card density",
                                                "Window timeout", "Timer bar" };

static Card *held() { return cardOf(h.surface); }

// The held card's saved word for `key`, as a position in `is`; -1: none saved.
static int8_t savedPos(const char *key, const char *const *is, uint8_t n) {
    Card *c = held();
    char v[20];
    if (!c || !c->hasId() || !Settings::card(c->id(), key, v, sizeof(v))) return -1;
    for (uint8_t i = 0; i < n; i++) if (!strcmp(v, is[i])) return (int8_t)i;
    return -1;
}

static int8_t savedLook() {
    Card *c = held();
    char v[20];
    int8_t look = -1;
    if (c && c->hasId() && Settings::card(c->id(), "selector", v, sizeof(v))) Kit::selectorLookFromName(v, look);
    return look;
}

static uint8_t posOf(const CardLabel *is, uint8_t n, CardLabel l) {
    for (uint8_t k = 0; k < n; k++) if (is[k] == l) return k;
    return 0;
}

// The view again (a row opened or closed; the selector's look changed),
// outside the selector's glide or the toggle's click, where it was scrolled to.
static void againAsync(void *) {
    if (s.phase != PopupPhase::PHASE_OPEN || s.view != PopupView::VIEW_CARD_UI) return;
    if (s_list) s_scrollY = lv_obj_get_scroll_y(s_list);
    showView(PopupView::VIEW_CARD_UI);
}
static void again() { lv_async_call(againAsync, nullptr); }

// This window's wait and bar, as the card now has them - at once (round 4).
static void retime() {
    int8_t i = savedPos("close", TIMEOUT_IS, 4);
    const uint32_t ms = i < 0 ? Kit::windowCloseMs() : i == 3 ? 0 : (uint32_t)atoi(TIMEOUT_IS[i]) * 1000;
    i = savedPos("bar", BAR_IS, 2);
    const bool bar = i < 0 ? Kit::windowBarShown() : i == 0;
    Kit::sheetRetime(s, ms, bar);
}

// --- What choosing does ----------------------------------------------------------
// On the held card at once, kept under its id - Inherit too, since K49 (a card
// the dashboard names is Custom unless told otherwise).
static void entityKeep(CardLabel l) {
    keepCardSetting("label", cardLabelName(l));
    if (Card *c = held()) { c->setLabelMode(l); restyleHeld(*c); }
}
static void entityChosen(uint8_t i) { if (i < 4) entityKeep(ENTITY_IS[i]); }

static void clabelKeep(CardGroups::LabelSource ls) {
    Card *c = held();
    if (!c) return;
    CardGroups::setLabelSource(*c, ls, c->pageAreaColor());
    restyleHeld(*c);
}
static void clabelChosen(uint8_t i) { clabelKeep(i ? CardGroups::LS_NONE : CardGroups::LS_CUSTOM); }

static void lookKeep(int8_t look) {
    keepCardSetting("selector", Kit::selectorLookName(look));
    Kit::selectorLookOverride(look);
    again();   // the rows show the look
}
static void metalChosen(uint8_t i) { lookKeep((int8_t)((Kit::selectorLookNow() & 1) | (i ? 2 : 0))); }
static void shapeChosen(uint8_t i) { lookKeep((int8_t)((Kit::selectorLookNow() & 2) | (i & 1))); }

// A card decides compact or full when it is built: the page is built again
// once the window has gone (it holds the page's cards).
static void densityKeep(const char *v) { keepCardSetting("density", v); s_rebuildPage = true; }
static void densityChosen(uint8_t i) { if (i < 3) densityKeep(DENSITY_IS[i]); }

static void timeoutChosen(uint8_t i) { if (i < 4) { keepCardSetting("close", TIMEOUT_IS[i]); retime(); } }
static void barChosen(uint8_t i)     { if (i < 2) { keepCardSetting("bar", BAR_IS[i]); retime(); } }

// THE INHERIT TOGGLE. On: the card's own choice goes. Off: what it inherits is
// made its own, so nothing changes until a choice is made.
static void inheritCb(lv_event_t *ev) {
    Card *c = held();
    if (!c) return;
    const uint8_t row = (uint8_t)(uintptr_t)lv_event_get_user_data(ev);
    switch (row) {
        case CUI_ENTITY:
            if (c->labelMode() != CardLabel::LBL_INHERIT) entityKeep(CardLabel::LBL_INHERIT);
            else entityKeep(cardLabelMode());
            break;
        case CUI_CLABEL:
            clabelKeep(CardGroups::labelSource(*c) == CardGroups::LS_INHERIT ? CardGroups::LS_CUSTOM
                                                                             : CardGroups::LS_INHERIT);
            break;
        case CUI_SELECTOR: {
            const int8_t look = savedLook();
            keepCardSetting("selector", look < 0 ? Kit::selectorLookName((int8_t)Kit::selectorLookNow()) : nullptr);
            Kit::selectorLookOverride(look < 0 ? (int8_t)Kit::selectorLookNow() : (int8_t)-1);
            break;
        }
        case CUI_DENSITY:
            densityKeep(savedPos("density", DENSITY_IS, 3) < 0
                        ? (c->variant() == CardVariant::VAR_FULL ? "full" : "compact") : nullptr);
            break;
        case CUI_TIMEOUT: {
            const uint32_t ms = Kit::windowCloseMs();
            keepCardSetting("close", savedPos("close", TIMEOUT_IS, 4) >= 0 ? nullptr
                                   : !ms ? "off" : ms <= 10000 ? "10" : ms <= 30000 ? "30" : "60");
            retime();
            break;
        }
        case CUI_BAR:
            keepCardSetting("bar", savedPos("bar", BAR_IS, 2) >= 0 ? nullptr : Kit::windowBarShown() ? "on" : "off");
            retime();
            break;
    }
    again();
}

// --- The view ----------------------------------------------------------------------
static void sel(lv_obj_t *line, const char *const *w, uint8_t n, uint8_t chosen, void (*cb)(uint8_t), int32_t room) {
    if (!line || s_cuiN >= sizeof(s_cuiSel) / sizeof(s_cuiSel[0])) return;
    Kit::choiceSelector(s_cuiSel[s_cuiN++], line, w, n, chosen, cb, room);
}

void buildCardUi() {
    Card *c = held();
    if (!c) return;
    const int32_t w = lv_area_get_width(&s.winRect) - 2 * s.pad;
    s_cuiN = 0;
    s_list = Kit::choiceList(s.stage, w);
    auto row = [&](uint8_t r, bool inherited) {
        return Kit::inheritRow(s_list, ROW_WHAT[r], inherited, inheritCb, (void *)(uintptr_t)r);
    };

    lv_obj_t *l = row(CUI_ENTITY, c->labelMode() == CardLabel::LBL_INHERIT);
    sel(l, ENTITY_W, 4, posOf(ENTITY_IS, 4, c->labelMode()), entityChosen, w);

    const CardGroups::LabelSource ls = CardGroups::labelSource(*c);
    l = row(CUI_CLABEL, ls == CardGroups::LS_INHERIT);
    sel(l, CLABEL_W, 2, ls == CardGroups::LS_NONE ? 1 : 0, clabelChosen, w);

    const int8_t look = savedLook();
    l = row(CUI_SELECTOR, look < 0);
    sel(l, METAL_W, 2, (look & 2) ? 1 : 0, metalChosen, w / 2);
    sel(l, SHAPE_W, 2, look & 1, shapeChosen, w / 2);

    int8_t i = savedPos("density", DENSITY_IS, 3);
    l = row(CUI_DENSITY, i < 0);
    sel(l, DENSITY_W, 3, i < 0 ? 0 : (uint8_t)i, densityChosen, w);

    i = savedPos("close", TIMEOUT_IS, 4);
    l = row(CUI_TIMEOUT, i < 0);
    sel(l, TIMEOUT_W, 4, i < 0 ? 0 : (uint8_t)i, timeoutChosen, w);

    i = savedPos("bar", BAR_IS, 2);
    l = row(CUI_BAR, i < 0);
    sel(l, BAR_W, 2, i < 0 ? 0 : (uint8_t)i, barChosen, w);

    if (s_scrollY) {
        lv_obj_update_layout(s_list);
        lv_obj_scroll_to_y(s_list, s_scrollY, LV_ANIM_OFF);
        s_scrollY = 0;
    }
}

// The card's own window look, before its window is built: its timeout and bar
// (the sheet's), its selector (the kit's, until the window has gone).
void cardWindowLook(const Card &c, Kit::SheetSpec &k) {
    char v[20];
    k.closeMs = 0;
    k.bar     = 0;
    int8_t look = -1;
    if (c.hasId()) {
        if (Settings::card(c.id(), "close", v, sizeof(v)))
            k.closeMs = !strcmp(v, "off") ? Kit::SHEET_NEVER : (uint32_t)atoi(v) * 1000;
        if (Settings::card(c.id(), "bar", v, sizeof(v))) k.bar = !strcmp(v, "off") ? 2 : 1;
        if (Settings::card(c.id(), "selector", v, sizeof(v))) Kit::selectorLookFromName(v, look);
    }
    Kit::selectorLookOverride(look);
}

void cardUiCb(lv_event_t *ev) {
    (void)ev;
    deckSet(DECK_PEEK);
    s_scrollY = 0;
    lv_async_call([](void *) { if (s.phase == PopupPhase::PHASE_OPEN) showView(PopupView::VIEW_CARD_UI); }, nullptr);
}

bool cardUiTakeRebuild() {
    const bool r = s_rebuildPage;
    s_rebuildPage = false;
    return r;
}

} // namespace popup
