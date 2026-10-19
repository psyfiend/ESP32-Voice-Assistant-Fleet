// A CARD'S OPTIONS BY WHERE IT IS (2.11 round 3, K96; the words are the "2.11
// Clusters" mock's, section D - proposals, not settled): SETTINGS' Visibility
// row opens this view, and what it offers depends on where the held card is.
//
//   On the page    Hide from this page - K66's remove, for every card: to the
//                  page's pool with its settings, after the dialog that asks
//                  (PopupDialog.cpp); the page's list of hidden cards brings it
//                  back (the page's name in the header). Add to a group -
//                  greyed until the card list (K91).
//   In a cluster   Hide from the cluster - its Visible unticked (K104), back from
//                  the cluster's window; Add card to dashboard - a copy right
//                  after the cluster, in its group (K65; K92 rule 9's "which
//                  group?" comes with the card list).
//
// A member's own view inside a group card's window keeps SETTINGS' Add card
// to dashboard (K17).
#include "PopupState.h"
#include "Cards/ClusterCard.h"

namespace popup {

namespace {

bool s_rebuildPage = false;

Card *held() { return cardOf(h.surface); }

// The cluster a mini tile stands in: the first card surface above its own.
ClusterCard *clusterOfTile() {
    Card *c = held();
    if (!c || !c->mini()) return nullptr;
    for (lv_obj_t *o = lv_obj_get_parent(h.surface); o; o = lv_obj_get_parent(o)) {
        if (!lv_obj_has_flag(o, CardPopup::CARD_SURFACE_FLAG)) continue;
        Card *p = cardOf(o);
        return (p && !strcmp(p->typeName(), "cluster")) ? static_cast<ClusterCard *>(p) : nullptr;
    }
    return nullptr;
}

// The stage's whole height, so a short list starts at the top as the others do.
lv_obj_t *column(const char *heading) {
    lv_obj_t *list = plain(s.stage);
    lv_obj_set_size(list, lv_area_get_width(&s.winRect) - 2 * s.pad, lv_pct(100));
    lv_obj_add_flag(list, LV_OBJ_FLAG_SCROLLABLE);
    UI::tameScroll(list);
    lv_obj_set_flex_flow(list, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(list, mm(UI::shape().S3), 0);
    lv_label_set_text(Kit::label(list, KS::KS_PANEL_TITLE), heading);
    return list;
}

void note(lv_obj_t *list, const char *words) {
    lv_obj_t *l = Kit::label(list, KS::KS_LABEL_TAG_DIM);
    lv_obj_set_width(l, lv_pct(100));
    lv_label_set_long_mode(l, LV_LABEL_LONG_WRAP);
    lv_label_set_text(l, words);
}

void hideTileCb(lv_event_t *) { lv_async_call([](void *) { visibilityHideTile(); }, nullptr); }

// --- ADD TO A CLUSTER (K91, K100: Copy / Move on one screen, Copy first) -------------
// The clusters on this page, found on the screen; the one chosen and Copy or
// Move, kept while the view is rebuilt for a chip.
constexpr uint8_t PICK_MAX = 8;
ClusterCard *s_cl[PICK_MAX] = {};
const char  *s_clName[PICK_MAX] = {};
uint8_t      s_nCl = 0, s_pick = 0;
bool         s_move = false;
const char *const COPY_W[] = { "Copy - stays here", "Move" };

void findClusters(lv_obj_t *o) {
    const uint32_t n = lv_obj_get_child_count(o);
    for (uint32_t i = 0; i < n && s_nCl < PICK_MAX; i++) {
        lv_obj_t *c = lv_obj_get_child(o, (int32_t)i);
        if (lv_obj_has_flag(c, CardPopup::CARD_SURFACE_FLAG)) {
            Card *k = cardOf(c);
            if (k && k->hasId() && !strcmp(k->typeName(), "cluster")) {
                s_cl[s_nCl] = static_cast<ClusterCard *>(k);
                s_clName[s_nCl++] = k->area()[0] ? k->area() : k->shownName(CardLabel::LBL_NAME);
            }
            continue;   // a card's own objects hold no more cards (a cluster's tiles are its members)
        }
        findClusters(c);
    }
}

// Its entity is in the cluster already - a member of the dashboard's, or a copy added here.
bool alreadyIn(const ClusterCard &cl, const Entity &e) {
    for (uint8_t i = 0; i < cl.memberCount(); i++) {
        const Card *m = cl.memberAt(i);
        const Entity *me = m ? m->firstEntity() : nullptr;
        if (me && !strcmp(me->desc.id, e.desc.id)) return true;
    }
    return false;
}

// One entity, and not a group card itself: one level of nesting (K92 rule 3).
const Entity *addable(Card *c) {
    if (!c || !c->hasId() || c->mini() || s.cluster || s.nEnt != 1) return nullptr;
    return c->firstEntity();
}

void addToCb(lv_event_t *) {
    lv_async_call([](void *) { if (s.phase == PopupPhase::PHASE_OPEN) showView(PopupView::VIEW_ADD_TO); }, nullptr);
}

void pickCb(lv_event_t *ev) {
    s_pick = (uint8_t)(uintptr_t)lv_event_get_user_data(ev);
    addToCb(ev);
}

void moveCb(lv_event_t *ev) {
    s_move = (uintptr_t)lv_event_get_user_data(ev) == 1;
    addToCb(ev);
}

void addCb(lv_event_t *) { lv_async_call([](void *) { visibilityAddTo(s_pick, s_move); }, nullptr); }

// Add card to dashboard, from a mini tile: a copy right after its cluster.
void addTileCb(lv_event_t *) {
    lv_async_call([](void *) {
        Card *c = held();
        ClusterCard *cl = clusterOfTile();
        const Entity *e = c ? c->firstEntity() : nullptr;
        if (!cl || !e || s.phase != PopupPhase::PHASE_OPEN) return;
        char id[CARD_ID_MAX + 1];
        if (CardAdded::add(*cl, *e, id, sizeof(id))) showView(PopupView::VIEW_VISIBILITY);   // the row says Added
    }, nullptr);
}

}  // namespace

// Hide from the cluster: gone from it when the window closes - which it does now.
void visibilityHideTile() {
    Card *c = held();
    if (!c || !c->hasId() || !c->mini() || s.phase != PopupPhase::PHASE_OPEN) return;
    Settings::setCard(c->id(), "hidden", "yes");
    s_rebuildPage = true;
    CardPopup::close();
}

void buildVisibility() {
    Card *c = held();
    if (!c) return;
    ClusterCard *cl = clusterOfTile();
    lv_obj_t *list = column(cl ? "IN THE CLUSTER" : "ON THIS PAGE");
    DeckRowSpec rows[2];
    if (cl) {
        const Entity *e = c->firstEntity();
        const bool made = e && cl->hasId() && CardAdded::addedFor(cl->id(), e->desc.id);
        rows[0] = { "Hide from the cluster", Kit::RowKind::ROW_BUTTON, "Hide", 0, c->hasId(), hideTileCb, nullptr };
        rows[1] = { "Add card to dashboard", Kit::RowKind::ROW_BUTTON, made ? "Added" : "Add", 0,
                    !made && e && cl->hasId(), addTileCb, nullptr };
        Kit::rows(list, rows, 2);
        note(list, "A hidden member stays in the cluster's list and comes back from the cluster's window "
                   "(a long press on its header).");
        return;
    }
    rows[0] = { "Hide from this page",  Kit::RowKind::ROW_BUTTON, "Hide", 0, c->hasId(), hideCardCb, nullptr };
    rows[1] = { "Add to a cluster", Kit::RowKind::ROW_BUTTON, "Add", 0, addable(c) != nullptr, addToCb, nullptr };
    Kit::rows(list, rows, 2);
    note(list, "Hidden cards go to this page's list of hidden cards, with their settings, and come back "
               "from there: tap the page's name in the header.");
    s_pick = 0;
    s_move = false;
}

// Add to a cluster: which one, Copy or Move, and Add.
void buildAddTo() {
    Card *c = held();
    const Entity *e = addable(c);
    lv_obj_t *list = column("ADD TO A CLUSTER");
    s_nCl = 0;
    findClusters(lv_screen_active());
    if (!e || !s_nCl) {
        note(list, e ? "There is no cluster on this page." : "This card cannot join a cluster: it stands for "
                                                            "more than one thing, or is a cluster itself.");
        return;
    }
    if (s_pick >= s_nCl) s_pick = 0;
    const int32_t room = (lv_area_get_width(&s.winRect) - 2 * s.pad) * 2 / 3;
    lv_obj_t *r = Kit::row(list, "Cluster", true);
    Kit::chipRow(r, s_clName, s_nCl, s_pick, room, pickCb);
    r = Kit::row(list, "This card", true);
    Kit::chipRow(r, COPY_W, 2, s_move ? 1 : 0, room, moveCb);
    const bool in = alreadyIn(*s_cl[s_pick], *e);
    DeckRowSpec go = { in ? "It is in that cluster already" : "Add this card", Kit::RowKind::ROW_BUTTON, "Add", 0,
                       !in, addCb, nullptr };
    Kit::rows(list, &go, 1);
    note(list, "Copy: the card stays on the page and a copy joins the cluster. Move: the card joins it and "
               "leaves this page for its list of hidden cards. The cluster shows it when this window closes.");
}

bool visibilityAddTo(uint8_t pick, bool move) {
    Card *c = held();
    const Entity *e = addable(c);
    if (!e || s.phase != PopupPhase::PHASE_OPEN) return false;
    s_nCl = 0;
    findClusters(lv_screen_active());
    if (pick >= s_nCl || alreadyIn(*s_cl[pick], *e)) return false;
    char id[CARD_ID_MAX + 1];
    if (!CardAdded::addTo(*s_cl[pick], *e, id, sizeof(id))) return false;
    // A COPY LOOKS LIKE ITS CARD: the name it shows ("Thermo", not HA's
    // "THERMOSTAT Temperature") - a custom name, as one typed would be (K40).
    const char *name = c->shownName(CardLabel::LBL_NAME);
    if (name && name[0]) Settings::setCard(id, "name", name);
    if (move) CardAdded::hide(c->id());
    char msg[64];
    snprintf(msg, sizeof(msg), "%s to %s", move ? "Moved" : "Copied", s_clName[pick]);
    UIToolkit::show_toast(msg);
    CardPopup::close();   // the page is built again once the window has gone (CardAdded::takeChanged())
    return true;
}

void visibilityCb(lv_event_t *ev) {
    (void)ev;
    deckSet(DECK_PEEK);
    lv_async_call([](void *) { if (s.phase == PopupPhase::PHASE_OPEN) showView(PopupView::VIEW_VISIBILITY); }, nullptr);
}

bool visibilityTakeRebuild() {
    const bool r = s_rebuildPage;
    s_rebuildPage = false;
    return r;
}

}  // namespace popup
