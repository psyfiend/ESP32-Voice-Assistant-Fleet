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
static Kit::Selector  s_sel[ROWS_MAX];
static lv_obj_t      *s_list    = nullptr;   // the rows and the buttons, scrolling
static lv_obj_t      *s_confirm = nullptr;   // Reset all's question
static bool           s_tokensAfter = false; // the token page, once the window has gone

static void closeCb(lv_event_t *) { Kit::sheetClose(S); }

// --- A choice made ------------------------------------------------------------
static void buildList();

// Built again in place - the selector's own look changed. Never inside the
// selector's glide, whose objects these are.
static void rebuildAsync(void *) {
    if (!S.win || !s_list) return;
    const int32_t y = lv_obj_get_scroll_y(s_list);
    lv_obj_delete(s_list);
    s_list = nullptr;
    buildList();
    lv_obj_update_layout(s_list);
    lv_obj_scroll_to_y(s_list, y, LV_ANIM_OFF);
}

// From the next timer pass, outside the selector's glide: a choice may rebuild
// the whole dashboard under the window.
static void chooseAsync(void *p) {
    const uintptr_t v = (uintptr_t)p;
    const uint8_t r = (uint8_t)(v >> 8), i = (uint8_t)(v & 0xFF);
    if (!s_spec.rows || r >= s_spec.nRows || !s_spec.rows[r].choose) return;
    s_spec.rows[r].choose(i);
    if (s_spec.rows[r].rebuild) rebuildAsync(nullptr);
}

static void chooseRow(uint8_t r, uint8_t i) {
    lv_async_call(chooseAsync, (void *)(uintptr_t)(((uintptr_t)r << 8) | i));
}

// The selector's onChosen carries no user data: one function per row.
template <uint8_t R> static void chosenCb(uint8_t i) { chooseRow(R, i); }
static void (*const ROW_CB[ROWS_MAX])(uint8_t) = {
    chosenCb<0>, chosenCb<1>, chosenCb<2>, chosenCb<3>, chosenCb<4>,
    chosenCb<5>, chosenCb<6>, chosenCb<7>, chosenCb<8>, chosenCb<9>,
};

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
// A row per choice: its words, then its selector. Every choice the width of the
// widest word in any row (D-27: "its choices even, so rows line up"). Where the
// words and the widest selector will not fit side by side (the 4B's window),
// each row's words stand above its selector.
static void buildList() {
    const UIShape &k = UI::shape();
    const int32_t stageW = lv_area_get_width(&S.winRect) - 2 * S.pad;
    const int32_t gapX   = UI::mm(k.S5);
    const int32_t g      = UI::mm(k.S1);

    int32_t wordsW = 0, slotW = 0;
    uint8_t most = 0;
    for (uint8_t r = 0; r < s_spec.nRows; r++) {
        const Row &row = s_spec.rows[r];
        wordsW = LV_MAX(wordsW, Kit::textW(row.what, UI::type().NAME));
        slotW  = LV_MAX(slotW, Kit::selectorSlotW(row.words, row.n));
        most   = LV_MAX(most, row.n);
    }
    const int32_t selMaxW  = most * slotW + (most + 1) * g;
    const bool    stacked  = wordsW + gapX + selMaxW > stageW;
    if (stacked && selMaxW > stageW) slotW = (stageW - (most + 1) * g) / LV_MAX(most, 1);

    s_list = Kit::plain(S.stage);
    lv_obj_set_width    (s_list, lv_pct(100));
    lv_obj_set_flex_grow(s_list, 1);
    lv_obj_set_flex_flow(s_list, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(s_list, LV_FLEX_ALIGN_START, stacked ? LV_FLEX_ALIGN_CENTER : LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(s_list, UI::mm(stacked ? k.S3 : k.S4), 0);
    UI::tameScroll(s_list);
    lv_obj_add_flag(s_list, LV_OBJ_FLAG_SCROLLABLE);
    UI::unclipShadows(s_list);

    for (uint8_t r = 0; r < s_spec.nRows && r < ROWS_MAX; r++) {
        const Row &row = s_spec.rows[r];
        lv_obj_t *line = Kit::plain(s_list);
        lv_obj_set_size(line, lv_pct(100), LV_SIZE_CONTENT);
        lv_obj_set_flex_flow(line, stacked ? LV_FLEX_FLOW_COLUMN : LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(line, LV_FLEX_ALIGN_START, stacked ? LV_FLEX_ALIGN_CENTER : LV_FLEX_ALIGN_CENTER,
                              LV_FLEX_ALIGN_CENTER);
        lv_obj_set_style_pad_column(line, gapX, 0);
        lv_obj_set_style_pad_row(line, g, 0);
        UI::unclipShadows(line);

        lv_obj_t *w = Kit::label(line, Kit::KitStyle::KS_LABEL_NAME_DIM);
        lv_label_set_text(w, row.what);
        if (!stacked) lv_obj_set_width(w, wordsW);

        Kit::SelectorSpec sp = {};
        for (uint8_t i = 0; i < row.n && i < Kit::SELECTOR_MAX; i++) sp.glyph[i] = row.words[i];
        sp.n        = row.n;
        sp.chosen   = row.chosen ? row.chosen() : 0;
        sp.compact  = true;
        sp.slotW    = slotW;
        sp.onChosen = ROW_CB[r];
        Kit::selector(s_sel[r], line, sp);
    }

    // Tokens (the token page; the window goes first) and Reset all.
    lv_obj_t *btns = Kit::plain(s_list);
    lv_obj_set_size(btns, lv_pct(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(btns, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(btns, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(btns, gapX, 0);
    lv_obj_set_style_pad_top(btns, UI::mm(k.S3), 0);
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
    for (Kit::Selector &x : s_sel) x = Kit::Selector{};
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

void close()  { Kit::sheetClose(S); }
bool isOpen() { return S.win != nullptr; }

void refresh() {
    if (!S.win) return;
    for (uint8_t r = 0; r < s_spec.nRows && r < ROWS_MAX; r++)
        if (s_sel[r].obj && s_spec.rows[r].chosen) Kit::selectorSet(s_sel[r], s_spec.rows[r].chosen());
}

} // namespace TweaksSheet
