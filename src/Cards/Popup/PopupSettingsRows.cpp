// SETTINGS' rows and what they do (2.12 P1, split from CardPopup.cpp).
#include "PopupState.h"

namespace popup {

// ---------------------------------------------------------------------------
// THE SETTINGS PANEL'S ROWS (owner, 2026-10-07)
//
// One list says what the rows are, and both the panel's size and its contents
// are read from it, so the two cannot disagree. Choices are DROPDOWNS (the row
// is shorter than a line of chips, and a list of choices can grow); Paused is
// a CHECKBOX. A row that does not work yet (until saving, 2.10d) is shown,
// greyed, so the owner can see the whole list.
// ---------------------------------------------------------------------------
#ifdef DEBUG_POPUP
uint8_t s_dbgRows = 0;                  // GET /popup?rows=N: test rows added to SETTINGS
#endif

void pauseCheckCb(lv_event_t *ev);
void groupDropCb(lv_event_t *ev);

// The card's group, from a PC (/popup?clabel=N): HA area / the dashboard's
// areas and the owner's groups / Own group / None (CardGroups.h). SETTINGS'
// Card label row went to Card UI as Inherit / Custom / None (2.8 round 3,
// D-28): the group is chosen another way, later.
CardGroups::Choice s_clMap[24];
uint8_t s_clN = 0;
char    s_clOpts[24 * 44];

// SETTINGS' Tap action row (2.10d, K37, K39): only what the held card can do -
// Toggle where its tap toggles, Members where it has members, the two scene
// actions where its light offers scenes. Rebuilt with the rows; the map says
// which action each line is.
void tapDropCb(lv_event_t *ev);
void tapSceneDropCb(lv_event_t *ev);
TapAction s_tapMap[8];
uint8_t   s_tapN = 0;
char      s_tapOpts[128];

TapAction tapShown(const Card &c) {
    const TapAction t = c.tapAction();
    if (t != TapAction::TAP_DEFAULT) return t;
    // Nothing to toggle: History (owner, 2.10e round 2) - as Card::tap().
    return c.tapToggles() ? TapAction::TAP_TOGGLE : TapAction::TAP_HISTORY;
}

// WHAT "Scenes..." DOES (owner, 2026-10-13; interview §12): SCENES' Scene
// action row - Load default or Cycle scenes. The card's own when its Tap
// action is a scene's, else the choice kept for when it becomes one.
TapAction sceneActionOf(const Card &c) {
    const TapAction t = c.tapAction();
    if (t == TapAction::TAP_CYCLE_SCENES || t == TapAction::TAP_LOAD_SCENE) return t;
    char v[12];
    if (c.hasId() && Settings::card(c.id(), "scene_action", v, sizeof(v)) && !strcmp(v, "cycle"))
        return TapAction::TAP_CYCLE_SCENES;
    return TapAction::TAP_LOAD_SCENE;
}

uint16_t tapRowBuild(const Card &c) {
    s_tapN = 0;
    size_t used = 0;
    s_tapOpts[0] = '\0';
    auto add = [&used](TapAction t, const char *word) {
        if (s_tapN >= 8) return;
        used += snprintf(s_tapOpts + used, sizeof(s_tapOpts) - used, "%s%s", s_tapN ? "\n" : "", word);
        s_tapMap[s_tapN++] = t;
    };
    const Entity *sc[ENTITY_SCENES_MAX];
    if (c.tapToggles()) add(TapAction::TAP_TOGGLE, "Toggle");
    add(TapAction::TAP_DETAILS, "Details view");
    if (hasMembers())   add(TapAction::TAP_MEMBERS, "Members view");
    add(TapAction::TAP_HISTORY, "History view");
    // One line for both (owner, 2026-10-13): which one is SCENES' Scene action.
    if (c.offeredScenes(sc, ENTITY_SCENES_MAX)) add(sceneActionOf(c), "Scenes...");
    add(TapAction::TAP_NOTHING, "Nothing");
    const TapAction cur = tapShown(c);
    for (uint8_t i = 0; i < s_tapN; i++) if (s_tapMap[i] == cur) return i;
    return 0;
}

// A row's control made live or greyed after it was built (the scene row, as
// Tap action changes): the dropdown's state and colours, and the row's words.
void rowLive(lv_obj_t *dd, bool live) {
    if (!dd) return;
    const UIPalette &p = UI::pal();
    if (live) lv_obj_remove_state(dd, LV_STATE_DISABLED);
    else      lv_obj_add_state   (dd, LV_STATE_DISABLED);
    lv_obj_set_style_text_color  (dd, UI::c(live ? p.TEXT : p.TEXT_DIM), 0);
    lv_obj_set_style_border_color(dd, UI::c(live ? p.ACCENT : UI::mix(p.SURFACE_ALT, p.TEXT, 25)), 0);
    if (lv_obj_t *row = lv_obj_get_parent(dd))
        if (lv_obj_t *words = lv_obj_get_child(row, 0))
            lv_obj_set_style_text_color(words, UI::c(live ? p.TEXT : p.TEXT_DIM), 0);
}

// The Units row's entity: the card's one, when it is a temperature that can be
// shown either way (its °C and °F differ).
const Entity *unitsRowFor() {
    const Entity *e = (s.nEnt == 1) ? s.ent[0] : nullptr;
    if (!e) return nullptr;
    return strcmp(cardDisplayUnit(*e, TempUnit::TEMP_C), cardDisplayUnit(*e, TempUnit::TEMP_F)) ? e : nullptr;
}
bool unitsShownF() {
    const Entity *e = unitsRowFor();
    return e && !strcmp(cardDisplayUnit(*e, s.tempUnit), cardDisplayUnit(*e, TempUnit::TEMP_F));
}
void unitsDropCb(lv_event_t *ev);
void propsCb(lv_event_t *ev);
void addCardCb(lv_event_t *ev);
void removeCardCb(lv_event_t *ev);

uint8_t deckSpecs(DeckRowSpec *r) {
    uint8_t n = 0;
    Card *held = cardOf(h.surface);
    r[n++] = { "Paused", DeckRowKind::ROW_CHECK, nullptr, (uint16_t)aggregate().paused, true,
               pauseCheckCb, &s.swPause };
    // When a group defined here counts as on - HA's group helper option.
    // Not for a group defined in HA or Hue: the source decides (2.10c).
    if (s.nEnt > 1 && !s.native && !inMember())
        r[n++] = { "Active state", DeckRowKind::ROW_DROP, "Any members are on\nAll members are on",
                   (uint16_t)(s.groupOn == GroupOn::GROUP_ON_ALL), true, groupDropCb, &s.ddGroup };
    // Entity label and Card label are Card UI's since 2.8 round 3 (D-25, D-28).
    // Visibility is NOT SHOWN (owner, 2026-10-12, K68): it belongs to cards in
    // a group shown as a cluster, a group panel or a group page, and none
    // exists yet. It comes back with them, offered only there.
    // Tap action (2.10d). Long press always opens the window (K39), whatever
    // this says. Greyed in a member's own view: it is the card's.
    const bool cardRows = held && held->hasId() && !inMember();
    if (held) {
        const uint16_t sel = tapRowBuild(*held);
        r[n++] = { "Tap action", DeckRowKind::ROW_DROP, s_tapOpts, sel, cardRows, tapDropCb, nullptr };
    }
    // Show hidden scenes and Tap scene are in the SCENES panel, in the Scenes
    // view (K43); Scenes "Off" was dropped. The Selector look moved to the
    // system panel (K45): it is device-wide, per scheme.
    // UNITS (owner, 2.10e round 2): a temperature's °C or °F, for this card;
    // only where there is a choice to make.
    if (unitsRowFor() && !inMember()) {
        r[n++] = { "Units", DeckRowKind::ROW_DROP, "\xC2\xB0" "C\n\xC2\xB0" "F", (uint16_t)unitsShownF(),
                   held && held->hasId(), unitsDropCb, nullptr };
    }
    // ENTITY PROPERTIES (owner): a view of its own, from here - no room for
    // another corner icon.
    r[n++] = { "Entity properties", DeckRowKind::ROW_BUTTON, "View", 0, true, propsCb, nullptr };
    // K17 (K65): a member of a group card made a card of its own - a copy,
    // right after the group card, in its group. Once made, the row says so.
    if (inMember() && held && held->hasId() && s.ent[0]) {
        const bool made = CardAdded::addedFor(held->id(), s.ent[0]->desc.id) != nullptr;
        r[n++] = { "Add card to dashboard", DeckRowKind::ROW_BUTTON, made ? "Added" : "Add", 0, !made, addCardCb, nullptr };
    }
    // A card added on the device can be removed (K66) - to the page's pool,
    // after a dialog that asks (confirmRemove()).
    if (!inMember() && held && CardAdded::isAdded(held->id()))
        r[n++] = { "Remove card", DeckRowKind::ROW_BUTTON, "Remove", 0, true, removeCardCb, nullptr };
    // CARD UI (2.8 round 3, D-27): the card's own looks, a view of their own -
    // at the foot. The card's, so greyed in a member's view; a card without a
    // usable id cannot keep them.
    if (held)
        r[n++] = { "Card UI", DeckRowKind::ROW_BUTTON, "Open", 0, cardRows, cardUiCb, nullptr };
#ifdef DEBUG_POPUP
    // GET /popup?rows=N: N greyed test rows, so SETTINGS is too tall for the
    // screen and the panel's drag can be tried (P1b round 2).
    for (uint8_t k = 0; k < s_dbgRows && n < DECK_ROWS_MAX; k++)
        r[n++] = { "Test row", DeckRowKind::ROW_DROP, "Test", 0, false, nullptr, nullptr };
#endif
    return n;
}

void deckRender() {
    if (s.swPause) {
        if (aggregate().paused) lv_obj_add_state  (s.swPause, LV_STATE_CHECKED);
        else                    lv_obj_remove_state(s.swPause, LV_STATE_CHECKED);
    }
    if (s.ddGroup) lv_dropdown_set_selected(s.ddGroup, s.groupOn == GroupOn::GROUP_ON_ALL ? 1 : 0);
}

// A card setting changed in SETTINGS, kept for the held card under its id
// (2.10d). In RAM until the window closes - closeNow() asks for the write.
void keepCardSetting(const char *key, const char *value) {
    Card *c = cardOf(h.surface);
    if (c && c->hasId()) Settings::setCard(c->id(), key, value);
}

// Active state: any member / all members. On the held card too, so it
// repaints at once and its tap follows the same rule.
void groupDropCb(lv_event_t *ev) {
    const uint32_t k = lv_dropdown_get_selected((lv_obj_t *)lv_event_get_target(ev));
    s.groupOn = k ? GroupOn::GROUP_ON_ALL : GroupOn::GROUP_ON_ANY;
    if (Card *c = cardOf(h.surface)) c->setGroupOn(s.groupOn);
    keepCardSetting("active", groupOnName(s.groupOn));
    renderMain();
}

// The default scene: the one chosen, else the first offered - kept, so Load
// default always has one.
void ensureDefaultScene(Card &c) {
    if (c.tapScene()[0]) return;
    const Entity *sc[ENTITY_SCENES_MAX];
    if (!c.offeredScenes(sc, ENTITY_SCENES_MAX)) return;
    c.setTapScene(sc[0]->desc.externalRef);
    keepCardSetting("tap_scene", sc[0]->desc.externalRef);
    if (s.ddTapScene) lv_dropdown_set_selected(s.ddTapScene, 0);
}

// Tap action: on the held card at once (the next tap does it), kept under its
// id. "Scenes..." is SCENES' Scene action, with a default scene ready.
void tapDropCb(lv_event_t *ev) {
    const uint32_t k = lv_dropdown_get_selected((lv_obj_t *)lv_event_get_target(ev));
    Card *c = cardOf(h.surface);
    if (!c || k >= s_tapN) return;
    const TapAction t = s_tapMap[k];
    c->setTapAction(t);
    keepCardSetting("tap", tapActionName(t));
    if (t == TapAction::TAP_LOAD_SCENE) ensureDefaultScene(*c);
}

// Units: on the card at once and kept under its id; the window and a showing
// chart follow.
void unitsDropCb(lv_event_t *ev) {
    const uint32_t k = lv_dropdown_get_selected((lv_obj_t *)lv_event_get_target(ev));
    const TempUnit u = k ? TempUnit::TEMP_F : TempUnit::TEMP_C;
    s.tempUnit = u;
    if (Card *c = cardOf(h.surface)) { c->setTempUnit(u); c->restyle(); }
    keepCardSetting("units", tempUnitName(u));
    renderMain();
    if (s.view == PopupView::VIEW_HISTORY) histDraw();
}

// Entity properties: SETTINGS folds, and the view opens (deferred - the button
// is in the panel).
void propsCb(lv_event_t *ev) {
    (void)ev;
    deckSet(DECK_PEEK);
    lv_async_call([](void *) { if (s.phase == PopupPhase::PHASE_OPEN) showView(PopupView::VIEW_PROPS); }, nullptr);
}

// "Add as a card" (K17): made now, kept in the file, shown when the window
// closes - the page is built again then (GUIManager::tick()), not under the
// window, which holds the page's cards.
void addCardCb(lv_event_t *ev) {
    (void)ev;
    Card *held = cardOf(h.surface);
    if (!held || !inMember() || !s.ent[0]) return;
    char id[CARD_ID_MAX + 1];
    if (!CardAdded::add(*held, *s.ent[0], id, sizeof(id))) return;
    lv_async_call([](void *) { deckSync(); }, nullptr);   // the row now says "Added" - not from its own button's event
}

// Paused, a checkbox. Through the card when it is the held one (it repaints at
// once, and every card on the same entities follows through the registry).
void pauseCheckCb(lv_event_t *ev) {
    const bool on = lv_obj_has_state((lv_obj_t *)lv_event_get_target(ev), LV_STATE_CHECKED);
    // In a member's own view, only that member (2.10b) - the registry, and
    // every card on it follows.
    Card *c = inMember() ? nullptr : cardOf(h.surface);
    if (c) {
        c->setPaused(on);
    } else if (s.reg) {
        const uint32_t now = millis();
        for (uint8_t i = 0; i < s.nEnt; i++) if (s.ent[i]) s.reg->setPaused(s.ent[i]->desc.id, on, now);
    }
    deckRender();
    renderMain();
}

} // namespace popup
