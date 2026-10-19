// "UI Tweaks" (2.8 round 3) - see include/UI/TweaksSheet.h. A Sheet client: one
// view, no tabs, no panels; the X closes it, and it closes itself on the window
// timeout as every window does.
#include "UI/TweaksSheet.h"
#include "UI/Sheet.h"
#include "UI/UITokens.h"
#include <Arduino.h>
#include <string.h>

namespace TweaksSheet {

static Kit::Sheet     S;
static Kit::SheetSpec spec;
static Spec           s_spec = {};
static lv_obj_t      *s_list    = nullptr;   // the rows and the buttons, scrolling
static lv_obj_t      *s_confirm = nullptr;   // Reset all's question
static bool           s_tokensAfter = false; // the token page, once the window has gone

static void closeCb(lv_event_t *) { Kit::sheetClose(S); }

// --- A choice made ------------------------------------------------------------
static void buildList();

// Built again in place, where it was scrolled to (Reset all: every row read again).
static void rebuildAsync(void *) {
    if (!S.win || !s_list) return;
    const int32_t y = lv_obj_get_scroll_y(s_list);
    lv_obj_delete(s_list);
    s_list = nullptr;
    buildList();
    lv_obj_update_layout(s_list);
    lv_obj_scroll_to_y(s_list, y, LV_ANIM_OFF);
}

// From the next timer pass, outside the chip's own click: a choice may rebuild
// the whole dashboard under the window.
static void chooseAsync(void *p) {
    const uintptr_t v = (uintptr_t)p;
    const uint8_t r = (uint8_t)(v >> 8), i = (uint8_t)(v & 0xFF);
    if (!s_spec.rows || r >= s_spec.nRows) return;
    const Row &row = s_spec.rows[r];
    if (i == Kit::CHIP_SLIDER) { if (row.setValue && s_list) row.setValue(Kit::chipTableValue(s_list, r)); return; }
    if (row.choose) row.choose(i);
}

// A chip: lit now, the choice made from the next timer pass.
static void chipCb(lv_event_t *ev) {
    const uintptr_t v = (uintptr_t)lv_event_get_user_data(ev);
    Kit::chipTableLit(s_list, (uint8_t)(v >> 8), (uint8_t)(v & 0xFF));
    lv_async_call(chooseAsync, (void *)v);
}

// --- Tokens and Reset all -----------------------------------------------------
static void tokensCb(lv_event_t *) {
    s_tokensAfter = true;
    Kit::sheetClose(S);
}

static void confirmClose() {
    if (s_confirm) { lv_obj_delete(s_confirm); s_confirm = nullptr; }
}
static void confirmCancelCb(lv_event_t *) { lv_async_call([](void *) { confirmClose(); }, nullptr); }
static void confirmResetCb(lv_event_t *) {
    lv_async_call([](void *) {
        confirmClose();
        if (s_spec.resetAll) s_spec.resetAll();
        if (S.win) rebuildAsync(nullptr);   // every row read again, in the look now in force
    }, nullptr);
}

static void resetCb(lv_event_t *) {
    lv_async_call([](void *) {
        confirmClose();
        if (!S.win) return;
        Kit::DialogSpec d = {};
        d.role    = Kit::DialogRole::DLG_DESTRUCTIVE;
        d.title   = "Reset all UI Tweaks?";
        d.body[0] = "Every choice in this window goes back to how the board shipped.";
        d.buttons[0] = { "Cancel", Kit::ButtonRole::BTN_CANCEL, confirmCancelCb };
        d.buttons[1] = { "Reset all", Kit::ButtonRole::BTN_DESTRUCTIVE, confirmResetCb };
        d.nButtons  = 2;
        d.width     = LV_MIN(lv_area_get_width(&S.winRect) * 3 / 4, UI::mm(UI::shape().W_DIALOG_MAX));
        d.outside   = confirmCancelCb;
        d.countdown = true;   // it closes with the window: it says so (#23)
        s_confirm = Kit::dialog(d);
    }, nullptr);
}

// --- The view -----------------------------------------------------------------
// The kit's chip table (round 5: chips, not selectors), then the buttons.
static void buildList() {
    const UIShape &k = UI::shape();
    const int32_t stageW = lv_area_get_width(&S.winRect) - 2 * S.pad;
    const int32_t gapX   = UI::mm(k.S5);

    Kit::ChipRowSpec rows[ROWS_MAX];
    const uint8_t n = s_spec.nRows < ROWS_MAX ? s_spec.nRows : ROWS_MAX;
    for (uint8_t r = 0; r < n; r++) {
        const Row &row = s_spec.rows[r];
        rows[r] = { row.what, row.words, row.n, (uint8_t)(row.chosen ? row.chosen() : 0) };
        if (row.hi > row.lo) {   // a slider row
            rows[r].lo    = row.lo;
            rows[r].hi    = row.hi;
            rows[r].value = row.value ? row.value() : row.lo;
            rows[r].unit  = row.unit;
        }
    }
    s_list = Kit::chipTable(S.stage, stageW, rows, n, chipCb);

    // Tokens (the token page; the window goes first) and Reset all.
    lv_obj_t *btns = Kit::buttonRow(s_list);
    const int32_t bw = LV_MIN((stageW - gapX) / 2, Kit::textW("Reset all", UI::type().NAME) + 2 * UI::mm(k.S9));
    Kit::button(btns, Kit::ButtonRole::BTN_CANCEL, "Tokens", bw, Kit::controlH(), tokensCb);
    Kit::button(btns, Kit::ButtonRole::BTN_DESTRUCTIVE, "Reset all", bw, Kit::controlH(), resetCb);
}

static void build() {
    lv_label_set_text(S.lblTitleArea, "");
    lv_label_set_text(S.lblTitleName, "UI Tweaks");
    Kit::sheetLayoutTitle(S);
    buildList();
}

static void closing() {
    confirmClose();
    if (s_spec.closing) s_spec.closing();
}

static void closed() {
    s_list = nullptr;
    if (s_tokensAfter) {
        s_tokensAfter = false;
        if (s_spec.tokens) s_spec.tokens();
    }
}

bool open(const Spec &sp) {
    if (Kit::sheetShowing()) return false;
    s_spec = sp;
    if (s_spec.nRows > ROWS_MAX) s_spec.nRows = ROWS_MAX;
    s_tokensAfter = false;
    spec = Kit::SheetSpec{};
    spec.leftCb     = closeCb;
    spec.buildFirst = build;
    spec.closing    = closing;
    spec.closed     = closed;
    spec.sparks     = true;
    Kit::sheetPlace(S);
    Kit::sheetOpen(S, spec);
    if (s_spec.opened) s_spec.opened(S.winRect);
    return true;
}

#ifdef DEBUG_POPUP
void debugScrollEnd() {
    if (!s_list) return;
    lv_obj_update_layout(s_list);
    lv_obj_scroll_to_y(s_list, LV_COORD_MAX, LV_ANIM_OFF);
}
#endif

void close()  { Kit::sheetClose(S); }
bool isOpen() { return S.win != nullptr; }

void refresh() {
    if (!S.win) return;
    for (uint8_t r = 0; r < s_spec.nRows && r < ROWS_MAX; r++) {
        const Row &row = s_spec.rows[r];
        if (row.hi > row.lo) { if (row.value) Kit::chipTableSet(s_list, r, row.value()); continue; }
        if (row.chosen) Kit::chipTableLit(s_list, r, row.chosen());
    }
}

} // namespace TweaksSheet
