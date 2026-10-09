// CardGroups - see CardGroups.h.
#include "Cards/CardGroups.h"
#include "Cards/Card.h"
#include "Cards/CardIcons.h"   // cardAreaColor(), the card-label palette
#include "Cards/PageSpec.h"
#include "EntityRegistry.h"
#include "Settings.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

namespace {

const PageSpec *const *s_pages = nullptr;
uint8_t s_nPages = 0;
const EntityRegistry *s_reg = nullptr;

using CardGroups::Group;

void slugInto(const char *prefix, const char *name, char *out, size_t cap) {
    if (!cap) return;
    size_t k = (size_t)snprintf(out, cap, "%s", prefix);
    for (const char *p = name ? name : ""; *p && k + 1 < cap; p++) {
        const char ch = (char)tolower((unsigned char)*p);
        if ((ch >= 'a' && ch <= 'z') || (ch >= '0' && ch <= '9')) out[k++] = ch;
        else if (k && out[k - 1] != '_') out[k++] = '_';
    }
    out[k < cap ? k : cap - 1] = '\0';
}

bool startsWith(const char *s, const char *p) { return !strncmp(s, p, strlen(p)); }

// The dashboard's areas, each once, in page order.
uint8_t sheetAreas(const char **out, uint8_t cap) {
    uint8_t n = 0;
    for (uint8_t p = 0; p < s_nPages; p++)
        for (uint8_t i = 0; i < s_pages[p]->count; i++) {
            const char *a = s_pages[p]->cards[i].area;
            if (!a || !a[0]) continue;
            bool seen = false;
            for (uint8_t k = 0; k < n && !seen; k++) seen = !strcmp(out[k], a);
            if (!seen && n < cap) out[n++] = a;
        }
    return n;
}

// IS THIS DASHBOARD AREA AN HA AREA (K46)? When every card in it whose entity
// HA has placed in an area agrees on one area, it is that area under the
// dashboard's (shorter) name. Cards HA has not placed, and entities that are
// not HA's (the Fleet page's virtual lamps), do not count either way. "" when
// they disagree or nothing is known yet.
const char *linkOf(const char *area) {
    static char id[40];
    id[0] = '\0';
    if (!s_reg || !area || !area[0]) return id;
    for (uint8_t p = 0; p < s_nPages; p++)
        for (uint8_t i = 0; i < s_pages[p]->count; i++) {
            const CardSpec &cs = s_pages[p]->cards[i];
            if (!cs.area || strcmp(cs.area, area) != 0 || !cs.primaries[0]) continue;
            const Entity *e = s_reg->find(cs.primaries[0]);
            if (!e || e->desc.source != EntitySource::HA || !e->sourceAreaId[0]) continue;
            if (!id[0]) snprintf(id, sizeof(id), "%s", e->sourceAreaId);
            else if (strcmp(id, e->sourceAreaId) != 0) { id[0] = '\0'; return id; }
        }
    return id;
}

// HA's own name for an area id, from any entity in it.
bool haNameOf(const char *areaId, char *out, size_t cap) {
    out[0] = '\0';
    if (!s_reg) return false;
    for (uint8_t i = 0; i < s_reg->count(); i++) {
        const Entity *e = s_reg->at(i);
        if (e && !strcmp(e->sourceAreaId, areaId) && e->sourceArea[0]) {
            snprintf(out, cap, "%s", e->sourceArea);
            return true;
        }
    }
    return false;
}

void sheetGroupId(const char *area, char *out, size_t cap) {
    const char *link = linkOf(area);
    if (link[0]) snprintf(out, cap, "%s%s", CardGroups::HA_PREFIX, link);
    else         slugInto(CardGroups::SHEET_PREFIX, area, out, cap);
}

// Every known group, each once: the dashboard's areas (as linked HA groups
// where they are one), every HA area an entity here is in, and the groups in
// the settings file. LVGL thread only (one static list).
Group   s_list[32];
uint8_t s_listN = 0;

void addListed(const char *gid) {
    for (uint8_t i = 0; i < s_listN; i++) if (!strcmp(s_list[i].id, gid)) return;
    if (s_listN < 32 && CardGroups::byId(gid, s_list[s_listN])) s_listN++;
}

void collect() {
    s_listN = 0;
    const char *areas[32];
    const uint8_t na = sheetAreas(areas, 32);
    char gid[48];
    for (uint8_t i = 0; i < na; i++) { sheetGroupId(areas[i], gid, sizeof(gid)); addListed(gid); }
    if (s_reg)
        for (uint8_t i = 0; i < s_reg->count(); i++) {
            const Entity *e = s_reg->at(i);
            if (!e || !e->sourceAreaId[0]) continue;
            snprintf(gid, sizeof(gid), "%s%s", CardGroups::HA_PREFIX, e->sourceAreaId);
            addListed(gid);
        }
    Settings::forEachGroup([](const char *id, const char *, void *) { addListed(id); }, nullptr);
}

// A colour no listed group is using; when all are taken, the least used.
uint32_t freeColour() {
    collect();
    const uint8_t n = cardAreaHueCount();
    uint8_t best = 0, bestUse = 255;
    for (uint8_t h = 0; h < n; h++) {
        uint8_t use = 0;
        for (uint8_t i = 0; i < s_listN; i++) if (s_list[i].color == cardAreaHue(h)) use++;
        if (use < bestUse) { best = h; bestUse = use; }
    }
    return cardAreaHue(best);
}

// Leaving a solo group that no card is in any more: it goes (K46).
void dropIfEmptySolo(const char *gid) {
    if (!gid || !gid[0]) return;
    char kind[8];
    if (!Settings::group(gid, "kind", kind, sizeof(kind)) || strcmp(kind, "solo") != 0) return;
    if (Settings::countCards("group", gid)) return;
    Settings::setGroup(gid, "name", nullptr);
    Settings::setGroup(gid, "color", nullptr);
    Settings::setGroup(gid, "kind", nullptr);
}

}  // namespace

namespace CardGroups {

void setSources(const PageSpec *const *pages, uint8_t n, const EntityRegistry *reg) {
    s_pages  = pages;
    s_nPages = n;
    s_reg    = reg;
}

bool byId(const char *gid, Group &g) {
    if (!gid || !gid[0]) return false;
    memset(&g, 0, sizeof(g));
    snprintf(g.id, sizeof(g.id), "%s", gid);
    const bool ha = startsWith(gid, HA_PREFIX);
    if (ha) haNameOf(gid + strlen(HA_PREFIX), g.haName, sizeof(g.haName));

    // ITS OWN NAME: the dashboard's for a dashboard area (linked or not), else
    // HA's for an HA area; a group made here has none but its id.
    char base[40] = "";
    const char *areas[32];
    const uint8_t na = sheetAreas(areas, 32);
    char id[48];
    for (uint8_t i = 0; i < na && !base[0]; i++) {
        sheetGroupId(areas[i], id, sizeof(id));
        if (!strcmp(id, gid)) snprintf(base, sizeof(base), "%s", areas[i]);
    }
    if (!base[0] && ha) snprintf(base, sizeof(base), "%s", g.haName);

    // The name shown: a saved one (a rename, or a group made here) first.
    if (!Settings::group(gid, "name", g.name, sizeof(g.name))) {
        if (!base[0]) return false;   // a group nobody knows
        snprintf(g.name, sizeof(g.name), "%s", base);
    }
    // THE COLOUR NEVER FOLLOWS A RENAME (K46: colour is the group). A saved
    // one, else the colour of its own name - or of its id, for a group made
    // here. Found on WS_P4_5: renaming Office to "Work" turned it lime.
    char hex[12];
    g.color = (Settings::group(gid, "color", hex, sizeof(hex)) && strlen(hex) == 6)
            ? (uint32_t)strtoul(hex, nullptr, 16) : cardAreaColor(base[0] ? base : gid);
    char kind[8];
    g.solo = Settings::group(gid, "kind", kind, sizeof(kind)) && !strcmp(kind, "solo");
    return true;
}

bool ofCard(const Card &c, Group &g) {
    char v[48];
    if (c.hasId() && Settings::card(c.id(), "group", v, sizeof(v)) && v[0]) {
        if (!strcmp(v, "none")) return false;
        if (byId(v, g)) return true;
        // A group that has gone: the dashboard's area, until the owner chooses.
    }
    if (!c.sheetArea()[0]) return false;
    char gid[48];
    sheetGroupId(c.sheetArea(), gid, sizeof(gid));
    return byId(gid, g);
}

void apply(Card &c, bool areaColor) {
    c.setPageAreaColor(areaColor);
    c.setLabelAlways(false);
    Group g;
    if (!ofCard(c, g)) { c.setArea(""); c.setAreaColor(0); return; }
    // Its own words keep the group's colour (K46, rename a); empty words are a
    // card label in the group's colour alone.
    char words[40];
    if (c.hasId() && Settings::card(c.id(), "label_text", words, sizeof(words))) {
        c.setArea(words);
        c.setLabelAlways(true);
    } else {
        c.setArea(g.name);
    }
    c.setAreaColor(areaColor ? g.color : 0);
}

uint8_t rowChoices(const Card &c, Choice *out, uint8_t cap, char *opts, size_t optsCap, uint16_t &sel) {
    Group cur;
    const bool has = ofCard(c, cur);
    collect();
    uint8_t n = 0;
    size_t used = 0;
    opts[0] = '\0';
    sel = 0;
    for (uint8_t i = 0; i < s_listN && n + 2 < cap; i++) {
        const Group &g = s_list[i];
        // Two groups of one name: say which is HA's.
        bool twin = false;
        for (uint8_t k = 0; k < s_listN && !twin; k++) twin = k != i && !strcmp(s_list[k].name, g.name);
        used += snprintf(opts + used, optsCap > used ? optsCap - used : 0, "%s%s%s", n ? "\n" : "", g.name,
                         twin && g.haName[0] ? " (HA)" : "");
        out[n].kind = CH_GROUP;
        snprintf(out[n].gid, sizeof(out[n].gid), "%s", g.id);
        if (has && !strcmp(g.id, cur.id)) sel = n;
        n++;
    }
    used += snprintf(opts + used, optsCap > used ? optsCap - used : 0, "%sOwn group", n ? "\n" : "");
    out[n].kind = CH_OWN; out[n].gid[0] = '\0'; n++;
    snprintf(opts + used, optsCap > used ? optsCap - used : 0, "\nNone");
    out[n].kind = CH_NONE; out[n].gid[0] = '\0';
    if (!has) sel = n;
    n++;
    return n;
}

void choose(Card &c, const Choice &ch, bool areaColor) {
    if (!c.hasId()) return;
    char old[48] = "";
    Settings::card(c.id(), "group", old, sizeof(old));
    switch (ch.kind) {
        case CH_GROUP: Settings::setCard(c.id(), "group", ch.gid); break;
        case CH_NONE:  Settings::setCard(c.id(), "group", "none"); break;
        case CH_OWN: {
            // A group of one (K46, rename b): the words it has now, a colour
            // no other group is using. Another card may join it later.
            char id[48];
            const char *words = c.area()[0] ? c.area() : "Own";
            if (!Settings::newGroup(words, id, sizeof(id))) return;
            char hex[8];
            snprintf(hex, sizeof(hex), "%06lX", (unsigned long)(freeColour() & 0xFFFFFF));
            Settings::setGroup(id, "kind", "solo");
            Settings::setGroup(id, "color", hex);
            Settings::setCard(c.id(), "group", id);
            Settings::setCard(c.id(), "label_text", nullptr);   // the group's name is its words
            break;
        }
    }
    dropIfEmptySolo(old);
    apply(c, areaColor);
}

void forEach(void (*fn)(const Group &g, void *ctx), void *ctx) {
    if (!fn) return;
    collect();
    for (uint8_t i = 0; i < s_listN; i++) fn(s_list[i], ctx);
}

}  // namespace CardGroups
