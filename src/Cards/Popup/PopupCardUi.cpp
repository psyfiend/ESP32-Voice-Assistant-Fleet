// Card UI (2.8 round 3, D-25 / D-28): the card's own looks, a view of the card
// window reached from the foot of SETTINGS. Every row is the kit's choice rows
// (as UI Tweaks'), and "Inherit" follows UI Tweaks. Kept under the card's id;
// written when the window closes, as SETTINGS' rows are.
#include "PopupState.h"

namespace popup {

constexpr uint8_t CUI_ROWS = 7;
static Kit::Selector s_cuiSel[CUI_ROWS];
static bool          s_rebuildPage = false;   // the page is built again once the window has gone

// --- What the rows are -----------------------------------------------------------
// Entity label, in the owner's order (K40; named K45, "Name (HA)" D-28).
static const char *const ENTITY_W[]  = { "Inherit", "Name (HA)", "State", "Custom", "None" };
static const CardLabel   ENTITY_IS[] = { CardLabel::LBL_INHERIT, CardLabel::LBL_HA, CardLabel::LBL_STATE,
                                         CardLabel::LBL_NAME, CardLabel::LBL_NONE };
static uint8_t entityPos(CardLabel l) {
    for (uint8_t k = 0; k < 5; k++) if (ENTITY_IS[k] == l) return k;
    return 0;
}
static const char *const CLABEL_W[]  = { "Inherit", "Custom", "None" };   // CardGroups::LabelSource
static const char *const METAL_W[]   = { "Inherit", "Black", "Silver" };
static const char *const SHAPE_W[]   = { "Square", "Round" };
static const char *const DENSITY_W[] = { "Inherit", "Compact", "Full", "Auto" };
static const char *const DENSITY_IS[] = { nullptr, "compact", "full", "auto" };   // cardVariantName()'s words
static const char *const TIMEOUT_W[] = { "Inherit", "60 s", "30 s", "10 s", "Off" };
static const char *const TIMEOUT_IS[] = { nullptr, "60", "30", "10", "off" };
static const char *const BAR_W[]     = { "Inherit", "On", "Off" };
static const char *const BAR_IS[]    = { nullptr, "on", "off" };

// The held card's saved word for `key`, as a position in `is` (0: none saved).
static uint8_t savedPos(const char *key, const char *const *is, uint8_t n) {
    Card *c = cardOf(h.surface);
    char v[16];
    if (!c || !c->hasId() || !Settings::card(c->id(), key, v, sizeof(v))) return 0;
    for (uint8_t i = 1; i < n; i++) if (!strcmp(v, is[i])) return i;
    return 0;
}

static int8_t savedLook() {
    Card *c = cardOf(h.surface);
    char v[20];
    int8_t look = -1;
    if (c && c->hasId() && Settings::card(c->id(), "selector", v, sizeof(v))) Kit::selectorLookFromName(v, look);
    return look;
}

// The view again (the selector's look changed: the rows show it), outside the
// selector's glide.
static void againAsync(void *) {
    if (s.phase == PopupPhase::PHASE_OPEN && s.view == PopupView::VIEW_CARD_UI) showView(PopupView::VIEW_CARD_UI);
}

// --- What choosing does ----------------------------------------------------------
// On the held card at once, kept under its id - Inherit too, since K49 (a card
// the dashboard names is Custom unless told otherwise).
static void entityChosen(uint8_t i) {
    const CardLabel l = ENTITY_IS[i < 5 ? i : 0];
    keepCardSetting("label", cardLabelName(l));
    if (Card *c = cardOf(h.surface)) { c->setLabelMode(l); c->restyle(); }
}

static void clabelChosen(uint8_t i) {
    Card *c = cardOf(h.surface);
    if (!c || i > 2) return;
    CardGroups::setLabelSource(*c, (CardGroups::LabelSource)i, c->pageAreaColor());
    c->restyle();
}

static void lookKeep(int8_t look) {
    keepCardSetting("selector", Kit::selectorLookName(look));
    Kit::selectorLookOverride(look);
    lv_async_call(againAsync, nullptr);
}
static void metalChosen(uint8_t i) {
    lookKeep(i == 0 ? (int8_t)-1 : (int8_t)((Kit::selectorLookNow() & 1) | (i == 2 ? 2 : 0)));
}
static void shapeChosen(uint8_t i) { lookKeep((int8_t)((Kit::selectorLookNow() & 2) | (i & 1))); }

// A card decides compact or full when it is built: the page is built again
// once the window has gone (it holds the page's cards).
static void densityChosen(uint8_t i) {
    if (i > 3) return;
    keepCardSetting("density", DENSITY_IS[i]);
    s_rebuildPage = true;
}

// This window waits as chosen from now; the bar shows (or not) from the next.
static void timeoutChosen(uint8_t i) {
    if (i > 4) return;
    keepCardSetting("close", TIMEOUT_IS[i]);
    const uint32_t ms = i == 0 ? Kit::windowCloseMs() : i == 4 ? 0 : (uint32_t)atoi(TIMEOUT_IS[i]) * 1000;
    s.closeMs = ms;
}

static void barChosen(uint8_t i) { if (i <= 2) keepCardSetting("bar", BAR_IS[i]); }

// --- The view ----------------------------------------------------------------------
void buildCardUi() {
    Card *c = cardOf(h.surface);
    if (!c) return;
    const int8_t look = savedLook();
    // What the CARD chose - saved, else its default (Custom when the dashboard
    // names it, K49) - never what UI Tweaks makes of it.
    const Kit::ChoiceRow rows[CUI_ROWS] = {
        { "Entity label",   ENTITY_W,  5, entityPos(c->labelMode()),                   entityChosen  },
        { "Card label",     CLABEL_W,  3, (uint8_t)CardGroups::labelSource(*c),         clabelChosen  },
        { "Selector",       METAL_W,   3, (uint8_t)(look < 0 ? 0 : (look & 2) ? 2 : 1), metalChosen   },
        { "Selector shape", SHAPE_W,   2, (uint8_t)(Kit::selectorLookNow() & 1),        shapeChosen   },
        { "Card density",   DENSITY_W, 4, savedPos("density", DENSITY_IS, 4),           densityChosen },
        { "Window timeout", TIMEOUT_W, 5, savedPos("close", TIMEOUT_IS, 5),             timeoutChosen },
        { "Timer bar",      BAR_W,     3, savedPos("bar", BAR_IS, 3),                   barChosen     },
    };
    Kit::choiceRows(s.stage, lv_area_get_width(&s.winRect) - 2 * s.pad, rows, CUI_ROWS, s_cuiSel);
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
    lv_async_call([](void *) { if (s.phase == PopupPhase::PHASE_OPEN) showView(PopupView::VIEW_CARD_UI); }, nullptr);
}

bool cardUiTakeRebuild() {
    const bool r = s_rebuildPage;
    s_rebuildPage = false;
    return r;
}

} // namespace popup
