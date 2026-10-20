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
// to dashboard (K17). Inside a CLUSTER's window it has this view instead, the
// same as its tile's (round 6: one member, one answer), and a hidden member's
// says Show in the cluster - Members lists them all.
//
// Copy or Move, both ways, tracked (K111 into a cluster; K112 out of one, the
// sprint after round 7): a tile's Add card to dashboard asks which, and the
// card it makes on the page remembers the tile - moved, it goes back (Move back
// to the cluster); copied, or made here any other way, it can be removed.
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
// COPY OR MOVE, OUT OF A CLUSTER (the sprint after round 7): the tile's own card,
// so the page card can say where it came from; null for a group card's member
// (Desk's lamp: the source's, so a copy only - K92 rule 4).
Card         *s_joinMember = nullptr;
bool          s_joinMove = false;
constexpr uint8_t JOIN_MAX = 24;
char          s_joinGid[JOIN_MAX][48];  // every group: the suggestions first
char          s_joinName[JOIN_MAX][56];
uint8_t       s_nJoin = 0, s_nSuggest = 0, s_joinPick = 0;
const char *const JOIN_W[] = { "Join" };
uint8_t joinFirst() { return s_joinMember ? 1 : 0; }   // the groups' rows start after This card's

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

// This card's row: Copy or Move. A group's row: its one chip lit, the one before
// unlit (a row with one chip has no second to light).
void joinChipCb(lv_event_t *ev) {
    const uintptr_t v = (uintptr_t)lv_event_get_user_data(ev);
    const uint8_t row = (uint8_t)(v >> 8);
    if (s_joinMember && row == 0) {
        Kit::chipTableLit(s_table, 0, (uint8_t)(v & 0xFF));
        s_joinMove = (v & 0xFF) == 1;
        return;
    }
    const uint8_t g = (uint8_t)(row - joinFirst());
    if (row < joinFirst() || g >= s_nJoin) return;
    Kit::chipTableLit(s_table, (uint8_t)(s_joinPick + joinFirst()), 1);
    Kit::chipTableLit(s_table, row, 0);
    s_joinPick = g;
}

void joinGoCb(lv_event_t *) { lv_async_call([](void *) { visibilityJoin(s_joinPick, s_joinMove); }, nullptr); }

// The cluster the subject is in: the held tile's, or the window's own.
ClusterCard *clusterOfSubject() {
    if (s.cluster && inMember()) return static_cast<ClusterCard *>(held());
    return clusterOfTile();
}

// Add card to dashboard, from a mini tile: which group first.
void addTileCb(lv_event_t *) {
    lv_async_call([](void *) {
        Card *c = visibilitySubject();
        ClusterCard *cl = clusterOfSubject();
        const Entity *e = c ? c->firstEntity() : nullptr;
        if (!cl || !e || s.phase != PopupPhase::PHASE_OPEN) return;
        visibilityJoinOpen(cl, e, c->hasId() ? c : nullptr);
    }, nullptr);
}

// Move back to the cluster: a card moved out of one (the sprint after round 7)
// is its tile again, and the page card goes. The page is built again when the
// window has gone.
void backToClusterCb(lv_event_t *) {
    lv_async_call([](void *) {
        Card *c = visibilitySubject();
        if (!c || !c->hasId() || s.phase != PopupPhase::PHASE_OPEN) return;
        if (CardAdded::moveBack(c->id())) {
            UIToolkit::show_toast("Back in the cluster");
            CardPopup::close();
        }
    }, nullptr);
}

// Show in the cluster: a hidden member (or one only in the header) a tile again.
void showTileCb(lv_event_t *) {
    lv_async_call([](void *) {
        Card *c = visibilitySubject();
        ClusterCard *cl = clusterOfSubject();
        const int8_t i = (cl && c) ? clusterMemberOf(*cl, c->firstEntity()) : -1;
        if (i < 0 || s.phase != PopupPhase::PHASE_OPEN) return;
        clusterMemberShow(*cl, (uint8_t)i, true);
        showView(PopupView::VIEW_VISIBILITY);   // it says so at once; the cluster when the window closes
    }, nullptr);
}

// Move back to the page: a member moved in from a card (round 7) leaves, and
// that card is back where it was. The page is built again when the window goes.
void moveBackCb(lv_event_t *) {
    lv_async_call([](void *) {
        Card *c = visibilitySubject();
        if (!c || !c->hasId() || s.phase != PopupPhase::PHASE_OPEN) return;
        if (CardAdded::removeMember(c->id())) {
            UIToolkit::show_toast("Back on the page");
            CardPopup::close();
        }
    }, nullptr);
}

// A card on the page shows `eid` already - a solo card, not a tile or a group
// card's member (round 7: no second card of one light from Add card to dashboard).
bool onPage(lv_obj_t *o, const char *eid) {
    const uint32_t n = lv_obj_get_child_count(o);
    for (uint32_t i = 0; i < n; i++) {
        lv_obj_t *c = lv_obj_get_child(o, (int32_t)i);
        if (!lv_obj_has_flag(c, CardPopup::CARD_SURFACE_FLAG)) { if (onPage(c, eid)) return true; continue; }
        Card *k = cardOf(c);
        const Entity *e = k ? k->firstEntity() : nullptr;
        if (k && e && !k->mini() && k->entityCount() == 1 && strcmp(k->typeName(), "cluster") && !strcmp(e->desc.id, eid))
            return true;
    }
    return false;
}

// PAUSED (round 7, the owner: SETTINGS' Paused belongs with Visibility): the
// card's own, a tile's, or - in a member's own view - that member's alone.
const char *const PAUSED_W[] = { "No", "Yes" };
void pausedChoose(void *p) {
    const bool on = (uintptr_t)p == 1;
    if (s.phase != PopupPhase::PHASE_OPEN) return;
    Card *c = inMember() ? nullptr : held();
    if (c) {
        c->setPaused(on);
    } else if (s.reg) {
        const uint32_t now = millis();
        for (uint8_t i = 0; i < s.nEnt; i++) if (s.ent[i]) s.reg->setPaused(s.ent[i]->desc.id, on, now);
    }
    deckRender();
    renderMain();   // the controls' PAUSED pill, when they show next
}
void pausedChipCb(lv_event_t *ev) {
    const uintptr_t v = (uintptr_t)lv_event_get_user_data(ev);
    Kit::chipTableLit(s_table, (uint8_t)(v >> 8), (uint8_t)(v & 0xFF));
    lv_async_call(pausedChoose, (void *)(v & 0xFF));
}

}  // namespace

Card *visibilitySubject() {
    Card *c = held();
    if (!(c && s.cluster && inMember())) return c;
    ClusterCard *cl = static_cast<ClusterCard *>(c);
    const int8_t i = clusterMemberOf(*cl, s.ent[0]);
    return i >= 0 ? cl->memberAt((uint8_t)i) : nullptr;
}

// Hide from the cluster: gone from it when the window closes. From its tile the
// window closes now; from the cluster's own window it stays, saying so.
void visibilityHideTile() {
    Card *c = visibilitySubject();
    ClusterCard *cl = clusterOfSubject();
    if (!c || !c->hasId() || !c->mini() || s.phase != PopupPhase::PHASE_OPEN) return;
    if (s.cluster && inMember()) {
        const int8_t i = cl ? clusterMemberOf(*cl, c->firstEntity()) : -1;
        if (i >= 0) clusterMemberShow(*cl, (uint8_t)i, false);
        showView(PopupView::VIEW_VISIBILITY);
        return;
    }
    Settings::setCard(c->id(), "hidden", "yes");
    s_rebuildPage = true;
    CardPopup::close();
}

void buildVisibility() {
    Card *held_ = held();
    // A GROUP CARD'S MEMBER (Desk's lamp, round 7): its Paused, and Add card to
    // dashboard - SETTINGS' row until now - unless the page shows it already.
    if (held_ && inMember() && !s.cluster) {
        const Entity *e = s.ent[0];
        const Kit::ChipRowSpec paused = { "Paused", PAUSED_W, 2, (uint8_t)(aggregate().paused ? 1 : 0) };
        lv_obj_t *list = table(&paused, 1, pausedChipCb);
        const bool there = e && ((held_->hasId() && CardAdded::addedFor(held_->id(), e->desc.id)) ||
                                 onPage(lv_screen_active(), e->desc.id));
        if (e && held_->hasId() && !there) {
            const Action add = { "Add card to dashboard", Kit::ButtonRole::BTN_CANCEL, addCardCb };
            actions(list, &add, 1);
        }
        note(list, there ? "Its card is on the dashboard already. Paused: out of its group."
                         : "Paused: out of its group.");
        return;
    }
    Card *c = visibilitySubject();
    if (!c) return;
    ClusterCard *cl = clusterOfSubject();
    // Paused, for anything that is not a cluster itself (K110).
    const bool clusterSelf = s.cluster && !inMember();
    const Kit::ChipRowSpec paused = { "Paused", PAUSED_W, 2, (uint8_t)(aggregate().paused ? 1 : 0) };
    lv_obj_t *list = clusterSelf ? table(nullptr, 0, nullptr) : table(&paused, 1, pausedChipCb);
    Action a[4];
    uint8_t n = 0;
    if (cl && c->hasId() && CardAdded::isMovedToPage(c->id())) {
        // MOVED OUT TO THE PAGE (the sprint after round 7): its card is there now,
        // and that card's Visibility is where it comes back - nothing to do here.
        note(list, "Moved out of the cluster to the page: its card there moves it back (Visibility - Move back "
                   "to the cluster).");
        return;
    }
    if (cl) {
        // COPY OR MOVE, TRACKED (round 7): where the member came from says what it
        // may do - no second card of one entity on the page.
        const Entity *e = c->firstEntity();
        const char *from = CardAdded::fromOf(c->id());
        const bool moved = from && CardAdded::isMoved(from);         // its card left the page for it
        const bool fromHidden = from && CardAdded::isHidden(from);   // its card is in the hidden ones
        const bool there = e && ((cl->hasId() && CardAdded::addedFor(cl->id(), e->desc.id)) ||
                                 onPage(lv_screen_active(), e->desc.id));
        const int8_t i = clusterMemberOf(*cl, e);
        const bool tile = i < 0 || clusterMemberShown(*cl, (uint8_t)i) == MemberShown::SHOWN_TILE;
        if (c->hasId()) a[n++] = tile ? Action{ "Hide from the cluster", Kit::ButtonRole::BTN_CANCEL, hideTileCb }
                                      : Action{ "Show in the cluster", Kit::ButtonRole::BTN_CANCEL, showTileCb };
        if (moved) {
            a[n++] = { "Move back to the page", Kit::ButtonRole::BTN_CANCEL, moveBackCb };
        } else {
            if (e && cl->hasId() && !there && !fromHidden)
                a[n++] = { "Add card to dashboard", Kit::ButtonRole::BTN_CANCEL, addTileCb };
            // A copy made here can go again, for good (K91; the dashboard's members only hide).
            if (CardAdded::isMember(c->id()))
                a[n++] = { "Remove from the cluster", Kit::ButtonRole::BTN_DESTRUCTIVE, removeCopyCb };
        }
        actions(list, a, n);
        note(list, moved      ? "Moved here from the page: Move back to the page puts its card back where it was. "
                                "A hidden member stays in the cluster, listed in its Members."
                 : there      ? "Its card is on the dashboard already. A hidden member stays in the cluster, listed "
                                "in its Members."
                 : fromHidden ? "Its card is hidden from the page: it comes back from the page's hidden cards (tap "
                                "the page's name). A hidden member stays in the cluster, listed in its Members."
                              : "A hidden member stays in the cluster, listed in its Members.");
        return;
    }
    // A CARD TAKEN OUT OF A CLUSTER (the sprint after round 7): moved, it is that
    // member, and goes back; copied - or any card made here - it can go for good,
    // as the owner asked ("the dashboard copy can go without being hidden").
    const bool movedHere = c->hasId() && CardAdded::movedHere(c->id());
    const bool made = c->hasId() && !movedHere && CardAdded::isAdded(c->id());
    if (movedHere)   a[n++] = { "Move back to the cluster", Kit::ButtonRole::BTN_CANCEL, backToClusterCb };
    if (c->hasId())  a[n++] = { "Hide from this page", Kit::ButtonRole::BTN_CANCEL, hideCardCb };
    if (addable(c))  a[n++] = { "Add to a cluster",    Kit::ButtonRole::BTN_CANCEL, addToCb };
    if (made)        a[n++] = { "Remove from the page", Kit::ButtonRole::BTN_DESTRUCTIVE, removeFromPageCb };
    actions(list, a, n);
    note(list, clusterSelf ? "A hidden cluster goes to this page's hidden cards, and comes back from there: tap the "
                             "page's name in the header."
             : movedHere   ? "Moved out of its cluster: Move back to the cluster makes it a tile again."
             : made        ? "Made on this board: Remove from the page deletes it, settings and all."
                           : "Paused: out of every group it is in, its card greyed. Hidden cards go to this "
                             "page's hidden cards, with their settings, and come back from there: tap the page's name.");
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
    if (!CardAdded::addTo(*s_cl[pick], *e, id, sizeof(id), c->id())) return false;
    // A COPY LOOKS LIKE ITS CARD: the name it shows ("Thermo", not HA's
    // "THERMOSTAT Temperature") - a custom name, as one typed would be (K40).
    const char *name = c->shownName(CardLabel::LBL_NAME);
    if (name && name[0]) Settings::setCard(id, "name", name);
    if (move) CardAdded::moveOut(c->id());   // off the page, not one of its hidden cards (round 7)
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
void visibilityJoinOpen(Card *parent, const Entity *member, Card *memberCard) {
    s_joinParent = parent;
    s_joinEntity = member;
    // Copy or Move only out of a cluster, for a member that is a card of its own.
    s_joinMember = (memberCard && parent && !strcmp(parent->typeName(), "cluster")) ? memberCard : nullptr;
    s_joinMove   = false;
    s_joinPick   = 0;
    if (s.phase == PopupPhase::PHASE_OPEN) showView(PopupView::VIEW_JOIN);
}

// Joins which group: This card's Copy / Move first (out of a cluster), then
// every group a row, its suggestions first, the chosen one's chip lit; Add at
// the foot.
void buildJoin() {
    if (!s_joinParent || !s_joinEntity) return;
    joinCollect();
    if (!s_nJoin) { note(table(nullptr, 0, nullptr), "No group is known yet."); return; }
    if (s_joinPick >= s_nJoin) s_joinPick = 0;
    Kit::ChipRowSpec rows[JOIN_MAX + 1];
    const uint8_t f = joinFirst();
    if (f) rows[0] = { "This card", COPY_W, 2, (uint8_t)(s_joinMove ? 1 : 0) };
    for (uint8_t k = 0; k < s_nJoin; k++) rows[f + k] = { s_joinName[k], JOIN_W, 1, (uint8_t)(k == s_joinPick ? 0 : 1) };
    lv_obj_t *list = table(rows, (uint8_t)(f + s_nJoin), joinChipCb);
    const Action go = { "Add card to dashboard", Kit::ButtonRole::BTN_CONFIRM, joinGoCb };
    actions(list, &go, 1);
    // Where the suggestions come from, in words.
    char words[320];
    int w = snprintf(words, sizeof(words), "%s%s's card goes right after %s on this page.",
                     f ? "Copy: it stays in the cluster too, and the card on the page can be removed again. Move: it "
                         "leaves the cluster, and its card's Visibility moves it back. " : "",
                     s_joinEntity->desc.name, s_joinParent->shownName(CardLabel::LBL_NAME));
    if (!s_nSuggest) { note(list, words); return; }
    w += snprintf(words + w, sizeof(words) - w, " %s is its group card's group", s_joinName[0]);
    if (s_nSuggest > 1)
        snprintf(words + w, sizeof(words) - w, "; %s its area's in HA.", s_joinName[1]);
    else
        snprintf(words + w, sizeof(words) - w, "%s.", s_joinEntity->sourceAreaId[0] ? ", and its area's in HA" : "");
    note(list, words);
}

// Made, in group `pick`; the view it came from again (its row says Added). A
// Move takes the member out of its cluster, and the window closes.
bool visibilityJoin(uint8_t pick, bool move) {
    if (!s_joinParent || !s_joinEntity || s.phase != PopupPhase::PHASE_OPEN) return false;
    if (move && !s_joinMember) return false;   // a group card's member is the source's: a copy only
    joinCollect();
    char id[CARD_ID_MAX + 1];
    const char *from = s_joinMember ? s_joinMember->id() : nullptr;
    if (!CardAdded::add(*s_joinParent, *s_joinEntity, id, sizeof(id), from)) return false;
    if (pick < s_nJoin) Settings::setCard(id, "group", s_joinGid[pick]);   // add() gave it the parent's
    // ITS NAME IS THE TILE'S when the tile has one of its own (the dashboard's,
    // or one typed): a moved card is the same card, and a copy looks like it, as
    // Add to a cluster's copies do. A member with HA's name keeps following HA (K65).
    if (s_joinMember) {
        const char *nm = s_joinMember->shownName(CardLabel::LBL_NAME);
        if (nm && nm[0] && strcmp(nm, s_joinEntity->desc.name)) Settings::setCard(id, "name", nm);
    }
    char msg[80];
    snprintf(msg, sizeof(msg), "%s the dashboard, in %s", move ? "Moved to" : "Added to",
             pick < s_nJoin ? s_joinName[pick] : "its group");
    UIToolkit::show_toast(msg);
    if (move) {
        CardAdded::moveToPage(from);   // no tile, not in the hidden cards; Members says On the page
        CardPopup::close();            // the page is built again once the window has gone
        return true;
    }
    // Back to the Visibility it came from: a tile's, or a member's own view's (round 7: a
    // group card's member asks from there too).
    const bool tile = (held() && held()->mini()) || inMember();
    showView(tile ? PopupView::VIEW_VISIBILITY : controlsView());
    return true;
}

bool visibilityTakeRebuild() {
    const bool r = s_rebuildPage;
    s_rebuildPage = false;
    return r;
}

}  // namespace popup
