// "New cluster" (2.13, K140; mock 3) - see include/UI/NewClusterSheet.h.
#include "UI/NewClusterSheet.h"
#include "UI/HaOffer.h"
#include "UI/HaPicker.h"
#include "UI/Kit.h"
#include "UI/Sheet.h"
#include "UI/UIToolkit.h"
#include "UI/UITokens.h"
#include "Cards/CardAdded.h"
#include "Cards/CardGroups.h"
#include "HaCatalog.h"
#include "Entity.h"
#include "Settings.h"
#include <stdio.h>
#include <string.h>

namespace NewClusterSheet {

static Kit::Sheet       S;
static Kit::SheetSpec   sheet;
static char             s_title[48];
static char             s_area[32] = "";      // "" until one is chosen
static lv_obj_t        *s_goLbl = nullptr;
static HaOffer::AreaRow s_areas[HaCatalog::AREAS_MAX];
static uint8_t          s_nAreas = 0;
static char             s_words[HaCatalog::AREAS_MAX][48];

static void closeCb(lv_event_t *ev) { (void)ev; Kit::sheetClose(S); }

// A header's kind (K115's order: temperature, occupancy, light level), or -1.
static int headerKind(const HaOffer::Row &r) {
    const bool sensor = !strncmp(r.ref, "sensor.", 7), binary = !strncmp(r.ref, "binary_sensor.", 14);
    if (sensor && !strcmp(r.dc, "temperature")) return 0;
    if (binary && (!strcmp(r.dc, "occupancy") || !strcmp(r.dc, "motion") || !strcmp(r.dc, "presence"))) return 1;
    if (sensor && !strcmp(r.dc, "illuminance")) return 2;
    return -1;
}

// Ticked to start with: lights and switches (K92's tap set), and the first
// sensor of each header kind.
static bool s_kindTaken[3];
static bool preselect(const HaOffer::Row &r) {
    if (!strncmp(r.ref, "light.", 6) || !strncmp(r.ref, "switch.", 7)) return true;
    const int k = headerKind(r);
    if (k < 0 || s_kindTaken[k]) return false;
    s_kindTaken[k] = true;
    return true;
}

static void changed(uint8_t n, void *) {
    if (s_goLbl) lv_obj_set_state(lv_obj_get_parent(s_goLbl), LV_STATE_DISABLED, n == 0);
}

static void fill();

static void areaCb(lv_event_t *ev) {
    const uint8_t i = (uint8_t)(uintptr_t)lv_event_get_user_data(ev);
    if (i >= s_nAreas) return;
    snprintf(s_area, sizeof(s_area), "%s", s_areas[i].id);
    lv_async_call([](void *) { if (S.win) fill(); }, nullptr);
}

// The cluster, then each ticked entity a member of it; the header's sensors
// (four at most, K115's order) its "slots".
static bool create() {
    if (!s_area[0]) return false;
    HaOffer::Row rows[24];
    const uint8_t n = HaPicker::chosen(rows, 24);
    if (!n) return false;
    const char *areaName = HaOffer::areaName(s_area);
    char gid[48], cid[CARD_ID_MAX + 1];
    snprintf(gid, sizeof(gid), "%s%s", CardGroups::HA_PREFIX, s_area);
    if (!CardAdded::addCluster(areaName, areaName, gid, cid, sizeof(cid))) return false;
    char slotIds[CLUSTER_SLOTS_MAX][CARD_ID_MAX + 1] = {};
    int  slotKind[CLUSTER_SLOTS_MAX];
    uint8_t nSlots = 0;
    for (uint8_t i = 0; i < n; i++) {
        const Entity *e = HaOffer::entityFor(rows[i]);
        if (!e) continue;
        char id[CARD_ID_MAX + 1];
        if (!CardAdded::addTo(cid, areaName, gid, *e, id, sizeof(id), nullptr)) break;   // full
        if (e->learnt) {
            Settings::setCard(id, "ha", rows[i].ref);
            if (rows[i].dc[0])   Settings::setCard(id, "ha_dc", rows[i].dc);
            if (rows[i].unit[0]) Settings::setCard(id, "ha_unit", rows[i].unit);
        }
        const int k = headerKind(rows[i]);
        if (k >= 0 && nSlots < CLUSTER_SLOTS_MAX) {
            snprintf(slotIds[nSlots], sizeof(slotIds[0]), "%s", id);
            slotKind[nSlots++] = k;
        }
    }
    // K115: temperature, occupancy, light level - the order they are listed in.
    char list[CLUSTER_SLOTS_MAX * (CARD_ID_MAX + 2)] = "";
    for (int k = 0; k < 3; k++)
        for (uint8_t s = 0; s < nSlots; s++)
            if (slotKind[s] == k) {
                if (list[0]) strncat(list, ",", sizeof(list) - strlen(list) - 1);
                strncat(list, slotIds[s], sizeof(list) - strlen(list) - 1);
            }
    if (list[0]) Settings::setCard(cid, "slots", list);
    Settings::save(400);
    HaPicker::forget();
    Kit::sheetClose(S);
    return true;
}

static void createCb(lv_event_t *) { lv_async_call([](void *) { create(); }, nullptr); }

static void fill() {
    lv_obj_clean(S.stage);
    s_goLbl = nullptr;
    const int32_t w = lv_area_get_width(&S.winRect) - 2 * S.pad;
    lv_obj_t *col = Kit::plain(S.stage);
    lv_obj_set_size(col, w, lv_pct(100));
    lv_obj_set_flex_flow(col, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(col, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(col, UI::mm(UI::shape().S5), 0);
    lv_label_set_text(S.lblTitleArea, s_area[0] ? s_title : "");
    lv_label_set_text(S.lblTitleName, s_area[0] ? HaOffer::areaName(s_area) : "New cluster");   // K145
    Kit::sheetLayoutTitle(S);
    if (!HaOffer::ready()) {
        lv_label_set_text(Kit::label(col, Kit::KitStyle::KS_LABEL_TAG_DIM), "Waiting for Home Assistant");   // to confirm
        return;
    }
    if (!s_area[0]) {
        // THE AREA FIRST (K91: "a group to base it on"): every area with entities.
        Kit::drawerTitle(col, "Area");   // K145
        s_nAreas = HaOffer::areas(s_areas, HaCatalog::AREAS_MAX);
        const char *words[HaCatalog::AREAS_MAX];
        uint8_t nw = 0;
        for (uint8_t i = 0; i < s_nAreas; i++) {
            if (!s_areas[i].id[0]) continue;   // No area makes no cluster
            snprintf(s_words[i], sizeof(s_words[0]), "%s (%u)", s_areas[i].name, (unsigned)s_areas[i].items);
            words[nw++] = s_words[i];
        }
        // A chip's index is its area's: No area, the one skipped, is always last.
        Kit::chipGrid(col, w, words, nw, nw, areaCb);
        return;
    }
    for (bool &t : s_kindTaken) t = false;
    HaPicker::forget();
    HaPicker::Spec sp;
    sp.area      = s_area;
    sp.changed   = changed;
    sp.preselect = preselect;
    lv_obj_t *p = HaPicker::build(col, w, 1, sp);
    if (p) lv_obj_set_flex_grow(p, 1);
    lv_obj_t *row = Kit::buttonRow(col);
    const int32_t bw = Kit::textW("Create", UI::type().NAME) + 2 * UI::mm(UI::shape().S7);
    lv_obj_t *b = Kit::button(row, Kit::ButtonRole::BTN_CONFIRM, "Create", bw, Kit::controlH(), createCb);   // K145
    s_goLbl = Kit::labelIn(b);
    changed(HaPicker::chosenCount(), nullptr);
}

bool open(const char *pageTitle) {
    if (Kit::sheetShowing()) return false;
    HaPicker::forget();
    HaOffer::refresh();
    s_area[0] = '\0';
    snprintf(s_title, sizeof(s_title), "New cluster > ");   // K145
    (void)pageTitle;
    sheet = Kit::SheetSpec{};
    sheet.leftCb     = closeCb;
    sheet.buildFirst = fill;
    sheet.closed     = []() { s_goLbl = nullptr; };
    sheet.sparks     = true;
    Kit::sheetPlace(S);
    Kit::sheetOpen(S, sheet);
    return true;
}

void close()  { Kit::sheetClose(S); }
bool isOpen() { return S.win != nullptr; }

#ifdef DEBUG_POPUP
bool areaDebug(const char *areaId) {
    if (!isOpen()) return false;
    snprintf(s_area, sizeof(s_area), "%s", areaId ? areaId : "");
    fill();
    return true;
}
bool createDebug() { return isOpen() && create(); }
#endif

}  // namespace NewClusterSheet
