// MOVING A CARD, AND ITS ONE WAY BACK (owner, round 2 of the 2.11 follow-ups).
// Every word on the glass here is the owner's (docs/archive/TEST_2.11_FOLLOWUPS.md,
// round 2; K137); none is made up here.
//
//   A cluster's tile   Add to dashboard (a group, Copy or Move) and Change cluster
//                      (another cluster, Copy or Move)
//   A page's card      Change group (a group) and Add to cluster (a cluster, Copy
//                      or Move)
//   A group card's     Add card to dashboard (a group; a copy only - its member
//   member             is the source's, K92 rule 4)
//
// Each picks from a grid, then Next asks in a dialog what will happen, and the
// window closes once it is done. Restore puts a card back as it was built: a copy
// is deleted, a moved card goes home, a regrouped one takes its group back
// (CardAdded.h, "One card, one way back").
//
// The groups offered are the ones on this page (owner: the rest only exist on
// other pages), its own marked with an asterisk, then New group and None. The
// clusters offered are this page's that can take it - hidden ones too, marked
// "(h)" - never one that holds it already.
#include "PopupState.h"
#include "Cards/ClusterCard.h"

namespace popup {

namespace {

constexpr uint8_t PICK_MAX = 8, GROUP_PICK_MAX = 16;

enum class MoveMode : uint8_t { MOVE_DASH, MOVE_CLUSTER, MOVE_GROUP };

struct Move {
    MoveMode      mode    = MoveMode::MOVE_DASH;
    Card         *subject = nullptr;   // the card itself; null for a group card's member
    ClusterCard  *from    = nullptr;   // the cluster it is a tile of; null on the page
    Card         *parent  = nullptr;   // the group card a member comes out of
    const Entity *ent     = nullptr;
    bool          move    = false;
    uint8_t       pick    = 0;
} m;

lv_obj_t *s_grid = nullptr, *s_copy = nullptr;
char      s_newName[40] = "";          // New group's words, from the keyboard

int32_t listW() { return lv_area_get_width(&s.winRect) - 2 * s.pad; }

bool isMade(const char *id) { return id && (CardAdded::isAdded(id) || CardAdded::isMember(id)); }

// --- The clusters -------------------------------------------------------------------
ClusterCard *s_cl[PICK_MAX] = {};
uint8_t      s_nCl = 0;

void findClusters(lv_obj_t *o) {
    const uint32_t n = lv_obj_get_child_count(o);
    for (uint32_t i = 0; i < n && s_nCl < PICK_MAX; i++) {
        lv_obj_t *c = lv_obj_get_child(o, (int32_t)i);
        if (lv_obj_has_flag(c, CardPopup::CARD_SURFACE_FLAG)) {
            Card *k = cardOf(c);
            if (k && k->hasId() && !strcmp(k->typeName(), "cluster")) s_cl[s_nCl++] = static_cast<ClusterCard *>(k);
            continue;   // a card's own objects hold no more cards (a cluster's tiles are its members)
        }
        findClusters(c);
    }
}

void screenClusters() {
    s_nCl = 0;
    findClusters(lv_screen_active());
}

// A cluster's name as the page shows it: its own words, else its group's.
const char *clName(Card *c) { return c->area()[0] ? c->area() : c->shownName(CardLabel::LBL_NAME); }

// The same for one that is not built (hidden).
void clusterNameNow(const char *id, char *out, size_t cap) {
    screenClusters();
    for (uint8_t i = 0; i < s_nCl; i++)
        if (!strcmp(s_cl[i]->id(), id)) { snprintf(out, cap, "%s", clName(s_cl[i])); return; }
    if (Settings::card(id, "label_text", out, cap) && out[0]) return;
    const CardSpec *cs = CardAdded::specOf(id);
    CardGroups::Group g;
    if (CardGroups::ofId(id, cs ? cs->area : nullptr, g)) { snprintf(out, cap, "%s", g.name); return; }
    snprintf(out, cap, "%s", cs && cs->label ? cs->label : cs && cs->area ? cs->area : id);
}

// A group card of the page's spec shown as a cluster (its saved "present" wins).
bool clusterSpec(const CardSpec &cs) {
    if (!cs.memberCount || !cs.id) return false;
    GroupPresent p = cs.present;
    char v[16];
    if (Settings::card(cs.id, "present", v, sizeof(v))) groupPresentFromName(v, p);
    return p == GroupPresent::PRESENT_CLUSTER;
}

// The cluster of the page's spec that `memberId` is a compiled member of; null.
const CardSpec *homeCluster(const char *memberId) {
    const PageSpec *p = CardAdded::pageSpec();
    for (uint8_t c = 0; memberId && p && c < p->count; c++)
        for (uint8_t k = 0; k < p->cards[c].memberCount && p->cards[c].members; k++)
            if (p->cards[c].members[k].id && !strcmp(p->cards[c].members[k].id, memberId)) return &p->cards[c];
    return nullptr;
}

// It holds the entity as a member already - a tile, a hidden one, a copy - but
// not one moved out of it, which can come back (owner's 17).
bool holds(const ClusterCard &cl, const char *eid) {
    for (uint8_t i = 0; i < cl.memberCount(); i++) {
        const Card *mc = cl.memberAt(i);
        const Entity *me = mc ? mc->firstEntity() : nullptr;
        if (me && !strcmp(me->desc.id, eid) && !(mc->hasId() && CardAdded::isMovedToPage(mc->id()))) return true;
    }
    return false;
}

bool holdsSpec(const CardSpec &cs, const char *eid) {
    for (uint8_t k = 0; k < cs.memberCount && cs.members; k++) {
        const CardSpec &mc = cs.members[k];
        if (mc.primaries[0] && !strcmp(mc.primaries[0], eid) && !CardAdded::isMovedToPage(mc.id)) return true;
    }
    return CardAdded::inCluster(cs.id, eid);
}

// ONE LEVEL OF NESTING (K92; the owner, round 2): a group card may be a cluster's
// member, but not one whose own members are groups.
bool nested(const Entity &e) {
    for (uint8_t k = 0; k < e.nMembers && s.reg; k++) {
        const Entity *me = s.reg->at(e.members[k]);
        if (me && me->nMembers) return true;
    }
    return false;
}

bool canJoin() {
    if (!m.ent || nested(*m.ent)) return false;
    return !m.subject || (m.subject->entityCount() == 1 && strcmp(m.subject->typeName(), "cluster"));
}

struct Target {
    char id[CARD_ID_MAX + 1];
    char name[48];
    char area[ENTITY_SHORT_MAX];
    char gid[48];
    char gname[40];
    bool hidden;
};
Target      s_tgt[PICK_MAX];
char        s_tgtWord[PICK_MAX][56];
const char *s_tgtWords[PICK_MAX];
uint8_t     s_nTgt = 0;

void tgtAdd(const char *id, const char *name, const char *area, bool hidden) {
    if (s_nTgt >= PICK_MAX) return;
    Target &t = s_tgt[s_nTgt];
    snprintf(t.id, sizeof(t.id), "%s", id);
    snprintf(t.name, sizeof(t.name), "%s", name);
    snprintf(t.area, sizeof(t.area), "%s", area ? area : "");
    CardGroups::Group g;
    const bool has = CardGroups::ofId(id, area, g);
    snprintf(t.gid, sizeof(t.gid), "%s", has ? g.id : "");
    snprintf(t.gname, sizeof(t.gname), "%s", has ? g.name : name);
    t.hidden = hidden;
    snprintf(s_tgtWord[s_nTgt], sizeof(s_tgtWord[0]), hidden ? "%s (h)" : "%s", name);
    s_tgtWords[s_nTgt] = s_tgtWord[s_nTgt];
    s_nTgt++;
}

void collectTargets() {
    s_nTgt = 0;
    if (!canJoin()) return;
    const char *eid = m.ent->desc.id;
    screenClusters();
    for (uint8_t i = 0; i < s_nCl; i++)
        if (s_cl[i] != m.from && !holds(*s_cl[i], eid)) tgtAdd(s_cl[i]->id(), clName(s_cl[i]), s_cl[i]->sheetArea(), false);
    const PageSpec *p = CardAdded::pageSpec();
    for (uint8_t c = 0; p && c < p->count; c++) {
        const CardSpec &cs = p->cards[c];
        if (!clusterSpec(cs) || !CardAdded::isHidden(cs.id) || (m.from && !strcmp(cs.id, m.from->id())) ||
            holdsSpec(cs, eid))
            continue;
        char nm[48];
        clusterNameNow(cs.id, nm, sizeof(nm));
        tgtAdd(cs.id, nm, cs.area, true);
    }
}

// --- The groups ---------------------------------------------------------------------
struct GroupPick { char gid[48]; char name[40]; };
GroupPick   s_grp[GROUP_PICK_MAX];
char        s_grpWord[GROUP_PICK_MAX][48];
const char *s_grpWords[GROUP_PICK_MAX + 2];
uint8_t     s_nGrp = 0, s_cur = 0xFF;   // the card's own group; 0xFF: none

void grpAdd(const CardGroups::Group &g) {
    for (uint8_t i = 0; i < s_nGrp; i++) if (!strcmp(s_grp[i].gid, g.id)) return;
    if (s_nGrp >= GROUP_PICK_MAX) return;
    snprintf(s_grp[s_nGrp].gid, sizeof(s_grp[0].gid), "%s", g.id);
    snprintf(s_grp[s_nGrp].name, sizeof(s_grp[0].name), "%s", g.name);
    s_nGrp++;
}

void findGroups(lv_obj_t *o) {
    const uint32_t n = lv_obj_get_child_count(o);
    for (uint32_t i = 0; i < n; i++) {
        lv_obj_t *c = lv_obj_get_child(o, (int32_t)i);
        if (!lv_obj_has_flag(c, CardPopup::CARD_SURFACE_FLAG)) { findGroups(c); continue; }
        CardGroups::Group g;
        Card *k = cardOf(c);
        if (k && CardGroups::ofCard(*k, g)) grpAdd(g);   // a cluster's tiles wear its group
    }
}

bool currentGroup(CardGroups::Group &g) {
    if (m.subject) return CardGroups::ofCard(*m.subject, g);
    return m.parent && CardGroups::ofCard(*m.parent, g);   // a member wears its group card's (K92)
}

uint8_t groupNew()  { return s_nGrp; }
uint8_t groupNone() { return (uint8_t)(s_nGrp + 1); }

void collectGroups() {
    s_nGrp = 0;
    s_cur = 0xFF;
    CardGroups::Group cur;
    const bool has = currentGroup(cur);
    findGroups(lv_screen_active());
    if (has) grpAdd(cur);
    for (uint8_t i = 0; i < s_nGrp; i++) {
        const bool own = has && !strcmp(s_grp[i].gid, cur.id);
        if (own) s_cur = i;
        snprintf(s_grpWord[i], sizeof(s_grpWord[0]), own ? "%s *" : "%s", s_grp[i].name);
        s_grpWords[i] = s_grpWord[i];
    }
    s_grpWords[groupNew()]  = "New group";
    s_grpWords[groupNone()] = "None";
}

// The pick as a group id: "none", a group's, or a New group made now.
bool groupFor(char *gid, size_t cap) {
    if (m.pick == groupNone()) { snprintf(gid, cap, "none"); return true; }
    if (m.pick == groupNew())  return CardGroups::make(s_newName, gid, cap);
    if (m.pick >= s_nGrp) return false;
    snprintf(gid, cap, "%s", s_grp[m.pick].gid);
    return true;
}

const char *pickName() {
    if (m.pick == groupNew()) return s_newName;
    return m.pick < s_nGrp ? s_grp[m.pick].name : nullptr;
}

// --- Words for the dialogs -------------------------------------------------------------
// A name in the accent (the owner's mock): the dialog's words are recoloured.
const char *acc(char *buf, size_t cap, const char *name) {
    snprintf(buf, cap, "#%06lX %s#", (unsigned long)(UI::pal().ACCENT & 0xFFFFFF), name ? name : "");
    return buf;
}

char s_line[4][240];
const char *s_lines[4];
uint8_t s_nLines = 0;

void lines() { s_nLines = 0; }
void line(const char *fmt, const char *a = nullptr, const char *b = nullptr) {
    if (s_nLines >= 4) return;
    snprintf(s_line[s_nLines], sizeof(s_line[0]), fmt, a ? a : "", b ? b : "");
    s_lines[s_nLines] = s_line[s_nLines];
    s_nLines++;
}

const char *const RESTORABLE = "A card can always be restored to its original configuration.";

// --- Doing it --------------------------------------------------------------------------
// The card a copy or a move names as its original: a made card's own, else itself.
const char *origOf(const Card &c) {
    const char *f = isMade(c.id()) ? CardAdded::fromOf(c.id()) : nullptr;
    return f ? f : c.id();
}

// A NEW CARD LOOKS LIKE ITS CARD: the name it shows, kept when it is not HA's.
void nameLike(const char *id, const Card &like) {
    const char *nm = like.shownName(CardLabel::LBL_NAME);
    if (nm && nm[0] && m.ent && strcmp(nm, m.ent->desc.name)) Settings::setCard(id, "name", nm);
}

// The last tile in its cluster: moving it out leaves the cluster empty.
bool lastTile() {
    if (!m.from || !m.subject) return false;
    uint8_t tiles = 0;
    bool mine = false;
    for (uint8_t i = 0; i < m.from->memberCount(); i++) {
        if (clusterMemberShown(*m.from, i) != MemberShown::SHOWN_TILE) continue;
        tiles++;
        if (m.from->memberAt(i) == m.subject) mine = true;
    }
    return mine && tiles == 1;
}

// A card's group changed. A compiled card keeps the one it had (orig_group), and
// coming back to it leaves nothing to restore.
void regroup(const char *id, const char *gid, const char *sheetArea) {
    char cur[48];
    const bool had = Settings::card(id, "group", cur, sizeof(cur));
    if (!isMade(id)) {
        char was[48];
        if (!Settings::card(id, "orig_group", was, sizeof(was)))
            Settings::setCard(id, "orig_group", had ? cur : "inherit");
    }
    Settings::setCard(id, "group", gid);
    char was[48];
    if (!Settings::card(id, "orig_group", was, sizeof(was))) return;
    CardGroups::Group dflt;
    const bool backToDefault = !strcmp(was, "inherit") && CardGroups::ofId(nullptr, sheetArea, dflt) &&
                               !strcmp(dflt.id, gid);
    if (!strcmp(was, gid) || backToDefault) {
        Settings::setCard(id, "group", backToDefault ? nullptr : gid);
        Settings::setCard(id, "orig_group", nullptr);
    }
}

void done() {
    visibilityWantRebuild();
    CardPopup::close();   // the page is built again once the window has gone
}

void doDash() {
    char gid[48], id[CARD_ID_MAX + 1];
    if (!groupFor(gid, sizeof(gid))) return;
    if (!m.subject) {   // a group card's member: a copy, after its group card
        if (!m.parent || !CardAdded::add(*m.parent, *m.ent, id, sizeof(id))) return;
        Settings::setCard(id, "group", gid);
        done();
        return;
    }
    if (!m.from) return;
    const char *sid = m.subject->id();
    const bool last = m.move && lastTile();
    CardGroups::Group fg;
    const bool fromHas = CardGroups::ofCard(*m.from, fg);
    if (!m.move) {
        if (!CardAdded::add(*m.from, *m.ent, id, sizeof(id), origOf(*m.subject))) return;
        Settings::setCard(id, "group", gid);
        nameLike(id, *m.subject);
    } else if (isMade(sid)) {
        // A CARD MADE HERE RELOCATES - unless it is a page card moved in, and going
        // back to the page is its Restore (owner's 17).
        const char *o = CardAdded::fromOf(sid);
        if (CardAdded::isMoveOf(sid) && o && CardAdded::isMoved(o)) {
            char orig[CARD_ID_MAX + 1];
            snprintf(orig, sizeof(orig), "%s", o);
            CardAdded::unmake(sid);
            CardGroups::Group og;
            const CardSpec *os = CardAdded::specOf(orig);
            if (!CardGroups::ofId(orig, os ? os->area : nullptr, og) || strcmp(og.id, gid))
                regroup(orig, gid, os ? os->area : nullptr);
        } else {
            if (!CardAdded::toPage(sid, m.from->id(), m.from->placement().priority)) return;
            Settings::setCard(sid, "group", gid);
        }
    } else {
        if (!CardAdded::add(*m.from, *m.ent, id, sizeof(id), sid)) return;
        Settings::setCard(id, "group", gid);
        nameLike(id, *m.subject);
        CardAdded::moveToPage(sid, id);
        CardAdded::keepHome(sid, clName(m.from), fromHas ? fg.id : nullptr);
    }
    if (last) CardAdded::hide(m.from->id());   // an empty cluster hides itself (owner's 15)
    done();
}

void doCluster() {
    collectTargets();
    if (m.pick >= s_nTgt || !m.subject) return;
    const Target T = s_tgt[m.pick];
    const char *sid = m.subject->id();
    // MOVED BACK HOME BY HAND IS A RESTORE (owner's 17).
    if (m.move && isMade(sid) && CardAdded::isMoveOf(sid)) {
        const CardSpec *hc = homeCluster(CardAdded::fromOf(sid));
        if (hc && !strcmp(hc->id, T.id)) {
            const bool last = lastTile();
            CardAdded::unmake(sid);
            if (T.hidden) CardAdded::restore(T.id);
            if (last) CardAdded::hide(m.from->id());
            done();
            return;
        }
    }
    const bool last = m.move && lastTile();
    char id[CARD_ID_MAX + 1];
    if (!m.move) {
        if (!CardAdded::addTo(T.id, T.area, T.gid, *m.ent, id, sizeof(id), origOf(*m.subject))) return;
        nameLike(id, *m.subject);
    } else if (isMade(sid)) {
        if (!CardAdded::rehome(sid, T.id, T.area, T.gid)) return;
    } else {
        if (!CardAdded::addTo(T.id, T.area, T.gid, *m.ent, id, sizeof(id), sid)) return;
        nameLike(id, *m.subject);
        if (m.from) {
            CardGroups::Group fg;
            CardAdded::moveToPage(sid, id);
            CardAdded::keepHome(sid, clName(m.from), CardGroups::ofCard(*m.from, fg) ? fg.id : nullptr);
        } else {
            CardAdded::moveOut(sid, id);
        }
    }
    if (T.hidden) CardAdded::restore(T.id);    // a hidden cluster is used again
    if (last) CardAdded::hide(m.from->id());   // an empty cluster hides itself (owner's 15)
    done();
}

void doGroup() {
    char gid[48];
    if (!m.subject || !groupFor(gid, sizeof(gid))) return;
    regroup(m.subject->id(), gid, m.subject->sheetArea());
    CardGroups::apply(*m.subject, m.subject->pageAreaColor());
    done();
}

// --- The dialogs -----------------------------------------------------------------------
void askDash() {
    collectGroups();
    char a[96], b[96];
    const char *from = m.from ? clName(m.from) : m.parent ? m.parent->shownName(CardLabel::LBL_NAME) : "";
    lines();
    if (!m.move && m.from)
        line("This card will be copied from the cluster %s and placed onto the dashboard as a separate card.",
             acc(a, sizeof(a), from));
    else if (!m.move)
        line("This card will be copied from the group card %s and placed onto the dashboard as a separate card.",
             acc(a, sizeof(a), from));
    else
        line("This card will be removed from the cluster %s and placed onto the dashboard.", acc(a, sizeof(a), from));
    if (m.pick == groupNone())
        line("The card will not be a member of any group.");
    else if (m.pick == s_cur)
        line(m.move ? "The card will still be a member of the %s group, certain group settings may affect this card."
                    : "The card will still be a member of the %s group but certain settings can be applied "
                      "separately to each card.", acc(a, sizeof(a), pickName()));
    else
        line("The card will become a member of the group %s.", acc(a, sizeof(a), pickName()));
    line(RESTORABLE);
    if (m.move && lastTile())
        line("The cluster %s will be empty after moving this card. An empty cluster will be hidden automatically "
             "but can be used again by adding cards to it.", acc(b, sizeof(b), from));
    confirmAsk(Kit::DialogRole::DLG_CONFIRM, "Add card to dashboard?", s_lines, s_nLines, m.move ? "Move" : "Copy",
               Kit::ButtonRole::BTN_CONFIRM, doDash);
}

void askCluster() {
    collectTargets();
    if (m.pick >= s_nTgt) return;
    const Target &T = s_tgt[m.pick];
    char a[96], b[96];
    lines();
    if (!m.move && m.from)
        line("This card will be copied to the cluster %s and a copy will remain in the %s cluster.",
             acc(a, sizeof(a), T.name), acc(b, sizeof(b), clName(m.from)));
    else if (!m.move)
        line("This card will be copied to the cluster %s and a copy will remain on this page.", acc(a, sizeof(a), T.name));
    else if (m.from)
        line("This card will be removed from the cluster %s and moved to the cluster %s.",
             acc(a, sizeof(a), clName(m.from)), acc(b, sizeof(b), T.name));
    else
        line("This card will be removed from this page and moved to the cluster %s.", acc(a, sizeof(a), T.name));
    if (!m.move)
        line("The new copy will change its group membership to be a member of %s. Changes made to one copy will "
             "not apply to the other.", acc(a, sizeof(a), T.gname));
    else
        line("The moved card will become a member of the %s group, and settings applied to that cluster may be "
             "propagated to the new card.", acc(a, sizeof(a), T.gname));
    line(RESTORABLE);
    if (m.move && lastTile())
        line("The cluster %s will be empty after moving this card. An empty cluster will be hidden automatically "
             "but can be used again by adding cards to it.", acc(b, sizeof(b), clName(m.from)));
    confirmAsk(Kit::DialogRole::DLG_CONFIRM, "Add card to cluster?", s_lines, s_nLines, m.move ? "Move" : "Copy",
               Kit::ButtonRole::BTN_CONFIRM, doCluster);
}

void askGroup() {
    collectGroups();
    char a[96], b[96];
    lines();
    const bool toNone = m.pick == groupNone();
    if (s_cur == 0xFF)
        line("This will add the card to the group %s.", acc(a, sizeof(a), pickName()));
    else if (toNone)
        line("This will remove the card from group %s, leaving it with no group.", acc(a, sizeof(a), s_grp[s_cur].name));
    else
        line("This will remove the card from group %s and add it to the group %s.",
             acc(a, sizeof(a), s_grp[s_cur].name), acc(b, sizeof(b), pickName()));
    line(RESTORABLE);
    confirmAsk(Kit::DialogRole::DLG_CONFIRM, "Change group for this card?", s_lines, s_nLines, "Confirm",
               Kit::ButtonRole::BTN_CONFIRM, doGroup);
}

void ask() {
    if (s.phase != PopupPhase::PHASE_OPEN) return;
    if (m.mode == MoveMode::MOVE_CLUSTER) askCluster();
    else if (m.mode == MoveMode::MOVE_DASH) askDash();
    else askGroup();
}

// NEW GROUP: its name on the keyboard first, then the dialog.
void named(const char *words, void *) {
    if (!words || !words[0]) return;
    snprintf(s_newName, sizeof(s_newName), "%s", words);
    lv_async_call([](void *) { ask(); }, nullptr);   // once the keyboard has gone
}

void next() {
    if (s.phase != PopupPhase::PHASE_OPEN) return;
    if (m.mode == MoveMode::MOVE_CLUSTER) { ask(); return; }
    collectGroups();
    if (m.pick == groupNew()) {
        Kit::KeyboardSpec k = {};
        k.title  = "New group name";
        k.maxLen = sizeof(s_newName) - 1;
        k.save   = named;
        Kit::keyboard(k);
        return;
    }
    // Its own group again: nothing changes.
    if (m.mode == MoveMode::MOVE_GROUP && (m.pick == s_cur || (s_cur == 0xFF && m.pick == groupNone()))) {
        showView(PopupView::VIEW_VISIBILITY);
        return;
    }
    ask();
}

void nextCb(lv_event_t *) { lv_async_call([](void *) { next(); }, nullptr); }

void gridCb(lv_event_t *ev) {
    m.pick = (uint8_t)(uintptr_t)lv_event_get_user_data(ev);
    Kit::chipGridLit(s_grid, m.pick);
}

void copyCb(lv_event_t *ev) {
    m.move = (uintptr_t)lv_event_get_user_data(ev) == 1;
    Kit::chipGridLit(s_copy, m.move ? 1 : 0);   // a chip row's children are its chips
}

// --- The views --------------------------------------------------------------------------
lv_obj_t *words(lv_obj_t *list, KS role, const char *w) {
    lv_obj_t *l = Kit::label(list, role);
    lv_obj_set_width(l, lv_pct(100));
    lv_obj_set_style_text_align(l, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_long_mode(l, LV_LABEL_LONG_WRAP);
    lv_label_set_text(l, w);
    return l;
}

// Copy and Move (Copy first) beside a blue Next, on one line; Next alone for a
// group. A group card's member is copied only.
void nextRow(lv_obj_t *list, bool copyMove, bool copyOnly) {
    lv_obj_t *row = Kit::buttonRow(list);
    s_copy = nullptr;
    if (copyMove) {
        static const char *const W[] = { "Copy", "Move" };
        if (copyOnly) m.move = false;
        s_copy = Kit::chipRow(row, W, copyOnly ? 1 : 2, m.move ? 1 : 0, 0, copyCb);
    }
    const int32_t w = Kit::textW("Next", UI::type().NAME) + 2 * mm(UI::shape().S9);
    Kit::button(row, Kit::ButtonRole::BTN_CONFIRM, "Next", w, Kit::controlH(), nextCb);
}

// The entity is on the page already, as a card of its own (round 7: no second
// card of one entity on the dashboard).
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

void openMove(MoveMode mode, Card *subject, ClusterCard *from, Card *parent, const Entity *e) {
    m = Move{};
    m.mode    = mode;
    m.subject = subject;
    m.from    = from;
    m.parent  = parent;
    m.ent     = e;
    s_newName[0] = '\0';
    if (mode != MoveMode::MOVE_CLUSTER) {
        collectGroups();
        m.pick = s_cur == 0xFF ? groupNone() : s_cur;
    }
    if (s.phase == PopupPhase::PHASE_OPEN) showView(PopupView::VIEW_JOIN);
}

}  // namespace

// --- From Visibility ----------------------------------------------------------------------
void moveOpen(Card *subject, ClusterCard *from, bool toCluster) {
    if (!subject || !subject->hasId()) return;
    const MoveMode mode = toCluster ? MoveMode::MOVE_CLUSTER : from ? MoveMode::MOVE_DASH : MoveMode::MOVE_GROUP;
    openMove(mode, subject, from, nullptr, subject->firstEntity());
}

void visibilityJoinOpen(Card *parent, const Entity *member, Card *memberCard) {
    if (parent && !strcmp(parent->typeName(), "cluster") && memberCard)
        openMove(MoveMode::MOVE_DASH, memberCard, static_cast<ClusterCard *>(parent), nullptr, member);
    else
        openMove(MoveMode::MOVE_DASH, nullptr, nullptr, parent, member);
}

const char *visibilityJoinTitle() {
    if (m.mode == MoveMode::MOVE_CLUSTER) return m.from ? "Change cluster" : "Add to cluster";
    return m.mode == MoveMode::MOVE_GROUP ? "Change group" : "Add to dashboard";
}

bool visibilityJoinBack() { return false; }

void buildJoin() {
    s_grid = s_copy = nullptr;
    lv_obj_t *list = Kit::chipTable(s.stage, listW(), nullptr, 0, nullptr);
    if (!m.ent) return;
    if (m.mode == MoveMode::MOVE_CLUSTER) {
        collectTargets();
        if (m.pick >= s_nTgt) m.pick = 0;
        words(list, KS::KS_LABEL_NAME, "Select target cluster:");
        if (s_nTgt) s_grid = Kit::chipGrid(list, listW(), s_tgtWords, s_nTgt, m.pick, gridCb);
        words(list, KS::KS_LABEL_TAG_DIM, "A card cannot be added to a cluster if that cluster already contains a copy.");
        if (!s_nTgt) { words(list, KS::KS_LABEL_TAG_DIM, "This card cannot be added to any clusters on this page."); return; }
        nextRow(list, true, false);
        return;
    }
    const bool member = !m.subject;   // a group card's member
    if (m.mode == MoveMode::MOVE_DASH) {
        const bool there = member ? (m.parent && m.parent->hasId() && CardAdded::addedFor(m.parent->id(), m.ent->desc.id))
                                  : false;
        if (there || onPage(lv_screen_active(), m.ent->desc.id)) {
            words(list, KS::KS_LABEL_TAG_DIM, "You cannot add this card to the dashboard as a copy already exists there.");
            return;
        }
    }
    collectGroups();
    if (m.pick > groupNone()) m.pick = groupNone();
    words(list, KS::KS_LABEL_NAME, "Select group for this card:");
    s_grid = Kit::chipGrid(list, listW(), s_grpWords, (uint8_t)(s_nGrp + 2), m.pick, gridCb);
    words(list, KS::KS_LABEL_TAG_DIM, "Select 'New group' to create a new group for this card. Select 'None' to add "
                                      "the card with no group assigned.");
    if (m.mode == MoveMode::MOVE_DASH && member)
        words(list, KS::KS_LABEL_TAG_DIM, "Members of a group card cannot be moved, only copied.");
    nextRow(list, m.mode == MoveMode::MOVE_DASH, member);
}

// Add to a cluster from the page (VIEW_ADD_TO, /popup?view=9): the same view.
void buildAddTo() {
    Card *c = cardOf(h.surface);
    if (m.mode != MoveMode::MOVE_CLUSTER || m.subject != c) {
        m = Move{};
        m.mode = MoveMode::MOVE_CLUSTER;
        m.subject = c;
        m.ent = c ? c->firstEntity() : nullptr;
    }
    buildJoin();
}

// /popup's hands: the views' choices made and done, no dialog.
bool visibilityAddTo(uint8_t pick, bool move) {
    Card *c = cardOf(h.surface);
    if (!c || c->mini() || s.phase != PopupPhase::PHASE_OPEN) return false;
    openMove(MoveMode::MOVE_CLUSTER, c, nullptr, nullptr, c->firstEntity());
    collectTargets();
    if (pick >= s_nTgt) return false;
    m.pick = pick;
    m.move = move;
    doCluster();
    return true;
}

bool visibilityJoin(uint8_t pick, bool move) {
    if (m.mode != MoveMode::MOVE_DASH || !m.ent || s.phase != PopupPhase::PHASE_OPEN) return false;
    collectGroups();
    if (pick >= s_nGrp + 2 || pick == groupNew()) return false;
    m.pick = pick;
    m.move = move && m.subject;
    doDash();
    return true;
}

// --- Restore ------------------------------------------------------------------------------
namespace {

char s_rid[CARD_ID_MAX + 1] = "";

// The card Restore acts on: the card itself, or - a member moved out, seen in its
// cluster's Members - the card it became. Null: nothing to restore.
const char *restoreTarget(const Card *c) {
    if (!c || !c->hasId()) return nullptr;
    if (CardAdded::isMovedToPage(c->id())) return CardAdded::becameOf(c->id());
    if (isMade(c->id())) return c->id();
    char v[48];
    return Settings::card(c->id(), "orig_group", v, sizeof(v)) ? c->id() : nullptr;
}

// A card's name as it was built: one typed on it, else the dashboard's, else HA's.
void builtName(const char *id, char *out, size_t cap) {
    if (Settings::card(id, "name", out, cap) && out[0]) return;
    const CardSpec *cs = CardAdded::specOf(id);
    if (cs && cs->label && cs->label[0]) { snprintf(out, cap, "%s", cs->label); return; }
    const Entity *e = (cs && cs->primaries[0] && s.reg) ? s.reg->find(cs->primaries[0]) : nullptr;
    snprintf(out, cap, "%s", e ? e->desc.name : id);
}

// Its name now: one kept on it, else its entity's.
void nameNow(const char *id, char *out, size_t cap) {
    if (Settings::card(id, "name", out, cap) && out[0]) return;
    char eid[ENTITY_ID_MAX];
    const Entity *e = (Settings::card(id, "entity", eid, sizeof(eid)) && s.reg) ? s.reg->find(eid) : nullptr;
    snprintf(out, cap, "%s", e ? e->desc.name : "");
}

bool pageGroup(const char *id, CardGroups::Group &g) {
    const CardSpec *cs = CardAdded::specOf(id);
    return CardGroups::ofId(id, cs ? cs->area : nullptr, g);
}

void restoreClosed() {
    visibilityWantRebuild();
    CardPopup::close();
}

void goDelete() {
    CardAdded::unmake(s_rid);
    restoreClosed();
}

void goGroup() {
    char was[48];
    if (!Settings::card(s_rid, "orig_group", was, sizeof(was))) return;
    Settings::setCard(s_rid, "group", strcmp(was, "inherit") ? was : nullptr);
    Settings::setCard(s_rid, "orig_group", nullptr);
    Card *c = cardOf(h.surface);
    if (c && c->hasId() && !strcmp(c->id(), s_rid)) CardGroups::apply(*c, c->pageAreaColor());
    restoreClosed();
}

void goMove() {
    const char *o = CardAdded::fromOf(s_rid);
    char orig[CARD_ID_MAX + 1];
    snprintf(orig, sizeof(orig), "%s", o ? o : "");
    const CardSpec *hc = homeCluster(orig);
    if (hc) {
        char hn[48], hg[48], now[48];
        CardAdded::home(orig, hn, sizeof(hn), hg, sizeof(hg));
        clusterNameNow(hc->id, now, sizeof(now));
        if (hn[0] && strcmp(hn, now)) {
            // RENAMED OR REPURPOSED (owner's 16): onto the dashboard, in its group as
            // it was, with its name as it was.
            if (CardAdded::isMember(s_rid) && !CardAdded::toPage(s_rid, hc->id, hc->place.priority)) return;
            if (hg[0]) Settings::setCard(s_rid, "group", hg);
            char bn[48];
            builtName(orig, bn, sizeof(bn));
            Settings::setCard(s_rid, "name", bn);
            restoreClosed();
            return;
        }
        const bool hidden = CardAdded::isHidden(hc->id);
        CardAdded::unmake(s_rid);
        if (hidden) CardAdded::restore(hc->id);   // the cluster is used again
        restoreClosed();
        return;
    }
    CardAdded::unmake(s_rid);   // a page card: back on its page
    restoreClosed();
}

const char *const RESTORE_TITLE = "Restore this card to its original configuration?";

}  // namespace

bool restoreOffered(const Card *c) { return restoreTarget(c) != nullptr; }

void restoreAsk(const Card *c) {
    const char *t = restoreTarget(c);
    if (!t || s.phase != PopupPhase::PHASE_OPEN) return;
    snprintf(s_rid, sizeof(s_rid), "%s", t);
    char a[96], b[96];
    lines();
    CardGroups::Group g;
    if (!isMade(s_rid)) {
        // A GROUP CHANGED, NOTHING MOVED: its group back.
        char was[48];
        Settings::card(s_rid, "orig_group", was, sizeof(was));
        const CardSpec *cs = CardAdded::specOf(s_rid);
        const bool has = !strcmp(was, "inherit") ? CardGroups::ofId(nullptr, cs ? cs->area : c->sheetArea(), g)
                       : strcmp(was, "none") ? CardGroups::byId(was, g) : false;
        if (has) line("This card was originally a member of the %s group.", acc(a, sizeof(a), g.name));
        else     line("This card was originally not a member of any group.");
        line("Tap 'Restore' to assign this card back to its original group %s.", acc(a, sizeof(a), has ? g.name : "None"));
        confirmAsk(Kit::DialogRole::DLG_DESTRUCTIVE, RESTORE_TITLE, s_lines, s_nLines, "Restore",
                   Kit::ButtonRole::BTN_DESTRUCTIVE, goGroup);
        return;
    }
    const char *o = CardAdded::fromOf(s_rid);
    const CardSpec *hc = homeCluster(o);
    if (!CardAdded::isMoveOf(s_rid)) {
        // A COPY, ITS ORIGINAL STILL HOME: deleted.
        if (hc) {
            char now[48];
            clusterNameNow(hc->id, now, sizeof(now));
            line("This card was originally a member of the %s cluster.", acc(a, sizeof(a), now));
        } else if (o && pageGroup(o, g)) {
            line("This card was originally a member of the %s group.", acc(a, sizeof(a), g.name));
        }
        line("The original copy of this card still exists in its original state, tap 'Delete' to confirm you "
             "wish to delete this copy.");
        confirmAsk(Kit::DialogRole::DLG_DESTRUCTIVE, RESTORE_TITLE, s_lines, s_nLines, "Delete",
                   Kit::ButtonRole::BTN_DESTRUCTIVE, goDelete);
        return;
    }
    // A MOVE: home again, with its name as it was.
    char bn[48], cur[48], nameLine[120] = "", nameTail[120] = "";
    builtName(o, bn, sizeof(bn));
    nameNow(s_rid, cur, sizeof(cur));
    if (bn[0] && strcmp(bn, cur)) {
        char n1[96];
        snprintf(nameLine, sizeof(nameLine), " Its name was originally %s.", acc(n1, sizeof(n1), bn));
        snprintf(nameTail, sizeof(nameTail), " and assume its original name %s", acc(n1, sizeof(n1), bn));
    }
    char l1[240], l2[240];
    if (hc) {
        char hn[48], hg[48], now[48];
        CardAdded::home(o, hn, sizeof(hn), hg, sizeof(hg));
        clusterNameNow(hc->id, now, sizeof(now));
        if (!hn[0]) snprintf(hn, sizeof(hn), "%s", now);
        const bool renamed = strcmp(hn, now) != 0;
        snprintf(l1, sizeof(l1), "This card was originally a member of the %s cluster.%s", acc(a, sizeof(a), hn), nameLine);
        line("%s", l1);
        if (renamed) {
            const bool has = hg[0] && CardGroups::byId(hg, g);
            snprintf(l2, sizeof(l2), "Tap 'Restore' to confirm you wish to restore it to the %s group%s.",
                     acc(b, sizeof(b), has ? g.name : hn), nameTail);
            line("%s", l2);
            line("The original cluster has been renamed or repurposed and the card cannot be restored there.");
        } else {
            snprintf(l2, sizeof(l2), "Tap 'Restore' to confirm you wish to restore it to the %s cluster%s.",
                     acc(b, sizeof(b), hn), nameTail);
            line("%s", l2);
            if (CardAdded::isHidden(hc->id))
                line("The original cluster had become hidden, restoring this card will reactivate the cluster.");
        }
    } else {
        const bool has = o && pageGroup(o, g);
        if (has) snprintf(l1, sizeof(l1), "This card was originally a member of the %s group.%s", acc(a, sizeof(a), g.name), nameLine);
        else     snprintf(l1, sizeof(l1), "This card was originally not a member of any group.%s", nameLine);
        line("%s", l1);
        snprintf(l2, sizeof(l2), "Tap 'Restore' to confirm you wish to restore it to the %s group%s.",
                 acc(b, sizeof(b), has ? g.name : "None"), nameTail);
        line("%s", l2);
    }
    confirmAsk(Kit::DialogRole::DLG_DESTRUCTIVE, RESTORE_TITLE, s_lines, s_nLines, "Restore",
               Kit::ButtonRole::BTN_DESTRUCTIVE, goMove);
}

}  // namespace popup
