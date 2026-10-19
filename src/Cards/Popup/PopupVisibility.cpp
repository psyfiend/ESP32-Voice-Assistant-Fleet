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

// --- WHICH GROUP? (K92 rule 9; the mock's "Table's card joins which group?") ----------
// A member made a card of its own joins a group: its group card's - chosen
// already, so Add is one tap - the group of its entity's HA area, or any other.
Card         *s_joinParent = nullptr;   // the group card or cluster it comes out of
const Entity *s_joinEntity = nullptr;
constexpr uint8_t JOIN_MAX = 24;
char          s_joinGid[JOIN_MAX][48];  // every group: the suggestions first
char          s_joinName[JOIN_MAX][56];
const char   *s_joinWords[3];
uint8_t       s_nJoin = 0, s_nSuggest = 0, s_joinPick = 0;
char          s_otherOpts[JOIN_MAX * 44];
lv_obj_t     *s_otherDd = nullptr;

void joinAdd(const char *gid, const char *name) {
    if (s_nJoin >= JOIN_MAX || !gid[0]) return;
    for (uint8_t k = 0; k < s_nJoin; k++) if (!strcmp(s_joinGid[k], gid)) return;
    snprintf(s_joinGid[s_nJoin], sizeof(s_joinGid[0]), "%s", gid);
    snprintf(s_joinName[s_nJoin], sizeof(s_joinName[0]), "%s", name);
    s_nJoin++;
}

// The suggestions, then every other group (CardGroups::forEach()).
void joinCollect() {
    s_nJoin = 0;
    CardGroups::Group g;
    if (s_joinParent && CardGroups::ofCard(*s_joinParent, g)) joinAdd(g.id, g.name);
    if (s_joinEntity && s_joinEntity->sourceAreaId[0]) {
        char gid[48];
        snprintf(gid, sizeof(gid), "%s%s", CardGroups::HA_PREFIX, s_joinEntity->sourceAreaId);
        if (CardGroups::byId(gid, g)) joinAdd(g.id, g.name);
    }
    s_nSuggest = s_nJoin;
    CardGroups::forEach([](const CardGroups::Group &gr, void *) { joinAdd(gr.id, gr.name); }, nullptr);
}

void joinChipCb(lv_event_t *ev) {
    const uint8_t k = (uint8_t)(uintptr_t)lv_event_get_user_data(ev);
    if (k >= s_nSuggest) return;   // the other group chosen: it is that one already
    s_joinPick = k;
    lv_async_call([](void *) { if (s.phase == PopupPhase::PHASE_OPEN) showView(PopupView::VIEW_JOIN); }, nullptr);
}

void joinOtherCb(lv_event_t *ev) {
    const uint32_t k = lv_dropdown_get_selected((lv_obj_t *)lv_event_get_target(ev));
    if (k == 0) return;                          // "-": a suggestion stays chosen
    s_joinPick = (uint8_t)(s_nSuggest + k - 1);
    lv_async_call([](void *) { if (s.phase == PopupPhase::PHASE_OPEN) showView(PopupView::VIEW_JOIN); }, nullptr);
}

void joinGoCb(lv_event_t *) { lv_async_call([](void *) { visibilityJoin(s_joinPick); }, nullptr); }

// Add card to dashboard, from a mini tile: which group first.
void addTileCb(lv_event_t *) {
    lv_async_call([](void *) {
        Card *c = held();
        ClusterCard *cl = clusterOfTile();
        const Entity *e = c ? c->firstEntity() : nullptr;
        if (!cl || !e || s.phase != PopupPhase::PHASE_OPEN) return;
        visibilityJoinOpen(cl, e);
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
    DeckRowSpec rows[3];
    if (cl) {
        const Entity *e = c->firstEntity();
        const bool made = e && cl->hasId() && CardAdded::addedFor(cl->id(), e->desc.id);
        uint8_t n = 0;
        rows[n++] = { "Hide from the cluster", Kit::RowKind::ROW_BUTTON, "Hide", 0, c->hasId(), hideTileCb, nullptr };
        rows[n++] = { "Add card to dashboard", Kit::RowKind::ROW_BUTTON, made ? "Added" : "Add", 0,
                      !made && e && cl->hasId(), addTileCb, nullptr };
        // A copy made here can go again, for good (K91; the dashboard's members only hide).
        if (CardAdded::isMember(c->id()))
            rows[n++] = { "Remove from the cluster", Kit::RowKind::ROW_BUTTON, "Remove", 0, true, removeCopyCb, nullptr };
        Kit::rows(list, rows, n);
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

// Which group: opened for `member` coming out of `parent` (a group card, its
// member's view; or a cluster, its tile's Visibility).
void visibilityJoinOpen(Card *parent, const Entity *member) {
    s_joinParent = parent;
    s_joinEntity = member;
    s_joinPick   = 0;
    if (s.phase == PopupPhase::PHASE_OPEN) showView(PopupView::VIEW_JOIN);
}

void buildJoin() {
    lv_obj_t *list = column("JOINS WHICH GROUP?");
    if (!s_joinParent || !s_joinEntity) return;
    joinCollect();
    if (!s_nJoin) { note(list, "No group is known yet."); return; }
    if (s_joinPick >= s_nJoin) s_joinPick = 0;
    // The suggestions as chips; the one chosen among the others, as a chip after them.
    uint8_t nChips = 0;
    for (uint8_t k = 0; k < s_nSuggest && nChips < 2; k++) s_joinWords[nChips++] = s_joinName[k];
    const bool other = s_joinPick >= s_nSuggest;
    if (other) s_joinWords[nChips++] = s_joinName[s_joinPick];
    lv_obj_t *r = Kit::row(list, "Group", true);
    Kit::chipRow(r, s_joinWords, nChips, other ? nChips - 1 : s_joinPick,
                 (lv_area_get_width(&s.winRect) - 2 * s.pad) * 2 / 3, joinChipCb);
    // Every other group, in a list.
    size_t used = snprintf(s_otherOpts, sizeof(s_otherOpts), "-");
    for (uint8_t k = s_nSuggest; k < s_nJoin; k++)
        used += snprintf(s_otherOpts + used, used < sizeof(s_otherOpts) ? sizeof(s_otherOpts) - used : 0,
                         "\n%s", s_joinName[k]);
    DeckRowSpec rows[2] = {
        { "Another group", Kit::RowKind::ROW_DROP, s_otherOpts, (uint16_t)(other ? s_joinPick - s_nSuggest + 1 : 0),
          s_nJoin > s_nSuggest, joinOtherCb, &s_otherDd },
        { "Add card to dashboard", Kit::RowKind::ROW_BUTTON, "Add", 0, true, joinGoCb, nullptr },
    };
    Kit::rows(list, rows, 2);
    // Where the suggestions come from, in words.
    char words[200];
    int w = snprintf(words, sizeof(words), "%s's card goes right after %s on this page.",
                     s_joinEntity->desc.name, s_joinParent->shownName(CardLabel::LBL_NAME));
    if (!s_nSuggest) { note(list, words); return; }
    w += snprintf(words + w, sizeof(words) - w, " %s is its group card's group", s_joinName[0]);
    if (s_nSuggest > 1)
        snprintf(words + w, sizeof(words) - w, "; %s its area's in HA.", s_joinName[1]);
    else
        snprintf(words + w, sizeof(words) - w, "%s.", s_joinEntity->sourceAreaId[0] ? ", and its area's in HA" : "");
    note(list, words);
}

// Made, in group `pick`; the view it came from again (its row says Added).
bool visibilityJoin(uint8_t pick) {
    if (!s_joinParent || !s_joinEntity || s.phase != PopupPhase::PHASE_OPEN) return false;
    joinCollect();
    char id[CARD_ID_MAX + 1];
    if (!CardAdded::add(*s_joinParent, *s_joinEntity, id, sizeof(id))) return false;
    if (pick < s_nJoin) Settings::setCard(id, "group", s_joinGid[pick]);   // add() gave it the parent's
    char msg[80];
    snprintf(msg, sizeof(msg), "Added to the dashboard, in %s", pick < s_nJoin ? s_joinName[pick] : "its group");
    UIToolkit::show_toast(msg);
    const bool tile = held() && held()->mini();
    showView(tile ? PopupView::VIEW_VISIBILITY : controlsView());
    return true;
}

bool visibilityTakeRebuild() {
    const bool r = s_rebuildPage;
    s_rebuildPage = false;
    return r;
}

}  // namespace popup
