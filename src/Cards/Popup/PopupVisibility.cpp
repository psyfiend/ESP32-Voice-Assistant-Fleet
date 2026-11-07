// A CARD'S OPTIONS BY WHERE IT IS (2.11 round 3, K96; the buttons the owner's,
// round 2 of the 2.11 follow-ups): SETTINGS' Visibility row opens this view, and
// what it offers depends on where the held card is.
//
//   On the page    Hide from this page - to the page's pool, after its dialog
//                  (PopupDialog.cpp); Change group; Add to cluster
//   In a cluster   Unpin this card (or Pin this card, K160) - on the page or only
//                  in the group; Add to dashboard; Change group
//   A group card's Paused, and Add card to dashboard (K17) - a copy
//   member
//
// Restore - small, red, on the bottom row's right - on any card that has
// something to restore: a copy, a moved card, a group changed (PopupMove.cpp,
// where the moves and their dialogs are).
//
// A member's own view inside a CLUSTER's window has this view too, the same as
// its tile's (round 6: one member, one answer); one moved out of the cluster
// offers only its Restore.
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
    Kit::helperNote(l);   // UI Tweaks' Show helper text (2.13)
}

void restoreCb(lv_event_t *) {
    lv_async_call([](void *) { if (s.phase == PopupPhase::PHASE_OPEN) restoreAsk(visibilitySubject()); }, nullptr);
}

// Buttons, one to a row, one width - the longest words' - centred. RESTORE ON
// THE BOTTOM ROW, OFF TO THE RIGHT (owner, round 2): the column as it was, and
// Restore beside its last button; a row of its own where there is no room.
struct Action { const char *words; Kit::ButtonRole role; lv_event_cb_t cb; };
void actions(lv_obj_t *list, const Action *a, uint8_t n, bool restore) {
    const UIShape &k = UI::shape();
    int32_t widest = 0;
    for (uint8_t i = 0; i < n; i++) widest = LV_MAX(widest, Kit::textW(a[i].words, UI::type().NAME));
    const int32_t bw = LV_MIN(listW(), widest + 2 * mm(k.S9));
    lv_obj_t *row = nullptr;
    for (uint8_t i = 0; i < n; i++) {
        row = Kit::buttonRow(list);
        Kit::button(row, a[i].role, a[i].words, bw, Kit::controlH(), a[i].cb);
    }
    if (!restore) return;
    const int32_t rw = Kit::textW("Restore", UI::type().NAME) + 2 * mm(k.S7);
    if (!row || (listW() - bw) / 2 < rw + mm(k.S5)) {
        row = Kit::buttonRow(list);
        lv_obj_set_height(row, Kit::controlH());
    }
    lv_obj_t *r = Kit::button(row, Kit::ButtonRole::BTN_DESTRUCTIVE, "Restore", rw, Kit::controlH(), restoreCb);
    lv_obj_add_flag(r, LV_OBJ_FLAG_FLOATING);   // out of the row's centring
    lv_obj_align(r, LV_ALIGN_RIGHT_MID, 0, 0);
}

// THE SAME WORDS FOR CARDS AND TILES (round 14, the owner's) - a member's without
// the hidden cards' half (K160: a member is unpinned, never hidden).
const char *const VIS_NOTE = "Paused cards will not send or receive any updates. Hidden cards can be restored.";
const char *const VIS_NOTE_MEMBER = "Paused cards will not send or receive any updates.";

// The cluster the subject is in: the held tile's, or the window's own group's -
// a cluster's, or a group shown as one card's (K155).
ClusterCard *clusterOfSubject() {
    if (inMember())
        if (ClusterCard *g = heldGroup()) return g;
    return clusterOfTile();
}

void moveAsync(void *toCluster) {
    if (s.phase == PopupPhase::PHASE_OPEN) moveOpen(visibilitySubject(), clusterOfSubject(), toCluster != nullptr);
}
void toDashCb(lv_event_t *)    { lv_async_call(moveAsync, nullptr); }        // a tile's Add to dashboard; a card's Change group
void toClusterCb(lv_event_t *) { lv_async_call(moveAsync, (void *)1); }      // Change cluster; Add to cluster

Card *cardWithId(lv_obj_t *o, const char *id) {
    for (uint32_t i = 0; i < lv_obj_get_child_count(o); i++) {
        lv_obj_t *c = lv_obj_get_child(o, (int32_t)i);
        if (lv_obj_has_flag(c, CardPopup::CARD_SURFACE_FLAG)) {
            Card *k = cardOf(c);
            if (k && k->hasId() && !strcmp(k->id(), id)) return k;
            continue;
        }
        if (Card *k = cardWithId(c, id)) return k;
    }
    return nullptr;
}

// The group's name as its header says it, for a dialog's words.
const char *groupWords(const ClusterCard *cl) {
    if (!cl) return "";
    return cl->area()[0] ? cl->area() : cl->shownName(CardLabel::LBL_NAME);
}

// UNPIN AND PIN ASK FIRST (owner, 2026-11-05, K165; refines K160's "no dialog"):
// every change to a group's membership says what will happen, in his words.
void unpinGo() { if (s.phase == PopupPhase::PHASE_OPEN) visibilityHideTile(); }

void pinGo() {
    Card *c = visibilitySubject();
    ClusterCard *cl = clusterOfSubject();
    const int8_t i = (cl && c) ? clusterMemberOf(*cl, c->firstEntity()) : -1;
    if (i < 0 || s.phase != PopupPhase::PHASE_OPEN) return;
    clusterMemberShow(*cl, (uint8_t)i, true);
    // THE WINDOW CLOSES AND THE PAGE ARRANGES (owner, 2026-11-07, F14): a pin changes
    // the page - a solo card can become a group - so it is shown at once.
    s_rebuildPage = true;
    CardPopup::close();
}

// UNPIN THIS CARD (K160): off the page, still in the group.
void unpinTileCb(lv_event_t *) {
    lv_async_call([](void *) {
        Card *c = visibilitySubject();
        if (!c || s.phase != PopupPhase::PHASE_OPEN) return;
        // Names in the accent (owner, 2026-11-05: every card or group name a dialog shows).
        const unsigned long acc = (unsigned long)(UI::pal().ACCENT & 0xFFFFFF);
        const char *g = groupWords(clusterOfSubject());
        static char l1[180], l2[220];
        snprintf(l1, sizeof(l1), "This will hide the card #%06lX %s# from the group when looking at the page.",
                 acc, c->shownName(CardLabel::LBL_NAME));
        snprintf(l2, sizeof(l2), "The card will still be a member of group #%06lX %s# and will be visible on the "
                                 "'Group view' and the 'Members view'.", acc, g);
        const char *lines[] = { l1, l2 };
        confirmAsk(Kit::DialogRole::DLG_CONFIRM, "Unpin this card?", lines, 2, "Confirm",
                   Kit::ButtonRole::BTN_CONFIRM, unpinGo);
    }, nullptr);
}

// PIN THIS CARD (K160): on the page as a tile.
void showTileCb(lv_event_t *) {
    lv_async_call([](void *) {
        if (s.phase != PopupPhase::PHASE_OPEN) return;
        static char l1[180];
        snprintf(l1, sizeof(l1), "This will make the card visible from the page as a member of group #%06lX %s#.",
                 (unsigned long)(UI::pal().ACCENT & 0xFFFFFF), groupWords(clusterOfSubject()));
        const char *lines[] = { l1 };
        confirmAsk(Kit::DialogRole::DLG_CONFIRM, "Pin this card?", lines, 1, "Confirm",
                   Kit::ButtonRole::BTN_CONFIRM, pinGo);
    }, nullptr);
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

// A GROUP'S OWN VISIBILITY (owner, 2026-11-05, K174; 2026-11-06, K191): a line saying
// how it is presented, Paused (not for a group itself), and Show as... and Rename
// group with its buttons. Show as is a flow (PopupShowAs.cpp), never a chip.
void showAsCb(lv_event_t *) {
    lv_async_call([](void *) { if (s.phase == PopupPhase::PHASE_OPEN) showAsOpen(); }, nullptr);
}
void renameGroupCb(lv_event_t *) {
    lv_async_call([](void *) { if (s.phase == PopupPhase::PHASE_OPEN) clusterRename(); }, nullptr);
}

}  // namespace

// In a group's member: that member's own card (K157) - none for one of Desk's
// lamps, which has no card of its own here.
Card *visibilitySubject() {
    Card *c = held();
    ClusterCard *g = heldGroup();
    if (!(c && g && inMember())) return c;
    Card *m = memberCard();
    for (uint8_t i = 0; m && i < g->memberCount(); i++)
        if (g->memberAt(i) == m) return m;
    return nullptr;
}

// Unpin this card (K160): off the page when the window closes. From its tile the
// window closes now; from the group's own window it stays, saying so.
void visibilityHideTile() {
    Card *c = visibilitySubject();
    ClusterCard *cl = clusterOfSubject();
    if (!c || !c->hasId() || !c->mini() || s.phase != PopupPhase::PHASE_OPEN) return;
    const int8_t i = cl ? clusterMemberOf(*cl, c->firstEntity()) : -1;
    if (heldGroup() && inMember()) {
        if (i >= 0) clusterMemberShow(*cl, (uint8_t)i, false);
        showView(PopupView::VIEW_VISIBILITY);
        return;
    }
    if (i >= 0) clusterMemberShow(*cl, (uint8_t)i, false);   // out of the header too
    else        Settings::setCard(c->id(), "hidden", "yes");
    s_rebuildPage = true;
    CardPopup::close();
}

void buildVisibility() {
    Card *held_ = held();
    // A GROUP CARD'S MEMBER (Desk's lamp, round 7): its Paused, and Add card to
    // dashboard - a copy (PopupMove.cpp says when it cannot be).
    if (held_ && inMember() && !heldGroup()) {
        const Kit::ChipRowSpec paused = { "Paused", PAUSED_W, 2, (uint8_t)(aggregate().paused ? 1 : 0) };
        lv_obj_t *list = table(&paused, 1, pausedChipCb);
        if (s.ent[0] && held_->hasId()) {
            const Action add = { "Add card to dashboard", Kit::ButtonRole::BTN_CANCEL, addCardCb };
            actions(list, &add, 1, false);
        }
        note(list, VIS_NOTE);
        return;
    }
    Card *c = visibilitySubject();
    if (!c) return;
    ClusterCard *cl = clusterOfSubject();
    // Paused, for anything that is not a cluster itself (K110).
    const bool clusterSelf = s.cluster && !inMember();
    const bool groupSelf = !inMember() && (s.cluster || s.groupCard);   // K174
    const Kit::ChipRowSpec paused = { "Paused", PAUSED_W, 2, (uint8_t)(aggregate().paused ? 1 : 0) };
    lv_obj_t *list;
    list = clusterSelf ? table(nullptr, 0, nullptr) : table(&paused, 1, pausedChipCb);
    if (groupSelf) {
        // The owner's words (K191): how the page shows it now.
        lv_obj_t *l = Kit::label(list, KS::KS_LABEL_NAME);
        lv_obj_set_width(l, lv_pct(100));
        lv_obj_set_style_text_align(l, LV_TEXT_ALIGN_CENTER, 0);
        lv_label_set_long_mode(l, LV_LABEL_LONG_WRAP);
        lv_label_set_text(l, s.cluster ? "Group card being presented as 'Group'"
                                       : "Group card being presented as 'Solo card'");
        lv_obj_move_to_index(l, 0);
    }
    const bool restore = c->hasId() && restoreOffered(c);
    Action a[5];
    uint8_t n = 0;
    if (groupSelf && c->hasId()) a[n++] = { "Show as...", Kit::ButtonRole::BTN_CANCEL, showAsCb };       // K191
    if (groupSelf && c->hasId()) a[n++] = { "Rename group", Kit::ButtonRole::BTN_CANCEL, renameGroupCb };   // K174
    if (cl) {
        // MOVED OUT (the sprint after round 7): its card is elsewhere now, and its
        // Restore brings it back.
        if (c->hasId() && CardAdded::isMovedToPage(c->id())) {
            actions(list, a, 0, restore);
            note(list, VIS_NOTE_MEMBER);
            return;
        }
        const int8_t i = clusterMemberOf(*cl, c->firstEntity());
        const bool pinned = i < 0 || clusterMemberPinned(*cl, (uint8_t)i);
        if (c->hasId()) {
            a[n++] = pinned ? Action{ "Unpin this card", Kit::ButtonRole::BTN_CANCEL, unpinTileCb }   // K160
                            : Action{ "Pin this card", Kit::ButtonRole::BTN_CANCEL, showTileCb };
            a[n++] = { "Add to dashboard", Kit::ButtonRole::BTN_CANCEL, toDashCb };
            a[n++] = { "Change group", Kit::ButtonRole::BTN_CANCEL, toClusterCb };
        }
        actions(list, a, n, restore);
        note(list, VIS_NOTE_MEMBER);
        return;
    }
    // A BUILT-IN GROUP IS NEVER HIDDEN (K171: the Fleet page's Panel).
    const CardSpec *spec = c->hasId() ? CardAdded::specOf(c->id()) : nullptr;
    if (c->hasId() && !(spec && spec->builtIn)) a[n++] = { "Hide from this page", Kit::ButtonRole::BTN_CANCEL, hideCardCb };
    if (c->hasId() && !clusterSelf) {
        a[n++] = { "Change label", Kit::ButtonRole::BTN_CANCEL, toDashCb };   // K146
        a[n++] = { "Add to group", Kit::ButtonRole::BTN_CANCEL, toClusterCb };
    }
    actions(list, a, n, restore && !clusterSelf);
    note(list, clusterSelf ? "Hidden groups can be restored from the page's Edit cards."   // K148; to confirm
                           : VIS_NOTE);
}

void visibilityCb(lv_event_t *ev) {
    (void)ev;
    deckSet(DECK_PEEK);
    lv_async_call([](void *) { if (s.phase == PopupPhase::PHASE_OPEN) showView(PopupView::VIEW_VISIBILITY); }, nullptr);
}

void visibilityWantRebuild() { s_rebuildPage = true; }

bool visibilityTakeRebuild() {
    const bool r = s_rebuildPage;
    s_rebuildPage = false;
    return r;
}

// Where a member moved out has gone, in words for its row.
const char *movedWords(const Card *member, char *buf, size_t cap) {
    const char *to = (member && member->hasId()) ? CardAdded::movedTo(member->id()) : nullptr;
    if (!to) return nullptr;
    if (!to[0]) { snprintf(buf, cap, "On the page"); return buf; }
    if (Card *k = cardWithId(lv_screen_active(), to)) {
        snprintf(buf, cap, "In %s", k->area()[0] ? k->area() : k->shownName(CardLabel::LBL_NAME));
        return buf;
    }
    snprintf(buf, cap, "In another group");
    return buf;
}

}  // namespace popup
