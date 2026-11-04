// "Edit cards" from the page panel (2.13 round 2, K148, K149) - see
// include/UI/EditCardsSheet.h.
#include "UI/EditCardsSheet.h"
#include "UI/HaOffer.h"
#include "UI/Kit.h"
#include "UI/Sheet.h"
#include "UI/UIToolkit.h"
#include "UI/UITokens.h"
#include "Cards/Card.h"
#include "Cards/CardAdded.h"
#include "Cards/CardGroups.h"
#include "Cards/CardPopup.h"   // CARD_SURFACE_FLAG: the page's groups, from its cards
#include "Cards/CardWhere.h"
#include "Entity.h"
#include "Settings.h"
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

namespace EditCardsSheet {

using KS   = Kit::KitStyle;
using Act  = EditCards::Act;
using Item = EditCards::Item;

static Kit::Sheet     S;
static Kit::SheetSpec sheet;
static Spec           s_spec;
static char           s_title[48];
static bool           s_step = false;        // the group step showing
static lv_obj_t      *s_dlg = nullptr;       // the question, while it is up
static void         (*s_go)() = nullptr;     // ...what its button does

static Item    s_items[24];                  // what was ticked when Add, Hide... was tapped
static uint8_t s_nItems = 0;

static void showLists(bool keep);
static void showStep();

// --- The question ---------------------------------------------------------------------
static char        s_line[4][420];
static const char *s_lines[4];
static uint8_t     s_nLines = 0;

static void line(const char *fmt, ...) {
    if (s_nLines >= 4) return;
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(s_line[s_nLines], sizeof(s_line[0]), fmt, ap);
    va_end(ap);
    s_lines[s_nLines] = s_line[s_nLines];
    s_nLines++;
}

static const char *acc(char *buf, size_t cap, const char *name) {
    snprintf(buf, cap, "#%06lX %s#", (unsigned long)(UI::pal().ACCENT & 0xFFFFFF), name ? name : "");
    return buf;
}

static void dlgClose() {
    if (s_dlg) lv_obj_delete(s_dlg);
    s_dlg = nullptr;
}

static void cancelCb(lv_event_t *) { lv_async_call([](void *) { dlgClose(); }, nullptr); }

static void goCb(lv_event_t *) {
    lv_async_call([](void *) {
        dlgClose();
        if (S.win && s_go) s_go();
    }, nullptr);
}

// GO OR NO-GO (K149): Cancel and one button, every choice made before it.
static void ask(Kit::DialogRole role, const char *title, const char *verb, Kit::ButtonRole verbRole, void (*go)()) {
    dlgClose();
    s_go = go;
    Kit::DialogSpec d = {};
    d.role  = role;
    d.title = title;
    for (uint8_t i = 0; i < s_nLines && i < 4; i++) d.body[i] = s_lines[i];
    d.buttons[0] = { "Cancel", Kit::ButtonRole::BTN_CANCEL, cancelCb };
    d.buttons[1] = { verb, verbRole, goCb };
    d.nButtons  = 2;
    d.width     = LV_MIN(lv_area_get_width(&S.winRect) * 3 / 4, UI::mm(UI::shape().W_DIALOG_MAX));
    d.outside   = cancelCb;
    d.countdown = true;
    s_dlg = Kit::dialog(d);
}

// After any of them: the window goes, and the page is built again under it.
static void done(uint8_t n, const char *fmt) {
    if (!n) return;
    Settings::save(400);
    char msg[48];
    snprintf(msg, sizeof(msg), fmt, (unsigned)n);
    UIToolkit::show_toast(msg);
    EditCards::forget();
    Kit::sheetClose(S);
}

// --- The group step (mock 10: Add to dashboard's own) ------------------------------------
// Area (when the cards come from more than one HA area: each into its own area's
// group), the groups on this page - the cards' own area's marked "*" - then New
// group and None.
constexpr uint8_t GRP_MAX = 16;
struct Grp { char gid[48]; char name[44]; };
static Grp         s_grp[GRP_MAX];
static uint8_t     s_nGrp = 0;
static const char *s_words[GRP_MAX + 3];
static bool        s_hasArea = false;        // the first chip is Area
static uint8_t     s_pick = 0;
static char        s_newName[40] = "";
static lv_obj_t   *s_grid = nullptr;
static char        s_oneArea[32] = "";       // every card's HA area, when they share one

static uint8_t pickNew()  { return (uint8_t)((s_hasArea ? 1 : 0) + s_nGrp); }
static uint8_t pickNone() { return (uint8_t)(pickNew() + 1); }

static void grpAdd(const CardGroups::Group &g, bool own) {
    for (uint8_t i = 0; i < s_nGrp; i++) if (!strcmp(s_grp[i].gid, g.id)) return;
    if (s_nGrp >= GRP_MAX) return;
    snprintf(s_grp[s_nGrp].gid, sizeof(s_grp[0].gid), "%s", g.id);
    snprintf(s_grp[s_nGrp].name, sizeof(s_grp[0].name), own ? "%s *" : "%s", g.name);
    s_nGrp++;
}

static void findGroups(lv_obj_t *o) {
    const uint32_t n = lv_obj_get_child_count(o);
    for (uint32_t i = 0; i < n; i++) {
        lv_obj_t *c = lv_obj_get_child(o, (int32_t)i);
        if (!lv_obj_has_flag(c, CardPopup::CARD_SURFACE_FLAG)) { findGroups(c); continue; }
        Card *k = (Card *)lv_obj_get_user_data(c);
        CardGroups::Group g;
        if (k && CardGroups::ofCard(*k, g)) grpAdd(g, false);   // a cluster's tiles wear its group
    }
}

static void collectGroups() {
    s_nGrp = 0;
    s_oneArea[0] = '\0';
    bool one = true;
    for (uint8_t i = 0; i < s_nItems; i++) {
        if (i == 0) snprintf(s_oneArea, sizeof(s_oneArea), "%s", s_items[i].areaId);
        else one &= !strcmp(s_oneArea, s_items[i].areaId);
    }
    if (!one) s_oneArea[0] = '\0';
    s_hasArea = !one;
    // The cards' own area first, when they share one (its group is made if new).
    char own[48] = "";
    if (one && s_oneArea[0]) {
        snprintf(own, sizeof(own), "%s%s", CardGroups::HA_PREFIX, s_oneArea);
        CardGroups::Group g;
        if (!CardGroups::byId(own, g)) {
            snprintf(g.id, sizeof(g.id), "%s", own);
            snprintf(g.name, sizeof(g.name), "%s", HaOffer::areaName(s_oneArea));
        }
        grpAdd(g, true);
    }
    findGroups(lv_screen_active());
    uint8_t w = 0;
    if (s_hasArea) s_words[w++] = "Area";   // K158
    for (uint8_t i = 0; i < s_nGrp; i++) s_words[w++] = s_grp[i].name;
    s_words[w++] = "New label";             // K158
    s_words[w++] = "None";
    s_pick = 0;   // Area, or the cards' own area's group
}

static void keepName(const char *words, void *) {
    snprintf(s_newName, sizeof(s_newName), "%s", words ? words : "");
    if (!s_newName[0]) { s_pick = 0; if (s_grid) Kit::chipGridLit(s_grid, s_pick); }
}

static void gridCb(lv_event_t *ev) {
    s_pick = (uint8_t)(uintptr_t)lv_event_get_user_data(ev);
    if (s_grid) Kit::chipGridLit(s_grid, s_pick);
    if (s_pick == pickNew()) {
        Kit::KeyboardSpec k = {};
        k.title  = "New label name";   // K158
        k.text   = s_newName;
        k.maxLen = sizeof(s_newName) - 1;
        k.save   = keepName;
        Kit::keyboard(k);
    }
}

// The group a card goes into: its own area's (Area), the pick's, a New group
// made now (once), or none.
static bool gidFor(const Item &it, char *gid, size_t cap, char *made, size_t madeCap) {
    if (s_pick == pickNone()) { snprintf(gid, cap, "none"); return true; }
    if (s_pick == pickNew()) {
        if (!made[0] && (!s_newName[0] || !CardGroups::make(s_newName, made, madeCap))) return false;
        snprintf(gid, cap, "%s", made);
        return true;
    }
    if (s_hasArea && s_pick == 0) {
        if (it.areaId[0]) snprintf(gid, cap, "%s%s", CardGroups::HA_PREFIX, it.areaId);
        else              snprintf(gid, cap, "none");
        return true;
    }
    const uint8_t g = (uint8_t)(s_pick - (s_hasArea ? 1 : 0));
    if (g >= s_nGrp) return false;
    snprintf(gid, cap, "%s", s_grp[g].gid);
    return true;
}

static const char *pickWords() {
    if (s_pick == pickNew()) return s_newName;
    const uint8_t g = (uint8_t)(s_pick - (s_hasArea ? 1 : 0));
    if (g >= s_nGrp) return "";
    static char name[44];
    snprintf(name, sizeof(name), "%s", s_grp[g].name);
    const size_t len = strlen(name);
    if (len > 2 && !strcmp(name + len - 2, " *")) name[len - 2] = '\0';
    return name;
}

static void doAdd() {
    uint8_t added = 0;
    char made[48] = "";
    for (uint8_t i = 0; i < s_nItems; i++) {
        const Item &it = s_items[i];
        HaOffer::Row r;
        const Entity *e = EditCards::haRowOf(it.key, r) ? HaOffer::entityFor(r) : nullptr;
        char gid[48];
        if (!e || !gidFor(it, gid, sizeof(gid), made, sizeof(made))) continue;
        char id[CARD_ID_MAX + 1];
        if (!CardAdded::addToPage(*e, HaOffer::areaName(r.areaId), gid, id, sizeof(id))) break;   // full
        if (e->learnt) {
            Settings::setCard(id, "ha", r.ref);
            if (r.dc[0])   Settings::setCard(id, "ha_dc", r.dc);
            if (r.unit[0]) Settings::setCard(id, "ha_unit", r.unit);
        }
        added++;
    }
    done(added, "Added %u");   // to confirm (round 1's)
}

static void askAdd() {
    if (s_pick == pickNew() && !s_newName[0]) { UIToolkit::show_toast("Name the new label first"); return; }   // K158
    static char names[400], page[96], grp[96];
    EditCards::namesOf(names, sizeof(names));
    const bool many = s_nItems > 1;
    s_nLines = 0;
    line(many ? "The cards %s will be added from Home Assistant and placed onto the %s page."
              : "The card %s will be added from Home Assistant and placed onto the %s page.",
         names, acc(page, sizeof(page), s_spec.page ? s_spec.page : ""));
    // K158: a page card's colored name is its label.
    if (s_pick == pickNone())
        line(many ? "The cards will have no label." : "The card will have no label.");
    else if (s_hasArea && s_pick == 0)
        line(many ? "Each card will be labeled with its Home Assistant area." : "The card will be labeled with its Home Assistant area.");
    else
        line(many ? "The cards will be labeled %s." : "The card will be labeled %s.", acc(grp, sizeof(grp), pickWords()));
    line("A card can always be restored to its original configuration.");
    ask(Kit::DialogRole::DLG_CONFIRM, many ? "Add cards to dashboard?" : "Add card to dashboard?", "Add",
        Kit::ButtonRole::BTN_CONFIRM, doAdd);
}

static void nextCb(lv_event_t *) { lv_async_call([](void *) { askAdd(); }, nullptr); }

static void buildStep() {
    lv_label_set_text(S.lblTitleArea, s_title);
    lv_label_set_text(S.lblTitleName, "Add to dashboard");   // the existing step's title
    lv_label_set_text(S.lblLeft, LV_SYMBOL_LEFT);
    Kit::sheetLayoutTitle(S);
    lv_obj_clean(S.stage);
    lv_obj_update_layout(S.win);
    const int32_t w = lv_obj_get_content_width(S.stage);
    lv_obj_t *col = Kit::plain(S.stage);
    lv_obj_set_size(col, w, lv_pct(100));
    lv_obj_set_flex_flow(col, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(col, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(col, UI::mm(UI::shape().S6), 0);
    lv_label_set_text(Kit::label(col, KS::KS_LABEL_NAME),
                      s_nItems > 1 ? "Select label for these cards:" : "Select label for this card:");   // K158
    s_grid = Kit::chipGrid(col, w, s_words, (uint8_t)(pickNone() + 1), s_pick, gridCb);
    lv_obj_t *n = Kit::label(col, KS::KS_LABEL_TAG_DIM);
    lv_obj_set_width(n, w);
    lv_obj_set_style_text_align(n, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_long_mode(n, LV_LABEL_LONG_WRAP);
    lv_label_set_text(n, s_nItems > 1 ? "Select 'New label' to create a new label for these cards. Select 'None' to "
                                        "add the cards with no label."
                                      : "Select 'New label' to create a new label for this card. Select 'None' to "
                                        "add the card with no label.");   // K158
    lv_obj_t *row = Kit::buttonRow(col);
    const int32_t bw = Kit::textW("Next", UI::type().NAME) + 2 * UI::mm(UI::shape().S7);
    Kit::button(row, Kit::ButtonRole::BTN_CONFIRM, "Next", bw, Kit::controlH(), nextCb);
}

static void showStep() {
    s_step = true;
    collectGroups();
    buildStep();
}

// --- Hide, Remove, Restore -------------------------------------------------------------
static void doHide() {
    uint8_t n = 0;
    for (uint8_t i = 0; i < s_nItems; i++) n += CardAdded::hide(s_items[i].key) ? 1 : 0;
    done(n, "Hid %u");   // to confirm
}

static void doRemove() {
    uint8_t n = 0;
    for (uint8_t i = 0; i < s_nItems; i++) n += CardAdded::drop(s_items[i].key) ? 1 : 0;
    done(n, "Removed %u");   // to confirm
}

static void doRestore() {
    uint8_t n = 0;
    for (uint8_t i = 0; i < s_nItems; i++) n += CardAdded::restore(s_items[i].key) ? 1 : 0;
    done(n, "Restored %u");   // to confirm
}

static void act(Act a, void *) {
    s_nItems = EditCards::chosen(s_items, 24);
    if (!s_nItems) return;
    if (a == Act::ACT_ADD)     { showStep(); return; }
    if (a == Act::ACT_RESTORE) { doRestore(); return; }   // nothing goes: no question
    static char names[400];
    EditCards::namesOf(names, sizeof(names));
    const bool many = s_nItems > 1;
    s_nLines = 0;
    if (a == Act::ACT_HIDE) {
        line(many ? "The cards %s will be hidden from this page." : "The card %s will be hidden from this page.", names);
        line("Hidden cards can always be restored from Edit cards.");   // to confirm
        ask(Kit::DialogRole::DLG_DESTRUCTIVE, many ? "Hide these cards?" : "Hide this card?", "Hide",
            Kit::ButtonRole::BTN_DESTRUCTIVE, doHide);
        return;
    }
    line(many ? "The cards %s will be removed from this device." : "The card %s will be removed from this device.", names);
    line("Nothing will be removed from Home Assistant. You can always add these entities back to your device.");
    line(many ? "Tap Remove to confirm you wish to remove these cards from the device."
              : "Tap Remove to confirm you wish to remove this card from the device.");
    ask(Kit::DialogRole::DLG_DESTRUCTIVE, many ? "Remove these cards?" : "Remove this card?", "Remove",
        Kit::ButtonRole::BTN_DESTRUCTIVE, doRemove);
}

// --- The lists ----------------------------------------------------------------------------
static const char *held(const HaOffer::Row &r, void *) {
    static char words[56];
    return (r.have && CardWhere::words(*r.have, nullptr, HaOffer::registry(), words, sizeof(words))) ? words : nullptr;
}

static void buildLists(bool keep) {
    lv_label_set_text(S.lblTitleArea, s_title);
    lv_label_set_text(S.lblTitleName, "Edit cards");   // K148
    lv_label_set_text(S.lblLeft, LV_SYMBOL_CLOSE);
    Kit::sheetLayoutTitle(S);
    lv_obj_clean(S.stage);
    s_grid = nullptr;
    lv_obj_update_layout(S.win);
    EditCards::Spec sp;
    for (uint8_t k = 0; k < s_spec.nLists && k < 2; k++) sp.lists[k] = s_spec.lists[k];
    sp.nLists    = s_spec.nLists;
    sp.held      = held;
    sp.ctx       = s_spec.ctx;
    sp.act       = act;
    sp.keepTicks = keep;
    EditCards::build(S.stage, lv_obj_get_content_width(S.stage), lv_obj_get_content_height(S.stage), sp);
}

static void showLists(bool keep) {
    s_step = false;
    buildLists(keep);
}

static void leftCb(lv_event_t *) {
    if (s_step) lv_async_call([](void *) { if (S.win) showLists(true); }, nullptr);   // back, the ticks kept
    else        Kit::sheetClose(S);
}

bool open(const Spec &spec) {
    if (Kit::sheetShowing()) return false;
    s_spec = spec;
    s_step = false;
    s_newName[0] = '\0';
    EditCards::forget();
    HaOffer::refresh();
    snprintf(s_title, sizeof(s_title), "%s > ", spec.page ? spec.page : "Page");
    sheet = Kit::SheetSpec{};
    sheet.leftCb     = leftCb;
    sheet.buildFirst = []() { buildLists(false); };
    sheet.closing    = []() { dlgClose(); };
    sheet.closed     = []() { s_grid = nullptr; s_step = false; };
    sheet.sparks     = true;
    Kit::sheetPlace(S);
    // THE PAGE'S ROOM (K148), as the group view has: under the header, the whole width.
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
bool goDebug() {
    if (s_dlg && s_go) { dlgClose(); s_go(); return true; }
    if (s_step && S.win) { askAdd(); return true; }
    return false;
}

bool groupDebug(uint8_t i) {
    if (!s_step || !s_grid || i > pickNone()) return false;
    s_pick = i;
    Kit::chipGridLit(s_grid, s_pick);
    return true;
}
#endif

}  // namespace EditCardsSheet
