#include "UI/HaOffer.h"
#include "UI/UIToolkit.h"
#include "Cards/CardGroups.h"
#include "HaCatalog.h"
#include "HaProvider.h"
#include "HaValue.h"
#include "EntityRegistry.h"
#include "Settings.h"
#include <Arduino.h>
#include <esp_heap_caps.h>
#include <string.h>
#include <stdio.h>

namespace {

HaCatalog      *s_cat  = nullptr;
HaProvider     *s_prov = nullptr;
EntityRegistry *s_reg  = nullptr;
uint32_t        s_seenEpoch = 0;

// ",ref,ref,...," - the leading and trailing commas make a search for ",ref,"
// exact. KNOWN is the settings file's list; NOTIFIED this boot's notices.
constexpr size_t LIST = 6144;
char *s_known    = nullptr;
char *s_notified = nullptr;
bool  s_knownRead = false;   // the file had a list (else the first catalogue fills it)

char *listBuf() {
    char *p = (char *)heap_caps_calloc(1, LIST, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    return p ? p : (char *)calloc(1, LIST);
}

bool inList(const char *list, const char *ref) {
    if (!list || !ref || !ref[0]) return false;
    char key[80];
    snprintf(key, sizeof(key), ",%s,", ref);
    return strstr(list, key) != nullptr;
}

bool addToList(char *list, const char *ref) {
    if (!list || inList(list, ref)) return false;
    size_t n = strlen(list);
    if (!n) { list[0] = ','; list[1] = '\0'; n = 1; }
    if (n + strlen(ref) + 2 >= LIST) return false;   // full: it simply stays new
    strcat(list, ref);
    strcat(list, ",");
    return true;
}

void saveKnown() { Settings::setSystem("ha", "known", s_known); Settings::save(2000); }

// The HA areas the board's groups are linked to (K141): an area id, or a floor
// id standing for its areas. Into `out` as ",id,id,".
void linkedAreas(char *out, size_t cap) {
    snprintf(out, cap, ",");
    struct Ctx { char *out; size_t cap; } ctx{out, cap};
    CardGroups::forEach([](const CardGroups::Group &g, void *p) {
        Ctx &c = *(Ctx *)p;
        char a[40];
        if (!HaOffer::groupArea(g.id, a, sizeof(a)) || !a[0]) return;
        const size_t n = strlen(c.out);
        if (n + strlen(a) + 2 < c.cap) { strcat(c.out, a); strcat(c.out, ","); }
    }, &ctx);
}

// An item's area is linked: its area, or its area's floor, is in `linked`.
bool isLinked(const char *linked, const HaCatalog::Item &it) {
    if (!it.areaId[0]) return false;
    if (inList(linked, it.areaId)) return true;
    const int a = s_cat->findArea(it.areaId);
    return a >= 0 && s_cat->area(a).floor[0] && inList(linked, s_cat->area(a).floor);
}

// A new catalogue: the first one ever knows everything; after that, a new
// entity in a linked area gets its notice once this boot.
void onCatalogue() {
    if (!s_known || !s_notified) return;
    char linked[512];
    linkedAreas(linked, sizeof(linked));

    struct Note { char text[96]; };
    Note notes[4];
    uint8_t nNotes = 0, more = 0;
    bool changed = false;
    s_cat->lock();
    for (uint8_t i = 0; i < s_cat->itemCount(); i++) {
        const HaCatalog::Item &it = s_cat->item(i);
        if (!s_knownRead) { changed |= addToList(s_known, it.ref); continue; }
        if (inList(s_known, it.ref) || inList(s_notified, it.ref) || !isLinked(linked, it)) continue;
        addToList(s_notified, it.ref);
        if (nNotes < 4) {
            const int a = s_cat->findArea(it.areaId);
            // K145's words: "New in Office: 3D Printer".
            snprintf(notes[nNotes++].text, sizeof(Note::text), "New in %s: %s",
                     a >= 0 ? s_cat->area(a).name : it.areaId, it.name);
        } else {
            more++;
        }
    }
    s_cat->unlock();
    if (!s_knownRead) {
        s_knownRead = true;
        if (changed) saveKnown();
        Serial.printf("[HaOffer] first catalogue: %u entities known\n", (unsigned)s_cat->itemCount());
        return;
    }
    for (uint8_t k = 0; k < nNotes; k++) UIToolkit::show_notice(notes[k].text);
    if (more) Serial.printf("[HaOffer] %u more new entities without a notice\n", (unsigned)more);
}

}  // namespace

namespace HaOffer {

void begin(HaCatalog *cat, HaProvider *prov, EntityRegistry *reg) {
    s_cat = cat;
    s_prov = prov;
    s_reg = reg;
    if (!s_known) s_known = listBuf();
    if (!s_notified) s_notified = listBuf();
    if (s_known) {
        s_knownRead = Settings::system("ha", "known", s_known, LIST) && s_known[0];
        if (!s_knownRead) s_known[0] = '\0';
    }
}

void tick() {
    if (!s_cat || !s_cat->ready()) return;
    const uint32_t e = s_cat->epoch();
    if (e == s_seenEpoch) return;
    s_seenEpoch = e;
    onCatalogue();
}

void refresh() { if (s_prov) s_prov->requestCatalog(); }
bool ready() { return s_cat && s_cat->ready(); }
uint32_t epoch() { return s_cat ? s_cat->epoch() : 0; }

uint8_t areas(AreaRow *out, uint8_t cap) {
    if (!s_cat || !out) return 0;
    uint8_t n = 0;
    s_cat->lock();
    for (uint8_t i = 0; i < s_cat->areaCount() && n < cap; i++) {
        const HaCatalog::Area &a = s_cat->area(i);
        if (!a.items) continue;
        AreaRow &r = out[n++];
        snprintf(r.id, sizeof(r.id), "%s", a.id);
        snprintf(r.name, sizeof(r.name), "%s", a.name);
        snprintf(r.floorName, sizeof(r.floorName), "%s", a.floorName);
        r.items = a.items;
        r.fresh = 0;
        for (uint8_t k = 0; k < s_cat->itemCount(); k++)
            if (strcmp(s_cat->item(k).areaId, a.id) == 0 && !inList(s_known, s_cat->item(k).ref)) r.fresh++;
    }
    const uint8_t none = s_cat->noArea();
    if (none && n < cap) {
        AreaRow &r = out[n++];
        r = AreaRow{};
        r.items = none;
        for (uint8_t k = 0; k < s_cat->itemCount(); k++)
            if (s_cat->findArea(s_cat->item(k).areaId) < 0 && !inList(s_known, s_cat->item(k).ref)) r.fresh++;
    }
    s_cat->unlock();
    return n;
}

const char *areaName(const char *areaId) {
    static char name[40];
    name[0] = '\0';
    if (!s_cat || !areaId || !areaId[0]) return name;
    s_cat->lock();
    const int a = s_cat->findArea(areaId);
    if (a >= 0) snprintf(name, sizeof(name), "%s", s_cat->area(a).name);
    s_cat->unlock();
    return name;
}

uint8_t rows(const char *areaId, Row *out, uint8_t cap) {
    if (!s_cat || !out) return 0;
    const bool none = !areaId || !areaId[0];
    uint8_t n = 0;
    s_cat->lock();
    for (uint8_t i = 0; i < s_cat->itemCount() && n < cap; i++) {
        const HaCatalog::Item &it = s_cat->item(i);
        const bool here = none ? s_cat->findArea(it.areaId) < 0 : strcmp(it.areaId, areaId) == 0;
        if (!here) continue;
        Row &r = out[n++];
        snprintf(r.ref, sizeof(r.ref), "%s", it.ref);
        snprintf(r.name, sizeof(r.name), "%s", it.name);
        snprintf(r.areaId, sizeof(r.areaId), "%s", it.areaId);
        snprintf(r.dc, sizeof(r.dc), "%s", it.dc);
        snprintf(r.unit, sizeof(r.unit), "%s", it.unit);
        snprintf(r.state, sizeof(r.state), "%s", it.state);
        r.isNew = s_knownRead && !inList(s_known, it.ref);
        r.have = nullptr;
    }
    s_cat->unlock();
    for (uint8_t k = 0; k < n && s_reg; k++) out[k].have = s_reg->findRef(EntitySource::HA, out[k].ref);
    return n;
}

uint8_t newIn(const char *areaId) {
    if (!s_cat || !s_knownRead) return 0;
    uint8_t n = 0;
    s_cat->lock();
    for (uint8_t i = 0; i < s_cat->itemCount(); i++) {
        const HaCatalog::Item &it = s_cat->item(i);
        if (strcmp(it.areaId, areaId ? areaId : "") == 0 && !inList(s_known, it.ref)) n++;
    }
    s_cat->unlock();
    return n;
}

void seen(const char *areaId) {
    if (!s_cat || !s_known || !s_knownRead) return;
    const bool none = !areaId || !areaId[0];
    bool changed = false;
    s_cat->lock();
    for (uint8_t i = 0; i < s_cat->itemCount(); i++) {
        const HaCatalog::Item &it = s_cat->item(i);
        const bool here = none ? s_cat->findArea(it.areaId) < 0 : strcmp(it.areaId, areaId) == 0;
        if (here) changed |= addToList(s_known, it.ref);
    }
    s_cat->unlock();
    if (changed) saveKnown();
}

const Entity *entityFor(const Row &r, bool *made) {
    if (made) *made = false;
    if (!s_reg) return nullptr;
    if (const Entity *e = s_reg->findRef(EntitySource::HA, r.ref)) return e;
    EntityDescriptor d;
    if (!haDescriptor(r.ref, r.dc, r.unit, d)) return nullptr;
    snprintf(d.name, sizeof(d.name), "%s", r.name);
    const Entity *e = s_reg->adopt(d);
    if (e && made) *made = true;
    return e;
}

bool groupArea(const char *gid, char *out, size_t cap) {
    out[0] = '\0';
    if (!gid || !gid[0]) return false;
    char v[40] = "";
    if (Settings::group(gid, "ha_area", v, sizeof(v)) && v[0]) {
        if (!strcmp(v, "none")) return false;
        snprintf(out, cap, "%s", v);
        return true;
    }
    const size_t pre = strlen(CardGroups::HA_PREFIX);
    if (!strncmp(gid, CardGroups::HA_PREFIX, pre) && gid[pre]) {
        snprintf(out, cap, "%s", gid + pre);
        return true;
    }
    // By name: a dashboard area called what an HA area or floor is called.
    CardGroups::Group g;
    if (!s_cat || !CardGroups::byId(gid, g) || !g.name[0]) return false;
    s_cat->lock();
    for (uint8_t i = 0; i < s_cat->areaCount() && !out[0]; i++) {
        const HaCatalog::Area &a = s_cat->area(i);
        if (!strcasecmp(a.name, g.name)) snprintf(out, cap, "%s", a.id);
        else if (a.floor[0] && !strcasecmp(a.floorName, g.name)) snprintf(out, cap, "%s", a.floor);
    }
    s_cat->unlock();
    return out[0] != '\0';
}

bool setGroupArea(const char *gid, const char *place) {
    if (!gid || !gid[0]) return false;
    const bool ok = Settings::setGroup(gid, "ha_area", place && place[0] ? place : "none");
    Settings::save(400);
    return ok;
}

const char *placeName(const char *place) {
    static char name[40];
    name[0] = '\0';
    if (!s_cat || !place || !place[0]) return name;
    s_cat->lock();
    for (uint8_t i = 0; i < s_cat->areaCount() && !name[0]; i++) {
        const HaCatalog::Area &a = s_cat->area(i);
        if (!strcmp(a.id, place)) snprintf(name, sizeof(name), "%s", a.name);
        else if (!strcmp(a.floor, place)) snprintf(name, sizeof(name), "%s", a.floorName);
    }
    s_cat->unlock();
    return name;
}

bool inPlace(const char *areaId, const char *place) {
    if (!areaId || !areaId[0] || !place || !place[0]) return false;
    if (!strcmp(areaId, place)) return true;
    if (!s_cat) return false;
    s_cat->lock();
    const int a = s_cat->findArea(areaId);
    const bool on = a >= 0 && !strcmp(s_cat->area(a).floor, place);
    s_cat->unlock();
    return on;
}

void dump(void (*out)(const char *line, void *ctx), void *ctx) {
    char line[200];
    if (!s_cat) { out("no catalogue\n", ctx); return; }
    char linked[512];
    linkedAreas(linked, sizeof(linked));
    snprintf(line, sizeof(line), "catalogue epoch %lu, %s; linked areas %s\n", (unsigned long)epoch(),
             s_knownRead ? "known list read" : "no known list yet", linked);
    out(line, ctx);
    AreaRow *a = (AreaRow *)heap_caps_malloc(sizeof(AreaRow) * HaCatalog::AREAS_MAX, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    Row *rws = (Row *)heap_caps_malloc(sizeof(Row) * 40, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!a || !rws) { free(a); free(rws); return; }
    const uint8_t na = areas(a, HaCatalog::AREAS_MAX);
    for (uint8_t i = 0; i < na; i++) {
        snprintf(line, sizeof(line), "AREA %-14s %-18s floor %-10s %u items, %u new\n", a[i].id[0] ? a[i].id : "-",
                 a[i].id[0] ? a[i].name : "No area", a[i].floorName, (unsigned)a[i].items, (unsigned)a[i].fresh);
        out(line, ctx);
        const uint8_t nr = rows(a[i].id, rws, 40);
        for (uint8_t k = 0; k < nr; k++) {
            snprintf(line, sizeof(line), "   %-44s %-30s %-10s %-8s %-6s %s%s\n", rws[k].ref, rws[k].name, rws[k].state,
                     rws[k].dc, rws[k].unit, rws[k].have ? rws[k].have->desc.id : "-", rws[k].isNew ? " NEW" : "");
            out(line, ctx);
        }
    }
    free(rws);
    free(a);
}

}  // namespace HaOffer
