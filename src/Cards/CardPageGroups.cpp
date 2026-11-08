// A PAGE'S GROUP CARDS (K88-K92, K154-K156): CardPage's members that make a
// group card - a cluster, its one card or its summary - and the cluster the
// card window asks for (makeGroup()). Split from CardPage.cpp to keep both
// under the structure check's 1,000 lines.
#include "Cards/CardPage.h"
#include "Cards/CardTypes.h"
#include "Cards/CardCatalog.h"
#include "Cards/CardGroups.h"
#include "Cards/CardAdded.h"     // a cluster's members added on the device (K91)
#include "Cards/ClusterCard.h"
#include "Cards/CardPopup.h"     // a card's Long press opens, by name (K156)
#include "Layout/Cluster.h"
#include "UI/UITokens.h"
#include "Settings.h"
#include <Arduino.h>
#include <string.h>

// A GROUP CARD MADE ON THE DEVICE (K88-K92): its presentation is the
// saved "present", else the dashboard's. As a CLUSTER its members become mini
// tiles (the visible ones) and header values (the `slots`); it asks for as many
// cells as its tiles need (Layout::clusterSpan(), until the arranger). A cluster
// that cannot fit on this board is shown as its ONE CARD (K92 rule 6): the
// members that switch together - lights and switches (rule 5) - behind one tile
// whose tap acts on them all.
namespace {
// A GROUP CARD'S MEMBERS, and where each stands (makeGroupCard(), newCluster()):
// the dashboard's, then those added on the device (K91; CardAdded); the header's
// by id, highest priority first - the saved list, else the dashboard's. Filled in
// place (slotIds point into saved).
struct GroupMembers {
    const CardSpec *mem[GROUP_MEMBERS_MAX];
    uint8_t     n = 0;
    const char *slotIds[CLUSTER_SLOTS_MAX] = {nullptr};
    char        saved[CLUSTER_SLOTS_MAX * (CARD_ID_MAX + 1) + 1] = "";

    // A MEMBER MOVED OUT TO THE PAGE ("hidden": "moved", K112): its card is on
    // the page now, so it is no tile, no header value, and
    // not behind the one card - still a member, so Members lists it.
    static bool movedOut(const CardSpec &m) { return cardIdUsable(m.id) && CardAdded::isMovedToPage(m.id); }
    int8_t slotOf(const char *id) const {
        if (id && cardIdUsable(id) && CardAdded::isMovedToPage(id)) return -1;
        for (uint8_t k = 0; k < CLUSTER_SLOTS_MAX; k++)
            if (id && slotIds[k] && !strcmp(slotIds[k], id)) return (int8_t)k;
        return -1;
    }
    // A member's tile: shown unless it is in the header (K92 rule 7); "hidden"
    // saved on it says otherwise either way (K104's Visible) - "yes", or "moved".
    bool tileOf(const CardSpec &m) const {
        bool tile = slotOf(m.id) < 0;
        char h[8];
        if (cardIdUsable(m.id) && Settings::card(m.id, "hidden", h, sizeof(h))) tile = !strcmp(h, "no");
        return tile;
    }
    // Hidden from the cluster: no tile, not in its header, not moved out.
    bool hiddenOf(const CardSpec &m) const { return !tileOf(m) && slotOf(m.id) < 0 && !movedOut(m); }
};

void groupMembers(const CardSpec &cs, GroupMembers &g) {
    const bool hasId = cardIdUsable(cs.id);
    g.n = 0;
    for (uint8_t k = 0; k < cs.memberCount && g.n < GROUP_MEMBERS_MAX; k++) g.mem[g.n++] = &cs.members[k];
    if (hasId) g.n += CardAdded::membersIn(cs.id, g.mem + g.n, (uint8_t)(GROUP_MEMBERS_MAX - g.n));
    if (hasId && Settings::card(cs.id, "slots", g.saved, sizeof(g.saved))) {
        uint8_t k = 0;
        for (char *p = g.saved; *p && k < CLUSTER_SLOTS_MAX;) {
            g.slotIds[k++] = p;
            char *comma = strchr(p, ',');
            if (!comma) break;
            *comma = '\0';
            p = comma + 1;
        }
    } else {
        for (uint8_t k = 0; k < CLUSTER_SLOTS_MAX; k++) g.slotIds[k] = cs.slots[k];
    }
}

// NOT ALL LIGHTS AND SWITCHES (K188): its solo card's tap toggles nothing. The
// header's sensors do not count - they are the group view's header, not its cards.
bool groupMixed(const GroupMembers &g, EntityRegistry &reg) {
    for (uint8_t k = 0; k < g.n; k++) {
        const char *pid = g.mem[k]->primaries[0];
        const Entity *e = pid ? reg.find(pid) : nullptr;
        if (!e || GroupMembers::movedOut(*g.mem[k]) || g.slotOf(g.mem[k]->id) >= 0) continue;
        if (!(e->desc.writable && (e->desc.kind == EntityKind::LIGHT || e->desc.kind == EntityKind::SWITCH))) return true;
    }
    return false;
}

bool isGroupSpec(const CardSpec &cs) {
    return (cs.memberCount && cs.members) || (cs.present == GroupPresent::PRESENT_CLUSTER && !cs.primaries[0]);
}
}  // namespace

// THE CLUSTER ITSELF (for the page and the card window): its members made and
// configured, its header's style, its tap. `allTiles`: the group view's (K154).
ClusterCard *CardPage::newCluster(const CardSpec &cs, const PageSpec &spec, EntityRegistry &reg, bool allTiles) {
    char v[64];
    const bool hasId = cardIdUsable(cs.id);
    GroupMembers g;
    groupMembers(cs, g);
    ClusterCard *cl = new ClusterCard();
    // ITS OWN CARD STATE FOR ITS TILES: "card_state", a StateCardFill
    // by name in GUIManagerTweaks' order; none, the device's.
    int8_t ownFill = -1;
    if (hasId && Settings::card(cs.id, "card_state", v, sizeof(v))) {
        static const char *const FILLS[] = { "fill", "glow", "bracket", "corner" };
        for (int8_t f = 0; f < 4; f++) if (!strcmp(v, FILLS[f])) ownFill = f;
    }
    // THE GROUP VIEW HAS EVERY MEMBER (K154): the hidden ones as tiles too, last
    // and marked. The page has the ones that earn their space.
    for (int pass = 0; pass < (allTiles ? 2 : 1); pass++)
        for (uint8_t k = 0; k < g.n; k++) {
            const CardSpec &ms = *g.mem[k];
            const bool hidden = allTiles && g.hiddenOf(ms);
            if (allTiles && hidden != (pass == 1)) continue;
            Card *m = makeCard(ms, spec, reg, k);
            if (!m) continue;
            m->setOwnFill(ownFill);
            m->setMini(true);
            m->setHeaderStyle(CardHeaderStyle::HDR_NONE);
            if (cl->addMember(m, g.tileOf(ms) || hidden, g.slotOf(ms.id)) && hidden)
                cl->markHidden((uint8_t)(cl->memberCount() - 1));
        }
    if (hasId) cl->setId(cs.id);
    if (cs.label) cl->setLabel(cs.label);
    cl->setSheetArea(cs.area);
    CardGroups::apply(*cl, spec.areaColor);
    // A CLUSTER FILLS ITS CELLS: Float and Tag take room from a solo card for
    // its label, and a cluster has none - its header names it. Reserving the
    // tag's room above it would shrink its tiles.
    cl->setHeaderStyle(CardHeaderStyle::HDR_NONE);
    cl->setShowArea(false);
    // The header's style (K102): the cluster's own, else the page's, else the spec's.
    ClusterStyle st = spec.clusterStyle;
    if (spec.slug && Settings::page(spec.slug, "cl_style", v, sizeof(v))) clusterStyleFromName(v, st);
    if (hasId && Settings::card(cs.id, "cl_style", v, sizeof(v))) clusterStyleFromName(v, st);
    cl->setStyle(st);
    cl->setIcons(!(hasId && Settings::card(cs.id, "cl_icons", v, sizeof(v)) && !strcmp(v, "off")));
    // ITS TAP: the one chosen in SETTINGS, read here because a cluster is not
    // made by makeCard(). Else its group view (K154); a long press opens
    // Members, which lists every member, hidden ones too (refines K107:
    // opening a list switches nothing).
    TapAction ta = TapAction::TAP_GROUP;
    if (hasId && Settings::card(cs.id, "tap", v, sizeof(v))) tapActionFromName(v, ta);
    // ONLY WHAT A CLUSTER OFFERS (K156): a saved Details view would mean "its
    // window" (K129: a cluster has none), and a Toggle is no cluster's: both
    // fall back to its group view.
    if (ta != TapAction::TAP_MEMBERS && ta != TapAction::TAP_HISTORY && ta != TapAction::TAP_NOTHING)
        ta = TapAction::TAP_GROUP;
    cl->setTapAction(ta);
    uint8_t lo;   // Long press opens (K156)
    if (hasId && Settings::card(cs.id, "long", v, sizeof(v)) && CardPopup::openViewFromName(v, lo)) cl->setLongOpen(lo);
    return cl;
}

ClusterCard *CardPage::makeGroup(const char *id, bool allTiles) {
    if (!id || !id[0] || !_reg || !_spec.cards) return nullptr;
    for (uint8_t i = 0; i < _spec.count; i++) {
        const CardSpec &cs = _spec.cards[i];
        if (!cs.id || strcmp(cs.id, id) || !isGroupSpec(cs)) continue;
        ClusterCard *cl = newCluster(cs, _spec, *_reg, allTiles);
        if (allTiles) cl->setBinder(_binder);   // the group view is live; a model only read
        return cl;
    }
    return nullptr;
}

Card *CardPage::makeGroupCard(const CardSpec &cs, const PageSpec &spec, EntityRegistry &reg) {
    char v[64];
    const bool hasId = cardIdUsable(cs.id);
    GroupPresent pres = cs.present;
    if (hasId && Settings::card(cs.id, "present", v, sizeof(v))) groupPresentFromName(v, pres);

    GroupMembers gm;
    groupMembers(cs, gm);
    const CardSpec *const *mem = gm.mem;
    const uint8_t nMem = gm.n;
    auto movedOut = [](const CardSpec &m) { return GroupMembers::movedOut(m); };

    if (pres == GroupPresent::PRESENT_CLUSTER) {
        uint8_t nTiles = 0;
        for (uint8_t k = 0; k < nMem; k++)
            if (mem[k]->primaries[0] && reg.find(mem[k]->primaries[0]) && gm.tileOf(*mem[k])) nTiles++;
        Layout::ClusterGeom g;
        g.cellW   = widthOf(_sub) > 0 ? widthOf(_sub) : (int32_t)UI::grid().cellW;
        g.cellH   = (int32_t)UI::grid().cellH;
        g.gap     = _gap;
        g.chromeW = ClusterCard::chromeW();
        g.chromeH = ClusterCard::chromeH();   // whole cells: no tag's room
        g.tileGap = UI::mm(UI::shape().C_TILE_GAP);
        g.minTile = UI::mm(UI::shape().C_TILE_MIN);
        const uint8_t cellCols = _uCols / _sub ? _uCols / _sub : 1;
        // FEWER THAN TWO PINNED SHOWS ITS SOLO CARD (K188): the
        // pins are kept, and a second pin makes it a group again.
        const Layout::ClusterSpan sp = Layout::clusterSpan(nTiles ? nTiles : 1, g, cellCols, _maxRows);
        if (nTiles >= 2 && sp.fits) {
            ClusterCard *cl = newCluster(cs, spec, reg, false);
            CardPlacement pl = cs.place;
            pl.prefSpanX = pl.minSpanX = (uint8_t)(sp.cols * _sub);
            pl.prefSpanY = pl.minSpanY = (uint8_t)(sp.rows * _sub);
            cl->setPlacement(pl);
            cl->setBinder(_binder);
            Serial.printf("[Cards] cluster %s: %u members, %u tiles, %ux%u cells\n",
                          cs.label ? cs.label : "?", (unsigned)cl->memberCount(), (unsigned)nTiles,
                          (unsigned)sp.cols, (unsigned)sp.rows);
            return cl;
        }
        Serial.printf("[Cards] cluster %s: %u tiles - fewer than two, or too many for this page: shown as its card\n",
                      cs.label ? cs.label : "?", (unsigned)nTiles);
    }

    // AS ONE CARD: the members that switch together (lights and switches),
    // else every member, behind one tile. AS A SUMMARY (K98): every member -
    // one of a kind - with each one's name for the words under the icon.
    CardSpec one = {};
    one.id    = cs.id;
    one.label = cs.label ? cs.label : cs.area;   // a cluster is named by its group: the one card is too
    one.area  = cs.area;
    one.place = cs.place;
    one.groupOn = cs.groupOn;
    uint8_t n = 0;
    if (pres == GroupPresent::PRESENT_SUMMARY) {
        for (uint8_t k = 0; k < nMem && n < CARD_PRIMARY_MAX; k++)
            if (mem[k]->primaries[0] && reg.find(mem[k]->primaries[0]) && !movedOut(*mem[k]))
                one.primaries[n++] = mem[k]->primaries[0];
        if (!n) return nullptr;
        SummaryCard *sc = new SummaryCard();
        for (uint8_t k = 0; k < nMem; k++)
            if (mem[k]->primaries[0] && mem[k]->label) sc->setMemberName(mem[k]->primaries[0], mem[k]->label);
        Card *c = makeCard(one, spec, reg, 0, sc);
        // ITS TAP OPENS ITS MEMBERS (K98) - never a door's - unless the owner chose.
        char t[16];
        if (c && !(hasId && Settings::card(cs.id, "tap", t, sizeof(t)))) c->setTapAction(TapAction::TAP_MEMBERS);
        return c;
    }
    // ITS FACE (K191): one member chosen in its GROUP panel ("face", that member's
    // id); else the members that switch together, else every member.
    const CardSpec *faceMem = nullptr;
    if (hasId && Settings::card(cs.id, "face", v, sizeof(v)))
        for (uint8_t k = 0; k < nMem && !n; k++)
            if (mem[k]->id && !strcmp(mem[k]->id, v) && mem[k]->primaries[0] && reg.find(mem[k]->primaries[0]) &&
                !movedOut(*mem[k])) {
                one.primaries[n++] = mem[k]->primaries[0];
                faceMem = mem[k];
            }
    for (int pass = 0; pass < 2 && !n; pass++)
        for (uint8_t k = 0; k < nMem && n < CARD_PRIMARY_MAX; k++) {
            const char *pid = mem[k]->primaries[0];
            const Entity *e = pid ? reg.find(pid) : nullptr;
            if (!e || movedOut(*mem[k])) continue;
            const bool switches = e->desc.writable &&
                                  (e->desc.kind == EntityKind::LIGHT || e->desc.kind == EntityKind::SWITCH);
            if (pass == 0 && !switches) continue;
            one.primaries[n++] = pid;
        }
    Card *c = n ? makeCard(one, spec, reg, 0) : nullptr;
    if (!c) return nullptr;
    c->setGroupCard(true);   // its GROUP panel (K191, K155)
    c->setGroupMixed(groupMixed(gm, reg));   // K188: a tap toggles only lights and switches
    // A FACE TOGGLES THE WHOLE GROUP: its other lights and switches.
    if (faceMem)
        for (uint8_t k = 0; k < nMem; k++) {
            const Entity *e = mem[k] != faceMem && mem[k]->primaries[0] ? reg.find(mem[k]->primaries[0]) : nullptr;
            if (e && !movedOut(*mem[k]) && e->desc.writable &&
                (e->desc.kind == EntityKind::LIGHT || e->desc.kind == EntityKind::SWITCH))
                c->addGroupSwitch(e);
        }
    // A FACE CHOSEN: THE CARD IS NAMED AS THAT MEMBER - "Front Room
    // Temperature", its card label still the group's ("Indoor Temp").
    if (faceMem) {
        char nm[ENTITY_NAME_MAX];
        const Entity *fe = reg.find(faceMem->primaries[0]);
        if (cardIdUsable(faceMem->id) && Settings::card(faceMem->id, "name", nm, sizeof(nm)) && nm[0]) {
            c->setLabel(nm);   // the name typed on the member
        } else if (fe && fe->learnt) {
            // A LEARNT ENTITY'S NAME ARRIVES AFTER THE PAGE IS BUILT (its entity id
            // until HA's first state): no label of its own, so
            // the card reads its entity's name at every render, as Deck Light does.
            c->setLabel("");
        } else {
            // A declared one's own name is fixed; HA's is the owner's word for it.
            snprintf(nm, sizeof(nm), "%s", fe && fe->sourceName[0] ? fe->sourceName
                                         : fe && fe->desc.name[0] ? fe->desc.name : faceMem->label ? faceMem->label : "");
            if (nm[0]) c->setLabel(nm);
        }
    }
    // ITS OWN TAP AND LONG PRESS (K156): "card_tap" and "card_long" - makeCard()
    // read the cluster's ("tap", "long") under the same id.
    c->setTapAction(TapAction::TAP_DEFAULT);
    c->setLongOpen(0xFF);
    TapAction ta;
    uint8_t lo;
    if (hasId && Settings::card(cs.id, "card_tap", v, sizeof(v)) && tapActionFromName(v, ta)) c->setTapAction(ta);
    if (hasId && Settings::card(cs.id, "card_long", v, sizeof(v)) && CardPopup::openViewFromName(v, lo)) c->setLongOpen(lo);
    return c;
}
