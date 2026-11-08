#include "Cards/CardAdded.h"
#include "Cards/Card.h"
#include "Cards/CardGroups.h"
#include "Cards/PageSpec.h"
#include "Entity.h"
#include "Settings.h"
#include <Arduino.h>
#include <esp_heap_caps.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

// See CardAdded.h.

namespace {

struct Rec {
    char    id[CARD_ID_MAX + 1];
    char    page[24];
    char    in[CARD_ID_MAX + 1];      // a member added to this cluster (2.11, K91), else ""
    char    from[CARD_ID_MAX + 1];    // ...the card it was copied or moved from (round 7), else ""
    char    after[CARD_ID_MAX + 1];
    char    entity[ENTITY_ID_MAX];
    char    area[ENTITY_SHORT_MAX];
    uint8_t pri;
    bool    removed;
    bool    cluster;                  // a cluster made here (2.13, K140): "kind": "cluster", no entity
    char    name[ENTITY_SHORT_MAX];   // ...its name
};

// MAX of each, allocated once by begin() - in PSRAM since 2.13, when MAX went
// from 24 to 64: ~40 KB with the specs, too much for an S3's internal RAM.
Rec      *s_rec  = nullptr;
CardSpec *s_spec = nullptr;        // each record as a spec - built once, never moved
uint8_t  s_n = 0;
uint16_t s_pooled = 0;             // removed ones, in the file only
char     s_page[24] = "";
const PageSpec *s_pageSpec = nullptr;   // the page being built, as compiled
bool     s_changed = false;
bool   (*s_compiled)(const char *) = nullptr;

void copy(char *dst, size_t cap, const char *src) { snprintf(dst, cap, "%s", src ? src : ""); }

// The record's spec: what a compiled card would say, from the record.
void specFor(uint8_t i) {
    const Rec &r = s_rec[i];
    CardSpec cs{};
    cs.id           = r.id;
    cs.primaries[0] = r.entity;
    cs.area         = r.area[0] ? r.area : nullptr;
    cs.label        = nullptr;                    // Inherit: HA's name (K65)
    cs.labelMode    = CardLabel::LBL_INHERIT;
    cs.place.priority = r.pri;
    // A CLUSTER MADE HERE (2.13): no entity of its own; its members are the
    // records "in" it (membersIn()), its words its name.
    if (r.cluster) {
        cs.primaries[0] = nullptr;
        cs.present      = GroupPresent::PRESENT_CLUSTER;
        cs.label        = r.name;
    }
    s_spec[i] = cs;
}

void readOne(const char *id, const char *page, void *) {
    if (s_n >= CardAdded::MAX || !cardIdWellFormed(id)) return;
    Rec &r = s_rec[s_n];
    r = Rec{};
    copy(r.id, sizeof(r.id), id);
    copy(r.page, sizeof(r.page), page);
    char v[48];
    Settings::card(id, "after", v, sizeof(v));  copy(r.after, sizeof(r.after), v);
    Settings::card(id, "entity", v, sizeof(v)); copy(r.entity, sizeof(r.entity), v);
    Settings::card(id, "area", v, sizeof(v));   copy(r.area, sizeof(r.area), v);
    if (Settings::card(id, "from", v, sizeof(v))) copy(r.from, sizeof(r.from), v);
    r.pri = Settings::card(id, "pri", v, sizeof(v)) ? (uint8_t)atoi(v) : 128;
    // REMOVED ONES STAY IN THE FILE ONLY (the page's pool, K66): a slot here is
    // for a card on a page. known() and idTaken() still find them in the file.
    // So do ones moved into a cluster (round 7): off the page, not in its pool.
    if (Settings::card(id, "removed", v, sizeof(v)) && (!strcmp(v, "yes") || !strcmp(v, "moved"))) { s_pooled++; return; }
    r.removed = false;
    if (Settings::card(id, "kind", v, sizeof(v)) && !strcmp(v, "cluster")) {
        r.cluster = true;
        if (!Settings::card(id, "name", r.name, sizeof(r.name))) copy(r.name, sizeof(r.name), r.area);
    }
    if (!r.entity[0] && !r.cluster) return;   // nothing to show: not taken
    specFor(s_n);
    s_n++;
}

// A MEMBER ADDED TO A CLUSTER ON THE DEVICE (2.11, K91, K100): a card entry with
// "in" (the cluster's id) where a page card has "page" - a copy with an id of
// its own (K92: a grouping is of copies), wearing its cluster's group.
void readMember(const char *id, const char *in, void *) {
    if (s_n >= CardAdded::MAX || !cardIdWellFormed(id) || !cardIdWellFormed(in)) return;
    Rec &r = s_rec[s_n];
    r = Rec{};
    copy(r.id, sizeof(r.id), id);
    copy(r.in, sizeof(r.in), in);
    char v[48];
    Settings::card(id, "entity", v, sizeof(v)); copy(r.entity, sizeof(r.entity), v);
    Settings::card(id, "area", v, sizeof(v));   copy(r.area, sizeof(r.area), v);
    if (Settings::card(id, "from", v, sizeof(v))) copy(r.from, sizeof(r.from), v);
    r.pri = 128;
    if (!r.entity[0]) return;
    specFor(s_n);
    s_n++;
}

bool idTaken(const char *id) {
    if (s_compiled && s_compiled(id)) return true;
    for (uint8_t i = 0; i < s_n; i++) if (!strcmp(s_rec[i].id, id)) return true;
    char v[8];
    return Settings::card(id, "page", v, sizeof(v));   // one this firmware could not take
}

// Lowercase letters and digits, '_' between words, at most `cap - 1`.
void slug(const char *in, char *out, size_t cap) {
    size_t k = 0;
    for (const char *p = in ? in : ""; *p && k < cap - 1; p++) {
        const char ch = (char)tolower((unsigned char)*p);
        if ((ch >= 'a' && ch <= 'z') || (ch >= '0' && ch <= '9')) out[k++] = ch;
        else if (k && out[k - 1] != '_') out[k++] = '_';
    }
    while (k && out[k - 1] == '_') k--;
    out[k] = '\0';
}

// K33: <area>_<label>_<yymmdd>_<hhmm>, the label dropped where it repeats the
// area (and a leading area word dropped from it: "Office lamp" in Office is
// "office_lamp", not "office_office_lamp"), a letter for a same-minute twin.
bool makeId(const char *area, const char *label, char *id, size_t cap) {
    char a[14], l[18], base[CARD_ID_MAX + 1];
    slug(area, a, sizeof(a));
    slug(label, l, sizeof(l));
    const size_t an = strlen(a);
    const char *lp = l;
    if (an && !strncmp(l, a, an) && (l[an] == '_' || l[an] == '\0')) lp = l[an] ? l + an + 1 : l + an;
    if (a[0] && lp[0]) snprintf(base, sizeof(base), "%s_%s", a, lp);
    else               snprintf(base, sizeof(base), "%s", a[0] ? a : lp[0] ? lp : "card");
    char stamp[16] = "000000_0000";
    time_t now = time(nullptr);
    struct tm tmv;
    if (now > 1700000000 && localtime_r(&now, &tmv)) strftime(stamp, sizeof(stamp), "%y%m%d_%H%M", &tmv);
    for (char suffix = 0; suffix <= 'z'; suffix = suffix ? (char)(suffix + 1) : 'b') {
        if (suffix) snprintf(id, cap, "%s_%s%c", base, stamp, suffix);
        else        snprintf(id, cap, "%s_%s", base, stamp);
        if (cardIdWellFormed(id) && !idTaken(id)) return true;
    }
    return false;
}

}  // namespace

namespace CardAdded {

void begin(bool (*compiledId)(const char *id)) {
    if (!s_rec) {
        s_rec  = (Rec *)heap_caps_calloc(MAX, sizeof(Rec), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
        s_spec = (CardSpec *)heap_caps_calloc(MAX, sizeof(CardSpec), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
        if (!s_rec)  s_rec  = (Rec *)calloc(MAX, sizeof(Rec));
        if (!s_spec) s_spec = (CardSpec *)calloc(MAX, sizeof(CardSpec));
        if (!s_rec || !s_spec) { Serial.println("[CardAdded] no memory for the added cards"); return; }
    }
    s_compiled = compiledId;
    s_n = 0;
    s_pooled = 0;
    Settings::forEachCardWith("page", readOne, nullptr);
    const uint8_t onPages = s_n;
    Settings::forEachCardWith("in", readMember, nullptr);
    Serial.printf("[CardAdded] %u card%s added on this device on its pages, %u removed (the pool), %u in clusters\n",
                  (unsigned)onPages, onPages == 1 ? "" : "s", (unsigned)s_pooled, (unsigned)(s_n - onPages));
}

void setPage(const char *slugName, const PageSpec *compiled) {
    copy(s_page, sizeof(s_page), slugName);
    s_pageSpec = compiled;
}

const PageSpec *pageSpec() { return s_pageSpec; }

const CardSpec *specOf(const char *cardId) {
    if (!cardId) return nullptr;
    const CardSpec *found = nullptr;
    if (s_pageSpec)
        forEachCardSpec(*s_pageSpec, [&](const CardSpec &cs) { if (!found && cs.id && !strcmp(cs.id, cardId)) found = &cs; });
    // SINCE K171 NEARLY EVERY CARD IS MADE HERE (2026-11-06): look among the made
    // ones too, or Restore and the names it says never find a former compiled group.
    for (uint8_t i = 0; !found && i < count(); i++)
        if (specAt(i).id && !strcmp(specAt(i).id, cardId)) found = &specAt(i);
    return found;
}

uint8_t merge(const PageSpec &page, CardSpec *out, uint8_t cap) {
    uint8_t n = 0;
    bool done[MAX] = {};
    const char *pg = page.slug ? page.slug : "";
    // Each card, then whatever was added after it - and after those, in turn.
    auto followers = [&](const char *id, auto &&self) -> void {
        if (!id) return;
        for (uint8_t i = 0; i < s_n && n < cap; i++) {
            const Rec &r = s_rec[i];
            if (done[i] || r.removed || strcmp(r.page, pg) || strcmp(r.after, id)) continue;
            done[i] = true;
            out[n++] = s_spec[i];
            self(r.id, self);
        }
    };
    for (uint8_t c = 0; c < page.count && n < cap; c++) {
        // K96: hidden, in the pool; round 7: moved into a cluster, in neither.
        if (!isHidden(page.cards[c].id) && !isMoved(page.cards[c].id)) out[n++] = page.cards[c];
        followers(page.cards[c].id, followers);
    }
    // Their card is gone: at the end.
    for (uint8_t i = 0; i < s_n && n < cap; i++) {
        if (done[i] || s_rec[i].removed || strcmp(s_rec[i].page, pg)) continue;
        done[i] = true;
        out[n++] = s_spec[i];
        followers(s_rec[i].id, followers);
    }
    return n;
}

uint8_t count() { return s_n; }
const CardSpec &specAt(uint8_t i) { return s_spec[i < s_n ? i : 0]; }
bool liveAt(uint8_t i) { return i < s_n && !s_rec[i].removed; }
bool nearLimit() { return (uint16_t)s_n * 4 >= (uint16_t)MAX * 3; }   // within 25% (K176)

bool known(const char *cardId) {
    if (!cardId) return false;
    for (uint8_t i = 0; i < s_n; i++) if (!strcmp(s_rec[i].id, cardId)) return true;
    char v[8];
    return Settings::card(cardId, "page", v, sizeof(v));   // in the pool
}

bool isAdded(const char *cardId) {
    if (!cardId) return false;
    for (uint8_t i = 0; i < s_n; i++) if (!s_rec[i].removed && !strcmp(s_rec[i].id, cardId)) return true;
    return false;
}

const char *addedFor(const char *parentId, const char *entityId) {
    if (!parentId || !entityId) return nullptr;
    for (uint8_t i = 0; i < s_n; i++) {
        const Rec &r = s_rec[i];
        if (!r.removed && !strcmp(r.after, parentId) && !strcmp(r.entity, entityId)) return r.id;
    }
    return nullptr;
}

bool add(const Card &parent, const Entity &member, char *idOut, size_t cap, const char *fromId) {
    if (!s_rec || s_n >= MAX || !parent.hasId() || !s_page[0]) return false;
    Rec &r = s_rec[s_n];
    r = Rec{};
    if (!makeId(parent.sheetArea(), member.desc.name, r.id, sizeof(r.id))) return false;
    copy(r.page, sizeof(r.page), s_page);
    copy(r.after, sizeof(r.after), parent.id());
    copy(r.entity, sizeof(r.entity), member.desc.id);
    copy(r.area, sizeof(r.area), parent.sheetArea());
    if (fromId && cardIdWellFormed(fromId)) copy(r.from, sizeof(r.from), fromId);
    r.pri = parent.placement().priority;
    r.removed = false;

    char pri[8];
    snprintf(pri, sizeof(pri), "%u", (unsigned)r.pri);
    Settings::setCard(r.id, "page", r.page);
    Settings::setCard(r.id, "after", r.after);
    Settings::setCard(r.id, "entity", r.entity);
    if (r.area[0]) Settings::setCard(r.id, "area", r.area);
    if (r.from[0]) Settings::setCard(r.id, "from", r.from);
    Settings::setCard(r.id, "pri", pri);
    // ITS PARENT'S GROUP (owner, K65): the card label's words, colour and type.
    CardGroups::Group g;
    if (CardGroups::ofCard(parent, g)) Settings::setCard(r.id, "group", g.id);

    specFor(s_n);
    s_n++;
    s_changed = true;
    if (idOut && cap) copy(idOut, cap, r.id);
    Serial.printf("[CardAdded] %s: %s after %s on %s\n", r.id, r.entity, r.after, r.page);
    return true;
}

bool addToPage(const Entity &e, const char *area, const char *groupId, char *idOut, size_t cap) {
    if (!s_rec || s_n >= MAX || !s_page[0]) return false;
    Rec &r = s_rec[s_n];
    r = Rec{};
    if (!makeId(area ? area : "", e.desc.name, r.id, sizeof(r.id))) return false;
    copy(r.page, sizeof(r.page), s_page);
    copy(r.entity, sizeof(r.entity), e.desc.id);
    copy(r.area, sizeof(r.area), area ? area : "");
    r.pri = CardPlacement{}.priority;   // the default: a page's own cards' (128)
    r.removed = false;
    char pri[8];
    snprintf(pri, sizeof(pri), "%u", (unsigned)r.pri);
    Settings::setCard(r.id, "page", r.page);   // no "after": it follows the page's own cards
    Settings::setCard(r.id, "entity", r.entity);
    if (r.area[0]) Settings::setCard(r.id, "area", r.area);
    Settings::setCard(r.id, "pri", pri);
    if (groupId && groupId[0]) Settings::setCard(r.id, "group", groupId);
    specFor(s_n);
    s_n++;
    s_changed = true;
    if (idOut && cap) copy(idOut, cap, r.id);
    Serial.printf("[CardAdded] %s: %s on %s, from HA\n", r.id, r.entity, r.page);
    return true;
}

bool addCluster(const char *name, const char *area, const char *groupId, char *idOut, size_t cap) {
    if (!s_rec || s_n >= MAX || !s_page[0] || !name || !name[0]) return false;
    Rec &r = s_rec[s_n];
    r = Rec{};
    if (!makeId(area ? area : "", name, r.id, sizeof(r.id))) return false;
    copy(r.page, sizeof(r.page), s_page);
    copy(r.area, sizeof(r.area), area ? area : "");
    copy(r.name, sizeof(r.name), name);
    r.cluster = true;
    r.pri = CardPlacement{}.priority;
    char pri[8];
    snprintf(pri, sizeof(pri), "%u", (unsigned)r.pri);
    Settings::setCard(r.id, "page", r.page);
    Settings::setCard(r.id, "kind", "cluster");
    Settings::setCard(r.id, "name", r.name);
    if (r.area[0]) Settings::setCard(r.id, "area", r.area);
    Settings::setCard(r.id, "pri", pri);
    if (groupId && groupId[0]) Settings::setCard(r.id, "group", groupId);
    specFor(s_n);
    s_n++;
    s_changed = true;
    if (idOut && cap) copy(idOut, cap, r.id);
    Serial.printf("[CardAdded] %s: a cluster, \"%s\", on %s\n", r.id, r.name, r.page);
    return true;
}

bool addTo(const Card &cluster, const Entity &e, char *idOut, size_t cap, const char *fromId) {
    if (!cluster.hasId()) return false;
    CardGroups::Group g;
    return addTo(cluster.id(), cluster.sheetArea(), CardGroups::ofCard(cluster, g) ? g.id : nullptr, e, idOut, cap,
                 fromId);
}

bool addTo(const char *clusterId, const char *clusterArea, const char *groupId, const Entity &e,
           char *idOut, size_t cap, const char *fromId) {
    if (!s_rec || s_n >= MAX || !clusterId || !cardIdWellFormed(clusterId)) return false;
    Rec &r = s_rec[s_n];
    r = Rec{};
    if (!makeId(clusterArea, e.desc.name, r.id, sizeof(r.id))) return false;
    copy(r.in, sizeof(r.in), clusterId);
    copy(r.entity, sizeof(r.entity), e.desc.id);
    copy(r.area, sizeof(r.area), clusterArea);
    if (fromId && cardIdWellFormed(fromId)) copy(r.from, sizeof(r.from), fromId);
    r.pri = 128;
    Settings::setCard(r.id, "in", r.in);
    Settings::setCard(r.id, "entity", r.entity);
    if (r.area[0]) Settings::setCard(r.id, "area", r.area);
    if (r.from[0]) Settings::setCard(r.id, "from", r.from);
    // A MEMBER WEARS ITS CLUSTER'S GROUP (K92 rule 2).
    if (groupId && groupId[0]) Settings::setCard(r.id, "group", groupId);
    specFor(s_n);
    s_n++;
    s_changed = true;
    if (idOut && cap) copy(idOut, cap, r.id);
    Serial.printf("[CardAdded] %s: %s into %s\n", r.id, r.entity, r.in);
    return true;
}

uint8_t membersIn(const char *clusterId, const CardSpec **out, uint8_t cap) {
    uint8_t n = 0;
    for (uint8_t i = 0; clusterId && i < s_n && n < cap; i++)
        if (!s_rec[i].removed && !strcmp(s_rec[i].in, clusterId)) out[n++] = &s_spec[i];
    return n;
}

bool isMember(const char *cardId) {
    for (uint8_t i = 0; cardId && i < s_n; i++)
        if (!s_rec[i].removed && s_rec[i].in[0] && !strcmp(s_rec[i].id, cardId)) return true;
    return false;
}

// A MEMBER MOVED OUT KNOWS WHICH CARD IT BECAME (the follow-ups): "moved_to",
// that card's id - a page card, or a copy in another cluster. Without it (a move
// saved before it existed) any card it came from is that card, as before.
// A page card moved into a cluster (round 2 of the follow-ups) knows it too.
static bool becameThis(const char *origId, const char *cardId) {
    if (!isMovedToPage(origId) && !isMoved(origId)) return false;
    char v[CARD_ID_MAX + 1];
    return !Settings::card(origId, "moved_to", v, sizeof(v)) || !strcmp(v, cardId);
}

// The move undone: a tile in its cluster again.
static void unmove(const char *memberId) {
    Settings::setCard(memberId, "hidden", nullptr);
    Settings::setCard(memberId, "moved_to", nullptr);
    Settings::setCard(memberId, "home_name", nullptr);
    Settings::setCard(memberId, "home_group", nullptr);
}

bool isMoveOf(const char *cardId) {
    const char *from = fromOf(cardId);
    return from && becameThis(from, cardId);
}

const char *becameOf(const char *origId) {
    for (uint8_t i = 0; origId && i < s_n; i++)
        if (!s_rec[i].removed && !strcmp(s_rec[i].from, origId) && becameThis(origId, s_rec[i].id)) return s_rec[i].id;
    return nullptr;
}

// Deleted, entry and all, wherever it is. A MOVED CARD GOES BACK (round 7): the
// card it was moved from is home again - on its page, or a tile in its cluster -
// the move undone, nothing lost. A copy's original is untouched.
bool unmake(const char *cardId) {
    for (uint8_t i = 0; cardId && i < s_n; i++) {
        Rec &r = s_rec[i];
        if (r.removed || strcmp(r.id, cardId)) continue;
        const bool move = r.from[0] && becameThis(r.from, r.id);
        r.removed = true;   // the record stays (a Card points at its id) until the next boot
        if (move && isMoved(r.from)) restore(r.from);
        else if (move)               unmove(r.from);
        Settings::dropCard(r.id);
        s_changed = true;
        Serial.printf("[CardAdded] %s deleted%s\n", r.id, move ? ", its original back home" : "");
        return true;
    }
    return false;
}

const char *clusterOf(const char *cardId) {
    for (uint8_t i = 0; cardId && i < s_n; i++)
        if (!s_rec[i].removed && s_rec[i].in[0] && !strcmp(s_rec[i].id, cardId)) return s_rec[i].in;
    return nullptr;
}

// A GROUP MADE ON THE BOARD IS DELETED WHEN NO CARD IS IN IT: no entry names it
// in "in". A member moved out to the page keeps its "in" (its Restore goes back
// there), so that group stays, hidden by the caller. Its label group goes with
// it unless another card wears it. A compiled group is never deleted. Called once
// a change is complete, never between a removal and an add in the same save.
bool emptied(const char *clusterId) {
    if (!clusterId || !cardIdWellFormed(clusterId)) return false;
    if (s_compiled && s_compiled(clusterId)) return false;
    char v[48];
    if (!Settings::card(clusterId, "kind", v, sizeof(v)) || strcmp(v, "cluster")) return true;   // already gone
    if (Settings::countCards("in", clusterId)) return false;
    char gid[48] = "";
    Settings::card(clusterId, "group", gid, sizeof(gid));
    for (uint8_t i = 0; i < s_n; i++)
        if (!strcmp(s_rec[i].id, clusterId)) s_rec[i].removed = true;   // the record stays until the next boot
    Settings::dropCard(clusterId);
    if (gid[0] && !Settings::countCards("group", gid)) {
        Settings::setGroup(gid, "name", nullptr);
        Settings::setGroup(gid, "color", nullptr);
        Settings::setGroup(gid, "kind", nullptr);
        Settings::setGroup(gid, "ha_area", nullptr);
    }
    s_changed = true;
    Serial.printf("[CardAdded] %s deleted: no card is in it%s\n", clusterId, gid[0] ? " (and its label group)" : "");
    return true;
}

void keepHome(const char *memberId, const char *name, const char *groupId) {
    if (!memberId) return;
    Settings::setCard(memberId, "home_name", name && name[0] ? name : nullptr);
    Settings::setCard(memberId, "home_group", groupId && groupId[0] ? groupId : nullptr);
}

bool home(const char *memberId, char *name, size_t nameCap, char *groupId, size_t gidCap) {
    if (!memberId) return false;
    const bool n = Settings::card(memberId, "home_name", name, nameCap);
    if (!n) name[0] = '\0';
    if (!Settings::card(memberId, "home_group", groupId, gidCap)) groupId[0] = '\0';
    return n;
}

bool removeMember(const char *cardId) {
    for (uint8_t i = 0; cardId && i < s_n; i++)
        if (!s_rec[i].removed && s_rec[i].in[0] && !strcmp(s_rec[i].id, cardId)) return unmake(cardId);
    return false;
}

const char *fromOf(const char *cardId) {
    for (uint8_t i = 0; cardId && i < s_n; i++)
        if (!s_rec[i].removed && !strcmp(s_rec[i].id, cardId)) return s_rec[i].from[0] ? s_rec[i].from : nullptr;
    return nullptr;
}

bool isMovedToPage(const char *memberId) {
    char v[8];
    return memberId && Settings::card(memberId, "hidden", v, sizeof(v)) && !strcmp(v, "moved");
}

bool moveToPage(const char *memberId, const char *toId) {
    if (!memberId || !cardIdWellFormed(memberId) || isMovedToPage(memberId)) return false;
    Settings::setCard(memberId, "hidden", "moved");
    if (toId && toId[0]) Settings::setCard(memberId, "moved_to", toId);
    s_changed = true;
    Serial.printf("[CardAdded] %s moved out of its cluster to %s (no tile, not in the hidden cards)\n", memberId,
                  toId && toId[0] ? toId : "the page");
    return true;
}

bool movedHere(const char *pageCardId) {
    const char *from = isAdded(pageCardId) ? fromOf(pageCardId) : nullptr;
    return from && becameThis(from, pageCardId);
}

// The page card goes - deleted, as a copy removed from a cluster is - and the
// member it was is a tile again: the move undone, nothing lost.
bool moveBack(const char *pageCardId) { return movedHere(pageCardId) && unmake(pageCardId); }

bool drop(const char *pageCardId) {
    if (!pageCardId || movedHere(pageCardId)) return false;
    for (uint8_t i = 0; i < s_n; i++) {
        Rec &r = s_rec[i];
        if (r.removed || r.in[0] || strcmp(r.id, pageCardId)) continue;   // a page card, not a cluster's copy
        r.removed = true;   // the record stays (a Card points at its id) until the next boot
        Settings::dropCard(pageCardId);
        s_changed = true;
        Serial.printf("[CardAdded] %s removed from the page (its entry deleted)\n", pageCardId);
        return true;
    }
    return false;
}

bool isMoved(const char *cardId) {
    char v[8];
    return cardId && Settings::card(cardId, "removed", v, sizeof(v)) && !strcmp(v, "moved");
}

bool moveOut(const char *cardId, const char *toId) {
    if (!cardId || !cardIdWellFormed(cardId) || isHidden(cardId) || isMoved(cardId)) return false;
    for (uint8_t i = 0; i < s_n; i++)
        if (!s_rec[i].removed && !s_rec[i].in[0] && !strcmp(s_rec[i].id, cardId)) s_rec[i].removed = true;
    Settings::setCard(cardId, "removed", "moved");
    if (toId && toId[0]) Settings::setCard(cardId, "moved_to", toId);
    s_changed = true;
    Serial.printf("[CardAdded] %s moved into a cluster (off its page, not in its pool)\n", cardId);
    return true;
}

bool rehome(const char *memberId, const Card &cluster) {
    if (!cluster.hasId()) return false;
    CardGroups::Group g;
    return rehome(memberId, cluster.id(), cluster.sheetArea(), CardGroups::ofCard(cluster, g) ? g.id : nullptr);
}

// A card made here RELOCATES (round 2 of the follow-ups): out of a cluster or off
// the page, into another cluster - its record changes, its id and settings stay.
bool rehome(const char *cardId, const char *clusterId, const char *clusterArea, const char *groupId) {
    if (!cardId || !clusterId || !cardIdWellFormed(clusterId)) return false;
    for (uint8_t i = 0; i < s_n; i++) {
        Rec &r = s_rec[i];
        if (r.removed || strcmp(r.id, cardId)) continue;
        if (!r.in[0]) {   // a page card: off the page
            r.page[0] = r.after[0] = '\0';
            Settings::setCard(r.id, "page", nullptr);
            Settings::setCard(r.id, "after", nullptr);
            Settings::setCard(r.id, "pri", nullptr);
        }
        copy(r.in, sizeof(r.in), clusterId);
        copy(r.area, sizeof(r.area), clusterArea);
        r.pri = 128;
        specFor(i);
        Settings::setCard(r.id, "in", r.in);
        Settings::setCard(r.id, "area", r.area[0] ? r.area : nullptr);
        // A MEMBER WEARS ITS CLUSTER'S GROUP (K92 rule 2).
        Settings::setCard(r.id, "group", groupId && groupId[0] ? groupId : nullptr);
        s_changed = true;
        Serial.printf("[CardAdded] %s moved into %s\n", r.id, r.in);
        return true;
    }
    return false;
}

bool toPage(const char *cardId, const char *afterId, uint8_t pri) {
    if (!cardId || !afterId || !s_page[0]) return false;
    for (uint8_t i = 0; i < s_n; i++) {
        Rec &r = s_rec[i];
        if (r.removed || !r.in[0] || strcmp(r.id, cardId)) continue;
        r.in[0] = '\0';
        copy(r.page, sizeof(r.page), s_page);
        copy(r.after, sizeof(r.after), afterId);
        r.pri = pri;
        specFor(i);
        char p[8];
        snprintf(p, sizeof(p), "%u", (unsigned)pri);
        Settings::setCard(r.id, "in", nullptr);
        Settings::setCard(r.id, "page", r.page);
        Settings::setCard(r.id, "after", r.after);
        Settings::setCard(r.id, "pri", p);
        s_changed = true;
        Serial.printf("[CardAdded] %s out of its cluster onto %s, after %s\n", r.id, r.page, r.after);
        return true;
    }
    return false;
}

const char *movedTo(const char *memberId) {
    if (!memberId || !isMovedToPage(memberId)) return nullptr;
    for (uint8_t i = 0; i < s_n; i++)
        if (!s_rec[i].removed && !strcmp(s_rec[i].from, memberId) && becameThis(memberId, s_rec[i].id))
            return s_rec[i].in;
    return "";
}

bool inCluster(const char *clusterId, const char *entityId) {
    for (uint8_t i = 0; clusterId && entityId && i < s_n; i++)
        if (!s_rec[i].removed && !strcmp(s_rec[i].in, clusterId) && !strcmp(s_rec[i].entity, entityId)) return true;
    return false;
}

bool remove(const char *cardId) {
    for (uint8_t i = 0; i < s_n; i++) {
        Rec &r = s_rec[i];
        if (r.removed || strcmp(r.id, cardId)) continue;
        r.removed = true;
        Settings::setCard(r.id, "removed", "yes");
        s_changed = true;
        Serial.printf("[CardAdded] %s removed (kept in the page's pool)\n", r.id);
        return true;
    }
    return false;
}

bool isHidden(const char *cardId) {
    char v[8];
    return cardId && Settings::card(cardId, "removed", v, sizeof(v)) && !strcmp(v, "yes");
}

bool hide(const char *cardId) {
    if (!cardId || !cardIdWellFormed(cardId) || isHidden(cardId)) return false;
    if (isAdded(cardId)) return remove(cardId);
    Settings::setCard(cardId, "removed", "yes");
    s_changed = true;
    Serial.printf("[CardAdded] %s hidden (kept in the page's pool)\n", cardId);
    return true;
}

bool restore(const char *cardId) {
    if (!isHidden(cardId) && !isMoved(cardId)) return false;
    Settings::setCard(cardId, "removed", nullptr);
    Settings::setCard(cardId, "moved_to", nullptr);
    s_changed = true;
    // An added card comes back to its record: the one removed this boot, else
    // read from the file as begin() would (a Card points at its id: never moved).
    char page[24];
    if (Settings::card(cardId, "page", page, sizeof(page))) {
        bool found = false;
        for (uint8_t i = 0; i < s_n && !found; i++)
            if (!strcmp(s_rec[i].id, cardId)) { s_rec[i].removed = false; found = true; }
        if (!found) readOne(cardId, page, nullptr);
        if (s_pooled) s_pooled--;
    }
    Serial.printf("[CardAdded] %s restored to its page\n", cardId);
    return true;
}

namespace {
struct PoolCtx { const char *slug; CardAdded::PoolItem *out; uint8_t cap, n; };

// An added card hidden from this page (a compiled one has no "page" key: its
// page's spec lists it, below).
void poolAdded(const char *id, const char *value, void *p) {
    PoolCtx &x = *(PoolCtx *)p;
    char page[24];
    if (x.n >= x.cap || strcmp(value, "yes") || !Settings::card(id, "page", page, sizeof(page)) ||
        strcmp(page, x.slug)) return;
    CardAdded::PoolItem &it = x.out[x.n++];
    copy(it.id, sizeof(it.id), id);
    char v[ENTITY_ID_MAX];
    Settings::card(id, "entity", v, sizeof(v)); copy(it.entity, sizeof(it.entity), v);
    Settings::card(id, "area", v, sizeof(v));   copy(it.area, sizeof(it.area), v);
    it.label = nullptr;
}
}  // namespace

uint8_t pool(const PageSpec &page, PoolItem *out, uint8_t cap) {
    PoolCtx x = { page.slug ? page.slug : "", out, cap, 0 };
    for (uint8_t c = 0; c < page.count && x.n < cap; c++) {
        const CardSpec &cs = page.cards[c];
        if (!isHidden(cs.id)) continue;
        PoolItem &it = out[x.n++];
        copy(it.id, sizeof(it.id), cs.id);
        const char *e = cs.primaries[0];
        if (!e && cs.memberCount && cs.members) e = cs.members[0].primaries[0];
        copy(it.entity, sizeof(it.entity), e);
        copy(it.area, sizeof(it.area), cs.area);
        it.label = cs.label;
    }
    Settings::forEachCardWith("removed", poolAdded, &x);
    return x.n;
}

bool takeChanged() {
    const bool c = s_changed;
    s_changed = false;
    return c;
}

namespace {
// The ids carrying a key, collected first: an entry is never changed while the
// file is being walked.
constexpr uint8_t RESET_IDS = 64;
struct Ids { char id[RESET_IDS][CARD_ID_MAX + 1]; uint8_t n; };
void collect(const char *id, const char *, void *p) {
    Ids &x = *(Ids *)p;
    if (x.n < RESET_IDS) copy(x.id[x.n++], sizeof(x.id[0]), id);
}
}  // namespace

ResetCount resetToCompiled(bool apply) {
    ResetCount c = {};
    Ids *ids = new Ids();   // ~2.6 KB, for the moment it takes
    if (!ids) return c;
    // Cards made here: on a page ("page") or in a cluster ("in"), deleted whole.
    static const char *const MADE[] = { "page", "in" };
    for (const char *key : MADE) {
        ids->n = 0;
        Settings::forEachCardWith(key, collect, ids);
        for (uint8_t i = 0; i < ids->n; i++) {
            c.made++;
            if (apply) Settings::dropCard(ids->id[i]);
        }
    }
    // Then what was done to the compiled ones: hidden or moved off a page, a
    // member hidden or moved out, a group card's slots and presentation.
    struct Undo { const char *key; uint8_t ResetCount::*count; };
    static const Undo UNDO[] = {
        { "removed", &ResetCount::hidden }, { "hidden", &ResetCount::members },
        { "slots", &ResetCount::clusters }, { "present", &ResetCount::clusters },
        { "moved_to", nullptr },   // a moved member's note of its card: goes with its "hidden"
        { "home_name", nullptr }, { "home_group", nullptr },   // ...and of its cluster as it was
    };
    for (const Undo &u : UNDO) {
        ids->n = 0;
        Settings::forEachCardWith(u.key, collect, ids);
        for (uint8_t i = 0; i < ids->n; i++) {
            char v[4];
            const char *id = ids->id[i];
            if (Settings::card(id, "page", v, sizeof(v)) || Settings::card(id, "in", v, sizeof(v))) continue;   // made here: counted above
            if (u.count) c.*u.count += 1;
            if (apply) Settings::setCard(id, u.key, nullptr);
        }
    }
    delete ids;
    if (apply) {
        begin(s_compiled);   // the records again: none made here now
        s_changed = true;
        Serial.printf("[CardAdded] reset to the compiled dashboard: %u made here deleted, %u hidden or moved back, "
                      "%u members, %u cluster settings\n", (unsigned)c.made, (unsigned)c.hidden,
                      (unsigned)c.members, (unsigned)c.clusters);
    }
    return c;
}

}  // namespace CardAdded
