// "New group" (2.13 round 5, the owner's design, DECISIONS K168; replaces round 1's
// area chips and picker, K140) - see include/UI/NewClusterSheet.h.
#include "UI/NewClusterSheet.h"
#include "UI/EditCards.h"
#include "UI/HaOffer.h"
#include "UI/Kit.h"
#include "UI/Sheet.h"
#include "UI/UIToolkit.h"
#include "UI/UITokens.h"
#include "Cards/CardAdded.h"
#include "Cards/CardGroups.h"
#include "Cards/CardIcons.h"   // cardHeaderGlyphFor(): a sensor's third tap (K168)
#include "HaCatalog.h"
#include "Entity.h"
#include "Settings.h"
#include "PsramArray.h"
#include <stdio.h>
#include <string.h>
#include <strings.h>

namespace NewClusterSheet {

using KS   = Kit::KitStyle;
using Act  = EditCards::Act;
using Item = EditCards::Item;

enum class Step : uint8_t { STEP_AREA, STEP_CARDS, STEP_FACE, STEP_TAP, STEP_NAME };

static Kit::Sheet       S;
static Kit::SheetSpec   sheet;
static Step             s_step = Step::STEP_AREA;
static char             s_area[32] = "";      // the HA area it is modelled after; "" Create new
static char             s_list[56] = "";      // its list's words: "New group", "Office (new)"
static char             s_name[40] = "";      // the name it will have
static bool             s_nameTaken = false;  // a group already has the area's name
static uint8_t          s_namePick = 0;       // 0 the name, 1 Change name
static lv_obj_t        *s_nameGrid = nullptr;
static lv_obj_t        *s_dlg = nullptr;
static PsramArray<HaOffer::AreaRow, HaCatalog::AREAS_MAX> s_areas;   // ~6.5 KB
static uint8_t          s_nAreas = 0;
static uint8_t          s_areaAt[HaCatalog::AREAS_MAX];   // a chip's area
static const char      *s_words[HaCatalog::AREAS_MAX + 1];
static PsramArray<Item, 32> s_items;   // ~10 KB
static uint8_t          s_nItems = 0;
// SHOWN AS A SOLO CARD (K191): fewer than two cards pinned - its face (0 all, else
// s_items[face - 1]) and its tap, asked before its name.
static bool             s_solo = false;
static uint8_t          s_face = 0;
static uint8_t          s_tap = 0;
static lv_obj_t        *s_grid = nullptr;

static const char *const NEW_GROUP = "New group";   // K168

static void showArea();
static void showCards(bool keep);
static void showName();

static void dlgClose() {
    if (s_dlg) lv_obj_delete(s_dlg);
    s_dlg = nullptr;
}

// --- Is the name taken? -----------------------------------------------------------------
static bool s_found = false;
static const char *s_want = nullptr;
static bool nameInUse(const char *name) {
    s_found = false;
    s_want = name;
    CardGroups::forEach([](const CardGroups::Group &g, void *) {
        if (s_want && !strcasecmp(g.name, s_want)) s_found = true;
    }, nullptr);
    return s_found;
}

// --- 1. THE AREA (K168): "Model this group after an existing HA area?" ------------------
static void areaCb(lv_event_t *ev) {
    const uint8_t i = (uint8_t)(uintptr_t)lv_event_get_user_data(ev);
    lv_async_call([](void *p) {
        if (!S.win) return;
        const uint8_t k = (uint8_t)(uintptr_t)p;
        uint8_t nShown = 0;
        for (uint8_t a = 0; a < s_nAreas; a++) if (s_areas[a].id[0]) nShown++;
        if (k >= nShown) {   // Create new
            s_area[0] = '\0';
            snprintf(s_list, sizeof(s_list), "%s", NEW_GROUP);
        } else {
            const HaOffer::AreaRow &a = s_areas[s_areaAt[k]];
            snprintf(s_area, sizeof(s_area), "%s", a.id);
            snprintf(s_list, sizeof(s_list), "%s (new)", a.name);   // K168
        }
        EditCards::forget();
        showCards(false);
    }, (void *)(uintptr_t)i);
}

static void fillArea() {
    lv_obj_clean(S.stage);
    lv_label_set_text(S.lblTitleArea, "");
    lv_label_set_text(S.lblTitleName, NEW_GROUP);
    lv_label_set_text(S.lblLeft, LV_SYMBOL_CLOSE);
    Kit::sheetLayoutTitle(S);
    lv_obj_update_layout(S.win);
    const int32_t w = lv_obj_get_content_width(S.stage);
    lv_obj_t *col = Kit::plain(S.stage);
    lv_obj_set_size(col, w, lv_pct(100));
    lv_obj_set_flex_flow(col, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(col, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(col, UI::mm(UI::shape().S6), 0);
    UI::tameScroll(col);
    if (!HaOffer::ready()) {
        lv_label_set_text(Kit::label(col, KS::KS_LABEL_TAG_DIM), "Waiting for Home Assistant");   // to confirm
        return;
    }
    lv_label_set_text(Kit::label(col, KS::KS_LABEL_NAME), "Model this group after an existing HA area?");   // K168
    s_nAreas = HaOffer::areas(s_areas, HaCatalog::AREAS_MAX);
    uint8_t nw = 0;
    for (uint8_t i = 0; i < s_nAreas; i++) {
        if (!s_areas[i].id[0]) continue;   // No area is nothing to model a group after
        s_areaAt[nw] = i;
        s_words[nw++] = s_areas[i].name;
    }
    s_words[nw++] = "Create new";   // K168: the last button
    Kit::chipGrid(col, w, s_words, nw, nw, areaCb);   // nothing lit
}

static void showArea() {
    s_step = Step::STEP_AREA;
    fillArea();
}

// --- 2. EDIT CARDS' FLOW (K168): the new group's list first, then HA's areas ------------
static uint8_t fillNone(Item *, uint8_t, void *) { return 0; }   // nothing in it yet

static const char *haGlyph(const HaOffer::Row &r, uint32_t &tint) {
    if (r.have) return cardHeaderGlyph(*r.have, tint);
    return cardHeaderGlyphFor(r.ref, r.dc, r.unit, tint);
}

static void askSolo();   // below: fewer than two pinned (K191)

static void act(Act a, void *) {
    if (a != Act::ACT_SAVE) return;
    s_nItems = EditCards::chosen(s_items, 32);
    if (!s_nItems) return;
    uint8_t pinned = 0;
    for (uint8_t i = 0; i < s_nItems; i++) pinned += s_items[i].step == 2 ? 1 : 0;
    s_solo = pinned < 2;
    lv_async_call([](void *) { if (S.win) { if (s_solo) askSolo(); else showName(); } }, nullptr);
}

static void showCards(bool keep) {
    s_step = Step::STEP_CARDS;
    lv_obj_clean(S.stage);
    lv_label_set_text(S.lblTitleArea, "");
    lv_label_set_text(S.lblTitleName, NEW_GROUP);
    lv_label_set_text(S.lblLeft, LV_SYMBOL_LEFT);
    Kit::sheetLayoutTitle(S);
    lv_obj_update_layout(S.win);
    EditCards::Spec sp;
    sp.lists[0]  = { s_list, fillNone, true };
    sp.nLists    = 1;
    sp.area      = s_area[0] ? s_area : nullptr;
    sp.cycle     = true;              // K161's taps: pin, a sensor's header (K168), unpinned (K190)
    sp.pinFirst  = true;              // K190: the first tap adds a card pinned
    sp.haGlyph   = haGlyph;
    sp.saveWord  = "Create group";    // K168
    sp.act       = act;
    sp.keepTicks = keep;
    EditCards::build(S.stage, lv_obj_get_content_width(S.stage), lv_obj_get_content_height(S.stage), sp);
}

// --- 3. ITS NAME (K168) ----------------------------------------------------------------
static void keepName(const char *words, void *) {
    if (words && words[0]) { snprintf(s_name, sizeof(s_name), "%s", words); s_namePick = 0; }   // the new name, lit
    lv_async_call([](void *) { if (S.win && s_step == Step::STEP_NAME) showName(); }, nullptr);
}

static void nameGridCb(lv_event_t *ev) {
    s_namePick = (uint8_t)(uintptr_t)lv_event_get_user_data(ev);
    if (s_nameGrid) Kit::chipGridLit(s_nameGrid, s_namePick);
    if (s_namePick != 1) return;
    static char now[40];
    snprintf(now, sizeof(now), "%s", s_name);
    Kit::KeyboardSpec k = {};
    k.title  = "New group name";   // K168, as Add to dashboard's "New label name"
    k.text   = now;
    k.maxLen = sizeof(s_name) - 1;
    k.save   = keepName;
    Kit::keyboard(k);
}

static bool create();

static void askCreate() {
    dlgClose();
    if (s_namePick == 1 && !s_name[0]) return;
    static char l1[200], l2[420], names[400];
    char acc[64];
    snprintf(acc, sizeof(acc), "#%06lX %s#", (unsigned long)(UI::pal().ACCENT & 0xFFFFFF), s_name);
    snprintf(l1, sizeof(l1), "A new group will be created named %s. You can rename this group from the 'Visibility' menu.", acc);
    EditCards::namesOf(names, sizeof(names));
    snprintf(l2, sizeof(l2), "The cards that will be added to the group are: %s", names);
    Kit::DialogSpec d = {};
    d.role    = Kit::DialogRole::DLG_CONFIRM;
    d.title   = "Create new group?";
    d.body[0] = l1;
    d.body[1] = l2;
    d.body[2] = "From the GROUP menu you can add or remove cards, and from the 'Members view' you can change "
                "which cards are pinned.";
    d.buttons[0] = { "Cancel", Kit::ButtonRole::BTN_CANCEL, [](lv_event_t *) {
        lv_async_call([](void *) { dlgClose(); }, nullptr);
    } };
    d.buttons[1] = { "Create", Kit::ButtonRole::BTN_CONFIRM, [](lv_event_t *) {
        lv_async_call([](void *) { dlgClose(); if (S.win) create(); }, nullptr);
    } };
    d.nButtons  = 2;
    d.width     = LV_MIN(lv_area_get_width(&S.winRect) * 3 / 4, UI::mm(UI::shape().W_DIALOG_MAX));
    d.outside   = [](lv_event_t *) { lv_async_call([](void *) { dlgClose(); }, nullptr); };
    d.countdown = true;
    s_dlg = Kit::dialog(d);
}

static void nextCb(lv_event_t *) { lv_async_call([](void *) { if (S.win) askCreate(); }, nullptr); }

static void showName() {
    s_step = Step::STEP_NAME;
    // The area's name, unless a group has it already (two groups may model one
    // area, but not by one name); Create new: "New group" (K168).
    const char *areaName = s_area[0] ? HaOffer::areaName(s_area) : "";
    s_nameTaken = s_area[0] && nameInUse(areaName);
    if (!s_name[0]) snprintf(s_name, sizeof(s_name), "%s", (s_area[0] && !s_nameTaken) ? areaName : NEW_GROUP);
    lv_obj_clean(S.stage);
    lv_label_set_text(S.lblTitleArea, "");
    lv_label_set_text(S.lblTitleName, NEW_GROUP);
    lv_label_set_text(S.lblLeft, LV_SYMBOL_LEFT);
    Kit::sheetLayoutTitle(S);
    lv_obj_update_layout(S.win);
    const int32_t w = lv_obj_get_content_width(S.stage);
    lv_obj_t *col = Kit::plain(S.stage);
    lv_obj_set_size(col, w, lv_pct(100));
    lv_obj_set_flex_flow(col, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(col, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(col, UI::mm(UI::shape().S6), 0);
    static char line[120];
    snprintf(line, sizeof(line), "The group will be named '%s'. Do you want to enter a different name?", s_name);   // K168
    lv_obj_t *l = Kit::label(col, KS::KS_LABEL_NAME);
    lv_obj_set_width(l, w);
    lv_obj_set_style_text_align(l, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_long_mode(l, LV_LABEL_LONG_WRAP);
    lv_label_set_text(l, line);
    static const char *words[2];
    words[0] = s_name;
    words[1] = "Change name";   // K168
    s_nameGrid = Kit::chipGrid(col, w, words, 2, s_namePick, nameGridCb);
    lv_obj_t *row = Kit::buttonRow(col);
    const int32_t bw = Kit::textW("Next", UI::type().NAME) + 2 * UI::mm(UI::shape().S7);
    Kit::button(row, Kit::ButtonRole::BTN_CONFIRM, "Next", bw, Kit::controlH(), nextCb);
}

// --- SHOWN AS A SOLO CARD (owner, 2026-11-06, K191) ---------------------------------------
// Fewer than two cards pinned: a dialog says so, then the face and the tap are asked
// as Show as... asks them (Cards/Popup/PopupShowAs.cpp), then the name.
static const Entity *itemEntity(const Item &it) {
    HaOffer::Row r;
    return EditCards::haRowOf(it.key, r) ? HaOffer::entityFor(r) : nullptr;
}
static bool itemSwitches(const Item &it) {
    const Entity *e = itemEntity(it);
    return e && e->desc.writable && (e->desc.kind == EntityKind::LIGHT || e->desc.kind == EntityKind::SWITCH);
}
static bool itemsMixed() {
    // A sensor put in the header (its third tap) is the header's, not a card: it does not count.
    for (uint8_t i = 0; i < s_nItems; i++) if (s_items[i].step != 3 && !itemSwitches(s_items[i])) return true;
    return false;
}

static const char *s_faceW[33];
static const char *s_tapW[4];
static TapAction   s_tapMap[4];
static uint8_t     s_nTap = 0;

static void gridCb(lv_event_t *ev) {
    const uint8_t i = (uint8_t)(uintptr_t)lv_event_get_user_data(ev);
    if (s_step == Step::STEP_FACE) s_face = i;
    else                           s_tap = i;
    if (s_grid) Kit::chipGridLit(s_grid, i);
}

static void stepNextCb(lv_event_t *);

// A step of the sheet: its words, a grid of chips, and Next - as the name step.
static void stepView(const char *prompt, const char *const *words, uint8_t n, uint8_t lit) {
    lv_obj_clean(S.stage);
    lv_label_set_text(S.lblTitleArea, "");
    lv_label_set_text(S.lblTitleName, NEW_GROUP);
    lv_label_set_text(S.lblLeft, LV_SYMBOL_LEFT);
    Kit::sheetLayoutTitle(S);
    lv_obj_update_layout(S.win);
    const int32_t w = lv_obj_get_content_width(S.stage);
    lv_obj_t *col = Kit::plain(S.stage);
    lv_obj_set_size(col, w, lv_pct(100));
    lv_obj_set_flex_flow(col, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(col, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(col, UI::mm(UI::shape().S6), 0);
    lv_obj_t *l = Kit::label(col, KS::KS_LABEL_NAME);
    lv_obj_set_width(l, w);
    lv_obj_set_style_text_align(l, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_long_mode(l, LV_LABEL_LONG_WRAP);
    lv_label_set_text(l, prompt);
    s_grid = Kit::chipGrid(col, w, words, n, lit, gridCb);
    lv_obj_t *row = Kit::buttonRow(col);
    const int32_t bw = Kit::textW("Next", UI::type().NAME) + 2 * UI::mm(UI::shape().S7);
    Kit::button(row, Kit::ButtonRole::BTN_CONFIRM, "Next", bw, Kit::controlH(), stepNextCb);
}

static void showFace() {
    s_step = Step::STEP_FACE;
    bool lights = false;
    for (uint8_t i = 0; i < s_nItems; i++) lights |= itemSwitches(s_items[i]);
    s_faceW[0] = lights ? "All lights" : "All members";   // the Face row's words (K151)
    for (uint8_t i = 0; i < s_nItems; i++) s_faceW[i + 1] = s_items[i].name;
    stepView("Select card to show as the face:", s_faceW, (uint8_t)(s_nItems + 1), s_face);
}

static void showTap() {
    s_step = Step::STEP_TAP;
    s_nTap = 0;
    const bool mx = itemsMixed();   // K188: a mixed group toggles nothing
    auto add = [](TapAction t, const char *w) { s_tapMap[s_nTap] = t; s_tapW[s_nTap++] = w; };
    if (!mx) add(TapAction::TAP_TOGGLE, "Toggle");
    add(TapAction::TAP_GROUP, "Group view");
    add(TapAction::TAP_MEMBERS, "Members view");
    add(TapAction::TAP_HISTORY, "History view");
    if (s_tap >= s_nTap) s_tap = 0;
    stepView("Select tap action:", s_tapW, s_nTap, s_tap);
}

static void stepNextCb(lv_event_t *) {
    lv_async_call([](void *) {
        if (!S.win) return;
        if (s_step == Step::STEP_FACE) showTap();
        else                           showName();
    }, nullptr);
}

static void askSolo() {
    dlgClose();
    // The one pinned card offered as the face (K191), else all of them; the tap
    // Toggle, or Members when mixed (both first in their lists).
    s_face = 0;
    for (uint8_t i = 0; i < s_nItems; i++) if (s_items[i].step == 2) s_face = (uint8_t)(i + 1);
    s_tap = 0;
    Kit::DialogSpec d = {};
    d.role    = Kit::DialogRole::DLG_CONFIRM;
    d.title   = "Create group";
    d.body[0] = "This will create a group on the page shown as a 'Solo card'.";   // the owner's words (K191)
    d.buttons[0] = { "Cancel", Kit::ButtonRole::BTN_CANCEL, [](lv_event_t *) {
        lv_async_call([](void *) { dlgClose(); }, nullptr);
    } };
    d.buttons[1] = { "Next", Kit::ButtonRole::BTN_CONFIRM, [](lv_event_t *) {
        lv_async_call([](void *) { dlgClose(); if (S.win) showFace(); }, nullptr);
    } };
    d.nButtons = 2;
    d.width    = LV_MIN(lv_area_get_width(&S.winRect) * 3 / 4, UI::mm(UI::shape().W_DIALOG_MAX));
    d.outside  = [](lv_event_t *) { lv_async_call([](void *) { dlgClose(); }, nullptr); };
    s_dlg = Kit::dialog(d);
}

// --- MADE (K168): the group, its cluster on the page, each card a member of it -----------
static bool create() {
    if (!s_nItems || !s_name[0]) return false;
    char gid[48], cid[CARD_ID_MAX + 1];
    const char *areaName = s_area[0] ? HaOffer::areaName(s_area) : "";
    CardGroups::Group g;
    char areaGid[48];
    snprintf(areaGid, sizeof(areaGid), "%s%s", CardGroups::HA_PREFIX, s_area);
    if (s_area[0] && !strcmp(s_name, areaName) && !s_nameTaken && !CardGroups::byId(areaGid, g)) {
        // The area's own group (K141: linked to it by its id), as round 1 made it.
        snprintf(gid, sizeof(gid), "%s", areaGid);
    } else {
        // A group of its own: the area's is another group's already, or it has
        // another name. Modelled after an area, it is linked to it (K141).
        if (!CardGroups::make(s_name, gid, sizeof(gid))) return false;
        if (s_area[0]) HaOffer::setGroupArea(gid, s_area);
    }
    if (!CardAdded::addCluster(s_name, s_name, gid, cid, sizeof(cid))) return false;
    char slots[CLUSTER_SLOTS_MAX * (CARD_ID_MAX + 2)] = "";
    uint8_t nSlots = 0;
    for (uint8_t i = 0; i < s_nItems; i++) {
        const Item &it = s_items[i];
        HaOffer::Row r;
        const Entity *e = EditCards::haRowOf(it.key, r) ? HaOffer::entityFor(r) : nullptr;
        if (!e) continue;
        char id[CARD_ID_MAX + 1];
        if (!CardAdded::addTo(cid, s_name, gid, *e, id, sizeof(id), nullptr)) break;   // full
        if (e->learnt) {
            Settings::setCard(id, "ha", r.ref);
            if (r.dc[0])   Settings::setCard(id, "ha_dc", r.dc);
            if (r.unit[0]) Settings::setCard(id, "ha_unit", r.unit);
        }
        // Pinned (K190: its first tap); a sensor's next: in the header (K168).
        Settings::setCard(id, "hidden", it.step == 2 ? nullptr : "yes");
        if (s_solo && s_face == i + 1) Settings::setCard(cid, "face", id);   // K191
        if (it.step == 3 && nSlots < CLUSTER_SLOTS_MAX) {
            snprintf(slots + strlen(slots), sizeof(slots) - strlen(slots), "%s%s", nSlots ? "," : "", id);
            nSlots++;
        }
    }
    Settings::setCard(cid, "slots", slots[0] ? slots : nullptr);
    if (s_solo) {   // K191: shown as its solo card, its tap as chosen
        Settings::setCard(cid, "present", "card");
        if (s_tap < s_nTap) Settings::setCard(cid, "card_tap", tapActionName(s_tapMap[s_tap]));
    }
    Settings::save(400);
    EditCards::forget();
    Kit::sheetClose(S);
    return true;
}

static void leftCb(lv_event_t *) {
    lv_async_call([](void *) {
        if (!S.win) return;
        if (s_step == Step::STEP_NAME && s_solo) showTap();      // K191's steps, back
        else if (s_step == Step::STEP_TAP)   showFace();
        else if (s_step == Step::STEP_NAME || s_step == Step::STEP_FACE) showCards(true);   // back, the taps kept
        else if (s_step == Step::STEP_CARDS) { EditCards::forget(); showArea(); }
        else                                 Kit::sheetClose(S);
    }, nullptr);
}

bool open(const char *pageTitle) {
    (void)pageTitle;
    if (Kit::sheetShowing()) return false;
    HaOffer::refresh();
    EditCards::forget();
    s_area[0] = s_list[0] = s_name[0] = '\0';
    s_namePick = 0;
    s_nItems = 0;
    s_solo = false;
    s_face = s_tap = s_nTap = 0;
    s_step = Step::STEP_AREA;
    sheet = Kit::SheetSpec{};
    sheet.leftCb     = leftCb;
    sheet.buildFirst = showArea;
    sheet.closing    = []() { dlgClose(); };
    sheet.closed     = []() { s_nameGrid = nullptr; s_grid = nullptr; EditCards::forget(); };
    sheet.sparks     = true;
    Kit::sheetPlace(S);
    // FULL SIZE, AS EDIT CARDS (K168): the page's room under the header.
    const int32_t sw = lv_obj_get_width(lv_screen_active()), sh = lv_obj_get_height(lv_screen_active());
    const int32_t gap = UI::mm(UI::shape().S4);
    S.winRect.x1 = gap;
    S.winRect.x2 = sw - gap - 1;
    S.winRect.y1 = UIToolkit::systemHeaderPx() + gap;
    S.winRect.y2 = sh - gap - 1;
    Kit::sheetOpen(S, sheet);
    return true;
}

void close()  { Kit::sheetClose(S); }
bool isOpen() { return S.win != nullptr; }

#ifdef DEBUG_POPUP
// /popup?ncarea=<id>: that area chosen ("" Create new) - Edit cards' flow follows.
bool areaDebug(const char *areaId) {
    if (!isOpen()) return false;
    snprintf(s_area, sizeof(s_area), "%s", areaId ? areaId : "");
    if (s_area[0]) snprintf(s_list, sizeof(s_list), "%s (new)", HaOffer::areaName(s_area));
    else           snprintf(s_list, sizeof(s_list), "%s", NEW_GROUP);
    EditCards::forget();
    showCards(false);
    return true;
}
// /popup?nccreate=1: Create group, the name step's Next, the dialog's Create.
bool createDebug() {
    if (!isOpen()) return false;
    if (s_step == Step::STEP_CARDS && s_dlg) { dlgClose(); showFace(); return true; }   // K191's dialog: Next
    if (s_step == Step::STEP_CARDS) { act(Act::ACT_SAVE, nullptr); return true; }
    if (s_step == Step::STEP_FACE) { showTap(); return true; }
    if (s_step == Step::STEP_TAP)  { showName(); return true; }
    if (s_step == Step::STEP_NAME && !s_dlg) { askCreate(); return true; }
    if (s_dlg) { dlgClose(); return create(); }
    return false;
}
#endif

}  // namespace NewClusterSheet
