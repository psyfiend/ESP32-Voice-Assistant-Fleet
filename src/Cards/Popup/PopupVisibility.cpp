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
    rows[0] = { "Hide from this page", Kit::RowKind::ROW_BUTTON, "Hide", 0, c->hasId(), hideCardCb, nullptr };
    rows[1] = { "Add to a group",      Kit::RowKind::ROW_BUTTON, "Add",  0, false, nullptr, nullptr };   // K91's card list, later
    Kit::rows(list, rows, 2);
    note(list, "Hidden cards go to this page's list of hidden cards, with their settings, and come back "
               "from there: tap the page's name in the header.");
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
