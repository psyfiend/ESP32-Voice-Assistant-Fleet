// "Add card" (2.13, K140; mock 2) - see include/UI/AddCardSheet.h.
#include "UI/AddCardSheet.h"
#include "UI/HaOffer.h"
#include "UI/HaPicker.h"
#include "UI/Kit.h"
#include "UI/Sheet.h"
#include "UI/UIToolkit.h"
#include "UI/UITokens.h"
#include "Cards/CardAdded.h"
#include "Cards/CardGroups.h"
#include "Cards/CardWhere.h"
#include "Entity.h"
#include "Settings.h"
#include <stdio.h>
#include <string.h>

namespace AddCardSheet {

static Kit::Sheet     S;
static Kit::SheetSpec sheet;
static char           s_title[48];
static lv_obj_t      *s_addLbl = nullptr;

static void closeCb(lv_event_t *ev) { (void)ev; Kit::sheetClose(S); }

static const char *held(const HaOffer::Row &r, void *) {
    static char words[56];
    return (r.have && CardWhere::words(*r.have, nullptr, HaOffer::registry(), words, sizeof(words))) ? words : nullptr;
}

static void changed(uint8_t n, void *) {
    if (!s_addLbl) return;
    char w[16];
    if (n) snprintf(w, sizeof(w), "Add %u", (unsigned)n);   // the mock's "Add 2"
    else   snprintf(w, sizeof(w), "Add");
    lv_label_set_text(s_addLbl, w);
}

// Each chosen entity a card on the page, in its HA area's group (K46: an HA
// area is a group, "ha_<area>"); one the compiled table does not have keeps its
// HA id on its entry (design/ha-cards.md 2.3).
static bool addChosen() {
    HaOffer::Row rows[24];
    const uint8_t n = HaPicker::chosen(rows, 24);
    uint8_t added = 0;
    for (uint8_t i = 0; i < n; i++) {
        const Entity *e = HaOffer::entityFor(rows[i]);
        if (!e) continue;
        char gid[48] = "";
        if (rows[i].areaId[0]) snprintf(gid, sizeof(gid), "%s%s", CardGroups::HA_PREFIX, rows[i].areaId);
        char id[CARD_ID_MAX + 1];
        if (!CardAdded::addToPage(*e, HaOffer::areaName(rows[i].areaId), gid, id, sizeof(id))) break;   // full
        if (e->learnt) {
            Settings::setCard(id, "ha", rows[i].ref);
            if (rows[i].dc[0])   Settings::setCard(id, "ha_dc", rows[i].dc);
            if (rows[i].unit[0]) Settings::setCard(id, "ha_unit", rows[i].unit);
        }
        added++;
    }
    if (!added) return false;
    Settings::save(400);
    HaPicker::forget();
    char msg[48];
    snprintf(msg, sizeof(msg), "Added %u", (unsigned)added);   // to confirm (the cluster's says "Added N to <name>")
    UIToolkit::show_toast(msg);
    Kit::sheetClose(S);
    return true;
}

static void addCb(lv_event_t *) { lv_async_call([](void *) { addChosen(); }, nullptr); }

static void build() {
    lv_label_set_text(S.lblTitleArea, s_title);
    lv_label_set_text(S.lblTitleName, "Add card");   // K145
    Kit::sheetLayoutTitle(S);
    s_addLbl = nullptr;
    const int32_t w = lv_area_get_width(&S.winRect) - 2 * S.pad;
    lv_obj_t *col = Kit::plain(S.stage);
    lv_obj_set_size(col, w, lv_pct(100));
    lv_obj_set_flex_flow(col, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(col, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(col, UI::mm(UI::shape().S5), 0);
    if (!HaOffer::ready()) {
        lv_obj_t *l = Kit::label(col, Kit::KitStyle::KS_LABEL_TAG_DIM);
        lv_label_set_text(l, "Waiting for Home Assistant");   // to confirm (not in K145)
        return;
    }
    HaPicker::Spec sp;
    sp.areaColumn = true;
    sp.held       = held;
    sp.changed    = changed;
    lv_obj_t *p = HaPicker::build(col, w, 1, sp);
    if (p) lv_obj_set_flex_grow(p, 1);
    lv_obj_t *row = Kit::buttonRow(col);
    const int32_t bw = Kit::textW("Add 00", UI::type().NAME) + 2 * UI::mm(UI::shape().S7);
    lv_obj_t *b = Kit::button(row, Kit::ButtonRole::BTN_CONFIRM, "Add", bw, Kit::controlH(), addCb);
    s_addLbl = Kit::labelIn(b);
    changed(HaPicker::chosenCount(), nullptr);
}

bool open(const char *pageTitle) {
    if (Kit::sheetShowing()) return false;
    HaPicker::forget();
    HaOffer::refresh();
    snprintf(s_title, sizeof(s_title), "%s > ", pageTitle ? pageTitle : "Page");
    sheet = Kit::SheetSpec{};
    sheet.leftCb     = closeCb;
    sheet.buildFirst = build;
    sheet.closed     = []() { s_addLbl = nullptr; };
    sheet.sparks     = true;
    Kit::sheetPlace(S);
    Kit::sheetOpen(S, sheet);
    return true;
}

void close()  { Kit::sheetClose(S); }
bool isOpen() { return S.win != nullptr; }

#ifdef DEBUG_POPUP
bool addDebug() { return isOpen() && addChosen(); }
#endif

}  // namespace AddCardSheet
