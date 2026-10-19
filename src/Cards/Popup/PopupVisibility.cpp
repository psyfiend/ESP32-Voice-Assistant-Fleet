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

// THE VIEWS ARE CHIP TABLES (round 4, the owner: "look more like the Card UI or
// UI Tweaks sheet"): choices as chip rows, actions as buttons below them.
int32_t listW() { return lv_area_get_width(&s.winRect) - 2 * s.pad; }
lv_obj_t *s_table = nullptr;

lv_obj_t *table(const Kit::ChipRowSpec *rows, uint8_t n, lv_event_cb_t cb) {
    s_table = Kit::chipTable(s.stage, listW(), rows, n, cb);
    return s_table;
}

void note(lv_obj_t *list, const char *words) {
    lv_obj_t *l = Kit::label(list, KS::KS_LABEL_TAG_DIM);
    lv_obj_set_width(l, lv_pct(100));
    lv_obj_set_style_text_align(l, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_long_mode(l, LV_LABEL_LONG_WRAP);
    lv_label_set_text(l, words);
}

// Buttons, one to a row, one width - the longest words' - centred.
struct Action { const char *words; Kit::ButtonRole role; lv_event_cb_t cb; };
void actions(lv_obj_t *list, const Action *a, uint8_t n) {
    const UIShape &k = UI::shape();
    int32_t widest = 0;
    for (uint8_t i = 0; i < n; i++) widest = LV_MAX(widest, Kit::textW(a[i].words, UI::type().NAME));
    const int32_t bw = LV_MIN(listW(), widest + 2 * mm(k.S9));
    for (uint8_t i = 0; i < n; i++) Kit::button(Kit::buttonRow(list), a[i].role, a[i].words, bw, Kit::controlH(), a[i].cb);
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

// Add to's chips: row 0 the cluster, row 1 Copy or Move.
void addToChipCb(lv_event_t *ev) {
    const uintptr_t v = (uintptr_t)lv_event_get_user_data(ev);
    const uint8_t row = (uint8_t)(v >> 8), i = (uint8_t)(v & 0xFF);
    Kit::chipTableLit(s_table, row, i);
    if (row == 0) s_pick = i;
    else          s_move = i == 1;
}

void addCb(lv_event_t *) {
    lv_async_call([](void *) {
        if (!visibilityAddTo(s_pick, s_move)) UIToolkit::show_toast("It is in that cluster already");
    }, nullptr);
}

// --- WHICH GROUP? (K92 rule 9; the mock's "Table's card joins which group?") ----------
// A member made a card of its own joins a group: its group card's - chosen
// already, so Add is one tap - the group of its entity's HA area, or any other.
Card         *s_joinParent = nullptr;   // the group card or cluster it comes out of
const Entity *s_joinEntity = nullptr;
constexpr uint8_t JOIN_MAX = 24;
char          s_joinGid[JOIN_MAX][48];  // every group: the suggestions first
char          s_joinName[JOIN_MAX][56];
uint8_t       s_nJoin = 0, s_nSuggest = 0, s_joinPick = 0;
const char *const JOIN_W[] = { "Join" };

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

// A group's row: its one chip lit, the one before unlit (a row with one chip has
// no second to light).
void joinChipCb(lv_event_t *ev) {
    const uint8_t row = (uint8_t)((uintptr_t)lv_event_get_user_data(ev) >> 8);
    if (row >= s_nJoin) return;
    Kit::chipTableLit(s_table, s_joinPick, 1);
    Kit::chipTableLit(s_table, row, 0);
    s_joinPick = row;
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
    lv_obj_t *list = table(nullptr, 0, nullptr);
    Action a[3];
    uint8_t n = 0;
    if (cl) {
        const Entity *e = c->firstEntity();
        const bool made = e && cl->hasId() && CardAdded::addedFor(cl->id(), e->desc.id);
        if (c->hasId()) a[n++] = { "Hide from the cluster", Kit::ButtonRole::BTN_CANCEL, hideTileCb };
        if (!made && e && cl->hasId()) a[n++] = { "Add card to dashboard", Kit::ButtonRole::BTN_CANCEL, addTileCb };
        // A copy made here can go again, for good (K91; the dashboard's members only hide).
        if (CardAdded::isMember(c->id())) a[n++] = { "Remove from the cluster", Kit::ButtonRole::BTN_DESTRUCTIVE, removeCopyCb };
        actions(list, a, n);
        note(list, made ? "Its card is on the dashboard already. A hidden member stays in the cluster and "
                          "comes back from the cluster's window (a long press on its header)."
                        : "A hidden member stays in the cluster and comes back from the cluster's window (a "
                          "long press on its header).");
        return;
    }
    if (c->hasId())  a[n++] = { "Hide from this page", Kit::ButtonRole::BTN_CANCEL, hideCardCb };
    if (addable(c))  a[n++] = { "Add to a cluster",    Kit::ButtonRole::BTN_CANCEL, addToCb };
    actions(list, a, n);
    note(list, "Hidden cards go to this page's list of hidden cards, with their settings, and come back "
               "from there: tap the page's name in the header.");
    s_pick = 0;
    s_move = false;
}

// Add to a cluster: which one, Copy or Move, and Add.
void buildAddTo() {
    Card *c = held();
    const Entity *e = addable(c);
    s_nCl = 0;
    findClusters(lv_screen_active());
    if (!e || !s_nCl) {
        note(table(nullptr, 0, nullptr), e ? "There is no cluster on this page."
                                           : "This card cannot join a cluster: it stands for more than one "
                                             "thing, or is a cluster itself.");
        return;
    }
    if (s_pick >= s_nCl) s_pick = 0;
    const Kit::ChipRowSpec rows[2] = {
        { "Cluster",   s_clName, s_nCl, s_pick },
        { "This card", COPY_W,   2,     (uint8_t)(s_move ? 1 : 0) },
    };
    lv_obj_t *list = table(rows, 2, addToChipCb);
    const Action go = { "Add", Kit::ButtonRole::BTN_CONFIRM, addCb };
    actions(list, &go, 1);
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

// Joins which group: every group a row, its suggestions first, the chosen one's
// chip lit; Add at the foot.
void buildJoin() {
    if (!s_joinParent || !s_joinEntity) return;
    joinCollect();
    if (!s_nJoin) { note(table(nullptr, 0, nullptr), "No group is known yet."); return; }
    if (s_joinPick >= s_nJoin) s_joinPick = 0;
    Kit::ChipRowSpec rows[JOIN_MAX];
    for (uint8_t k = 0; k < s_nJoin; k++) rows[k] = { s_joinName[k], JOIN_W, 1, (uint8_t)(k == s_joinPick ? 0 : 1) };
    lv_obj_t *list = table(rows, s_nJoin, joinChipCb);
    const Action go = { "Add card to dashboard", Kit::ButtonRole::BTN_CONFIRM, joinGoCb };
    actions(list, &go, 1);
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
