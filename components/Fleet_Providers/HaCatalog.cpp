#include "HaCatalog.h"
#include <esp_heap_caps.h>
#include <string.h>
#include <stdio.h>

bool HaCatalog::begin() {
    if (_areas && _items) return true;
    _areas = (Area *)heap_caps_calloc(AREAS_MAX, sizeof(Area), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    _items = (Item *)heap_caps_calloc(ITEMS_MAX, sizeof(Item), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!_areas) _areas = (Area *)calloc(AREAS_MAX, sizeof(Area));
    if (!_items) _items = (Item *)calloc(ITEMS_MAX, sizeof(Item));
    return _areas && _items;
}

// One line per thing, '|' between them, '>' between fields; the names have
// both replaced, so a field never holds one.
//   F>floor_id>Floor name|                    a floor, then its areas
//   A>area_id>Area name>floor_id|             an area ("" floor: on none)
//   E>entity_id>area_id>Name>device_class>unit>state|
// Floors and their areas in HA's own order, then the areas on no floor, then
// the entities. Measured 2026-11-02 on the owner's HA: 2 floors, 16 areas, 70
// entities, about 9 KB.
const char *HaCatalog::templateText() {
    return
        "{% for f in floors() %}F>{{ f }}>{{ floor_name(f) }}|"
        "{% for a in floor_areas(f) %}A>{{ a }}>{{ area_name(a) }}>{{ f }}|{% endfor %}{% endfor %}"
        "{% for a in areas() if not floor_id(a) %}A>{{ a }}>{{ area_name(a) }}>|{% endfor %}"
        "{% for e in label_entities('fleet') %}E>{{ e }}>{{ area_id(e) or '' }}>"
        "{{ (state_attr(e,'friendly_name') or e) | replace('|','/') | replace('>','/') }}>"
        "{{ state_attr(e,'device_class') or '' }}>{{ state_attr(e,'unit_of_measurement') or '' }}>"
        "{{ states(e) | replace('|','/') | replace('>','/') }}|{% endfor %}";
}

static void put(char *dst, size_t cap, const char *src) { snprintf(dst, cap, "%s", src ? src : ""); }

void HaCatalog::parse(const char *result) {
    if (!_areas || !_items || !result) return;
    const size_t len = strlen(result);
    char *buf = (char *)heap_caps_malloc(len + 1, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!buf) buf = (char *)malloc(len + 1);
    if (!buf) return;
    memcpy(buf, result, len + 1);

    std::lock_guard<std::mutex> lk(_mx);
    _nAreas = _nItems = 0;
    char floorName[32] = "", floorId[32] = "";
    char *save = nullptr;
    for (char *tok = strtok_r(buf, "|", &save); tok; tok = strtok_r(nullptr, "|", &save)) {
        char *f[7] = {};
        uint8_t n = 0;
        for (char *p = tok; n < 7; ) {
            f[n++] = p;
            char *gt = strchr(p, '>');
            if (!gt) break;
            *gt = '\0';
            p = gt + 1;
        }
        if (n < 2) continue;
        if (f[0][0] == 'F' && n >= 3) {
            put(floorId, sizeof(floorId), f[1]);
            put(floorName, sizeof(floorName), f[2]);
        } else if (f[0][0] == 'A' && n >= 3 && _nAreas < AREAS_MAX) {
            Area &a = _areas[_nAreas++];
            a = Area{};
            put(a.id, sizeof(a.id), f[1]);
            put(a.name, sizeof(a.name), f[2]);
            const char *fl = n >= 4 ? f[3] : "";
            put(a.floor, sizeof(a.floor), fl);
            if (fl[0] && strcmp(fl, floorId) == 0) put(a.floorName, sizeof(a.floorName), floorName);
        } else if (f[0][0] == 'E' && n >= 7 && _nItems < ITEMS_MAX) {
            if (strlen(f[1]) >= sizeof(Item::ref)) continue;   // an id too long to keep whole
            Item &it = _items[_nItems++];
            it = Item{};
            put(it.ref, sizeof(it.ref), f[1]);
            put(it.areaId, sizeof(it.areaId), f[2]);
            put(it.name, sizeof(it.name), f[3]);
            put(it.dc, sizeof(it.dc), f[4]);
            put(it.unit, sizeof(it.unit), f[5]);
            put(it.state, sizeof(it.state), f[6]);
        }
    }
    for (uint8_t i = 0; i < _nItems; i++) {
        const int a = findArea(_items[i].areaId);
        if (a >= 0 && _areas[a].items < 255) _areas[a].items++;
    }
    free(buf);
    _epoch.fetch_add(1);
    Serial.printf("[HaCatalog] %u areas, %u fleet entities\n", (unsigned)_nAreas, (unsigned)_nItems);
}

int HaCatalog::findArea(const char *id) const {
    if (!id || !id[0]) return -1;
    for (uint8_t i = 0; i < _nAreas; i++) if (strcmp(_areas[i].id, id) == 0) return i;
    return -1;
}

int HaCatalog::findItem(const char *ref) const {
    if (!ref || !ref[0]) return -1;
    for (uint8_t i = 0; i < _nItems; i++) if (strcmp(_items[i].ref, ref) == 0) return i;
    return -1;
}

uint8_t HaCatalog::noArea() const {
    uint8_t n = 0;
    for (uint8_t i = 0; i < _nItems; i++) if (findArea(_items[i].areaId) < 0) n++;
    return n;
}
