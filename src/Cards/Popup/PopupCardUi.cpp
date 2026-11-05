// Card UI (2.8 round 3, D-25 / D-28; round 5): the card's own looks, a view of
// the card window reached from the foot of SETTINGS. The kit's chip table, as UI
// Tweaks' - one row per option, Inherit its first chip (owner, round 5: round
// 4's Inherit buttons and selectors gone). "Inherit" follows UI Tweaks. Kept
// under the card's id; written when the window closes, as SETTINGS' rows are.
#include "PopupState.h"

namespace popup {

static lv_obj_t *s_table = nullptr;
static bool      s_rebuildPage = false;   // the page is built again once the window has gone

// --- What the rows are (each row's first chip is Inherit) -------------------------
static const char *const ENTITY_W[]  = { "Inherit", "Name (HA)", "State", "Custom", "None" };
static const CardLabel   ENTITY_IS[] = { CardLabel::LBL_INHERIT, CardLabel::LBL_HA, CardLabel::LBL_STATE,
                                         CardLabel::LBL_NAME, CardLabel::LBL_NONE };
static const char *const CLABEL_W[]  = { "Inherit", "Custom", "None" };   // CardGroups::LabelSource's order
static const char *const METAL_W[]   = { "Inherit", "Black", "Silver" };
static const char *const SHAPE_W[]   = { "Square", "Round" };
static const char *const DENSITY_W[] = { "Inherit", "Compact", "Full", "Auto" };
static const char *const DENSITY_IS[] = { nullptr, "compact", "full", "auto" };   // cardVariantName()'s words
static const char *const TIMEOUT_W[] = { "Inherit", "60 s", "30 s", "10 s", "Off" };
static const char *const TIMEOUT_IS[] = { nullptr, "60", "30", "10", "off" };
static const char *const BAR_W[]     = { "Inherit", "On", "Off" };
static const char *const BAR_IS[]    = { nullptr, "on", "off" };

// A CLUSTER'S OWN CARD STATE FOR ITS TILES (2.11's final pass, 2.13): UI Tweaks'
// words with Inherit first; saved on the cluster as "card_state" (K106's way).
static const char *const STATE_W[]   = { "Inherit", "Fill", "Icon glow", "Bracket", "Corner cut" };
static const char *const STATE_IS[]  = { nullptr, "fill", "glow", "bracket", "corner" };

// A GROUP'S GROUP VIEW (K162): its members as tiles or cards, else UI Tweaks'. A
// group's row in both presentations, greyed while it is shown as a solo card.
static const char *const GVIEW_W[]   = { "Inherit", "Tiles", "Cards" };
static const char *const GVIEW_IS[]  = { nullptr, "tiles", "cards" };
// ITS GROUP VIEW'S CARD MARGINS (K167): UI Tweaks' words, Inherit first.
static const char *const MARGIN_W[]  = { "Inherit", "Small", "Medium", "Large", "X-large" };
static const char *const MARGIN_IS[] = { nullptr, "small", "medium", "large", "xlarge" };

// THE GROUP VIEW SECTION (owner, 2026-11-05, K167): at the foot of a group's Card
// UI, under its own title, what affects only its group view - Card appearance (K162's
// choice) and Card margins.
enum CuiRow : uint8_t { CUI_ENTITY, CUI_CLABEL, CUI_METAL, CUI_SHAPE, CUI_DENSITY, CUI_TIMEOUT, CUI_BAR, CUI_STATE,
                        CUI_GVIEW, CUI_GMARGIN, CUI_ROWS };
static lv_obj_t *s_table2 = nullptr;   // the Group view section's rows
static uint8_t   s_n1 = 0;             // the rows in the first table

static Card *held() { return settingsSubject(); }   // a member's own card in its view (K166)

// ONLY WHAT THIS CARD CAN USE (round 6, the owner: Card UI "should read the
// specific features of whichever card it was launched from"). A cluster wears
// no label of either kind, has no density and no selector: its window's timeout
// and bar. A mini tile names its entity, and has no card label or density. The
// selector rows are for a window that has one - a light's controls. The table's
// rows, in order, as CuiRows.
static CuiRow  s_rowAt[CUI_ROWS];
static uint8_t s_nRows = 0;

static bool rowApplies(CuiRow r) {
    const Card *c = held();
    if (!c) return false;
    const bool cluster = isClusterCard(*c), mini = c->mini();
    switch (r) {
        case CUI_ENTITY:  return !cluster;
        case CUI_CLABEL:  return !cluster && !mini;
        case CUI_METAL:
        case CUI_SHAPE:   return !cluster && s.lightHero;
        case CUI_DENSITY: return !cluster && !mini;
        case CUI_STATE:   return cluster;   // its tiles'
        case CUI_GVIEW:
        case CUI_GMARGIN: return (cluster || s.groupCard) && !inMember();   // a group's, either way it is shown
        default:          return true;   // the window's own timeout and bar
    }
}

// The held card's saved word for `key`, as a position in `is` (0: none saved - Inherit).
static uint8_t savedPos(const char *key, const char *const *is, uint8_t n) {
    Card *c = held();
    char v[20];
    if (!c || !c->hasId() || !Settings::card(c->id(), key, v, sizeof(v))) return 0;
    for (uint8_t i = 1; i < n; i++) if (!strcmp(v, is[i])) return i;
    return 0;
}

static int8_t savedLook() {
    Card *c = held();
    char v[20];
    int8_t look = -1;
    if (c && c->hasId() && Settings::card(c->id(), "selector", v, sizeof(v))) Kit::selectorLookFromName(v, look);
    return look;
}

// Which chip each row has lit, as the card has it now.
static uint8_t litOf(uint8_t row) {
    Card *c = held();
    if (!c) return 0;
    switch (row) {
        case CUI_ENTITY:
            for (uint8_t k = 0; k < 5; k++) if (ENTITY_IS[k] == c->labelMode()) return k;
            return 0;
        case CUI_CLABEL:  return (uint8_t)CardGroups::labelSource(*c);
        case CUI_METAL: { const int8_t l = savedLook(); return l < 0 ? 0 : (l & 2) ? 2 : 1; }
        case CUI_SHAPE:   return Kit::selectorLookNow() & 1;
        case CUI_DENSITY: return savedPos("density", DENSITY_IS, 4);
        case CUI_TIMEOUT: return savedPos("close", TIMEOUT_IS, 5);
        case CUI_BAR:     return savedPos("bar", BAR_IS, 3);
        case CUI_STATE:   return savedPos("card_state", STATE_IS, 5);
        case CUI_GVIEW:   return savedPos("group_view", GVIEW_IS, 3);
        case CUI_GMARGIN: return savedPos("margins", MARGIN_IS, 5);
    }
    return 0;
}

// Row `r` of the view (either table's) lit as the card has it now.
static void relight(uint8_t r) {
    if (r < s_n1) Kit::chipTableLit(s_table, r, litOf(s_rowAt[r]));
    else if (s_table2) Kit::chipTableLit(s_table2, (uint8_t)(r - s_n1), litOf(s_rowAt[r]));
}

// This window's wait and bar, as the card now has them - at once (round 4).
static void retime() {
    uint8_t i = savedPos("close", TIMEOUT_IS, 5);
    const uint32_t ms = i == 0 ? Kit::windowCloseMs() : i == 4 ? 0 : (uint32_t)atoi(TIMEOUT_IS[i]) * 1000;
    i = savedPos("bar", BAR_IS, 3);
    Kit::sheetRetime(s, ms, i == 0 ? Kit::windowBarShown() : i == 1);
}

static void lookKeep(int8_t look) {
    keepCardSetting("selector", Kit::selectorLookName(look));
    Kit::selectorLookOverride(look);
}

// --- CUSTOM TYPES ITS WORDS (2.11, K95) ----------------------------------------------
// A tap on Custom - lit already or not - opens the keyboard on the words the card
// shows now; Save keeps them, Cancel leaves the card Custom with the words it had.
static void relightAsync(void *) { if (s.phase == PopupPhase::PHASE_OPEN) showView(PopupView::VIEW_CARD_UI); }

// The entity label's words: the card's custom name (K37, K40). None: the
// dashboard's name again, which the page knows - built again once the window has gone.
static void nameSaved(const char *words, void *) {
    Card *c = held();
    if (!c || s.phase != PopupPhase::PHASE_OPEN) return;
    keepCardSetting("name", words[0] ? words : nullptr);
    if (words[0]) {
        c->setLabel(words);
        snprintf(s.name, sizeof(s.name), "%s", words);   // the window's title says it too
    } else {
        s_rebuildPage = true;
    }
    restyleHeld(*c);
    lv_async_call(relightAsync, nullptr);
}

// The card label's words, in its group's colour (K46 rename a). None is a card
// label in the colour alone, as Custom has always allowed.
static void labelSaved(const char *words, void *) {
    Card *c = held();
    if (!c || s.phase != PopupPhase::PHASE_OPEN) return;
    keepCardSetting("label_text", words);
    CardGroups::apply(*c, c->pageAreaColor());
    restyleHeld(*c);
}

static void typeWords(bool name) {
    Card *c = held();
    if (!c || !c->hasId()) return;
    static char now[ENTITY_NAME_MAX], hint[48];
    Kit::KeyboardSpec k = {};
    if (name) {
        snprintf(now, sizeof(now), "%s", c->shownName(CardLabel::LBL_NAME));
        snprintf(hint, sizeof(hint), "None: the dashboard's name");
        k.title  = "The card's name";
        k.maxLen = (uint8_t)LV_MIN(sizeof(now) - 1, (size_t)Kit::KEYBOARD_TEXT_MAX);
        k.save   = nameSaved;
    } else {
        if (!Settings::card(c->id(), "label_text", now, sizeof(now))) snprintf(now, sizeof(now), "%s", c->area());
        snprintf(hint, sizeof(hint), "None: the color alone");
        k.title  = "The card label's words";
        k.maxLen = 39;   // a group's name at most (SettingsHttp)
        k.save   = labelSaved;
    }
    k.text = now;
    k.hint = hint;
    Kit::keyboard(k);
}

// --- What a chip does: on the held card at once, kept under its id ----------------
static void chooseAsync(void *p) {
    const uintptr_t v = (uintptr_t)p;
    const uint8_t at = (uint8_t)(v >> 8), i = (uint8_t)(v & 0xFF);
    Card *c = held();
    if (!c || s.phase != PopupPhase::PHASE_OPEN || at >= s_nRows) return;
    const CuiRow row = s_rowAt[at];   // the table's row, as the option it is
    switch (row) {
        case CUI_ENTITY: {
            // Inherit too, since K49 (a card the dashboard names is Custom unless told otherwise).
            const CardLabel l = ENTITY_IS[i < 5 ? i : 0];
            keepCardSetting("label", cardLabelName(l));
            c->setLabelMode(l);
            restyleHeld(*c);
            break;
        }
        case CUI_CLABEL:
            CardGroups::setLabelSource(*c, (CardGroups::LabelSource)(i < 3 ? i : 0), c->pageAreaColor());
            restyleHeld(*c);
            break;
        case CUI_METAL:   // the scheme's own, or chosen; choosing one keeps the shape in force
            lookKeep(i == 0 ? (int8_t)-1 : (int8_t)((Kit::selectorLookNow() & 1) | (i == 2 ? 2 : 0)));
            break;
        case CUI_SHAPE:   // choosing a shape chooses the look
            lookKeep((int8_t)((Kit::selectorLookNow() & 2) | (i & 1)));
            break;
        case CUI_DENSITY: // a card decides compact or full when it is built: the page, once the window has gone
            keepCardSetting("density", DENSITY_IS[i < 4 ? i : 0]);
            s_rebuildPage = true;
            break;
        case CUI_TIMEOUT: keepCardSetting("close", TIMEOUT_IS[i < 5 ? i : 0]); retime(); break;
        case CUI_BAR:     keepCardSetting("bar", BAR_IS[i < 3 ? i : 0]);       retime(); break;
        case CUI_STATE:   // its tiles take it when the page is built again, once the window has gone
            keepCardSetting("card_state", STATE_IS[i < 5 ? i : 0]);
            s_rebuildPage = true;
            break;
        case CUI_GVIEW:   // read as the group view opens (PopupGroup.cpp)
            if (!s.groupCard) keepCardSetting("group_view", GVIEW_IS[i < 3 ? i : 0]);
            break;
        case CUI_GMARGIN: // read as the group view opens (PopupGroup.cpp, K167)
            if (!s.groupCard) keepCardSetting("margins", MARGIN_IS[i < 5 ? i : 0]);
            break;
        default: break;
    }
    // Every row lit as the card now has it (a look chosen moves the shape's chip too).
    if (s.view == PopupView::VIEW_CARD_UI)
        for (uint8_t r = 0; r < s_nRows; r++) relight(r);
    if (row == CUI_ENTITY && ENTITY_IS[i < 5 ? i : 0] == CardLabel::LBL_NAME) typeWords(true);
    if (row == CUI_CLABEL && i == (uint8_t)CardGroups::LS_CUSTOM)          typeWords(false);
}

static void chipCb(lv_event_t *ev) {
    lv_async_call(chooseAsync, lv_event_get_user_data(ev));
}

// The Group view section's chips: their rows after the first table's.
static void chipCb2(lv_event_t *ev) {
    const uintptr_t v = (uintptr_t)lv_event_get_user_data(ev);
    const uintptr_t at = ((v >> 8) & 0xFF) + s_n1;
    lv_async_call(chooseAsync, (void *)((at << 8) | (v & 0xFF)));
}

// --- The view ----------------------------------------------------------------------
void buildCardUi() {
    if (!held()) return;
    const Kit::ChipRowSpec all[CUI_ROWS] = {
        { "Entity label",   ENTITY_W,  5, litOf(CUI_ENTITY)  },
        { "Card label",     CLABEL_W,  3, litOf(CUI_CLABEL)  },
        { "Selector",       METAL_W,   3, litOf(CUI_METAL)   },
        { "Selector shape", SHAPE_W,   2, litOf(CUI_SHAPE)   },
        { "Card density",   DENSITY_W, 4, litOf(CUI_DENSITY) },
        { "Window timeout", TIMEOUT_W, 5, litOf(CUI_TIMEOUT) },
        { "Timer bar",      BAR_W,     3, litOf(CUI_BAR)     },
        { "Card state",     STATE_W,   5, litOf(CUI_STATE)   },   // a cluster's (its place in the list: to confirm)
        { "Card appearance", GVIEW_W,  3, litOf(CUI_GVIEW)   },   // K162, named so in its section (K167)
        { "Card margins",   MARGIN_W,  5, litOf(CUI_GMARGIN) },   // K167
    };
    Kit::ChipRowSpec rows[CUI_ROWS];
    s_nRows = 0;
    s_n1 = 0;
    for (uint8_t r = 0; r < CUI_ROWS; r++) {
        if (!rowApplies((CuiRow)r)) continue;
        s_rowAt[s_nRows] = (CuiRow)r;
        rows[s_nRows] = all[r];
        // Shown as a solo card, it has no group view (K162).
        if ((r == CUI_GVIEW || r == CUI_GMARGIN) && s.groupCard) rows[s_nRows].greyed = true;
        if (r < CUI_GVIEW) s_n1++;
        s_nRows++;
    }
    const int32_t w = lv_area_get_width(&s.winRect) - 2 * s.pad;
    s_table = Kit::chipTable(s.stage, w, rows, s_n1, chipCb);
    s_table2 = nullptr;
    if (s_nRows > s_n1) {
        lv_obj_t *t = Kit::drawerTitle(s.stage, "Group view");   // the section's title (K167)
        lv_obj_set_width(t, w);
        s_table2 = Kit::chipTable(s.stage, w, rows + s_n1, (uint8_t)(s_nRows - s_n1), chipCb2);
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
    lv_async_call([](void *) { if (s.phase == PopupPhase::PHASE_OPEN) showView(PopupView::VIEW_CARD_UI); }, nullptr);
}

void cardUiTypeWords(bool name) { typeWords(name); }

bool cardUiTakeRebuild() {
    const bool r = s_rebuildPage;
    s_rebuildPage = false;
    return r;
}

} // namespace popup
